// A LIST AS AN UPDATE VALUE. SET takes one value per column.
#include "common.hpp"
#include <vector>

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    auto q = orm::update<Item>()
                 .set(orm::field<&Item::name>, orm::PlaceholderList<std::string>{})
                 .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    (void)db.execute(q, std::vector<std::string>{"a"}, 1);   // must not compile
    return 0;
}
