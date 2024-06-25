/**
 *  @file   rules.cpp
 *  @brief  Rule class implementation
 *  @author Alex Tsvetanov
 *  @date   2023-08-31
 ***********************************************/

#ifndef WEBFRAME_ORM_RULES_IMPL
#define WEBFRAME_ORM_RULES_IMPL

#include "../../include/ORM/rules.hpp"

#define IMPLEMENT_RULE_OPERATOR(op)     \
    template< \
        typename T1, details::string_literal op1, typename T2, \
        typename U1, details::string_literal op2, typename U2 \
    > \
    constexpr inline Rule<Rule<T1, op1, T2>, #op, Rule<U1, op2, U2>> operator op (Rule<T1, op1, T2> rule1, Rule<U1, op2, U2> rule2) { \
        return Rule<Rule<T1, op1, T2>, #op, Rule<U1, op2, U2>>(rule1, rule2); \
    }

#define IMPLEMENT_MEM_PTR_OPERATOR(op) \
    template<auto P, typename T1> requires (!std::is_same_v<T1, nullptr_t>) \
    constexpr inline auto operator op (details::mem_ptr<P> a, T1 x) { \
        if constexpr (details::is_mem_ptr_t<T1>) \
            return Rule<typename details::mem_ptr<P>::ptr_t, #op, typename T1::ptr_t>(a.get(), x.get()); \
        if constexpr (details::is_not_mem_ptr_t<T1>) \
            return Rule<typename details::mem_ptr<P>::ptr_t, #op, T1>(a.get(), x); \
    } \
    template<auto P, typename T1> requires (!details::is_mem_ptr_t<T1>) \
    constexpr inline auto operator op (T1 x, details::mem_ptr<P> a) { \
        return Rule<T1, #op, typename details::mem_ptr<P>::ptr_t>(x, a.get()); \
    }

#define IMPLEMENT_MEM_PTR_OPERATOR_WITH_NULL(op) \
    template<auto P> \
    constexpr inline auto operator op (details::mem_ptr<P> a, nullptr_t) { \
        return Rule<typename details::mem_ptr<P>::ptr_t, #op, nullptr_t>(a.get(), nullptr); \
    } \
    template<auto P> \
    constexpr inline auto operator op (nullptr_t, details::mem_ptr<P> a) { \
        return Rule<nullptr_t, #op, typename details::mem_ptr<P>::ptr_t>(nullptr, a.get()); \
    }

namespace webframe::ORM {
    template<details::string_literal op1>
    constexpr inline auto not_op() {
        if constexpr (op1.operator std::string_view() == "&&"sv) {
            return details::string_literal("||");
        }
        if constexpr (op1.operator std::string_view() == "||"sv) {
            return details::string_literal("&&");
        }
        if constexpr (op1.operator std::string_view() == "^"sv) {
            return details::string_literal("==");
        }
        if constexpr (op1.operator std::string_view() == ">"sv) {
            return details::string_literal("<=");
        }
        if constexpr (op1.operator std::string_view() == "<"sv) {
            return details::string_literal(">=");
        }
        if constexpr (op1.operator std::string_view() == ">="sv) {
            return details::string_literal("<");
        }
        if constexpr (op1.operator std::string_view() == "<="sv) {
            return details::string_literal(">");
        }
        if constexpr (op1.operator std::string_view() == "=="sv) {
            return details::string_literal("!=");
        }
        if constexpr (op1.operator std::string_view() == "!="sv) {
            return details::string_literal("==");
        }
        if constexpr (op1.operator std::string_view() == "in"sv) {
            return details::string_literal("not in");
        }
        if constexpr (op1.operator std::string_view() == "not in"sv) {
            return details::string_literal("in");
        }
        if constexpr (op1.operator std::string_view() == "exists"sv) {
            return details::string_literal("not exists");
        }
        if constexpr (op1.operator std::string_view() == "not exists"sv) {
            return details::string_literal("exists");
        }
    }

    template<typename T>
    struct not_type;

    template<details::is_mem_ptr_t T1, details::string_literal op, auto T2>
    class Rule<T1, op, details::mem_ptr<T2>> : public Rule<T1, op, typename details::mem_ptr<T2>::ptr_t> { };

    template<auto T1, details::string_literal op, details::is_mem_ptr_t T2>
    class Rule<details::mem_ptr<T1>, op, T2> : public Rule<typename details::mem_ptr<T1>::ptr_t, op, T2> { };

    template<details::is_not_mem_ptr_t T1, details::string_literal op, details::is_not_mem_ptr_t T2>
    class Rule<T1, op, T2> : public IRule {
    public:
        static constexpr std::string_view operation = op.operator std::string_view();
        T1 _1; T2 _2;
        constexpr Rule() {}
        constexpr Rule(T1 a, T2 b) : _1(a), _2(b) { }
    };

    template<typename T1, typename Table1, details::string_literal op, details::is_not_mem_ptr_t T2>
    class Rule<T1 Table1::*, op, T2> : public IRule {
    public:
        static constexpr std::string_view operation = op.operator std::string_view();
        T1 Table1::* _1; T2 _2;
        constexpr Rule() {}
        constexpr Rule(T1 Table1::* a, T2 b) : _1(a), _2(b) { }
    };

    template<details::is_not_mem_ptr_t T1, details::string_literal op, typename Table2, typename T2>
    class Rule<T1, op, T2 Table2::*> : public IRule {
    public:
        static constexpr std::string_view operation = op.operator std::string_view();
        T1 _1; T2 Table2::* _2;
        constexpr Rule() {}
        constexpr Rule(T1 a, T2 Table2::* b) : _1(a), _2(b) { }
    };

    template<typename T1, typename Table1, details::string_literal op, typename Table2, typename T2>
    class Rule<T1 Table1::*, op, T2 Table2::*> : public IRule {
    public:
        static constexpr std::string_view operation = op.operator std::string_view();
        T1 Table1::* _1; T2 Table2::* _2;
        constexpr Rule() {}
        constexpr Rule(T1 Table1::* a, T2 Table2::* b) : _1(a), _2(b) { }
    };

    template<
        typename T1, details::string_literal op1, typename T2,
        typename U1, details::string_literal op2, typename U2
    >
    constexpr bool are_same (const Rule<T1, op1, T2>& rule1, const Rule<U1, op2, U2>& rule2) {
        // Different operations
        if constexpr (op1.operator std::string_view() != op2.operator std::string_view()) {
            return false;
        }
        // Different operand types
        else if constexpr (requires { {rule1._1.operation};} ^ requires { {rule2._1.operation};}) {
            return false;
        }
        else if constexpr (requires { {rule1._2.operation};} ^ requires { {rule2._2.operation};}) {
            return false;
        }
        else {
            bool answer = true;
            // Both rules
            if constexpr (requires { {rule1._1.operation};} && requires { {rule2._1.operation};}) {
                answer = answer && (are_same(rule1._1, rule2._1));
            }
            // Both not rules
            if constexpr (!requires { {rule1._1.operation};} && requires { {rule2._1.operation};}) {
                answer = answer && (rule1._1 == rule2._1);
            }
            // Both rules
            if constexpr (requires { {rule1._2.operation};} && requires { {rule2._2.operation};}) {
                answer = answer && (are_same(rule1._2, rule2._2));
            }
            // Both not rules
            if constexpr (!requires { {rule1._2.operation};} && requires { {rule2._2.operation};}) {
                answer = answer && (rule1._2 == rule2._2);
            }
            return answer;
        }
    }

    template<typename U1, details::string_literal op, typename U2>
    std::ostream& operator << (std::ostream& out, const Rule<U1, op, U2>& rule) {
        out << "(";
        if constexpr (requires { { typename details::i_mem_ptr<U1>::Table() }; }) {
            out << typeid(typename details::i_mem_ptr<U1>::type).name() << " "
                << typeid(typename details::i_mem_ptr<U1>::Table).name() << "::*";
        }
        if constexpr (!requires { { typename details::i_mem_ptr<U1>::Table() }; }) {
            out << rule._1;
        }
        out << ") " << rule.operation << " (";
        if constexpr (requires { { typename details::i_mem_ptr<U2>::Table() }; }) {
            out << typeid(typename details::i_mem_ptr<U2>::type).name() << " "
                << typeid(typename details::i_mem_ptr<U2>::Table).name() << "::*";
        }
        if constexpr (!requires { { typename details::i_mem_ptr<U2>::Table() }; }) {
            out << rule._2;
        }
        out << ")";
        return out;
    }

    IMPLEMENT_RULE_OPERATOR(==);
    IMPLEMENT_RULE_OPERATOR(!=);
    IMPLEMENT_RULE_OPERATOR(&&);
    IMPLEMENT_RULE_OPERATOR(||);
    IMPLEMENT_RULE_OPERATOR(^);

    IMPLEMENT_MEM_PTR_OPERATOR(==);
    IMPLEMENT_MEM_PTR_OPERATOR(!=);
    IMPLEMENT_MEM_PTR_OPERATOR(<);
    IMPLEMENT_MEM_PTR_OPERATOR(>);
    IMPLEMENT_MEM_PTR_OPERATOR(<=);
    IMPLEMENT_MEM_PTR_OPERATOR(>=);

#undef IMPLEMENT_RULE_OPERATOR
#undef IMPLEMENT_MEM_PTR_OPERATOR

    template<typename T>
    struct not_type {
        using type = T;
        constexpr static inline auto convert(T a) {
            return a;
        };
    };

    template<auto P>
    struct not_type<details::mem_ptr<P>> {
        using type = typename details::mem_ptr<P>::ptr_t;
        constexpr static inline auto convert(typename details::mem_ptr<P>::ptr_t a) {
            return a;
        };
    };

    template<typename T1, details::string_literal op1, typename T2>
    struct not_type<Rule<T1, op1, T2>> { 
        using type = Rule<typename not_type<T1>::type, not_op<op1>(), typename not_type<T2>::type>;
        constexpr static inline type convert(Rule<T1, op1, T2> a) {
            return type(not_type<T1>::convert(a._1), not_type<T2>::convert(a._2));
        };
    };

    template<
        typename T1, details::string_literal op1, typename T2
    >
    constexpr inline Rule<typename not_type<T1>::type, not_op<op1>(), typename not_type<T2>::type> operator! (Rule<T1, op1, T2> a) {
        return not_type<Rule<T1, op1, T2>>::convert(a);
    }

    template<typename T>
    constexpr inline Rule<std::nullptr_t, "exists", T> exists (T a) {
        return Rule<std::nullptr_t, "exists", T>(nullptr, a);
    }
    
    template<typename T>
    constexpr inline Rule<std::nullptr_t, "not exists", T> not_exists (T a) {
        return Rule<std::nullptr_t, "not exists", T>(nullptr, a);
    }
}

#endif
