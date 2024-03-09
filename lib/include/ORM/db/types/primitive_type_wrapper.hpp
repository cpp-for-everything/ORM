#pragma once

#include <concepts>
#include <iostream>

namespace webframe::ORM {
namespace details {
template <typename T>
requires(std::is_fundamental_v<T>) class Wrapper {
protected:
  virtual T &get() = 0;
  virtual const T &get() const = 0;
};

template <typename T, typename U = void>
concept operator_not_available = false;

#define BinaryOperatorWrapper(op, name)                                               \
  template <typename T> struct name;                                                  \
                                                                                      \
  template <typename T>                                                               \
  requires requires(T & a, T b) { {a op b}; }                                         \
  struct name<T> : public Wrapper<T> {                                                \
    template <typename U> inline decltype(T() op U()) operator op(U b) const {        \
      return (this->get() op b);                                                      \
    }                                                                                 \
  };                                                                                  \
                                                                                      \
  template <typename T>                                                               \
  requires(!requires(T & a, T b) { {a op b}; }) struct name<T>                        \
      : public Wrapper<T> {                                                           \
    template <typename U> inline T operator op(U) const {                             \
      static_assert(operator_not_available<T, U>,                                     \
                    "error: invalid operands of types ‘T’ and ‘U’ to binary " \
                    "‘operator" #op "’");                                             \
      return this->get();                                                             \
    }                                                                                 \
  }

#define BinaryRefOperatorWrapper(op, name)                                            \
  template <typename T> struct name;                                                  \
                                                                                      \
  template <typename T>                                                               \
  requires requires(T & a, T b) { {a op b}; }                                         \
  struct name<T> : public Wrapper<T> {                                                \
    template <typename U>                                                             \
    requires(!std::is_base_of_v<std::ios_base, U>) inline T &                         \
    operator op(U b) {                                                                \
      return (this->get() op b);                                                      \
    }                                                                                 \
  };                                                                                  \
                                                                                      \
  template <typename T>                                                               \
  requires(!requires(T & a, T b) { {a op b}; }) struct name<T>                        \
      : public Wrapper<T> {                                                           \
    template <typename U>                                                             \
    requires(!std::is_base_of_v<std::ios_base, U>) inline T &operator op(U) {         \
      static_assert(operator_not_available<T, U>,                                     \
                    "error: invalid operands of types ‘T’ and ‘U’ to binary " \
                    "‘operator" #op "’");                                             \
      return (this->get());                                                           \
    }                                                                                 \
  }

#define UnaryPrefixOperatorWrapper(op, name)                                   \
  template <typename T> struct name;                                           \
                                                                               \
  template <typename T>                                                        \
  requires requires(T & a) { {op a}; }                                         \
  struct name<T> : public Wrapper<T> {                                         \
    inline T &operator op() { return (op this->get()); }                       \
  };                                                                           \
                                                                               \
  template <typename T>                                                        \
  requires(!requires(T & a) { {op a}; }) struct name<T> : public Wrapper<T> {  \
    inline T &operator op() {                                                  \
      static_assert(                                                           \
          operator_not_available<T>,                                           \
          "error: invalid operand of types ‘T’ to unary prefix ‘operator" #op  \
          "’");                                                                \
      return (this->get());                                                    \
    }                                                                          \
  }

#define UnarySuffixOperatorWrapper(op, name)                                   \
  template <typename T> struct name;                                           \
                                                                               \
  template <typename T>                                                        \
  requires requires(T & a) { {a op}; }                                         \
  struct name<T> : public Wrapper<T> {                                         \
    inline T &operator op(int) { return (this->get() op); }                    \
  };                                                                           \
                                                                               \
  template <typename T>                                                        \
  requires(!requires(T & a) { {a op}; }) struct name<T> : public Wrapper<T> {  \
    inline T &operator op(int) {                                               \
      static_assert(                                                           \
          operator_not_available<T>,                                           \
          "error: invalid operand of types ‘T’ to unary suffix ‘operator" #op  \
          "’");                                                                \
      return (this->get());                                                    \
    }                                                                          \
  }

BinaryOperatorWrapper(+, PlusWrapper);
BinaryOperatorWrapper(-, MinusWrapper);
BinaryOperatorWrapper(/, DevisionWrapper);
BinaryOperatorWrapper(*, MultiplicationWrapper);
BinaryOperatorWrapper(%, ModulasWrapper);

BinaryOperatorWrapper(&, BitWiseAndWrapper);
BinaryOperatorWrapper(|, BitWiseOrWrapper);
BinaryOperatorWrapper(^, BitWiseXorWrapper);
BinaryOperatorWrapper(>>, BitWiseShiftRightWrapper);
BinaryOperatorWrapper(<<, BitWiseShiftLeftWrapper);

BinaryOperatorWrapper(==, EqualsWrapper);
BinaryOperatorWrapper(!=, NotEqualsWrapper);
BinaryOperatorWrapper(>, GreaterThanWrapper);
BinaryOperatorWrapper(<, LessThanWrapper);
BinaryOperatorWrapper(>=, GreaterOrEqualWrapper);
BinaryOperatorWrapper(<=, LessOrEqualWrapper);
BinaryOperatorWrapper(&&, AndWrapper);
BinaryOperatorWrapper(||, OrWrapper);

BinaryRefOperatorWrapper(+=, AssignmentPlusWrapper);
BinaryRefOperatorWrapper(-=, AssignmentMinusWrapper);
BinaryRefOperatorWrapper(/=, AssignmentDevisionWrapper);
BinaryRefOperatorWrapper(*=, AssignmentMultiplicationWrapper);
BinaryRefOperatorWrapper(%=, AssignmentModulasWrapper);
BinaryRefOperatorWrapper(>>=, AssignmentShiftRightWrapper);
BinaryRefOperatorWrapper(<<=, AssignmentShiftLeftWrapper);

BinaryRefOperatorWrapper(&=, AssignmentBitWiseAndWrapper);
BinaryRefOperatorWrapper(|=, AssignmentBitWiseOrWrapper);
BinaryRefOperatorWrapper(^=, AssignmentBitWiseXorWrapper);

UnaryPrefixOperatorWrapper(!, PrefixNotWrapper);

UnaryPrefixOperatorWrapper(++, PrefixIncrementWrapper);
UnaryPrefixOperatorWrapper(--, PrefixDecrementWrapper);

UnarySuffixOperatorWrapper(++, SuffixIncrementWrapper);
UnarySuffixOperatorWrapper(--, SuffixDecrementWrapper);

#undef BinaryOperatorWrapper
#undef BinaryRefOperatorWrapper
#undef UnaryPrefixOperatorWrapper
#undef UnarySuffixOperatorWrapper

template <typename T> struct istreamOperationsWrapper : public Wrapper<T> {
  template <class CharT, class Traits>
  friend inline std::basic_istream<CharT, Traits> &
  operator>>(std::basic_istream<CharT, Traits> &in,
             istreamOperationsWrapper<T> &a) {
    return (in >> a.get());
  }
};

template <typename T> struct ostreamOperationsWrapper : public Wrapper<T> {
  template <class CharT, class Traits>
  friend inline std::basic_ostream<CharT, Traits> &
  operator<<(std::basic_ostream<CharT, Traits> &out,
             const ostreamOperationsWrapper<T> &a) {
    return (out << a.get());
  }
};
} // namespace details

template <typename T, details::string_literal name, size_t size, size_t P = 0>
class Primitive : public details::PlusWrapper<T>,
                  public details::MinusWrapper<T>,
                  public details::DevisionWrapper<T>,
                  public details::MultiplicationWrapper<T>,
                  public details::ModulasWrapper<T>,
                  public details::BitWiseAndWrapper<T>,
                  public details::BitWiseOrWrapper<T>,
                  public details::BitWiseXorWrapper<T>,
                  public details::BitWiseShiftRightWrapper<T>,
                  public details::BitWiseShiftLeftWrapper<T>,
                  public details::EqualsWrapper<T>,
                  public details::NotEqualsWrapper<T>,
                  public details::GreaterThanWrapper<T>,
                  public details::LessThanWrapper<T>,
                  public details::GreaterOrEqualWrapper<T>,
                  public details::LessOrEqualWrapper<T>,
                  public details::AndWrapper<T>,
                  public details::OrWrapper<T>,
                  public details::AssignmentPlusWrapper<T>,
                  public details::AssignmentMinusWrapper<T>,
                  public details::AssignmentDevisionWrapper<T>,
                  public details::AssignmentMultiplicationWrapper<T>,
                  public details::AssignmentModulasWrapper<T>,
                  public details::AssignmentShiftRightWrapper<T>,
                  public details::AssignmentShiftLeftWrapper<T>,
                  public details::AssignmentBitWiseAndWrapper<T>,
                  public details::AssignmentBitWiseOrWrapper<T>,
                  public details::AssignmentBitWiseXorWrapper<T>,
                  public details::PrefixNotWrapper<T>,
                  public details::PrefixIncrementWrapper<T>,
                  public details::PrefixDecrementWrapper<T>,
                  public details::SuffixIncrementWrapper<T>,
                  public details::SuffixDecrementWrapper<T>,
                  public details::istreamOperationsWrapper<T>,
                  public details::ostreamOperationsWrapper<T> {
protected:
  T val;
  virtual T &get() final { return val; };
  virtual const T &get() const final { return val; };

public:
  Primitive() : val() {}
  Primitive(auto... _vals) : val(_vals...) {}

public:
  virtual operator T() final { return get(); }

  using native_type = T;
  static constexpr size_t db_size = size;
  static constexpr size_t db_percision = P;
  static constexpr std::string_view db_type = name;
};
} // namespace webframe::ORM