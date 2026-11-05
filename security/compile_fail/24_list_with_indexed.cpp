// A LIST BESIDE AN INDEXED PLACEHOLDER. A list expands to a run time number of
// positions, so an index fixed at compile time could point at the wrong one. This
// is the parameter confusion of the literal operand case, closed before it opens.
#include "common.hpp"
#include <functional>
#include <vector>

int main()
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::in(orm::field<&Item::name>, orm::PlaceholderList<std::string>{}))
                           .where(orm::field<&Item::id> == orm::ph<int, std::placeholders::_2>);
    (void)db.execute(q, std::vector<std::string>{"a"}, 1);   // must not compile
    return 0;
}
