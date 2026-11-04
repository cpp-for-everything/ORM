// CONTROL, must compile. A query written the documented way: identifiers from
// the entity, the value through a typed placeholder, here carrying an injection
// payload as data. If this did not compile, every rejection in this directory
// could be failing for the same unrelated reason.
#include "common.hpp"

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::field<&Item::name> == orm::Placeholder<std::string>{});
    (void)db.execute(q, std::string("x' OR '1'='1"));
    return 0;
}
