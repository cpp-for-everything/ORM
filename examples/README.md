# Примери / демонстрации за устната защита

Самостоятелни примери, всеки от които доказва една ос от приноса. **Демо 1–6** са
header-only — без драйвери, без база: **компилаторът е валидаторът**. **Демо 7** е
живо — изпълнява една заявка срещу реални SQLite / PostgreSQL / MySQL / MongoDB.

## Бърз старт

```bash
bash examples/run_demos.sh          # пуска всички (1–7) едно по едно
bash examples/run_demos.sh 1 6 7    # само избрани (по номер)
```

Скриптът сам намира (или изтегля) Boost.PFR и засича дали `std::chrono::utc_clock`
е наличен. За всяко демо показва: позитивният вариант **компилира и се изпълнява**,
а негативният е **отказ на компилатора** (четим `static_assert`).

### Изисквания
- C++23 компилатор: **g++ 14+** или **clang 17+** (`CXX=g++-14 bash examples/run_demos.sh` за избор).
- **Демо 1**: `sqlite3` (системен или Homebrew — намира се автоматично).
- **Демо 7**: **Docker** (вдига 4-те бази от `docker/docker-compose.yml`).

## Примерите

| # | Файл | Доказва | Режими |
|---|---|---|---|
| 1 | `01_capability_gating.cpp` | договор за способности + **явен `.join`**: SQLite приема JOIN, MongoDB го отказва при компилация | PASS / `-DORM_DEMO_DB_MONGODB` |
| 2 | `02_query_anatomy.cpp` | заявката е `constexpr` стойност, не низ (инварианти през `static_assert`) | PASS |
| 3 | `03_type_safety.cpp` | тип поле↔стойност при сравнение се сверява при компилация | PASS / `-DDEMO_REJECT` |
| 4 | `04_param_arity.cpp` | брой/тип на аргументите на `execute()` спрямо placeholder-ите | PASS / `-DDEMO_REJECT` |
| 5 | `05_backend_shape.cpp` | Redis налага достъп само по първичен ключ при компилация | PASS / `-DDEMO_REJECT` |
| 6 | `06_relationships.cpp` | `relationship` движи заявката → авто-JOIN (INNER/LEFT/RIGHT/FULL + многозвенно), рендериран през MockDB | PASS |
| 7 | `07_live_relationships.cpp` | **една заявка → 4 живи бази**: SQL `INNER JOIN` × Mongo `$lookup`, идентичен `joined_row` | живо (Docker) |
| 8 | `08_async_task.cpp` | бонус: корутинен `Task<T>` модел | PASS |

## Как се компилира едно демо ръчно

Header-only (Демо 2–6), от корена на хранилището:

```bash
c++ -std=c++23 -I lib/include -I <път-до-boost_pfr>/include \
    -DORM_HAS_REFLECTION=0 -DORM_HAS_UTC_CLOCK=0 \
    examples/06_relationships.cpp -o demo && ./demo
```

(`ORM_HAS_UTC_CLOCK=1` за Linux/libstdc++; `0` за macOS/libc++. `run_demos.sh` го засича сам.)

Негативният вариант (Демо 3/4/5) — добави `-DDEMO_REJECT`; за Демо 1 — `-DORM_DEMO_DB_MONGODB`.
Очаква се да **НЕ** компилира, с четим `static_assert`.

Демо 7 (живо):

```bash
bash doc/v2/bg_presentation/demo/07_live_relationships/run_live.sh
```

## Синтаксис на заявките (за справка)

**Авто-JOIN през `relationship` (препоръчан — Демо 6/7).** Връзката дефинира FK колона;
избор на поле от свързана таблица извежда JOIN-а:

```cpp
struct Book { ...; relationship<store_as::reference<&Author::id>, "author_id"> author_id; };
auto q = select(field<&Book::title>, field<&Author::name>);   // → INNER JOIN / $lookup
```

**Явен `.join<Mode, Table>(rule)` (Демо 1).** Все още се поддържа — пълен контрол над
условието; gating-ът по способности важи (релационен приема, MongoDB отказва):

```cpp
auto q = select(field<&User::id>, field<&Post::body>)
    .join<orm::join::mode::inner, Post>(field<&User::id> == field<&Post::author_id>);
// съкратено, ако `inner` е в обхват:  using enum orm::join::mode;  .join<inner, Post>(...)
```

## Бележки
- Тези файлове **огледало** са на `doc/v2/bg_presentation/demo/0X_…/` (там стоят README-та
  и предварително уловените изходи за слайдовете). Тук са събрани за бърз достъп на сцената.
- `compile_commands.json` се генерира от `CMakeLists.txt` (`cmake -B build && cmake --build build`)
  за автодопълване в редактор; за самата защита `run_demos.sh` е достатъчен.
