// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"

TEST_CASE("arrays - moving")
{
	static constexpr auto filename = "foo.toml"sv;

	parsing_should_succeed(
		FILE_LINE_ARGS,
		R"(test = [ "foo" ])"sv,
		[&](table&& tbl)
		{
			// sanity-check initial state of a freshly-parsed array
			auto arr1 = tbl["test"].as<array>();
			REQUIRE(arr1);
			CHECK(arr1->size() == 1u);
			CHECK(arr1->source().begin == source_position{ 1, 8 });
			CHECK(arr1->source().end == source_position{ 1, 17 });
			CHECK(arr1->source().path);
			CHECK(*arr1->source().path == filename);
			REQUIRE(arr1->get_as<std::string>(0u));
			CHECK(*arr1->get_as<std::string>(0u) == "foo"sv);

			// sanity check the virtual type checks
			CHECK(arr1->type() == node_type::array);
			CHECK(!arr1->is_table());
			CHECK(arr1->is_array());
			CHECK(!arr1->is_array_of_tables());
			CHECK(!arr1->is_value());
			CHECK(!arr1->is_string());
			CHECK(!arr1->is_integer());
			CHECK(!arr1->is_floating_point());
			CHECK(!arr1->is_number());
			CHECK(!arr1->is_boolean());
			CHECK(!arr1->is_date());
			CHECK(!arr1->is_time());
			CHECK(!arr1->is_date_time());

			// sanity check the virtual type casts (non-const)
			CHECK(!arr1->as_table());
			CHECK(arr1->as_array() == arr1);
			CHECK(!arr1->as_string());
			CHECK(!arr1->as_integer());
			CHECK(!arr1->as_floating_point());
			CHECK(!arr1->as_boolean());
			CHECK(!arr1->as_date());
			CHECK(!arr1->as_time());
			CHECK(!arr1->as_date_time());

			// sanity check the virtual type casts (const)
			const auto carr1 = &std::as_const(*arr1);
			CHECK(!carr1->as_table());
			CHECK(carr1->as_array() == carr1);
			CHECK(!carr1->as_string());
			CHECK(!carr1->as_integer());
			CHECK(!carr1->as_floating_point());
			CHECK(!carr1->as_boolean());
			CHECK(!carr1->as_date());
			CHECK(!carr1->as_time());
			CHECK(!carr1->as_date_time());

			// sanity-check initial state of default-constructed array
			array arr2;
			CHECK(arr2.source().begin == source_position{});
			CHECK(arr2.source().end == source_position{});
			CHECK(!arr2.source().path);
			CHECK(arr2.size() == 0u);

			// check the results of move-assignment
			arr2 = std::move(*arr1);
			CHECK(arr2.source().begin == source_position{ 1, 8 });
			CHECK(arr2.source().end == source_position{ 1, 17 });
			CHECK(arr2.source().path);
			CHECK(*arr2.source().path == filename);
			CHECK(arr2.size() == 1u);
			REQUIRE(arr2.get_as<std::string>(0u));
			CHECK(*arr2.get_as<std::string>(0u) == "foo"sv);

			// check that moved-from array is now the same as default-constructed
			CHECK(arr1->source().begin == source_position{});
			CHECK(arr1->source().end == source_position{});
			CHECK(!arr1->source().path);
			CHECK(arr1->size() == 0u);

			// check the results of move-construction
			array arr3{ std::move(arr2) };
			CHECK(arr3.source().begin == source_position{ 1, 8 });
			CHECK(arr3.source().end == source_position{ 1, 17 });
			CHECK(arr3.source().path);
			CHECK(*arr3.source().path == filename);
			CHECK(arr3.size() == 1u);
			REQUIRE(arr3.get_as<std::string>(0u));
			CHECK(*arr3.get_as<std::string>(0u) == "foo"sv);

			// check that moved-from array is now the same as default-constructed
			CHECK(arr2.source().begin == source_position{});
			CHECK(arr2.source().end == source_position{});
			CHECK(!arr2.source().path);
			CHECK(arr2.size() == 0u);
		},
		filename);
}

TEST_CASE("arrays - copying")
{
	static constexpr auto filename = "foo.toml"sv;

	parsing_should_succeed(
		FILE_LINE_ARGS,
		R"(test = [ "foo" ])"sv,
		[&](table&& tbl)
		{
			// sanity-check initial state of a freshly-parsed array
			auto arr1 = tbl["test"].as<array>();
			REQUIRE(arr1);
			CHECK(arr1->size() == 1u);
			CHECK(arr1->source().begin == source_position{ 1, 8 });
			CHECK(arr1->source().end == source_position{ 1, 17 });
			CHECK(arr1->source().path);
			CHECK(*arr1->source().path == filename);
			REQUIRE(arr1->get_as<std::string>(0u));
			CHECK(*arr1->get_as<std::string>(0u) == "foo"sv);

			// sanity-check initial state of default-constructed array
			array arr2;
			CHECK(arr2.source().begin == source_position{});
			CHECK(arr2.source().end == source_position{});
			CHECK(!arr2.source().path);
			CHECK(arr2.size() == 0u);

			// check the results of copy-assignment
			arr2 = *arr1;
			CHECK(arr2.source().begin == source_position{});
			CHECK(arr2.source().end == source_position{});
			CHECK(!arr2.source().path);
			CHECK(arr2.size() == 1u);
			REQUIRE(arr2.get_as<std::string>(0u));
			CHECK(*arr2.get_as<std::string>(0u) == "foo"sv);
			CHECK(arr2 == *arr1);

			// check the results of copy-construction
			array arr3{ arr2 };
			CHECK(arr3.source().begin == source_position{});
			CHECK(arr3.source().end == source_position{});
			CHECK(!arr3.source().path);
			CHECK(arr3.size() == 1u);
			REQUIRE(arr3.get_as<std::string>(0u));
			CHECK(*arr3.get_as<std::string>(0u) == "foo"sv);
			CHECK(arr3 == *arr1);
			CHECK(arr3 == arr2);
		},
		filename);
}

TEST_CASE("arrays - construction")
{
	{
		array arr;
		CHECK(arr.size() == 0u);
		CHECK(arr.empty());
		CHECK(arr.begin() == arr.end());
		CHECK(arr.cbegin() == arr.cend());
		CHECK(arr.source().begin == source_position{});
		CHECK(arr.source().end == source_position{});
		CHECK(!arr.source().path);
		CHECK(!arr.is_homogeneous());
	}

	{
		array arr{ 42 };
		CHECK(arr.size() == 1u);
		CHECK(!arr.empty());
		CHECK(arr.begin() != arr.end());
		CHECK(arr.cbegin() != arr.cend());
		REQUIRE(arr.get_as<int64_t>(0u));
		CHECK(*arr.get_as<int64_t>(0u) == 42);
		CHECK(arr.get(0u) == &arr[0u]);
		CHECK(arr.is_homogeneous());
		CHECK(arr.is_homogeneous<int64_t>());
		CHECK(!arr.is_homogeneous<double>());
		CHECK(arr.get(0u) == &arr.at(0u));

		const array& carr = arr;
		CHECK(carr.size() == 1u);
		CHECK(!carr.empty());
		CHECK(carr.begin() != carr.end());
		CHECK(carr.cbegin() != carr.cend());
		REQUIRE(carr.get_as<int64_t>(0u));
		CHECK(*carr.get_as<int64_t>(0u) == 42);
		CHECK(carr.get(0u) == &carr[0u]);
		CHECK(carr.is_homogeneous());
		CHECK(carr.is_homogeneous<int64_t>());
		CHECK(!carr.is_homogeneous<double>());
		CHECK(carr.get(0u) == &carr.at(0u));
	}

	{
		array arr{ 42, "test"sv, 10.0f, array{}, value{ 3 } };
		CHECK(arr.size() == 5u);
		CHECK(!arr.empty());
		REQUIRE(arr.get_as<int64_t>(0u));
		CHECK(*arr.get_as<int64_t>(0u) == 42);
		CHECK(arr.get(0u) == &arr[0u]);
		REQUIRE(arr.get_as<std::string>(1u));
		CHECK(*arr.get_as<std::string>(1u) == "test"sv);
		CHECK(arr.get(1u) == &arr[1u]);
		REQUIRE(arr.get_as<double>(2u));
		CHECK(*arr.get_as<double>(2u) == 10.0);
		REQUIRE(arr.get_as<array>(3u));
		REQUIRE(arr.get_as<int64_t>(4u));
		CHECK(*arr.get_as<int64_t>(4u) == 3);
		CHECK(!arr.is_homogeneous());
		CHECK(arr.get(0u) == &arr.at(0u));
		CHECK(arr.get(1u) == &arr.at(1u));
	}

#if TOML_ENABLE_WINDOWS_COMPAT
	{
		array arr{ "mixed", "string"sv, L"test", L"kek"sv };
		CHECK(arr.size() == 4u);
		CHECK(arr.is_homogeneous());
		CHECK(arr.is_homogeneous<std::string>());
		CHECK(*arr.get_as<std::string>(0) == "mixed"sv);
		CHECK(*arr.get_as<std::string>(1) == "string"sv);
		CHECK(*arr.get_as<std::string>(2) == "test"sv);
		CHECK(*arr.get_as<std::string>(3) == "kek"sv);
	}
#endif // TOML_ENABLE_WINDOWS_COMPAT
}

TEST_CASE("arrays - equality")
{
	array arr1{ 1, 2, 3 };
	CHECK(arr1 == arr1);
	{
		auto ilist = { 1, 2, 3 };
		CHECK(arr1 == ilist);
		CHECK(ilist == arr1);

		ilist = { 2, 3, 4 };
		CHECK(arr1 != ilist);
		CHECK(ilist != arr1);

		auto ivec = std::vector{ 1, 2, 3 };
		CHECK(arr1 == ivec);
		CHECK(ivec == arr1);

		ivec = std::vector{ 2, 3, 4 };
		CHECK(arr1 != ivec);
		CHECK(ivec != arr1);
	}

	array arr2{ 1, 2, 3 };
	CHECK(arr1 == arr2);

	array arr3{ 1, 2 };
	CHECK(arr1 != arr3);

	array arr4{ 1, 2, 3, 4 };
	CHECK(arr1 != arr4);

	array arr5{ 1, 2, 3.0 };
	CHECK(arr1 != arr5);

	array arr6{};
	CHECK(arr1 != arr6);
	CHECK(arr6 == arr6);

	array arr7{};
	CHECK(arr6 == arr7);
}

TEST_CASE("arrays - insertion and erasure")
{
	array arr;

	// insert(const_iterator pos, ElemType&& val)
	auto it = arr.insert(arr.cbegin(), 42);
	CHECK(it == arr.begin());
	CHECK(arr.size() == 1u);
	CHECK(!arr.empty());
	REQUIRE(arr.get_as<int64_t>(0u));
	CHECK(*arr.get_as<int64_t>(0u) == 42);
	REQUIRE(arr == array{ 42 });

	// insert(const_iterator pos, size_t count, ElemType&& val)
	it = arr.insert(arr.cend(), 3, 10.0f);
	CHECK(it == arr.begin() + 1);
	CHECK(arr.size() == 4u);
	REQUIRE(arr.get_as<double>(1u));
	CHECK(*arr.get_as<double>(1u) == 10.0);
	REQUIRE(arr.get_as<double>(2u));
	CHECK(*arr.get_as<double>(2u) == 10.0);
	REQUIRE(arr.get_as<double>(3u));
	CHECK(*arr.get_as<double>(3u) == 10.0);
	REQUIRE(arr == array{ 42, 10.0, 10.0, 10.0 });

	// emplace(const_iterator pos, Args &&... args) noexcept
	it = arr.emplace<array>(arr.cbegin(), 1, 2, 3);
	CHECK(it == arr.begin());
	CHECK(arr.size() == 5u);
	REQUIRE(arr.get_as<array>(0u));
	CHECK(arr.get_as<array>(0u)->size() == 3u);
	REQUIRE(arr == array{ array{ 1, 2, 3 }, 42, 10.0, 10.0, 10.0 });

	// push_back(ElemType&& val) noexcept
	{
		arr.push_back("test"sv);
		auto& val = *arr.back().as_string();
		CHECK(arr.size() == 6u);
		REQUIRE(arr.get_as<std::string>(5u));
		CHECK(*arr.get_as<std::string>(5u) == "test"sv);
		CHECK(val == "test"sv);
		CHECK(&val == &arr.back());
		REQUIRE(arr == array{ array{ 1, 2, 3 }, 42, 10.0, 10.0, 10.0, "test"sv });
	}

	// decltype(auto) emplace_back(Args&&... args) noexcept
	{
		decltype(auto) val = arr.emplace_back<std::string>("test2"sv);
		CHECK(arr.size() == 7u);
		REQUIRE(arr.get_as<std::string>(6u));
		CHECK(*arr.get_as<std::string>(6u) == "test2"sv);
		CHECK(val == "test2"sv);
		CHECK(&val == &arr.back());
		REQUIRE(arr == array{ array{ 1, 2, 3 }, 42, 10.0, 10.0, 10.0, "test"sv, "test2"sv });
	}

	// erase(const_iterator pos) noexcept;
	it = arr.erase(arr.cbegin());
	REQUIRE(arr == array{ 42, 10.0, 10.0, 10.0, "test"sv, "test2"sv });
	CHECK(it == arr.begin());
	CHECK(arr.size() == 6u);

	// erase(const_iterator first, const_iterator last) noexcept;
	it = arr.erase(arr.cbegin() + 2, arr.cbegin() + 4);
	REQUIRE(arr == array{ 42, 10.0, "test"sv, "test2"sv });
	CHECK(it == arr.begin() + 2);
	CHECK(arr.size() == 4u);

	arr.pop_back();
	REQUIRE(arr == array{ 42, 10.0, "test"sv });
	CHECK(arr.size() == 3u);

	arr.clear();
	REQUIRE(arr == array{});
	CHECK(arr.size() == 0u);
	CHECK(arr.empty());

	// insert(const_iterator pos, Iter first, Iter last)
	{
		auto vals = std::vector{ 1.0, 2.0, 3.0 };
		arr.insert(arr.cbegin(), vals.begin(), vals.end());
		CHECK(arr.size() == 3u);
		REQUIRE(arr.get_as<double>(0u));
		CHECK(*arr.get_as<double>(0u) == 1.0);
		REQUIRE(arr.get_as<double>(1u));
		CHECK(*arr.get_as<double>(1u) == 2.0);
		REQUIRE(arr.get_as<double>(2u));
		CHECK(*arr.get_as<double>(2u) == 3.0);

		arr.insert(arr.cbegin() + 1, vals.begin(), vals.end());
		CHECK(arr.size() == 6u);
		REQUIRE(arr.get_as<double>(0u));
		CHECK(*arr.get_as<double>(0u) == 1.0);
		REQUIRE(arr.get_as<double>(1u));
		CHECK(*arr.get_as<double>(1u) == 1.0);
		REQUIRE(arr.get_as<double>(2u));
		CHECK(*arr.get_as<double>(2u) == 2.0);
		REQUIRE(arr.get_as<double>(3u));
		CHECK(*arr.get_as<double>(3u) == 3.0);
		REQUIRE(arr.get_as<double>(4u));
		CHECK(*arr.get_as<double>(4u) == 2.0);
		REQUIRE(arr.get_as<double>(5u));
		CHECK(*arr.get_as<double>(5u) == 3.0);
	}

	// insert(const_iterator pos, Iter first, Iter last) (with move iterators)
	{
		arr.clear();

		std::vector<std::string> vals{ "foo", "bar", "kek" };
		arr.insert(arr.cbegin(), std::make_move_iterator(vals.begin()), std::make_move_iterator(vals.end()));
		CHECK(arr.size() == 3u);
		REQUIRE(arr.get_as<std::string>(0));
		CHECK(*arr.get_as<std::string>(0) == "foo");
		REQUIRE(arr.get_as<std::string>(1));
		CHECK(*arr.get_as<std::string>(1) == "bar");
		REQUIRE(arr.get_as<std::string>(2));
		CHECK(*arr.get_as<std::string>(2) == "kek");

		REQUIRE(vals.size() == 3u);
		CHECK(vals[0] == "");
		CHECK(vals[1] == "");
		CHECK(vals[2] == "");
	}

	// iterator insert(const_iterator pos, std::initializer_list<ElemType> ilist) noexcept
	{
		arr.clear();

		arr.insert(arr.cbegin(), { 1.0, 2.0, 3.0 });
		CHECK(arr.size() == 3u);
		REQUIRE(arr.get_as<double>(0u));
		CHECK(*arr.get_as<double>(0u) == 1.0);
		REQUIRE(arr.get_as<double>(1u));
		CHECK(*arr.get_as<double>(1u) == 2.0);
		REQUIRE(arr.get_as<double>(2u));
		CHECK(*arr.get_as<double>(2u) == 3.0);

		arr.insert(arr.cbegin() + 1, { 1.0, 2.0, 3.0 });
		CHECK(arr.size() == 6u);
		REQUIRE(arr.get_as<double>(0u));
		CHECK(*arr.get_as<double>(0u) == 1.0);
		REQUIRE(arr.get_as<double>(1u));
		CHECK(*arr.get_as<double>(1u) == 1.0);
		REQUIRE(arr.get_as<double>(2u));
		CHECK(*arr.get_as<double>(2u) == 2.0);
		REQUIRE(arr.get_as<double>(3u));
		CHECK(*arr.get_as<double>(3u) == 3.0);
		REQUIRE(arr.get_as<double>(4u));
		CHECK(*arr.get_as<double>(4u) == 2.0);
		REQUIRE(arr.get_as<double>(5u));
		CHECK(*arr.get_as<double>(5u) == 3.0);
	}

	// iterator replace(const_iterator pos, ElemType&& elem) noexcept
	{
		arr.clear();
		arr.insert(arr.begin(), { 1, 2, 3 });
		CHECK(arr == array{ 1, 2, 3 });
		arr.replace(arr.begin() + 1u, "two"sv);
		CHECK(arr == array{ 1, "two"sv, 3 });
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	arr.clear();
	it = arr.insert(arr.cbegin(), L"test");
	CHECK(*arr.get_as<std::string>(0u) == "test"sv);

	it = arr.emplace<std::string>(arr.cbegin(), L"test2"sv);
	CHECK(*arr.get_as<std::string>(0u) == "test2"sv);

	arr.push_back(L"test3"s);
	CHECK(*arr.back().as_string() == "test3"sv);

	arr.emplace_back<std::string>(L"test4");
	CHECK(*arr.back().as_string() == "test4"sv);

#endif // TOML_ENABLE_WINDOWS_COMPAT

	// push_back with value_flags
	{
		arr.clear();

		auto hex = toml::value{ 1 };
		hex.flags(value_flags::format_as_hexadecimal);
		CHECK(hex.flags() == value_flags::format_as_hexadecimal);

		arr.push_back(hex);
		CHECK(hex.flags() == value_flags::format_as_hexadecimal);
		CHECK(arr.back().as_integer()->flags() == value_flags::format_as_hexadecimal);

		arr.push_back(std::move(hex));
		CHECK(hex.flags() == value_flags{});
		CHECK(arr.back().as_integer()->flags() == value_flags::format_as_hexadecimal);
	}
}

TEST_CASE("arrays - flattening")
{
	{
		array arr{
			1,
			2,
			3,
			array{ 4, 5 },
			6,
			array{},
			array{ 7,
				   array{
					   8,
					   array{ 9 },
					   10,
					   array{},
				   },
				   11 },
		};
		arr.flatten();
		CHECK(arr == array{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 });
	}

	{
		array arr{ array{},
				   array{ inserter{ array{} } },
				   array{ array{}, array{ array{}, array{} }, array{} },
				   array{ array{ array{ array{ array{ array{ 1 } } } } } } };
		arr.flatten();
		CHECK(arr == array{ 1 });
	}
}

TEST_CASE("arrays - pruning")
{
	// [ 1, [ 2, [], 3 ], { 4 = 5, 6 = 7 }, [], 8, [{}], 9, 10 ]
	const auto arr =
		array{ 1, array{ 2, array{}, 3 }, table{ { "4", 5 }, { "6", array{} } }, array{}, 8, array{ table{} }, 9, 10 };

	// [ 1, [ 2, 3 ], { 4 = 5, 6 = 7 }, 8, 9, 10 ]
	const auto pruned_recursive = array{ 1, array{ 2, 3 }, table{ { "4", 5 } }, 8, 9, 10 };
	CHECK(array{ arr }.prune(true) == pruned_recursive);

	// [ 1, [ 2, [], 3 ], { 4 = 5, 6 = 7 }, [], 8, [{}], 9, 10 ]
	const auto pruned_flat =
		array{ 1, array{ 2, array{}, 3 }, table{ { "4", 5 }, { "6", array{} } }, 8, array{ table{} }, 9, 10 };
	CHECK(array{ arr }.prune(false) == pruned_flat);
}

TEST_CASE("arrays - resizing and truncation")
{
	array arr{ 1, 2, 3, 4, 5 };
	CHECK(arr.size() == 5u);

	// truncate with no change
	arr.truncate(5u);
	CHECK(arr.size() == 5u);
	CHECK(arr == array{ 1, 2, 3, 4, 5 });

	// truncate down to three elements
	arr.truncate(3u);
	CHECK(arr.size() == 3u);
	CHECK(arr == array{ 1, 2, 3 });

	// resize down to two elements
	arr.resize(2u, 42);
	CHECK(arr.size() == 2u);
	CHECK(arr == array{ 1, 2 });

	// resize with no change
	arr.resize(2u, 42);
	CHECK(arr.size() == 2u);
	CHECK(arr == array{ 1, 2 });

	// resize up to six elements
	arr.resize(6u, 42);
	CHECK(arr.size() == 6u);
	CHECK(arr == array{ 1, 2, 42, 42, 42, 42 });
}

TEST_CASE("arrays - for_each")
{
	const auto arr = array{ 1, 2.0, 3, "four", false };

	SECTION("type checking")
	{
		int count	= 0;
		int ints	= 0;
		int floats	= 0;
		int numbers = 0;
		int strings = 0;
		int bools	= 0;
		arr.for_each(
			[&](const auto& v) noexcept
			{
				count++;
				if constexpr (toml::is_integer<decltype(v)>)
					ints++;
				if constexpr (toml::is_floating_point<decltype(v)>)
					floats++;
				if constexpr (toml::is_number<decltype(v)>)
					numbers++;
				if constexpr (toml::is_string<decltype(v)>)
					strings++;
				if constexpr (toml::is_boolean<decltype(v)>)
					bools++;
			});
		CHECK(count == 5);
		CHECK(ints == 2);
		CHECK(floats == 1);
		CHECK(numbers == (ints + floats));
		CHECK(strings == 1);
		CHECK(bools == 1);
	}

#if !TOML_RETURN_BOOL_FROM_FOR_EACH_BROKEN

	SECTION("early-exit (elem, index)")
	{
		int count = 0;
		arr.for_each(
			[&](const auto& elem, size_t /*idx*/) noexcept -> bool
			{
				count++;
				return !toml::is_string<decltype(elem)>;
			});
		CHECK(count == 4);
	}

	SECTION("early-exit (elem)")
	{
		int count = 0;
		arr.for_each(
			[&](const auto& elem) noexcept -> bool
			{
				count++;
				return !toml::is_string<decltype(elem)>;
			});
		CHECK(count == 4);
	}

	SECTION("early-exit (index, elem)")
	{
		int count = 0;
		arr.for_each(
			[&](size_t /*idx*/, const auto& elem) noexcept -> bool
			{
				count++;
				return !toml::is_string<decltype(elem)>;
			});
		CHECK(count == 4);
	}

#endif
}
