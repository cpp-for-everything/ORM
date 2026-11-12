#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/connector/trait.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/result/joined_row.hpp"        // joined_row_for / hydrate_joined (pulls join_infer)
#include "ORM/entity/table.hpp"
#include "ORM/db/migration/ddl.hpp"
#include <sqlite3.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <functional>
#include <tuple>

namespace orm {

    // ── SQLiteDB tag + connection handle ─────────────────────────────────────
    // Owns a sqlite3* connection. Open via SQLiteDB::open("path/to/db.sqlite").
    struct SQLiteDB
    {
        sqlite3* handle{nullptr};

        SQLiteDB() = default;
        SQLiteDB(const SQLiteDB&) = delete;
        SQLiteDB& operator=(const SQLiteDB&) = delete;

        SQLiteDB(SQLiteDB&& o) noexcept : handle(o.handle) { o.handle = nullptr; }
        SQLiteDB& operator=(SQLiteDB&& o) noexcept
        {
            if (this != &o) { close(); handle = o.handle; o.handle = nullptr; }
            return *this;
        }

        ~SQLiteDB() { close(); }

        [[nodiscard]] static SQLiteDB open(const char* path)
        {
            SQLiteDB db;
            if (sqlite3_open(path, &db.handle) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
            return db;
        }

        void close() noexcept
        {
            if (handle) { sqlite3_close(handle); handle = nullptr; }
        }

        [[nodiscard]] bool is_open() const noexcept { return handle != nullptr; }
    };

    // ── SQLite wire-type mapping ───────────────────────────────────────────────
    // Maps C++ ORM types to SQLite column type affinities and bind/column calls.
    namespace sqlite_detail {

        // ── bind one C++ value to a prepared statement parameter slot ─────────
        // A failed bind (for example SQLITE_RANGE) is an error, not a silent skip.
        inline void check_bind(sqlite3_stmt* stmt, int rc)
        {
            if (rc != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(stmt)));
        }

        // Reads the error text first, because finalize can reset it, then
        // finalizes the statement and throws.
        [[noreturn]] inline void finalize_and_throw(sqlite3_stmt* stmt)
        {
            const std::string msg = sqlite3_errmsg(sqlite3_db_handle(stmt));
            sqlite3_finalize(stmt);
            throw std::runtime_error(msg);
        }

        // Runs a statement that returns no rows (INSERT, UPDATE, DELETE) and
        // requires it to finish with SQLITE_DONE.
        inline void step_to_done(sqlite3_stmt* stmt)
        {
            if (sqlite3_step(stmt) != SQLITE_DONE)
                finalize_and_throw(stmt);
            sqlite3_finalize(stmt);
        }

        inline void bind_value(sqlite3_stmt* stmt, int col, int v)
        {
            check_bind(stmt, sqlite3_bind_int(stmt, col, v));
        }

        inline void bind_value(sqlite3_stmt* stmt, int col, std::int64_t v)
        {
            check_bind(stmt, sqlite3_bind_int64(stmt, col, v));
        }

        inline void bind_value(sqlite3_stmt* stmt, int col, double v)
        {
            check_bind(stmt, sqlite3_bind_double(stmt, col, v));
        }

        inline void bind_value(sqlite3_stmt* stmt, int col, const std::string& v)
        {
            check_bind(stmt, sqlite3_bind_text(stmt, col,
                v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT));
        }

        inline void bind_value(sqlite3_stmt* stmt, int col, const std::u8string& v)
        {
            check_bind(stmt, sqlite3_bind_text(stmt, col,
                reinterpret_cast<const char*>(v.data()),
                static_cast<int>(v.size()), SQLITE_TRANSIENT));
        }

        inline void bind_value(sqlite3_stmt* stmt, int col, std::nullptr_t)
        {
            check_bind(stmt, sqlite3_bind_null(stmt, col));
        }

        // ── read one column from a result row into a C++ value ────────────────
        template <typename T>
        T read_column(sqlite3_stmt* stmt, int col);

        template <>
        inline int read_column<int>(sqlite3_stmt* stmt, int col)
        {
            return sqlite3_column_int(stmt, col);
        }

        template <>
        inline std::int64_t read_column<std::int64_t>(sqlite3_stmt* stmt, int col)
        {
            return sqlite3_column_int64(stmt, col);
        }

        template <>
        inline double read_column<double>(sqlite3_stmt* stmt, int col)
        {
            return sqlite3_column_double(stmt, col);
        }

        template <>
        inline std::string read_column<std::string>(sqlite3_stmt* stmt, int col)
        {
            const auto* txt = sqlite3_column_text(stmt, col);
            const int   len = sqlite3_column_bytes(stmt, col);
            return txt ? std::string(reinterpret_cast<const char*>(txt), len) : std::string{};
        }

        template <>
        inline std::u8string read_column<std::u8string>(sqlite3_stmt* stmt, int col)
        {
            const auto* txt = sqlite3_column_text(stmt, col);
            const int   len = sqlite3_column_bytes(stmt, col);
            return txt
                ? std::u8string(reinterpret_cast<const char8_t*>(txt), len)
                : std::u8string{};
        }

        // ── hydrate one row of a tuple from the current sqlite3 cursor row ────
        template <typename Tuple, std::size_t... Is>
        Tuple hydrate_row(sqlite3_stmt* stmt, std::index_sequence<Is...>)
        {
            return Tuple{ read_column<std::tuple_element_t<Is, Tuple>>(stmt, static_cast<int>(Is))... };
        }

        template <typename Tuple>
        Tuple hydrate_row(sqlite3_stmt* stmt)
        {
            return hydrate_row<Tuple>(stmt, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
        }

        // ── SQL builder helpers ───────────────────────────────────────────────
        // Builds a comma-separated column list from an orm_tuple<mem_ptr<>...>.
        template <typename Tuple, std::size_t... Is>
        std::string columns_from_tuple(std::index_sequence<Is...>)
        {
            std::string out;
            std::size_t idx = 0;
            using expander = int[];
            using T = Tuple;
            (void)expander{ 0, (void(
                out += (idx++ > 0 ? ", " : "")
                    + std::string(T::template orm_type<Is>::column_name())
            ), 0)... };
            return out;
        }

        template <typename Tuple>
        std::string columns_from_tuple()
        {
            return columns_from_tuple<Tuple>(std::make_index_sequence<Tuple::size>{});
        }

        // Builds "?, ?, ..." with N placeholders.
        inline std::string placeholders(std::size_t n)
        {
            std::string out;
            for (std::size_t i = 0; i < n; ++i) { if (i) out += ", "; out += "?"; }
            return out;
        }

        // Builds "col1, col2, ..." from orm_tuple<mem_ptr<>...>.
        template <typename OrmTuple, std::size_t... Is>
        std::string render_columns(std::index_sequence<Is...>)
        {
            std::string out;
            std::size_t idx = 0;
            ((void)(out += (idx++ > 0 ? ", " : "")
                + std::string(OrmTuple::template orm_type<Is>::column_name())), ...);
            return out;
        }

        template <typename OrmTuple>
        std::string render_columns()
        {
            return render_columns<OrmTuple>(std::make_index_sequence<OrmTuple::size>{});
        }

        // Extracts the entity (table) type from the first field in an orm_tuple.
        template <typename OrmTuple>
        struct entity_of
        {
            using type = typename OrmTuple::template orm_type<0>::table_type;
        };

        template <typename OrmTuple>
        using entity_of_t = typename entity_of<OrmTuple>::type;

        // Binds a pack of runtime values to a prepared statement.
        // For anonymous placeholders (sequential ?): slot 1, 2, 3...
        // For indexed placeholders (?N): pass as tuple, SQLite uses ?NNN natively
        // so we just bind arg[0]→slot1, arg[1]→slot2, … regardless.
        // SQLite's ?NNN syntax means two occurrences of ?1 both get the same value.
        template <typename... Params>
        void bind_params([[maybe_unused]] sqlite3_stmt* stmt, Params&&... params)
        {
            int slot = 1;
            auto bind_one = [&](const auto& v) { bind_value(stmt, slot++, v); };
            try
            {
                (detail::for_each_bound(bind_one, params), ...);
            }
            catch (...)
            {
                sqlite3_finalize(stmt);
                throw;
            }
        }

        // Renders WHERE clause as " WHERE col op ?, ..." from a Wheres orm_tuple.
        // Placeholder slots are emitted as "?" regardless of operand type.
        // This reuses the same render_rule logic already in MockDB but inline here
        // to avoid a dependency on the MockDB header.
        template <typename T1, detail::string_literal Op, typename T2>
        std::string render_rule_sqlite(const Rule<T1, Op, T2>& r, detail::list_cursor* lists);

        // Detects raw member pointer types (T Class::*) — what Rule stores as lhs.
        template <typename T>
        struct is_raw_mem_ptr_impl : std::false_type {};
        template <typename T, typename C>
        struct is_raw_mem_ptr_impl<T C::*> : std::true_type
        {
            using value_t = T;
        };

        template <typename T>
        concept is_raw_mem_ptr = is_raw_mem_ptr_impl<std::remove_cv_t<T>>::value;

        template <typename T>
        std::string render_operand_sqlite(const T& /*v*/)
        {
            if constexpr (is_field<T>)
                return std::string(T::column_name());
            else if constexpr (is_raw_mem_ptr<T>)
            {
                // T is e.g. orm::property<int,"id"> Item::* — value_type has column_name().
                using Prop = typename is_raw_mem_ptr_impl<std::remove_cv_t<T>>::value_t;
                return std::string(Prop::column_name());
            }
            else if constexpr (is_placeholder_v<T>)
            {
                // IndexedPlaceholder emits ?N (SQLite named-parameter syntax);
                // anonymous Placeholder emits plain ? (sequential binding).
                constexpr int idx = placeholder_index_v<T>;
                if constexpr (idx > 0)
                    return std::string("?") + std::to_string(idx);
                else
                    return "?";
            }
            else
                return "?";
        }

        inline std::string render_operand_sqlite(std::nullptr_t) { return "NULL"; }

        // Translates ORM Rule operator symbols to SQL equivalents.
        constexpr std::string_view sql_op(std::string_view op) noexcept
        {
            if (op == "==") return "=";
            if (op == "!=") return "<>";
            if (op == "&&") return "AND";
            if (op == "||") return "OR";
            return op;
        }

        template <typename T1, detail::string_literal Op, typename T2>
        std::string render_rule_sqlite(const Rule<T1, Op, T2>& r, detail::list_cursor* lists)
        {
            if constexpr (is_list_placeholder_v<T2>)
            {
                return detail::render_in_list(
                    render_operand_sqlite(r.lhs_),
                    static_cast<std::string_view>(Op) == "NOT IN",
                    detail::require_lists(lists).take(),
                    [] { return "?"; });
            }
            else if constexpr (is_rule<T1> && is_rule<T2>)
            {
                // Two statements on purpose. The operands of + are unsequenced, and
                // list lengths are consumed in order, left operand first.
                std::string lhs = detail::wrap_child<T1, Op>(render_rule_sqlite(r.lhs_, lists));
                std::string rhs = detail::wrap_child<T2, Op>(render_rule_sqlite(r.rhs_, lists));
                return lhs + " " + std::string(sql_op(static_cast<std::string_view>(Op)))
                    + " " + rhs;
            }
            else
            {
                return render_operand_sqlite(r.lhs_)
                    + " " + std::string(sql_op(static_cast<std::string_view>(Op))) + " "
                    + render_operand_sqlite(r.rhs_);
            }
        }

        template <typename Wheres>
        std::string render_wheres_sqlite(const Wheres& w, detail::list_cursor* lists = nullptr)
        {
            if constexpr (Wheres::size == 0)
            {
                return {};
            }
            else
            {
                std::string out = " WHERE ";
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t idx = 0;
                    ((void)(out += (idx++ > 0 ? " AND " : "")
                        + detail::wrap_top_clause<
                              std::remove_cvref_t<decltype(w.template get<Is>())>>(
                              render_rule_sqlite(w.template get<Is>(), lists),
                              Wheres::size > 1)), ...);
                }(std::make_index_sequence<Wheres::size>{});
                return out;
            }
        }

        // ── relationship-aware SQL rendering (connector-local) ────────────────
        // The ORM core hands us the compile-time join plan; turning it into SQL
        // text — including which dialect keyword join::mode maps to — is the
        // connector's job, so this lives here rather than in the ORM core.
        [[nodiscard]] inline std::string_view join_kind_sql(orm::join::mode m) noexcept
        {
            switch (m)
            {
                case orm::join::mode::inner: return "INNER JOIN";
                case orm::join::mode::left:  return "LEFT JOIN";
                case orm::join::mode::right: return "RIGHT JOIN";
                default:                     return "FULL JOIN";
            }
        }

        template <typename... Fields>
        [[nodiscard]] std::string qualified_columns_impl(orm::detail::orm_tuple<Fields...>*)
        {
            std::string out;
            bool first = true;
            (((out += (first ? std::string{} : std::string{", "})
                 + std::string(orm::table_name<typename Fields::table_type>()) + "."
                 + std::string(Fields::column_name())),
              first = false),
             ...);
            return out;
        }
        template <typename Response>
        [[nodiscard]] std::string render_qualified_columns()
        {
            return qualified_columns_impl(static_cast<Response*>(nullptr));
        }

        template <typename... Steps>
        [[nodiscard]] std::string inferred_joins_impl(orm::detail::tl<Steps...>*)
        {
            std::string out;
            ((out += " " + std::string(join_kind_sql(Steps::mode)) + " "
                   + std::string(orm::table_name<typename Steps::to>()) + " ON "
                   + std::string(orm::table_name<typename Steps::from>()) + "."
                   + std::string(Steps::rel::column_name()) + " = "
                   + std::string(orm::table_name<typename Steps::to>()) + "."
                   + std::string(Steps::rel::target_column())),
             ...);
            return out;
        }
        template <typename Response>
        [[nodiscard]] std::string render_inferred_joins()
        {
            return inferred_joins_impl(static_cast<orm::detail::join_plan_t<Response>*>(nullptr));
        }

        // ── explicit JOIN, GROUP BY, ORDER BY and LIMIT clauses ───────────────
        template <typename Joins, std::size_t I>
        void append_join_item(std::string& out, const Joins& j, detail::list_cursor* lists)
        {
            const auto& jr = j.template get<I>();
            using JR = std::remove_cvref_t<decltype(jr)>;
            out += " " + std::string(join_kind_sql(JR::mode)) + " "
                 + std::string(orm::table_name<typename JR::table_type>()) + " ON "
                 + render_rule_sqlite(jr.to_rule(), lists);
        }

        template <typename Joins, std::size_t... Is>
        [[nodiscard]] std::string render_joins_impl(const Joins& j, detail::list_cursor* lists,
                                                    std::index_sequence<Is...>)
        {
            std::string out;
            (append_join_item<Joins, Is>(out, j, lists), ...);
            return out;
        }

        template <typename Joins>
        [[nodiscard]] std::string render_joins(const Joins& j, detail::list_cursor* lists)
        {
            if constexpr (Joins::size == 0)
                return {};
            else
                return render_joins_impl(j, lists, std::make_index_sequence<Joins::size>{});
        }

        template <typename Groups, std::size_t I>
        void append_group_by_item(std::string& out, bool& first)
        {
            using GB = typename Groups::template orm_type<I>;
            if (!first) out += ", ";
            out += std::string(mem_ptr<GB::member>::column_name());
            first = false;
        }

        template <typename Groups, std::size_t... Is>
        [[nodiscard]] std::string render_group_by_impl(std::index_sequence<Is...>)
        {
            std::string out = " GROUP BY ";
            bool first = true;
            (append_group_by_item<Groups, Is>(out, first), ...);
            return out;
        }

        template <typename Groups>
        [[nodiscard]] std::string render_group_by(const Groups& /*g*/)
        {
            if constexpr (Groups::size == 0)
                return {};
            else
                return render_group_by_impl<Groups>(std::make_index_sequence<Groups::size>{});
        }

        template <typename Orders, std::size_t I>
        void append_order_by_item(std::string& out, bool& first)
        {
            using OB = typename Orders::template orm_type<I>;
            constexpr std::string_view dir =
                (OB::sort == order::direction::asc) ? "ASC" : "DESC";
            if (!first) out += ", ";
            out += std::string(mem_ptr<OB::member>::column_name()) + " " + std::string(dir);
            first = false;
        }

        template <typename Orders, std::size_t... Is>
        [[nodiscard]] std::string render_order_by_impl(std::index_sequence<Is...>)
        {
            std::string out = " ORDER BY ";
            bool first = true;
            (append_order_by_item<Orders, Is>(out, first), ...);
            return out;
        }

        template <typename Orders>
        [[nodiscard]] std::string render_order_by(const Orders& /*o*/)
        {
            if constexpr (Orders::size == 0)
                return {};
            else
                return render_order_by_impl<Orders>(std::make_index_sequence<Orders::size>{});
        }

        template <typename Limits>
        [[nodiscard]] std::string render_limits(const Limits& lims)
        {
            if constexpr (Limits::size == 0)
                return {};
            else
            {
                const auto& p = lims.template get<0>();
                return std::format(" LIMIT {} OFFSET {}",
                    p.get_elements_per_page(),
                    p.get_elements_per_page() * p.get_number_of_page());
            }
        }

        // ── SELECT SQL builder ────────────────────────────────────────────────
        // Inferred = multi-table query with relationships and no explicit .join():
        // emit table-qualified columns + the inferred JOIN chain. Otherwise the
        // bare single-table form.
        template <typename Response, bool Inferred, typename Query>
        [[nodiscard]] std::string build_select_sql(const Query& q,
                                                   detail::list_cursor* lists = nullptr)
        {
            if constexpr (Inferred)
            {
                using Base = orm::detail::base_table_t<Response>;
                return std::format("SELECT {} FROM {}{}{}{}{}{}",
                    render_qualified_columns<Response>(),
                    std::string(orm::table_name<Base>()),
                    render_inferred_joins<Response>(),
                    render_wheres_sqlite(q.where_clauses(), lists),
                    render_group_by(q.group_clauses()),
                    render_order_by(q.order_clauses()),
                    render_limits(q.limit_clauses()));
            }
            else
            {
                using Entity = entity_of_t<Response>;
                // Joins first: list sizes are consumed in the order the SQL text reads.
                const std::string joins = render_joins(q.join_clauses(), lists);
                const std::string wheres = render_wheres_sqlite(q.where_clauses(), lists);
                return std::format("SELECT {} FROM {}{}{}{}{}{}",
                    render_columns<Response>(),
                    std::string(orm::table_name<Entity>()),
                    joins,
                    wheres,
                    render_group_by(q.group_clauses()),
                    render_order_by(q.order_clauses()),
                    render_limits(q.limit_clauses()));
            }
        }

        // ── step the cursor and hydrate every row, then finalize ──────────────
        // Inferred → partial entities grouped into a joined_row_for<Response>.
        // Otherwise → a flat projected_type<Response> tuple (unchanged behaviour).
        template <typename Response, bool Inferred>
        [[nodiscard]] auto select_rows(sqlite3_stmt* stmt)
        {
            if constexpr (Inferred)
            {
                using Flat = projected_type<Response>;
                using Row  = joined_row_for<Response>;
                std::vector<Row> rows;
                int rc = SQLITE_OK;
                while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
                    rows.push_back(orm::hydrate_joined<Response>(hydrate_row<Flat>(stmt)));
                if (rc != SQLITE_DONE)
                    finalize_and_throw(stmt);
                sqlite3_finalize(stmt);
                return result<Row, Response>{ std::move(rows) };
            }
            else
            {
                using Row = projected_type<Response>;
                std::vector<Row> rows;
                int rc = SQLITE_OK;
                while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
                    rows.push_back(hydrate_row<Row>(stmt));
                if (rc != SQLITE_DONE)
                    finalize_and_throw(stmt);
                sqlite3_finalize(stmt);
                return result<Row, Response>{ std::move(rows) };
            }
        }

    } // namespace sqlite_detail

    // ── connector_trait<SQLiteDB> ─────────────────────────────────────────────
    template <>
    struct connector_trait<SQLiteDB>
    {
        using supports_joins        = void;
        using supports_transactions = void;
        using supports_aggregation  = void;
        using supports_list_placeholder = void;
        using supports_multi_row_insert = void;

        template <typename T>
        struct wire_type { using type = T; };

        struct cursor_type
        {
            sqlite3_stmt* stmt{nullptr};
            [[nodiscard]] bool has_next() const noexcept { return stmt != nullptr; }
        };

        // ── SELECT (no runtime params) ────────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto execute(
            SQLiteDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
        {
            constexpr bool inferred = orm::detail::is_multi_table<Response> && Joins::size == 0;
            const std::string sql = sqlite_detail::build_select_sql<Response, inferred>(q);

            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db.handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));

            return sqlite_detail::select_rows<Response, inferred>(stmt);
        }

        // ── SELECT (with runtime params bound to WHERE placeholders) ──────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            SQLiteDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
        {
            constexpr bool inferred = orm::detail::is_multi_table<Response> && Joins::size == 0;
            auto lists = detail::list_sizes(params...);
            const std::string sql = sqlite_detail::build_select_sql<Response, inferred>(q, &lists);

            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db.handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
            sqlite_detail::bind_params(stmt, std::forward<Params>(params)...);

            return sqlite_detail::select_rows<Response, inferred>(stmt);
        }

        // ── INSERT (with runtime values for each column placeholder) ──────────
        template <typename Properties, typename... Params>
        static auto execute(SQLiteDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            using Entity = sqlite_detail::entity_of_t<Properties>;
            const std::string sql = std::format("INSERT INTO {} ({}) VALUES {}",
                table_name<Entity>(),
                sqlite_detail::render_columns<Properties>(),
                detail::multi_row_placeholders(Properties::size, q.rows_));
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db.handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
            sqlite_detail::bind_params(stmt, std::forward<Params>(params)...);
            sqlite_detail::step_to_done(stmt);
            return result<std::tuple<>>{};
        }

        // ── UPDATE (with runtime values for SET + WHERE placeholders) ─────────
        // Clang 18 ICE: nested IIFE generic-lambda + pack in SET clause building.
        template <typename Statements, std::size_t I>
        static void append_update_set_item(std::string& set_clause, std::size_t& idx)
        {
            using Stmt = typename Statements::template orm_type<I>;
            set_clause += (idx++ > 0 ? ", " : "")
                + std::string(Stmt::field_tag::column_name()) + " = ?";
        }

        template <typename Statements, std::size_t... Is>
        static std::string build_update_set_clause(std::index_sequence<Is...>)
        {
            std::string set_clause;
            std::size_t idx = 0;
            (append_update_set_item<Statements, Is>(set_clause, idx), ...);
            return set_clause;
        }

        template <typename Table, typename Statements, typename Wheres, typename... Params>
        static auto execute(SQLiteDB& db, update_query<Table, Statements, Wheres> q,
                            Params&&... params)
            -> result<std::tuple<>>
        {
            const std::string set_clause =
                build_update_set_clause<Statements>(std::make_index_sequence<Statements::size>{});

            auto lists = detail::list_sizes(params...);
            const std::string sql = std::format("UPDATE {} SET {}{}",
                table_name<Table>(),
                set_clause,
                sqlite_detail::render_wheres_sqlite(q.wheres(), &lists));
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db.handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
            sqlite_detail::bind_params(stmt, std::forward<Params>(params)...);
            sqlite_detail::step_to_done(stmt);
            return result<std::tuple<>>{};
        }

        // ── Transaction control ───────────────────────────────────────────────
        static void begin(SQLiteDB& db)    { run_control(db, "BEGIN"); }
        static void commit(SQLiteDB& db)   { run_control(db, "COMMIT"); }
        static void rollback(SQLiteDB& db) { run_control(db, "ROLLBACK"); }

        // ── DDL, for orm::migrate<SQLiteDB> ───────────────────────────────────
        [[nodiscard]] static constexpr sql_dialect ddl_dialect() noexcept
        {
            return {'"', alter_type_form::unsupported, true};
        }

        // Exactly one statement. sqlite3_prepare_v2 stops at the first statement and
        // reports where it stopped, so anything after it is refused, not ignored.
        // sqlite3_exec would have run it.
        static void execute_ddl(SQLiteDB& db, const std::string& sql)
        {
            sqlite3_stmt* stmt = nullptr;
            const char*   tail = nullptr;
            if (sqlite3_prepare_v2(db.handle, sql.c_str(), -1, &stmt, &tail) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
            if (tail != nullptr && *tail != '\0')
            {
                sqlite3_finalize(stmt);
                throw std::runtime_error("orm: more than one statement in a DDL call");
            }
            const int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            if (rc != SQLITE_DONE)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
        }

        [[nodiscard]] static live_schema read_schema(SQLiteDB& db)
        {
            constexpr const char* sql =
                "SELECT m.name, p.name, p.type FROM sqlite_master m "
                "JOIN pragma_table_info(m.name) p "
                "WHERE m.type = 'table' AND m.name NOT LIKE 'sqlite_%'";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db.handle, sql, -1, &stmt, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
            live_schema out;
            auto text = [&](int col) -> std::string
            {
                const auto* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
                return v ? v : "";
            };
            while (sqlite3_step(stmt) == SQLITE_ROW)
                out[text(0)][text(1)] = ddl::upper(text(2));
            sqlite3_finalize(stmt);
            return out;
        }

        // ── DELETE (with optional runtime WHERE params) ───────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(SQLiteDB& db, delete_query<Table, Wheres> q,
                            Params&&... params)
            -> result<std::tuple<>>
        {
            auto lists = detail::list_sizes(params...);
            const std::string sql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                sqlite_detail::render_wheres_sqlite(q.wheres(), &lists));
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db.handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
            sqlite_detail::bind_params(stmt, std::forward<Params>(params)...);
            sqlite_detail::step_to_done(stmt);
            return result<std::tuple<>>{};
        }

    private:
        static void run_control(SQLiteDB& db, const char* sql)
        {
            if (sqlite3_exec(db.handle, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
                throw std::runtime_error(sqlite3_errmsg(db.handle));
        }
    };

} // namespace orm
