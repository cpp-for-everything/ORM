#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  DDL rendering for orm::migrate<DB>.
//
//  A query's identifiers are template arguments, fixed at compile time. A migration
//  cannot have that: part of what it handles is read back from the live database,
//  where anyone with CREATE rights can have put a column of any name. So identifiers
//  and types reach this file as run-time strings, and this file is where they stop
//  being trusted.
//
//  Identifiers are checked against an allowlist and then quoted. Types are checked
//  against a closed list. Anything else is refused, not escaped: a migration has no
//  use for a name that needs escaping, and a refusal cannot be got wrong the way an
//  escaping rule can.
// ─────────────────────────────────────────────────────────────────────────────
#include "ORM/entity/table.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/rules.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <concepts>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

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
    // Everything in here came from the database, so none of it is trusted.
    using live_schema = std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::string>
    >;

    // ── entity_meta ───────────────────────────────────────────────────────────
    // Runtime descriptor for a registered entity. Prefer entity_meta_of<>() below,
    // which fills this from the entity's declaration.
    struct entity_meta
    {
        std::string table_name;
        std::vector<std::pair<std::string, std::string>> columns; // {col_name, col_type}
    };

    // A migration that was refused before anything ran.
    class migration_refused : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // ── sql_dialect: what differs between DDL targets ─────────────────────────
    enum class alter_type_form
    {
        alter_column_type,   // ALTER COLUMN c TYPE t    (PostgreSQL)
        modify_column,       // MODIFY COLUMN c t        (MySQL, MariaDB)
        unsupported          // no such statement        (SQLite)
    };

    struct sql_dialect
    {
        char            quote;              // identifier quote character
        alter_type_form alter_type;
        bool            transactional_ddl;  // DDL can be rolled back
    };

    namespace ddl {

        inline constexpr std::size_t max_identifier_length = 63;

        // A letter or underscore, then letters, digits and underscores. ASCII only:
        // nothing here can close a quote, start a comment or end a statement.
        [[nodiscard]] constexpr bool is_safe_identifier(std::string_view s) noexcept
        {
            auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
            auto digit  = [](char c) { return c >= '0' && c <= '9'; };
            if (s.empty() || s.size() > max_identifier_length || !letter(s.front()))
                return false;
            return std::all_of(s.begin(), s.end(), [&](char c) { return letter(c) || digit(c); });
        }

        [[nodiscard]] inline std::string upper(std::string_view s)
        {
            std::string out(s);
            std::transform(out.begin(), out.end(), out.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return out;
        }

        inline constexpr std::array<std::string_view, 8> allowed_types = {
            "INTEGER", "BIGINT", "SMALLINT", "REAL", "DOUBLE PRECISION", "BOOLEAN", "TEXT", "BLOB",
        };

        // VARCHAR(n), the one parameterised type: the digits are read, not copied.
        [[nodiscard]] inline bool is_varchar(std::string_view t) noexcept
        {
            constexpr std::string_view head = "VARCHAR(";
            if (t.size() <= head.size() + 1 || t.substr(0, head.size()) != head || t.back() != ')')
                return false;
            const std::string_view n = t.substr(head.size(), t.size() - head.size() - 1);
            return n.size() <= 5 && n.front() != '0' &&
                   std::all_of(n.begin(), n.end(), [](char c) { return c >= '0' && c <= '9'; });
        }

        [[nodiscard]] inline std::string checked_type(std::string_view type)
        {
            const std::string t = upper(type);
            if (std::find(allowed_types.begin(), allowed_types.end(), t) == allowed_types.end() &&
                !is_varchar(t))
                throw migration_refused(std::format("orm: column type is not on the allowlist: {}", type));
            return t;
        }

        // Quoting does not make a name safe, the check does. Quoting is there so that
        // a safe name that happens to be a keyword, such as `order`, still works.
        [[nodiscard]] inline std::string quoted(std::string_view name, const sql_dialect& d)
        {
            if (!is_safe_identifier(name))
                throw migration_refused(std::format("orm: identifier is not on the allowlist: {}", name));
            return std::format("{0}{1}{0}", d.quote, name);
        }

        [[nodiscard]] inline std::string render(const create_table_op& op, const sql_dialect& d)
        {
            if (op.columns.empty())
                throw migration_refused("orm: a table needs at least one column");
            std::string cols;
            for (const auto& [name, type] : op.columns)
            {
                if (!cols.empty()) cols += ", ";
                cols += quoted(name, d) + " " + checked_type(type);
            }
            return std::format("CREATE TABLE {} ({})", quoted(op.table, d), cols);
        }

        [[nodiscard]] inline std::string render(const add_column_op& op, const sql_dialect& d)
        {
            return std::format("ALTER TABLE {} ADD COLUMN {} {}",
                               quoted(op.table, d), quoted(op.column, d), checked_type(op.type));
        }

        [[nodiscard]] inline std::string render(const drop_column_op& op, const sql_dialect& d)
        {
            return std::format("ALTER TABLE {} DROP COLUMN {}", quoted(op.table, d), quoted(op.column, d));
        }

        [[nodiscard]] inline std::string render(const alter_column_type_op& op, const sql_dialect& d)
        {
            const std::string table  = quoted(op.table, d);
            const std::string column = quoted(op.column, d);
            const std::string type   = checked_type(op.new_type);
            switch (d.alter_type)
            {
                case alter_type_form::alter_column_type:
                    return std::format("ALTER TABLE {} ALTER COLUMN {} TYPE {}", table, column, type);
                case alter_type_form::modify_column:
                    return std::format("ALTER TABLE {} MODIFY COLUMN {} {}", table, column, type);
                default:
                    throw migration_refused(
                        "orm: this engine cannot change a column's type in place");
            }
        }

        // ── the DDL type of a C++ type ────────────────────────────────────────
        template <typename T>
        [[nodiscard]] consteval std::string_view type_of()
        {
            if constexpr (std::same_as<T, bool>)                        return "BOOLEAN";
            else if constexpr (std::integral<T> && sizeof(T) <= 2)      return "SMALLINT";
            else if constexpr (std::integral<T> && sizeof(T) <= 4)      return "INTEGER";
            else if constexpr (std::integral<T>)                        return "BIGINT";
            else if constexpr (std::floating_point<T>)                  return "DOUBLE PRECISION";
            else if constexpr (std::same_as<T, std::string> ||
                               std::same_as<T, std::u8string>)          return "TEXT";
            else
                static_assert(sizeof(T) == 0, "orm: no DDL type is defined for this C++ type");
        }

        template <auto Field>
        [[nodiscard]] std::pair<std::string, std::string> column_of()
        {
            using Column = mem_ptr<Field>;
            using Value  = orm::detail::unwrap_property_t<typename Column::value_type>;
            return {std::string(Column::column_name()), std::string(type_of<Value>())};
        }

    } // namespace ddl

    // ── entity_meta_of<&T::a, &T::b, ...>() ───────────────────────────────────
    // The descriptor of an entity, taken from its declaration. Every name and type
    // here is fixed at compile time, so the entity side of a migration holds no
    // string that came from anywhere else.
    template <auto First, auto... Rest>
    [[nodiscard]] entity_meta entity_meta_of()
    {
        using Table = typename mem_ptr<First>::table_type;
        static_assert((std::same_as<Table, typename mem_ptr<Rest>::table_type> && ...),
                      "orm: entity_meta_of takes the columns of one entity");
        return entity_meta{std::string(table_name<Table>()),
                           {ddl::column_of<First>(), ddl::column_of<Rest>()...}};
    }

} // namespace orm
