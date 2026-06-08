// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"

TEST_CASE("parsing - comments")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# This is a full-line comment
								key = "value"  # This is a comment at the end of a line
								another = "# This is not a comment"
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl.size() == 2);
							   CHECK(tbl["key"] == "value"sv);
							   CHECK(tbl["another"] == "# This is not a comment"sv);
						   });

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(# this = "looks like a KVP but is commented out)"sv,
						   [](table&& tbl) { CHECK(tbl.size() == 0); });

#if TOML_LANG_AT_LEAST(1, 0, 0)
	{
		// toml/issues/567 (disallow non-TAB control characters in comments)
		// 00 - 08
		parsing_should_fail(FILE_LINE_ARGS, "# \u0000 some trailing garbage"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0001"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0002"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0003"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0004"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0005"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0006"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0007"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0008"sv);

		// skip tab and line breaks (real and otherwise)
		// \u0009 is \t
		// \u000A is \n
		// \u000B is \v (vertical tab)
		// \u000C is \f (form feed)
		// \u000D is \r

		// 0E - 1F
		parsing_should_fail(FILE_LINE_ARGS, "# \u000E"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u000F"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0010"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0011"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0012"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0013"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0014"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0015"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0016"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0017"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0018"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u0019"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u001A"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u001B"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u001C"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u001D"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u001E"sv);
		parsing_should_fail(FILE_LINE_ARGS, "# \u001F"sv);
		// 7F
		parsing_should_fail(FILE_LINE_ARGS, "# \u007F"sv);
	}
#else
	{
		parsing_should_succeed(FILE_LINE_ARGS,
							   S("## 00 - 08"
								 "# \u0000  "
								 "# \u0001  "
								 "# \u0002  "
								 "# \u0003  "
								 "# \u0004  "
								 "# \u0005  "
								 "# \u0006  "
								 "# \u0007  "
								 "# \u0008  "
								 "## 0A - 1F"
								 "# \u000A  "
								 "# \u000B  "
								 "# \u000C  "
								 "# \u000D  "
								 "# \u000E  "
								 "# \u000F  "
								 "# \u0010  "
								 "# \u0011  "
								 "# \u0012  "
								 "# \u0013  "
								 "# \u0014  "
								 "# \u0015  "
								 "# \u0016  "
								 "# \u0017  "
								 "# \u0018  "
								 "# \u0019  "
								 "# \u001A  "
								 "# \u001B  "
								 "# \u001C  "
								 "# \u001D  "
								 "# \u001E  "
								 "# \u001F  "
								 "## 7F	   "
								 "# \u007F  "sv));
	}
#endif
}
