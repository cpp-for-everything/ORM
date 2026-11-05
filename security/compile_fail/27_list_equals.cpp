// A LIST COMPARED WITH ==. Equality is one value against one value.
#include "common.hpp"

int main()
{
    auto rule = orm::field<&Item::name> == orm::PlaceholderList<std::string>{};   // must not compile
    (void)rule;
    return 0;
}
