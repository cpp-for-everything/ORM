#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Rendering support for orm::PlaceholderList<T> (IN / NOT IN).
//
//  A list's length is known only at run time, so on an SQL backend the number of
//  parameter markers is the one part of the statement text that varies at run
//  time. render_in_list() below is the whole of that variation. It takes the
//  list's LENGTH, never its elements: no element value can reach the text,
//  because the function that writes the text is never given one. The elements
//  are bound afterwards, one marker each, by the connector's ordinary binding.
//
//  Backends with a native list type (Neo4j, MongoDB) bind the list as one value
//  and need no markers at all.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace orm::detail {

    template <typename T>
    struct is_std_vector : std::false_type
    {
    };
    template <typename T, typename A>
    struct is_std_vector<std::vector<T, A>> : std::true_type
    {
    };
    template <typename T>
    inline constexpr bool is_std_vector_v = is_std_vector<std::remove_cvref_t<T>>::value;

    // The lengths of the list arguments of one execute() call, in argument order.
    // Rendering walks the query in the same order and takes one length per list.
    struct list_cursor
    {
        std::vector<std::size_t> sizes;
        std::size_t              next = 0;

        [[nodiscard]] std::size_t take()
        {
            if (next >= sizes.size())
                throw std::logic_error("orm: a list placeholder has no matching argument");
            return sizes[next++];
        }
    };

    template <typename... Params>
    [[nodiscard]] list_cursor list_sizes(const Params&... params)
    {
        list_cursor c;
        auto one = [&c]<typename P>(const P& p)
        {
            if constexpr (is_std_vector_v<P>)
                c.sizes.push_back(p.size());
        };
        (one(params), ...);
        return c;
    }

    // Calls fn once per bindable value: once for a scalar, once per element for a
    // list. Connectors with one marker per element bind through this.
    template <typename Fn, typename P>
    void for_each_bound(Fn&& fn, P&& p)
    {
        if constexpr (is_std_vector_v<P>)
        {
            for (auto&& element : p)
                fn(element);
        }
        else
            fn(std::forward<P>(p));
    }

    // `col IN (m, m, m)`. An empty list has no valid SQL form, `IN ()` is a syntax
    // error on PostgreSQL and MySQL, so it renders as the constant it means: IN
    // over nothing is false, NOT IN over nothing is true.
    template <typename Marker>
    [[nodiscard]] std::string render_in_list(std::string_view column, bool negated,
                                             std::size_t length, Marker&& marker)
    {
        if (length == 0)
            return negated ? "1 = 1" : "1 = 0";
        std::string out(column);
        out += negated ? " NOT IN (" : " IN (";
        for (std::size_t i = 0; i < length; ++i)
        {
            if (i > 0)
                out += ", ";
            out += marker();
        }
        out += ')';
        return out;
    }

    [[nodiscard]] inline list_cursor& require_lists(list_cursor* lists)
    {
        if (lists == nullptr)
            throw std::logic_error("orm: a list placeholder was rendered without its arguments");
        return *lists;
    }

} // namespace orm::detail
