// LEVEL 3, structure. A predicate supplied as text. where() accepts only rules
// built from fields, so a string is rejected before anything is rendered.
#include "common.hpp"

int main()
{
    auto q = orm::select(orm::field<&Item::id>)
                 .where("name = 'x' OR '1'='1'");   // must not compile
    (void)q;
    return 0;
}
