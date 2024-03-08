#pragma once

#include "../../lib/moka/moka.h"

#include <ORM/ORM.hpp>

using namespace webframe::ORM;

namespace RulesTests {
    struct User {
        property<int, "id"> id;
        property<std::string, "name"> name;
    };
    struct Post {
        property<int, "id"> id;
        property<int, "author_id"> author_id;
        property<std::string, "content"> content;
    };

    constexpr auto replace_with_value(auto mem_ptr) {
        if constexpr (std::is_same_v<decltype(mem_ptr), int>) return mem_ptr;
        if constexpr (std::is_same_v<decltype(mem_ptr), std::string>) return mem_ptr;
        if constexpr (std::is_same_v<decltype(mem_ptr), std::string_view>) return mem_ptr;
        if constexpr (std::is_same_v<decltype(mem_ptr), const char*>) return std::string(mem_ptr);

        if constexpr (std::is_same_v<decltype(mem_ptr), decltype(&User::id)>) { return 1; }
        if constexpr (std::is_same_v<decltype(mem_ptr), decltype(&User::name)>) { return "Username"; }
        if constexpr (std::is_same_v<decltype(mem_ptr), decltype(&Post::id)>) { return 2; }
        if constexpr (std::is_same_v<decltype(mem_ptr), decltype(&Post::author_id)>) { return 3; }
        if constexpr (std::is_same_v<decltype(mem_ptr), decltype(&Post::content)>) { return "PostContent"; }
    }

    constexpr auto evaluate_rule(auto rule) {
        if constexpr (requires { 
            { rule.operation };
        }) {
            if constexpr (rule.operation == "&&"sv)
                return evaluate_rule(rule._1) && evaluate_rule(rule._2);
            if constexpr (rule.operation == "||"sv)
                return evaluate_rule(rule._1) || evaluate_rule(rule._2);
            if constexpr (rule.operation == "^"sv)
                return evaluate_rule(rule._1) ^ evaluate_rule(rule._2);
            if constexpr (rule.operation == ">"sv)
                return evaluate_rule(rule._1) > evaluate_rule(rule._2);
            if constexpr (rule.operation == "<"sv)
                return evaluate_rule(rule._1) < evaluate_rule(rule._2);
            if constexpr (rule.operation == ">="sv)
                return evaluate_rule(rule._1) >= evaluate_rule(rule._2);
            if constexpr (rule.operation == "<="sv)
                return evaluate_rule(rule._1) <= evaluate_rule(rule._2);
            if constexpr (rule.operation == "=="sv)
                return evaluate_rule(rule._1) == evaluate_rule(rule._2);
            if constexpr (rule.operation == "!="sv)
                return evaluate_rule(rule._1) != evaluate_rule(rule._2);
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
                    constexpr bool left_operand_type = std::is_same_v<property<int, "id"> User::*, decltype(rule._1)>;
                    must_be_equal(left_operand_type, true, "Type of left operand doesn't match.");
                    must_be_equal(rule._1, &User::id, "Left operand doesn't match.");
                });

                it.should("should have right operand 5", [rule]() {
                    constexpr bool right_operand_type = std::is_same_v<int, decltype(rule._2)>;
                    must_be_equal(right_operand_type, true, "Type of right operand doesn't match.");
                    must_be_equal(rule._2, 5, "Right operand doesn't match.");
                });

                it.should("should have operation \'>\'", [rule]() {
                    must_be_equal(rule.operation, ">", "Operation doesn't match.");
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
                        constexpr bool left_operand_type = std::is_same_v<property<int, "id"> User::*, decltype(rule._1)>;
                        must_be_equal(left_operand_type, true, "Type of left operand doesn't match.");
                        must_be_equal(rule._1, &User::id, "Left operand doesn't match.");
                    });

                    it.should("should have right operand 5", [rule]() {
                        constexpr bool right_operand_type = std::is_same_v<int, decltype(rule._2)>;
                        must_be_equal(right_operand_type, true, "Type of right operand doesn't match.");
                        must_be_equal(rule._2, 5, "Right operand doesn't match.");
                    });

                    it.should("should have operation \'<=\'", [rule]() {
                        must_be_equal(rule.operation, "<=", "Operation doesn't match.");
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
                        constexpr auto expected_operand = (P<&User::id> <= 5);
                        constexpr bool match = are_same(rule._1, expected_operand);
                        must_be_equal(match, true, "Left operand doesn't match.");
                        must_be_equal(rule._1._1, expected_operand._1, "Left operand doesn't match.");
                        must_be_equal(rule._1._2, expected_operand._2, "Left operand doesn't match.");
                        must_be_equal(rule._1.operation, expected_operand.operation, "Left operand doesn't match.");
                    });

                    it.should("should have right operand User::id > 5", [rule]() {
                        constexpr auto expected_operand = (P<&User::id> > 5);
                        constexpr bool match = are_same(rule._2, expected_operand);
                        must_be_equal(match, true, "Right operand doesn't match.");
                        must_be_equal(rule._2._1, expected_operand._1, "Right operand doesn't match.");
                        must_be_equal(rule._2._2, expected_operand._2, "Right operand doesn't match.");
                        must_be_equal(rule._2.operation, expected_operand.operation, "Right operand doesn't match.");
                    });

                    it.should("should have operation \'&&\'", [rule]() {
                        must_be_equal(rule.operation, "&&", "Operation doesn't match.");
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
                    constexpr auto expected_rule = ((P<&Post::author_id> < 1) || (P<&User::id> > 5) || (P<&Post::id> >= 5)) && ((P<&Post::content> != "text") == (P<&User::name> == "user"));
                    constexpr auto match = are_same(rule, expected_rule);
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