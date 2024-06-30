#pragma once

#include <concepts>

template <typename T>
concept lvalue = requires(T& a, T b) {
	{
		a = b
	};
};

template <typename T, typename U>
concept lvalue_act_as = requires(T& a, U b) {
	{
		a = b
	};
};

template <typename T, typename U, typename R>
concept lvalue_results_in = requires(T& a, U b) {
	{
		a = b
	} -> std::same_as<R>;
};
