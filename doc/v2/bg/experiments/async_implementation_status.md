# Статус на реализацията на неблокиращите (native async) пътища по системи за съхранение

## 1. Цел и обхват

Настоящият документ ревизира едно конкретно твърдение от Глава~4 на дипломната работа, според което PostgreSQL е единствената система за съхранение, за която асинхронната интеграция е реализирана докрай чрез неблокиращия интерфейс на драйвера, а останалите backend-и използват само универсалния обединителен (thread-pool) път. Целта на одита е да се установи фактическото състояние на кода в момента на защитата, да се документират разликите между подразбиращата се ("default") универсална реализация (`async_db<DB>` чрез `run_on_pool(...)`) и конкретните native async пътеки, и да се идентифицират драйверните дефекти, които ограничават реализацията.

Обхватът е ограничен до:

- Backend-ите под `lib/include/ORM/db/connectors/` в текущата ревизия (clone на `D:\GitHub\ORM`, клон `shorter-documentation`).
- Специализациите на `connector_trait<DB>` и наличието на `using supports_async = void;`.
- Файлове `*_async.hpp` за реалната native async реализация.
- Реакторният слой `IoContext` в `lib/include/ORM/async/io_context.hpp` и неговите конкретни реализации в `lib/src/ORM/async/`.
- Два конкретни upstream дефекта в MariaDB/MariaDB Connector-C, флагирани от автора.

Не се ревизират: цикълът на жизнения цикъл на `Task<T>` (изпипан в Глава~4); работата на обединението от нишки (`ThreadPool`); миграционният слой; контрактните тестове на `connector_trait` (те се отнасят до синхронния път).

## 2. Текуща карта на асинхронната реализация по системи за съхранение

| Backend | Sync тип | Async тип | `supports_async` декларация | Native non-blocking API в кода | Универсален път (thread-pool) | Известни ограничения |
|---|---|---|---|---|---|---|
| SQLite | `SQLiteDB` (`sqlite_db.hpp:22`) | — (липсва `*_async.hpp`) | не | няма | `async_db<SQLiteDB>` → `run_on_pool` (`async_db.hpp:46-50`) | SQLite е процесно вграден; драйверът няма non-blocking API. |
| PostgreSQL | `PostgreSQLDB` (mock, `postgresql_db.hpp`) и `PostgreSQLLiveDB` (`postgresql_live.hpp:29`) | `AsyncPostgreSQLDB` (`postgresql_async.hpp:23`) | да (`postgresql_async.hpp:161`) | `PQconnectStart`, `PQconnectPoll`, `PQsetnonblocking`, `PQsendQueryParams`, `PQflush`, `PQisBusy`, `PQconsumeInput`, `PQgetResult` (`postgresql_async.hpp:62-149`) | резервен `execute(...)` чрез блокиращ `PQexec` в `connector_trait<AsyncPostgreSQLDB>` (`postgresql_async.hpp:233-251`) | реализиран изцяло без известни ограничения. |
| MySQL | `MySQLDB` (mock) и `MySQLLiveDB` (`mysql_live.hpp:374`) | `AsyncMySQLDB` (`mysql_async.hpp:22`) | да (`mysql_async.hpp:168`) | `mysql_options(..., MYSQL_OPT_NONBLOCK, ...)`, `mysql_real_connect_start`/`_cont`, `mysql_real_query_start`/`_cont`, `mysql_store_result_start`/`_cont`, `mysql_get_socket` (`mysql_async.hpp:67-156`) | резервен синхронен `execute(...)` чрез `mysql_real_query` + `mysql_store_result` (`mysql_async.hpp:222-257`) | константно ограничен от драйвера върху Windows/Schannel TLS — PR #310 (open). На POSIX системи реализацията е пълна, но не е покрита от автоматизиран тест. |
| Cassandra | `CassandraDB` (mock) и `CassandraLiveDB` (`cassandra_live.hpp:24`) | `AsyncCassandraDB` (`cassandra_async.hpp:60`) | да (`cassandra_async.hpp:141`) | `cass_session_connect_keyspace`, `cass_session_execute`, `cass_future_set_callback`, `cass_future_ready`, `cass_future_get_result` (`cassandra_async.hpp:23-127`) | резервен sync `execute(...)` с `cass_future_wait` (`cassandra_async.hpp:194-238`) | callback се изпълнява в нишка на драйвера, а не в `IoContext` — документирано ограничение, не дефект. |
| Redis | `RedisDB` (mock) и `RedisLiveDB` (`redis_live.hpp:26`) | `AsyncRedisDB` (`redis_async.hpp:131`) | да (`redis_async.hpp:197`) | `redisAsyncConnect`, `redisAsyncCommandArgv`, `redisAsyncHandleRead`/`Write` (`redis_async.hpp:69-191`) | резервен sync `execute(...)` хвърля `runtime_error` ("AsyncRedisDB does not support synchronous execute", `redis_async.hpp:239`) | адаптерът към `IoContext` (`redis_adapter::attach`) използва `watch_readable(fd, callback)`/`watch_writable(fd, callback)` (`redis_async.hpp:34, 44`), но `IoContext` декларира само `watch_readable(int fd) -> Task<void>` (`io_context.hpp:75`) — двуаргументните претоварвания липсват, така че адаптерът не се компилира в текущата ревизия. |
| MongoDB | `MongoDB` (mock, `mongodb_db.hpp:45`) и `MongoDBLive` (`mongodb_live.hpp:30`) | — (липсва `*_async.hpp`) | не | няма | `async_db<MongoDBLive>` → `run_on_pool` | `libmongoc` е изцяло синхронен; native non-blocking API не съществува. |
| Neo4j | `Neo4jDB` (mock, `neo4j_db.hpp:54`) и `Neo4jLiveDB` (`neo4j_live.hpp:25`) | — (липсва `*_async.hpp`) | не | няма | `async_db<Neo4jLiveDB>` → `run_on_pool` | `libneo4j-client` е изцяло синхронен. Bolt протоколът би позволил пряко асинхронно изпълнение, но това би изисквало собствена реализация. |
| MockDB | `MockDB` (`mock_db.hpp:?`) | — | не (декларира само `supports_concurrent_execute`, `supports_constexpr_sql` и др., `mock_db.hpp:251-257`) | няма | `async_db<MockDB>` → `run_on_pool`; именно този път се покрива от `tests/unit/test_async_connector.cpp` | предназначен само за тестове. |

Допълнително: `IoContext::create(...)` (`io_context.hpp:92`) на ниво изграждане избира различен изходен файл според платформата (`lib/src/ORM/async/CMakeLists.txt:6-12`): `kqueue_context.cpp` за macOS, `uring_context.cpp` за Linux, `iocp_context.cpp` за Windows. Във файловата система реално присъства само `kqueue_context.cpp` (`lib/src/ORM/async/kqueue_context.cpp`); `uring_context.cpp` и `iocp_context.cpp` са посочени в CMake, но не съществуват. Това означава, че native non-blocking пътищата компилират и работят само на macOS — на Linux и Windows реакторният слой няма реализация и кодът не би се свързал. Това е критично за оценката на "до каква степен е реализирано".

## 3. Анализ по backend

### 3.1 PostgreSQL — пълна native реализация (потвърдено)

`AsyncPostgreSQLDB::connect(conninfo, ctx)` (`postgresql_async.hpp:57-96`) задвижва крайния автомат на `libpq`: `PQconnectStart` инициира връзката неблокиращо; цикъл `PQconnectPoll` изчаква `PGRES_POLLING_OK`, като при `PGRES_POLLING_WRITING`/`PGRES_POLLING_READING` корутината се спира чрез `co_await ctx.watch_writable(PQsocket(...))`/`co_await ctx.watch_readable(...)`; накрая се извиква `PQsetnonblocking(db.conn_, 1)`. За изпълнение на заявка `pg_async_detail::exec_async` (`postgresql_async.hpp:109-150`) използва `PQsendQueryParams`, цикъл `PQflush`+`watch_writable`, цикъл `PQisBusy`+`watch_readable`+`PQconsumeInput`, и накрая `PQgetResult` плюс източване на оставащите резултати. Това е канонично коректно. Резервен синхронен `execute(...)` (`postgresql_async.hpp:215-251`) използва блокиращо `PQexec`, но той е алтернатива при използване на `db<AsyncPostgreSQLDB>` извън `async_db` — не на native пътя. Заключение: реализацията е native, цялостна, без известни ограничения отвъд платформената наличност на `IoContext` (виж §3.7).

### 3.2 MySQL — пълна native реализация, но с upstream дефект на Windows/Schannel

`AsyncMySQLDB::connect(...)` (`mysql_async.hpp:56-91`) активира `MYSQL_OPT_NONBLOCK` и завърта стандартния `_start`/`_cont` цикъл с `mysql_get_socket` + `watch_readable`/`watch_writable` според маски `MYSQL_WAIT_READ`/`MYSQL_WAIT_WRITE`. Помощникът `mysql_async_detail::query_async` (`mysql_async.hpp:126-157`) реализира `mysql_real_query_start`/`_cont` и `mysql_store_result_start`/`_cont` по същия начин. Това отговаря на стратегията, описана в `docs/superpowers/specs/2026-04-06-async-connectors-02-per-library-strategy.md:9-66`. Реализацията е native (не `run_on_pool`). Известно ограничение: на Windows с Schannel TLS MariaDB Connector/C не насочва шифровани четения/записи към async dispatcher-а, което води до "Lost connection during query" — виж §4.2 и PR #310. На POSIX системи няма знаен ограничителен дефект. Кодът обаче не е покрит от автоматизирани тестове (`tests/unit/test_async_connector.cpp` използва само `MockDB`); `test_mysql_live.cpp` е синхронен.

### 3.3 Cassandra — native callback-to-coroutine мост (потвърдено)

`AsyncCassandraDB::connect(...)` (`cassandra_async.hpp:109-128`) използва `cass_session_connect_keyspace` и изчаква резултата чрез `CassFutureAwaitable` (`cassandra_async.hpp:23-53`). Awaiter-ът регистрира `cass_future_set_callback`, който при готовност на бъдещата стойност извиква `std::coroutine_handle<>::resume()`. `connector_trait<AsyncCassandraDB>::async_execute(...)` (`cassandra_async.hpp:155-190`) изпълнява `cass_statement_new` + `cass_session_execute` + `co_await CassFutureAwaitable{future}`, след което извлича резултата с `cass_future_get_result`. DataStax драйверът поддържа собствен пул от I/O нишки, така че native асинхронното изпълнение е свойство на драйвера, а не на нашия `IoContext`. Това е документирано ограничение: callback се изпълнява на нишка на драйвера, а не на тази на `IoContext` (вж. `docs/superpowers/specs/2026-04-06-async-connectors-02-per-library-strategy.md:569-577`). Реализацията е native, без upstream дефекти.

### 3.4 Redis — native API е използван, но IoContext адаптерът не компилира

`AsyncRedisDB::connect(host, port, ctx)` (`redis_async.hpp:165-185`) използва `redisAsyncConnect` и закача нашия `redis_adapter::attach`, който замества `addRead`/`delRead`/`addWrite`/`delWrite` обратни извиквания на hiredis с такива, които предполагат `IoContext::watch_readable(fd, callback)` и `watch_writable(fd, callback)`. Командата се изпълнява чрез `RedisCommandAwaitable` (`redis_async.hpp:69-127`), който вътре извиква `redisAsyncCommandArgv` и възобновява корутината от callback-а на драйвера.

Проблемът: `IoContext` в `lib/include/ORM/async/io_context.hpp:75-89` дефинира само едноаргументен coroutine-базиран `watch_readable(int fd) -> Task<void>` и `watch_writable(int fd) -> Task<void>`; двуаргументни претоварвания `watch_readable(int fd, std::function<void()> cb)` няма. Следователно `redis_adapter::add_read` и `add_write` (`redis_async.hpp:31-49`) се позовават на несъществуващи методи и адаптерът не би се компилирал, ако файлът беше включен в активен таргет. На практика никой обект на компилацията не включва `redis_async.hpp` — той не присъства в `tests/unit/CMakeLists.txt`, нито в интеграционни тестове, нито в библиотечните таргети. Заключение: дизайнът е native (hiredis async API), но интеграцията с реакторния слой не е довършена и кодът ще откаже компилация при първото му използване извън упражнение на хедъра. Това трябва да се отрази честно — Redis native пътят е в "почти завършено" състояние, но не е оперативно валидиран.

### 3.5 SQLite — без native път (експлицитно)

SQLite е процесно вграден, не мрежов; драйверът не предоставя non-blocking API, защото блокирането се случва на ниво файлово-системни четения и записи. В кода липсва `sqlite_async.hpp`. Потребителят може да използва `async_db<SQLiteDB>`, но статичното разклонение `if constexpr (has_capability<DB, cap::supports_async>)` в `async_db<DB>::operator<<` (`async_db.hpp:41-51`) ще избере универсалния път `run_on_pool`. Алтернативите, описани в `docs/superpowers/specs/2026-04-06-async-connectors-02-per-library-strategy.md:969-1043` (отделна "writer thread", използване на WAL за паралелни четения, евентуално `io_uring` за директен файлов вход-изход) са предвидени, но не реализирани.

### 3.6 MongoDB и Neo4j — без native път (правилно по дизайн)

`libmongoc` и `libneo4j-client` нямат публичен non-blocking интерфейс. Драйверите управляват вътрешни нишки/пулове, но не предоставят socket accessor или callback API, които биха позволили интеграция с външен `IoContext`. Същото като при SQLite, `async_db<MongoDBLive>` и `async_db<Neo4jLiveDB>` използват универсалния `run_on_pool` път. Това е приемливо архитектурно решение, защото обединителният модел гарантира неблокиране на викащата корутина, дори ако драйверът блокира. Алтернативата за Neo4j би била собствена реализация на Bolt протокола — голям обем работа, изричен post-MVP елемент в плановите документи.

### 3.7 Платформена готовност на `IoContext`

В `lib/src/ORM/async/CMakeLists.txt` се вижда, че `IoContext::create(...)` зависи от платформено-специфичен изходен файл. Във хранилището присъства само `kqueue_context.cpp` (macOS). `uring_context.cpp` и `iocp_context.cpp` са препратени в CMake, но физически не съществуват, така че таргетът `orm::async` ще се построи само на macOS; на Linux и Windows ще пропадне на свързването, освен ако CMake пропусне native path-а и активира INTERFACE-fallback (което редом с това оставя `IoContext::create` без дефиниция и затова също не би проработило).

Тази платформена непълнота директно ограничава тезата за "пълна native реализация" дори за PostgreSQL и MySQL: без `IoContext` за Linux/Windows тяхната async реализация не е изпълнима в реална CI/production среда. Това е важна добавка към реалното състояние на проекта и тя не е отразена в текста на Глава~4.

## 4. Случаят MariaDB / MySQL — известни драйверни дефекти

Авторът флагира два upstream PR-а. Тяхното изследване показва следното.

### 4.1 PR https://github.com/MariaDB/server/pull/4991

**Заглавие (verbatim):** "MDEV-39453: mtr: make main.group_by MDEV-6129 UNION query deterministic".

**Резюме:** Регресионен тест за MDEV-6129 изпълнява `SELECT 1 AS test UNION SELECT 2 AS test ORDER BY test IS NULL ASC;`. Тъй като `test IS NULL = 0` за двата реда, ключът за `ORDER BY` е константа и редът на връщаните стойности е неопределен. Под паралелно изпълнение на MTR (MariaDB Test Runner) понякога сървърът връща `2,1` вместо очакваното `1,2`, което води до спорадичен flake.

**Засегнат код:** `mysql-test/main/group_by.test` — добавя се директива `--sorted_result` пред заявката.

**Статус на сливане:** Затворен (closed), не е слят. На 27 април 2026 maintainer-ът отбелязва, че същата корекция вече е слята в 10.11 чрез PR #4946 ("MDEV-39333 fix main.group_by test to be ordered predictably"), който се очаква да достигне 11.4 чрез стандартния merge-up процес.

**Връзка с асинхронната реализация в нашия конектор:** **няма**. PR-ът е чисто тестова стабилизация на MTR, не променя сървърен код, не докосва нито `mysql_real_query_start`, нито `mysql_real_connect_start`, нито Schannel/TLS, нито каквато и да е част от non-blocking client API-то.

Хипотеза: авторът вероятно е смесил номера на свой собствен MariaDB PR с друг проблем. Този PR не оправдава никое архитектурно ограничение в нашата реализация. Препоръката към работата е тази препратка да не се цитира като драйверен дефект, ограничаващ async пътя.

### 4.2 PR https://github.com/mariadb-corporation/mariadb-connector-c/pull/310

**Заглавие (verbatim):** "Windows: make Schannel async read/write cooperate with non-blocking client API".

**Резюме (verbatim из описанието):** "This PR carries two commits that together close the last visible gap in the Windows Connector/C async API." Без този fix non-blocking client API-то (`mysql_*_start`/`_cont`) пропада над TLS на Windows със Schannel със симптоми "Lost connection to server during query" или "Lost connection to server at 'reading authorization packet'".

**Двойната причина на бъга:**

1. *Compile-time изключване на TLS async dispatcher-а.* Условието `#if defined(HAVE_TLS) && !defined(HAVE_SCHANNEL)` в `libmariadb/ma_pvio.c` изключва TLS async dispatch-а за Schannel build-ове, така че шифрованият трафик се обработва от пътя за суров socket I/O. Резултат: deformatирани header-и на пакета и закриване на връзката.

2. *Блокиране в async fiber контекст.* Encrypt/decrypt помощниците на Schannel ползват `pvio->methods->read/write`, които при `WSAEWOULDBLOCK` извикват `select()`. Това блокира OS-нишката на fiber-а в async режим и води до deadlock — application fiber-ът никога не получава ход.

**Засегнат код (modified files):** `plugins/pvio/pvio_socket.c` (нов `pvio_socket_wait_or_yield`, който прави `my_context_yield` вместо блокиращ `select`), `libmariadb/ma_pvio.c` (премахва Schannel exclusion от async dispatcher), `libmariadb/secure/schannel.c` (добавя липсващите async function stubs, делегиращи към синхронните), `unittest/libmariadb/CMakeLists.txt` (премахва skip за async тестове на Windows). Общо 4 файла, ~83 добавки и ~11 премахвания.

**Статус на сливане:** **Open** към момента на изследването (24 април 2026 — отворен), target branch `mariadb-corporation:3.4`, source branch `cpp-for-everything:main`. Не е слят. Не е известно в коя версия на Connector-C ще влезе.

**Верификация (verbatim from PR):** "Testing confirms 5/5 async test cases pass on Windows 11 with Schannel TLS (cipher: TLS_AES_256_GCM_SHA384), and downstream integration with a coroute ORM shows 860 assertions across 162 test cases all passing." Тоест авторът е тествал собствения си fix с целевата ORM (тази дипломна работа) и потвърждава, че патчирана Connector-C версия позволява стабилно асинхронно изпълнение.

**Връзка с асинхронната реализация в нашия конектор:** Пряка и сериозна. Нашият `AsyncMySQLDB` използва точно функциите, които без този fix са невъзможни на Windows над TLS: `mysql_real_connect_start`/`_cont` (`mysql_async.hpp:70-79`), `mysql_real_query_start`/`_cont` (`mysql_async.hpp:131-138`), `mysql_store_result_start`/`_cont` (`mysql_async.hpp:146-153`). Докато PR #310 не е слят и не е освободен в стабилна версия на Connector-C, MySQL async пътят:

- Работи коректно на POSIX (Linux/macOS) системи, тъй като те ползват други TLS backend-и (OpenSSL/GnuTLS) или нямат описаната Schannel специфика.
- Не работи коректно на Windows над TLS — поведението е недетерминирано "Lost connection" грешки.
- Работи на Windows без TLS, защото проблемът е в Schannel encrypt/decrypt-а.

**Митигация в нашия код:** Не е реализирана експлицитна митигация на това upstream ограничение. Алтернативите биха били: (а) feature-gate на `AsyncMySQLDB` с runtime detection на Windows+Schannel и fallback към `async_db<MySQLLiveDB>::operator<<` (универсалния пул); (б) изискване на patched Connector-C версия. Текущата позиция в кода е "архитектурата е реализирана, но реализацията зависи от поправка нагоре по веригата за пълно покритие на платформите".

## 5. Корекция на твърдението в Глава 4

Текущото изречение в `chapters/04_implementation.tex:59` гласи:

> "Това е единственият случай в дипломната работа, в който интеграцията през неблокиращия интерфейс на драйвера е реализирана докрай; за останалите системи за съхранение този път е валиден архитектурно, но реализацията използва обединителния модел."

Това твърдение е неточно в две посоки: подценява MySQL/Cassandra/Redis (за тях има native реализации, не само архитектурен план), и не отразява upstream ограничението при MySQL на Windows/Schannel, нито факта, че за SQLite/MongoDB/Neo4j native път няма по принцип, а не "не е реализиран".

Предложена замяна (academic tone, Bulgarian):

> "Интеграцията през неблокиращия интерфейс на драйвера е реализирана за повече от една система за съхранение. PostgreSQL е каноничният представителен случай — `AsyncPostgreSQLDB` използва крайния автомат на \code{libpq} (\code{PQconnectPoll}, \code{PQsendQueryParams}, \code{PQflush}, \code{PQisBusy}, \code{PQconsumeInput}) изцяло през реакторния слой \code{IoContext}. По същия модел е реализиран и MySQL (`AsyncMySQLDB` чрез двойките \code{\_start}/\code{\_cont} на MariaDB Connector/C), при който обаче на Windows над TLS със Schannel пълната функционалност зависи от \cite{mariadb_connector_c_pr_310} — открита, но още неслята поправка на драйвера. Cassandra (`AsyncCassandraDB`) използва вече асинхронния модел на DataStax драйвера чрез мост от \code{cass\_future\_set\_callback} към \code{std::coroutine\_handle::resume}. Redis (`AsyncRedisDB`) е базиран върху hiredis async API, но интеграцията на event-loop адаптера с реакторния слой все още не е оперативно валидирана. За SQLite, MongoDB и Neo4j native неблокиращ път не е приложим — съответните драйвери не предоставят такъв интерфейс, така че `async_db<DB>` за тях ползва универсалния обединителен (thread-pool) път през \code{run\_on\_pool}, което е честен инженерен избор."

Този текст:

- Прави експлицитно списъка на realized native пътища (PostgreSQL, MySQL, Cassandra, Redis).
- Не overclaim-ва — отбелязва Windows/Schannel ограничението при MySQL и непълнотата на Redis адаптера.
- Не underclaim-ва — не свежда MySQL/Cassandra до "само архитектурно".
- Не въвежда теми отвъд тези на главата.
- Запазва академичния тон и стиловите конвенции.

Бележка за работата по компилация: ще е необходимо добавяне на bib entry за PR #310 (`mariadb_connector_c_pr_310`).

## 6. Препоръчителна актуализация на Таблица 9 (capability matrix)

Таблицата на редове ~118–142 на `chapters/04_implementation.tex` (`tab:backend_capability_matrix`) описва само релационни маркери — joins, transactions, aggregation, upsert и bulk insert. Тя не съдържа колона за async. Този одит препоръчва **една от двете** алтернативи:

(а) Минимална корекция в колоната "Коментар", за да отрази async статуса:

| Backend | Нов коментар |
|---|---|
| SQLite | "пълен релационен референтен път; async само през пул от нишки (драйверът няма non-blocking API)" |
| PostgreSQL | "силен релационен договор с жива проверка; native async чрез \code{libpq}" |
| MySQL | "оперативен път с диалектни различия; native async чрез \code{\_start}/\code{\_cont}, ограничен на Windows/Schannel" |
| MongoDB | "документен модел без релационни маркери; async само през пул от нишки" |
| Redis | "ключ-стойност с минималистичен договор; native async чрез hiredis, но адаптерът е в развитие" |
| Neo4j | "графова система с акцент върху транзакциите; async само през пул от нишки" |
| Cassandra | "ширококолонен с ограничен набор от способности; native async чрез \code{CassFuture} callback мост" |

(б) Алтернативно — добавяне на отделна колона "Async" с три възможни стойности: "native" / "native (с ограничения)" / "през пул". Тази версия е по-четлива, но изисква преоформяне на таблицата.

И в двете алтернативи остава честно да се отбележи (било в caption или в обкръжаващия текст), че native пътищата на текущата ревизия се компилират и работят само на macOS, защото `lib/src/ORM/async/` съдържа единствено `kqueue_context.cpp` — за Linux/Windows реакторният слой не е реализиран.

---

**Кратко резюме на находките:** Native non-blocking реализации съществуват за PostgreSQL, MySQL, Cassandra и Redis; SQLite, MongoDB и Neo4j по архитектурно решение използват универсалния обединителен път. От четирите native реализации: PostgreSQL е напълно довършена; MySQL е довършена, но на Windows над TLS зависи от MariaDB Connector/C PR #310 (open, неслят); Cassandra е довършена и стабилна; Redis е реализирана концептуално, но `redis_adapter` извиква едни претоварвания на `IoContext::watch_readable/watch_writable`, които не съществуват, и кодът не би се компилирал. Освен това, реакторният слой `IoContext` има конкретна реализация само за macOS (`kqueue_context.cpp`); препратените `uring_context.cpp` (Linux) и `iocp_context.cpp` (Windows) не присъстват във файловата система. MariaDB/server PR #4991 е unrelated към async — той е стабилизация на MTR тест и не оправдава никакво архитектурно ограничение в нашата работа. Документът е готов за консумация на адрес `D:\GitHub\ORM\doc\v2\bg\experiments\async_implementation_status.md`.
