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

#ifdef ORM_MONGODB_LIVE_AVAILABLE
#include <mongoc/mongoc.h>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <stdexcept>
#include <format>
#include <cstdint>
#include <memory>
#include <cstring>

namespace orm {

    // ── MongoDBLive ────────────────────────────────────────────────────────────
    // Live MongoDB connection using libmongoc (C API).
    // Open via MongoDBLive::connect(uri_string, db_name).
    struct MongoDBLive
    {
        mongoc_client_t* client_{nullptr};
        std::string      default_db_;

        MongoDBLive() = default;
        MongoDBLive(const MongoDBLive&) = delete;
        MongoDBLive& operator=(const MongoDBLive&) = delete;
        
        MongoDBLive(MongoDBLive&& other) noexcept
            : client_(other.client_), default_db_(std::move(other.default_db_))
        {
            other.client_ = nullptr;
        }
        
        MongoDBLive& operator=(MongoDBLive&& other) noexcept
        {
            if (this != &other)
            {
                if (client_)
                    mongoc_client_destroy(client_);
                client_ = other.client_;
                default_db_ = std::move(other.default_db_);
                other.client_ = nullptr;
            }
            return *this;
        }
        
        ~MongoDBLive()
        {
            if (client_)
                mongoc_client_destroy(client_);
        }

        // uri_string: e.g. "mongodb://localhost:27017"
        // db_name: the database to use for collection lookups
        [[nodiscard]] static MongoDBLive connect(const char* uri_string, const char* db_name)
        {
            // mongoc_init must be called once per process
            static bool initialized = []() { mongoc_init(); return true; }();
            (void)initialized;
            
            MongoDBLive db;
            bson_error_t error;
            mongoc_uri_t* uri = mongoc_uri_new_with_error(uri_string, &error);
            if (!uri)
                throw std::runtime_error(std::format("MongoDB URI parse failed: {}", error.message));
            
            db.client_ = mongoc_client_new_from_uri(uri);
            mongoc_uri_destroy(uri);
            
            if (!db.client_)
                throw std::runtime_error("MongoDB client creation failed");
            
            // Test connection
            bson_t* ping = BCON_NEW("ping", BCON_INT32(1));
            bson_t reply;
            bool ok = mongoc_client_command_simple(db.client_, "admin", ping, nullptr, &reply, &error);
            bson_destroy(ping);
            bson_destroy(&reply);
            
            if (!ok)
            {
                mongoc_client_destroy(db.client_);
                throw std::runtime_error(std::format("MongoDB connection failed: {}", error.message));
            }
            
            db.default_db_ = db_name;
            return db;
        }

        [[nodiscard]] bool is_open() const noexcept { return client_ != nullptr; }

        [[nodiscard]] mongoc_client_t* native() { return client_; }
    };

    // ── BSON/Mongo rendering helpers ───────────────────────────────────────────
    namespace mongo_live_detail {

        // RAII wrapper for bson_t
        struct BsonDoc
        {
            bson_t* doc_{nullptr};
            
            BsonDoc() : doc_(bson_new()) {}
            explicit BsonDoc(bson_t* d) : doc_(d) {}
            ~BsonDoc() { if (doc_) bson_destroy(doc_); }
            
            BsonDoc(const BsonDoc&) = delete;
            BsonDoc& operator=(const BsonDoc&) = delete;
            
            BsonDoc(BsonDoc&& other) noexcept : doc_(other.doc_) { other.doc_ = nullptr; }
            BsonDoc& operator=(BsonDoc&& other) noexcept
            {
                if (this != &other)
                {
                    if (doc_) bson_destroy(doc_);
                    doc_ = other.doc_;
                    other.doc_ = nullptr;
                }
                return *this;
            }
            
            bson_t* get() { return doc_; }
            const bson_t* get() const { return doc_; }
            bson_t* release() { bson_t* tmp = doc_; doc_ = nullptr; return tmp; }
        };

        template <typename T>
        [[nodiscard]] std::string field_name_of(const T& /*v*/)
        {
            if constexpr (is_field<T>)
                return std::string(T::column_name());
            else if constexpr (detail::is_raw_mem_ptr<T>)
                return std::string(detail::column_name_of<T>());
            else
                return "field";
        }

        // Convert string value to typed column T.
        template <typename T>
        [[nodiscard]] T convert_col(const std::string& s)
        {
            if (s.empty()) return T{};
            if constexpr (std::is_same_v<T, std::string>)
                return s;
            else if constexpr (std::is_same_v<T, std::u8string>)
                return std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size());
            else if constexpr (std::is_same_v<T, bool>)
                return s == "1" || s == "true";
            else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
                return static_cast<T>(std::stod(s));
            else if constexpr (std::is_integral_v<T>)
                return static_cast<T>(std::stoll(s));
            else
                return T{};
        }

        template <typename T>
        [[nodiscard]] std::string param_to_string(const T& v)
        {
            using D = std::decay_t<T>;
            if constexpr (std::is_arithmetic_v<D>)   return std::to_string(v);
            else if constexpr (std::is_same_v<D, std::string>) return v;
            else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>)
                return std::string(v);
            else return {};
        }

        template <typename... Params>
        [[nodiscard]] std::vector<std::string> collect_params(Params&&... params)
        {
            std::vector<std::string> out;
            out.reserve(sizeof...(params));
            (out.push_back(param_to_string(std::forward<Params>(params))), ...);
            return out;
        }

        // Convert a BSON iterator value to string for hydration.
        [[nodiscard]] inline std::string iter_to_string(bson_iter_t* iter)
        {
            const bson_type_t t = bson_iter_type(iter);
            
            if (t == BSON_TYPE_UTF8)
            {
                uint32_t len;
                const char* str = bson_iter_utf8(iter, &len);
                return std::string(str, len);
            }
            if (t == BSON_TYPE_INT32)
                return std::to_string(bson_iter_int32(iter));
            if (t == BSON_TYPE_INT64)
                return std::to_string(bson_iter_int64(iter));
            if (t == BSON_TYPE_DOUBLE)
                return std::to_string(bson_iter_double(iter));
            if (t == BSON_TYPE_BOOL)
                return bson_iter_bool(iter) ? "1" : "0";
            return {};
        }

        // Build a BSON filter document from a Rule tree and positional params.
        template <typename T1, detail::string_literal Op, typename T2>
        void append_rule(bson_t* doc, const Rule<T1, Op, T2>& r,
                         const std::vector<std::string>& params, int& pi);

        template <typename T1, detail::string_literal Op, typename T2>
        void append_rule(bson_t* doc, const Rule<T1, Op, T2>& r,
                         const std::vector<std::string>& params, int& pi)
        {
            constexpr std::string_view op_sv = static_cast<std::string_view>(Op);

            if constexpr (is_rule<T1> && is_rule<T2>)
            {
                const char* logical = (op_sv == "AND") ? "$and" : "$or";
                
                bson_t child_array;
                BSON_APPEND_ARRAY_BEGIN(doc, logical, &child_array);
                
                bson_t sub0, sub1;
                bson_init(&sub0);
                bson_init(&sub1);
                
                append_rule(&sub0, r.lhs_, params, pi);
                append_rule(&sub1, r.rhs_, params, pi);
                
                bson_append_document(&child_array, "0", 1, &sub0);
                bson_append_document(&child_array, "1", 1, &sub1);
                
                bson_append_array_end(doc, &child_array);
                
                bson_destroy(&sub0);
                bson_destroy(&sub1);
            }
            else
            {
                const std::string field = field_name_of(r.lhs_);
                const char* bson_op;
                if      (op_sv == "==") bson_op = "$eq";
                else if (op_sv == ">")  bson_op = "$gt";
                else if (op_sv == "<")  bson_op = "$lt";
                else if (op_sv == ">=") bson_op = "$gte";
                else if (op_sv == "<=") bson_op = "$lte";
                else if (op_sv == "!=") bson_op = "$ne";
                else                    bson_op = "$eq";

                std::string val;
                if constexpr (is_placeholder_v<T2>)
                {
                    if (pi < static_cast<int>(params.size()))
                        val = params[pi++];
                }
                
                bson_t child;
                BSON_APPEND_DOCUMENT_BEGIN(doc, field.c_str(), &child);
                
                // Append value with correct BSON type
                if (!val.empty())
                {
                    // Try to parse as integer first
                    try {
                        std::size_t pos = 0;
                        int int_val = std::stoi(val, &pos);
                        if (pos == val.size()) {
                            // Successfully parsed entire string as int
                            BSON_APPEND_INT32(&child, bson_op, int_val);
                        } else {
                            // Partial parse, try as double
                            pos = 0;
                            double dbl_val = std::stod(val, &pos);
                            if (pos == val.size()) {
                                BSON_APPEND_DOUBLE(&child, bson_op, dbl_val);
                            } else {
                                // Not a number, use as string
                                BSON_APPEND_UTF8(&child, bson_op, val.c_str());
                            }
                        }
                    } catch (...) {
                        // Fall back to string
                        BSON_APPEND_UTF8(&child, bson_op, val.c_str());
                    }
                }
                else
                {
                    BSON_APPEND_UTF8(&child, bson_op, val.c_str());
                }
                
                bson_append_document_end(doc, &child);
            }
        }

        template <typename Wheres>
        [[nodiscard]] BsonDoc build_filter(
            const Wheres& w, const std::vector<std::string>& params)
        {
            BsonDoc doc;
            int pi = 0;
            if constexpr (Wheres::size > 0)
            {
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    (append_rule(doc.get(), w.template get<Is>(), params, pi), ...);
                }(std::make_index_sequence<Wheres::size>{});
            }
            return doc;
        }

        // Build a BSON projection document: {"col1":1,"col2":1,"_id":0}
        template <typename Tuple>
        [[nodiscard]] BsonDoc build_projection(const Tuple& t)
        {
            BsonDoc doc;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                ([&]()
                {
                    const std::string col = std::string(t.template get<Is>().column_name());
                    BSON_APPEND_INT32(doc.get(), col.c_str(), 1);
                }(), ...);
            }(std::make_index_sequence<Tuple::size>{});
            BSON_APPEND_INT32(doc.get(), "_id", 0);
            return doc;
        }

    } // namespace mongo_live_detail

    // ── connector_trait<MongoDBLive> specialisation ───────────────────────────
    template <>
    struct connector_trait<MongoDBLive>
    {
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
            MongoDBLive& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q)
            -> result<projected_type<Response>, Response>
        {
            return exec_select<projected_type<Response>, Response>(db, q, {});
        }

        // ── SELECT (with runtime params) ──────────────────────────────────────
        template <typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders,
                  typename... Params>
        static auto execute(
            MongoDBLive& db,
            select_query<Response, Joins, Wheres, Limits, Groups, Orders> q,
            Params&&... params)
            -> result<projected_type<Response>, Response>
        {
            auto p = mongo_live_detail::collect_params(std::forward<Params>(params)...);
            return exec_select<projected_type<Response>, Response>(db, q, p);
        }

        // ── INSERT (no runtime params) ────────────────────────────────────────
        template <typename Properties>
        static auto execute(MongoDBLive& db, insert_query<Properties> /*q*/)
            -> result<std::tuple<>>
        {
            return exec_insert<Properties>(db, {});
        }

        // ── INSERT (with runtime params) ──────────────────────────────────────
        template <typename Properties, typename... Params>
        static auto execute(MongoDBLive& db, insert_query<Properties> q, Params&&... params)
            -> result<std::tuple<>>
        {
            auto p = mongo_live_detail::collect_params(std::forward<Params>(params)...);
            return exec_insert<Properties>(db, p, q);
        }

        // ── DELETE (no runtime params) ────────────────────────────────────────
        template <typename Table, typename Wheres>
        static auto execute(MongoDBLive& db, delete_query<Table, Wheres> q)
            -> result<std::tuple<>>
        {
            return exec_delete<Table, Wheres>(db, q.wheres(), {});
        }

        // ── DELETE (with runtime params) ──────────────────────────────────────
        template <typename Table, typename Wheres, typename... Params>
        static auto execute(MongoDBLive& db, delete_query<Table, Wheres> q, Params&&... params)
            -> result<std::tuple<>>
        {
            auto p = mongo_live_detail::collect_params(std::forward<Params>(params)...);
            return exec_delete<Table, Wheres>(db, q.wheres(), p);
        }

    private:
        template <typename Row, typename Response, typename Joins, typename Wheres,
                  typename Limits, typename Groups, typename Orders>
        static auto exec_select(
            MongoDBLive& db,
            const select_query<Response, Joins, Wheres, Limits, Groups, Orders>& q,
            const std::vector<std::string>& params)
            -> result<Row, Response>
        {
            using Entity = typename Response::template orm_type<0>::table_type;
            const std::string coll_name = std::string(table_name<Entity>());

            auto filter = mongo_live_detail::build_filter(q.where_clauses(), params);
            auto proj   = mongo_live_detail::build_projection(q.selected_properties());

            // Build options document with projection
            bson_t opts;
            bson_init(&opts);
            BSON_APPEND_DOCUMENT(&opts, "projection", proj.get());

            mongoc_collection_t* coll = mongoc_client_get_collection(
                db.native(), db.default_db_.c_str(), coll_name.c_str());
            
            mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(
                coll, filter.get(), &opts, nullptr);
            
            bson_destroy(&opts);

            constexpr std::size_t ncols = std::tuple_size_v<Row>;
            std::vector<Row> rows;
            
            const bson_t* doc;
            while (mongoc_cursor_next(cursor, &doc))
            {
                std::array<std::string, ncols> col_vals;
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    ([&]()
                    {
                        const std::string col =
                            std::string(q.selected_properties().template get<Is>().column_name());
                        
                        bson_iter_t iter;
                        if (bson_iter_init_find(&iter, doc, col.c_str()))
                            col_vals[Is] = mongo_live_detail::iter_to_string(&iter);
                    }(), ...);
                }(std::make_index_sequence<ncols>{});
                rows.push_back(make_row<Row>(col_vals, std::make_index_sequence<ncols>{}));
            }
            
            bson_error_t error;
            if (mongoc_cursor_error(cursor, &error))
            {
                mongoc_cursor_destroy(cursor);
                mongoc_collection_destroy(coll);
                throw std::runtime_error(std::format("MongoDB find failed: {}", error.message));
            }
            
            mongoc_cursor_destroy(cursor);
            mongoc_collection_destroy(coll);
            return result<Row, Response>{ std::move(rows) };
        }

        template <typename Row, typename ColsArray, std::size_t... Is>
        static Row make_row(const ColsArray& col_vals, std::index_sequence<Is...>)
        {
            return Row{ mongo_live_detail::convert_col<std::tuple_element_t<Is, Row>>(
                col_vals[Is])... };
        }

        template <typename Properties>
        static auto exec_insert(
            MongoDBLive& db,
            const std::vector<std::string>& params,
            insert_query<Properties> q = {})
            -> result<std::tuple<>>
        {
            using Entity = typename Properties::template orm_type<0>::table_type;
            const std::string coll_name = std::string(table_name<Entity>());

            bson_t doc;
            bson_init(&doc);
            
            std::size_t pi = 0;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                ([&]()
                {
                    const std::string col =
                        std::string(q.signature().template get<Is>().column_name());
                    const std::string val = (pi < params.size()) ? params[pi++] : "";
                    
                    // Append value with correct BSON type
                    if (!val.empty())
                    {
                        try {
                            std::size_t pos = 0;
                            int int_val = std::stoi(val, &pos);
                            if (pos == val.size()) {
                                BSON_APPEND_INT32(&doc, col.c_str(), int_val);
                            } else {
                                pos = 0;
                                double dbl_val = std::stod(val, &pos);
                                if (pos == val.size()) {
                                    BSON_APPEND_DOUBLE(&doc, col.c_str(), dbl_val);
                                } else {
                                    BSON_APPEND_UTF8(&doc, col.c_str(), val.c_str());
                                }
                            }
                        } catch (...) {
                            BSON_APPEND_UTF8(&doc, col.c_str(), val.c_str());
                        }
                    }
                    else
                    {
                        BSON_APPEND_UTF8(&doc, col.c_str(), val.c_str());
                    }
                }(), ...);
            }(std::make_index_sequence<Properties::size>{});

            mongoc_collection_t* coll = mongoc_client_get_collection(
                db.native(), db.default_db_.c_str(), coll_name.c_str());
            
            bson_error_t error;
            bool ok = mongoc_collection_insert_one(coll, &doc, nullptr, nullptr, &error);
            
            bson_destroy(&doc);
            mongoc_collection_destroy(coll);
            
            if (!ok)
                throw std::runtime_error(std::format("MongoDB insert failed: {}", error.message));
            
            return result<std::tuple<>>{};
        }

        template <typename Table, typename Wheres>
        static auto exec_delete(
            MongoDBLive& db,
            const Wheres& wheres,
            const std::vector<std::string>& params)
            -> result<std::tuple<>>
        {
            const std::string coll_name = std::string(table_name<Table>());
            auto filter = mongo_live_detail::build_filter(wheres, params);

            mongoc_collection_t* coll = mongoc_client_get_collection(
                db.native(), db.default_db_.c_str(), coll_name.c_str());
            
            bson_error_t error;
            bool ok = mongoc_collection_delete_many(
                coll, filter.get(), nullptr, nullptr, &error);
            
            mongoc_collection_destroy(coll);
            
            if (!ok)
                throw std::runtime_error(std::format("MongoDB delete failed: {}", error.message));
            
            return result<std::tuple<>>{};
        }
    };

} // namespace orm

#endif // ORM_MONGODB_LIVE_AVAILABLE
