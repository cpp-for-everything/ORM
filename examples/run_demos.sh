#!/usr/bin/env bash
# ============================================================================
#  run_demos.sh — пуска ВСИЧКИ демонстрации за устната защита, едно по едно.
# ----------------------------------------------------------------------------
#  Демо 1–6 са header-only (без драйвери): компилаторът е валидаторът. За всяко
#  показваме, че позитивният вариант КОМПИЛИРА и се ИЗПЪЛНЯВА, а негативният е
#  ОТКАЗ на компилатора (четим static_assert). Демо 7 е ЖИВО — изпълнява една
#  заявка срещу реални SQLite / PostgreSQL / MySQL / MongoDB през Docker и
#  печата данните.
#
#  Употреба:
#    bash examples/run_demos.sh           # всички
#    bash examples/run_demos.sh 6 7       # само избрани (по номер)
#
#  Изисквания: C++23 компилатор (g++ 14 / clang 17+). Демо 1 ползва sqlite3.
#  Демо 7 ползва Docker. Boost.PFR се намира автоматично (или се изтегля).
# ============================================================================
set -uo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
CXX=${CXX:-c++}

# std::chrono::utc_clock е наличен на libstdc++ (Linux/g++) но не на libc++ (macOS) —
# засичаме го, иначе ORM ползва fallback.
UTC=0
printf '#include <chrono>\nint main(){(void)std::chrono::utc_clock::now();}\n' \
    | $CXX -std=c++23 -x c++ - -o /tmp/orm_utc_probe 2>/dev/null && UTC=1

COMMON="-std=c++23 -O0 -I $REPO/lib/include -DORM_HAS_REFLECTION=0 -DORM_HAS_UTC_CLOCK=$UTC"

# ── Boost.PFR: намери в дървото или изтегли в кеш ───────────────────────────
PFR=$(find "$REPO" -path '*boost_pfr-src/include' -type d 2>/dev/null | head -1)
if [ -z "$PFR" ]; then
    CACHE="${TMPDIR:-/tmp}/orm_demo_pfr"
    [ -d "$CACHE/include" ] || { echo "Изтегляне на Boost.PFR 2.2.0 …";
        git clone --depth 1 -b 2.2.0 https://github.com/boostorg/pfr "$CACHE" >/dev/null 2>&1; }
    PFR="$CACHE/include"
fi
COMMON="$COMMON -I $PFR"

# ── sqlite3 флагове (за Демо 1) ─────────────────────────────────────────────
if pkg-config --exists sqlite3 2>/dev/null; then SQLITE=$(pkg-config --cflags --libs sqlite3)
elif [ -d /opt/homebrew/opt/sqlite ];     then SQLITE="-I/opt/homebrew/opt/sqlite/include -L/opt/homebrew/opt/sqlite/lib -lsqlite3"
else                                            SQLITE="-lsqlite3"; fi

ok()  { printf '\033[1;32m   ✓ %s\033[0m\n' "$*"; }
bad() { printf '\033[1;31m   ✗ %s\033[0m\n' "$*"; }
hdr() { printf '\n\033[1;44m  %s  \033[0m\n' "$*"; }

run_pass()   { local src=$1 extra=${2:-}
    if $CXX $COMMON $extra "$HERE/$src" -o /tmp/orm_demo 2>/tmp/orm_err
        then ok "PASS компилира → изпълнение:"; /tmp/orm_demo
        else bad "НЕ компилира (неочаквано):"; sed -n '1,6p' /tmp/orm_err; fi; }

run_reject() { local src=$1 extra=${2:-}
    if $CXX $COMMON $extra "$HERE/$src" -o /tmp/orm_demo 2>/tmp/orm_err
        then bad "компилира, а НЕ би трябвало — gating-ът не сработи!"
        else ok "REJECT — компилаторът отказа (както трябва):"
             grep -m1 -iE 'static.?assert|error:' /tmp/orm_err | sed 's/^/     /'; fi; }

SELECTED=("$@")                       # избрани демота по номер (празно = всички)
want() {                              # ползва глобалните SELECTED + SEL; bash 3.2-safe
    [ "${#SELECTED[@]}" -eq 0 ] && return 0
    local n; for n in "${SELECTED[@]}"; do [ "$n" = "$SEL" ] && return 0; done
    return 1
}
demo() { SEL=$1; shift; want || return 1; hdr "$@"; }

demo 1 "Демо 1 — Capability gating + явен .join<inner, Post>" && {
    run_pass   01_capability_gating.cpp "$SQLITE"
    run_reject 01_capability_gating.cpp -DORM_DEMO_DB_MONGODB; }
demo 2 "Демо 2 — Заявката е constexpr стойност, не низ" && run_pass 02_query_anatomy.cpp
demo 3 "Демо 3 — Тип поле↔стойност при компилация" && {
    run_pass   03_type_safety.cpp
    run_reject 03_type_safety.cpp -DDEMO_REJECT; }
demo 4 "Демо 4 — Брой/тип на параметрите при компилация" && {
    run_pass   04_param_arity.cpp
    run_reject 04_param_arity.cpp -DDEMO_REJECT; }
demo 5 "Демо 5 — Redis: само по първичен ключ" && {
    run_pass   05_backend_shape.cpp
    run_reject 05_backend_shape.cpp -DDEMO_REJECT; }
demo 6 "Демо 6 — relationship движи заявката (авто-JOIN, MockDB)" && run_pass 06_relationships.cpp
demo 7 "Демо 7 — Една заявка, четири ЖИВИ бази (Docker)" && {
    if command -v docker >/dev/null 2>&1
        then bash "$REPO/doc/v2/bg_presentation/demo/07_live_relationships/run_live.sh"
        else bad "Docker липсва — пропуснато. Виж 07_live_relationships.cpp."; fi; }

printf '\n\033[1;32m✔ Готово.\033[0m  Демо 1–6: компилаторът е валидаторът.  Демо 7: жива multi-store заявка.\n'
