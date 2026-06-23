# Демо 6 — relationship движи заявката (автоматичен JOIN)

**Какво доказва:** `orm::relationship<store_as::reference<&Target::pk>, "fk_col">`
дефинира **истинска FK колона** и носи метаданните за свързване. Избор на поле от
свързана таблица **автоматично извежда JOIN-а при компилация** — включително
многозвенно (Comment → Post → User, с междинна таблица, която не е избрана). Видът на
JOIN-а (INNER/LEFT/RIGHT/FULL) идва от `field` / `optional_field` (матрица 2×2).

Това затваря дупката: преди `relationship` не дефинираше колона и не се ползваше никъде;
сега релационно-осъзнатите заявки са реални.

## Файлове
- `relationships.cpp` — FK relationship-и + автоматично изведени заявки през MockDB.
- `program-output.txt` — изведеният SQL за INNER/LEFT/RIGHT/FULL + многозвенно.

## Команда (без драйвери)
```
c++ -std=c++23 -I lib/include relationships.cpp -o rel && ./rel
```

## Какво извежда компилаторът

| Заявка | Изведен SQL |
|---|---|
| `select(field<&Post::id>, field<&User::name>)` | `… FROM posts INNER JOIN user ON posts.user_id = user.id` |
| `select(field<&Post::id>, optional_field<&User::name>)` | `… LEFT JOIN user …` |
| `select(optional_field<&Post::id>, field<&User::name>)` | `… RIGHT JOIN user …` |
| `select(field<&Comment::body>, field<&User::name>)` | `… comments INNER JOIN posts … INNER JOIN user …` |

## Какво да кажете (≈20 s)
> `relationship<reference<&User::id>, "user_id">` е истинска колона за външен ключ.
> Когато заявка избере поле от свързана таблица, компилаторът обхожда връзките и
> синтезира JOIN-а — за `field<&User::name>` това е INNER JOIN, а `optional_field`
> дава LEFT/RIGHT/FULL по матрица 2×2. И е многозвенно: `Comment → Post → User`
> присъединява `posts` автоматично, макар да не е избрана нито една нейна колона.
> Всичко е изведено и проверено **при компилация**.

> Бележка: рендерирането на JOIN е реализирано (демонстрира се през MockDB); хидратирането
> на резултата в частични обекти и материализацията за всеки конектор е следващата стъпка.
