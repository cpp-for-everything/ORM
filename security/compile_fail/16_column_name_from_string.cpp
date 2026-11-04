// LEVEL 2, identifiers. The way a developer would naturally try it: a column
// name held in a std::string. It is not even the right type for a column name,
// so it is rejected before the question of constness arises.
#include "common.hpp"

int main(int argc, char** argv)
{
    std::string column = argv[argc - 1];   // e.g. "password"
    orm::property<int, column> probe;      // must not compile
    (void)probe;
    return 0;
}
