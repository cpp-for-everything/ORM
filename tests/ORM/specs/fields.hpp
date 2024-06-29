#pragma once

#include "../../lib/moka/moka.h"
#include "../checks/lvalue.hpp"
#include "../checks/rvalue.hpp"

#include <ORM/ORM.hpp>
#include <string_view>

using namespace webframe::ORM;
using namespace webframe::ORM::literals;

namespace FieldsTests {
    void init(Moka::Context& it) {

        it.describe_many<TINYINT, SMALLINT, INT, BIGINT, UTINYINT, USMALLINT, UINT, UBIGINT>("\'%s\' property", 
            {"int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",}, []<typename U>(Moka::Context& it) {
            it.should_many<int, long, long long, short>("be compatible with \'%s\' type", 
                        {"int", "long", "long long", "short"}, []<typename T>() {
                bool value = lvalue_act_as<property<U, "age">, T>;
                must_be_equal(value, true, "Integer property is not compatible with the type");
            });
            it.should_many<int, long, long long, short>("be valid rvalue for \'%s\' type", 
                        {"int", "long", "long long", "short"}, []<typename T>() {
                bool value = rvalue_for<property<U, "age">, T>;
                must_be_equal(value, true, "Integer property is not compatible with the type");
            });
            it.should_many<int, long, long long, short>("be resulting in valid \'%s\' when put as a rvalue", 
                        {"int", "long", "long long", "short"}, []<typename T>() {
                bool value = rvalue_results_in<property<U, "age">, T, T&>;
                must_be_equal(value, true, "Integer property is not compatible with the type");
            });
        });

        it.describe_many<FLOAT<>, DOUBLE<>>("\'%s\' property", 
            {"float", "double", "long double"}, []<typename U>(Moka::Context& it) {
            it.should_many<float, double, long double>("be compatible with \'%s\' type", 
                        {"float", "double", "long double"}, []<typename T>() {
                bool value = lvalue_act_as<property<U, "age">, T>;
                must_be_equal(value, true, "Integer property is not compatible with the type");
            });
            it.should_many<float, double, long double>("be valid rvalue for \'%s\' type", 
                        {"float", "double", "long double"}, []<typename T>() {
                bool value = rvalue_for<property<U, "age">, T>;
                must_be_equal(value, true, "Integer property is not compatible with the type");
            });
            it.should_many<float, double, long double>("be resulting in valid \'%s\' when put as a rvalue", 
                        {"float", "double", "long double"}, []<typename T>() {
                bool value = rvalue_results_in<property<U, "age">, T, T&>;
                must_be_equal(value, true, "Integer property is not compatible with the type");
            });
        });

        it.describe_many<TEXT<100>, INT, BIGINT, CHAR<1>>("Properties of '%s' type", {"TEXT<100>", "INT", "BIGINT", "CHAR<1>"}, [&]<typename T>(Moka::Context& it) {
            it.should_for_values<"name"_sl, "Webframe's most random name"_sl>("have correct name when it is \'%s\'",
                        {"name", "Webframe's most random name"}, [&]<auto value>() {
                property<T, value> test_property;
                must_be_equal(test_property.name(), value.to_sv(), "String property has wrong name");
            });
        });

    }
}