// CONTROL, must compile. Comparing two columns stores a member pointer on both
// sides, not a value, and the literal operand check must not reject it.
#include "common.hpp"

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::field<&Item::id> == orm::field<&Item::owner>);
    db << q;
    return 0;
}
