// A LIST WITH NOTHING TO SUPPLY IT. operator<< takes no arguments, so the list
// could only render unbound.
#include "common.hpp"

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::in(orm::field<&Item::name>, orm::PlaceholderList<std::string>{}));
    (void)(db << q);   // must not compile
    return 0;
}
