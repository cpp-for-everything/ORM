/**
 *  @file   table.cpp
 *  @brief  Functionalities for entities to be treated as tables
 *  @author Alex Tsvetanov
 *  @date   2022-09-09
 ***********************************************/

#ifndef WEBFRAME_ORM_TABLE_IMPL
#define WEBFRAME_ORM_TABLE_IMPL

#include "../../include/ORM/table.hpp"

DISABLE_WARNING_PUSH
DISABLE_WARNING_UNREACHABLE_CODE

template <typename T> template <size_t i> size_t webframe::ORM::Table<T>::_get_index_by_column(const std::string_view column)
{
	if constexpr (i >= pfr::tuple_size<T>::value)
	{
		return (size_t)-1;
	}
	if constexpr (std::derived_from<pfr::tuple_element_t<i, T>, details::relationship>)
	{
		return (size_t)-1;
	}
	if constexpr (std::derived_from<pfr::tuple_element_t<i, T>, details::property>)
	{
		if (pfr::tuple_element_t<i, T>::_name() == column)
		{
			return i;
		}
	}
	if constexpr (i + 1 < pfr::tuple_size<T>::value)
	{
		return _get_index_by_column<i + 1>(column);
	}
	return (size_t)-1;
}

template <typename T> template <size_t i> T& webframe::ORM::Table<T>::_tuple_to_struct(T& output, const typename webframe::ORM::Table<T>::tuple_t& tuple)
{
	if constexpr (std::derived_from<pfr::tuple_element_t<i, T>, details::property>)
	{
		pfr::get<i>(output) = std::get<i>(tuple);
	}
	if constexpr (std::derived_from<pfr::tuple_element_t<i, T>, details::relationship>)
	{
		pfr::get<i>(output) = std::get<i>(tuple);
	}
	if constexpr (i + 1 < pfr::tuple_size<T>::value)
	{
		return _tuple_to_struct<i + 1>(output, tuple);
	}
	if constexpr (i + 1 >= pfr::tuple_size<T>::value)
	{
		return output;
	}
}

template <typename T> template <size_t i, typename T1> T& webframe::ORM::Table<T>::_to_struct(T& output, T1 arg)
{
	pfr::get<i>(output) = arg;
	return output;
}

template <typename T> template <size_t i, typename T1, typename... Ts> T& webframe::ORM::Table<T>::_to_struct(T& output, T1 arg, Ts... args)
{
	pfr::get<i>(output) = arg;
	return _to_struct<i + 1>(output, args...);
}

template <typename T>
template <size_t i, typename T1>
typename webframe::ORM::Table<T>::tuple_t& webframe::ORM::Table<T>::_to_tuple(typename webframe::ORM::Table<T>::tuple_t& output, T1 arg)
{
	std::get<i>(output) = arg;
	return output;
}

template <typename T>
template <size_t i, typename T1, typename... Ts>
typename webframe::ORM::Table<T>::tuple_t& webframe::ORM::Table<T>::_to_tuple(typename webframe::ORM::Table<T>::tuple_t& output, T1 arg, Ts... args)
{
	std::get<i>(output) = arg;
	return _to_tuple<i + 1>(output, args...);
}

template <typename T> template <typename... Ts> T webframe::ORM::Table<T>::to_struct(Ts... args)
{
	T output;
	return _to_struct<0, Ts...>(output, args...);
}

template <typename T> template <typename... Ts> typename webframe::ORM::Table<T>::tuple_t webframe::ORM::Table<T>::to_tuple(Ts... args)
{
	tuple_t output;
	return _to_tuple<0>(output, args...);
}

template <typename T> constexpr size_t webframe::ORM::Table<T>::get_index_by_column(std::string_view column)
{
	return _get_index_by_column<0>(column);
}

template <typename T> T webframe::ORM::Table<T>::tuple_to_struct(const typename webframe::ORM::Table<T>::tuple_t& tuple)
{
	T output;
	return _tuple_to_struct<0>(output, tuple);
}

DISABLE_WARNING_POP

#endif
