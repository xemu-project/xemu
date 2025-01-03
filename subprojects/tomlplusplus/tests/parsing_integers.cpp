// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

TEST_CASE("parsing - integers (decimal)")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   BOM_PREFIX R"(
								int1 = +99
								int2 = 42
								int3 = 0
								int4 = -17
								int5 = 1_000
								int6 = 5_349_221
								int7 = 1_2_3_4_5     # VALID but discouraged
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["int1"] == 99);
							   CHECK(tbl["int2"] == 42);
							   CHECK(tbl["int3"] == 0);
							   CHECK(tbl["int4"] == -17);
							   CHECK(tbl["int5"] == 1000);
							   CHECK(tbl["int6"] == 5349221);
							   CHECK(tbl["int7"] == 12345);
						   });

	// "Each underscore must be surrounded by at least one digit on each side."
	parsing_should_fail(FILE_LINE_ARGS, "int5 = 1__000"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int5 = _1_000"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int5 = 1_000_"sv);

	// "Leading zeroes are not allowed."
	parsing_should_fail(FILE_LINE_ARGS, "int1 = +099"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int2 = 042"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int3 = 00"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int4 = -017"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int5 = 01_000"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int6 = 05_349_221"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int7 = 01_2_3_4_5"sv);

	// "Integer values -0 and +0 are valid and identical to an unprefixed zero."
	parsing_should_succeed(FILE_LINE_ARGS,
						   "zeroes = [-0, +0]"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["zeroes"][0] == 0);
							   CHECK(tbl["zeroes"][1] == 0);
						   });

	// "64 bit (signed long) range expected (−9,223,372,036,854,775,808 to 9,223,372,036,854,775,807)."
	parse_expected_value(FILE_LINE_ARGS, "9223372036854775807"sv, INT64_MAX);
	parse_expected_value(FILE_LINE_ARGS, "-9223372036854775808"sv, INT64_MIN);
	parsing_should_fail(FILE_LINE_ARGS, "val =  9223372036854775808"sv); // INT64_MAX + 1
	parsing_should_fail(FILE_LINE_ARGS, "val = -9223372036854775809"sv); // INT64_MIN - 1

	// signs in weird places
	parsing_should_fail(FILE_LINE_ARGS, "val = +-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = -+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = ++1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = --1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = 1-"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = 1+"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = -1+"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = +1-"sv);

	// value tests
	parse_expected_value(FILE_LINE_ARGS, "0"sv, 0);
	parse_expected_value(FILE_LINE_ARGS, "1"sv, 1);
	parse_expected_value(FILE_LINE_ARGS, "+1"sv, 1);
	parse_expected_value(FILE_LINE_ARGS, "-1"sv, -1);
	parse_expected_value(FILE_LINE_ARGS, "1234"sv, 1234);
	parse_expected_value(FILE_LINE_ARGS, "+1234"sv, 1234);
	parse_expected_value(FILE_LINE_ARGS, "-1234"sv, -1234);
	parse_expected_value(FILE_LINE_ARGS, "1_2_3_4"sv, 1234);
	parse_expected_value(FILE_LINE_ARGS, "+1_2_3_4"sv, 1234);
	parse_expected_value(FILE_LINE_ARGS, "-1_2_3_4"sv, -1234);
	parse_expected_value(FILE_LINE_ARGS, "123_456_789"sv, 123456789);
}

TEST_CASE("parsing - integers (hex, bin, oct)")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# hexadecimal with prefix `0x`
								hex1 = 0xDEADBEEF
								hex2 = 0xdeadbeef
								hex3 = 0xdead_beef

								# octal with prefix `0o`
								oct1 = 0o01234567
								oct2 = 0o755 # useful for Unix file permissions

								# binary with prefix `0b`
								bin1 = 0b11010110
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["hex1"] == 0xDEADBEEF);
							   CHECK(tbl["hex2"] == 0xDEADBEEF);
							   CHECK(tbl["hex3"] == 0xDEADBEEF);
							   CHECK(tbl["oct1"] == 01234567);
							   CHECK(tbl["oct2"] == 0755);
							   CHECK(tbl["bin1"] == 0b11010110);
						   });

	// "leading + is not allowed"
	parsing_should_fail(FILE_LINE_ARGS, "hex1 = +0xDEADBEEF"sv);
	parsing_should_fail(FILE_LINE_ARGS, "hex2 = +0xdeadbeef"sv);
	parsing_should_fail(FILE_LINE_ARGS, "hex3 = +0xdead_beef"sv);
	parsing_should_fail(FILE_LINE_ARGS, "oct1 = +0o01234567"sv);
	parsing_should_fail(FILE_LINE_ARGS, "oct2 = +0o7550"sv);
	parsing_should_fail(FILE_LINE_ARGS, "int6 = +05_349_221"sv);
	parsing_should_fail(FILE_LINE_ARGS, "bin1 = +0b11010110"sv);

	// "leading zeros are allowed (after the prefix)"
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								hex1 = 0x000DEADBEEF
								hex2 = 0x00000deadbeef
								hex3 = 0x0dead_beef
								oct1 = 0o0001234567
								oct2 = 0o000755
								bin1 = 0b0000011010110
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["hex1"] == 0xDEADBEEF);
							   CHECK(tbl["hex2"] == 0xDEADBEEF);
							   CHECK(tbl["hex3"] == 0xDEADBEEF);
							   CHECK(tbl["oct1"] == 01234567);
							   CHECK(tbl["oct2"] == 0755);
							   CHECK(tbl["bin1"] == 0b11010110);
						   });

	// "***Non-negative*** integer values may also be expressed in hexadecimal, octal, or binary"
	parsing_should_fail(FILE_LINE_ARGS, "val = -0x1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = -0o1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = -0b1"sv);

	// "64 bit (signed long) range expected (−9,223,372,036,854,775,808 to 9,223,372,036,854,775,807)."
	// (ignoring INT64_MIN because toml doesn't allow these forms to represent negative values)
	parse_expected_value(FILE_LINE_ARGS, "0x7FFFFFFFFFFFFFFF"sv, INT64_MAX);
	parse_expected_value(FILE_LINE_ARGS, "0o777777777777777777777"sv, INT64_MAX);
	parse_expected_value(FILE_LINE_ARGS,
						 "0b111111111111111111111111111111111111111111111111111111111111111"sv,
						 INT64_MAX);
	parsing_should_fail(FILE_LINE_ARGS, "val =       0x8000000000000000"sv); // INT64_MAX + 1
	parsing_should_fail(FILE_LINE_ARGS, "val = 0o1000000000000000000000"sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = 0b1000000000000000000000000000000000000000000000000000000000000000"sv);

	// missing values after base prefix
	parsing_should_fail(FILE_LINE_ARGS, "val = 0x "sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = 0o "sv);
	parsing_should_fail(FILE_LINE_ARGS, "val = 0b "sv);

	// value tests
	parse_expected_value(FILE_LINE_ARGS, "0xDEADBEEF"sv, 0xDEADBEEF);
	parse_expected_value(FILE_LINE_ARGS, "0xdeadbeef"sv, 0xDEADBEEF);
	parse_expected_value(FILE_LINE_ARGS, "0xDEADbeef"sv, 0xDEADBEEF);
	parse_expected_value(FILE_LINE_ARGS, "0xDEAD_BEEF"sv, 0xDEADBEEF);
	parse_expected_value(FILE_LINE_ARGS, "0xdead_beef"sv, 0xDEADBEEF);
	parse_expected_value(FILE_LINE_ARGS, "0xdead_BEEF"sv, 0xDEADBEEF);
	parse_expected_value(FILE_LINE_ARGS, "0xFF"sv, 0xFF);
	parse_expected_value(FILE_LINE_ARGS, "0x00FF"sv, 0xFF);
	parse_expected_value(FILE_LINE_ARGS, "0x0000FF"sv, 0xFF);
	parse_expected_value(FILE_LINE_ARGS, "0o777"sv, 0777);
	parse_expected_value(FILE_LINE_ARGS, "0o7_7_7"sv, 0777);
	parse_expected_value(FILE_LINE_ARGS, "0o007"sv, 0007);
	parse_expected_value(FILE_LINE_ARGS, "0b10000"sv, 0b10000);
	parse_expected_value(FILE_LINE_ARGS, "0b010000"sv, 0b10000);
	parse_expected_value(FILE_LINE_ARGS, "0b01_00_00"sv, 0b10000);
	parse_expected_value(FILE_LINE_ARGS, "0b111111"sv, 0b111111);
}
