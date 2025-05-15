// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"

// this file is about testing user misc. repros submitted via github issues, et cetera.

TEST_CASE("user feedback")
{
	SECTION("tomlplusplus/issues/49") // https://github.com/marzer/tomlplusplus/issues/49#issuecomment-664428571
	{
		toml::table t1;
		t1.insert_or_assign("bar1", toml::array{ 1, 2, 3 });
		CHECK(t1 == toml::table{ { "bar1"sv, toml::array{ 1, 2, 3 } } });

		t1.insert_or_assign("foo1", *t1.get("bar1"));
		CHECK(t1 == toml::table{ { "bar1"sv, toml::array{ 1, 2, 3 } }, { "foo1"sv, toml::array{ 1, 2, 3 } } });

		// t1["foo1"] = t1["bar1"]; // does nothing, should this fail to compile?
		//  - yes -

		//*t1["foo1"].node() = *t1["bar1"].node(); // compile failure; copying node_view would be a bad thing
		// - correct; copying toml::node directly like that is impossible b.c. it's an abstract base class-

		toml::array* array1 = t1["foo1"].node()->as_array();
		array1->push_back(4);
		CHECK(t1 == toml::table{ { "bar1"sv, toml::array{ 1, 2, 3 } }, { "foo1"sv, toml::array{ 1, 2, 3, 4 } } });

		t1.insert_or_assign("foo3", t1["foo1"]);
		CHECK(t1
			  == toml::table{ { "bar1"sv, toml::array{ 1, 2, 3 } },
							  { "foo1"sv, toml::array{ 1, 2, 3, 4 } },
							  { "foo3"sv, toml::array{ 1, 2, 3, 4 } } });

		t1.insert_or_assign("foo2", *t1["foo1"].node());
		CHECK(t1
			  == toml::table{ { "bar1"sv, toml::array{ 1, 2, 3 } },
							  { "foo1"sv, toml::array{ 1, 2, 3, 4 } },
							  { "foo2"sv, toml::array{ 1, 2, 3, 4 } },
							  { "foo3"sv, toml::array{ 1, 2, 3, 4 } } });

		toml::array* array2 = t1["foo2"].node()->as_array();
		array2->push_back("wrench");
		CHECK(t1
			  == toml::table{ { "bar1"sv, toml::array{ 1, 2, 3 } },
							  { "foo1"sv, toml::array{ 1, 2, 3, 4 } },
							  { "foo2"sv, toml::array{ 1, 2, 3, 4, "wrench" } },
							  { "foo3"sv, toml::array{ 1, 2, 3, 4 } } });

		toml::table t2 = t1;
		CHECK(t2 == t1);
		CHECK(&t2 != &t1);

		// t2.emplace("bar", toml::array{6, 7}); // fails to compile? not sure what I did wrong
		//  - it should be this: -
		t2.emplace<toml::array>("bar", 6, 7);
		CHECK(t2
			  == toml::table{ { "bar"sv, toml::array{ 6, 7 } },
							  { "bar1"sv, toml::array{ 1, 2, 3 } },
							  { "foo1"sv, toml::array{ 1, 2, 3, 4 } },
							  { "foo2"sv, toml::array{ 1, 2, 3, 4, "wrench" } },
							  { "foo3"sv, toml::array{ 1, 2, 3, 4 } } });

		t2.insert_or_assign("bar2", toml::array{ 6, 7 });
		CHECK(t2
			  == toml::table{ { "bar"sv, toml::array{ 6, 7 } },
							  { "bar1"sv, toml::array{ 1, 2, 3 } },
							  { "bar2"sv, toml::array{ 6, 7 } },
							  { "foo1"sv, toml::array{ 1, 2, 3, 4 } },
							  { "foo2"sv, toml::array{ 1, 2, 3, 4, "wrench" } },
							  { "foo3"sv, toml::array{ 1, 2, 3, 4 } } });
	}

	SECTION("tomlplusplus/issues/65") // https://github.com/marzer/tomlplusplus/issues/65
	{
		// these test a number of things
		// - a comment at EOF
		// - a malformed UTF-8 sequence in a comment
		// - a malformed UTF-8 sequence during a KVP
		// - overlong numeric literals
		// all should fail to parse, but correctly issue an error (not crash!)

		parsing_should_fail(FILE_LINE_ARGS, "#\xf1\x63");
		parsing_should_fail(FILE_LINE_ARGS, "1= 0x6cA#+\xf1");
		parsing_should_fail(FILE_LINE_ARGS, "p=06:06:06#\x0b\xff");
		parsing_should_fail(
			FILE_LINE_ARGS,
			"''''d' 't' '+o\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
			"\x0c\x0c\x0c\x0c\x0c\r\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
			"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
			"\x0c\x0c\x0c\x0c\x0c\x0c\x0cop1\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
			"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
			"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c' 'ml'\n\n%\x87");
		parsing_should_fail(
			FILE_LINE_ARGS,
			R"(t =[ 9, 2, 1,"r", 100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.0 ])");
	}

	SECTION("tomlplusplus/issues/67") // https://github.com/marzer/tomlplusplus/issues/67
	{
		const auto data = R"(array=["v1", "v2", "v3"])"sv;

		parsing_should_succeed(FILE_LINE_ARGS,
							   data,
							   [](auto&& table)
							   {
								   auto arr = table["array"].as_array();
								   for (auto it = arr->cbegin(); it != arr->cend();)
									   if (it->value_or(std::string_view{}) == "v2"sv)
										   it = arr->erase(it);
									   else
										   ++it;
								   CHECK(arr->size() == 2);
							   });
	}

	SECTION("tomlplusplus/issues/68") // https://github.com/marzer/tomlplusplus/issues/68
	{
		const auto data = R"(array=["v1", "v2", "v3"])"sv;
		parsing_should_succeed(FILE_LINE_ARGS,
							   data,
							   [](auto&& table)
							   {
								   std::stringstream ss;
								   ss << table;
								   CHECK(ss.str() == "array = [ 'v1', 'v2', 'v3' ]"sv);
							   });
	}

	SECTION("tomlplusplus/issues/69") // https://github.com/marzer/tomlplusplus/issues/69
	{
		using namespace toml::literals; // should compile without namespace ambiguity
		auto table = "[table]\nkey=\"value\""_toml;
	}

	SECTION("tomlplusplus/pull/80") // https://github.com/marzer/tomlplusplus/pull/80
	{
		const auto data = R"(
			a = { "key" = 1 } # inline table
			b = []            # array value
			[[c]]             # array-of-tables with a single, empty table element
		)"sv;

		parsing_should_succeed(FILE_LINE_ARGS,
							   data,
							   [](auto&& table)
							   {
								   std::stringstream ss;
								   ss << table;
								   CHECK(ss.str() == R"(a = { key = 1 }
b = []

[[c]])"sv);
							   });
	}

	SECTION("tomlplusplus/issues/100") // https://github.com/marzer/tomlplusplus/issues/100
	{
		// this tests for two separate things that should fail gracefully, not crash:
		// 1. pathologically-nested inputs
		// 2. a particular sequence of malformed UTF-8

		parsing_should_fail(FILE_LINE_ARGS, "fl =[ [[[[[[[[[[[[[[[\x36\x80\x86\x00\x00\x00\x2D\x36\x9F\x20\x00"sv);

		std::string s(2048_sz, '[');
		constexpr auto start = "fl =[ "sv;
		memcpy(s.data(), start.data(), start.length());
		parsing_should_fail(FILE_LINE_ARGS, std::string_view{ s });
	}

	SECTION("tomlplusplus/issues/112") // https://github.com/marzer/tomlplusplus/issues/112
	{
		parsing_should_fail(FILE_LINE_ARGS,
							R"(
			[a.b.c.d]
			  u = 6
			[a]
			  b.t = 8
			[a.b] # should cause redefinition error here
			  u = 0
		)",
							6);

		parsing_should_fail(FILE_LINE_ARGS,
							R"(
			[a]
			  b.t = 8
			[a.b] # should cause redefinition error here
			  u = 0
		)",
							4);
	}

	SECTION("tomlplusplus/issues/125") // https://github.com/marzer/tomlplusplus/issues/125
	{
		parse_expected_value(FILE_LINE_ARGS, R"("\u0800")"sv, "\xE0\xA0\x80"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u7840")"sv, "\xE7\xA1\x80"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\uAA23")"sv, "\xEA\xA8\xA3"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\uA928")"sv, "\xEA\xA4\xA8"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u9CBF")"sv, "\xE9\xB2\xBF"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u2247")"sv, "\xE2\x89\x87"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u13D9")"sv, "\xE1\x8F\x99"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u69FC")"sv, "\xE6\xA7\xBC"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u8DE5")"sv, "\xE8\xB7\xA5"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u699C")"sv, "\xE6\xA6\x9C"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u8CD4")"sv, "\xE8\xB3\x94"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u4ED4")"sv, "\xE4\xBB\x94"sv);
		parse_expected_value(FILE_LINE_ARGS, R"("\u2597")"sv, "\xE2\x96\x97"sv);
	}

	SECTION("tomlplusplus/issues/127") // https://github.com/marzer/tomlplusplus/issues/127
	{
		parse_expected_value(FILE_LINE_ARGS,
							 "12:34:56.11122233345678"sv,
							 toml::time{
								 12,
								 34,
								 56,
								 111222333u // should truncate the .45678 part
							 });
	}

	SECTION("tomlplusplus/issues/128") // https://github.com/marzer/tomlplusplus/issues/128
	{
		parsing_should_fail(FILE_LINE_ARGS, "\f"sv);
		parsing_should_fail(FILE_LINE_ARGS, "\v"sv);
		parsing_should_succeed(FILE_LINE_ARGS, " "sv);
		parsing_should_succeed(FILE_LINE_ARGS, "\t"sv);
		parsing_should_succeed(FILE_LINE_ARGS, "\n"sv);
	}

	SECTION("tomlplusplus/issues/129") // https://github.com/marzer/tomlplusplus/issues/129
	{
		parsing_should_fail(FILE_LINE_ARGS, R"(
			hex = 0x
			oct = 0o
			bin = 0b
		)"sv);
	}

	SECTION("tomlplusplus/issues/130") // https://github.com/marzer/tomlplusplus/issues/130
	{
		parse_expected_value(FILE_LINE_ARGS, "0400-01-01 00:00:00"sv, toml::date_time{ { 400, 1, 1 }, { 0, 0, 0 } });
		parse_expected_value(FILE_LINE_ARGS, "0400-01-01         "sv, toml::date{ 400, 1, 1 });
		parse_expected_value(FILE_LINE_ARGS, "0400-01-01T00:00:00"sv, toml::date_time{ { 400, 1, 1 }, { 0, 0, 0 } });
		parse_expected_value(FILE_LINE_ARGS, "1000-01-01 00:00:00"sv, toml::date_time{ { 1000, 1, 1 }, { 0, 0, 0 } });
	}

	SECTION("tomlplusplus/issues/131") // https://github.com/marzer/tomlplusplus/issues/131
	{
		parsing_should_fail(FILE_LINE_ARGS, R"(
			a={}
			[a.b]
		)"sv);
	}

	SECTION("tomlplusplus/issues/132") // https://github.com/marzer/tomlplusplus/issues/132
	{
		parsing_should_fail(FILE_LINE_ARGS, "#\r"sv);
	}

	SECTION("tomlplusplus/issues/134") // https://github.com/marzer/tomlplusplus/issues/134
	{
		// binary
		parsing_should_fail(
			FILE_LINE_ARGS,
			"val = 0b11111111_11111111_11111111_11111111_11111111_11111111_11111111_11111111"sv); // uint64_t
																								  // max
		parsing_should_fail(
			FILE_LINE_ARGS,
			"val = 0b10000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000"sv); // int64_t
																								  // max
																								  // + 1
		parse_expected_value(FILE_LINE_ARGS,
							 "0b01111111_11111111_11111111_11111111_11111111_11111111_11111111_11111111"sv,
							 INT64_MAX); // int64_t max

		// octal
		parsing_should_fail(FILE_LINE_ARGS, " val = 0o1777777777777777777777"sv); // uint64_t max
		parsing_should_fail(FILE_LINE_ARGS, " val = 0o1000000000000000000000"sv); // int64_t max + 1
		parse_expected_value(FILE_LINE_ARGS, "      0o0777777777777777777777"sv, INT64_MAX);

		// decimal
		parsing_should_fail(FILE_LINE_ARGS, " val =  100000000000000000000"sv);
		parsing_should_fail(FILE_LINE_ARGS, " val =   18446744073709551615"sv); // uint64_t max
		parsing_should_fail(FILE_LINE_ARGS, " val =   10000000000000000000"sv);
		parsing_should_fail(FILE_LINE_ARGS, " val =    9999999999999999999"sv);
		parsing_should_fail(FILE_LINE_ARGS, " val =    9223372036854775808"sv); // int64_t max + 1
		parse_expected_value(FILE_LINE_ARGS, "         9223372036854775807"sv, INT64_MAX);
		parse_expected_value(FILE_LINE_ARGS, "         1000000000000000000"sv, 1000000000000000000LL);
		parse_expected_value(FILE_LINE_ARGS, "        -1000000000000000000"sv, -1000000000000000000LL);
		parse_expected_value(FILE_LINE_ARGS, "        -9223372036854775808"sv, INT64_MIN);
		parsing_should_fail(FILE_LINE_ARGS, " val =   -9223372036854775809"sv); // int64_t min - 1
		parsing_should_fail(FILE_LINE_ARGS, " val =  -10000000000000000000"sv);
		parsing_should_fail(FILE_LINE_ARGS, " val =  -18446744073709551615"sv); // -(uint64_t max)
		parsing_should_fail(FILE_LINE_ARGS, " val = -100000000000000000000"sv);

		// hexadecimal
		parsing_should_fail(FILE_LINE_ARGS, " val = 0xFFFFFFFFFFFFFFFF"sv); // uint64_t max
		parsing_should_fail(FILE_LINE_ARGS, " val = 0x8000000000000000"sv); // int64_t max + 1
		parse_expected_value(FILE_LINE_ARGS, "      0x7FFFFFFFFFFFFFFF"sv, INT64_MAX);
	}

	SECTION("tomlplusplus/issues/135") // https://github.com/marzer/tomlplusplus/issues/135
	{
		parsing_should_succeed(FILE_LINE_ARGS, "0=0"sv);
		parsing_should_succeed(FILE_LINE_ARGS, "1=1"sv);
		parsing_should_succeed(FILE_LINE_ARGS, "2=2"sv);

		parsing_should_succeed(FILE_LINE_ARGS,
							   "0=0\n"
							   "1=1\n"
							   "2=2\n"sv);

		parsing_should_fail(FILE_LINE_ARGS,
							"0=0\n"
							"\u2000\u2000\n"
							"1=1\n"
							"2=2\n"sv);
	}

	SECTION("tomlplusplus/issues/152") // https://github.com/marzer/tomlplusplus/issues/152
	{
		// clang-format off
		static constexpr auto data = R"([shaders.room_darker])" "\n"
									 R"(file = "room_darker.frag")" "\n"
									 R"(args = { n = "integer", ambientLightLevel = "float" })";
		// clang-format on

		parsing_should_succeed(FILE_LINE_ARGS,
							   data,
							   [](auto&& tbl)
							   {
								   const auto check_location = [&](std::string_view path, auto line, auto col)
								   {
									   INFO("Checking source location of  \""sv << path << "\""sv)
									   auto v = tbl.at_path(path);
									   REQUIRE(v.node());
									   CHECK(v.node()->source().begin.line == static_cast<toml::source_index>(line));
									   CHECK(v.node()->source().begin.column == static_cast<toml::source_index>(col));
								   };

								   check_location("shaders"sv, 1, 1);
								   check_location("shaders.room_darker"sv, 1, 1);
								   check_location("shaders.room_darker.file"sv, 2, 8);
								   check_location("shaders.room_darker.args"sv, 3, 8);
								   check_location("shaders.room_darker.args.n"sv, 3, 14);
								   check_location("shaders.room_darker.args.ambientLightLevel"sv, 3, 45);
							   });
	}

	SECTION("toml/issues/908") // https://github.com/toml-lang/toml/issues/908
	{
		parsing_should_fail(FILE_LINE_ARGS, R"(
			a = [{ b = 1 }]
			[a.c]
			foo = 1
		)"sv);

		parsing_should_succeed(FILE_LINE_ARGS, R"(
			[[a]]
			b = 1

			[a.c]
			foo = 1
		)"sv);
	}

	SECTION("tomlplusplus/issues/169") // https://github.com/marzer/tomlplusplus/issues/169
	{
		parsing_should_fail(FILE_LINE_ARGS, R"(
			[a]
			b = [c"]
		)"sv);
	}

	SECTION("tomlplusplus/issues/179") // https://github.com/marzer/tomlplusplus/issues/179
	{
		parse_expected_value(FILE_LINE_ARGS, "0.848213"sv, 0.848213);
		parse_expected_value(FILE_LINE_ARGS, "6.9342"sv, 6.9342);
		parse_expected_value(FILE_LINE_ARGS, "-995.9214"sv, -995.9214);
	}

	SECTION("tomlplusplus/issues/187") // https://github.com/marzer/tomlplusplus/issues/187
	{
		parsing_should_succeed(FILE_LINE_ARGS, R"(
			[[a.b]]
			x = 1

			[a]
			y = 2
		)"sv);
	}

	SECTION("tomlplusplus/issues/207") // https://github.com/marzer/tomlplusplus/issues/207
	{
		enum class an_enum
		{
			zero,
			one,
			two,
			three
		};

		parsing_should_succeed(FILE_LINE_ARGS,
							   "val = 2\n",
							   [](auto&& tbl)
							   {
								   const auto val = tbl["val"].template value_or<an_enum>(an_enum::zero);
								   CHECK(val == an_enum::two);
							   });
	}

	SECTION("tomlplusplus/issues/176") // https://github.com/marzer/tomlplusplus/issues/176
	{
		parsing_should_succeed(FILE_LINE_ARGS, "  a      = \"x\\ty\""sv);
		parsing_should_succeed(FILE_LINE_ARGS, "\"a\"    = \"x\\ty\""sv);
		parsing_should_succeed(FILE_LINE_ARGS, "\"a\tb\" = \"x\\ty\""sv);
		parsing_should_fail(FILE_LINE_ARGS, "\"a\nb\" = \"x\\ty\""sv); // literal newline in single-line key

		static constexpr auto input = R"(
								"a"    = "x\ty"
								"a\tb" = "x\ty"
								"a\nb" = "x\ty"
								)"sv;

		static constexpr auto output = "a = 'x\ty'\n"
									   "\"a\\tb\" = 'x\ty'\n" // tab and newlines in keys should be emitted
									   "\"a\\nb\" = 'x\ty'"	  // as escapes, not literals
									   ""sv;

		parsing_should_succeed(FILE_LINE_ARGS,
							   input,
							   [&](auto&& tbl)
							   {
								   CHECK(tbl["a"]);
								   CHECK(tbl["a\tb"]);
								   CHECK(tbl["a\nb"]);

								   std::stringstream ss;
								   ss << tbl;
								   CHECK(ss.str() == output);
							   });
	}
}
