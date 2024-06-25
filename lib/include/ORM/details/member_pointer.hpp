/**
 *  @file   member_pointer.hpp
 *  @brief  Concept definitions different types of member pointers
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#ifndef WEBFRAME_ORM_MEM_PTR
#define WEBFRAME_ORM_MEM_PTR

#include <concepts>
#include <string_view>
#include <iostream>
#include "string_literal.hpp"

namespace webframe::ORM::details {
    template<typename T>
    concept is_property = std::is_member_pointer_v<T>;
    
    template<typename T>
    concept is_not_property = !std::is_member_pointer_v<T>;

    template<typename T>
    concept is_query_string = std::is_convertible_v<T, std::string_view> && !is_property<T>;

    template <typename T> class i_mem_ptr;
    template <auto property> class mem_ptr;
    
    class mem_ptr_detector {};
    
    template <typename _Table, typename T>
    class i_mem_ptr<T _Table::*> : public mem_ptr_detector {
    public:
        using Table = _Table;
        using type = T;
        using ptr_t = T Table::*;
    public:
        virtual constexpr ptr_t get() const = 0;
    };

    template<typename T>
    concept is_mem_ptr_t = std::derived_from<T, mem_ptr_detector>;

    template<typename T>
    concept is_not_mem_ptr_t = !is_mem_ptr_t<T>;
}

namespace webframe::ORM {

    class IStatement {};
    template<typename T>
    concept is_statement = std::derived_from<T, IStatement>;
    template<typename T1, details::string_literal op, typename T2>
    class Statement;

    template<details::is_mem_ptr_t T1, details::string_literal op, details::is_not_mem_ptr_t T2>
    class Statement<T1, op, T2> : public IStatement {
    public:
        static constexpr std::string_view operation = op.operator std::string_view();
        typename T1::ptr_t _1; T2 _2;
        constexpr Statement() {}
        constexpr Statement(typename T1::ptr_t a, T2 b) : _1(a), _2(b) { }
    };

    template<details::is_mem_ptr_t T1, details::string_literal op, details::is_mem_ptr_t T2>
    class Statement<T1, op, T2> : public IStatement {
    public:
        static constexpr std::string_view operation = op.operator std::string_view();
        typename T1::ptr_t _1; typename T2::ptr_t _2;
        constexpr Statement() {}
        constexpr Statement(typename T1::ptr_t a, typename T2::ptr_t b) : _1(a), _2(b) { }
    };

    template<
        details::is_mem_ptr_t T1, details::string_literal op1, typename T2,
        details::is_mem_ptr_t U1, details::string_literal op2, typename U2
    >
    constexpr inline bool are_same (const Statement<T1, op1, T2>& Statement1, const Statement<U1, op2, U2>& Statement2) {
        // Different operations
        if constexpr (op1.operator std::string_view() != op2.operator std::string_view()) {
            return false;
        }
        // Different operand types
        else if constexpr (requires { {Statement1._1.operation};} ^ requires { {Statement2._1.operation};}) {
            return false;
        }
        else if constexpr (requires { {Statement1._2.operation};} ^ requires { {Statement2._2.operation};}) {
            return false;
        }
        else {
            bool answer = true;
            // Both Statements
            if constexpr (requires { {Statement1._1.operation};} && requires { {Statement2._1.operation};}) {
                answer = answer && (are_same(Statement1._1, Statement2._1));
            }
            // Both not Statements
            if constexpr (!requires { {Statement1._1.operation};} && requires { {Statement2._1.operation};}) {
                answer = answer && (Statement1._1 == Statement2._1);
            }
            // Both Statements
            if constexpr (requires { {Statement1._2.operation};} && requires { {Statement2._2.operation};}) {
                answer = answer && (are_same(Statement1._2, Statement2._2));
            }
            // Both not Statements
            if constexpr (!requires { {Statement1._2.operation};} && requires { {Statement2._2.operation};}) {
                answer = answer && (Statement1._2 == Statement2._2);
            }
            return answer;
        }
    }

    template<details::is_mem_ptr_t U1, details::string_literal op, typename U2>
    inline std::ostream& operator << (std::ostream& out, const Statement<U1, op, U2>& statement) {
        out << "(";
        if constexpr (requires { { typename details::i_mem_ptr<U1>::Table() }; }) {
            out << typeid(typename details::i_mem_ptr<U1>::type).name() << " "
                << typeid(typename details::i_mem_ptr<U1>::Table).name() << "::*";
        }
        if constexpr (!requires { { typename details::i_mem_ptr<U1>::Table() }; }) {
            out << statement._1;
        }
        out << ") " << statement.operation << " (";
        if constexpr (requires { { typename details::i_mem_ptr<U2>::Table() }; }) {
            out << typeid(typename details::i_mem_ptr<U2>::type).name() << " "
                << typeid(typename details::i_mem_ptr<U2>::Table).name() << "::*";
        }
        if constexpr (!requires { { typename details::i_mem_ptr<U2>::Table() }; }) {
            out << statement._2;
        }
        out << ")";
        return out;
    }
}

namespace webframe::ORM::details {
    template <auto>
    class mem_ptr;

    template <auto property> requires (!std::is_same_v<decltype(property), nullptr_t>)
    class mem_ptr<property> : public i_mem_ptr<decltype(property)> {
    public:
        using typename i_mem_ptr<decltype(property)>::Table;
        using typename i_mem_ptr<decltype(property)>::type;
        using typename i_mem_ptr<decltype(property)>::ptr_t;

        constexpr inline ptr_t get() const { return property; }
        
        constexpr inline auto operator= (auto x) const {
            if constexpr (details::is_mem_ptr_t<decltype(x)>)
                return webframe::ORM::Statement<typename details::mem_ptr<property>, "=", typename decltype(x)::ptr_t>(this->get(), x.get());
            if constexpr (details::is_not_mem_ptr_t<decltype(x)>)
                return webframe::ORM::Statement<typename details::mem_ptr<property>, "=", decltype(x)>(this->get(), x);
        }
    };

    struct Filler {
    private:
        template<typename T, string_literal lit>
        struct pseudo_property {    
        public:
            using var_t = T;
            static constexpr std::string_view _name() {
                return lit.value;
            }
        };
    public:
        static constexpr std::string_view table_name = "";
        pseudo_property<nullptr_t, "NULL"> Null;
    };

    template <>
    class mem_ptr<nullptr> : public mem_ptr<&Filler::Null> {
    public:
        using typename i_mem_ptr<decltype(&Filler::Null)>::Table;
        using typename i_mem_ptr<decltype(&Filler::Null)>::type;
        using typename i_mem_ptr<decltype(&Filler::Null)>::ptr_t;
    };
}

namespace webframe::ORM {
    template <auto Property>
    constexpr details::mem_ptr<Property> P = details::mem_ptr<Property>();
}

#include "../../../src/ORM/details/member_pointer.cpp"

#endif