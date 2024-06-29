#pragma once

#include "../../lib/moka/moka.h"

#include <ORM/ORM.hpp>

using namespace webframe::ORM;
using namespace webframe::ORM::literals;

template <typename T1, details::rule_operators op1, typename T2, typename U1, details::rule_operators op2, typename U2>
constexpr bool are_same(const Rule<T1, op1, T2>& rule1, const Rule<U1, op2, U2>& rule2)
{
    // Different operations
    if constexpr (op1 != op2)
    {
        return false;
    }
    // Different operand types
    else if constexpr (
        requires {
            {
                rule1.a.operation
            };
        } ^
        requires {
            {
                rule2.a.operation
            };
        })
    {
        return false;
    }
    else if constexpr (
        requires {
            {
                rule1.b.operation
            };
        } ^
        requires {
            {
                rule2.b.operation
            };
        })
    {
        return false;
    }
    else
    {
        bool answer = true;
        // Both rules
        if constexpr (
            requires {
                {
                    rule1.a.operation
                };
            } &&
            requires {
                {
                    rule2.a.operation
                };
            })
        {
            answer = answer && (are_same(rule1.a, rule2.a));
        }
        // Both not rules
        if constexpr (
            !requires {
                {
                    rule1.a.operation
                };
            } &&
            requires {
                {
                    rule2.a.operation
                };
            })
        {
            answer = answer && (rule1.a == rule2.a);
        }
        // Both rules
        if constexpr (
            requires {
                {
                    rule1.b.operation
                };
            } &&
            requires {
                {
                    rule2.b.operation
                };
            })
        {
            answer = answer && (are_same(rule1.b, rule2.b));
        }
        // Both not rules
        if constexpr (
            !requires {
                {
                    rule1.b.operation
                };
            } &&
            requires {
                {
                    rule2.b.operation
                };
            })
        {
            answer = answer && (rule1.b == rule2.b);
        }
        return answer;
    }
}







namespace RulesTests {
    struct User {
        static constexpr std::string_view entity_name = "Users";
        property<INT, "id"> id;
        property<TEXT<>, "name"> name;
    };
    struct Post {
        static constexpr std::string_view entity_name = "Posts";
        property<INT, "id"> id;
        property<INT, "author_id"> author_id;
        property<TEXT<>, "content"> content;
    };

    constexpr auto replace_with_value(auto mem_ptr) {
        if constexpr (std::is_same_v<decltype(mem_ptr), int>) return mem_ptr;
        else if constexpr (std::is_same_v<decltype(mem_ptr), std::string>) return mem_ptr;
        else if constexpr (std::is_same_v<decltype(mem_ptr), std::string_view>) return mem_ptr;
        else if constexpr (std::is_same_v<decltype(mem_ptr), const char*>) return std::string(mem_ptr);

        else if constexpr (mem_ptr.template equals<&User::id>()) { return 1; }
        else if constexpr (mem_ptr.template equals<&User::name>()) { return "Username"; }
        else if constexpr (mem_ptr.template equals<&Post::id>()) { return 2; }
        else if constexpr (mem_ptr.template equals<&Post::author_id>()) { return 3; }
        else if constexpr (mem_ptr.template equals<&Post::content>()) { return "PostContent"; }
    }

    constexpr auto evaluate_rule(auto rule) {
        if constexpr (requires { 
            { rule.operation };
        }) {
            if constexpr (rule.operation == details::And)
                return evaluate_rule(rule.a) && evaluate_rule(rule.b);
            if constexpr (rule.operation == details::Or)
                return evaluate_rule(rule.a) || evaluate_rule(rule.b);
            if constexpr (rule.operation == details::Xor)
                return evaluate_rule(rule.a) ^ evaluate_rule(rule.b);
            if constexpr (rule.operation == details::Greater)
                return evaluate_rule(rule.a) > evaluate_rule(rule.b);
            if constexpr (rule.operation == details::Less)
                return evaluate_rule(rule.a) < evaluate_rule(rule.b);
            if constexpr (rule.operation == details::Greater_or_equal)
                return evaluate_rule(rule.a) >= evaluate_rule(rule.b);
            if constexpr (rule.operation == details::Less_or_equal)
                return evaluate_rule(rule.a) <= evaluate_rule(rule.b);
            if constexpr (rule.operation == details::Equals)
                return evaluate_rule(rule.a) == evaluate_rule(rule.b);
            if constexpr (rule.operation == details::Not_equal)
                return evaluate_rule(rule.a) != evaluate_rule(rule.b);
        }
        if constexpr (!requires { 
            { rule.operation };
        }) {
            return replace_with_value(rule);
        }
    }

    void init(Moka::Context& it) {
        it.describe("Compile-time rule optimization", [](Moka::Context& it) {
            it.describe("User::id > 5", [](Moka::Context& it){
                constexpr auto rule = (P<&User::id> > 5);
                it.should("should have left operand User::id", [rule]() {
                    constexpr bool left_operand_type = std::is_same_v<details::mem_ptr_wrapper<&User::id>, decltype(rule.a)>;
                    must_be_equal(left_operand_type, true, "Type of left operand doesn't match.");
                    must_be_equal(rule.a.to_mem_ptr(), &User::id, "Left operand doesn't match.");
                });

                it.should("should have right operand 5", [rule]() {
                    constexpr bool right_operand_type = std::is_same_v<int, decltype(rule.b)>;
                    must_be_equal(right_operand_type, true, "Type of right operand doesn't match.");
                    must_be_equal(rule.b, 5, "Right operand doesn't match.");
                });

                it.should("should have operation \'>\'", [rule]() {
                    must_be_equal(rule.operation, details::Greater, "Operation doesn't match.");
                });

                it.should("be evaluated as false when User::id is 1", [rule]() {
                    constexpr bool eval = evaluate_rule(rule);
                    must_be_equal(eval, false, "For User::id=1, User::id > 5 is not false wrongly.");
                });
            });
            it.describe("!(User::id > 5)", [](Moka::Context& it){
                constexpr auto rule = !(P<&User::id> > 5);
                it.describe("should be simplified to User::id <= 5", [rule](Moka::Context& it) {
                    it.should("should have left operand User::id", [rule]() {
                        constexpr bool left_operand_type = std::is_same_v<details::mem_ptr_wrapper<&User::id>, decltype(rule.a)>;
                        must_be_equal(left_operand_type, true, "Type of left operand doesn't match.");
                        must_be_equal(rule.a.to_mem_ptr(), &User::id, "Left operand doesn't match.");
                    });

                    it.should("should have right operand 5", [rule]() {
                        constexpr bool right_operand_type = std::is_same_v<int, decltype(rule.b)>;
                        must_be_equal(right_operand_type, true, "Type of right operand doesn't match.");
                        must_be_equal(rule.b, 5, "Right operand doesn't match.");
                    });

                    it.should("should have operation \'<=\'", [rule]() {
                        must_be_equal(rule.operation, details::Less_or_equal, "Operation doesn't match.");
                    });
                });
                it.should("be evaluated as true when User::id is 1", [rule]() {
                    constexpr bool eval = evaluate_rule(rule);
                    must_be_equal(eval, true, "For User::id=1, !(User::id > 5) is not true wrongly.");
                });
            });
            it.describe("!(User::id > 5) && (User::id > 5)", [](Moka::Context& it){
                constexpr auto rule = !(P<&User::id> > 5) && (P<&User::id> > 5);
                it.describe("should be simplified to User::id <= 5 && (User::id > 5)", [rule](Moka::Context& it) {
                    it.should("should have left operand User::id <= 5", [rule]() {
                        auto expected_operand = (P<&User::id> <= 5);
                        constexpr bool match = are_same(rule.a, expected_operand);
                        must_be_equal(match, true, "Left operand doesn't match.");
                        must_be_equal(rule.a.a.to_mem_ptr(), expected_operand.a.to_mem_ptr(), "Left operand doesn't match.");
                        must_be_equal(rule.a.b, expected_operand.b, "Left operand doesn't match.");
                        must_be_equal(rule.a.operation, expected_operand.operation, "Left operand doesn't match.");
                    });

                    it.should("should have right operand User::id > 5", [rule]() {
                        auto expected_operand = (P<&User::id> > 5);
                        constexpr bool match = are_same(rule.b, expected_operand);
                        must_be_equal(match, true, "Right operand doesn't match.");
                        must_be_equal(rule.b.a.to_mem_ptr(), expected_operand.a.to_mem_ptr(), "Right operand doesn't match.");
                        must_be_equal(rule.b.b, expected_operand.b, "Right operand doesn't match.");
                        must_be_equal(rule.b.operation, expected_operand.operation, "Right operand doesn't match.");
                    });

                    it.should("should have operation \'&&\'", [rule]() {
                        must_be_equal(rule.operation, details::And, "Operation doesn't match.");
                    });
                });
                it.should("be evaluated as false when User::id is 1", [rule]() {
                    constexpr bool eval = evaluate_rule(rule);
                    must_be_equal(eval, false, "For User::id=1, !(User::id > 5) && (User::id > 5) is not false wrongly.");
                });
            });
            it.describe("!(Post::id > 5) && (User::id < 5)", [](Moka::Context& it){
                constexpr auto rule = !(P<&Post::id> > 5) && (P<&User::id> < 5);
                it.should("be evaluated as false when User::id is 1", [rule]() {
                    constexpr bool eval = evaluate_rule(rule);
                    must_be_equal(eval, true, "For User::id=1 and Post:id=2, !(Post::id > 5) && (User::id < 5) is not true wrongly.");
                });
            });
            it.describe("!(((Post::author_id >= 1) && !(User::id > 5) && (Post::id < 5)) || ((Post::content == \"text\") ^ (User::name != \"user\")))", [](Moka::Context& it){
                constexpr auto rule = !(((P<&Post::author_id> >= 1) && !(P<&User::id> > 5) && (P<&Post::id> < 5)) || ((P<&Post::content> == "text") ^ (P<&User::name> != "user")));
                it.should("get optimized to (((Post::author_id < 1) || (User::id > 5) || (Post::id >= 5)) && ((Post::content != \"text\")) == (User::name == \"user\"))", [rule]() {
                    auto expected_rule = ((P<&Post::author_id> < 1) || (P<&User::id> > 5) || (P<&Post::id> >= 5)) && (((P<&Post::content> == "text") && (P<&User::name> != "user")) || ((P<&Post::content> != "text") && (P<&User::name> == "user")));
                    auto match = are_same(rule, expected_rule);
                    must_be_equal(match, true, "The rule was not optimized properly.");
                    must_be_equal(evaluate_rule(rule), evaluate_rule(expected_rule), "The rule was not optimized properly.");
                });
                it.should("be evaluated as false when User::id=1; User::name=\"Username\"; Post::id=2; Post::author_id=3; Post::content=\"PostContent\"", [rule]() {
                    constexpr bool eval = evaluate_rule(rule);
                    must_be_equal(eval, false, "Rule evaluation failed.");
                });
            });
        });
    }
}