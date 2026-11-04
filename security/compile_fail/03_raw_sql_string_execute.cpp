// LEVEL 3, structure. The classic vulnerable pattern: the whole query built as
// text from input and handed over for execution.
#include "common.hpp"

int main(int argc, char** argv)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    std::string name = argv[argc - 1];
    std::string sql  = "SELECT id FROM items WHERE name = '" + name + "'";
    (void)db.execute(sql);   // must not compile: there is no query text entry point
    return 0;
}
