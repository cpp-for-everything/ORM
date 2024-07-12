#pragma once

#include <ORM/utils/mem_ptr_wrapper.hpp>
#include <ORM/query/utils/query.hpp>
#include <ORM/tools/statement.hpp>
#include <ORM/query/utils/placeholder_lister.hpp>

namespace webframe::ORM
{
    namespace details
    {
        class IInsert : public IQuery {};
        class IInsertSelect : public IQuery {};

        /**
         * @brief Insert query class
         * 
         * @tparam IntoTable The class representing the table or collection where the data is going to get inserted
         * @tparam InsertedColumns Set of mem_ptr_wrappers containing the columns which would be given values. All columns should be from the same class.
         * @tparam InsertedValues Set of set values and/or placeholders which would be replaced by arguments when the query gets executed
         * @tparam OnDuplicateKeyUpdateAssignments Set of assigments which would be executed on duplicated data being inserted
         */
        template<typename IntoTable, is_type_container InsertedColumns, is_type_container InsertedValues, is_type_container OnDuplicateKeyUpdateAssignments>
        class InsertQuery : public IInsert
        {
            using into_table = IntoTable;
            using columns = InsertedColumns;
            using inserted_values = InsertedValues;
            using on_duplicate_key_update_assignments = OnDuplicateKeyUpdateAssignments;

            typename InsertedColumns::tuple_equivalent cols;
            typename InsertedValues::tuple_equivalent vals;
            typename OnDuplicateKeyUpdateAssignments::tuple_equivalent update_statements;
        public:
            constexpr auto get_columns() const { return cols; }
            constexpr auto get_update_statements() const { return update_statements; }
            constexpr auto get_values() const { return vals; }
        public:
            static constexpr bool allow_repeats = true;
            using parameters_type = typename details::tuple_of_the_placeholders<typename InsertedValues::tuple_equivalent>::type;
            using result_type = IntoTable;

            constexpr InsertQuery(typename InsertedColumns::tuple_equivalent _cols, 
                                    typename InsertedValues::tuple_equivalent _vals,
                                    typename OnDuplicateKeyUpdateAssignments::tuple_equivalent stmts) : cols(_cols), vals(_vals), update_statements(stmts) { }

            template<typename... Statements>
            constexpr auto on_duplicate_key_update(Statements... stmts) const
            {
                return InsertQuery<IntoTable, InsertedColumns, InsertedValues, type_constainer<Statements...>>(get_columns(), get_values(), { stmts... });
            }
        };

        /**
         * @brief Insert into select query class
         * 
         * @tparam IntoTable The class representing the table or collection where the data is going to get inserted
         * @tparam InsertedColumns Set of mem_ptr_wrappers containing the columns which would be given values. All columns should be from the same class.
         * @tparam SelectQuery The type of the select query that would be used in the insert into select query
         * @tparam OnDuplicateKeyUpdateAssignments Set of assigments which would be executed on duplicated data being inserted
         */
        template<typename IntoTable, is_type_container InsertedColumns, typename SelectQuery, is_type_container OnDuplicateKeyUpdateAssignments>
        class InsertIntoSelectQuery : public IInsertSelect
        {
            using into_table = IntoTable;
            using columns = InsertedColumns;
            using select_query = SelectQuery;
            using on_duplicate_key_update_assignments = OnDuplicateKeyUpdateAssignments;

            typename InsertedColumns::tuple_equivalent cols;
            SelectQuery selecting_query;
            typename OnDuplicateKeyUpdateAssignments::tuple_equivalent update_statements;
        public:
            constexpr auto get_columns() const { return cols; }
            constexpr auto get_update_statements() const { return update_statements; }
            constexpr auto get_select_query() const { return selecting_query; }
        public:
            static constexpr bool allow_repeats = false;
            using parameters_type = typename SelectQuery::parameters_type;
            using result_type = IntoTable;

            constexpr InsertIntoSelectQuery(typename InsertedColumns::tuple_equivalent _cols, 
                                    SelectQuery query,
                                    typename OnDuplicateKeyUpdateAssignments::tuple_equivalent stmts) : cols(_cols), selecting_query(query), update_statements(stmts) { }

            template<typename... Statements>
            constexpr auto on_duplicate_key_update(Statements... stmts) const
            {
                return InsertIntoSelectQuery<IntoTable, InsertedColumns, SelectQuery, type_constainer<Statements...>>(get_columns(), get_select_query(), { stmts... });
            }
        };

        template<auto... mem_ptrs> requires (std::is_member_object_pointer_v<decltype(mem_ptrs)> && ...)
        class GenericInsertQuery
        {
            using IntoTable = typename std::tuple_element<0, std::tuple<typename mem_ptr_wrapper<mem_ptrs>::class_type ...>>::type;
            using InsertedColumns = type_constainer<mem_ptr_wrapper<mem_ptrs>...>;
            using OnDuplicateKeyUpdateAssignments = type_constainer<>;
            inline constexpr typename InsertedColumns::tuple_equivalent get_columns() const 
            {
                return { DB<mem_ptrs>... };
            }
            inline constexpr typename OnDuplicateKeyUpdateAssignments::tuple_equivalent get_update_statements() const
            {
                return {};
            }
        public:
            constexpr auto values(auto... arguments) const
            {
                return InsertQuery<IntoTable, InsertedColumns, type_constainer<decltype(arguments)...>, OnDuplicateKeyUpdateAssignments>(get_columns(), {arguments...}, get_update_statements());
            }

            template<typename SelectQuery>
            constexpr auto into(SelectQuery query) const
            {
                return InsertIntoSelectQuery<IntoTable, InsertedColumns, SelectQuery, OnDuplicateKeyUpdateAssignments>(get_columns(), query, get_update_statements());
            }
        };
    }
    template<auto... mem_ptrs>
    constexpr auto insert = details::GenericInsertQuery<mem_ptrs...>();
} // namespace webframe::ORM
