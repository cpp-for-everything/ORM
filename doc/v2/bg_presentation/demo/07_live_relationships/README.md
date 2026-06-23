# Демо 7 — Една заявка, четири живи хранилища (relationship на живо)

**Какво доказва:** релационно-осъзнатата заявка не е само compile-time рендериране
(това е Демо 6 през MockDB) — тя **се изпълнява срещу реални бази** и връща частични
обекти. Един C++ модел + една C++ заявка → **SQL `INNER JOIN`** за релационните бази и
**`$lookup` агрегация** за MongoDB, с **идентичен** `joined_row<Book, Author>` резултат.

Това е прякото доказателство на централната теза „един модел, много системи за
съхранение" — върху **жив** SQL **и** жив NoSQL.

## Един модел, една заявка (споделени от всички backend-и)

```cpp
struct Author { property<int,"id"> id; property<std::string,"name"> name; };
struct Book   { property<int,"id"> id; property<std::string,"title"> title;
                relationship<store_as::reference<&Author::id>, "author_id"> author_id; };

constexpr auto q = orm::select(orm::field<&Book::title>, orm::field<&Author::name>);
auto rows = (db << q).to_vector();      // std::vector<joined_row<Book, Author>>
```

Backend-ът е единственото, което се сменя — избира се при компилация:
`-DORM_DEMO_SQLITE | -DORM_DEMO_POSTGRESQL | -DORM_DEMO_MYSQL | -DORM_DEMO_MONGODB`.

## Стартиране (само Docker — еднакво на macOS / Linux / Windows)

```bash
bash doc/v2/bg_presentation/demo/07_live_relationships/run_live.sh
```

Скриптът вдига SQLite (in-process) + PostgreSQL + MySQL/MariaDB + MongoDB от
`docker/docker-compose.yml`, компилира `live_relationships.cpp` със съответната
клиентска библиотека във всеки per-DB образ и го пуска срещу живата база.

| Backend | Превод на JOIN-а | Изход |
|---|---|---|
| SQLite (вграден) | `INNER JOIN authors ON books.author_id = authors.id` | 2 реда |
| PostgreSQL | `INNER JOIN ...` | 2 реда |
| MySQL / MariaDB | `INNER JOIN ...` | 2 реда |
| MongoDB | `$lookup { from: authors, localField: author_id, foreignField: id }` | 2 реда |

Пълният изход е в `program-output.txt`.

## Какво да кажете (≈30 s)

> Демо 6 показа, че релацията се **превежда** при компилация. Тук същата заявка се
> **изпълнява** — срещу четири реални бази, всяка в свой контейнер. PostgreSQL, MySQL и
> SQLite получават SQL `INNER JOIN`; MongoDB получава `$lookup` агрегация. И четирите
> връщат един и същ `joined_row<Book, Author>` с частично хидратирани обекти. Един
> модел, една заявка, релационни и документни бази — без нито един ред backend-специфичен
> код в приложението. Всичко върви „out of the box" през Docker, идентично на всяка ОС.
