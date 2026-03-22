#pragma once
// ── MockDB DDL extensions for orm::migrate<MockDB> ────────────────────────────
// Include this header after both mock_db.hpp and migration.hpp to get
// ddl_for() implementations for connector_trait<MockDB>.
//
// Usage in tests:
//   #include "ORM/db/connectors/MockDB/mock_db.hpp"
//   #include "ORM/db/migration/migration.hpp"
//   #include "ORM/db/connectors/MockDB/mock_db_migration.hpp"
//
#include "ORM/db/connectors/MockDB/mock_db.hpp"
#include "ORM/db/migration/migration.hpp"
#include <format>

namespace orm {

    // ── Inject ddl_for() into connector_trait<MockDB> via a separate specialisation
    // extension (non-template static methods added via an out-of-class definition
    // is not possible in C++; use a free-function bridge instead).

    namespace mockdb {

        [[nodiscard]] inline std::string ddl_for(const create_table_op& op)
        {
            std::string cols;
            for (std::size_t i = 0; i < op.columns.size(); ++i)
            {
                if (i > 0) cols += ", ";
                cols += op.columns[i].first + " " + op.columns[i].second;
            }
            return std::format("CREATE TABLE {} ({})", op.table, cols);
        }

        [[nodiscard]] inline std::string ddl_for(const add_column_op& op)
        {
            return std::format("ALTER TABLE {} ADD COLUMN {} {}",
                op.table, op.column, op.type);
        }

        [[nodiscard]] inline std::string ddl_for(const drop_column_op& op)
        {
            return std::format("ALTER TABLE {} DROP COLUMN {}",
                op.table, op.column);
        }

        [[nodiscard]] inline std::string ddl_for(const alter_column_type_op& op)
        {
            return std::format("ALTER TABLE {} ALTER COLUMN {} TYPE {}",
                op.table, op.column, op.new_type);
        }

    } // namespace mockdb

    // ── MockMigrateDB — a thin wrapper around MockDB that adds ddl_for() ─────
    // Since C++ does not allow adding methods to an existing template specialisation
    // after the fact, we provide connector_trait<MockMigrateDB> which inherits all
    // MockDB capabilities and also satisfies the ddl_for() extension point.
    struct MockMigrateDB : MockDB {};

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

        // ── DDL generation ────────────────────────────────────────────────────
        [[nodiscard]] static std::string ddl_for(const create_table_op& op)
        {
            return mockdb::ddl_for(op);
        }
        [[nodiscard]] static std::string ddl_for(const add_column_op& op)
        {
            return mockdb::ddl_for(op);
        }
        [[nodiscard]] static std::string ddl_for(const drop_column_op& op)
        {
            return mockdb::ddl_for(op);
        }
        [[nodiscard]] static std::string ddl_for(const alter_column_type_op& op)
        {
            return mockdb::ddl_for(op);
        }

        template <typename Query>
        static constexpr std::string_view render_constexpr(Query)
        {
            return "SELECT id FROM ?";
        }
    };

} // namespace orm
