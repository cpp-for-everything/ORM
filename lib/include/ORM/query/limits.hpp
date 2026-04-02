#pragma once
#include <cstddef>

namespace orm {

    struct Pagification
    {
        std::size_t elements_per_page{0};
        std::size_t page_number{0};

        [[nodiscard]] constexpr std::size_t get_elements_per_page() const noexcept
        {
            return elements_per_page;
        }

        [[nodiscard]] constexpr std::size_t get_number_of_page() const noexcept
        {
            return page_number;
        }
    };

    namespace literals {

        struct per_page_helper
        {
            std::size_t n;
        };

        struct page_helper
        {
            std::size_t n;
        };

        [[nodiscard]] constexpr per_page_helper operator""_per_page(unsigned long long n)
        {
            return {n};
        }

        [[nodiscard]] constexpr page_helper operator""_page(unsigned long long n)
        {
            return {n};
        }

        [[nodiscard]] constexpr Pagification operator*(per_page_helper p, page_helper g)
        {
            return {p.n, g.n};
        }

        [[nodiscard]] constexpr Pagification operator&(per_page_helper p, page_helper g)
        {
            return {p.n, g.n};
        }

        [[nodiscard]] constexpr Pagification operator&(page_helper g, per_page_helper p)
        {
            return {p.n, g.n};
        }

    } // namespace literals

} // namespace orm
