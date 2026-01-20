// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"

#ifdef _WIN32
TOML_DISABLE_WARNINGS;
#include <Windows.h>
TOML_ENABLE_WARNINGS;
#endif

template <typename T>
static constexpr T one = static_cast<T>(1);

TEST_CASE("values - construction")
{
	static constexpr auto check_value = [](const auto& init_value, auto expected_native_type_tag)
	{
		using init_type			   = impl::remove_cvref<decltype(init_value)>;
		using native_type		   = impl::native_type_of<init_type>;
		using expected_native_type = typename decltype(expected_native_type_tag)::type;
		static_assert(std::is_same_v<native_type, expected_native_type>);

		auto v			 = value{ init_value };
		using value_type = decltype(v);
		static_assert(std::is_same_v<value_type, value<native_type>>);

		if constexpr (std::is_same_v<std::string, native_type>)
		{
#if TOML_HAS_CHAR8
			using char8_type = char8_t;
#else
			using char8_type = char;
#endif

			using init_char_type = impl::remove_cvref<decltype(init_value[0])>;
			using init_view_type = std::basic_string_view<init_char_type>;
			static_assert(impl::is_one_of<init_char_type, char, wchar_t, char8_type>);

			const auto init_view = init_view_type{ init_value };
			if constexpr (impl::is_one_of<init_char_type, char, char8_type>)
			{
				const auto coerced_view =
					std::string_view{ reinterpret_cast<const char*>(init_view.data()), init_view.length() };

				CHECK(v == coerced_view);
				CHECK(coerced_view == v);
			}
#if TOML_ENABLE_WINDOWS_COMPAT
			else if constexpr (impl::is_one_of<init_char_type, wchar_t>)
			{
				const auto narrowed_string = impl::narrow(init_view);

				CHECK(v == narrowed_string);
				CHECK(narrowed_string == v);
			}
#endif
			else
			{
				static_assert(impl::always_false<init_char_type>, "evaluated unreachable branch");
			}
		}
		else if constexpr (impl::is_one_of<native_type, int64_t, double, bool>)
		{
			CHECK(v == static_cast<native_type>(init_value));
			CHECK(static_cast<native_type>(init_value) == v);
		}
		else // dates + times
		{
			CHECK(v == init_value);
			CHECK(init_value == v);
		}

		static constexpr auto expected_node_type = impl::node_type_of<native_type>;

		CHECK(v.is_homogeneous());
		CHECK(v.template is_homogeneous<native_type>());
		CHECK(v.is_homogeneous(expected_node_type));

		// sanity check the virtual type checks
		CHECK(v.type() == expected_node_type);
		CHECK(!v.is_table());
		CHECK(!v.is_array());
		CHECK(!v.is_array_of_tables());
		CHECK(v.is_value());
		CHECK(v.is_string() == (expected_node_type == node_type::string));
		CHECK(v.is_integer() == (expected_node_type == node_type::integer));
		CHECK(v.is_floating_point() == (expected_node_type == node_type::floating_point));
		CHECK(v.is_number()
			  == (expected_node_type == node_type::integer || expected_node_type == node_type::floating_point));
		CHECK(v.is_boolean() == (expected_node_type == node_type::boolean));
		CHECK(v.is_date() == (expected_node_type == node_type::date));
		CHECK(v.is_time() == (expected_node_type == node_type::time));
		CHECK(v.is_date_time() == (expected_node_type == node_type::date_time));

		// sanity check the virtual type casts (non-const)
		CHECK(!v.as_table());
		CHECK(!v.as_array());
		if constexpr (expected_node_type == node_type::string)
			CHECK(v.as_string() == &v);
		else
			CHECK(!v.as_string());
		if constexpr (expected_node_type == node_type::integer)
			CHECK(v.as_integer() == &v);
		else
			CHECK(!v.as_integer());
		if constexpr (expected_node_type == node_type::floating_point)
			CHECK(v.as_floating_point() == &v);
		else
			CHECK(!v.as_floating_point());
		if constexpr (expected_node_type == node_type::boolean)
			CHECK(v.as_boolean() == &v);
		else
			CHECK(!v.as_boolean());
		if constexpr (expected_node_type == node_type::date)
			CHECK(v.as_date() == &v);
		else
			CHECK(!v.as_date());
		if constexpr (expected_node_type == node_type::time)
			CHECK(v.as_time() == &v);
		else
			CHECK(!v.as_time());
		if constexpr (expected_node_type == node_type::date_time)
			CHECK(v.as_date_time() == &v);
		else
			CHECK(!v.as_date_time());

		// sanity check the virtual type casts (const)
		const auto& cv = std::as_const(v);
		CHECK(!cv.as_table());
		CHECK(!cv.as_array());
		if constexpr (expected_node_type == node_type::string)
			CHECK(cv.as_string() == &v);
		else
			CHECK(!cv.as_string());
		if constexpr (expected_node_type == node_type::integer)
			CHECK(cv.as_integer() == &v);
		else
			CHECK(!cv.as_integer());
		if constexpr (expected_node_type == node_type::floating_point)
			CHECK(cv.as_floating_point() == &v);
		else
			CHECK(!cv.as_floating_point());
		if constexpr (expected_node_type == node_type::boolean)
			CHECK(cv.as_boolean() == &v);
		else
			CHECK(!cv.as_boolean());
		if constexpr (expected_node_type == node_type::date)
			CHECK(cv.as_date() == &v);
		else
			CHECK(!cv.as_date());
		if constexpr (expected_node_type == node_type::time)
			CHECK(cv.as_time() == &v);
		else
			CHECK(!cv.as_time());
		if constexpr (expected_node_type == node_type::date_time)
			CHECK(cv.as_date_time() == &v);
		else
			CHECK(!cv.as_date_time());
	};

	check_value(one<signed char>, type_tag<int64_t>{});
	check_value(one<signed short>, type_tag<int64_t>{});
	check_value(one<signed int>, type_tag<int64_t>{});
	check_value(one<signed long>, type_tag<int64_t>{});
	check_value(one<signed long long>, type_tag<int64_t>{});
	check_value(one<unsigned char>, type_tag<int64_t>{});
	check_value(one<unsigned short>, type_tag<int64_t>{});
	check_value(one<unsigned int>, type_tag<int64_t>{});
	check_value(one<unsigned long>, type_tag<int64_t>{});
	check_value(one<unsigned long long>, type_tag<int64_t>{});
	check_value(true, type_tag<bool>{});
	check_value(false, type_tag<bool>{});
	check_value("kek", type_tag<std::string>{});
	check_value("kek"s, type_tag<std::string>{});
	check_value("kek"sv, type_tag<std::string>{});
	check_value("kek"sv.data(), type_tag<std::string>{});
#if TOML_HAS_CHAR8
	check_value(u8"kek", type_tag<std::string>{});
	check_value(u8"kek"s, type_tag<std::string>{});
	check_value(u8"kek"sv, type_tag<std::string>{});
	check_value(u8"kek"sv.data(), type_tag<std::string>{});
#endif

#ifdef _WIN32
	check_value(one<BOOL>, type_tag<int64_t>{});
	check_value(one<SHORT>, type_tag<int64_t>{});
	check_value(one<INT>, type_tag<int64_t>{});
	check_value(one<LONG>, type_tag<int64_t>{});
	check_value(one<INT_PTR>, type_tag<int64_t>{});
	check_value(one<LONG_PTR>, type_tag<int64_t>{});
	check_value(one<USHORT>, type_tag<int64_t>{});
	check_value(one<UINT>, type_tag<int64_t>{});
	check_value(one<ULONG>, type_tag<int64_t>{});
	check_value(one<UINT_PTR>, type_tag<int64_t>{});
	check_value(one<ULONG_PTR>, type_tag<int64_t>{});
	check_value(one<WORD>, type_tag<int64_t>{});
	check_value(one<DWORD>, type_tag<int64_t>{});
	check_value(one<DWORD32>, type_tag<int64_t>{});
	check_value(one<DWORD64>, type_tag<int64_t>{});
	check_value(one<DWORDLONG>, type_tag<int64_t>{});

#if TOML_ENABLE_WINDOWS_COMPAT

	check_value(L"kek", type_tag<std::string>{});
	check_value(L"kek"s, type_tag<std::string>{});
	check_value(L"kek"sv, type_tag<std::string>{});
	check_value(L"kek"sv.data(), type_tag<std::string>{});

#endif // TOML_ENABLE_WINDOWS_COMPAT

#endif
}

TEST_CASE("values - toml_formatter")
{
	static constexpr auto print_value = [](auto&& raw)
	{
		auto val = toml::value{ std::forward<decltype(raw)>(raw) };
		std::stringstream ss;
		ss << val;
		return ss.str();
	};

	CHECK(print_value(1) == "1");
	CHECK(print_value(1.0f) == "1.0");
	CHECK(print_value(1.0) == "1.0");

	CHECK(print_value(1.5f) == "1.5");
	CHECK(print_value(1.5) == "1.5");

	CHECK(print_value(10) == "10");
	CHECK(print_value(10.0f) == "10.0");
	CHECK(print_value(10.0) == "10.0");

	CHECK(print_value(100) == "100");
	CHECK(print_value(100.0f) == "100.0");
	CHECK(print_value(100.0) == "100.0");

	CHECK(print_value(1000) == "1000");
	CHECK(print_value(1000.0f) == "1000.0");
	CHECK(print_value(1000.0) == "1000.0");

	CHECK(print_value(10000) == "10000");
	CHECK(print_value(10000.0f) == "10000.0");
	CHECK(print_value(10000.0) == "10000.0");

	CHECK(print_value(std::numeric_limits<double>::infinity()) == "inf");
	CHECK(print_value(-std::numeric_limits<double>::infinity()) == "-inf");
	CHECK(print_value(std::numeric_limits<double>::quiet_NaN()) == "nan");

	// only integers for large values;
	// large floats might get output as scientific notation and that's fine
	CHECK(print_value(10000000000) == "10000000000");
	CHECK(print_value(100000000000000) == "100000000000000");
}

TEST_CASE("nodes - value() int/float/bool conversions")
{
#define CHECK_VALUE_PASS(type, v) CHECK(n.value<type>() == static_cast<type>(v))
#define CHECK_VALUE_FAIL(type)	  CHECK(!n.value<type>())

	// bools
	{
		value val{ false };
		const node& n = val;
		CHECK_VALUE_PASS(bool, false);
		CHECK_VALUE_PASS(int8_t, 0);
		CHECK_VALUE_PASS(uint8_t, 0);
		CHECK_VALUE_PASS(int16_t, 0);
		CHECK_VALUE_PASS(uint16_t, 0);
		CHECK_VALUE_PASS(int32_t, 0);
		CHECK_VALUE_PASS(uint32_t, 0);
		CHECK_VALUE_PASS(int64_t, 0);
		CHECK_VALUE_PASS(uint64_t, 0);
		CHECK_VALUE_FAIL(float);
		CHECK_VALUE_FAIL(double);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = true;
		CHECK_VALUE_PASS(bool, true);
		CHECK_VALUE_PASS(int8_t, 1);
		CHECK_VALUE_PASS(uint8_t, 1);
		CHECK_VALUE_PASS(int16_t, 1);
		CHECK_VALUE_PASS(uint16_t, 1);
		CHECK_VALUE_PASS(int32_t, 1);
		CHECK_VALUE_PASS(uint32_t, 1);
		CHECK_VALUE_PASS(int64_t, 1);
		CHECK_VALUE_PASS(uint64_t, 1);
		CHECK_VALUE_FAIL(float);
		CHECK_VALUE_FAIL(double);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);
	}

	// ints
	{
		value val{ 0 };
		const node& n = val;
		CHECK_VALUE_PASS(bool, false); // int -> bool coercion
		CHECK_VALUE_PASS(int8_t, 0);
		CHECK_VALUE_PASS(uint8_t, 0);
		CHECK_VALUE_PASS(int16_t, 0);
		CHECK_VALUE_PASS(uint16_t, 0);
		CHECK_VALUE_PASS(int32_t, 0);
		CHECK_VALUE_PASS(uint32_t, 0);
		CHECK_VALUE_PASS(int64_t, 0);
		CHECK_VALUE_PASS(uint64_t, 0);
		CHECK_VALUE_PASS(float, 0);
		CHECK_VALUE_PASS(double, 0);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = 100;
		CHECK_VALUE_PASS(bool, true); // int -> bool coercion
		CHECK_VALUE_PASS(int8_t, 100);
		CHECK_VALUE_PASS(uint8_t, 100);
		CHECK_VALUE_PASS(int16_t, 100);
		CHECK_VALUE_PASS(uint16_t, 100);
		CHECK_VALUE_PASS(int32_t, 100);
		CHECK_VALUE_PASS(uint32_t, 100);
		CHECK_VALUE_PASS(int64_t, 100);
		CHECK_VALUE_PASS(uint64_t, 100);
		CHECK_VALUE_PASS(float, 100);
		CHECK_VALUE_PASS(double, 100);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = -100;
		CHECK_VALUE_PASS(bool, true); // int -> bool coercion
		CHECK_VALUE_PASS(int8_t, -100);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_PASS(int16_t, -100);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_PASS(int32_t, -100);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_PASS(int64_t, -100);
		CHECK_VALUE_FAIL(uint64_t);
		CHECK_VALUE_PASS(float, -100);
		CHECK_VALUE_PASS(double, -100);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = 1000;
		CHECK_VALUE_PASS(bool, true); // int -> bool coercion
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_PASS(int16_t, 1000);
		CHECK_VALUE_PASS(uint16_t, 1000);
		CHECK_VALUE_PASS(int32_t, 1000);
		CHECK_VALUE_PASS(uint32_t, 1000);
		CHECK_VALUE_PASS(int64_t, 1000);
		CHECK_VALUE_PASS(uint64_t, 1000);
		CHECK_VALUE_PASS(float, 1000);
		CHECK_VALUE_PASS(double, 1000);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = -1000;
		CHECK_VALUE_PASS(bool, true); // int -> bool coercion
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_PASS(int16_t, -1000);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_PASS(int32_t, -1000);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_PASS(int64_t, -1000);
		CHECK_VALUE_FAIL(uint64_t);
		CHECK_VALUE_PASS(float, -1000);
		CHECK_VALUE_PASS(double, -1000);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = (std::numeric_limits<int64_t>::max)();
		CHECK_VALUE_PASS(bool, true); // int -> bool coercion
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_FAIL(int16_t);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_FAIL(int32_t);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_PASS(int64_t, (std::numeric_limits<int64_t>::max)());
		CHECK_VALUE_PASS(uint64_t, (std::numeric_limits<int64_t>::max)());
		CHECK_VALUE_FAIL(float);
		CHECK_VALUE_FAIL(double);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = (std::numeric_limits<int64_t>::min)();
		CHECK_VALUE_PASS(bool, true); // int -> bool coercion
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_FAIL(int16_t);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_FAIL(int32_t);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_PASS(int64_t, (std::numeric_limits<int64_t>::min)());
		CHECK_VALUE_FAIL(uint64_t);
		CHECK_VALUE_FAIL(float);
		CHECK_VALUE_FAIL(double);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);
	}

	// floats
	{
		value val{ 0.0 };
		const node& n = val;
		CHECK_VALUE_FAIL(bool);
		CHECK_VALUE_PASS(int8_t, 0);
		CHECK_VALUE_PASS(uint8_t, 0);
		CHECK_VALUE_PASS(int16_t, 0);
		CHECK_VALUE_PASS(uint16_t, 0);
		CHECK_VALUE_PASS(int32_t, 0);
		CHECK_VALUE_PASS(uint32_t, 0);
		CHECK_VALUE_PASS(int64_t, 0);
		CHECK_VALUE_PASS(uint64_t, 0);
		CHECK_VALUE_PASS(float, 0);
		CHECK_VALUE_PASS(double, 0);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = 1.0;
		CHECK_VALUE_FAIL(bool);
		CHECK_VALUE_PASS(int8_t, 1);
		CHECK_VALUE_PASS(uint8_t, 1);
		CHECK_VALUE_PASS(int16_t, 1);
		CHECK_VALUE_PASS(uint16_t, 1);
		CHECK_VALUE_PASS(int32_t, 1);
		CHECK_VALUE_PASS(uint32_t, 1);
		CHECK_VALUE_PASS(int64_t, 1);
		CHECK_VALUE_PASS(uint64_t, 1);
		CHECK_VALUE_PASS(float, 1);
		CHECK_VALUE_PASS(double, 1);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = -1.0;
		CHECK_VALUE_FAIL(bool);
		CHECK_VALUE_PASS(int8_t, -1);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_PASS(int16_t, -1);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_PASS(int32_t, -1);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_PASS(int64_t, -1);
		CHECK_VALUE_FAIL(uint64_t);
		CHECK_VALUE_PASS(float, -1);
		CHECK_VALUE_PASS(double, -1);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = 1.5;
		CHECK_VALUE_FAIL(bool);
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_FAIL(int16_t);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_FAIL(int32_t);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_FAIL(int64_t);
		CHECK_VALUE_FAIL(uint64_t);
		CHECK_VALUE_PASS(float, 1.5);
		CHECK_VALUE_PASS(double, 1.5);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = -1.5;
		CHECK_VALUE_FAIL(bool);
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_FAIL(int16_t);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_FAIL(int32_t);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_FAIL(int64_t);
		CHECK_VALUE_FAIL(uint64_t);
		CHECK_VALUE_PASS(float, -1.5);
		CHECK_VALUE_PASS(double, -1.5);
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = std::numeric_limits<double>::infinity();
		CHECK_VALUE_FAIL(bool);
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_FAIL(int16_t);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_FAIL(int32_t);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_FAIL(int64_t);
		CHECK_VALUE_FAIL(uint64_t);
		CHECK_VALUE_PASS(float, std::numeric_limits<float>::infinity());
		CHECK_VALUE_PASS(double, std::numeric_limits<double>::infinity());
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = -std::numeric_limits<double>::infinity();
		CHECK_VALUE_FAIL(bool);
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_FAIL(int16_t);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_FAIL(int32_t);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_FAIL(int64_t);
		CHECK_VALUE_FAIL(uint64_t);
		CHECK_VALUE_PASS(float, -std::numeric_limits<float>::infinity());
		CHECK_VALUE_PASS(double, -std::numeric_limits<double>::infinity());
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);

		*val = std::numeric_limits<double>::quiet_NaN();
		CHECK_VALUE_FAIL(bool);
		CHECK_VALUE_FAIL(int8_t);
		CHECK_VALUE_FAIL(uint8_t);
		CHECK_VALUE_FAIL(int16_t);
		CHECK_VALUE_FAIL(uint16_t);
		CHECK_VALUE_FAIL(int32_t);
		CHECK_VALUE_FAIL(uint32_t);
		CHECK_VALUE_FAIL(int64_t);
		CHECK_VALUE_FAIL(uint64_t);
		{
			auto fval = n.value<float>();
			REQUIRE(fval.has_value());
			CHECK(impl::fpclassify(*fval) == impl::fp_class::nan);
		}
		{
			auto fval = n.value<double>();
			REQUIRE(fval.has_value());
			CHECK(impl::fpclassify(*fval) == impl::fp_class::nan);
		}
		CHECK_VALUE_FAIL(std::string);
		CHECK_VALUE_FAIL(std::string_view);
		CHECK_VALUE_FAIL(toml::date);
		CHECK_VALUE_FAIL(toml::time);
		CHECK_VALUE_FAIL(toml::date_time);
	}
}
