/**
 *  @file   select.cpp
 *  @brief  Functional-programming-like select queries implementation
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#ifndef WEBFRAME_ORM_SELECT_IMPL
#define WEBFRAME_ORM_SELECT_IMPL

#include "../../../include/ORM/CRUD/select.hpp"

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <webframe::ORM::modes::join _mode, typename Table, typename... Ts>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::join(Ts... rules) const
{
	using tuple_type =
		decltype(std::tuple_cat(std::declval<Rules>().to_tuple(), std::make_tuple((std::declval<webframe::ORM::JoinRule<_mode, Table, Ts>>())...)));
	using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
	orm_tuple_type orm_new_tuple(std::tuple_cat(joins.to_tuple(), std::make_tuple((webframe::ORM::JoinRule<_mode, Table, decltype(rules)>(rules))...)));
	return select_query<way, Result, orm_tuple_type, Filters, Limits, Grouping, Ordering>(
		properties_to_select_query, orm_new_tuple, filters, limits, groupings, ordering);
}

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <typename... Ts>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::where(Ts... rules) const
{
	using tuple_type = decltype(std::tuple_cat(std::declval<Filters>().to_tuple(), std::make_tuple((std::declval<Ts>())...)));
	using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
	orm_tuple_type orm_new_tuple(std::tuple_cat(filters.to_tuple(), std::make_tuple(rules...)));
	return select_query<way, Result, Rules, orm_tuple_type, Limits, Grouping, Ordering>(
		properties_to_select_query, joins, orm_new_tuple, limits, groupings, ordering);
}

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <typename... Ts>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::limit(Ts... rules) const
{
	using tuple_type = decltype(std::tuple_cat(std::declval<Limits>().to_tuple(), std::make_tuple((std::declval<Ts>())...)));
	using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
	orm_tuple_type orm_new_tuple(std::tuple_cat(limits.to_tuple(), std::make_tuple(rules...)));
	return select_query<way, Result, Rules, Filters, orm_tuple_type, Grouping, Ordering>(
		properties_to_select_query, joins, filters, orm_new_tuple, groupings, ordering);
}

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <auto... Ps>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::group_by(auto on) const
{
	using tuple_type = decltype(std::tuple_cat(
		std::declval<Grouping>().to_tuple(),
		std::declval<std::tuple<decltype(webframe::ORM::GroupByRule<webframe::ORM::details::orm_tuple<decltype(Ps)...>, decltype(on)>(
			webframe::ORM::details::orm_tuple(std::make_tuple(Ps...)), on))>>()));
	using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
	orm_tuple_type orm_new_tuple(std::tuple_cat(
		groupings.to_tuple(),
		std::make_tuple(webframe::ORM::GroupByRule<webframe::ORM::details::orm_tuple<decltype(Ps)...>, decltype(on)>(
			webframe::ORM::details::orm_tuple(std::make_tuple(Ps...)), on))));
	return select_query<way, Result, Rules, Filters, Limits, orm_tuple_type, Ordering>(
		properties_to_select_query, joins, filters, limits, orm_new_tuple, ordering);
}

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <auto... Ps>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::group_by() const
{
	using tuple_type = decltype(std::tuple_cat(
		std::declval<Grouping>().to_tuple(),
		std::declval<std::tuple<decltype(webframe::ORM::GroupByRule<webframe::ORM::details::orm_tuple<decltype(Ps)...>, nullptr_t>(
			webframe::ORM::details::orm_tuple(std::make_tuple(Ps...)), nullptr))>>()));
	using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
	orm_tuple_type orm_new_tuple(std::tuple_cat(
		groupings.to_tuple(),
		std::make_tuple(webframe::ORM::GroupByRule<webframe::ORM::details::orm_tuple<decltype(Ps)...>, nullptr_t>(
			webframe::ORM::details::orm_tuple(std::make_tuple(Ps...)), nullptr))));
	return select_query<way, Result, Rules, Filters, Limits, orm_tuple_type, Ordering>(
		properties_to_select_query, joins, filters, limits, orm_new_tuple, ordering);
}

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <auto... Args>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::order_by() const
{
	return order_by_impl<Args...>(std::make_index_sequence<sizeof...(Args)>{});
}

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <auto Arg1, auto... Args, size_t... inds>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::order_by_impl(
	std::index_sequence<inds...>) const
{
	constexpr auto args = webframe::ORM::details::orm_tuple<decltype(Args)...>(std::make_tuple(Args...));
	if constexpr (sizeof...(Args) > 0)
	{
		if constexpr (webframe::ORM::details::is_property<decltype(Arg1)> && std::is_same_v<decltype(args.template get<0>()), webframe::ORM::modes::order>)
		{
			return order_by_helper2<Arg1, Args...>().template order_by_impl<Args...>(std::make_index_sequence<sizeof...(Args)>{});
		}
		if constexpr (webframe::ORM::details::is_property<decltype(Arg1)> && !std::is_same_v<decltype(args.template get<0>()), webframe::ORM::modes::order>)
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

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <auto arg, auto...>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::order_by_helper1() const
{
	using tuple_type = decltype(std::tuple_cat(
		std::declval<Ordering>().to_tuple(), std::declval<std::tuple<decltype(webframe::ORM::OrderBy<arg, webframe::ORM::modes::DEFAULT>())>>()));
	using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
	orm_tuple_type orm_new_tuple(std::tuple_cat(ordering.to_tuple(), std::make_tuple(webframe::ORM::OrderBy<arg, webframe::ORM::modes::DEFAULT>())));
	return select_query<way, Result, Rules, Filters, Limits, Grouping, orm_tuple_type>(
		properties_to_select_query, joins, filters, limits, groupings, orm_new_tuple);
}

template <webframe::ORM::modes::select way, typename Result, typename Rules, typename Filters, typename Limits, typename Grouping, typename Ordering>
template <auto arg, webframe::ORM::modes::order sort, auto...>
constexpr inline auto webframe::ORM::CRUD::select_query<way, Result, Rules, Filters, Limits, Grouping, Ordering>::order_by_helper2() const
{
	using tuple_type = decltype(std::tuple_cat(std::declval<Ordering>().to_tuple(), std::declval<std::tuple<decltype(webframe::ORM::OrderBy<arg, sort>())>>()));
	using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<tuple_type>()));
	orm_tuple_type orm_new_tuple(std::tuple_cat(ordering.to_tuple(), std::make_tuple(webframe::ORM::OrderBy<arg, sort>())));
	return select_query<way, Result, Rules, Filters, Limits, Grouping, orm_tuple_type>(
		properties_to_select_query, joins, filters, limits, groupings, orm_new_tuple);
}

#endif
