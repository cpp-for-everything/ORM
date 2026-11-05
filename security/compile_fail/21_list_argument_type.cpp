// ARGUMENT TYPE AGAINST THE LIST. The query declares strings, the call passes ints.
#include "common.hpp"
#include <vector>

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::in(orm::field<&Item::name>, orm::PlaceholderList<std::string>{}));
    (void)db.execute(q, std::vector<int>{1});   // must not compile
    return 0;
}
