#!/usr/bin/env bash
# ============================================================================
#  ДЕМО B — Емпирична проверка на корутинния модел на изпълнение
# ============================================================================
#  Изпълнява избран подмножество от benchmarks/bench_async.cpp.
#  Резултатите потвърждават тенденцията в Таблица~6 на Глава~4: при
#  доминираща фаза на изчакване и/или повече задачи, асинхронният модел
#  печели многократно. Абсолютните числа варират според хардуера; тук
#  работим на WSL2 / 12-core x86_64 / GCC 14.2 / io_uring.
#
#  Очаквано време на изпълнение: ~25 s.
#
#  Незадължителна добавка: ако MariaDB е достъпна на 127.0.0.1:3307,
#  скриптът пуска и емпиричната проверка на _start/_cont върху жива
#  база (4 теста, ~300 ms).
# ============================================================================

set -uo pipefail

# Корен и build каталог: подразбират се за WSL, но docker-compose ги override-ва
# с /work и /work/build-docker. Самостоятелните пускания (директно от host-а)
# минават с WSL пътищата.
ORM_ROOT=${ORM_ROOT:-/mnt/d/GitHub/ORM}
BUILD_DIR=${BUILD_DIR:-$ORM_ROOT/build-wsl-uring}
BENCH="$BUILD_DIR/benchmarks/bench_async"
PROBE="$BUILD_DIR/tests/integration/test_mysql_async_coroutine_probe"

# MariaDB координати: WSL варианта вдига контейнера на 127.0.0.1:3307,
# Docker compose варианта говори с mariadb:3306 през вътрешната мрежа.
MYSQL_HOST=${ORM_MYSQL_HOST:-127.0.0.1}
MYSQL_PORT=${ORM_MYSQL_PORT:-3307}
MYSQL_USER=${ORM_MYSQL_USER:-orm_test}
MYSQL_PASSWORD=${ORM_MYSQL_PASSWORD:-orm_test_password}
MYSQL_DATABASE=${ORM_MYSQL_DATABASE:-orm_test}

print_step() {
    printf '\n\033[1;34m== %s ==\033[0m\n' "$*"
}

if [ ! -x "$BENCH" ]; then
    echo "ГРЕШКА: bench_async не е построен. Изпълнете prewarm.sh първо." >&2
    exit 1
fi

print_step "1) Корутинен async vs синхронен thread-pool"
printf '   Реактор: io_uring | Linux WSL2 | %s\n\n' "$(uname -r)"

# Показваме трите случая, при които ефектът е ясно видим. Случаят с
# 10 µs изчакване (нисък overhead, async губи) е документиран в
# Глава 4 и съзнателно е изключен от това подмножество, за да не
# отнема време на сцената.
#
# Едновременно пишем CSV копие, което awk обработва, за да изведе
# реално измерените ускорения (а не статичните "очаквани" числа).
BENCH_CSV=$(mktemp)
trap 'rm -f "$BENCH_CSV"' EXIT

"$BENCH" \
    --benchmark_filter='BM_(Sync|Async)Throughput/4/(100|1000)/(100|1000)/' \
    --benchmark_color=true \
    --benchmark_out="$BENCH_CSV" \
    --benchmark_out_format=csv 2>&1

# Парсваме CSV-то: за всяка тройка (threads/tasks/wait_us) сравняваме
# items_per_second за sync vs async варианта и принтваме реалното
# съотношение.
awk -F, '
    # Google benchmark изпраща в CSV файла и preamble редове
    # (timestamp, "Running ...", "Run on ...", "Load Average ...") преди
    # истинския CSV header. Намираме header-а по съдържание.
    $1 == "name" {
        for (i = 1; i <= NF; i++) col[$i] = i
        next
    }
    $1 ~ /^"?BM_(Sync|Async)Throughput/ {
        name = $1
        gsub(/^"|"$/, "", name)
        split(name, parts, "/")
        kind = (parts[1] ~ /Async/) ? "a" : "s"
        key  = parts[2] "/" parts[3] "/" parts[4]
        ips_col = ("items_per_second" in col) ? col["items_per_second"] : 7
        rate[key, kind] = $ips_col + 0
        if (!(key in seen)) { seen[key] = 1; order[++n_keys] = key }
    }
    function fmt(x) {
        if (x >= 1e6) return sprintf("%.1fM", x / 1e6)
        if (x >= 1e3) return sprintf("%.1fk", x / 1e3)
        return sprintf("%.0f", x)
    }
    END {
        printf "\n  \033[1;32mИзмерени ускорения (Async vs Sync, items_per_second)\033[0m\n"
        printf "  (виж Таблица 6 на Глава 4):\n\n"
        for (i = 1; i <= n_keys; i++) {
            k = order[i]
            split(k, p, "/")
            s = rate[k, "s"]; a = rate[k, "a"]
            if (s > 0 && a > 0) {
                ratio = a / s
                printf "    \033[1;33m%5.1f×\033[0m  %s нишки × %s задачи × %s µs   (sync %s/s → async %s/s)\n", \
                    ratio, p[1], p[2], p[3], fmt(s), fmt(a)
            }
        }
        printf "\n  Тенденция: колкото повече waiting / повече задачи,\n"
        printf "  толкова по-голяма е победата на корутинния модел.\n"
    }
' "$BENCH_CSV"

# ── Незадължителна добавка: жива MariaDB ─────────────────────────────────
if [ -x "$PROBE" ] && timeout 1 bash -c "</dev/tcp/${MYSQL_HOST}/${MYSQL_PORT}" 2>/dev/null; then
    print_step "2) Бонус: native _start/_cont срещу жива MariaDB"
    ORM_TEST_MYSQL_LIVE=1 \
    ORM_MYSQL_HOST="$MYSQL_HOST" \
    ORM_MYSQL_PORT="$MYSQL_PORT" \
    ORM_MYSQL_USER="$MYSQL_USER" \
    ORM_MYSQL_PASSWORD="$MYSQL_PASSWORD" \
    ORM_MYSQL_DATABASE="$MYSQL_DATABASE" \
        "$PROBE" --gtest_color=yes
else
    printf '\n\033[1;33m(MariaDB не е достъпна на %s:%s — пропускаме бонуса)\033[0m\n' \
        "$MYSQL_HOST" "$MYSQL_PORT"
fi
