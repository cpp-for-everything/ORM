// LEVEL 2, identifiers. Stronger than constness. The column name is part of the
// property's type, so two columns of the same C++ type, here both int, are
// still different types. One variable cannot hold "either column", which is
// what choosing a column from input would need.
#include "common.hpp"

int main(int argc, char**)
{
    auto column = (argc > 1) ? &Item::owner : &Item::id;   // must not compile
    (void)column;
    return 0;
}
