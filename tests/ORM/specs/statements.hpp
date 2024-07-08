#pragma once

#include "../../lib/moka/moka.h"

#include <ORM/ORM.hpp>

using namespace webframe::ORM;
using namespace webframe::ORM::literals;

namespace StatementsTests
{
	struct User
	{
		static constexpr std::string_view entity_name = "Users";
		property<INT, "id"> id;
		property<TEXT<>, "name"> name;
	};

	struct Post
	{
		static constexpr std::string_view entity_name = "Posts";
		property<INT, "id"> id;
		property<INT, "author_id"> author_id;
		property<TEXT<>, "content"> content;
	};

	void init(Moka::Context& it)
	{
		it.describe("Compile-time statements optimization", [](Moka::Context& it) {
			it.describe("Statement expressions", [](Moka::Context& it) {
				it.describe("Const(5) + 6", [](Moka::Context& it) {
					constexpr auto stmt = 5_c + 6;

					it.should("have left side = 5", [&]() {
						constexpr bool check = (stmt.a.a == 5);
						static_assert(check, "Something went wrong with the compile-time value of the constant in the left side of the expression.");
						must_be_equal(check, true, "Left side of Constant<int>(5) + Constant<int>(6) is not constant of 5");
					});

					it.should("have right side = 6", [&]() {
						constexpr bool check = (stmt.b.a == 6);
						static_assert(check, "Something went wrong with the compile-time value of the constant in the right side of the expression.");
						must_be_equal(check, true, "Right side of Constant<int>(5) + Constant<int>(6) is not constant of 6");
					});

					it.should("have operator +", [&]() {
						constexpr bool check = (stmt.operation == details::expression_operators::Plus);
						static_assert(check, "Something went wrong with the compile-time value of the operation of the expression.");
						must_be_equal(check, true, "Last operation of Constant<int>(5) + Constant<int>(6) is not plus");
					});
				});

				it.describe("User::id + 6", [](Moka::Context& it) {
					constexpr auto stmt = DB<&User::id> + 6;

					it.should("have left side = User::id", [&]() {
						constexpr bool check = (stmt.a.equals<&User::id>());
						static_assert(check, "Something went wrong with the compile-time value of the left side of the expression.");
						must_be_equal(check, true, "Left side of User::id + 6 is not User::id");
					});

					it.should("have right side = 6", [&]() {
						constexpr bool check = (stmt.b.a == 6);
						static_assert(check, "Something went wrong with the compile-time value of the constant in the right side of the expression.");
						must_be_equal(check, true, "Right side of User::id + 6 is not constant of 6");
					});

					it.should("have operator +", [&]() {
						constexpr bool check = (stmt.operation == details::expression_operators::Plus);
						static_assert(check, "Something went wrong with the compile-time value of the operation of the expression.");
						must_be_equal(check, true, "Last operation of Constant<int>(5) + Constant<int>(6) is not plus");
					});
				});

				it.describe("User::id + Const(6) * Const(5)", [](Moka::Context& it) {
					constexpr auto stmt = DB<&User::id> + Constant<int>(6) * Constant<int>(5);

					it.should("have left side = User::id", [&]() {
						constexpr bool check = (stmt.a.equals<&User::id>());
						static_assert(check, "Something went wrong with the compile-time value of the left side of the expression.");
						must_be_equal(check, true, "Left side of User::id + 6 is not User::id");
					});

					it.should("have right side = 6 * 5", [&]() {
						constexpr bool check = (stmt.b.a.a == 6) && (stmt.b.b.a == 5) && (stmt.b.operation == details::expression_operators::Mul);
						static_assert(check, "Something went wrong with the compile-time value of the expression in the right side of the expression.");
						must_be_equal(check, true, "Right side of User::id + 6 * 5 is not constant of 6 * 5");
					});

					it.should("have operator +", [&]() {
						constexpr bool check = (stmt.operation == details::expression_operators::Plus);
						static_assert(check, "Something went wrong with the compile-time value of the operation of the expression.");
						must_be_equal(check, true, "Last operation ofUser::id + 6 * 5 is not plus");
					});
				});

				it.describe("Optimizing the constants in User::id + 6 * 5", [](Moka::Context& it) {
					constexpr auto stmt = DB<&User::id> + 6 * 5;

					it.should("have left side = User::id", [&]() {
						constexpr bool check = (stmt.a.equals<&User::id>());
						static_assert(check, "Something went wrong with the compile-time value of the left side of the expression.");
						must_be_equal(check, true, "Left side of User::id + 6 is not User::id");
					});

					it.should("have right side = 30", [&]() {
						constexpr bool check = (stmt.b.a == 30);
						static_assert(check, "Something went wrong with the compile-time value of the expression in the right side of the expression.");
						must_be_equal(check, true, "Right side of User::id + 6 * 5 is not constant of 30");
					});

					it.should("have operator +", [&]() {
						constexpr bool check = (stmt.operation == details::expression_operators::Plus);
						static_assert(check, "Something went wrong with the compile-time value of the operation of the expression.");
						must_be_equal(check, true, "Last operation of User::id + 6 * 5 is not plus");
					});
				});

				it.describe("Optimizing the constants in 6 * 5 + DB<&User::id>", [](Moka::Context& it) {
					constexpr auto stmt = 6 * 5 + DB<&User::id>;

					it.should("have right side = User::id", [&]() {
						constexpr bool check = (stmt.b.equals<&User::id>());
						static_assert(check, "Something went wrong with the compile-time value of the right side of the expression.");
						must_be_equal(check, true, "Right side of User::id + 6 is not User::id");
					});

					it.should("have left side = 30", [&]() {
						constexpr bool check = (stmt.a.a == 30);
						static_assert(check, "Something went wrong with the compile-time value of the expression in the left side of the expression.");
						must_be_equal(check, true, "Left side of User::id + 6 * 5 is not constant of 30");
					});

					it.should("have operator +", [&]() {
						constexpr bool check = (stmt.operation == details::expression_operators::Plus);
						static_assert(check, "Something went wrong with the compile-time value of the operation of the expression.");
						must_be_equal(check, true, "Last operation of User::id + 6 * 5 is not plus");
					});
				});

				it.describe("6 * 5 + User::id", [](Moka::Context& it) {
					constexpr auto stmt = 6_c * 5_c + DB<&User::id>;

					it.should("have left side = 6 * 5", [&]() {
						constexpr bool check = (stmt.a.a.a == 6) && (stmt.a.b.a == 5) && (stmt.a.operation == details::expression_operators::Mul);
						static_assert(check, "Something went wrong with the compile-time value of the expression in the right side of the expression.");
						must_be_equal(check, true, "Right side of User::id + 6 * 5 is not constant of 6 * 5");
					});

					it.should("have right side = User::id", [&]() {
						constexpr bool check = (stmt.b.equals<&User::id>());
						static_assert(check, "Something went wrong with the compile-time value of the left side of the expression.");
						must_be_equal(check, true, "Left side of User::id + 6 is not User::id");
					});

					it.should("have operator +", [&]() {
						constexpr bool check = (stmt.operation == details::expression_operators::Plus);
						static_assert(check, "Something went wrong with the compile-time value of the operation of the expression.");
						must_be_equal(check, true, "Last operation of 6 * 5 + User::id is not plus");
					});
				});
			});
			it.describe("Statement", [](Moka::Context& it) {
				it.describe("User::id = 5 + 6", [](Moka::Context& it) {
					constexpr auto stmt = (DB<& User::id> = 5_c + 6_c);
					it.should("assign value to User::id", [&]() {
						constexpr bool check = stmt.a.equals<&User::id>();
						static_assert(check, "Statement was unable to set the assignee to User::id.");
						must_be_equal(check, true, "Statement was unable to set the assignee to User::id.");
					});
					it.should("assign the value 5 + 6", [&]() {
						constexpr bool check = stmt.b.a.a == 5 && stmt.b.b.a == 6;
						static_assert(check, "Statement was unable to set the assigned value to 5 + 6.");
						must_be_equal(check, true, "Statement was unable to set the assigned value to 5 + 6.");
					});
				});
			});
		});
	}
} // namespace StatementsTests
