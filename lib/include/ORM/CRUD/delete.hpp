#pragma once

#include "../details/orm_tuple.hpp"
#include "../details/member_pointer.hpp"
#include "utils.hpp"

namespace webframe::ORM::CRUD
{
	template <typename Table, typename Filters, typename Ordering, typename Limits> class delete_query : public details::delete_query_t
	{
		Filters filters;
		Limits limits;
		Ordering ordering;

		public:
		using table = Table;
		using tables =
			decltype(webframe::ORM::details::orm_tuple(typename details::unique_tables_t<decltype(std::tuple_cat(
														   typename webframe::ORM::details::get_properties<Filters>::properties(),
														   typename webframe::ORM::details::get_properties<Limits>::properties()))>::orm_tuple_type()));
		using properties = decltype(webframe::ORM::details::orm_tuple(std::tuple_cat(typename webframe::ORM::details::get_properties<Filters>::properties())));

		static constexpr size_t where_size = Filters::size;
		static constexpr size_t order_size = Ordering::size;
		static constexpr size_t limit_size = Limits::size;

		constexpr delete_query() : filters(), limits(), ordering()
		{
		}
		constexpr delete_query(Filters _filters, Ordering _ordering, Limits _limits) : filters(_filters), limits(_limits), ordering(_ordering)
		{
		}

		constexpr inline Filters wheres() const
		{
			return filters;
		}
		constexpr inline Ordering order_clauses() const
		{
			return ordering;
		}
		constexpr inline Limits limit_clauses() const
		{
			return limits;
		}

		template <typename... Ts> constexpr inline auto where(Ts... new_filters) const
		{
			using new_wheres = decltype(webframe::ORM::details::orm_tuple(std::tuple_cat(filters.to_tuple(), std::tuple<Ts...>(new_filters...))));
			return delete_query<Table, new_wheres, Ordering, Limits>(
				webframe::ORM::details::orm_tuple(std::tuple_cat(filters.to_tuple(), std::tuple<Ts...>(new_filters...))), ordering, limits);
		}

		protected:
		template <auto arg, auto...> constexpr inline auto order_by_helper1() const
		{
			using tuple_type = decltype(std::tuple_cat(
				std::declval<Ordering>().to_tuple(), std::declval<std::tuple<decltype(webframe::ORM::OrderBy<arg, webframe::ORM::modes::DEFAULT>())>>()));
			using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
			orm_tuple_type orm_new_tuple(std::tuple_cat(ordering.to_tuple(), std::make_tuple(webframe::ORM::OrderBy<arg, webframe::ORM::modes::DEFAULT>())));
			return delete_query<Table, Filters, orm_tuple_type, Limits>(filters, orm_new_tuple, limits);
		}

		template <auto arg, webframe::ORM::modes::order sort, auto...> constexpr inline auto order_by_helper2() const
		{
			using tuple_type =
				decltype(std::tuple_cat(std::declval<Ordering>().to_tuple(), std::declval<std::tuple<decltype(webframe::ORM::OrderBy<arg, sort>())>>()));
			using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
			orm_tuple_type orm_new_tuple(std::tuple_cat(ordering.to_tuple(), std::make_tuple(webframe::ORM::OrderBy<arg, sort>())));
			return delete_query<Table, Filters, orm_tuple_type, Limits>(filters, orm_new_tuple, limits);
		}

		template <auto Arg1, auto... Args, size_t... inds> constexpr inline auto order_by_impl(std::index_sequence<inds...>) const
		{
			constexpr auto args = webframe::ORM::details::orm_tuple<decltype(Args)...>(std::make_tuple(Args...));
			if constexpr (sizeof...(Args) > 0)
			{
				if constexpr (
					webframe::ORM::details::is_property<decltype(Arg1)> && std::is_same_v<decltype(args.template get<0>()), webframe::ORM::modes::order>)
				{
					return order_by_helper2<Arg1, Args...>().template order_by_impl<Args...>(std::make_index_sequence<sizeof...(Args)>{});
				}
				if constexpr (
					webframe::ORM::details::is_property<decltype(Arg1)> && !std::is_same_v<decltype(args.template get<0>()), webframe::ORM::modes::order>)
				{
					return order_by_helper1<Arg1, Args...>().template order_by_impl<Args...>(std::make_index_sequence<sizeof...(Args)>{});
				}
			}
			if constexpr (sizeof...(Args) > 0 && !webframe::ORM::details::is_property<decltype(Arg1)>)
			{
				return order_by_impl<Args...>(std::make_index_sequence<sizeof...(Args)>{});
			}
			if constexpr (sizeof...(Args) == 0)
			{
				if constexpr (webframe::ORM::details::is_property<decltype(Arg1)>)
				{
					return order_by_helper1<Arg1>();
				}
				if constexpr (!webframe::ORM::details::is_property<decltype(Arg1)>)
				{
					return *this;
				}
			}
		}

		public:
		template <auto... Args> constexpr inline auto order_by() const
		{
			return order_by_impl<Args...>(std::make_index_sequence<sizeof...(Args)>{});
		}

		template <typename... Ts> constexpr inline auto limit(Ts... new_limits) const
		{
			using new_limits_t = decltype(webframe::ORM::details::orm_tuple(std::tuple_cat(limits.to_tuple(), std::tuple<Ts...>(new_limits...))));
			return delete_query<Table, Filters, Ordering, new_limits_t>(
				filters, ordering, webframe::ORM::details::orm_tuple(std::tuple_cat(limits.to_tuple(), std::tuple<Ts...>(new_limits...))));
		}

		template <typename, typename, typename, typename> friend class delete_query;
	};
} // namespace webframe::ORM::CRUD

namespace webframe::ORM
{
	template <typename Table>
	static constexpr auto deleteq = CRUD::delete_query<Table, details::orm_tuple<>, details::orm_tuple<>, details::orm_tuple<>>(
		details::orm_tuple(), details::orm_tuple(), details::orm_tuple());
}
