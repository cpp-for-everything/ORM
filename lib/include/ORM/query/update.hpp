#pragma once

#include <ORM/utils/mem_ptr_wrapper.hpp>
#include <ORM/query/utils/query.hpp>
#include <ORM/tools/statement.hpp>
#include <ORM/query/utils/placeholder_lister.hpp>
#include <ORM/query/utils/order.hpp>

namespace webframe::ORM
{
	namespace details
	{
		class IUpdate;

		template <typename Table, typename WhereClauses, typename Orders, typename Limit> class GenericUpdateQuery;
		template <typename Table, typename WhereClauses, typename Orders, typename Limit> class UpdateQuery;

		class IUpdate : public IQuery
		{
		};

		template <typename Table, typename WhereClauses, typename Orders, typename Limit> class GenericUpdateQuery : public IUpdate
		{
			typename WhereClauses::tuple_equivalent rules;
			typename Orders::tuple_equivalent orders;
			typename Limit::tuple_equivalent limits;

			public:
			using UpdatedTable = Table;
			using WhereRules = WhereClauses;
			using OrderingRules = Orders;
			using LimitRule = Limit;

			using parameters_type = typename details::tuple_of_the_placeholders<typename WhereRules::tuple_equivalent>::type;
			using result_type = Table;

			protected:
			constexpr GenericUpdateQuery(
				typename WhereRules::tuple_equivalent _rules, typename Orders::tuple_equivalent _orders, typename Limit::tuple_equivalent _limits)
				: rules(_rules), orders(_orders), limits(_limits)
			{
			}

			public:
			constexpr typename WhereClauses::tuple_equivalent get_rules() const
			{
				return rules;
			}

			constexpr typename Orders::tuple_equivalent get_orders() const
			{
				return orders;
			}

			constexpr typename Limit::tuple_equivalent get_limits() const
			{
				return limits;
			}

			template <template <typename, typename, typename, typename> typename ResType, typename... Ts> constexpr auto where(Ts... rules) const
			{
				return ResType<Table, details::type_constainer<Ts...>, Orders, Limit>({rules...}, orders, limits);
			}

			template <template <typename, typename, typename, typename> typename ResType, auto ptr, OrderEnum column_order = Default>
			constexpr auto order_by() const
			{
				static_assert(WhereClauses::size > 0, "Where clause is required for the update query in order to use the order_by functionality.");
				using NewOrders = decltype(details::concat(Orders(), OrderWrapper<details::mem_ptr_wrapper<ptr>, column_order>{}));
				return ResType<Table, WhereClauses, NewOrders, Limit>(
					rules, std::tuple_cat(orders, std::make_tuple(OrderWrapper<details::mem_ptr_wrapper<ptr>, column_order>{})), limits);
			}

			template <template <typename, typename, typename, typename> typename ResType, auto rows, auto offset = 0> constexpr auto limit() const
			{
				static_assert(Orders::size > 0, "Order_by clause is required for the update query in order to use the order_by functionality.");
				return ResType<Table, WhereClauses, Orders, details::type_constainer<LimitWrapper<rows, offset>>>(
					rules, orders, std::make_tuple(LimitWrapper<rows, offset>{}));
			}
		};

		template <typename Table, typename WhereClauses, typename Orders, typename Limit>
			requires(WhereClauses::size == 0 && Orders::size == 0 && Limit::size == 0)
		class UpdateQuery<Table, WhereClauses, Orders, Limit> : public GenericUpdateQuery<Table, WhereClauses, Orders, Limit>
		{
			public:
			static constexpr bool allow_repeats = false;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::UpdatedTable;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::WhereRules;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::OrderingRules;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::LimitRule;

			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::parameters_type;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::result_type;

			constexpr UpdateQuery(
				typename WhereRules::tuple_equivalent _rules, typename Orders::tuple_equivalent _orders, typename Limit::tuple_equivalent _limit)
				: GenericUpdateQuery<Table, WhereClauses, Orders, Limit>(_rules, _orders, _limit)
			{
			}

			template <typename... Ts> constexpr auto where(Ts... rules) const
			{
				return GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::template where<UpdateQuery>(rules...);
			}
		};

		template <typename Table, typename WhereClauses, typename Orders, typename Limit>
			requires(WhereClauses::size > 0 && Orders::size == 0 && Limit::size == 0)
		class UpdateQuery<Table, WhereClauses, Orders, Limit> : public GenericUpdateQuery<Table, WhereClauses, Orders, Limit>
		{
			public:
			static constexpr bool allow_repeats = false;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::UpdatedTable;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::WhereRules;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::OrderingRules;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::LimitRule;

			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::parameters_type;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::result_type;

			constexpr UpdateQuery(
				typename WhereRules::tuple_equivalent _rules, typename Orders::tuple_equivalent _orders, typename Limit::tuple_equivalent _limit)
				: GenericUpdateQuery<Table, WhereClauses, Orders, Limit>(_rules, _orders, _limit)
			{
			}

			template <typename... Ts> constexpr auto where(Ts... rules) const
			{
				return GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::template where<UpdateQuery>(rules...);
			}

			template <auto ptr, OrderEnum column_order = Default> constexpr auto order_by() const
			{
				return GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::template order_by<UpdateQuery, ptr, column_order>();
			}
		};

		template <typename Table, typename WhereClauses, typename Orders, typename Limit>
			requires(WhereClauses::size > 0 && Orders::size > 0 && Limit::size == 0)
		class UpdateQuery<Table, WhereClauses, Orders, Limit> : public GenericUpdateQuery<Table, WhereClauses, Orders, Limit>
		{
			public:
			static constexpr bool allow_repeats = false;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::UpdatedTable;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::WhereRules;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::OrderingRules;

			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::parameters_type;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::result_type;

			constexpr UpdateQuery(
				typename WhereRules::tuple_equivalent _rules, typename Orders::tuple_equivalent _orders, typename Limit::tuple_equivalent _limit)
				: GenericUpdateQuery<Table, WhereClauses, Orders, Limit>(_rules, _orders, _limit)
			{
			}

			template <auto ptr, OrderEnum column_order = Default> constexpr auto order_by() const
			{
				return GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::template order_by<UpdateQuery, ptr, column_order>();
			}

			template <auto rows, auto offset = 0> constexpr auto limit() const
			{
				return GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::template limit<UpdateQuery, rows, offset>();
			}
		};

		template <typename Table, typename WhereClauses, typename Orders, typename Limit>
			requires(WhereClauses::size > 0 && Orders::size > 0 && Limit::size == 1)
		class UpdateQuery<Table, WhereClauses, Orders, Limit> : public GenericUpdateQuery<Table, WhereClauses, Orders, Limit>
		{
			public:
			static constexpr bool allow_repeats = false;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::UpdatedTable;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::WhereRules;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::OrderingRules;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::LimitRule;

			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::parameters_type;
			using typename GenericUpdateQuery<Table, WhereClauses, Orders, Limit>::result_type;

			constexpr UpdateQuery(
				typename WhereRules::tuple_equivalent _rules, typename Orders::tuple_equivalent _orders, typename Limit::tuple_equivalent _limit)
				: GenericUpdateQuery<Table, WhereClauses, Orders, Limit>(_rules, _orders, _limit)
			{
			}
		};
	} // namespace details
	template <details::is_table Table>
	constexpr auto update = details::UpdateQuery<Table, details::type_constainer<>, details::type_constainer<>, details::type_constainer<>>({}, {}, {});
} // namespace webframe::ORM
