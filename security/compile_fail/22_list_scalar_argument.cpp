// A SCALAR WHERE A LIST BELONGS. One string, not a list of them. A comma separated
// string passed here is how the concatenating habit would come back.
#include "common.hpp"

int main(int argc, char** argv)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::in(orm::field<&Item::name>, orm::PlaceholderList<std::string>{}));
    (void)db.execute(q, std::string(argv[argc - 1]));   // must not compile
    return 0;
}
