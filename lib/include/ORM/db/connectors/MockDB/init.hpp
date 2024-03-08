#pragma once

#include "data.hpp"
#include "../interface.hpp"
#include "../../../details/member_pointer.hpp"
#include "../../../rules.hpp"
#include "../../../CRUD/utils.hpp"
#include <iostream>
#include <concepts>
#include <sstream>
#include <type_traits>

namespace webframe::ORM {

    // template<>
    // Connector<MockDB::data::mock_int, int>::operator webframe::ORM::MockDB::data::mock_int() const {
    //     return MockDB::data::mock_int();
    // }

    // template<>
    // Connector<MockDB::data::mock_int, int>::operator MockDB::data::mock_int&() {
    //     MockDB::data::mock_int* answer_ptr = new MockDB::data::mock_int;
    //     MockDB::data::mock_int& answer = *answer_ptr;
    //     return answer;
    // }
    
    // template<>
    // Connector<MockDB::data::mock_string, std::string>::operator MockDB::data::mock_string() const {
    //     return MockDB::data::mock_string();
    // }

    // template<>
    // Connector<MockDB::data::mock_string, std::string>::operator MockDB::data::mock_string&() {
    //     MockDB::data::mock_string* answer_ptr = new MockDB::data::mock_string;
    //     MockDB::data::mock_string& answer = *answer_ptr;
    //     return answer;
    // }

    namespace MockDB {
        class MockDB {
        public:
            static constexpr std::string_view delim = "\n\t";
        private:
            template<size_t ind_placeholders, typename T>
            struct prepare_rule;
            
            template<size_t ind_placeholders, typename T1, auto op, typename T2>
            struct prepare_rule<ind_placeholders, webframe::ORM::Rule<T1, op, T2>> {
                static constexpr size_t run() {
                    if constexpr (std::derived_from<T1, webframe::ORM::IRule>) {
                        constexpr size_t new_placeholder = prepare_rule<ind_placeholders, T1>::run();
                        if constexpr (std::derived_from<T2, webframe::ORM::IRule>) {
                            return prepare_rule<new_placeholder, T2>::run();
                        }
                        return new_placeholder;
                    }
                    if constexpr (!std::derived_from<T1, webframe::ORM::IRule>) {
                        constexpr size_t new_placeholder = ind_placeholders + (webframe::ORM::details::is_orm_placeholder<T1>);
                        if constexpr (std::derived_from<T2, webframe::ORM::IRule>) {
                            return prepare_rule<new_placeholder, T2>::run();
                        }
                        return new_placeholder;
                    }
                }
            };
            
            template<size_t ind_placeholders, typename T1, auto op, typename T2, typename Tuple>
            static void print_rule(std::stringstream& ss, const webframe::ORM::Rule<T1, op, T2> rule, const Tuple vals) {
                if constexpr (std::derived_from<T1, webframe::ORM::IRule>) {
                    constexpr size_t new_placeholder = prepare_rule<ind_placeholders, T1>::run();
                    print_rule<ind_placeholders>(ss, rule._1, vals);
                    ss << " " << op.operator std::string_view() << " ";
                    if constexpr (std::derived_from<T2, webframe::ORM::IRule>) {
                        print_rule<new_placeholder>(ss, rule._2, vals);
                    }
                    if constexpr (!std::derived_from<T2, webframe::ORM::IRule>) {
                        if constexpr (webframe::ORM::details::is_orm_placeholder<T2>) {
                            ss << injection_prevent(vals.template get<new_placeholder>());
                        }
                        if constexpr (!webframe::ORM::details::is_orm_placeholder<T2>) {
                            ss << injection_prevent(rule._2);
                        }
                    }
                }
                if constexpr (!std::derived_from<T1, webframe::ORM::IRule>) {
                    constexpr size_t new_placeholder = ind_placeholders + (webframe::ORM::details::is_orm_placeholder<T1>);
                    if constexpr (webframe::ORM::details::is_orm_placeholder<T1>) {
                        ss << injection_prevent(vals.template get<ind_placeholders>()) << " ";
                    }
                    if constexpr (!webframe::ORM::details::is_orm_placeholder<T1>) {
                        ss << injection_prevent(rule._1) << " ";
                    }
                    ss << op.operator std::string_view() << " ";
                    if constexpr (std::derived_from<T2, webframe::ORM::IRule>) {
                        print_rule<new_placeholder>(ss, rule._2, vals);
                    }
                    if constexpr (!std::derived_from<T2, webframe::ORM::IRule>) {
                        if constexpr (webframe::ORM::details::is_orm_placeholder<T2>) {
                            ss << injection_prevent(vals.template get<new_placeholder>());
                        }
                        if constexpr (!webframe::ORM::details::is_orm_placeholder<T2>) {
                            ss << injection_prevent(rule._2);
                        }
                    }
                }
            }
            
            template<typename T>
            static constexpr auto injection_prevent(T val) {
                if constexpr (std::is_same_v<T, nullptr_t>)
                    return "NULL"sv;
                if constexpr (std::is_same_v<T, int>)
                    return val;
                if constexpr (std::is_same_v<T, std::string>)
                    return "\"" + val + "\"";
                if constexpr (std::is_same_v<T, std::string_view>)
                    return "\"" + val + "\"";
                if constexpr (std::is_same_v<T, const char*>)
                    return std::string("\"") + std::string(val) + "\"";
                if constexpr (std::is_member_pointer_v<T>) {
                    if constexpr (std::is_same_v<webframe::ORM::details::Filler, typename webframe::ORM::details::i_mem_ptr<T>::Table>)
                        return std::string(webframe::ORM::details::i_mem_ptr<T>::type::_name());
                    if constexpr (!std::is_same_v<webframe::ORM::details::Filler, typename webframe::ORM::details::i_mem_ptr<T>::Table>)
                        return std::string(webframe::ORM::details::i_mem_ptr<T>::Table::table_name) + "." + std::string(webframe::ORM::details::i_mem_ptr<T>::type::_name());
                }
                //if constexpr (
                //    !std::is_same_v<T, int> &&
                //    !std::is_same_v<T, std::string> &&
                //    !std::is_same_v<T, std::string_view> &&
                //    !std::is_same_v<T, const char*> &&
                //    !std::is_member_pointer_v<T>
                //)
                //    return val;
            }
            
            template<size_t ind_query, size_t ind_param>
            void execute_insert_impl(std::stringstream& ss, const auto& query,  const auto& tuple_params) const {
                if constexpr (ind_query < std::remove_cvref_t<decltype(query)>::properties::size) {
                    if constexpr (
                        !details::is_property<decltype(query.signature.template get<ind_query>())> &&
                        !details::is_orm_placeholder<decltype(query.signature.template get<ind_query>())>
                    ) {
                        ss << query.signature.template get<ind_query>();
                        if constexpr (ind_query + 1 < std::remove_cvref_t<decltype(query)>::properties::size) {
                            ss << ", ";
                        }
                        execute_insert_impl<ind_query + 1, ind_param>(ss, query, tuple_params);
                    }
                    if constexpr (ind_param < std::remove_cvref_t<decltype(tuple_params)>::size) {
                        if constexpr (
                            details::is_orm_placeholder<decltype(query.signature.template get<ind_query>())> ||
                            details::is_property<decltype(query.signature.template get<ind_query>())>
                        ) {
                            ss << injection_prevent(tuple_params.template get<ind_param>());
                            if constexpr (ind_query + 1 < std::remove_cvref_t<decltype(query)>::properties::size) {
                                ss << ", ";
                            }
                            execute_insert_impl<ind_query + 1, ind_param + 1>(ss, query, tuple_params);
                        }
                    }
                }
                if constexpr (ind_query == std::remove_cvref_t<decltype(query)>::properties::size) {
                    ss << ")";
                    if constexpr (ind_param * 2 <= std::remove_cvref_t<decltype(tuple_params)>::size) {
                        ss << ", (";
                        execute_insert_impl<0, ind_param>(ss, query, tuple_params);
                    }
                }
            }

            std::string execute_insert(auto query,  auto... params) const requires(decltype(query)::tables::size == 1) {
                webframe::ORM::details::orm_tuple<decltype(params)...> args (std::make_tuple(params...));
                std::stringstream ss;
                ss << "INSERT INTO " << std::remove_cvref_t<decltype(query)>::tables::template orm_type<0>::table_name;
                ss << "(";
                for_with<typename decltype(query)::properties>(
                    [&]<size_t i, typename T>() {
                        ss << webframe::ORM::details::i_mem_ptr<decltype(std::declval<T>().get<i>())>::type::_name();
                        if constexpr (i != T::size - 1)
                            ss << ", ";
                    },
                    std::make_index_sequence<std::remove_cvref_t<decltype(query)>::properties::size>{}
                );
                ss << ") VALUES (";
                execute_insert_impl<0, 0>(ss, query, args);
                return ss.str();
            }

            template<std::size_t... Is>
            void for_(auto func, std::index_sequence<Is...>) const {
                (func.template operator()<Is>(), ...);
            }

            template<typename T, std::size_t... Is>
            void for_with(auto func, std::index_sequence<Is...>) const {
                (func.template operator()<Is, T>(), ...);
            }
            
            template<size_t ind_to_select, size_t ind_join, size_t ind_filter, size_t ind_limit, size_t ind_grouping, size_t ind_ordering, size_t ind_param>
            inline void execute_select_impl(std::stringstream& ss, const auto& query,  const auto& tuple_params) const {
                using select_type = std::remove_cvref_t<decltype(query)>;
                if constexpr (ind_to_select < select_type::Response::size) {
                    ss << injection_prevent(query.selected_properties().template get<ind_to_select>());
                    if constexpr (ind_to_select + 1 < select_type::Response::size) {
                        ss << ", ";
                    }
                    execute_select_impl<ind_to_select + 1, ind_join, ind_filter, ind_limit, ind_grouping, ind_ordering, ind_param>(ss, query, tuple_params);
                }
                if constexpr (
                    ind_to_select == select_type::Response::size &&
                    ind_join < select_type::Joins::size
                ) {
                    constexpr auto mode = decltype(query.join_clauses().template get<ind_join>())::mode;
                    if constexpr (mode == webframe::ORM::modes::join::LEFT) ss << " LEFT";
                    if constexpr (mode == webframe::ORM::modes::join::RIGHT) ss << " RIGHT";
                    //if constexpr (mode == webframe::ORM::modes::join::FULL) ss << " FULL";
                    if constexpr (mode == webframe::ORM::modes::join::INNER) ss << " INNER";
                    ss << delim << "JOIN " << decltype(query.join_clauses().template get<ind_join>())::Table::table_name << " ON ";
                    print_rule<ind_param>(ss, query.join_clauses().template get<ind_join>().to_rule(), tuple_params);
                    constexpr size_t next_placeholder = prepare_rule<ind_param, decltype(query.join_clauses().template get<ind_join>().to_rule())>::run();
                    execute_select_impl<ind_to_select, ind_join + 1, ind_filter, ind_limit, ind_grouping, ind_ordering, next_placeholder>(ss, query, tuple_params);
                }
                if constexpr (
                    ind_to_select == select_type::Response::size &&
                    ind_join == select_type::Joins::size &&
                    ind_filter < select_type::Wheres::size
                ) {
                    ss << delim << "WHERE ";
                    print_rule<ind_param>(ss, query.where_clauses().template get<ind_filter>(), tuple_params);
                    constexpr size_t next_placeholder = prepare_rule<ind_param, decltype(query.where_clauses().template get<ind_filter>())>::run();
                    execute_select_impl<ind_to_select, ind_join, ind_filter + 1, ind_limit, ind_grouping, ind_ordering, next_placeholder>(ss, query, tuple_params);
                }
                if constexpr (
                    ind_to_select == select_type::Response::size &&
                    ind_join == select_type::Joins::size &&
                    ind_filter == select_type::Wheres::size &&
                    ind_limit < select_type::Limitations::size
                ) {
                    ss << delim << "LIMIT " << query.limit_clauses().template get<ind_limit>().get_elements_per_page() << " x " << query.limit_clauses().template get<ind_limit>().get_number_of_page();
                    execute_select_impl<ind_to_select, ind_join, ind_filter, ind_limit + 1, ind_grouping, ind_ordering, ind_param>(ss, query, tuple_params);
                }
                if constexpr (
                    ind_to_select == select_type::Response::size &&
                    ind_join == select_type::Joins::size &&
                    ind_filter == select_type::Wheres::size &&
                    ind_limit == select_type::Limitations::size &&
                    ind_grouping < select_type::Groupings::size
                ) {
                    ss << delim << "GROUP BY (";
                    for_([&]<size_t i>() { 
                        ss << injection_prevent(query.group_clauses().template get<ind_grouping>().properties.template get<i>());
                    }, std::make_index_sequence<decltype(query.group_clauses().template get<ind_grouping>())::Properties::size>{});
                    ss << ")";
                    if constexpr(decltype(query.group_clauses().template get<ind_grouping>())::has_rule()) {
                        ss << delim << "HAVING ";
                        print_rule<ind_param>(ss, query.group_clauses().template get<ind_grouping>().to_rule(), tuple_params);
                        constexpr size_t next_placeholder = prepare_rule<ind_param, decltype(query.group_clauses().template get<ind_grouping>().to_rule())>::run();
                        execute_select_impl<ind_to_select, ind_join, ind_filter, ind_limit, ind_grouping + 1, ind_ordering, next_placeholder>(ss, query, tuple_params);
                    }
                    else {
                        execute_select_impl<ind_to_select, ind_join, ind_filter, ind_limit, ind_grouping + 1, ind_ordering, ind_param>(ss, query, tuple_params);
                    }
                }
                if constexpr (
                    ind_to_select == select_type::Response::size &&
                    ind_join == select_type::Joins::size &&
                    ind_filter == select_type::Wheres::size &&
                    ind_limit == select_type::Limitations::size &&
                    ind_grouping == select_type::Groupings::size &&
                    ind_ordering < select_type::OrderBy::size
                ) {
                    if constexpr (ind_ordering == 0)
                        ss << delim << "ORDER BY ";
                    ss << injection_prevent(decltype(query.order_clauses().template get<ind_ordering>())::property);
                    if constexpr (decltype(query.order_clauses().template get<ind_ordering>())::sort == webframe::ORM::modes::order::ASC) {
                        ss << " ASC";
                    }
                    if constexpr (decltype(query.order_clauses().template get<ind_ordering>())::sort == webframe::ORM::modes::order::DESC) {
                        ss << " DESC";
                    }
                    if constexpr (ind_ordering + 1 < decltype(query.order_clauses())::size) {
                        ss << ", ";
                    }
                    execute_select_impl<ind_to_select, ind_join, ind_filter, ind_limit, ind_grouping, ind_ordering + 1, ind_param>(ss, query, tuple_params);
                }
                return;
            }
            std::string execute_select(auto query, auto... params) const {
                webframe::ORM::details::orm_tuple<decltype(params)...> args (std::make_tuple(params...));
                std::stringstream ss;
                ss << "SELECT ";
                if constexpr (decltype(query)::mode == webframe::ORM::modes::select::ALL) ss << "ALL ";
                if constexpr (decltype(query)::mode == webframe::ORM::modes::select::DISTINCT) ss << "DISTINCT ";
                if constexpr (decltype(query)::mode == webframe::ORM::modes::select::DISTINCTROW) ss << "DISTINCTROW ";
                execute_select_impl<0, 0, 0, 0, 0, 0, 0>(ss, query, args);
                return ss.str();
            }

            template<size_t ind_query, size_t ind_param, size_t ind_filter, size_t ind_ordering, size_t ind_limit>
            inline void execute_update_impl(std::stringstream& ss, const auto& query,  const auto& tuple_params) const {
                if constexpr (ind_query < std::remove_cvref_t<decltype(query)>::size) {
                    auto statement = query.updates().template get<ind_query>();
                    ss << injection_prevent(statement._1) << " = ";
                    if constexpr (details::is_orm_placeholder<decltype(statement._2)>) {
                        ss << injection_prevent(tuple_params.template get<ind_param>()) << ", ";
                        execute_update_impl<ind_query+1, ind_param+1, ind_filter, ind_ordering, ind_limit>(ss, query, tuple_params);
                    }
                    if constexpr (!details::is_orm_placeholder<decltype(statement._2)>) {
                        ss << injection_prevent(statement._2);
                        if constexpr (ind_query + 1 != std::remove_cvref_t<decltype(query)>::size)
                            ss << ", ";
                        execute_update_impl<ind_query+1, ind_param, ind_filter, ind_ordering, ind_limit>(ss, query, tuple_params);
                    }
                }
                if constexpr (ind_query == std::remove_cvref_t<decltype(query)>::size) {
                    if constexpr (ind_filter != std::remove_cvref_t<decltype(query)>::where_size) {
                        ss << delim << "WHERE ";
                    }
                    if constexpr (ind_filter == std::remove_cvref_t<decltype(query)>::where_size) {
                        if constexpr (ind_ordering == std::remove_cvref_t<decltype(query)>::order_size) {
                            // re-run functionality not available
                            // if constexpr (ind_limit == std::remove_cvref_t<decltype(query)>::limit_size && ind_param + ind_query + ind_filter + ind_ordering + ind_limit <= std::remove_cvref_t<decltype(tuple_params)>::size) {
                            //     execute_update_impl<0, ind_param, 0, 0, 0>(ss, query, tuple_params);
                            // }
                            if constexpr (ind_limit < std::remove_cvref_t<decltype(query)>::limit_size) {
                                ss << delim << "LIMIT " << query.limit_clauses().template get<ind_limit>().get_elements_per_page() << " x " << query.limit_clauses().template get<ind_limit>().get_number_of_page();
                                execute_update_impl<ind_query, ind_param, ind_filter, ind_ordering, ind_limit + 1>(ss, query, tuple_params);
                            }
                        }
                        if constexpr (ind_ordering < std::remove_cvref_t<decltype(query)>::order_size && std::remove_cvref_t<decltype(query)>::order_size != 0) {
                            if constexpr (ind_ordering == 0) {
                                ss << delim << "ORDER BY ";
                            }
                            ss << injection_prevent(decltype(query.order_clauses().template get<ind_ordering>())::property);
                            if constexpr (decltype(query.order_clauses().template get<ind_ordering>())::sort == webframe::ORM::modes::order::ASC) {
                                ss << " ASC";
                            }
                            if constexpr (decltype(query.order_clauses().template get<ind_ordering>())::sort == webframe::ORM::modes::order::DESC) {
                                ss << " DESC";
                            }
                            if constexpr (ind_ordering + 1 < decltype(query.order_clauses())::size) {
                                ss << ", ";
                            }
                            execute_update_impl<ind_query, ind_param, ind_filter, ind_ordering + 1, ind_limit>(ss, query, tuple_params);
                        }
                    }
                    if constexpr (ind_filter < std::remove_cvref_t<decltype(query)>::where_size) {
                        print_rule<ind_param>(ss, query.wheres().template get<ind_filter>(), tuple_params);
                        constexpr size_t next_placeholder = prepare_rule<ind_param, decltype(query.wheres().template get<ind_filter>())>::run();
                        execute_update_impl<ind_query, next_placeholder, ind_filter + 1, ind_ordering, ind_limit>(ss, query, tuple_params);
                    }
                }
                return;
            }
            std::string execute_update(auto query,  auto... params) const requires(decltype(query)::tables::size == 1) {
                webframe::ORM::details::orm_tuple<decltype(params)...> args (std::make_tuple(params...));
                std::stringstream ss;
                ss << "UPDATE " << decltype(query)::tables::template orm_type<0>::table_name << " SET ";
                execute_update_impl<0, 0, 0, 0, 0>(ss, query, args);
                return ss.str();
            }
            template<size_t ind_param, size_t ind_filter, size_t ind_ordering, size_t ind_limit>
            inline void execute_delete_impl(std::stringstream& ss, const auto& query,  const auto& tuple_params) const {
                if constexpr (ind_filter != std::remove_cvref_t<decltype(query)>::where_size) {
                    ss << delim << "WHERE ";
                }
                if constexpr (ind_filter == std::remove_cvref_t<decltype(query)>::where_size) {
                    if constexpr (ind_ordering == std::remove_cvref_t<decltype(query)>::order_size) {
                        // re-run functionality not available
                        // if constexpr (ind_limit == std::remove_cvref_t<decltype(query)>::limit_size && ind_param + ind_filter + ind_ordering + ind_limit <= std::remove_cvref_t<decltype(tuple_params)>::size) {
                        //     execute_delete_impl<ind_param, 0, 0, 0>(ss, query, tuple_params);
                        // }
                        if constexpr (ind_limit < std::remove_cvref_t<decltype(query)>::limit_size) {
                            ss << delim << "LIMIT " << query.limit_clauses().template get<ind_limit>().get_elements_per_page() << " x " << query.limit_clauses().template get<ind_limit>().get_number_of_page();
                            execute_delete_impl<ind_param, ind_filter, ind_ordering, ind_limit + 1>(ss, query, tuple_params);
                        }
                    }
                    if constexpr (ind_ordering < std::remove_cvref_t<decltype(query)>::order_size && std::remove_cvref_t<decltype(query)>::order_size != 0) {
                        if constexpr (ind_ordering == 0) {
                            ss << delim << "ORDER BY ";
                        }
                        ss << injection_prevent(decltype(query.order_clauses().template get<ind_ordering>())::property);
                        if constexpr (decltype(query.order_clauses().template get<ind_ordering>())::sort == webframe::ORM::modes::order::ASC) {
                            ss << " ASC";
                        }
                        if constexpr (decltype(query.order_clauses().template get<ind_ordering>())::sort == webframe::ORM::modes::order::DESC) {
                            ss << " DESC";
                        }
                        if constexpr (ind_ordering + 1 < decltype(query.order_clauses())::size) {
                            ss << ", ";
                        }
                        execute_delete_impl<ind_param, ind_filter, ind_ordering + 1, ind_limit>(ss, query, tuple_params);
                    }
                }
                if constexpr (ind_filter < std::remove_cvref_t<decltype(query)>::where_size) {
                    print_rule<ind_param>(ss, query.wheres().template get<ind_filter>(), tuple_params);
                    constexpr size_t next_placeholder = prepare_rule<ind_param, decltype(query.wheres().template get<ind_filter>())>::run();
                    execute_delete_impl<next_placeholder, ind_filter + 1, ind_ordering, ind_limit>(ss, query, tuple_params);
                }
                return;
            }
            std::string execute_delete(auto query,  auto... params) const {
                webframe::ORM::details::orm_tuple<decltype(params)...> args (std::make_tuple(params...));
                std::stringstream ss;
                ss << "DELETE " << decltype(query)::table::table_name << " ";
                execute_delete_impl<0, 0, 0, 0>(ss, query, args);
                return ss.str();
            }
        public:
            template<webframe::ORM::CRUD::details::is_query QueryType>
            inline auto execute(QueryType query, auto... params) const {
                // Select query handling
                if constexpr (std::is_base_of_v<webframe::ORM::CRUD::details::select_query_t, decltype(query)>) {
                    return this->execute_select(query, params...);
                }
                // Insert query handling
                if constexpr (std::is_base_of_v<webframe::ORM::CRUD::details::insert_query_t, decltype(query)>) {
                    return this->execute_insert(query, params...);
                }
                // Update query handling
                if constexpr (std::is_base_of_v<webframe::ORM::CRUD::details::update_query_t, decltype(query)>) {
                    return this->execute_update(query, params...);
                }
                // Delete query handling
                if constexpr (std::is_base_of_v<webframe::ORM::CRUD::details::delete_query_t, decltype(query)>) {
                    return this->execute_delete(query, params...);
                }
            }
        };
    }

}
