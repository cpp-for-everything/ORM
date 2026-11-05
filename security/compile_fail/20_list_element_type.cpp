// ELEMENT TYPE AGAINST THE COLUMN. A list of strings for an int column.
#include "common.hpp"

int main()
{
    auto rule = orm::in(orm::field<&Item::id>, orm::PlaceholderList<std::string>{});   // must not compile
    (void)rule;
    return 0;
}
