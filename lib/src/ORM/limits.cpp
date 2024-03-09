/**
 *  @file   limits.cpp
 *  @brief  Pagification literals implementation
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#ifndef WEBFRAME_ORM_LIMITS_IMPL
#define WEBFRAME_ORM_LIMITS_IMPL

#include "../../include/ORM/limits.hpp"

constexpr webframe::ORM::Pagification::Pagification(
    unsigned long long _elements_per_page, unsigned long long _number_of_page)
    : elements_per_page(_elements_per_page), number_of_page(_number_of_page) {}

inline unsigned long long
webframe::ORM::Pagification::get_elements_per_page() const {
  return elements_per_page;
}
inline unsigned long long
webframe::ORM::Pagification::get_number_of_page() const {
  return number_of_page;
}

class webframe::ORM::Pagification::Per_page {
public:
  unsigned long long elements_per_page;

  constexpr Per_page() : elements_per_page(0) {}
  constexpr Per_page(unsigned long long _elements_per_page)
      : elements_per_page(_elements_per_page) {}
  constexpr operator webframe::ORM::Pagification() const {
    return Pagification(elements_per_page);
  }
};

class webframe::ORM::Pagification::Page {
public:
  unsigned long long number_of_page;

  constexpr Page() : number_of_page(0) {}
  constexpr Page(unsigned long long _number_of_page)
      : number_of_page(_number_of_page) {}
  constexpr operator webframe::ORM::Pagification() const {
    return webframe::ORM::Pagification(0, number_of_page);
  }
};

namespace webframe::ORM {
constexpr Pagification::Per_page
operator""_per_page(unsigned long long elements_per_page) {
  return Pagification::Per_page(elements_per_page);
}
constexpr Pagification::Page
operator""_page(unsigned long long number_of_page) {
  return Pagification::Page(number_of_page);
}

constexpr Pagification operator&(Pagification::Per_page a,
                                 Pagification::Page b) {
  return Pagification(a.elements_per_page, b.number_of_page);
}

constexpr Pagification operator&(Pagification::Page b,
                                 Pagification::Per_page a) {
  return Pagification(a.elements_per_page, b.number_of_page);
}
} // namespace webframe::ORM

#endif
