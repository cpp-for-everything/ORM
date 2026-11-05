#pragma once
// ── MockDB DDL support for orm::migrate<MockMigrateDB> ────────────────────────
// Include this header after both mock_db.hpp and migration.hpp.
//
// Usage in tests:
//   #include "ORM/db/connectors/MockDB/mock_db.hpp"
//   #include "ORM/db/migration/migration.hpp"
//   #include "ORM/db/connectors/MockDB/mock_db_migration.hpp"
//
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/db/migration/migration.hpp"
#include <string>
#include <vector>

namespace orm {

    // ── MockMigrateDB — MockDB plus a record of the DDL it was asked to run ───
    struct MockMigrateDB : MockDB
    {
        std::vector<std::string> executed_ddl;
    };

    template <>
    struct connector_trait<MockMigrateDB>
    {
        using supports_joins               = void;
        using supports_transactions        = void;
        using supports_aggregation         = void;
        using supports_concurrent_execute  = void;
        using supports_constexpr_sql       = void;

        template <typename T>
        struct wire_type { using type = T; };

        struct cursor_type
        {
            [[nodiscard]] bool has_next() const noexcept { return false; }
        };

        // Forward all execute() calls to MockDB's implementations
        template <typename... Args>
        static auto execute(MockMigrateDB& db, Args&&... args)
        {
            return connector_trait<MockDB>::execute(
                static_cast<MockDB&>(db), std::forward<Args>(args)...);
        }

        static void begin(MockMigrateDB& db)    { static_cast<MockDB&>(db).last_sql = "BEGIN"; }
        static void commit(MockMigrateDB& db)   { static_cast<MockDB&>(db).last_sql = "COMMIT"; }
        static void rollback(MockMigrateDB& db) { static_cast<MockDB&>(db).last_sql = "ROLLBACK"; }

        // ── DDL ───────────────────────────────────────────────────────────────
        [[nodiscard]] static constexpr sql_dialect ddl_dialect() noexcept
        {
            return {'"', alter_type_form::alter_column_type, true};
        }

        static void execute_ddl(MockMigrateDB& db, const std::string& sql)
        {
            db.executed_ddl.push_back(sql);
        }

        template <typename Query>
        static constexpr std::string_view render_constexpr(Query)
        {
            return "SELECT id FROM ?";
        }
    };

} // namespace orm
