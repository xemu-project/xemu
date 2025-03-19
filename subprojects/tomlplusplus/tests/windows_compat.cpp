// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

#if TOML_ENABLE_WINDOWS_COMPAT

TOML_DISABLE_WARNINGS;
#include <windows.h>
TOML_ENABLE_WARNINGS;

TEST_CASE("windows compat")
{
	static constexpr auto toml_text = R"(
		[library]
		name = "toml++"
		authors = ["Mark Gillard <mark.gillard@outlook.com.au>"]
		free = true

		[dependencies]
		cpp = 17
	)"sv;

	auto res = toml::parse(toml_text, L"kek.toml");
#if !TOML_EXCEPTIONS
	REQUIRE(res.succeeded());
#endif
	table& tbl = res;

	// source paths
	REQUIRE(tbl.source().path != nullptr);
	CHECK(*tbl.source().path == "kek.toml"sv);
	CHECK(tbl.source().wide_path().has_value());
	CHECK(tbl.source().wide_path().value() == L"kek.toml"sv);

	// direct lookups from tables
	REQUIRE(tbl.get("library") != nullptr);
	CHECK(tbl.get("library") == tbl.get("library"sv));
	CHECK(tbl.get("library") == tbl.get("library"s));
	CHECK(tbl.get(L"library") != nullptr);
	CHECK(tbl.get(L"library") == tbl.get(L"library"sv));
	CHECK(tbl.get(L"library") == tbl.get(L"library"s));
	CHECK(tbl.get(L"library") == tbl.get("library"));

	// node-view lookups
	CHECK(tbl[L"library"].node() != nullptr);
	CHECK(tbl[L"library"].node() == tbl.get(L"library"));

	// value queries
	REQUIRE(tbl[L"library"][L"name"].as_string() != nullptr);
	CHECK(tbl[L"library"][L"name"].value<std::wstring>() == L"toml++"s);
	CHECK(tbl[L"library"][L"name"].value_or(L""sv) == L"toml++"s);
	CHECK(tbl[L"library"][L"name"].value_or(L""s) == L"toml++"s);
	CHECK(tbl[L"library"][L"name"].value_or(L"") == L"toml++"s);

	// node-view comparisons
	CHECK(tbl[L"library"][L"name"] == "toml++"sv);
	CHECK(tbl[L"library"][L"name"] == "toml++"s);
	CHECK(tbl[L"library"][L"name"] == "toml++");
	CHECK(tbl[L"library"][L"name"] == L"toml++"sv);
	CHECK(tbl[L"library"][L"name"] == L"toml++"s);
	CHECK(tbl[L"library"][L"name"] == L"toml++");

	// table manipulation
	tbl.insert(L"foo", L"bar");
	REQUIRE(tbl.contains("foo"));
	REQUIRE(tbl.contains(L"foo"));
	CHECK(tbl["foo"] == "bar");
	tbl.insert_or_assign(L"foo", L"kek");
	CHECK(tbl["foo"] == "kek");
	tbl.erase(L"foo");
	REQUIRE(!tbl.contains("foo"));
	REQUIRE(!tbl.contains(L"foo"));

	// windows types
	CHECK(tbl[L"library"][L"free"].value<BOOL>() == 1);
	CHECK(tbl[L"dependencies"][L"cpp"].value<BOOL>() == 17);
	CHECK(tbl[L"dependencies"][L"cpp"].value<SHORT>() == 17);
	CHECK(tbl[L"dependencies"][L"cpp"].value<INT>() == 17);
	CHECK(tbl[L"dependencies"][L"cpp"].value<LONG>() == 17);
	CHECK(tbl[L"dependencies"][L"cpp"].value<INT_PTR>() == 17);
	CHECK(tbl[L"dependencies"][L"cpp"].value<LONG_PTR>() == 17);
	CHECK(tbl[L"dependencies"][L"cpp"].value<USHORT>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<UINT>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<ULONG>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<UINT_PTR>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<ULONG_PTR>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<WORD>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<DWORD>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<DWORD32>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<DWORD64>() == 17u);
	CHECK(tbl[L"dependencies"][L"cpp"].value<DWORDLONG>() == 17u);
}

#endif // TOML_ENABLE_WINDOWS_COMPAT
