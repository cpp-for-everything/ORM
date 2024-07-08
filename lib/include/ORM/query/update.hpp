#pragma once

#include <ORM/utils/mem_ptr_wrapper.hpp>
#include <ORM/query/utils/query.hpp>
#include <ORM/tools/statement.hpp>

namespace webframe::ORM
{
    namespace details
    {
        enum OrderEnum
        {
            ASC, DESC, Default
        };

        class IUpdate : public IQuery {};
    
        template<typename Table, typename WhereClauses, OrderEnum Order>
        class UpdateQuery
        {
            using UpdatedTable = Table;
            using WhereRules = WhereClauses;
            static constexpr OrderEnum Ordering = Order;

            typename WhereRules::tuple_equivalent rules;
        public:
            constexpr typename WhereRules::tuple_equivalent get_rules() const
            {
                return rules;
            }

            static constexpr bool allow_repeats = false;
            using parameters_type = InsertedColumns;
            using result_type = IntoTable;
        };
    }
}
