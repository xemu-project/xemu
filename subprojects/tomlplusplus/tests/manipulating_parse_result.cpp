// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"

#if !TOML_EXCEPTIONS

TEST_CASE("parse_result - good parse")
{
	auto result = "key = true"_toml;
	static_assert(std::is_same_v<decltype(result), parse_result>);
	static_assert(!std::is_same_v<decltype(result), table>);

	REQUIRE(result.succeeded());
	REQUIRE(!result.failed());
	REQUIRE(result);

	REQUIRE(!result.table().empty());
	REQUIRE(result.table().size() == 1);
	REQUIRE(!std::move(result).table().empty());
	REQUIRE(!static_cast<const parse_result&>(result).table().empty());

	REQUIRE(!static_cast<table&>(result).empty());
	REQUIRE(!static_cast<table&&>(result).empty());
	REQUIRE(!static_cast<const table&>(result).empty());

	auto& tbl = static_cast<table&>(result);
	CHECK(tbl["key"sv]);
	CHECK(result["key"sv]);
	CHECK(&result["key"sv].ref<bool>() == &tbl["key"sv].ref<bool>());
	CHECK(result.begin() == tbl.begin());
	CHECK(result.end() == tbl.end());
	CHECK(result.begin() != tbl.end());
	CHECK(result.cbegin() == tbl.cbegin());
	CHECK(result.cend() == tbl.cend());
	CHECK(result.cbegin() != tbl.cend());

	auto& cresult = static_cast<const parse_result&>(result);
	auto& ctbl	  = static_cast<const table&>(cresult);
	CHECK(cresult.begin() == ctbl.begin());
	CHECK(cresult.end() == ctbl.end());
	CHECK(cresult.begin() != ctbl.end());
	CHECK(cresult.cbegin() == ctbl.cbegin());
	CHECK(cresult.cend() == ctbl.cend());
	CHECK(cresult.cbegin() != ctbl.cend());
	CHECK(ctbl["key"sv]);
	CHECK(cresult["key"sv]);
	CHECK(&cresult["key"sv].ref<bool>() == &ctbl["key"sv].ref<bool>());

	size_t tbl_iterations{};
	for (auto&& [k, v] : tbl)
	{
		(void)k;
		(void)v;
		tbl_iterations++;
	}
	size_t result_iterations{};
	for (auto& [k, v] : result)
	{
		(void)k;
		(void)v;
		result_iterations++;
	}
	size_t cresult_iterations{};
	for (auto [k, v] : cresult)
	{
		(void)k;
		(void)v;
		cresult_iterations++;
	}
	CHECK(tbl_iterations == tbl.size());
	CHECK(tbl_iterations == result_iterations);
	CHECK(tbl_iterations == cresult_iterations);
}

TEST_CASE("parse_result - bad parse")
{
	auto result = "key = trUe"_toml;
	static_assert(std::is_same_v<decltype(result), parse_result>);
	static_assert(!std::is_same_v<decltype(result), table>);

	REQUIRE(!result.succeeded());
	REQUIRE(result.failed());
	REQUIRE(!result);

	CHECK(!result["key"sv]);
	CHECK(result.begin() == decltype(result.begin()){});
	CHECK(result.end() == decltype(result.end()){});
	CHECK(result.cbegin() == decltype(result.cbegin()){});
	CHECK(result.cend() == decltype(result.cend()){});

	auto& cresult = static_cast<const parse_result&>(result);
	CHECK(!result["key"sv]);
	CHECK(cresult.begin() == decltype(cresult.begin()){});
	CHECK(cresult.end() == decltype(cresult.end()){});
	CHECK(cresult.cbegin() == decltype(cresult.cbegin()){});
	CHECK(cresult.cend() == decltype(cresult.cend()){});

	for (auto [k, v] : result)
	{
		(void)k;
		(void)v;
		FAIL("This code should not run");
	}
	for (auto [k, v] : cresult)
	{
		(void)k;
		(void)v;
		FAIL("This code should not run");
	}
}

#endif //!TOML_EXCEPTIONS
