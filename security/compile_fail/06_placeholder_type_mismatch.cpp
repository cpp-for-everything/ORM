// LEVEL 1, values. A placeholder is typed. A string cannot be bound where an int
// is expected, so a whole family of coercion tricks never reaches run time.
#include "common.hpp"

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    (void)db.execute(q, std::string("1 OR 1=1"));   // must not compile
    return 0;
}
