#!/usr/bin/env bash
# ============================================================================
#  PREWARM — Подготвя средата за демото преди защитата
# ============================================================================
#  Изпълнете преди излизане на сцената. Скриптът:
#    1. Сглобява bench_async, test_async_unit и test_mysql_async_coroutine_probe
#       (със system libs ON, всички живи конектори ON).
#    2. Прави "сухо" пускане на двете демо скрипта, за да зареди файловете в
#       кеша на ОС и да загрее JIT/SSE state.
#    3. Опитва да стартира MariaDB Docker контейнера (незадължителен бонус).
#
#  Работи и в двата режима:
#    - WSL на host-а: default-ите сочат към /mnt/d/GitHub/ORM и build-wsl-uring,
#      реактор io_uring, MariaDB сайдкарът на 127.0.0.1:3307.
#    - Вътре в demo Docker контейнера: docker-compose подава ORM_ROOT=/work,
#      BUILD_DIR=/work/build-docker, MariaDB на mariadb:3306; скриптът ги
#      ползва и преминава автоматично на epoll реактор (uring изисква по-нов
#      kernel от base ubuntu:24.04).
#
#  Очаквано време: ~30 s (без бонус), ~50 s (с бонус).
# ============================================================================

set -euo pipefail

# Детектираме Docker средата по канонични белези: или сме в контейнер
# (/.dockerenv), или host-ът ни е подал ORM_ROOT=/work (docker-compose
# поведение).
IN_DOCKER=0
if [ -f /.dockerenv ] || [ "${ORM_ROOT:-}" = "/work" ]; then
    IN_DOCKER=1
fi

if [ "$IN_DOCKER" = "1" ]; then
    ORM_ROOT=${ORM_ROOT:-/work}
    BUILD_DIR=${BUILD_DIR:-/work/build-docker}
    REACTOR=${ORM_LINUX_REACTOR:-epoll}
else
    ORM_ROOT=${ORM_ROOT:-/mnt/d/GitHub/ORM}
    BUILD_DIR=${BUILD_DIR:-$ORM_ROOT/build-wsl-uring}
    REACTOR=${ORM_LINUX_REACTOR:-uring}
fi
DEMO_DIR="$ORM_ROOT/doc/v2/bg_presentation/demo"

print_step() { printf '\n\033[1;34m== %s ==\033[0m\n' "$*"; }

print_step "1) Сглобяване на нужните цели (реактор: $REACTOR)"
# Ако build директорията не съществува, я създаваме с нужните опции.
# Ако съществува, не пускаме повторно `cmake .` --- регенерацията през
# монтиран Windows път (cmake recompact → ninja) често пропада с
# „No such file or directory" поради разлика в кеш-семантиката между
# WSL и NTFS. Ninja сам ще извика cmake при нужда.
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    mkdir -p "$BUILD_DIR"
    cmake -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DORM_USE_SYSTEM_LIBS=ON \
          -DORM_ENABLE_BENCHMARKS=ON \
          -DORM_ENABLE_SQLITE=ON \
          -DORM_ENABLE_MYSQL=ON \
          -DORM_ENABLE_POSTGRESQL=ON \
          -DORM_ENABLE_MONGODB=ON \
          -DORM_ENABLE_REDIS=ON \
          -DORM_ENABLE_CASSANDRA=ON \
          -DORM_ENABLE_NEO4J=ON \
          -DORM_LINUX_REACTOR="$REACTOR" \
          -S "$ORM_ROOT" -B "$BUILD_DIR" > /dev/null
fi

ninja -C "$BUILD_DIR" test_async_unit bench_async test_mysql_async_coroutine_probe 2>&1 | tail -3

print_step "2) Загряваща компилация на Демо A"
bash "$DEMO_DIR/run_demo_a.sh" > /dev/null 2>&1 || true
echo "   Демо A компилирано и кеширано."

print_step "3) Загряваща run-проба на Демо B"
"$BUILD_DIR/benchmarks/bench_async" \
    --benchmark_filter='BM_SyncThroughput/4/100/100/' \
    --benchmark_color=false 2>&1 | tail -1
echo "   Демо B загрято."

# В Docker средата MariaDB вече е сайдкар и docker-compose го гарантира.
# WSL варианта продължава да дърпа docker compose в /docker/.
if [ "$IN_DOCKER" = "1" ]; then
    print_step "4) MariaDB сайдкар (Docker compose)"
    MYSQL_HOST=${ORM_MYSQL_HOST:-mariadb}
    MYSQL_PORT=${ORM_MYSQL_PORT:-3306}
    if timeout 2 bash -c "</dev/tcp/${MYSQL_HOST}/${MYSQL_PORT}" 2>/dev/null; then
        echo "   ✓ MariaDB е достъпна на ${MYSQL_HOST}:${MYSQL_PORT}"
    else
        echo "   ⚠ MariaDB не отговаря на ${MYSQL_HOST}:${MYSQL_PORT} — бонусът ще се пропусне"
    fi
else
    print_step "4) (Бонус) Опит за MariaDB Docker контейнер"
    if command -v docker >/dev/null 2>&1; then
        if docker info >/dev/null 2>&1; then
            if docker compose -f "$ORM_ROOT/docker/docker-compose.yml" up -d mariadb 2>&1 | tail -2; then
                echo "   Изчакваме MariaDB да отговори (до 30 s)..."
                for i in $(seq 1 30); do
                    if timeout 1 bash -c '</dev/tcp/127.0.0.1/3307' 2>/dev/null; then
                        echo "   ✓ MariaDB е достъпна на 127.0.0.1:3307"
                        break
                    fi
                    sleep 1
                done
            fi
        else
            echo "   ⚠ Docker daemon не отговаря — бонусът ще се пропусне"
        fi
    else
        echo "   ⚠ docker не е инсталиран в тази WSL дистрибуция — бонусът ще се пропусне"
    fi
fi

printf '\n\033[1;32m✓ Готово. Изпълнете на сцената:\033[0m\n'
printf '   bash %s\n' "$DEMO_DIR/run_demo_a.sh"
printf '   bash %s\n' "$DEMO_DIR/run_demo_b.sh"
