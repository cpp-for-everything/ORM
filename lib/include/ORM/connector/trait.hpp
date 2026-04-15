#pragma once
#include "ORM/connector/capabilities.hpp"

namespace orm {

    // ── is_connector<DB> concept ──────────────────────────────────────────────
    // A valid connector_trait<DB> specialisation must provide:
    //   - wire_type<CppType>::type   (C++ → wire type mapping)
    //   - cursor_type                (result iteration cursor)
    //   - execute(DB&, QueryIR, Params...) → orm::result<...>
    template <typename DB>
    concept is_connector = requires {
        typename connector_trait<DB>::template wire_type<int>::type;
        typename connector_trait<DB>::cursor_type;
    };

    // ── is_async_connector<DB> concept ─────────────────────────────────────────
    // An async connector must be a valid connector, declare the supports_async
    // capability, and provide async_execute().
    template <typename DB>
    concept is_async_connector =
        is_connector<DB> &&
        has_capability<DB, cap::supports_async>;

} // namespace orm
