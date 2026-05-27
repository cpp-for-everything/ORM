# Калибриращ експеримент с PostgreSQL за асинхронния модел

Дизайнов документ за валидиращ микро-бенчмарк, който допълва съществуващия
`benchmarks/bench_async.cpp`. Този документ описва **само дизайна**; не съдържа
имплементация на C++ код и не модифицира съществуващите бенчмаркове или
текстовете на дипломната работа.

Свързани файлове:

- `benchmarks/bench_async.cpp` — текущ синтетичен бенчмарк със `SimulatedAsyncIO`
- `lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_db.hpp` — `PostgreSQLDB`
  (синхронен ORM tag със стъб `MockPGconn`, не пипа мрежата)
- `lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_live.hpp` —
  `PostgreSQLLiveDB` (синхронен libpq през `PQexecParams`)
- `lib/include/ORM/db/connectors/PostgreSQLDB/postgresql_async.hpp` —
  `AsyncPostgreSQLDB` (роден неблокиращ libpq през `PQconnectPoll`,
  `PQsendQueryParams`, `PQflush`, `PQisBusy`, `PQconsumeInput`, плюс
  `co_await ctx.watch_readable/watch_writable`)
- `lib/include/ORM/connector/async_db.hpp` — универсалният офлоуд `async_db<DB>`
  (използва `run_on_pool` за бекенди без `supports_async`)
- `tests/integration/test_postgresql_live.cpp` — съществуваща интеграция, гейтвана
  чрез CMake опцията `ORM_POSTGRESQL_LIVE_ENABLED` и env променливите
  `ORM_POSTGRESQL_HOST/PORT/USER/PASSWORD/DATABASE`
- `docker/docker-compose.yml` — съдържа готов сервиз `postgresql` (image
  `postgres:15`, порт `5432`, парола `test_password`, db `test_db`)
- `benchmarks/CMakeLists.txt` — `FetchContent` на `google/benchmark v1.8.3`

## 1. Цел на калибриращия експеримент

Симулационният бенчмарк в `bench_async.cpp` измерва архитектурно предимство:
докато един coroutine е в `await_suspend` чрез `SimulatedAsyncIO`, работните
нишки на `orm::ThreadPool` се освобождават и могат да обслужват други
coroutines. Полученото ускорение (3.38× при 10 µs до 23.36× при 1000 µs изчакване,
Таблица \ref{tab:throughput_results} в `chapters/04_implementation.tex`) обаче
почива на `std::this_thread::sleep_for` срещу `TimerQueue`-управляван
`co_await` — т.е. на детерминирано микросекундно изчакване в отделна нишка.

Калибриращият експеримент тества **една единствена хипотеза**: че същата
архитектурна форма се възпроизвежда, когато фазата на изчакване е реално
мрежово RTT към PostgreSQL процес (а не контролиран таймер), при еквивалентна
дължина на изчакването. Изпълнението върху `AsyncPostgreSQLDB` (роден
неблокиращ libpq) трябва да покаже **същата качествена тенденция** като
`BM_AsyncThroughput` срещу `BM_SyncThroughput`, и то в зоната, в която
очакваното loopback RTT (~50–200 µs) пресича симулационната скала. Това е
калибриране, не оценка: не цели да измери абсолютна пропускливост на
PostgreSQL, а да покаже, че измерената с прокси архитектурна полза не е
артефакт на прокси-то.

## 2. Хипотеза и фалшифицируеми твърдения

**H1 (доминираща фаза на изчакване).** При workload, при който мрежовото RTT
доминира над локалната CPU работа на coroutine-а (типично RTT 100–200 µs за
loopback PG, CPU работа ~1 µs), асинхронната пътека `AsyncPostgreSQLDB +
IoContext` постига **поне 2× по-висока пропускливост (ops/sec)** от
синхронната `PostgreSQLLiveDB + run_on_pool` пътека при същия брой работни
нишки.

- Фалшифициране: ако измереното отношение е < 1.5× в режима с 4 нишки и
  N=1000 заявки, моделът на офлоуд-в-пул надвишава или съответства на родния
  IoContext и H1 не се потвърждава.
- Контекстна референция: `bench_async.cpp` отчита 22.29× при сходна
  конфигурация {4, 1000, 100 µs}. Дори половината от това би било силно
  потвърждение, защото реалното PG RTT е по-вариативно от
  `std::this_thread::sleep_for`.

**H2 (нулева фаза на изчакване).** Когато workload-ът се сближава до тесен
локален CPU цикъл (без istinsko мрежово запитване — напр. `SELECT 1` извикано
неколкократно в локален цикъл, без RTT измерване), отношението
асинхронна/синхронна пропускливост се движи в същия диапазон като Таблица
\ref{tab:zero_latency_results} (1.18× до 0.63×).

- Фалшифициране: отношение > 1.5× при нулева RTT би означавало, че libpq-то
  внася несинхронна форма дори при минимална мрежа, което би било изненада.
  Отношение < 0.3× би било индикация за непропорционален overhead в
  `IoContext` цикъла при PostgreSQL, който заслужава отделно изследване.

## 3. Експериментален дизайн

Три бенчмарк варианта, всеки реализиран през Google Benchmark в нов файл
`benchmarks/bench_pg_calibration.cpp` (документиран тук, **не имплементиран**).
Общ workload: N coroutine задачи, всяка една изпълнява `SELECT id, name FROM
users WHERE id = $1` срещу подготвена таблица със 100 000 реда. Параметърът
варира псевдослучайно в диапазона `[1, 100000]`. Връща се една редица.

### 3.1 `bench_pg_sync` — синхронен baseline

```pseudocode
state.range = {threads, tasks, query_kind}
pool = orm::ThreadPool(threads)
sync_conns = vector<PostgreSQLLiveDB>(threads)   // pre-warmed
for each iteration:
    latch = CompletionLatch(tasks)
    for i in 0..tasks:
        pool.post([&] {
            conn = sync_conns[thread_local_index]
            result = orm::db<PostgreSQLLiveDB>(conn).execute(prepared_q, param(i))
            DoNotOptimize(result)
            latch.count_down()
        })
    latch.wait()
state.SetItemsProcessed(iterations * tasks)
```

Това възпроизвежда сценария на `BM_SyncThroughput`, но `sleep_for` е заменен с
реално `PQexecParams`.

### 3.2 `bench_pg_async_native` — роден IoContext

```pseudocode
state.range = {threads, tasks, query_kind}
ctx = IoContext::create(threads)
ctx_thread = std::thread([&] { ctx->run(); })
async_conns = vector<AsyncPostgreSQLDB>(threads)   // pre-warmed via AsyncPostgreSQLDB::connect(...).sync_wait()
for each iteration:
    latch = CompletionLatch(tasks)
    for i in 0..tasks:
        auto t = [&](int idx) -> Task<void> {
            PGresult* r = co_await pg_async_detail::exec_async(
                async_conns[idx % threads],
                prepared_sql,
                { std::to_string(param(idx)) });
            DoNotOptimize(r);
            PQclear(r);
            latch.count_down();
            co_return;
        }(i);
        ctx->post([h = t.release()] { h.resume(); });
    latch.wait()
ctx->stop(); ctx_thread.join();
state.SetItemsProcessed(iterations * tasks)
```

Това е целевата валидираща пътека: `co_await ctx.watch_readable/writable`
освобождава работната нишка на `IoContext` точно както `SimulatedAsyncIO`
освобождава работна нишка на `ThreadPool`.

### 3.3 `bench_pg_async_pool` — универсален офлоуд (sanity)

```pseudocode
pool = orm::ThreadPool(threads)
sync_conns = vector<PostgreSQLLiveDB>(threads)
adb = orm::async_db<PostgreSQLLiveDB>(sync_conns[0], pool)
for each iteration:
    latch = CompletionLatch(tasks)
    for i in 0..tasks:
        auto t = [&](int idx) -> Task<void> {
            auto r = co_await adb.async_execute(prepared_q, param(idx));
            DoNotOptimize(r);
            latch.count_down();
            co_return;
        }(i);
        pool.post([h = t.release()] { h.resume(); });
    latch.wait()
```

Очакваме този вариант да седне **между** sync и native async: предимството от
coroutine композиция, но с цената на блокиращо `PQexecParams` в пула. Това е
контролна точка, която изключва хипотезата „асинхронно е по-бързо просто
заради coroutine машинката“.

### 3.4 Сравнителна решетка (sweep)

| Измерение         | Стойности                                              |
|-------------------|--------------------------------------------------------|
| Нишки             | 4, 8                                                   |
| Брой задачи       | 100, 1000                                              |
| Сложност          | (a) PK lookup (`WHERE id = $1`), (b) индексен скан с LIMIT 10 |
| Бенчмарк вариант  | `pg_sync`, `pg_async_native`, `pg_async_pool`          |

Общо $2 \times 2 \times 2 \times 3 = 24$ конфигурации. Метрика: `items/sec`,
`real_time` median + IQR, `iterations`. Изолирана метрика: микро-бенчмарк
`BM_PG_ConnectAcquire` за един `co_await connect()` като контрола на
оверхеда на конекшън handshake.

## 4. Среда и възпроизводимост

### 4.1 PostgreSQL контейнер

Сервизът `postgresql` в `docker/docker-compose.yml` (image `postgres:15`) е
напълно достатъчен. Бенчмаркът ще се пуска срещу следната дефиниция,
извлечена от съществуващия compose файл (без модификация):

```yaml
postgresql:
  image: postgres:15
  environment:
    POSTGRES_PASSWORD: test_password
    POSTGRES_DB: test_db
  ports:
    - "5432:5432"
  healthcheck:
    test: ["CMD-SHELL", "pg_isready -U postgres"]
```

За калибриращия експеримент препоръчваме `image: postgres:16` за
актуалност, със същите credentials. Конекшън стрингът се изгражда от същите
env променливи както `test_postgresql_live.cpp`:

```
host=127.0.0.1 port=5432 user=postgres password=test_password dbname=bench_db
```

### 4.2 Подготовка на схемата (еднократно, преди бенчмарка)

```sql
CREATE DATABASE bench_db;
\c bench_db
CREATE TABLE users (
    id    INTEGER PRIMARY KEY,
    name  TEXT NOT NULL,
    email TEXT NOT NULL
);
CREATE INDEX idx_users_email ON users(email);
INSERT INTO users (id, name, email)
    SELECT g, 'user' || g, 'u' || g || '@bench.local'
    FROM generate_series(1, 100000) g;
ALTER TABLE users SET (autovacuum_enabled = false);
VACUUM ANALYZE users;
```

### 4.3 Системни настройки

- **CPU афинитет.** Бенчмарк процесът се закача за конкретни ядра чрез
  `taskset -c 2,3,4,5` (Linux) или `Start-Process -Affinity` (Windows), за да
  се изключи влиянието на планировчика на ОС.
- **CPU governor.** На Linux: `cpupower frequency-set -g performance`; turbo
  изключено (`echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo`).
- **Изолация.** PostgreSQL контейнерът се закача за различни ядра
  (`docker run --cpuset-cpus="0,1"`), за да не споделя L1/L2 кеш с бенчмарка.
- **Warmup.** Първите 100 итерации се отхвърлят (Google Benchmark `MinTime` +
  ръчно `state.PauseTiming()` за първите итерации, или предварителен
  `__pgbench warmup` слой), за да се стабилизират libpq cache, plan cache и
  PG shared buffers.

### 4.4 Статистическа методика

Google Benchmark с `->Repetitions(10)->ReportAggregatesOnly(true)`. Отчитаме
**медиана и IQR** (не средно ± stddev, защото мрежовите RTT-та са
несиметрично разпределени). Цялото изпълнение се повтаря в **три отделни
сесии** на различни дни; крайният доклад използва най-консервативната
(най-ниска) асинхронна стойност и най-благоприятната (най-висока) синхронна
стойност — обратна на cherry-picking.

## 5. Контрол на смущаващи фактори

| Фактор                          | Мерки за изолиране                                                                                                   |
|---------------------------------|------------------------------------------------------------------------------------------------------------------------|
| Мрежов jitter                   | Само loopback (`127.0.0.1`); никакъв физически network stack. Контейнерът тече на същия хост, без `-p` пренасочване ако е възможно. |
| Connection pool warmup          | Всички `PostgreSQLLiveDB` и `AsyncPostgreSQLDB` се установяват в `SetUp`; никакво ново свързване по време на бенчмарка. |
| Statement plan caching          | Веднъж в `SetUp` се изпълнява `PREPARE bench_lookup AS SELECT ...` и `EXPLAIN (FORMAT JSON) EXECUTE bench_lookup(1)`; планът се записва, а в `TearDown` се сравнява втора `EXPLAIN`-снимка, за да се потвърди, че няма re-planning. |
| PG query planner variance       | `SET LOCAL plan_cache_mode = force_generic_plan;` преди бенчмарка, така че `EXECUTE` винаги използва същия generic plan. |
| Autovacuum                      | `ALTER TABLE users SET (autovacuum_enabled = false);` за `bench_db`. Базата се пресъздава между сесиите. |
| Background activities (PG)      | `synchronous_commit = off`, `fsync = off` за `bench_db` (приемливо за read-only бенчмарк); `checkpoint_timeout = 1h` за да няма checkpoint по време на измерване. |
| libpq connection overhead       | Пред-установени pool от $N_{threads}$ конекции; `AsyncPostgreSQLDB::connect` се синхронизира еднократно преди `for (auto _ : state)`. |
| ОС планировчик                  | `taskset` или `--cpuset-cpus`; `chrt -f 50` за real-time приоритет ако е възможно. |
| Thermal throttling              | Преди всяка repetition пауза от 2s + контрол на температурата чрез `sensors` (Linux). |

## 6. Очаквани резултати и тяхното тълкуване

### 6.1 Прогнозна крива

Калибрираме спрямо две котви от `chapters/04_implementation.tex`:

- **Котва 1 (Таблица \ref{tab:throughput_results}):** при изчакване 100 µs и
  4 нишки/1000 задачи синтетичното ускорение е 22.29×.
- **Котва 2 (Таблица \ref{tab:zero_latency_results}):** при 0 изчакване и
  4 нишки/100 задачи отношението е 1.18×; при 8/100 пада до 0.63×.

Loopback RTT за PostgreSQL `SELECT id, name FROM users WHERE id = $1` се очаква в
порядъка **80–200 µs** (наша оценка въз основа на libpq round-trip ~20 µs +
PG plan execution ~50–150 µs за PK lookup в напълно затоплена база).
Следователно очакваме за `pg_async_native` спрямо `pg_sync`:

| Конфигурация                       | Очаквано ускорение  |
|-----------------------------------|----------------------|
| 4 нишки, 1000 задачи, PK lookup   | 8×–20× (по-консервативно от 22.29×, заради libpq overhead) |
| 4 нишки, 100 задачи, PK lookup    | 4×–10×                |
| 4 нишки, 1000 задачи, index scan  | 6×–18×                |
| 8 нишки, 100 задачи               | 2×–5× (както и синтетичното 4.53×) |

За `pg_async_pool` (универсален офлоуд) очакваме то да седи в средата:
1.2×–3× над `pg_sync`, защото предимството идва само от композиционната
форма, не от истинско освобождаване на нишка.

### 6.2 Какво означава „калибрирано“

Експериментът се счита за **успешен** ако:

1. Прогнозираната зона на ускорение (8×–20× за 4/1000/PK) се потвърждава с
   медиана **в нея или над нея**.
2. `pg_async_pool` се намира **строго между** `pg_sync` и `pg_async_native`
   (т.е. редът е sync < pool offload < native async).
3. При H2 (нулева чиста изчакваща фаза, симулирана с локален CPU цикъл вместо
   PG заявка), отношението си остава 0.6×–1.2×, в съгласие с Таблица 7.

### 6.3 Аномалии за по-нататъшно изследване

- `pg_async_native` < 1.5× от `pg_sync` при високо RTT → намек, че libpq-то
  държи глобален мутекс или че `IoContext` имплементацията се сериализира на
  една нишка; разследване чрез `perf record` и `epoll_wait` tracing.
- `pg_async_pool` > `pg_async_native` → внасяне на дефект в native пътека
  (вероятно `PQflush` се вика прекалено често); валидация чрез
  `strace -e trace=poll,epoll_wait`.
- Голяма дисперсия (IQR > 30 % от медианата) → check за autovacuum триене,
  checkpoint, или thermal throttling.

## 7. Препоръчителен текст за вмъкване в дипломната работа

Следният параграф **не е добавян** към `chapters/04_implementation.tex`
(съгласно ограничението на тази задача). Той е препоръчан за бъдеща ревизия
като последен параграф в подсекция \ref{subsec:verification},
`\subsubsection{Покритие и ограничения на верификацията}`, и е формулиран
**като бъдеща работа**, не като завършена дейност.

> Естественото продължение на представената емпирична оценка е добавянето на
> калибриращ експеримент срещу жива инстанция на PostgreSQL чрез вече
> разработените съединители `PostgreSQLLiveDB` (синхронен libpq през
> `PQexecParams`) и `AsyncPostgreSQLDB` (роден неблокиращ libpq през
> `PQsendQueryParams`, `PQflush`, `PQisBusy` и `co_await
> ctx.watch_readable/watch_writable`). Експериментът би повторил схемата на
> `bench_async.cpp`, но с реално мрежово RTT по интерфейса loopback в
> диапазона приблизително 80–200~$\mu$s — точно в зоната, в която
> синтетичното изчакване от 100~$\mu$s показва ускорение от 22.29~$\times$.
> Целта не е измерване на абсолютна пропускливост на PostgreSQL, а
> калибриране на наблюдаваното архитектурно предимство: ако кривата за
> жив PostgreSQL легне в същия порядък като синтетичната, синтетичните числа
> придобиват външна емпирична потвърдимост. Този експеримент е документиран
> в `doc/v2/bg/experiments/postgresql_calibration_design.md` и е предвиден
> за бъдеща ревизия на дисертацията.

(154 думи в препоръчителния параграф.)

## 8. Оценка на разходите

### 8.1 Инженерно усилие

| Дейност                                                              | Оценка       |
|----------------------------------------------------------------------|--------------|
| `benchmarks/bench_pg_calibration.cpp` (3 бенчмарка + sweep)          | ~350–450 LOC |
| Допълване на `benchmarks/CMakeLists.txt` с `find_package(PostgreSQL)` гейт | ~25 LOC |
| Скрипт за seed на `bench_db` (`scripts/seed_bench_pg.sql`)           | ~20 LOC      |
| Скрипт за пускане + CPU pinning (`scripts/run_pg_calibration.sh`)    | ~40 LOC      |
| Документиране на резултатите в нов LaTeX appendix                    | ~80 реда     |
| **Общо инженерно време**                                              | **~10–14 часа** |

### 8.2 Docker инфраструктура

`docker/docker-compose.yml` вече има готов и здравно-проверяван `postgresql`
сервиз. Преизползваме същия image и credentials. Допълнителна работа: ~30
минути за `scripts/run_pg_calibration.sh`, който вдига само
`docker-compose up postgresql -d`, изчаква health check, изпълнява
seed скрипта, пуска бенчмарк executable-а и в края изпълнява
`docker-compose down`.

### 8.3 CI интеграция

Текущият Dockerfile target `postgresql-test` (виж `docker/Dockerfile` ред 62)
вече инсталира `libpq-dev` и компилира `ORM_ENABLE_POSTGRESQL=ON`.
Добавянето на бенчмарк target:

- Нов CMake флаг `ORM_BUILD_PG_CALIBRATION_BENCH` (default `OFF`).
- Нов CI job `bench-postgresql` в `.github/workflows/` (или еквивалент),
  който: (1) вдига compose сервиза, (2) build-ва бенчмарка, (3) пуска го с
  `--benchmark_format=json --benchmark_out=results.json`, (4) качва
  `results.json` като artifact.
- Опционално: nightly cron, не блокиращ PR-и, защото микросекундните
  измервания са чувствителни към натоварването на CI runner-а.
- Допълнителни ~50 реда YAML.

### 8.4 Времева рамка

- Ден 1 (4 часа): имплементация и локална калибрация.
- Ден 2 (4 часа): sweep и стабилизация на изолацията.
- Ден 3 (3 часа): три повторителни сесии + статистика.
- Ден 4 (3 часа): запис на резултати и LaTeX appendix.

Общо ~14 човеко-часа за пълен валидиращ цикъл, **без** да се променя
архитектурата на ORM библиотеката или съществуващия `bench_async.cpp`.
