#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/db/migration/ddl.hpp"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace orm {

    // ── migration_state ───────────────────────────────────────────────────────
    enum class migration_state
    {
        Idle, Connected, Diffing, Reviewing, Executing, Done, Failed
    };

    // Whether a migration may drop columns. Refused unless the caller says otherwise,
    // because a dropped column cannot be brought back by running the migration again.
    enum class destructive_ops
    {
        refuse,
        allow
    };

    // ── migrate<DB> ───────────────────────────────────────────────────────────
    // Schema migration tool. Compares registered entities against live schema
    // and generates DDL to bring the database into sync.
    //
    // A connector takes part by providing, in connector_trait<DB>:
    //   ddl_dialect()            the sql_dialect of the engine
    //   execute_ddl(db, sql)     runs ONE statement, and must refuse more than one
    //   read_schema(db)          the live_schema, for sync()
    template <typename DB>
    class migrate
    {
        static_assert(requires { connector_trait<DB>::ddl_dialect(); },
                      "orm: this connector has no DDL path. It does not provide "
                      "connector_trait<DB>::ddl_dialect().");

    public:
        explicit migrate(DB& conn) : conn_(conn) {}

        // ── diff — compute ordered list of DDL operations ─────────────────────
        [[nodiscard]] std::vector<ddl_op> diff(
            const live_schema&              live,
            std::span<const entity_meta>    entities) const
        {
            std::vector<ddl_op> ops;

            for (const auto& entity : entities)
            {
                auto live_it = live.find(entity.table_name);
                if (live_it == live.end())
                {
                    // Table missing entirely — CREATE TABLE
                    ops.push_back(create_table_op{entity.table_name, entity.columns});
                    continue;
                }

                const auto& live_cols = live_it->second;

                // ADD COLUMN for each entity column missing from live table
                for (const auto& [col, type] : entity.columns)
                {
                    if (live_cols.find(col) == live_cols.end())
                        ops.push_back(add_column_op{entity.table_name, col, type});
                }

                // DROP COLUMN for each live column missing from entity. This name
                // comes from the database, not from the program.
                for (const auto& [col, type] : live_cols)
                {
                    bool found = false;
                    for (const auto& [ecol, etype] : entity.columns)
                        if (ecol == col) { found = true; break; }
                    if (!found)
                        ops.push_back(drop_column_op{entity.table_name, col});
                }

                // ALTER COLUMN TYPE for columns present in both but with different types
                for (const auto& [col, type] : entity.columns)
                {
                    auto it = live_cols.find(col);
                    if (it != live_cols.end() && ddl::upper(it->second) != ddl::upper(type))
                        ops.push_back(alter_column_type_op{entity.table_name, col, type});
                }
            }

            return ops;
        }

        // ── statements — one checked DDL statement per operation ──────────────
        // Throws migration_refused if any name or type fails its check, so a list
        // comes back whole or not at all.
        [[nodiscard]] std::vector<std::string> statements(const std::vector<ddl_op>& ops) const
        {
            const sql_dialect dialect = connector_trait<DB>::ddl_dialect();
            std::vector<std::string> out;
            out.reserve(ops.size());
            for (const auto& op : ops)
                std::visit([&](const auto& o) { out.push_back(ddl::render(o, dialect)); }, op);
            return out;
        }

        // ── generate_ddl — the statements as text, for a person to read ───────
        // Never execute this string: it holds several statements.
        [[nodiscard]] std::string generate_ddl(const std::vector<ddl_op>& ops) const
        {
            std::string sql;
            for (const auto& statement : statements(ops))
            {
                if (!sql.empty()) sql += ";\n";
                sql += statement;
            }
            return sql;
        }

        // ── run — compute diff + optionally execute DDL ────────────────────────
        // Returns 0 when the schema is in sync or was brought into sync, and 2 when
        // drift was detected on a dry run. Throws migration_refused, with nothing
        // executed, if any operation fails its checks.
        [[nodiscard]] int run(const live_schema& live,
                              std::span<const entity_meta> entities,
                              bool dry_run = false,
                              destructive_ops destructive = destructive_ops::refuse)
        {
            state_ = migration_state::Connected;
            state_ = migration_state::Diffing;
            const auto ops = diff(live, entities);

            if (ops.empty())
            {
                state_ = migration_state::Done;
                return 0;
            }

            try
            {
                state_ = migration_state::Reviewing;
                const auto checked = statements(ops);   // every check, before anything runs

                if (dry_run)
                {
                    // Print diff to stdout and return exit code 2 (drift detected)
                    std::puts(generate_ddl(ops).c_str());
                    state_ = migration_state::Done;
                    return 2;
                }

                if (destructive == destructive_ops::refuse && drops_a_column(ops))
                    throw migration_refused(
                        "orm: this migration drops a column. Pass destructive_ops::allow "
                        "if that is intended.");

                state_ = migration_state::Executing;
                execute(checked);
            }
            catch (...)
            {
                state_ = migration_state::Failed;
                throw;
            }

            state_ = migration_state::Done;
            return 0;
        }

        // ── sync — read the live schema, then run ─────────────────────────────
        [[nodiscard]] int sync(std::span<const entity_meta> entities,
                               destructive_ops destructive = destructive_ops::refuse)
        {
            return run(connector_trait<DB>::read_schema(conn_), entities, false, destructive);
        }

        [[nodiscard]] migration_state state() const noexcept { return state_; }

    private:
        [[nodiscard]] static bool drops_a_column(const std::vector<ddl_op>& ops)
        {
            return std::any_of(ops.begin(), ops.end(), [](const ddl_op& op)
            {
                return std::holds_alternative<drop_column_op>(op);
            });
        }

        // One statement per call, so a statement can never carry a second one. Where
        // the engine can roll DDL back, the whole migration is one transaction. MySQL
        // and MariaDB commit at every DDL statement, so there a failure part way leaves
        // the earlier statements applied.
        void execute(const std::vector<std::string>& checked)
        {
            constexpr bool transactional = connector_trait<DB>::ddl_dialect().transactional_ddl;
            if constexpr (transactional) connector_trait<DB>::begin(conn_);
            try
            {
                for (const auto& statement : checked)
                    connector_trait<DB>::execute_ddl(conn_, statement);
                if constexpr (transactional) connector_trait<DB>::commit(conn_);
            }
            catch (...)
            {
                if constexpr (transactional) connector_trait<DB>::rollback(conn_);
                throw;
            }
        }

        DB&              conn_;
        migration_state  state_ = migration_state::Idle;
    };

} // namespace orm
