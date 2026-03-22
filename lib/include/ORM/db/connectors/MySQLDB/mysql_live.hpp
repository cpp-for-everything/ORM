#pragma once
#include "ORM/connector/capabilities.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include "ORM/query/field.hpp"
#include "ORM/query/placeholders.hpp"
#include "ORM/entity/table.hpp"
#include "ORM/details/member_pointer.hpp"

#ifdef ORM_MYSQL_LIVE_AVAILABLE
#include <mysql/mysql.h>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <format>
#include <cstdint>
#include <cstring>

namespace orm {

    // ── MySQLLiveDB ────────────────────────────────────────────────────────────
    // Live MySQL connection using libmysqlclient (C API).
    // Open via MySQLLiveDB::connect(host, port, user, password, database).
    struct MySQLLiveDB
    {
        MYSQL* conn_{nullptr};

        MySQLLiveDB() = default;
        MySQLLiveDB(const MySQLLiveDB&) = delete;
        MySQLLiveDB& operator=(const MySQLLiveDB&) = delete;
        
        MySQLLiveDB(MySQLLiveDB&& other) noexcept
            : conn_(other.conn_)
        {
            other.conn_ = nullptr;
        }
        
        MySQLLiveDB& operator=(MySQLLiveDB&& other) noexcept
        {
            if (this != &other)
            {
                if (conn_)
                    mysql_close(conn_);
                conn_ = other.conn_;
                other.conn_ = nullptr;
            }
            return *this;
        }
        
        ~MySQLLiveDB()
        {
            if (conn_)
                mysql_close(conn_);
        }

        // Connect via individual parameters
        [[nodiscard]] static MySQLLiveDB connect(
            const char* host,
            unsigned int port,
            const char* user,
            const char* password,
            const char* database)
        {
            MySQLLiveDB db;
            db.conn_ = mysql_init(nullptr);
            if (!db.conn_)
                throw std::runtime_error("MySQL init failed");
            
            if (!mysql_real_connect(db.conn_, host, user, password, database, port, nullptr, 0))
            {
                std::string err = mysql_error(db.conn_);
                mysql_close(db.conn_);
                throw std::runtime_error("MySQL connection failed: " + err);
            }
            return db;
        }

        [[nodiscard]] bool is_open() const noexcept
        {
            return conn_ && mysql_ping(conn_) == 0;
        }

        [[nodiscard]] MYSQL* native() { return conn_; }
    };

    // ── MySQL SQL rendering helpers ────────────────────────────────────────────
    namespace mysql_live_detail {

        template <typename T>
        [[nodiscard]] std::string render_operand(const T& /*v*/)
        {
            if constexpr (is_placeholder_v<T>)
                return "?";
            else if constexpr (is_field<T>)
                return std::string(T::column_name());
            else if constexpr (detail::is_raw_mem_ptr<T>)
                return std::string(detail::column_name_of<T>());
            else
                return "?";
        }

        [[nodiscard]] inline std::string render_operand(std::nullptr_t) { return "NULL"; }

        // Translate ORM operators to SQL operators
        [[nodiscard]] inline std::string translate_operator(std::string_view op)
        {
            if (op == "==") return "=";
            if (op == "!=") return "!=";
            return std::string(op);
        }

        template <typename T1, detail::string_literal Op, typename T2>
        [[nodiscard]] std::string render_rule(const Rule<T1, Op, T2>& r)
        {
            std::string sql_op = translate_operator(static_cast<std::string_view>(Op));
            
            if constexpr (is_rule<T1> && is_rule<T2>)
            {
                return render_rule(r.lhs_)
                    + " " + sql_op + " "
                    + render_rule(r.rhs_);
            }
            else
            {
                return render_operand(r.lhs_)
                    + " " + sql_op + " "
                    + render_operand(r.rhs_);
            }
        }

        template <typename Tuple>
        [[nodiscard]] std::string render_columns(const Tuple& t)
        {
            std::string out;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                std::size_t idx = 0;
                ((void)(out += (idx++ > 0 ? ", " : "")
                    + std::string(t.template get<Is>().column_name())), ...);
            }(std::make_index_sequence<Tuple::size>{});
            return out;
        }

        template <typename Wheres>
        [[nodiscard]] std::string render_wheres(const Wheres& w)
        {
            if constexpr (Wheres::size == 0)
                return {};
            else
            {
                std::string out = " WHERE ";
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t idx = 0;
                    ((void)(out += (idx++ > 0 ? " AND " : "") + render_rule(w.template get<Is>())), ...);
                }(std::make_index_sequence<Wheres::size>{});
                return out;
            }
        }

        template <typename Orders>
        [[nodiscard]] std::string render_order_by(const Orders& /*o*/)
        {
            if constexpr (Orders::size == 0)
                return {};
            else
            {
                std::string out = " ORDER BY ";
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t idx = 0;
                    ([&]()
                    {
                        using OB = typename Orders::template orm_type<Is>;
                        using Tag = mem_ptr<OB::member>;
                        constexpr std::string_view dir =
                            (OB::sort == order::direction::asc) ? "ASC" : "DESC";
                        out += (idx++ > 0 ? ", " : "")
                            + std::string(Tag::column_name()) + " " + std::string(dir);
                    }(), ...);
                }(std::make_index_sequence<Orders::size>{});
                return out;
            }
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

        [[nodiscard]] inline std::string positional_placeholders(std::size_t n)
        {
            std::string out;
            for (std::size_t i = 0; i < n; ++i)
            {
                if (i > 0) out += ", ";
                out += "?";
            }
            return out;
        }

        template <typename Stmts>
        [[nodiscard]] std::string render_set(const Stmts& s)
        {
            if constexpr (Stmts::size == 0)
                return {};
            else
            {
                std::string out;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    std::size_t idx = 0;
                    ([&]()
                    {
                        const auto& stmt = s.template get<Is>();
                        using StmtT = std::remove_cvref_t<decltype(stmt)>;
                        const std::string col = std::string(StmtT::field_tag::column_name());
                        out += (idx++ > 0 ? ", " : "") + col + " = ?";
                    }(), ...);
                }(std::make_index_sequence<Stmts::size>{});
                return out;
            }
        }

        // Escape string for MySQL
        [[nodiscard]] inline std::string escape_string(MYSQL* conn, const std::string& s)
        {
            std::vector<char> buf(s.size() * 2 + 1);
            mysql_real_escape_string(conn, buf.data(), s.c_str(), s.size());
            return std::string(buf.data());
        }

        // Build SQL with inline parameter substitution (for queries without prepared statements)
        template <typename T>
        [[nodiscard]] std::string to_sql_literal(MYSQL* conn, const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_same_v<D, std::nullptr_t>)
                return "NULL";
            else if constexpr (std::is_same_v<D, bool>)
                return v ? "1" : "0";
            else if constexpr (std::is_integral_v<D>)
                return std::to_string(v);
            else if constexpr (std::is_floating_point_v<D>)
                return std::to_string(v);
            else if constexpr (std::is_same_v<D, std::string>)
                return "'" + escape_string(conn, v) + "'";
            else if constexpr (std::is_same_v<D, std::u8string>)
            {
                std::string s(reinterpret_cast<const char*>(v.data()), v.size());
                return "'" + escape_string(conn, s) + "'";
            }
            else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>)
                return "'" + escape_string(conn, std::string(v)) + "'";
            else
                return "NULL";
        }

        // Convert MySQL field to typed column T
        template <typename T>
        [[nodiscard]] T convert_field(const char* field, unsigned long length)
        {
            if (!field)
                return T{};
            
            if constexpr (std::is_same_v<T, std::string>)
                return std::string(field, length);
            else if constexpr (std::is_same_v<T, std::u8string>)
                return std::u8string(reinterpret_cast<const char8_t*>(field), length);
            else if constexpr (std::is_same_v<T, bool>)
                return std::atoi(field) != 0;
            else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
                return static_cast<T>(std::atof(field));
            else if constexpr (std::is_integral_v<T>)
                return static_cast<T>(std::atoll(field));
            else
                return T{};
        }

        // Type mapping for MySQL prepared statements
        template <typename T>
        struct mysql_type_traits;
        
        template <> struct mysql_type_traits<char> { static constexpr enum_field_types type = MYSQL_TYPE_TINY; };
        template <> struct mysql_type_traits<signed char> { static constexpr enum_field_types type = MYSQL_TYPE_TINY; };
        template <> struct mysql_type_traits<unsigned char> { static constexpr enum_field_types type = MYSQL_TYPE_TINY; };
        template <> struct mysql_type_traits<short> { static constexpr enum_field_types type = MYSQL_TYPE_SHORT; };
        template <> struct mysql_type_traits<unsigned short> { static constexpr enum_field_types type = MYSQL_TYPE_SHORT; };
        template <> struct mysql_type_traits<int> { static constexpr enum_field_types type = MYSQL_TYPE_LONG; };
        template <> struct mysql_type_traits<unsigned int> { static constexpr enum_field_types type = MYSQL_TYPE_LONG; };
        template <> struct mysql_type_traits<long> { static constexpr enum_field_types type = MYSQL_TYPE_LONGLONG; };
        template <> struct mysql_type_traits<unsigned long> { static constexpr enum_field_types type = MYSQL_TYPE_LONGLONG; };
        template <> struct mysql_type_traits<long long> { static constexpr enum_field_types type = MYSQL_TYPE_LONGLONG; };
        template <> struct mysql_type_traits<unsigned long long> { static constexpr enum_field_types type = MYSQL_TYPE_LONGLONG; };
        template <> struct mysql_type_traits<float> { static constexpr enum_field_types type = MYSQL_TYPE_FLOAT; };
        template <> struct mysql_type_traits<double> { static constexpr enum_field_types type = MYSQL_TYPE_DOUBLE; };
        template <> struct mysql_type_traits<std::string> { static constexpr enum_field_types type = MYSQL_TYPE_STRING; };
        template <> struct mysql_type_traits<const char*> { static constexpr enum_field_types type = MYSQL_TYPE_STRING; };
        
        // Helper struct to hold parameter binding data with stable storage
        template <typename... Params>
        struct param_binder
        {
            std::tuple<Params...> param_values;
            std::vector<MYSQL_BIND> binds;
            std::vector<std::string> str_storage;
            std::vector<unsigned long> len_storage;
            
            explicit param_binder(const Params&... args) : param_values(args...)
            {
                binds.resize(sizeof...(Params));
                std::size_t idx = 0;
                std::apply([&](auto&... ps) {
                    (bind_one(binds[idx++], ps), ...);
                }, param_values);
            }
            
            template <typename T>
            void bind_one(MYSQL_BIND& bind, T& value)
            {
                using D = std::decay_t<T>;
                std::memset(&bind, 0, sizeof(MYSQL_BIND));
                bind.buffer_type = mysql_type_traits<D>::type;
                
                if constexpr (std::is_same_v<D, std::string>)
                {
                    str_storage.push_back(value);
                    len_storage.push_back(str_storage.back().size());
                    bind.buffer = const_cast<char*>(str_storage.back().data());
                    bind.buffer_length = len_storage.back();
                    bind.length = &len_storage.back();
                }
                else if constexpr (std::is_same_v<D, const char*>)
                {
                    str_storage.emplace_back(value);
                    len_storage.push_back(str_storage.back().size());
                    bind.buffer = const_cast<char*>(str_storage.back().data());
                    bind.buffer_length = len_storage.back();
                    bind.length = &len_storage.back();
                }
                else
                {
                    // For non-string types, point directly to the tuple storage
                    bind.buffer = &value;
                    bind.is_unsigned = std::is_unsigned_v<D>;
                }
            }
            
            void bind(MYSQL_STMT* stmt)
            {
                if (binds.empty()) return;
                if (mysql_stmt_bind_param(stmt, binds.data()) != 0)
                    throw std::runtime_error(std::format("MySQL bind failed: {}", mysql_stmt_error(stmt)));
            }
        };

    } // namespace mysql_live_detail

    // ── connector_trait<MySQLLiveDB> specialisation ───────────────────────────
    template <>
    struct connector_trait<MySQLLiveDB>
    {
        using supports_joins        = void;
        using supports_transactions = void;
        using supports_aggregation  = void;

        template <typename T>
        struct wire_type { using type = T; };

        struct cursor_type
        {
            [[nodiscard]] bool has_next() const noexcept { return false; }
        };

        // ── SELECT (no runtime params) ────────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto execute(
            MySQLLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            using Row = projected_type<Response>;
            using Entity = typename Response::template orm_type<0>::table_type;
            const std::string sql = std::format("SELECT {} FROM {}{}{}{}",
                mysql_live_detail::render_columns(q.selected_properties()),
                table_name<Entity>(),
                mysql_live_detail::render_wheres(q.where_clauses()),
                mysql_live_detail::render_order_by(q.order_clauses()),
                mysql_live_detail::render_limits(q.limit_clauses()));
            return exec_select<Row, Response>(db, sql);
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            MySQLLiveDB& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            using Row = projected_type<Response>;
            using Entity = typename Response::template orm_type<0>::table_type;
            
            // Build SQL with ? placeholders for prepared statement
            const std::string sql = std::format("SELECT {} FROM {}{}{}{}",
                mysql_live_detail::render_columns(q.selected_properties()),
                table_name<Entity>(),
                mysql_live_detail::render_wheres(q.where_clauses()),
                mysql_live_detail::render_order_by(q.order_clauses()),
                mysql_live_detail::render_limits(q.limit_clauses()));
            
            return exec_select_prepared<Row, Response>(db, sql, std::forward<Params>(params)...);
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(MySQLLiveDB& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;
            
            const std::string sql = std::format("INSERT INTO {} ({}) VALUES ({})",
                table_name<Entity>(),
                mysql_live_detail::render_columns(q.signature()),
                mysql_live_detail::positional_placeholders(sizeof...(Params)));
            
            return exec_prepared(db, sql, std::forward<Params>(params)...);
        }

        // ── UPDATE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Statements, typename Wheres, typename... Params>
        static auto execute(MySQLLiveDB& db, update_query<Table, Statements, Wheres> q,
                            Params&&... params)
            -> result<std::tuple<>>
        {
            const std::string sql = std::format("UPDATE {} SET {}{}",
                table_name<Table>(),
                mysql_live_detail::render_set(q.updates()),
                mysql_live_detail::render_wheres(q.wheres()));
            
            return exec_prepared(db, sql, std::forward<Params>(params)...);
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(MySQLLiveDB& db, delete_query<Table, Wheres> q,
                            Params&&... params)
            -> result<std::tuple<>>
        {
            const std::string sql = std::format("DELETE FROM {}{}",
                table_name<Table>(),
                mysql_live_detail::render_wheres(q.wheres()));
            
            return exec_prepared(db, sql, std::forward<Params>(params)...);
        }

    private:
        template <typename Row, typename Response>
        static auto exec_select(MySQLLiveDB& db, const std::string& sql)
            -> result<Row, Response>
        {
            if (mysql_query(db.native(), sql.c_str()) != 0)
                throw std::runtime_error(std::format("MySQL query failed: {}", mysql_error(db.native())));
            
            MYSQL_RES* res = mysql_store_result(db.native());
            if (!res)
                throw std::runtime_error(std::format("MySQL store_result failed: {}", mysql_error(db.native())));
            
            std::vector<Row> rows;
            MYSQL_ROW row;
            unsigned long* lengths = mysql_fetch_lengths(res);
            
            while ((row = mysql_fetch_row(res)))
            {
                lengths = mysql_fetch_lengths(res);
                rows.push_back(hydrate_row<Row>(row, lengths));
            }
            
            mysql_free_result(res);
            return result<Row, Response>{ std::move(rows) };
        }

        template <typename Row>
        static auto hydrate_row(MYSQL_ROW row, unsigned long* lengths) -> Row
        {
            return hydrate_impl<Row>(row, lengths, std::make_index_sequence<std::tuple_size_v<Row>>{});
        }

        template <typename Row, std::size_t... Is>
        static auto hydrate_impl(MYSQL_ROW row, unsigned long* lengths, std::index_sequence<Is...>) -> Row
        {
            return Row{ mysql_live_detail::convert_field<std::tuple_element_t<Is, Row>>(row[Is], lengths[Is])... };
        }

        // Execute prepared statement for SELECT queries
        template <typename Row, typename Response, typename... Params>
        static auto exec_select_prepared(MySQLLiveDB& db, const std::string& sql, Params&&... params)
            -> result<Row, Response>
        {
            MYSQL_STMT* stmt = mysql_stmt_init(db.native());
            if (!stmt)
                throw std::runtime_error("MySQL stmt_init failed");
            
            if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0)
            {
                std::string err = mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw std::runtime_error(std::format("MySQL prepare failed: {} (SQL: {})", err, sql));
            }
            
            unsigned long param_count = mysql_stmt_param_count(stmt);
            if (param_count != sizeof...(Params))
            {
                mysql_stmt_close(stmt);
                throw std::runtime_error(std::format("Parameter count mismatch: SQL has {} placeholders but {} parameters provided (SQL: {})", 
                    param_count, sizeof...(Params), sql));
            }
            
            // Bind parameters
            mysql_live_detail::param_binder binder(params...);
            binder.bind(stmt);
            
            // Execute
            if (mysql_stmt_execute(stmt) != 0)
            {
                std::string err = mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw std::runtime_error(std::format("MySQL execute failed: {}", err));
            }
            
            // Store result
            if (mysql_stmt_store_result(stmt) != 0)
            {
                std::string err = mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw std::runtime_error(std::format("MySQL store_result failed: {}", err));
            }
            
            // Get result metadata
            MYSQL_RES* meta = mysql_stmt_result_metadata(stmt);
            if (!meta)
            {
                mysql_stmt_close(stmt);
                throw std::runtime_error("MySQL result_metadata failed");
            }
            
            unsigned int num_fields = mysql_num_fields(meta);
            
            // Prepare result bindings
            std::vector<MYSQL_BIND> result_binds(num_fields);
            std::vector<std::vector<char>> buffers(num_fields);
            std::vector<unsigned long> lengths(num_fields);
            std::vector<char> is_nulls(num_fields);  // Use char instead of bool to avoid vector<bool> specialization
            
            for (unsigned int i = 0; i < num_fields; ++i)
            {
                buffers[i].resize(1024);
                std::memset(&result_binds[i], 0, sizeof(MYSQL_BIND));
                result_binds[i].buffer_type = MYSQL_TYPE_STRING;
                result_binds[i].buffer = buffers[i].data();
                result_binds[i].buffer_length = buffers[i].size();
                result_binds[i].length = &lengths[i];
                result_binds[i].is_null = reinterpret_cast<bool*>(&is_nulls[i]);
            }
            
            if (mysql_stmt_bind_result(stmt, result_binds.data()) != 0)
            {
                std::string err = mysql_stmt_error(stmt);
                mysql_free_result(meta);
                mysql_stmt_close(stmt);
                throw std::runtime_error(std::format("MySQL bind_result failed: {}", err));
            }
            
            // Fetch rows
            std::vector<Row> rows;
            while (mysql_stmt_fetch(stmt) == 0)
            {
                MYSQL_ROW row_data = new char*[num_fields];
                for (unsigned int i = 0; i < num_fields; ++i)
                    row_data[i] = is_nulls[i] ? nullptr : buffers[i].data();
                
                rows.push_back(hydrate_row<Row>(row_data, lengths.data()));
                delete[] row_data;
            }
            
            mysql_free_result(meta);
            mysql_stmt_close(stmt);
            return result<Row, Response>{ std::move(rows) };
        }

        // Execute prepared statement for non-SELECT queries
        template <typename... Params>
        static auto exec_prepared(MySQLLiveDB& db, const std::string& sql, Params&&... params)
            -> result<std::tuple<>>
        {
            MYSQL_STMT* stmt = mysql_stmt_init(db.native());
            if (!stmt)
                throw std::runtime_error("MySQL stmt_init failed");
            
            if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0)
            {
                std::string err = mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw std::runtime_error(std::format("MySQL prepare failed: {}", err));
            }
            
            // Bind parameters
            mysql_live_detail::param_binder binder(params...);
            binder.bind(stmt);
            
            // Execute
            if (mysql_stmt_execute(stmt) != 0)
            {
                std::string err = mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw std::runtime_error(std::format("MySQL execute failed: {}", err));
            }
            
            mysql_stmt_close(stmt);
            return result<std::tuple<>>{};
        }
    };

} // namespace orm

#endif // ORM_MYSQL_LIVE_AVAILABLE
