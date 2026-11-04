// CONTROL, must compile. Comparison with nullptr is a deliberate IS NULL test,
// not a literal value, and the literal operand check must not reject it.
#include "common.hpp"

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::field<&Item::name> == nullptr);
    db << q;
    return 0;
}
