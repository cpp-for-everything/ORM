// LITERAL OPERAND, entry point db.find_one(), which calls the connector
// directly and so needs its own check.
#include "common.hpp"

int main(int argc, char** argv)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    std::string user = argv[argc - 1];
    auto q = orm::select(orm::field<&Item::id>)
                 .where(orm::field<&Item::name> == user);
    (void)db.find_one(q);   // must not compile
    return 0;
}
