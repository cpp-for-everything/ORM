#pragma once

#include "../../lib/moka/moka.h"
#include "../checks/lvalue.hpp"
#include "../checks/rvalue.hpp"

#include <ORM/ORM.hpp>
#include <string_view>

using namespace webframe::ORM;
using namespace webframe::ORM::literals;

DISABLE_WARNING_PUSH
DISABLE_WARNING_LOSS_OF_DATA
namespace FieldsTests {
void init(Moka::Context &it) {
  it.describe_many<INTEGER<32, signed>, INTEGER<64, signed>,
                   INTEGER<16, signed>, int, long, long long, short>(
      "\'%s\' property",
      {"INTEGER32_signed", "INTEGER64_signed", "INTEGER16_signed", "int",
       "long", "long long", "short"},
      []<typename U>(Moka::Context &it) {
        it.should_many<int, long, long long, short>(
            "be compatible with \'%s\' type",
            {"int", "long", "long long", "short"}, []<typename T>() {
              static_assert(lvalue_act_as<property<U, "age">, T>,
                            "Integer property is not compatible with the type");
              property<U, "age"> a = T(0);
              must_be_equal(a, 0,
                            "Integer property is not compatible with the type");
            });
        it.should_many<int, long, long long, short>(
            "be valid rvalue for \'%s\' type",
            {"int", "long", "long long", "short"}, []<typename T>() {
              static_assert(rvalue_for<property<U, "age">, T>,
                            "Integer property is not compatible with the type");
              property<U, "age"> b = 0;
              T a = b;
              must_be_equal(a, 0,
                            "Integer property is not compatible with the type");
              must_be_equal(b, 0,
                            "Integer property is not compatible with the type");
            });
        it.should_many<int, long, long long, short>(
            "be resulting in valid \'%s\' when put as a rvalue",
            {"int", "long", "long long", "short"}, []<typename T>() {
              constexpr bool value =
                  rvalue_results_in<property<U, "age">, T, T &>;
              static_assert(rvalue_results_in<property<U, "age">, T, T &>,
                            "Integer property is not compatible with the type");
              must_be_equal(value, true,
                            "Integer property is not compatible with the type");
            });
      });

  it.describe_many<FLOAT<100>, DOUBLE<100>, DECIMAL<100, 10>, float, double,
                   long double>(
      "\'%s\' property",
      {"FLOAT", "DOUBLE", "DECIMAL", "float", "double", "long double"},
      []<typename U>(Moka::Context &it) {
        it.should_many<float, double, long double>(
            "be compatible with \'%s\' type",
            {"float", "double", "long double"}, []<typename T>() {
              static_assert(lvalue_act_as<property<U, "age">, T>,
                            "Integer property is not compatible with the type");
              property<U, "age"> a = T(0);
              must_be_equal(a, 0,
                            "Integer property is not compatible with the type");
            });
        it.should_many<float, double, long double>(
            "be valid rvalue for \'%s\' type",
            {"float", "double", "long double"}, []<typename T>() {
              static_assert(rvalue_for<property<U, "age">, T>,
                            "Integer property is not compatible with the type");
              property<U, "age"> b = 0;
              T a = b;
              must_be_equal(a, 0,
                            "Integer property is not compatible with the type");
              must_be_equal(b, 0,
                            "Integer property is not compatible with the type");
            });
        it.should_many<float, double, long double>(
            "be resulting in valid \'%s\' when put as a rvalue",
            {"float", "double", "long double"}, []<typename T>() {
              constexpr bool value =
                  rvalue_results_in<property<U, "age">, T, T &>;
              static_assert(rvalue_results_in<property<U, "age">, T, T &>,
                            "Integer property is not compatible with the type");
              must_be_equal(value, true,
                            "Integer property is not compatible with the type");
            });
      });

  it.describe_many<VARCHAR<100>, CHAR<100>, std::string, std::string_view, int,
                   long, char>(
      "Properties of '%s' type",
      {"VARCHAR", "CHAR", "std::string", "std::string_view", "int", "long",
       "char"},
      [&]<typename T>(Moka::Context &it) {
        it.should_for_values<"name"_sl, "Webframe's most random name"_sl>(
            "have correct name when it is \'%s\'",
            {"name", "Webframe's most random name"}, [&]<auto value>() {
              property<T, value> test_property;
              static_assert(test_property._name() == value.to_sv(),
                            "String property has wrong name");
              must_be_equal(test_property._name(), value.to_sv(),
                            "String property has wrong name");
            });
      });
}
} // namespace FieldsTests
DISABLE_WARNING_POP