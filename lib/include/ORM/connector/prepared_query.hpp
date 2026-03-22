#pragma once
#include "ORM/connector/trait.hpp"
#include "ORM/result/result.hpp"
#include <utility>

namespace orm {

    // ── orm::prepared_query<DB, Query> ────────────────────────────────────────
    // A query IR bound to a specific db<DB> instance.
    // Created via db<DB>::prepare(query).
    //
    // Intended for use as a static-local or long-lived object:
    //
    //   static const auto q = db.prepare(
    //       orm::select(orm::field<&User::id>)
    //           .where(orm::field<&User::id> == orm::ph<int, _1>));
    //
    //   auto result = q.execute(42);    // re-uses the same IR, no reconstruction
    //
    template <typename DB, typename Query>
    class prepared_query
    {
    public:
        prepared_query(DB& conn, Query q) : conn_(conn), query_(std::move(q)) {}

        // ── execute with runtime parameters ───────────────────────────────────
        template <typename... Params>
        auto execute(Params&&... params) const
        {
            return connector_trait<DB>::execute(
                conn_, query_, std::forward<Params>(params)...);
        }

        // ── execute with no parameters ────────────────────────────────────────
        auto execute() const
        {
            return connector_trait<DB>::execute(conn_, query_);
        }

        // ── expose the stored query IR for inspection ─────────────────────────
        [[nodiscard]] const Query& query() const noexcept { return query_; }

    private:
        DB&   conn_;
        Query query_;
    };

} // namespace orm
