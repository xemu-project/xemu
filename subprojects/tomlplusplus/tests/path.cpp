// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"
TOML_DISABLE_SPAM_WARNINGS;

TEST_CASE("path - parsing")
{
	SECTION("parsing")
	{
		CHECK(toml::path("").str() == "");
		CHECK(toml::path("[1]").str() == "[1]");
		CHECK(toml::path("[1][2]").str() == "[1][2]");
		CHECK(toml::path("  [1][2]").str() == "  [1][2]");
		CHECK(toml::path("a.  .b").str() == "a.  .b");
		CHECK(toml::path("test[23]").str() == "test[23]");
		CHECK(toml::path("[ 120  ]").str() == "[120]");
		CHECK(toml::path("[ 120\t\t]").str() == "[120]");
		CHECK(toml::path("test.value").str() == "test.value");
		CHECK(toml::path("test[0].value").str() == "test[0].value");
		CHECK(toml::path("test[1][2]\t .value").str() == "test[1][2].value");
		CHECK(toml::path("test[1]\t[2].value").str() == "test[1][2].value");
		CHECK(toml::path(".test[1][2]\t ..value").str() == ".test[1][2]..value");

#if TOML_ENABLE_WINDOWS_COMPAT

		CHECK(toml::path(L"").str() == "");
		CHECK(toml::path(L"[1]").str() == "[1]");
		CHECK(toml::path(L"[1][2]").str() == "[1][2]");
		CHECK(toml::path(L"  [1][2]").str() == "  [1][2]");
		CHECK(toml::path(L"a.  .b").str() == "a.  .b");
		CHECK(toml::path(L"test[23]").str() == "test[23]");
		CHECK(toml::path(L"[ 120  ]").str() == "[120]");
		CHECK(toml::path(L"[ 120\t\t]").str() == "[120]");
		CHECK(toml::path(L"test.value").str() == "test.value");
		CHECK(toml::path(L"test[0].value").str() == "test[0].value");
		CHECK(toml::path(L"test[1][2]\t .value").str() == "test[1][2].value");
		CHECK(toml::path(L"test[1]\t[2].value").str() == "test[1][2].value");
		CHECK(toml::path(L".test[1][2]\t ..value").str() == ".test[1][2]..value");

#endif // TOML_ENABLE_WINDOWS_COMPAT
	}

	SECTION("parsing - errors")
	{
		CHECK(!toml::path("test[][2].value"));
		CHECK(!toml::path("test[      "));
		CHECK(!toml::path("test[1]a.b"));
		CHECK(!toml::path("test[1]   a.b"));
		CHECK(!toml::path("test[1a]"));
		CHECK(!toml::path("test[a1]"));
		CHECK(!toml::path("test[1!]"));
		CHECK(!toml::path("test[!1]"));
		CHECK(!toml::path("test[1 2]"));
		CHECK(!toml::path("test[1.2]"));
		CHECK(!toml::path("test[0.2]"));

#if TOML_ENABLE_WINDOWS_COMPAT

		CHECK(!toml::path(L"test[][2].value"));
		CHECK(!toml::path(L"test[      "));
		CHECK(!toml::path(L"test[1]a.b"));
		CHECK(!toml::path(L"test[1]   a.b"));
		CHECK(!toml::path(L"test[1a]"));
		CHECK(!toml::path(L"test[a1]"));
		CHECK(!toml::path(L"test[1!]"));
		CHECK(!toml::path(L"test[!1]"));
		CHECK(!toml::path(L"test[1 2]"));
		CHECK(!toml::path(L"test[1.2]"));
		CHECK(!toml::path(L"test[0.2]"));

#endif // TOML_ENABLE_WINDOWS_COMPAT
	}

	SECTION("parsing from literal")
	{
		auto p0 = "a.b.c[1][12]"_tpath;
		CHECK(p0);
		CHECK(p0.str() == "a.b.c[1][12]");

		CHECK("ab.cd[1]"_tpath == toml::path("ab.cd[1]"));

		CHECK(("an.invalid.path[a1]"_tpath).str() == "");
	}
}

TEST_CASE("path - manipulating")
{
	SECTION("parent_node and truncation")
	{
		toml::path p0("");
		CHECK(p0.parent().str() == "");

		toml::path p1("start.middle.end");
		CHECK(p1.parent().str() == "start.middle");
		CHECK(p1.parent().parent().str() == "start");
		CHECK(p1.parent().parent().parent().str() == "");
		CHECK(p1.parent().parent().parent().parent().str() == "");

		toml::path p2("[1][2][3]");
		CHECK(p2.parent().str() == "[1][2]");
		CHECK(p2.parent().parent().str() == "[1]");
		CHECK(p2.parent().parent().parent().str() == "");

		toml::path p3(".test");
		CHECK(p3.parent().str() == "");

		toml::path p4("test..");
		CHECK(p4.parent().str() == "test.");
		CHECK(p4.parent().parent().str() == "test");
		CHECK(p4.parent().parent().parent().str() == "");

		toml::path p5("test.key[12].subkey");
		CHECK(p5.parent().str() == "test.key[12]");
		CHECK(p5.parent().parent().str() == "test.key");
		CHECK(p5.parent().parent().parent().str() == "test");
		CHECK(p5.parent().parent().parent().parent().str() == "");

		toml::path p6("test.key1.key2.key3.key4");
		CHECK(p6.truncated(0).str() == "test.key1.key2.key3.key4");
		CHECK(p6.truncated(1).str() == "test.key1.key2.key3");
		CHECK(p6.truncated(4).str() == "test");
		CHECK(p6.truncated(5).str() == "");
		CHECK(p6.truncated(20).str() == "");
		CHECK(p6.str() == "test.key1.key2.key3.key4");

		p6.truncate(0);
		CHECK(p6.str() == "test.key1.key2.key3.key4");
		p6.truncate(2);
		CHECK(p6.str() == "test.key1.key2");
		p6.truncate(3);
		CHECK(p6.str() == "");
	}

	SECTION("subpath")
	{
		toml::path p0("a.simple[1].path[2].object");

		CHECK(p0.subpath(p0.begin() + 1, p0.begin() + 4).str() == "simple[1].path");
		CHECK(p0.subpath(p0.begin() + 1, p0.end() - 1).str() == "simple[1].path[2]");
		CHECK(p0.subpath(p0.begin(), p0.begin()).str() == "");
		CHECK(p0.subpath(p0.begin(), p0.end() - 5).str() == "a");
		CHECK(p0.subpath(p0.begin() + 2, p0.end() - 1).str() == "[1].path[2]");

		CHECK(p0.subpath(p0.begin() + 5, p0.end() - 5).str() == "");
		CHECK(!p0.subpath(p0.end(), p0.begin()));

		CHECK(p0.subpath(1, 4).str() == "simple[1].path[2]");
		CHECK(p0.subpath(0, 0).str() == "");
		CHECK(p0.subpath(2, 0).str() == "");
		CHECK(p0.subpath(2, 1).str() == "[1]");
	}

	SECTION("leaf")
	{
		toml::path p0("one.two.three.four.five");
		CHECK(p0.leaf(0).str() == "");
		CHECK(p0.leaf().str() == "five");
		CHECK(p0.leaf(3).str() == "three.four.five");
		CHECK(p0.leaf(5).str() == "one.two.three.four.five");
		CHECK(p0.leaf(10).str() == "one.two.three.four.five");

		toml::path p1("[10][2][30][4][50]");
		CHECK(p1.leaf(0).str() == "");
		CHECK(p1.leaf().str() == "[50]");
		CHECK(p1.leaf(3).str() == "[30][4][50]");
		CHECK(p1.leaf(5).str() == "[10][2][30][4][50]");
		CHECK(p1.leaf(10).str() == "[10][2][30][4][50]");

		toml::path p2("one[1].two.three[3]");
		CHECK(p2.leaf(0).str() == "");
		CHECK(p2.leaf().str() == "[3]");
		CHECK(p2.leaf(3).str() == "two.three[3]");
		CHECK(p2.leaf(4).str() == "[1].two.three[3]");
		CHECK(p2.leaf(10).str() == "one[1].two.three[3]");
	}

	SECTION("append - string")
	{
		toml::path p0("start");
		CHECK(p0.size() == 1u);
		CHECK(p0.append("middle.end").str() == "start.middle.end");
		CHECK(p0.append("[12]").str() == "start.middle.end[12]");

		toml::path p1("");
		CHECK(p1.size() == 1u);
		p1.append("[1].key"sv);
		CHECK(p1.size() == 3u);
		CHECK(p1.str() == "[1].key"sv);

#if TOML_ENABLE_WINDOWS_COMPAT

		toml::path p2("start");
		CHECK(p2.size() == 1u);
		CHECK(p2.append(L"middle.end").str() == "start.middle.end");
		CHECK(p2.append(L"[12]").str() == "start.middle.end[12]");

		toml::path p3("");
		CHECK(p3.append(L"[1].key").str() == "[1].key");

#endif // TOML_ENABLE_WINDOWS_COMPAT

		toml::path p4;
		CHECK(p4.size() == 0u);
		CHECK(p4.append("[1].key").str() == "[1].key");
	}

	SECTION("append - toml::path copy")
	{
		toml::path p0("start");
		toml::path appendee1("middle.end");
		toml::path appendee2("[12]");
		CHECK(p0.append(appendee1).str() == "start.middle.end");
		CHECK(p0.append(appendee2).str() == "start.middle.end[12]");

		// Ensure copies and not moves
		CHECK(appendee1.str() == "middle.end");
		CHECK(appendee2.str() == "[12]");

		toml::path p1("");
		toml::path appendee3("[1].key");
		CHECK(p1.append(appendee3).str() == "[1].key");

		// Ensure copies and not moves
		CHECK(appendee3.str() == "[1].key");
	}

	SECTION("append - toml::path move")
	{
		toml::path p0("start");
		CHECK(p0.append(toml::path{ "middle.end" }).str() == "start.middle.end");
		CHECK(p0.append(toml::path{ "[12]" }).str() == "start.middle.end[12]");

		toml::path p1("");
		CHECK(p1.size() == 1u);
		CHECK(p1.append(toml::path{ "[1].key" }).str() == "[1].key");

		toml::path p2;
		CHECK(p2.size() == 0u);
		CHECK(p2.append(toml::path{ "[1].key" }).str() == "[1].key");
	}

	SECTION("prepend - string")
	{
		toml::path p0("start");
		CHECK(p0.prepend("middle.end").str() == "middle.end.start");
		CHECK(p0.prepend("[12]").str() == "[12].middle.end.start");

		toml::path p1;
		CHECK(p1.prepend("[1].key").str() == "[1].key");

		toml::path p2("");
		CHECK(p2.prepend("[1].key").str() == "[1].key.");

#if TOML_ENABLE_WINDOWS_COMPAT

		toml::path p3("start");
		CHECK(p3.prepend(L"middle.end").str() == "middle.end.start");
		CHECK(p3.prepend(L"[12]").str() == "[12].middle.end.start");

#endif // TOML_ENABLE_WINDOWS_COMPAT
	}

	SECTION("prepend - toml::path copy")
	{
		toml::path p0("start");
		toml::path prependee1("middle.end");
		toml::path prependee2("[12]");
		CHECK(p0.prepend(prependee1).str() == "middle.end.start");
		CHECK(p0.prepend(prependee2).str() == "[12].middle.end.start");

		// Ensure copies and not moves
		CHECK(prependee1.str() == "middle.end");
		CHECK(prependee2.str() == "[12]");

		toml::path p1;
		toml::path prependee3("[1].key");
		CHECK(p1.prepend(prependee3).str() == "[1].key");

		// Ensure copies and not moves
		CHECK(prependee3.str() == "[1].key");
	}

	SECTION("prepend - toml::path move")
	{
		toml::path p0("start");
		CHECK(p0.prepend(toml::path("middle.end")).str() == "middle.end.start");
		CHECK(p0.prepend(toml::path("[12]")).str() == "[12].middle.end.start");

		toml::path p1;
		CHECK(p1.prepend(toml::path("[1].key")).str() == "[1].key");
	}

	SECTION("alter components")
	{
		toml::path p0("start.mid[1][2].end");

		p0[3] = std::size_t{ 13 };
		CHECK(p0.str() == "start.mid[1][13].end");

		p0[0] = 2u;
		CHECK(p0.str() == "[2].mid[1][13].end");

		p0[0] = 10;
		CHECK(p0.str() == "[10].mid[1][13].end");

		p0[3] = "newkey";
		CHECK(p0.str() == "[10].mid[1].newkey.end");
	}

	SECTION("assign")
	{
		toml::path p0("start.mid[1][2].end");
		p0.assign("test.key[1]");
		CHECK(p0.str() == "test.key[1]");
		p0.assign("");
		CHECK(p0.str() == "");

		toml::path p1("a.test.path[1]");
		p1.assign("invalid[abc]");
		CHECK(!p1);
		CHECK(p1.str() == "");

		toml::path p2("another[1].test.path");
		p2.assign(toml::path("test"));
		CHECK(p2.str() == "test");
		p2.assign(toml::path(""));
		CHECK(p2.str() == "");

		toml::path p3("final.test[1]");
		p3.assign(toml::path("invalid[abc"));
		CHECK(!p3);
		CHECK(p3.str() == "");

#if TOML_ENABLE_WINDOWS_COMPAT

		toml::path p4("start.mid[1][2].end");
		p4.assign(L"test.key[1]");
		CHECK(p4.str() == "test.key[1]");
		p4.assign("");
		CHECK(p4.str() == "");

		toml::path p5("a.test.path[1]");
		p5.assign("invalid[abc]");
		CHECK(!p5);
		CHECK(p5.str() == "");

#endif // TOML_ENABLE_WINDOWS_COMPAT
	}
}

TEST_CASE("path - operators")
{
	SECTION("object equality")
	{
		CHECK(toml::path("a.b.c") == toml::path("a.b.c"));
		CHECK(toml::path("[1].a") == toml::path("[1].a"));

		CHECK(toml::path("a.b.c") != toml::path("a.b"));
		CHECK(toml::path("[1].b") != toml::path("[1].b.c"));
	}

	SECTION("string equality")
	{
		CHECK(toml::path("a.b.c") == "a.b.c");
		CHECK(toml::path("[1].a") == "[1].a");

		CHECK(toml::path("a.b.c") != "a.b");
		CHECK(toml::path("[1].b") != "[1].b.c");

#if TOML_ENABLE_WINDOWS_COMPAT

		CHECK(toml::path("a.b.c") == L"a.b.c");
		CHECK(toml::path("[1].a") == L"[1].a");

		CHECK(toml::path("a.b.c") != L"a.b");
		CHECK(toml::path("[1].b") != L"[1].b.c");

#endif // TOML_ENABLE_WINDOWS_COMPAT
	}

	SECTION("arithmetic")
	{
		CHECK(toml::path("a.b.c") + "a[1]" == "a.b.c.a[1]");
		CHECK((toml::path("a.b.c") + "a[1]") == "a.b.c.a[1]");

		CHECK(toml::path("a.b.c") + toml::path("a[1]") == "a.b.c.a[1]");

		toml::path p1("a.b");
		toml::path p2("c[1]");
		CHECK(p1 + p2 == "a.b.c[1]");

		CHECK(p1 + "c[1]" == "a.b.c[1]");

		CHECK("a.b" + p2 == "a.b.c[1]");

#if TOML_ENABLE_WINDOWS_COMPAT

		CHECK(toml::path("a.b.c") + L"a[1]" == "a.b.c.a[1]");

		CHECK(p1 + L"c[1]" == "a.b.c[1]");

		CHECK(L"a.b" + p2 == "a.b.c[1]");

#endif // TOML_ENABLE_WINDOWS_COMPAT
	}
}

TEST_CASE("path - misc")
{
	CHECK(toml::path{ "" }.str() == "");
	CHECK(toml::path{ "a" }.str() == "a");
	CHECK(toml::path{ "a.b" }.str() == "a.b");
	CHECK(toml::path{ "a.b.c" }.str() == "a.b.c");
	CHECK(toml::path{ ".a.b.c" }.str() == ".a.b.c");

	CHECK(toml::path{}.empty());
	CHECK(!toml::path{ "" }.empty());
	CHECK(!toml::path{ "a" }.empty());

	CHECK(static_cast<std::string>(toml::path("a.b[1]")) == "a.b[1]");
	CHECK(static_cast<bool>(toml::path("a.b[1]")));
	CHECK(!static_cast<bool>(toml::path("a.b[a b]")));

#if TOML_ENABLE_WINDOWS_COMPAT

	CHECK(static_cast<std::wstring>(toml::path("a.b[1]")) == L"a.b[1]");

#endif
}

TEST_CASE("path - accessing")
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
		CHECK(tbl[""] == at_path(tbl, toml::path("")));

		CHECK(tbl["a"]);
		CHECK(tbl["a"] == at_path(tbl, toml::path("a")));
		CHECK(tbl["a"] != at_path(tbl, toml::path(".a"))); // equivalent to ""."a"
		CHECK(!at_path(tbl, toml::path(".a")));

		CHECK(tbl["b"]);
		CHECK(tbl["b"] == at_path(tbl, toml::path("b")));

		CHECK(tbl["b"][0]);
		CHECK(tbl["b"][0] == at_path(tbl, toml::path("b[0]")));
		CHECK(tbl["b"][0] == at_path(tbl, toml::path("b[0]     ")));
		CHECK(tbl["b"][0] == at_path(tbl, toml::path("b[ 0\t]"))); // whitespace is allowed inside array indexer

		CHECK(tbl["b"][1]);
		CHECK(tbl["b"][1] != tbl["b"][0]);
		CHECK(tbl["b"][1] == at_path(tbl, toml::path("b[1]")));

		CHECK(tbl["b"][1][0]);
		CHECK(tbl["b"][1][0] == at_path(tbl, toml::path("b[1][0]")));
		CHECK(tbl["b"][1][0] == at_path(tbl, toml::path("b[1]    \t   [0]"))); // whitespace is allowed after array
																			   // indexers

		CHECK(tbl["b"][2]["c"]);
		CHECK(tbl["b"][2]["c"] == at_path(tbl, toml::path("b[2].c")));
		CHECK(tbl["b"][2]["c"] == at_path(tbl, toml::path("b[2]   \t.c"))); // whitespace is allowed after array
																			// indexers

		CHECK(tbl["d"]);
		CHECK(tbl["d"] == at_path(tbl, toml::path("d")));

		CHECK(tbl["d"]["e"]);
		CHECK(tbl["d"]["e"] == at_path(tbl, toml::path("d.e")));
		CHECK(tbl["d"]["e"] != at_path(tbl, toml::path("d. e"))); // equivalent to "d"." e"
		CHECK(!at_path(tbl, toml::path("d. e")));

		CHECK(tbl["d"][""]);
		CHECK(tbl["d"][""] == at_path(tbl, toml::path("d.")));

		CHECK(!at_path(tbl, toml::path("has.missing.component")));
	}

	SECTION("array")
	{
		// this section uses the node_view member function version of at_path

		auto arr = tbl["b"];

		CHECK(tbl["b"][0]);
		CHECK(tbl["b"][0] == arr.at_path(toml::path("[0]")));
		CHECK(tbl["b"][0] == arr.at_path(toml::path("[0]     ")));
		CHECK(tbl["b"][0] == arr.at_path(toml::path("[ 0\t]"))); // whitespace is allowed inside array indexer

		CHECK(tbl["b"][1]);
		CHECK(tbl["b"][1].node() != arr[0].node());
		CHECK(tbl["b"][1] == arr.at_path(toml::path("[1]")));

		CHECK(tbl["b"][1][0]);
		CHECK(tbl["b"][1][0] == arr.at_path(toml::path("[1][0]")));
		CHECK(tbl["b"][1][0] == arr.at_path(toml::path("[1]    \t   [0]"))); // whitespace is allowed after array
																			 // indexers

		CHECK(tbl["b"][2]["c"]);
		CHECK(tbl["b"][2]["c"] == arr.at_path(toml::path("[2].c")));
		CHECK(tbl["b"][2]["c"] == arr.at_path(toml::path("[2]   \t.c"))); // whitespace is allowed after array indexers

		CHECK(!arr.at_path(toml::path("[3].missing.component")));
	}

	SECTION("indexing operator")
	{
		// this section uses the operator[] of table and node_view
		CHECK(tbl[""]);
		CHECK(tbl[""] == tbl[toml::path("")]);

		CHECK(tbl["a"]);
		CHECK(tbl["a"] == tbl[toml::path("a")]);
		CHECK(tbl["a"] != tbl[toml::path(".a")]); // equivalent to ""."a"
		CHECK(!tbl[toml::path(".a")]);

		CHECK(tbl["b"]);
		CHECK(tbl["b"] == tbl[toml::path("b")]);

		CHECK(tbl["b"][0]);
		CHECK(tbl["b"][0] == tbl[toml::path("b[0]")]);
		CHECK(tbl["b"][0] == tbl[toml::path("b[0]     ")]);
		CHECK(tbl["b"][0] == tbl[toml::path("b[ 0\t]")]); // whitespace is allowed inside array indexer

		CHECK(tbl["b"][1]);
		CHECK(tbl["b"][1] != tbl[toml::path("b")][0]);
		CHECK(tbl["b"][1] == tbl[toml::path("b[1]")]);

		CHECK(tbl["b"][1][0]);
		CHECK(tbl["b"][1][0] == tbl[toml::path("b[1]")][0]);
		CHECK(tbl["b"][1][0] == tbl[toml::path("b[1]    \t   [0]")]); // whitespace is allowed after array
																	  // indexers

		CHECK(tbl["b"][2]["c"]);
		CHECK(tbl["b"][2]["c"] == tbl[toml::path("b")][toml::path("[2].c")]);
		CHECK(tbl["b"][2]["c"] == tbl[toml::path("b[2]   \t.c")]); // whitespace is allowed after array
																   // indexers

		CHECK(tbl["d"]);
		CHECK(tbl["d"] == tbl[toml::path("d")]);

		CHECK(tbl["d"]["e"]);
		CHECK(tbl["d"]["e"] == tbl[toml::path("d.e")]);
		CHECK(tbl["d"]["e"] != tbl[toml::path("d. e")]); // equivalent to "d"." e"
		CHECK(!tbl[toml::path("d. e")]);

		CHECK(tbl["d"][""]);
		CHECK(tbl["d"][""] == tbl[toml::path("d.")]);

		CHECK(!tbl[toml::path("has.missing.component")]);
	}
}
