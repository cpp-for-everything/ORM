// CONTROL, must compile. The declared form of a migration on a connector that has one.
#include "common.hpp"
#include "ORM/db/migration/migration.hpp"
#include "ORM/db/connectors/MockDB/mock_db_migration.hpp"

int main()
{
    orm::MockMigrateDB conn;
    orm::migrate<orm::MockMigrateDB> mig{conn};
    const orm::entity_meta meta = orm::entity_meta_of<&Item::id, &Item::name>();
    (void)mig.run({}, std::span<const orm::entity_meta>(&meta, 1));
    return 0;
}
