// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"
TOML_DISABLE_SPAM_WARNINGS;

TEST_CASE("node::visit")
{
	value<int64_t> val{ 3 };

	// check lvalue propagates correctly
	static_cast<node&>(val).visit(
		[](auto&& v) noexcept
		{
			using val_ref_type = decltype(v);
			static_assert(std::is_lvalue_reference_v<val_ref_type>);

			using val_type = std::remove_reference_t<val_ref_type>;
			static_assert(!std::is_const_v<val_type>);
			static_assert(!std::is_volatile_v<val_type>);
		});

	// check rvalue propagates correctly
	static_cast<node&&>(val).visit(
		[](auto&& v) noexcept
		{
			using val_ref_type = decltype(v);
			static_assert(std::is_rvalue_reference_v<val_ref_type>);

			using val_type = std::remove_reference_t<val_ref_type>;
			static_assert(!std::is_const_v<val_type>);
			static_assert(!std::is_volatile_v<val_type>);
		});

	// check const lvalue propagates correctly
	static_cast<const node&>(val).visit(
		[](auto&& v) noexcept
		{
			using val_ref_type = decltype(v);
			static_assert(std::is_lvalue_reference_v<val_ref_type>);

			using val_type = std::remove_reference_t<val_ref_type>;
			static_assert(std::is_const_v<val_type>);
			static_assert(!std::is_volatile_v<val_type>);
		});

	// check const rvalue propagates correctly
	static_cast<const node&&>(val).visit(
		[](auto&& v) noexcept
		{
			using val_ref_type = decltype(v);
			static_assert(std::is_rvalue_reference_v<val_ref_type>);

			using val_type = std::remove_reference_t<val_ref_type>;
			static_assert(std::is_const_v<val_type>);
			static_assert(!std::is_volatile_v<val_type>);
		});

	// check noexcept
	static constexpr auto throwing_visitor	   = [](auto&&) noexcept(false) {};
	static constexpr auto non_throwing_visitor = [](auto&&) noexcept(true) {};
	static_assert(!noexcept(static_cast<node&>(val).visit(throwing_visitor)));
	static_assert(!noexcept(static_cast<node&&>(val).visit(throwing_visitor)));
	static_assert(!noexcept(static_cast<const node&>(val).visit(throwing_visitor)));
	static_assert(!noexcept(static_cast<const node&&>(val).visit(throwing_visitor)));
	static_assert(noexcept(static_cast<node&>(val).visit(non_throwing_visitor)));
	static_assert(noexcept(static_cast<node&&>(val).visit(non_throwing_visitor)));
	static_assert(noexcept(static_cast<const node&>(val).visit(non_throwing_visitor)));
	static_assert(noexcept(static_cast<const node&&>(val).visit(non_throwing_visitor)));

	// check return
	static constexpr auto returns_boolean = [](auto& v) noexcept { return toml::is_integer<decltype(v)>; };
	auto return_test					  = static_cast<node&>(val).visit(returns_boolean);
	static_assert(std::is_same_v<decltype(return_test), bool>);
	CHECK(return_test == true);
}

TEST_CASE("node_view::visit")
{
	value<int64_t> val{ 3 };

	auto view  = node_view{ val };
	auto cview = node_view{ std::as_const(val) };
	static_assert(!std::is_same_v<decltype(view), decltype(cview)>);

	// check mutable views propagate correctly
	view.visit(
		[](auto&& v) noexcept
		{
			using val_ref_type = decltype(v);
			static_assert(std::is_lvalue_reference_v<val_ref_type>);

			using val_type = std::remove_reference_t<val_ref_type>;
			static_assert(!std::is_const_v<val_type>);
			static_assert(!std::is_volatile_v<val_type>);
		});

	// check const views propagate correctly
	cview.visit(
		[](auto&& v) noexcept
		{
			using val_ref_type = decltype(v);
			static_assert(std::is_lvalue_reference_v<val_ref_type>);

			using val_type = std::remove_reference_t<val_ref_type>;
			static_assert(std::is_const_v<val_type>);
			static_assert(!std::is_volatile_v<val_type>);
		});

	// check noexcept
	static constexpr auto throwing_visitor	   = [](auto&&) noexcept(false) {};
	static constexpr auto non_throwing_visitor = [](auto&&) noexcept(true) {};
	static_assert(!noexcept(view.visit(throwing_visitor)));
	static_assert(!noexcept(cview.visit(throwing_visitor)));
	static_assert(noexcept(view.visit(non_throwing_visitor)));
	static_assert(noexcept(cview.visit(non_throwing_visitor)));

	// check return
	static constexpr auto returns_boolean = [](auto&& v) noexcept { return toml::is_integer<decltype(v)>; };
	auto return_test					  = view.visit(returns_boolean);
	static_assert(std::is_same_v<decltype(return_test), bool>);
	CHECK(return_test == true);

	// check that null views don't invoke the visitor
	// clang-format off
	auto null_view	= decltype(view){};
	auto null_cview = decltype(cview){};
	unsigned count{};
	unsigned mask{};
	view.visit([&](auto&&)       noexcept { count++; mask |= 0b0001u; });
	cview.visit([&](auto&&)      noexcept { count++; mask |= 0b0010u; });
	null_view.visit([&](auto&&)  noexcept { count++; mask |= 0b0100u; });
	null_cview.visit([&](auto&&) noexcept { count++; mask |= 0b1000u; });
	CHECK(count == 2u);
	CHECK(mask == 0b0011u);
	// clang-format on
}
