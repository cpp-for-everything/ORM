// A LIST WRITTEN INTO THE QUERY. in() takes a PlaceholderList and nothing else,
// so a container of values has no way into the query.
#include "common.hpp"
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string> values{argv[argc - 1]};
    auto rule = orm::in(orm::field<&Item::name>, values);   // must not compile
    (void)rule;
    return 0;
}
