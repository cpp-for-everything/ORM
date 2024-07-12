#pragma once

#include <ORM/utils/mem_ptr_wrapper.hpp>
#include <ORM/query/utils/query.hpp>
#include <ORM/tools/statement.hpp>
#include <ORM/query/utils/placeholder_lister.hpp>

namespace webframe::ORM
{
    enum OrderEnum
    {
        ASC, DESC, Default
    };

    namespace details
    {
        template<details::is_table_pointer_to_member_variable_t mem_ptr, OrderEnum _order>
        struct OrderWrapper
        {
            using member_ptr = mem_ptr;
            static constexpr OrderEnum order = _order;
        };

        class IUpdate : public IQuery {};
    
        template<typename Table, typename WhereClauses, typename Orders>
        class UpdateQuery : public IUpdate
        {
            typename WhereClauses::tuple_equivalent rules;
            typename Orders::tuple_equivalent orders;
        public:
            using UpdatedTable = Table;
            using WhereRules = WhereClauses;
            using OrderingRules = Orders;

            constexpr UpdateQuery(typename WhereRules::tuple_equivalent _rules, typename Orders::tuple_equivalent _orders) : rules(_rules), orders(_orders) {}

            constexpr typename WhereClauses::tuple_equivalent get_rules() const
            {
                return rules;
            }
            
            constexpr typename Orders::tuple_equivalent get_orders() const
            {
                return orders;
            }

            static constexpr bool allow_repeats = false;
            using parameters_type = typename details::tuple_of_the_placeholders<typename WhereRules::tuple_equivalent>::type;
            using result_type = Table;

            template<typename... Ts>
            constexpr auto where(Ts... rules) const
            {
                return UpdateQuery<Table, details::type_constainer<Ts...>, Orders>({ rules... }, orders);
            }

            template<auto ptr, OrderEnum column_order = Default>
            constexpr auto order_by() const
            {
                static_assert(WhereClauses::size > 0, "Where clause is required for the update query in order to use the order_by functionality.");
                using NewOrders = decltype(details::concat(Orders(), OrderWrapper<details::mem_ptr_wrapper<ptr>, column_order>()));
                using NewOrders_tuple = typename NewOrders::tuple_equivalent;
                return UpdateQuery<Table, WhereClauses, NewOrders>(rules, NewOrders_tuple());
            }
        };
    }
    template<details::is_table Table>
    constexpr auto update = details::UpdateQuery<Table, details::type_constainer<>, details::type_constainer<>>({}, {});
}
