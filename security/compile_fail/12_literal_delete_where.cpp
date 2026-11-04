// LITERAL OPERAND, in DELETE ... WHERE. Measured before the fix: nothing was
// deleted.
#include "common.hpp"

int main(int argc, char**)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    int id = argc;
    auto q = orm::deleteq<Item>().where(orm::field<&Item::id> == id);
    (void)db.execute(q);   // must not compile
    return 0;
}
