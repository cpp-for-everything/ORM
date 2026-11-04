// LEVEL 2, identifiers. The table name is a static constexpr member of a trait.
// It cannot be computed from anything decided at run time.
#include "common.hpp"

std::string table_from_request();

struct Other
{
    orm::property<int, "id"> id;
};

namespace orm
{
    template <>
    struct table_name_trait<Other>
    {
        static constexpr std::string_view value = table_from_request();   // must not compile
    };
} // namespace orm

int main() { return 0; }
