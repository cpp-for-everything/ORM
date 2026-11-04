// LEVEL 2, identifiers. The column name is a template argument of
// orm::property. This case gives it a value of exactly the right type,
// orm::detail::string_literal, built at run time from input. The only thing
// wrong with it is that it is not a constant, and that alone is enough: it
// cannot name a column. (16 covers the other half: a std::string is not even
// the right type.)
#include "common.hpp"

int main(int argc, char** argv)
{
    char buf[3] = "id";
    buf[0] = argv[argc - 1][0];                    // depends on input
    orm::detail::string_literal<3> column(buf);    // the right type, a run time value
    orm::property<int, column> probe;              // must not compile
    (void)probe;
    return 0;
}
