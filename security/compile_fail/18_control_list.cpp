// CONTROL, must compile. IN and NOT IN written the documented way, with a list
// value that carries a payload as data, next to an ordinary placeholder. If this
// did not compile, the list rejections below could share one unrelated cause.
#include "common.hpp"
#include <vector>

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::in(orm::field<&Item::name>, orm::PlaceholderList<std::string>{}))
                           .where(orm::not_in(orm::field<&Item::id>, orm::PlaceholderList<int>{}))
                           .where(orm::field<&Item::owner> == orm::Placeholder<int>{});
    (void)db.execute(q, std::vector<std::string>{"x' OR '1'='1"}, std::vector<int>{1, 2}, 7);
    return 0;
}
