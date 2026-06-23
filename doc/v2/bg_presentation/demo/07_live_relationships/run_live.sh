#!/usr/bin/env bash
# ============================================================================
#  ДЕМО 7 — Една заявка, четири живи хранилища, през docker-compose
# ----------------------------------------------------------------------------
#  Компилира live_relationships.cpp срещу всеки backend и го пуска СРЕЩУ ЖИВА
#  база, вдигната от docker/docker-compose.yml. Всеки backend се сглобява със
#  своята клиентска библиотека (sqlite3 / libpq / libmariadb / libmongoc) в
#  съответния per-DB образ — но C++ заявката е една и съща.
#
#  Употреба (от свеж клон, само Docker — macOS / Linux / Windows):
#    bash doc/v2/bg_presentation/demo/07_live_relationships/run_live.sh
# ============================================================================
set -uo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# Изкачваме се до корена на хранилището (там, където е docker/docker-compose.yml).
ORM_ROOT=$SCRIPT_DIR
while [ "$ORM_ROOT" != / ] && [ ! -f "$ORM_ROOT/docker/docker-compose.yml" ]; do
    ORM_ROOT=$(dirname "$ORM_ROOT")
done
COMPOSE=(docker compose -f "$ORM_ROOT/docker/docker-compose.yml")

# Всеки backend е самостоятелен single-quoted bash блок: хардкоднат /repo път,
# inline търсене на Boost.PFR (вече изтеглен в per-DB образа), и backend-специфичните
# -D дефиниции / клиентска библиотека. C++ заявката в .cpp е една и съща.
run() { "${COMPOSE[@]}" run --rm -v "$ORM_ROOT:/repo" --entrypoint bash "$1" -c "$2"; echo; }

printf '\n\033[1;34m== Една заявка: select(field<&Book::title>, field<&Author::name>) ==\033[0m\n'
printf '   Резултат: std::vector<orm::joined_row<Book, Author>> — за ВСЕКИ backend.\n'

run test-sqlite '
  PFR=$(find /workspace /repo -path "*boost_pfr-src/include" -type d 2>/dev/null | head -1)
  g++ -std=c++23 -O0 -I /repo/lib/include -I "$PFR" -DORM_HAS_REFLECTION=0 -DORM_HAS_UTC_CLOCK=1 \
      -DORM_DEMO_SQLITE /repo/doc/v2/bg_presentation/demo/07_live_relationships/live_relationships.cpp \
      -lsqlite3 -o /tmp/d && /tmp/d'

run test-postgresql '
  PFR=$(find /workspace /repo -path "*boost_pfr-src/include" -type d 2>/dev/null | head -1)
  g++ -std=c++23 -O0 -I /repo/lib/include -I "$PFR" -I "$(pg_config --includedir)" \
      -DORM_HAS_REFLECTION=0 -DORM_HAS_UTC_CLOCK=1 -DORM_POSTGRESQL_LIVE_AVAILABLE -DORM_DEMO_POSTGRESQL \
      /repo/doc/v2/bg_presentation/demo/07_live_relationships/live_relationships.cpp \
      -lpq -o /tmp/d && /tmp/d 2>/dev/null'

run test-mysql '
  PFR=$(find /workspace /repo -path "*boost_pfr-src/include" -type d 2>/dev/null | head -1)
  g++ -std=c++23 -O0 -I /repo/lib/include -I "$PFR" $(mysql_config --include) \
      -DORM_HAS_REFLECTION=0 -DORM_HAS_UTC_CLOCK=1 -DORM_MYSQL_LIVE_AVAILABLE -DORM_DEMO_MYSQL \
      /repo/doc/v2/bg_presentation/demo/07_live_relationships/live_relationships.cpp \
      $(mysql_config --libs) -o /tmp/d && /tmp/d'

run test-mongodb '
  PFR=$(find /workspace /repo -path "*boost_pfr-src/include" -type d 2>/dev/null | head -1)
  g++ -std=c++23 -O0 -I /repo/lib/include -I "$PFR" \
      -DORM_HAS_REFLECTION=0 -DORM_HAS_UTC_CLOCK=1 -DORM_MONGODB_LIVE_AVAILABLE -DORM_DEMO_MONGODB \
      /repo/doc/v2/bg_presentation/demo/07_live_relationships/live_relationships.cpp \
      $(pkg-config --cflags --libs libmongoc-1.0) -o /tmp/d && /tmp/d'

printf '\033[1;32mЕдна заявка → SQL INNER JOIN за релационните бази, $lookup за MongoDB,\n'
printf 'идентичен joined_row<Book, Author> резултат. Релация × SQL × NoSQL — един модел.\033[0m\n'
