#pragma once

#include "../details/orm_tuple.hpp"
#include "../details/member_pointer.hpp"
#include "utils.hpp"

namespace webframe::ORM::CRUD
{
	template <typename Statements, typename Filters, typename Ordering, typename Limits> class update_query : public details::update_query_t
	{
		Statements statements;
		Filters filters;
		Limits limits;
		Ordering ordering;

		public:
		using tables =
			decltype(webframe::ORM::details::orm_tuple(typename details::unique_tables_t<decltype(std::tuple_cat(
														   typename webframe::ORM::details::get_properties<Statements>::properties(),
														   typename webframe::ORM::details::get_properties<Filters>::properties(),
														   typename webframe::ORM::details::get_properties<Limits>::properties()))>::orm_tuple_type()));
		using properties = decltype(webframe::ORM::details::orm_tuple(std::tuple_cat(
			typename webframe::ORM::details::get_placeholders<typename details::second_properties<Statements>::orm_tuple_type>::placeholders(),
			typename webframe::ORM::details::get_properties<Filters>::properties())));

		static constexpr size_t size = Statements::size;
		static constexpr size_t where_size = Filters::size;
		static constexpr size_t order_size = Ordering::size;
		static constexpr size_t limit_size = Limits::size;

		constexpr update_query() : statements(), filters(), limits(), ordering()
		{
		}
		constexpr update_query(Statements _statements, Filters _filters, Ordering _ordering, Limits _limits)
			: statements(_statements), filters(_filters), limits(_limits), ordering(_ordering)
		{
		}

		constexpr inline Statements updates() const
		{
			return statements;
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

		template <typename... Ts> constexpr inline auto update(Ts... new_statements) const
		{
			using new_statements_t = decltype(webframe::ORM::details::orm_tuple(std::tuple_cat(statements.to_tuple(), std::tuple<Ts...>(new_statements...))));
			return update_query<new_statements_t, Filters, Ordering, Limits>(
				webframe::ORM::details::orm_tuple(std::tuple_cat(statements.to_tuple(), std::tuple<Ts...>(new_statements...))), filters, ordering, limits);
		}

		template <typename... Ts> constexpr inline auto where(Ts... new_filters) const
		{
			using new_wheres = decltype(webframe::ORM::details::orm_tuple(std::tuple_cat(filters.to_tuple(), std::tuple<Ts...>(new_filters...))));
			return update_query<Statements, new_wheres, Ordering, Limits>(
				statements, webframe::ORM::details::orm_tuple(std::tuple_cat(filters.to_tuple(), std::tuple<Ts...>(new_filters...))), ordering, limits);
		}

		protected:
		template <auto arg, auto...> constexpr inline auto order_by_helper1() const
		{
			using tuple_type = decltype(std::tuple_cat(
				std::declval<Ordering>().to_tuple(), std::declval<std::tuple<decltype(webframe::ORM::OrderBy<arg, webframe::ORM::modes::DEFAULT>())>>()));
			using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
			orm_tuple_type orm_new_tuple(std::tuple_cat(ordering.to_tuple(), std::make_tuple(webframe::ORM::OrderBy<arg, webframe::ORM::modes::DEFAULT>())));
			return update_query<Statements, Filters, orm_tuple_type, Limits>(statements, filters, orm_new_tuple, limits);
		}

		template <auto arg, webframe::ORM::modes::order sort, auto...> constexpr inline auto order_by_helper2() const
		{
			using tuple_type =
				decltype(std::tuple_cat(std::declval<Ordering>().to_tuple(), std::declval<std::tuple<decltype(webframe::ORM::OrderBy<arg, sort>())>>()));
			using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
			orm_tuple_type orm_new_tuple(std::tuple_cat(ordering.to_tuple(), std::make_tuple(webframe::ORM::OrderBy<arg, sort>())));
			return update_query<Statements, Filters, orm_tuple_type, Limits>(statements, filters, orm_new_tuple, limits);
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
			return update_query<Statements, Filters, Ordering, new_limits_t>(
				statements, filters, ordering, webframe::ORM::details::orm_tuple(std::tuple_cat(limits.to_tuple(), std::tuple<Ts...>(new_limits...))));
		}

		template <typename, typename, typename, typename> friend class update_query;
	};
} // namespace webframe::ORM::CRUD

namespace webframe::ORM
{
	static constexpr auto update(auto... Statements)
	{
		return CRUD::update_query<details::orm_tuple<decltype(Statements)...>, details::orm_tuple<>, details::orm_tuple<>, details::orm_tuple<>>(
			details::orm_tuple<decltype(Statements)...>(std::tuple(Statements...)), details::orm_tuple(), details::orm_tuple(), details::orm_tuple());
	}
} // namespace webframe::ORM
