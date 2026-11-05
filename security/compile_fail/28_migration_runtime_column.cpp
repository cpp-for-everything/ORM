// A RUN TIME COLUMN IN entity_meta_of. The declared form of a migration takes member
// pointers as template arguments, so a column chosen at run time cannot be named.
#include "common.hpp"
#include "ORM/db/migration/ddl.hpp"

int main()
{
    auto column = &Item::id;                       // right type, but not a constant
    auto meta   = orm::entity_meta_of<column>();   // must not compile
    (void)meta;
    return 0;
}
