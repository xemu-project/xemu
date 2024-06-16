// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

TEST_CASE("parsing - booleans")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								bool1 = true
								bool2 = false
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["bool1"] == true);
							   CHECK(tbl["bool2"] == false);
						   });

	// "Always lowercase."
	parsing_should_fail(FILE_LINE_ARGS, "bool = True"sv);
	parsing_should_fail(FILE_LINE_ARGS, "bool = TRUE"sv);
	parsing_should_fail(FILE_LINE_ARGS, "bool = tRUE"sv);
	parsing_should_fail(FILE_LINE_ARGS, "bool = False"sv);
	parsing_should_fail(FILE_LINE_ARGS, "bool = FALSE"sv);
	parsing_should_fail(FILE_LINE_ARGS, "bool = fALSE"sv);

	// value tests
	parse_expected_value(FILE_LINE_ARGS, " true", true);
	parse_expected_value(FILE_LINE_ARGS, "false", false);
}
