// A CONNECTOR WITHOUT A DDL PATH. MockDB renders queries but declares no dialect, like
// every connector that has no DDL support: a migration on it does not compile.
#include "common.hpp"
#include "ORM/db/migration/migration.hpp"

int main()
{
    orm::MockDB conn;
    orm::migrate<orm::MockDB> mig{conn};   // must not compile
    (void)mig;
    return 0;
}
