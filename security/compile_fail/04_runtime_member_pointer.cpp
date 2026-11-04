// LEVEL 2, identifiers. The column to filter on, held in a run time variable.
// The variable always points at the same column, so its type is fine. It is
// rejected only because a value that is not a constant cannot be a template
// argument. (17 shows that a variable cannot even range over two columns.)
#include "common.hpp"

int main(int argc, char**)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    auto column = &Item::id;           // a run time variable
    if (argc > 1) column = &Item::id;  // assigned on a path decided at run time
    auto q = orm::select(orm::field<&Item::id>)
                 .where(orm::field<column> == orm::Placeholder<int>{});   // must not compile
    (void)db.execute(q, 1);
    return 0;
}
