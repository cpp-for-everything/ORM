// A CONNECTOR THAT DOES NOT DECLARE LIST SUPPORT. Redis and Cassandra are in this
// position: IN is refused at compile time rather than rendered badly at run time.
#include "common.hpp"
#include <vector>

struct NoListDB
{
};

namespace orm
{
    template <>
    struct connector_trait<NoListDB>
    {
        template <typename T>
        struct wire_type { using type = T; };
        struct cursor_type { };
        template <typename... A>
        static int execute(NoListDB&, A&&...) { return 0; }
    };
} // namespace orm

int main()
{
    NoListDB conn;
    orm::db<NoListDB> db{conn};
    constexpr auto q = orm::select(orm::field<&Item::id>)
                           .where(orm::in(orm::field<&Item::name>, orm::PlaceholderList<std::string>{}));
    (void)db.execute(q, std::vector<std::string>{"a"});   // must not compile
    return 0;
}
