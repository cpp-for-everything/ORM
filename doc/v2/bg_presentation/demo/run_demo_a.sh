#!/usr/bin/env bash
# ============================================================================
#  ДЕМО A — Компилаторът като първи валидатор
# ============================================================================
#  Опаковка над g++, която компилира examples/01_capability_gating.cpp първо
#  срещу MongoDB (трябва да се провали с четим static_assert), след това
#  срещу SQLite (трябва да премине и да се изпълни).
#
#  Source файлът живее в examples/ на корена на хранилището --- същият,
#  който се изгражда и от examples/CMakeLists.txt. Този скрипт е алтернативен
#  път за защитата (по-зрелищен в терминал).
#
#  Изисквания:
#    - WSL Ubuntu 24.04 с g++ 14
#    - Изграден build-wsl-uring/ (за пътя до boost_pfr)
# ============================================================================

set -uo pipefail

ORM_ROOT=${ORM_ROOT:-/mnt/d/GitHub/ORM}
BUILD_DIR=${BUILD_DIR:-$ORM_ROOT/build-wsl-uring}
SOURCE_FILE="$ORM_ROOT/examples/01_capability_gating.cpp"
# PFR_INC може да бъде override-нат отвън (например в docker-entrypoint.sh
# е настроен на /opt/boost_pfr/include, който е pre-cached в образа).
PFR_INC=${PFR_INC:-$BUILD_DIR/_deps/boost_pfr-src/include}

CXX=${CXX:-g++}
CXXFLAGS=(-std=c++23 -O0 -fdiagnostics-color=always
          -I "$ORM_ROOT/lib/include"
          -I "$PFR_INC"
          -DORM_HAS_REFLECTION=0
          -DORM_HAS_UTC_CLOCK=1)

print_step() {
    printf '\n\033[1;34m== %s ==\033[0m\n' "$*"
}

if [ ! -f "$SOURCE_FILE" ]; then
    echo "ГРЕШКА: не намирам $SOURCE_FILE" >&2
    exit 1
fi

print_step "1) Компилираме примера срещу MongoDB --- очакваме провал"
set +e
$CXX "${CXXFLAGS[@]}" -DORM_DEMO_DB_MONGODB \
     "$SOURCE_FILE" \
     -o /tmp/demo_a_mongo 2>&1 | head -25
rc=$?
set -e
if [ $rc -eq 0 ]; then
    echo "ГРЕШКА: MongoDB вариантът компилира, а не би трябвало."
    exit 1
fi
printf '\n\033[1;32m✓ Компилаторът отказа JOIN срещу MongoDB (както трябваше).\033[0m\n'

print_step "2) Превключваме на SQLite --- очакваме компилация и изпълнение"
$CXX "${CXXFLAGS[@]}" \
     "$SOURCE_FILE" \
     -lsqlite3 \
     -o /tmp/demo_a_sqlite

printf '\n\033[1;32m✓ Компилацията премина. Изпълняваме:\033[0m\n'
/tmp/demo_a_sqlite
