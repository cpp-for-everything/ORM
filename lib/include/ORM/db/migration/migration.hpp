#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/connector/trait.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <span>
#include <algorithm>
#include <functional>
#include <cstdio>

namespace orm {

    // ── DDL operation types ───────────────────────────────────────────────────

    struct create_table_op
    {
        std::string table;
        std::vector<std::pair<std::string, std::string>> columns; // {col_name, col_type}
    };

    struct add_column_op
    {
        std::string table;
        std::string column;
        std::string type;
    };

    struct drop_column_op
    {
        std::string table;
        std::string column;
    };

    struct alter_column_type_op
    {
        std::string table;
        std::string column;
        std::string new_type;
    };

    using ddl_op = std::variant<
        create_table_op,
        add_column_op,
        drop_column_op,
        alter_column_type_op
    >;

    // ── live_schema ───────────────────────────────────────────────────────────
    // Runtime map of the live database schema: table → {column → type}.
    using live_schema = std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::string>
    >;

    // ── entity_meta ───────────────────────────────────────────────────────────
    // Runtime descriptor for a registered entity.
    struct entity_meta
    {
        std::string table_name;
        std::vector<std::pair<std::string, std::string>> columns; // {col_name, col_type}
    };

    // ── migration_state ───────────────────────────────────────────────────────
    enum class migration_state
    {
        Idle, Connected, Diffing, Reviewing, Executing, Done, Failed
    };

    // ── migrate<DB> ───────────────────────────────────────────────────────────
    // Schema migration tool. Compares registered entities against live schema
    // and generates DDL to bring the database into sync.
    template <typename DB>
    class migrate
    {
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

                // DROP COLUMN for each live column missing from entity
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
                    if (it != live_cols.end() && it->second != type)
                        ops.push_back(alter_column_type_op{entity.table_name, col, type});
                }
            }

            return ops;
        }

        // ── generate_ddl — produce DDL SQL string from ops list ────────────────
        [[nodiscard]] std::string generate_ddl(const std::vector<ddl_op>& ops) const
        {
            std::string sql;
            for (const auto& op : ops)
            {
                if (!sql.empty()) sql += ";\n";
                std::visit([&](const auto& o) { sql += connector_trait<DB>::ddl_for(o); }, op);
            }
            return sql;
        }

        // ── run — compute diff + optionally execute DDL ────────────────────────
        // Returns 0 when schema is in sync; 2 when drift detected (dry_run).
        [[nodiscard]] int run(const live_schema& live,
                              std::span<const entity_meta> entities,
                              bool dry_run = false)
        {
            state_ = migration_state::Connected;

            state_ = migration_state::Diffing;
            auto ops = diff(live, entities);

            if (ops.empty())
            {
                state_ = migration_state::Done;
                return 0;
            }

            if (dry_run)
            {
                // Print diff to stdout and return exit code 2 (drift detected)
                std::string ddl = generate_ddl(ops);
                std::puts(ddl.c_str());
                state_ = migration_state::Done;
                return 2;
            }

            state_ = migration_state::Reviewing;
            state_ = migration_state::Executing;

            std::string ddl = generate_ddl(ops);
            (void)ddl; // In production: execute each statement against the DB

            state_ = migration_state::Done;
            return 0;
        }

        [[nodiscard]] migration_state state() const noexcept { return state_; }

    private:
        DB&              conn_;
        migration_state  state_ = migration_state::Idle;
    };

} // namespace orm
