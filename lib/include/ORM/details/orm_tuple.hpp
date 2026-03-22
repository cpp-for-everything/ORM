#pragma once
#include <tuple>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace orm::detail {

    template <typename... Ts>
    struct orm_tuple
    {
        static constexpr std::size_t size = sizeof...(Ts);

        template <std::size_t I>
        using orm_type = std::tuple_element_t<I, std::tuple<Ts...>>;

        std::tuple<Ts...> storage_;

        constexpr orm_tuple() = default;

        template <typename... Us>
            requires (sizeof...(Us) > 0 && sizeof...(Us) == sizeof...(Ts))
        explicit constexpr orm_tuple(Us&&... vals)
            : storage_(std::forward<Us>(vals)...) {}

        explicit constexpr orm_tuple(std::tuple<Ts...> t) : storage_(std::move(t)) {}

        template <std::size_t I>
        [[nodiscard]] constexpr auto& get() & { return std::get<I>(storage_); }

        template <std::size_t I>
        [[nodiscard]] constexpr const auto& get() const& { return std::get<I>(storage_); }

        [[nodiscard]] constexpr auto to_tuple() const& { return storage_; }
    };

    template <typename... Ts>
    orm_tuple(Ts...) -> orm_tuple<Ts...>;

    template <typename... As, typename... Bs>
    [[nodiscard]] constexpr auto tuple_cat(const orm_tuple<As...>& a, const orm_tuple<Bs...>& b)
    {
        return orm_tuple<As..., Bs...>(
            orm_tuple<As..., Bs...>(std::tuple_cat(a.storage_, b.storage_)));
    }

    template <typename Tuple, typename T>
    struct append_type;

    template <typename... Ts, typename T>
    struct append_type<orm_tuple<Ts...>, T>
    {
        using type = orm_tuple<Ts..., T>;
    };

    template <typename Tuple, typename T>
    using append_type_t = typename append_type<Tuple, T>::type;

} // namespace orm::detail
