#!/usr/bin/env bash
# ============================================================================
#  docker-entrypoint.sh --- автоматично изпълнение на двете демонстрации
# ============================================================================
#  Пуска се от `docker compose up`. Прави следното, с малко изход:
#    1. Сглобява examples/ (за Демо A) ако не е кеширано.
#    2. Сглобява bench_async + MariaDB пробата (за Демо B) ако не е
#       кеширано.
#    3. Изпълнява Демо A (compile-time gating).
#    4. Изпълнява Демо B (бенчмарк + жива MariaDB проба).
# ============================================================================

set -uo pipefail

ORM_ROOT=${ORM_ROOT:-/work}
BUILD_DIR=${BUILD_DIR:-$ORM_ROOT/build-docker}
DEMO_DIR="$ORM_ROOT/doc/v2/bg_presentation/demo"

banner() {
    printf '\n\033[1;36m═══════════════════════════════════════════════════════════════════\033[0m\n'
    printf '\033[1;36m  %s\033[0m\n' "$*"
    printf '\033[1;36m═══════════════════════════════════════════════════════════════════\033[0m\n'
}

step() { printf '\n\033[1;34m▸ %s\033[0m\n' "$*"; }

# ── 1. Сглобяване на examples/ (за Демо A) ────────────────────────────────
banner "Подготвителна стъпка --- сглобяване"
step "examples/ (за Демо A)"
cd "$ORM_ROOT/examples"
if [ ! -f build/build.ninja ]; then
    cmake -G Ninja -B build -DORM_EXAMPLES_BUILD_FAILING=OFF 2>&1 | tail -3
fi
# Билдваме само не-failing target-ите --- _mongodb_fails е умишлено
# счупен, не бива да отказва нашия CI/auto-run.
ninja -C build 01_capability_gating 02_query_anatomy 03_relationships 04_async_task 2>&1 | tail -3

# ── 2. Сглобяване на bench_async + MariaDB проба (за Демо B) ──────────────
step "build-docker/ (bench_async + MariaDB проба, всички живи конектори)"
cd "$ORM_ROOT"
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    # Всички конектори се активират — всичките им клиентски библиотеки са
    # инсталирани в образа (виж Dockerfile, секции 1, 3, 4). Това гарантира,
    # че `find_package` няма да фейлне за нито един от тях; самото линкване
    # се прави само за target-ите, които наистина ги ползват (test_*_live).
    cmake -G Ninja -B "$BUILD_DIR" \
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
        -DORM_LINUX_REACTOR=epoll 2>&1 | tail -3
fi
ninja -C "$BUILD_DIR" bench_async test_mysql_async_coroutine_probe 2>&1 | tail -3

# ── 3. Демо A ─────────────────────────────────────────────────────────────
banner "ДЕМО A --- Компилаторът като първи валидатор"
bash "$DEMO_DIR/run_demo_a.sh"

# ── 4. Демо B ─────────────────────────────────────────────────────────────
banner "ДЕМО B --- Корутинен async + жива MariaDB"
bash "$DEMO_DIR/run_demo_b.sh"

banner "Готово"
printf 'За интерактивна работа: docker compose run --rm demo bash\n\n'
