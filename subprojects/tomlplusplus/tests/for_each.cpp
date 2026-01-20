// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"
TOML_DISABLE_SPAM_WARNINGS;

TEST_CASE("array::for_each")
{
	toml::array arr{ 0, 1, 2, 3.0, "four", "five", 6 };

	// check lvalue propagates correctly
	static_cast<array&>(arr).for_each(
		[](auto&& elem, size_t) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<array&>(arr).for_each(
		[](size_t, auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<array&>(arr).for_each(
		[](auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});

	// check rvalue propagates correctly
	static_cast<array&&>(arr).for_each(
		[](auto&& elem, size_t) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<array&&>(arr).for_each(
		[](size_t, auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<array&&>(arr).for_each(
		[](auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});

	// check const lvalue propagates correctly
	static_cast<const array&>(arr).for_each(
		[](auto&& elem, size_t) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<const array&>(arr).for_each(
		[](size_t, auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<const array&>(arr).for_each(
		[](auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});

	// check const rvalue propagates correctly
	static_cast<const array&&>(arr).for_each(
		[](auto&& elem, size_t) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<const array&&>(arr).for_each(
		[](size_t, auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<const array&&>(arr).for_each(
		[](auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});

	// check noexcept - func(elem, i)
	{
		static constexpr auto throwing_visitor	   = [](auto&&, size_t) noexcept(false) {};
		static constexpr auto non_throwing_visitor = [](auto&&, size_t) noexcept(true) {};
		static_assert(!noexcept(static_cast<array&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<array&&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const array&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const array&&>(arr).for_each(throwing_visitor)));
		static_assert(noexcept(static_cast<array&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<array&&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const array&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const array&&>(arr).for_each(non_throwing_visitor)));
	}

	// check noexcept - func(i, elem)
	{
		static constexpr auto throwing_visitor	   = [](size_t, auto&&) noexcept(false) {};
		static constexpr auto non_throwing_visitor = [](size_t, auto&&) noexcept(true) {};
		static_assert(!noexcept(static_cast<array&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<array&&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const array&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const array&&>(arr).for_each(throwing_visitor)));
		static_assert(noexcept(static_cast<array&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<array&&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const array&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const array&&>(arr).for_each(non_throwing_visitor)));
	}

	// check noexcept - func(elem)
	{
		static constexpr auto throwing_visitor	   = [](auto&&) noexcept(false) {};
		static constexpr auto non_throwing_visitor = [](auto&&) noexcept(true) {};
		static_assert(!noexcept(static_cast<array&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<array&&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const array&>(arr).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const array&&>(arr).for_each(throwing_visitor)));
		static_assert(noexcept(static_cast<array&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<array&&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const array&>(arr).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const array&&>(arr).for_each(non_throwing_visitor)));
	}

	// check that the iteration actually does what it says on the box
	{
		toml::array arr2;
		arr.for_each([&](const auto& val) { arr2.push_back(val); });
		CHECK(arr == arr2);
	}

	// check that visitation works for a specific type
	{
		toml::array arr2;
		arr.for_each([&](const toml::value<int64_t>& val) { arr2.push_back(val); });
		CHECK(arr2 == toml::array{ 0, 1, 2, 6 });
	}

#if !TOML_RETURN_BOOL_FROM_FOR_EACH_BROKEN

	// check that early-stopping works
	{
		toml::array arr2;
		arr.for_each(
			[&](const auto& val)
			{
				if constexpr (!toml::is_number<decltype(val)>)
					return false;
				else
				{
					arr2.push_back(val);
					return true;
				}
			});
		CHECK(arr2 == toml::array{ 0, 1, 2, 3.0 });
	}

#endif
}

TEST_CASE("table::for_each")
{
	table tbl{ { "zero", 0 },	   //
			   { "one", 1 },	   //
			   { "two", 2 },	   //
			   { "three", 3.0 },   //
			   { "four", "four" }, //
			   { "five", "five" }, //
			   { "six", 6 } };

	// check lvalue propagates correctly
	static_cast<table&>(tbl).for_each(
		[](const toml::key&, auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<table&>(tbl).for_each(
		[](auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});

	// check rvalue propagates correctly
	static_cast<table&&>(tbl).for_each(
		[](const toml::key&, auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<table&&>(tbl).for_each(
		[](auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(!std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});

	// check const lvalue propagates correctly
	static_cast<const table&>(tbl).for_each(
		[](const toml::key&, auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<const table&>(tbl).for_each(
		[](auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_lvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});

	// check const rvalue propagates correctly
	static_cast<const table&&>(tbl).for_each(
		[](const toml::key&, auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});
	static_cast<const table&&>(tbl).for_each(
		[](auto&& elem) noexcept
		{
			using elem_ref_type = decltype(elem);
			static_assert(std::is_rvalue_reference_v<elem_ref_type>);

			using elem_type = std::remove_reference_t<elem_ref_type>;
			static_assert(std::is_const_v<elem_type>);
			static_assert(!std::is_volatile_v<elem_type>);
		});

	// check noexcept - func(key, value)
	{
		static constexpr auto throwing_visitor	   = [](const toml::key&, auto&&) noexcept(false) {};
		static constexpr auto non_throwing_visitor = [](const toml::key&, auto&&) noexcept(true) {};
		static_assert(!noexcept(static_cast<table&>(tbl).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<table&&>(tbl).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const table&>(tbl).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const table&&>(tbl).for_each(throwing_visitor)));
		static_assert(noexcept(static_cast<table&>(tbl).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<table&&>(tbl).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const table&>(tbl).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const table&&>(tbl).for_each(non_throwing_visitor)));
	}

	// check noexcept - func(value)
	{
		static constexpr auto throwing_visitor	   = [](auto&&) noexcept(false) {};
		static constexpr auto non_throwing_visitor = [](auto&&) noexcept(true) {};
		static_assert(!noexcept(static_cast<table&>(tbl).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<table&&>(tbl).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const table&>(tbl).for_each(throwing_visitor)));
		static_assert(!noexcept(static_cast<const table&&>(tbl).for_each(throwing_visitor)));
		static_assert(noexcept(static_cast<table&>(tbl).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<table&&>(tbl).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const table&>(tbl).for_each(non_throwing_visitor)));
		static_assert(noexcept(static_cast<const table&&>(tbl).for_each(non_throwing_visitor)));
	}

	// check that the iteration actually does what it says on the box
	{
		toml::table tbl2;
		tbl.for_each([&](auto&& key, auto&& val) { tbl2.insert_or_assign(key, val); });
		CHECK(tbl == tbl2);
	}

	// check that visitation works for a specific type
	{
		toml::table tbl2;
		tbl.for_each([&](auto&& key, const toml::value<int64_t>& val) { tbl2.insert_or_assign(key, val); });
		CHECK(tbl2
			  == table{ { "zero", 0 }, //
						{ "one", 1 },  //
						{ "two", 2 },  //
						{ "six", 6 } });
	}

#if !TOML_RETURN_BOOL_FROM_FOR_EACH_BROKEN

	// check that early-stopping works
	{
		toml::table tbl2;
		size_t added{};
		tbl.for_each(
			[&](auto&& key, const auto& val)
			{
				tbl2.insert_or_assign(key, val);
				added++;
				return added < 3u;
			});
		CHECK(tbl2.size() == 3u);
	}

#endif
}
