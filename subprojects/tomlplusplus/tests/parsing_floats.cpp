// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"

TEST_CASE("parsing - floats")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# fractional
								flt1 = +1.0
								flt2 = 3.1415
								flt3 = -0.01

								# exponent
								flt4 = 5e+22
								flt5 = 1e06
								flt6 = -2E-2

								# both
								flt7 = 6.626e-34

								flt8 = 224_617.445_991_228
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["flt1"] == 1.0);
							   CHECK(tbl["flt2"] == 3.1415);
							   CHECK(tbl["flt3"] == -0.01);
							   CHECK(tbl["flt4"].as<double>()->get() == 5e+22_a);
							   CHECK(tbl["flt5"].as<double>()->get() == 1e6_a);
							   CHECK(tbl["flt6"] == -2E-2);
							   CHECK(tbl["flt7"].as<double>()->get() == 6.626e-34_a);
							   CHECK(tbl["flt8"].as<double>()->get() == 224617.445991228_a);
						   });

	// "Each underscore must be surrounded by at least one digit on each side."
	parsing_should_fail(FILE_LINE_ARGS, "flt8 = 224_617.445_991_228_"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt8 = _224_617.445_991_228"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt8 = 224__617.445_991_228"sv);

	// "Float values -0.0 and +0.0 are valid and should map according to IEEE 754."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(zeroes = [-0.0, +0.0])"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["zeroes"][0] == -0.0);
							   CHECK(tbl["zeroes"][1] == +0.0);
						   });

	// "A float consists of an integer part followed by a fractional part and/or an exponent part"
	// (i.e. omitting the leading digits before the '.' is not legal in TOML)
	parsing_should_fail(FILE_LINE_ARGS, "flt = .1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = +.1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = -.1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = .1e1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = .1e+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = .1e-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = +.1e1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = +.1e+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = +.1e-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = -.1e1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = -.1e+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = -.1e-1"sv);

	// likewise, so is omitting digits _after_ the '.'
	parsing_should_fail(FILE_LINE_ARGS, "flt = 1."sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = +1."sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = -1."sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = 1.e1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = 1.e+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = 1.e-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = +1.e1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = +1.e+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = +1.e-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = -1.e1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = -1.e+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, "flt = -1.e-1"sv);

	// value tests
	parse_expected_value(FILE_LINE_ARGS, "1e1"sv, 1e1);
	parse_expected_value(FILE_LINE_ARGS, "1e+1"sv, 1e+1);
	parse_expected_value(FILE_LINE_ARGS, "1e-1"sv, 1e-1);
	parse_expected_value(FILE_LINE_ARGS, "1.0"sv, 1.0);
	parse_expected_value(FILE_LINE_ARGS, "1.0e1"sv, 1.0e1);
	parse_expected_value(FILE_LINE_ARGS, "1.0e+1"sv, 1.0e+1);
	parse_expected_value(FILE_LINE_ARGS, "1.0e-1"sv, 1.0e-1);
	parse_expected_value(FILE_LINE_ARGS, "+1e1"sv, +1e1);
	parse_expected_value(FILE_LINE_ARGS, "+1.0"sv, +1.0);
	parse_expected_value(FILE_LINE_ARGS, "+1.0e1"sv, +1.0e1);
	parse_expected_value(FILE_LINE_ARGS, "+1.0e+1"sv, +1.0e+1);
	parse_expected_value(FILE_LINE_ARGS, "+1.0e-1"sv, +1.0e-1);
	parse_expected_value(FILE_LINE_ARGS, "-1.0e+1"sv, -1.0e+1);
	parse_expected_value(FILE_LINE_ARGS, "-1e1"sv, -1e1);
	parse_expected_value(FILE_LINE_ARGS, "-1.0"sv, -1.0);
	parse_expected_value(FILE_LINE_ARGS, "-1.0e1"sv, -1.0e1);
	parse_expected_value(FILE_LINE_ARGS, "-1.0e-1"sv, -1.0e-1);
	parse_expected_value(FILE_LINE_ARGS, "0.1"sv, 0.1);
	parse_expected_value(FILE_LINE_ARGS, "0.001"sv, 0.001);
	parse_expected_value(FILE_LINE_ARGS, "0.100"sv, 0.100);
	parse_expected_value(FILE_LINE_ARGS, "+3.14"sv, +3.14);
	parse_expected_value(FILE_LINE_ARGS, "-3.14"sv, -3.14);
	parse_expected_value(FILE_LINE_ARGS, "3.1415_9265_3589"sv, 3.141592653589);
	parse_expected_value(FILE_LINE_ARGS, "+3.1415_9265_3589"sv, +3.141592653589);
	parse_expected_value(FILE_LINE_ARGS, "-3.1415_9265_3589"sv, -3.141592653589);
	parse_expected_value(FILE_LINE_ARGS, "123_456.789"sv, 123456.789);
	parse_expected_value(FILE_LINE_ARGS, "+123_456.789"sv, +123456.789);
	parse_expected_value(FILE_LINE_ARGS, "-123_456.789"sv, -123456.789);
	parse_expected_value(FILE_LINE_ARGS, "+0.0"sv, +0.0);
	parse_expected_value(FILE_LINE_ARGS, "-0.0"sv, -0.0);
	parse_expected_value(FILE_LINE_ARGS, "1e10"sv, 1e10);
	parse_expected_value(FILE_LINE_ARGS, "1e+10"sv, 1e+10);
	parse_expected_value(FILE_LINE_ARGS, "1e-10"sv, 1e-10);
	parse_expected_value(FILE_LINE_ARGS, "+1e10"sv, +1e10);
	parse_expected_value(FILE_LINE_ARGS, "+1e+10"sv, +1e+10);
	parse_expected_value(FILE_LINE_ARGS, "+1e-10"sv, +1e-10);
	parse_expected_value(FILE_LINE_ARGS, "-1e10"sv, -1e10);
	parse_expected_value(FILE_LINE_ARGS, "-1e+10"sv, -1e+10);
	parse_expected_value(FILE_LINE_ARGS, "-1e-10"sv, -1e-10);
	parse_expected_value(FILE_LINE_ARGS, "123e-10"sv, 123e-10);
	parse_expected_value(FILE_LINE_ARGS, "1E10"sv, 1E10);
	parse_expected_value(FILE_LINE_ARGS, "1E+10"sv, 1E+10);
	parse_expected_value(FILE_LINE_ARGS, "1E-10"sv, 1E-10);
	parse_expected_value(FILE_LINE_ARGS, "123E-10"sv, 123E-10);
	parse_expected_value(FILE_LINE_ARGS, "1_2_3E-10"sv, 123E-10);
	parse_expected_value(FILE_LINE_ARGS, "1_2_3E-1_0"sv, 123E-10);
	parse_expected_value(FILE_LINE_ARGS, "+0e0"sv, +0e0);
	parse_expected_value(FILE_LINE_ARGS, "-0e0"sv, -0e0);
	parse_expected_value(FILE_LINE_ARGS, "1_2_3E-01"sv, 123E-01);
	parse_expected_value(FILE_LINE_ARGS, "1_2_3E-0_1"sv, 123E-01);
	parse_expected_value(FILE_LINE_ARGS, "6.02e23"sv, 6.02e23);
	parse_expected_value(FILE_LINE_ARGS, "6.02e+23"sv, 6.02e+23);
	parse_expected_value(FILE_LINE_ARGS, "1.112_650_06e-17"sv, 1.11265006e-17);
	parse_expected_value(FILE_LINE_ARGS, "0.010284358729827818"sv, 0.010284358729827818);
	parse_expected_value(FILE_LINE_ARGS, "0.010284358729827818"sv, 0.010284358729827818);
	parse_expected_value(FILE_LINE_ARGS, "0.0102"sv, 0.0102);
	parse_expected_value(FILE_LINE_ARGS, "10.0102"sv, 10.0102);
	parse_expected_value(FILE_LINE_ARGS, "10.010284358729828"sv, 10.010284358729828);
	parse_expected_value(FILE_LINE_ARGS, "10.0"sv, 10.0);

// toml/issues/562 (hexfloats)
#if TOML_LANG_UNRELEASED
	parse_expected_value(FILE_LINE_ARGS, "       0x1.2p3"sv, 0x1.2p3);
	parse_expected_value(FILE_LINE_ARGS, "        0x10p1"sv, 0x10p1);
	parse_expected_value(FILE_LINE_ARGS, "       0x10p-1"sv, 0x10p-1);
	parse_expected_value(FILE_LINE_ARGS, "       0x10p+1"sv, 0x10p+1);
	parse_expected_value(FILE_LINE_ARGS, "       -0x10p1"sv, -0x10p1);
	parse_expected_value(FILE_LINE_ARGS, "      -0x10p-1"sv, -0x10p-1);
	parse_expected_value(FILE_LINE_ARGS, "       +0x10p1"sv, +0x10p1);
	parse_expected_value(FILE_LINE_ARGS, "      +0x10p+1"sv, +0x10p+1);
	parse_expected_value(FILE_LINE_ARGS, "      -0x10p+1"sv, -0x10p+1);
	parse_expected_value(FILE_LINE_ARGS, "      +0x10p-1"sv, +0x10p-1);
	parse_expected_value(FILE_LINE_ARGS, "      0x10.1p1"sv, 0x10.1p1);
	parse_expected_value(FILE_LINE_ARGS, "     0x10.1p-1"sv, 0x10.1p-1);
	parse_expected_value(FILE_LINE_ARGS, "     0x10.1p+1"sv, 0x10.1p+1);
	parse_expected_value(FILE_LINE_ARGS, "     -0x10.1p1"sv, -0x10.1p1);
	parse_expected_value(FILE_LINE_ARGS, "    -0x10.1p-1"sv, -0x10.1p-1);
	parse_expected_value(FILE_LINE_ARGS, "     +0x10.1p1"sv, +0x10.1p1);
	parse_expected_value(FILE_LINE_ARGS, "    +0x10.1p+1"sv, +0x10.1p+1);
	parse_expected_value(FILE_LINE_ARGS, "    -0x10.1p+1"sv, -0x10.1p+1);
	parse_expected_value(FILE_LINE_ARGS, "    +0x10.1p-1"sv, +0x10.1p-1);
#else
	parsing_should_fail(FILE_LINE_ARGS, " val =     0x10p1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    0x10p-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    0x10p+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    -0x10p1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   -0x10p-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    +0x10p1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   +0x10p+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   -0x10p+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   +0x10p-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   0x10.1p1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =  0x10.1p-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =  0x10.1p+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =  -0x10.1p1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val = -0x10.1p-1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =  +0x10.1p1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val = +0x10.1p+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val = -0x10.1p+1"sv);
	parsing_should_fail(FILE_LINE_ARGS, " val = +0x10.1p-1"sv);
#endif
}

TEST_CASE("parsing - inf and nan")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# infinity
								sf1 = inf  # positive infinity
								sf2 = +inf # positive infinity
								sf3 = -inf # negative infinity

								# not a number
								sf4 = nan  # actual sNaN/qNaN encoding is implementation specific
								sf5 = +nan # same as `nan`
								sf6 = -nan # valid, actual encoding is implementation specific
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(impl::fpclassify(**tbl["sf1"].as<double>()) == impl::fp_class::pos_inf);
							   CHECK(impl::fpclassify(**tbl["sf2"].as<double>()) == impl::fp_class::pos_inf);
							   CHECK(impl::fpclassify(**tbl["sf3"].as<double>()) == impl::fp_class::neg_inf);
							   CHECK(impl::fpclassify(**tbl["sf4"].as<double>()) == impl::fp_class::nan);
							   CHECK(impl::fpclassify(**tbl["sf5"].as<double>()) == impl::fp_class::nan);
							   CHECK(impl::fpclassify(**tbl["sf6"].as<double>()) == impl::fp_class::nan);
						   });

	parsing_should_fail(FILE_LINE_ARGS, " val =     NaN "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =     Nan "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =     NAN "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    +NaN "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    +Nan "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    +NAN "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    -NaN "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    -Nan "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    -NAN "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   1.nan "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   1,nan "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    .nan "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    ,nan "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   nan.1 "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   nan,1 "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    nan. "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    nan, "sv);

	parsing_should_fail(FILE_LINE_ARGS, " val =     Inf "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =     INF "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    +Inf "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    +INF "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    -Inf "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    -INF "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   1.inf "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   1,inf "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    .inf "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    ,inf "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   inf.1 "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =   inf,1 "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    inf. "sv);
	parsing_should_fail(FILE_LINE_ARGS, " val =    inf, "sv);
}
