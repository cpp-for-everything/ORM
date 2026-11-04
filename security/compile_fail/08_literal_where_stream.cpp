// LITERAL OPERAND, entry point db << query.
#include "common.hpp"

int main(int argc, char** argv)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    std::string user = argv[argc - 1];
    auto q = orm::select(orm::field<&Item::id>)
                 .where(orm::field<&Item::name> == user);
    db << q;   // must not compile
    return 0;
}
