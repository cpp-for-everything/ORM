#pragma once

#include <concepts>

template <typename U, typename T>
concept rvalue_for = requires(T& a, U b) {
	{
		a = b
	};
};

template <typename U, typename T, typename R>
concept rvalue_results_in = requires(T& a, U b) {
	{
		a = b
	} -> std::same_as<R>;
};
