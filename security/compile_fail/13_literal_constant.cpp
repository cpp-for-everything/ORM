// LITERAL OPERAND, even a compile time constant. The defect was never about the
// value being known late: no connector binds a literal at all, so `id > 0`
// compared against NULL exactly like a run time value did.
#include "common.hpp"

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::field<&Item::id> > 0);
    db << q;   // must not compile
    return 0;
}
