/**
 *  @file   limits.hpp
 *  @brief  Pagification literals
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#pragma once

namespace webframe::ORM
{
	class Pagification
	{
		private:
		unsigned long long elements_per_page;
		unsigned long long number_of_page;

		protected:
		class Page;
		class Per_page;

		public:
		unsigned long long get_elements_per_page() const;
		unsigned long long get_number_of_page() const;
		constexpr Pagification(unsigned long long = 0, unsigned long long = 0);
		friend constexpr Pagification operator&(Per_page, Page);
		friend constexpr Pagification operator&(Page, Per_page);
		friend constexpr Per_page operator""_per_page(unsigned long long);
		friend constexpr Page operator""_page(unsigned long long);
	};
} // namespace webframe::ORM

#include "../../src/ORM/limits.cpp"
