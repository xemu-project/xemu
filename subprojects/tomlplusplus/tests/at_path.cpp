// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"
TOML_DISABLE_SPAM_WARNINGS;

TEST_CASE("at_path")
{
	// clang-format off

	const auto tbl = table
	{
		{ ""sv, 0 }, // blank key
		{ "a"sv, 1 },
		{
			"b"sv,
			array
			{
				2,
				array{ 3 },
				table { { "c", 4 } }
			},
		},
		{ "d", table{ {"e", 5, }, {""sv, -1 } } }
	};

	// clang-format on

	/*

	# equivalent to the following TOML:

	"" = 0
	a = 1
	b = [
			2,
			[ 3 ],
			{ "c" = 4 }
	]
	d = { "e" = 5, "" = -1 }

	*/

	SECTION("table")
	{
		// this section uses the free function version of at_path

		CHECK(tbl[""]);
		CHECK(tbl[""] == at_path(tbl, ""));

		CHECK(tbl["a"]);
		CHECK(tbl["a"] == at_path(tbl, "a"));
		CHECK(tbl["a"] != at_path(tbl, ".a")); // equivalent to ""."a"
		CHECK(!at_path(tbl, ".a"));

		CHECK(tbl["b"]);
		CHECK(tbl["b"] == at_path(tbl, "b"));

		CHECK(tbl["b"][0]);
		CHECK(tbl["b"][0] == at_path(tbl, "b[0]"));
		CHECK(tbl["b"][0] == at_path(tbl, "b[0]     "));
		CHECK(tbl["b"][0] == at_path(tbl, "b[ 0\t]")); // whitespace is allowed inside indexer

		CHECK(tbl["b"][1]);
		CHECK(tbl["b"][1] != tbl["b"][0]);
		CHECK(tbl["b"][1] == at_path(tbl, "b[1]"));

		CHECK(tbl["b"][1][0]);
		CHECK(tbl["b"][1][0] == at_path(tbl, "b[1][0]"));
		CHECK(tbl["b"][1][0] == at_path(tbl, "b[1]    \t   [0]")); // whitespace is allowed after indexers

		CHECK(tbl["b"][2]["c"]);
		CHECK(tbl["b"][2]["c"] == at_path(tbl, "b[2].c"));
		CHECK(tbl["b"][2]["c"] == at_path(tbl, "b[2]   \t.c")); // whitespace is allowed after indexers

		// permissivity checks for missing trailing ']'
		// (this permissivity is undocumented but serves to reduce error paths in user code)
		CHECK(tbl["b"][1][0] == at_path(tbl, "b[1[0]"));
		CHECK(tbl["b"][1][0] == at_path(tbl, "b[1[0"));
		CHECK(tbl["b"][2]["c"] == at_path(tbl, "b[2.c"));

		CHECK(tbl["d"]);
		CHECK(tbl["d"] == at_path(tbl, "d"));

		CHECK(tbl["d"]["e"]);
		CHECK(tbl["d"]["e"] == at_path(tbl, "d.e"));
		CHECK(tbl["d"]["e"] != at_path(tbl, "d. e")); // equivalent to "d"." e"
		CHECK(!at_path(tbl, "d. e"));

		CHECK(tbl["d"][""]);
		CHECK(tbl["d"][""] == at_path(tbl, "d."));
	}

	SECTION("array")
	{
		// this section uses the node_view member function version of at_path

		auto arr = tbl["b"];

		CHECK(tbl["b"][0]);
		CHECK(tbl["b"][0] == arr.at_path("[0]"));
		CHECK(tbl["b"][0] == arr.at_path("[0]     "));
		CHECK(tbl["b"][0] == arr.at_path("[ 0\t]")); // whitespace is allowed inside indexer

		CHECK(tbl["b"][1]);
		CHECK(tbl["b"][1].node() != arr[0].node());
		CHECK(tbl["b"][1] == arr.at_path("[1]"));

		CHECK(tbl["b"][1][0]);
		CHECK(tbl["b"][1][0] == arr.at_path("[1][0]"));
		CHECK(tbl["b"][1][0] == arr.at_path("[1]    \t   [0]")); // whitespace is allowed after indexers

		CHECK(tbl["b"][2]["c"]);
		CHECK(tbl["b"][2]["c"] == arr.at_path("[2].c"));
		CHECK(tbl["b"][2]["c"] == arr.at_path("[2]   \t.c")); // whitespace is allowed after indexers
	}
}
