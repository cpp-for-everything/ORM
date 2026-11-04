// LITERAL OPERAND, in UPDATE ... SET. Measured before the fix: the one run time
// argument was bound into the literal's slot, the WHERE slot was left NULL, and
// the row was not updated.
#include "common.hpp"

int main(int argc, char** argv)
{
    orm::MockDB conn;
    orm::db<orm::MockDB> db{conn};
    std::string value = argv[argc - 1];
    auto q = orm::update<Item>()
                 .set(orm::field<&Item::name>, value)
                 .where(orm::field<&Item::id> == orm::Placeholder<int>{});
    (void)db.execute(q, 2);   // must not compile
    return 0;
}
