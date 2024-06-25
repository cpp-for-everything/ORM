#pragma once

#include <tuple>
#include "../details/member_pointer.hpp"
#include "../db/connectors/interface.hpp"
#include "../details/orm_tuple.hpp"

namespace webframe::ORM::CRUD::details
{

	// Unique tables utils
	template <typename U, typename T> struct contains;

	template <typename U, typename... Ts> struct contains<U, std::tuple<Ts...>>
	{
		static constexpr inline bool check()
		{
			return (std::is_same_v<U, Ts> || ...);
		}
	};

	template <typename U> struct contains<U, std::tuple<>>
	{
		static constexpr inline bool check()
		{
			return false;
		}
	};

	template <typename U, typename Tuple> constexpr bool contains_v = contains<U, Tuple>::check();

	template <typename T, typename Tuple>
	concept not_new_type = contains_v<T, Tuple>;

	template <typename T, typename Tuple>
	concept new_type = !not_new_type<T, Tuple>;

	template <typename T> struct type_container
	{
		using type = T;
		constexpr type_container()
		{
		}
	};

	template <typename orm_tuple_type> struct unique_first_tables;

	template <> struct unique_first_tables<webframe::ORM::details::orm_tuple<>>
	{
		static constexpr auto empty_tuple()
		{
			return type_container<std::tuple<>>();
		}
		using types = std::tuple<>;
		using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<std::tuple<>>()));
	};

	template <typename Statement1, typename... Statements> struct unique_first_tables<webframe::ORM::details::orm_tuple<Statement1, Statements...>>
	{
		static constexpr auto empty_tuple()
		{
			using T = decltype(std::declval<Statement1>()._1);
			using Ts = webframe::ORM::details::orm_tuple<Statements...>;
			// skip non-property things
			if constexpr (!webframe::ORM::details::is_property<T> || webframe::ORM::details::is_orm_placeholder<T>)
			{
				if constexpr (Ts::size == 0)
				{
					return type_container<std::tuple<>>();
				}
				else
				{
					return unique_first_tables<Ts>::empty_tuple();
				}
			}
			if constexpr (webframe::ORM::details::is_property<T> && !webframe::ORM::details::is_orm_placeholder<T>)
			{
				// so we end up with a property
				if constexpr (Ts::size == 0)
				{
					return type_container<std::tuple<typename webframe::ORM::details::i_mem_ptr<T>::Table>>();
				}

				constexpr auto empty_tuple_of_righter_tables = unique_first_tables<Ts>::empty_tuple();

				if constexpr (new_type<typename webframe::ORM::details::i_mem_ptr<T>::Table, typename decltype(empty_tuple_of_righter_tables)::type>)
				{
					return type_container<decltype(std::tuple_cat(
						std::tuple<typename webframe::ORM::details::i_mem_ptr<T>::Table>(), typename decltype(empty_tuple_of_righter_tables)::type()))>();
				}

				if constexpr (not_new_type<typename webframe::ORM::details::i_mem_ptr<T>::Table, typename decltype(empty_tuple_of_righter_tables)::type>)
				{
					return empty_tuple_of_righter_tables;
				}
			}
		}
		using types = typename decltype(empty_tuple())::type;
		using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<typename decltype(empty_tuple())::type>()));
	};

	template <typename orm_tuple_type> struct second_properties;

	template <typename Statement1, typename... Statements> struct second_properties<webframe::ORM::details::orm_tuple<Statement1, Statements...>>
	{
		static constexpr auto empty_tuple()
		{
			using T = decltype(std::declval<Statement1>()._2);
			using Ts = webframe::ORM::details::orm_tuple<Statements...>;
			// constexpr bool is_property = webframe::ORM::details::is_property<T>;
			//  skip property things
			// if constexpr (is_property) {
			//     if constexpr (Ts::size == 0)
			//         return type_container<std::tuple<>>();
			//     if constexpr (Ts::size != 0)
			//         return second_properties<Ts>::empty_tuple();
			// }
			// if constexpr (!is_property)
			{
				// so we end up with a property
				if constexpr (Ts::size == 0)
				{
					return type_container<std::tuple<T>>();
				}

				constexpr auto empty_tuple_of_righter_tables = second_properties<Ts>::empty_tuple();

				return type_container<decltype(std::tuple_cat(std::tuple<T>(), typename decltype(empty_tuple_of_righter_tables)::type()))>();
			}
			static_assert(true, "balo si e mamata");
		}
		using types = typename decltype(empty_tuple())::type;
		using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<types>()));
	};

	template <> struct second_properties<webframe::ORM::details::orm_tuple<>>
	{
		static constexpr type_container<std::tuple<>> empty_tuple()
		{
			return type_container<std::tuple<>>();
		}
		using types = typename decltype(empty_tuple())::type;
		using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<types>()));
	};

	// unique tables using mem_ptr
	template <auto... Ts> struct unique_tables;

	template <> struct unique_tables<>
	{
		static constexpr auto empty_tuple()
		{
			return type_container<std::tuple<>>();
		}
	};

	template <auto T, auto... Ts> struct unique_tables<T, Ts...>
	{
		static constexpr auto empty_tuple()
		{
			// skip non-property things
			if constexpr (!webframe::ORM::details::is_property<decltype(T)> || webframe::ORM::details::is_orm_placeholder<decltype(T)>)
			{
				if constexpr (sizeof...(Ts) == 0)
				{
					return type_container<std::tuple<>>();
				}
				else
				{
					return unique_tables<Ts...>::empty_tuple();
				}
			}
			if constexpr (webframe::ORM::details::is_property<decltype(T)> && !webframe::ORM::details::is_orm_placeholder<decltype(T)>)
			{
				// so we end up with a property
				if constexpr (sizeof...(Ts) == 0)
				{
					return type_container<std::tuple<typename webframe::ORM::details::mem_ptr<T>::Table>>();
				}

				constexpr auto empty_tuple_of_righter_tables = unique_tables<Ts...>::empty_tuple();

				if constexpr (new_type<typename webframe::ORM::details::mem_ptr<T>::Table, typename decltype(empty_tuple_of_righter_tables)::type>)
				{
					return type_container<decltype(std::tuple_cat(
						std::tuple<typename webframe::ORM::details::mem_ptr<T>::Table>(), typename decltype(empty_tuple_of_righter_tables)::type()))>();
				}

				if constexpr (not_new_type<typename webframe::ORM::details::mem_ptr<T>::Table, typename decltype(empty_tuple_of_righter_tables)::type>)
				{
					return empty_tuple_of_righter_tables;
				}
			}
		}
		using types = typename decltype(empty_tuple())::type;
		using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<typename decltype(empty_tuple())::type>()));
	};

	// unique tables using tuple
	template <typename tuple> struct unique_tables_t;

	template <typename... Ts> struct unique_tables_t<webframe::ORM::details::orm_tuple<Ts...>>
	{
		static constexpr auto empty_tuple()
		{
			return type_container<std::tuple<>>();
		}
		using types = typename unique_tables_t<std::tuple<Ts...>>::types;
		using orm_tuple_type = typename unique_tables_t<std::tuple<Ts...>>::orm_tuple_type;
	};

	template <> struct unique_tables_t<std::tuple<>>
	{
		static constexpr auto empty_tuple()
		{
			return type_container<std::tuple<>>();
		}
		using types = std::tuple<>;
		using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<std::tuple<>>()));
	};

	template <typename T, typename... Ts> struct unique_tables_t<std::tuple<T, Ts...>>
	{
		static constexpr auto empty_tuple()
		{
			// skip non-property things
			if constexpr (!webframe::ORM::details::is_property<T> || webframe::ORM::details::is_orm_placeholder<T>)
			{
				if constexpr (sizeof...(Ts) == 0)
				{
					return type_container<std::tuple<>>();
				}
				else
				{
					return unique_tables_t<std::tuple<Ts...>>::empty_tuple();
				}
			}
			if constexpr (webframe::ORM::details::is_property<T> && !webframe::ORM::details::is_orm_placeholder<T>)
			{
				// so we end up with a property
				if constexpr (sizeof...(Ts) == 0)
				{
					return type_container<std::tuple<typename webframe::ORM::details::i_mem_ptr<T>::Table>>();
				}

				constexpr auto empty_tuple_of_righter_tables = unique_tables_t<std::tuple<Ts...>>::empty_tuple();

				if constexpr (new_type<typename webframe::ORM::details::i_mem_ptr<T>::Table, typename decltype(empty_tuple_of_righter_tables)::type>)
				{
					return type_container<decltype(std::tuple_cat(
						std::tuple<typename webframe::ORM::details::i_mem_ptr<T>::Table>(), typename decltype(empty_tuple_of_righter_tables)::type()))>();
				}

				if constexpr (not_new_type<typename webframe::ORM::details::i_mem_ptr<T>::Table, typename decltype(empty_tuple_of_righter_tables)::type>)
				{
					return empty_tuple_of_righter_tables;
				}
			}
		}
		using types = typename decltype(empty_tuple())::type;
		using orm_tuple_type = decltype(webframe::ORM::details::orm_tuple(std::declval<typename decltype(empty_tuple())::type>()));
	};

	// Query utils
	class IQuery
	{
	};
	template <typename T>
	concept is_query = std::derived_from<T, IQuery>;

	template <typename Query, typename... args>
	concept params_match = requires {
		{
			webframe::ORM::details::convertible_tuples<typename Query::properties, typename webframe::ORM::details::orm_tuple<args...>>{}
		} -> webframe::ORM::details::pseudo_concept;
	};

	// DB << query operator
	template <typename DB, is_query QueryType> auto operator<<(DB db, const QueryType query)
	{
		return [ db, query ](auto... args)
			requires(params_match<QueryType, decltype(args)...>)
		{
			if constexpr (std::is_same_v<decltype(db.execute(query, args...)), void>)
			{
				db.execute(query, args...);
				return;
			}
			if constexpr (!std::is_same_v<decltype(db.execute(query, args...)), void>)
			{
				return db.execute(query, args...);
			}
		};
	}
	// concepts
	class select_query_t : public IQuery
	{
	};
	class insert_query_t : public IQuery
	{
	};
	class update_query_t : public IQuery
	{
	};
	class delete_query_t : public IQuery
	{
	};
} // namespace webframe::ORM::CRUD::details

namespace webframe::ORM::modes
{
	enum select
	{
		ALL,
		DISTINCT,
		DISTINCTROW
	};

	enum join
	{
		INNER,
		LEFT,
		RIGHT,
		FULL
	};

	enum order
	{
		DEFAULT,
		ASC,
		DESC
	};
} // namespace webframe::ORM::modes
