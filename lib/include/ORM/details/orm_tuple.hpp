#pragma once

#include <tuple>
#include <concepts>
#include "member_pointer.hpp"
#include "../rules.hpp"
#include "../fields.hpp"

namespace webframe::ORM::details {

// Extended std::tuple
    template<typename... Ts>
    class orm_tuple : public std::tuple<Ts...> {
    public:
        static constexpr size_t size = sizeof...(Ts);

        template<size_t ind>
        using orm_type = typename std::tuple_element<ind, std::tuple<Ts...>>::type;
        
        //explicit constexpr orm_tuple(const std::tuple<Ts...>&& t) : std::tuple<Ts...>(t) {}
        //explicit constexpr orm_tuple(const std::tuple<Ts...>& t) : std::tuple<Ts...>(t) {}
        constexpr orm_tuple(const std::tuple<Ts...> t) : std::tuple<Ts...>(t) {}
        constexpr orm_tuple() : std::tuple<Ts...>() {}
        
        constexpr std::tuple<Ts...> to_tuple() const {
            return *this;
        }

        template<typename T>
        constexpr auto get() const {
            return std::get<T>(*this);
        }
        
        template<auto ind_or_pointer>
        constexpr auto get() const {
            if constexpr (std::is_integral_v<decltype(ind_or_pointer)>)
                return std::get<ind_or_pointer>(*this);
            if constexpr (!std::is_integral_v<decltype(ind_or_pointer)>)
                return std::get<decltype(ind_or_pointer)>(*this);
        }
    };

    template<>
    class orm_tuple<> : public std::tuple<> {
    public:
        //explicit constexpr orm_tuple(const std::tuple<>&& t) : std::tuple<>(t) {}
        //explicit constexpr orm_tuple(const std::tuple<>& t) : std::tuple<>(t) {}
        constexpr orm_tuple(const std::tuple<> t) : std::tuple<>(t) {}
        constexpr orm_tuple() : std::tuple<>() {}
        static constexpr size_t size = 0;
        
        constexpr std::tuple<> to_tuple() const {
            return *this;
        }
    };

// Decomposite tuples of params
    template<typename value_or_property>
    class get_type;

    template<typename property> requires (std::is_member_pointer_v<property>)
    class get_type<property> {
    public:
        using type = typename webframe::ORM::details::i_mem_ptr<property>::type::var_t;
        using actual_type = typename webframe::ORM::details::i_mem_ptr<property>::type;
        using table = typename webframe::ORM::details::i_mem_ptr<property>::Table;
    };

    template<typename value> requires (!std::is_member_pointer_v<value>)
    class get_type<value> {
    public:
        using type = value;
        using actual_type = value;
        using table = decltype(nullptr);
    };

    template<typename Tuple>
    class decomposite_properties;

    template<typename... Ts>
    class decomposite_properties<orm_tuple<Ts...>> {
    public:
        using data_types  = orm_tuple<typename get_type<Ts>::type ...>;
        using data_tables = orm_tuple<typename get_type<Ts>::table ...>;
    };

// Convertible tuples utils
    template<typename T>
    class normalize_type;
    
    template<is_not_property T>
    class normalize_type<T> {
    public:
        using type = T;
    };

    template<is_property T>
    class normalize_type<T>{
    public:
        using type = typename i_mem_ptr<T>::type::var_t;
    };

    template<typename T, typename U>
    class convertible_tuples;

    template<typename... Ts, typename... Us>
    class convertible_tuples<webframe::ORM::details::orm_tuple<Ts...>, webframe::ORM::details::orm_tuple<Us...>> {
    public:
        using T = webframe::ORM::details::orm_tuple<Ts...>;
        using U = webframe::ORM::details::orm_tuple<Us...>;
        template<size_t... ind>
        static constexpr bool eval(std::index_sequence<ind...>) {
            if constexpr (T::size == 0) return U::size == 0;
            return (requires(typename normalize_type<typename T::template orm_type<ind % T::size>>::type var) {
                { var = std::declval<typename U::template orm_type<ind>>() };
            } && ...);
        }
        static constexpr bool value = eval(std::index_sequence_for<Us...>{});
    };

    template<typename T>
    concept pseudo_concept = T::value;

// Placeholders
    class IPlaceholder {};

    template<typename T>
    class placeholder : public IPlaceholder {
    public:
        webframe::ORM::property<T, "_"> _;
    };
    
    template<typename T>
    concept is_orm_placeholder = std::derived_from<typename webframe::ORM::details::i_mem_ptr<T>::Table, IPlaceholder>;

    template<typename T>
    concept is_not_orm_placeholder = !is_orm_placeholder<T>;

// Get properties
    template<typename Tuple_or_Property_or_Rule>
    class get_properties;

    // Tuple
    template<typename... Ts>
    class get_properties<std::tuple<Ts...>> {
    public:
        using properties = decltype(
            std::tuple_cat(
                typename get_properties<Ts>::properties()...
            )
        );
        using orm_properties = decltype(orm_tuple(properties()));
    };

    template<typename... Ts>
    class get_properties<orm_tuple<Ts...>> {
    public:
        using properties = typename get_properties<std::tuple<Ts...>>::properties;
        using orm_properties = decltype(orm_tuple(properties()));
    };

    // Property
    template<is_property T>
    class get_properties<T> {
    public:
        using properties = std::tuple<T>;
        using orm_properties = decltype(orm_tuple(properties()));
    };

    // Rule
    template<is_rule T>
    class get_properties<T> {
    public:
        using properties = decltype(
            std::tuple_cat(
                typename get_properties<decltype(T()._1)>::properties(),
                typename get_properties<decltype(T()._2)>::properties()
            )
        );
        using orm_properties = decltype(orm_tuple(properties()));
    };

    // Random value
    template <typename> struct is_tuple: std::false_type {};
    template <typename ...T> struct is_tuple<std::tuple<T...>>: std::true_type {};

    template<typename T>
    concept is_property_or_rule_or_tuple = is_property<T> || is_rule<T> || is_tuple<T>::value;
    template<typename T>
    concept is_not_property_not_rule_not_tuple = !is_property_or_rule_or_tuple<T>;
    template<is_not_property_not_rule_not_tuple T>
    class get_properties<T> {
    public:
        using properties = std::tuple<>;
        using orm_properties = decltype(orm_tuple(properties()));
    };

// Filter the placeholders from a tuple
    template<typename Tuple>
    class get_placeholders;

    template<typename... Ts>
    class get_placeholders<orm_tuple<Ts...>> {
    public:
        using placeholders = typename get_placeholders<std::tuple<Ts...>>::placeholders;
        using orm_placeholders = typename get_placeholders<std::tuple<Ts...>>::orm_placeholders;
    };

    template<>
    class get_placeholders<std::tuple<>> {
    public:
        using placeholders = std::tuple<>;
        using orm_placeholders = decltype(orm_tuple(placeholders()));
    };

    template<is_not_orm_placeholder T, typename... Ts>
    class get_placeholders<std::tuple<T, Ts...>> {
    public:
        using placeholders = typename get_placeholders<std::tuple<Ts...>>::placeholders;
        using orm_placeholders = typename get_placeholders<std::tuple<Ts...>>::orm_placeholders;
    };

    template<is_orm_placeholder T, typename... Ts>
    class get_placeholders<std::tuple<T, Ts...>> {
    public:
        using placeholders = decltype(
            std::tuple_cat(
                std::tuple<T>(),
                std::declval<typename get_placeholders<std::tuple<Ts...>>::placeholders>()
            )
        );
        using orm_placeholders = decltype(orm_tuple(std::declval<placeholders>()));
    };
    
    // filter the placeholders and the properties from a tuple
    template<typename Tuple>
    class get_placeholders_and_properties;

    template<>
    class get_placeholders_and_properties<std::tuple<>> {
    public:
        using placeholders = std::tuple<>;
        using orm_placeholders = decltype(orm_tuple(placeholders()));
    };

    template<is_orm_placeholder T, typename... Ts>
    class get_placeholders_and_properties<std::tuple<T, Ts...>> {
    public:
        using placeholders = decltype(
            std::tuple_cat(
                std::tuple<T>(),
                std::declval<typename get_placeholders_and_properties<std::tuple<Ts...>>::placeholders>()
            )
        );
        using orm_placeholders = decltype(orm_tuple(placeholders()));
    };

    template<typename T>
    concept is_property_but_not_placeholder = is_property<T> && !is_orm_placeholder<T>;
    template<is_property_but_not_placeholder T, typename... Ts>
    class get_placeholders_and_properties<std::tuple<T, Ts...>> {
    public:
        using placeholders = decltype(
            std::tuple_cat(
                std::tuple<T>(),
                std::declval<typename get_placeholders_and_properties<std::tuple<Ts...>>::placeholders>()
            )
        );
        using orm_placeholders = decltype(orm_tuple(placeholders()));
    };

    template<typename T>
    concept is_not_property_or_placeholder = !(is_property_but_not_placeholder<T> || is_orm_placeholder<T>);

    template<is_not_property_or_placeholder T, typename... Ts>
    class get_placeholders_and_properties<std::tuple<T, Ts...>> {
    public:
        using placeholders = typename get_placeholders<std::tuple<Ts...>>::placeholders;
        using orm_placeholders = typename get_placeholders<std::tuple<Ts...>>::orm_placeholders;
    };

    template<typename... Ts>
    class get_placeholders_and_properties<orm_tuple<Ts...>> {
    public:
        using placeholders = typename get_placeholders_and_properties<std::tuple<Ts...>>::placeholders;
        using orm_placeholders = typename get_placeholders_and_properties<std::tuple<Ts...>>::orm_placeholders;
    };
}

namespace webframe::ORM {
    template<typename T>
    static constexpr auto Placeholder = &details::placeholder<T>::_;
}