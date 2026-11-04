// LITERAL OPERAND, entry point db.execute(). A value written straight into
// where() used to compile, render as an unbound "?", and compare against NULL,
// so the query silently returned no rows. It must now be rejected.
#include "common.hpp"

int main(int argc, char** argv)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    std::string user = argv[argc - 1];
    auto q = orm::select(orm::field<&Item::id>)
                 .where(orm::field<&Item::name> == user);
    (void)db.execute(q);   // must not compile
    return 0;
}
