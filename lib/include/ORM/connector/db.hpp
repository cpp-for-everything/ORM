#pragma once
#include "ORM/connector/trait.hpp"
#include "ORM/connector/capabilities.hpp"
#include "ORM/connector/prepared_query.hpp"
#include "ORM/connector/param_check.hpp"
#include "ORM/result/result.hpp"
#include "ORM/query/select.hpp"
#include "ORM/query/insert.hpp"
#include "ORM/query/update.hpp"
#include "ORM/query/delete.hpp"
#include <type_traits>
#include <utility>

namespace orm {

    // ── orm::db<DB> ───────────────────────────────────────────────────────────
    // User-facing database handle. Owns a reference to the connection.
    // operator<< dispatches to connector_trait<DB>::execute with compile-time
    // capability gating via static_assert.
    template <typename DB>
        requires is_connector<DB>
    class db
    {
    public:
        explicit db(DB& connection) : conn_(&connection) {}

        // ── operator<< — primary query dispatch ───────────────────────────────
        template <typename Query>
        auto operator<<(Query q)
        {
            if constexpr (is_select_query<Query>)
            {
                if constexpr (decltype(q.join_clauses())::size > 0)
                {
                    static_assert(has_capability<DB, cap::supports_joins>,
                        "orm::db: this connector does not support JOIN operations. "
                        "Remove .join() clauses, use store_as::reference relationships, "
                        "or switch to a connector that declares `using supports_joins = void;`.");
                }
            }
            return connector_trait<DB>::execute(*conn_, std::move(q));
        }

        // ── execute with explicit runtime parameters ──────────────────────────
        template <typename Query, typename... Params>
        auto execute(Query q, Params&&... params)
        {
            detail::check_query_params<Query, Params...>();
            return connector_trait<DB>::execute(
                *conn_, std::move(q), std::forward<Params>(params)...);
        }

        // ── find_one — executes query, returns first row wrapped in optional ─────
        template <typename Query>
            requires is_select_query<Query>
        auto find_one(Query q)
        {
            auto res = connector_trait<DB>::execute(*conn_, std::move(q));
            using Row = typename decltype(res)::value_type;
            if (res.empty())
                return optional_result<Row>{};
            return optional_result<Row>(*res.begin());
        }

        // ── prepare — bind query IR to this db instance ──────────────────────
        // Returns a prepared_query<DB, Query> that can be executed repeatedly
        // without reconstructing the query IR. Ideal for static-local caching:
        //
        //   static const auto pq = db.prepare(
        //       orm::select(orm::field<&User::id>)
        //           .where(orm::field<&User::id> == orm::ph<int, _1>));
        //   auto res = pq.execute(42);
        //
        template <typename Query>
        [[nodiscard]] auto prepare(Query q) -> prepared_query<DB, Query>
        {
            return prepared_query<DB, Query>(*conn_, std::move(q));
        }

        [[nodiscard]] DB& connection() noexcept { return *conn_; }

        // Rebind to a different connection object.
        // Necessary for test fixtures that reassign the connection in SetUp().
        void rebind(DB& new_conn) noexcept { conn_ = &new_conn; }

    private:
        DB* conn_;
    };

} // namespace orm
