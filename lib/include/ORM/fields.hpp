/**
 *  @file   fields.hpp
 *  @brief  Properties and relationships structures
 *  @author Alex Tsvetanov
 *  @date   2023-06-04
 ***********************************************/

#pragma once
#include <ORM/ORM-version.hpp>

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <ORM/std/string_view>
#include <pfr.hpp>

#include <ORM/details/string_literal.hpp>
#include <ORM/details/fundamental_type.hpp>
#include <ORM/details/member_pointer.hpp>

namespace webframe::ORM {
    namespace details {
        class property {};

        class relationship {};
    }

    template<typename T, details::string_literal lit>
    class property;

    template<details::is_inheritable T, details::string_literal lit> requires (!std::is_same_v<T, std::string> && !std::is_same_v<T, std::string_view>)
    class property<T, lit> : public T, public details::property {
    public:
        using var_t = typename T::native_type;
        static constexpr std::string_view _name() {
            return lit.value;
        }
        
        property() : T() { }
        property(const T& a) : T(a) { }

        template<typename Y> requires requires(Y a) { { static_cast<var_t>(a) }; }
        property(const Y& a) : T(static_cast<var_t>(a)) { }
    };

    template<details::is_inheritable T, details::string_literal lit> requires (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>)
    class property<T, lit> : public T, public details::property {
    public:
        using var_t = T;
        static constexpr std::string_view _name() {
            return lit.value;
        }

        explicit inline operator T() {
            return *this;
        }

        inline const T& operator = (auto _value) {
            return static_cast<T&>(*this) = _value; 
        }
    };

    template<details::is_not_inheritable T, details::string_literal lit>
    class property<T, lit> : public details::property {
    protected:
        T value;
    public:
        using var_t = T;
        static constexpr std::string_view __name = lit.value;
        static constexpr std::string_view _name() {
            return lit.value;
        }

        property(const T value) : value(value) {}
        property(T& value) : value(value) {}

        property() : value() {}

        inline operator T() const { return value; }
        inline operator T& () { return value; }

        const T& operator = (auto _value) { 
            value = _value;
            return value; 
        }
    };

    enum RelationshipTypes {
        one2one,
        one2many
    };

    template<RelationshipTypes, auto, details::string_literal>
    class relationship;

    template<RelationshipTypes _type, typename P, class T, P T::* external_ref, details::string_literal name>
    class relationship <_type, external_ref, name> : public P, public details::relationship {
    public:
        using Source = T;
        static constexpr RelationshipTypes type = _type;
        static constexpr inline std::string_view _name() { return name.to_sv(); }
    protected:
        std::vector<T> records;
    };
}

#include "../../src/ORM/fields.cpp"