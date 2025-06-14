// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.hpp"
TOML_DISABLE_SPAM_WARNINGS;

TEST_CASE("parsing - TOML spec example")
{
	static constexpr auto toml_text = R"(
		# This is a TOML document.

		title = "TOML Example"

		[owner]
		name = "Tom Preston-Werner"
		dob = 1979-05-27T07:32:00-08:00 # First class dates

		[database]
		server = "192.168.1.1"
		ports = [ 8001, 8001, 8002 ]
		connection_max = 5000
		enabled = true

		[servers]

		  # Indentation (tabs and/or spaces) is allowed but not required
		  [servers.alpha]
		  ip = "10.0.0.1"
		  dc = "eqdc10"

		  [servers.beta]
		  ip = "10.0.0.2"
		  dc = "eqdc10"

		[clients]
		data = [ ["gamma", "delta"], [1, 2] ]

		# Line breaks are OK when inside arrays
		hosts = [
		  "alpha",
		  "omega"
		]
	)"sv;

	parsing_should_succeed(FILE_LINE_ARGS,
						   toml_text,
						   [](table&& tbl)
						   {
							   CHECK(tbl.size() == 5);

							   CHECK(tbl["title"] == "TOML Example"sv);

							   CHECK(tbl["owner"]);
							   CHECK(tbl["owner"].as<table>());
							   CHECK(tbl["owner"]["name"] == "Tom Preston-Werner"sv);
							   const auto dob = date_time{ { 1979, 5, 27 }, { 7, 32 }, { -8, 0 } };
							   CHECK(tbl["owner"]["dob"] == dob);

							   CHECK(tbl["database"].as<table>());
							   CHECK(tbl["database"]["server"] == "192.168.1.1"sv);
							   const auto ports = { 8001, 8001, 8002 };
							   CHECK(tbl["database"]["ports"] == ports);
							   CHECK(tbl["database"]["connection_max"] == 5000);
							   CHECK(tbl["database"]["enabled"] == true);

							   CHECK(tbl["servers"].as<table>());
							   CHECK(tbl["servers"]["alpha"].as<table>());
							   CHECK(tbl["servers"]["alpha"]["ip"] == "10.0.0.1"sv);
							   CHECK(tbl["servers"]["alpha"]["dc"] == "eqdc10"sv);
							   CHECK(tbl["servers"]["beta"].as<table>());
							   CHECK(tbl["servers"]["beta"]["ip"] == "10.0.0.2"sv);
							   CHECK(tbl["servers"]["beta"]["dc"] == "eqdc10"sv);

							   CHECK(tbl["clients"].as<table>());
							   REQUIRE(tbl["clients"]["data"].as<array>());
							   CHECK(tbl["clients"]["data"].as<array>()->size() == 2);
							   REQUIRE(tbl["clients"]["data"][0].as<array>());
							   CHECK(tbl["clients"]["data"][0].as<array>()->size() == 2);
							   CHECK(tbl["clients"]["data"][0][0] == "gamma"sv);
							   CHECK(tbl["clients"]["data"][0][1] == "delta"sv);
							   REQUIRE(tbl["clients"]["data"][1].as<array>());
							   CHECK(tbl["clients"]["data"][1].as<array>()->size() == 2);
							   CHECK(tbl["clients"]["data"][1][0] == 1);
							   CHECK(tbl["clients"]["data"][1][1] == 2);
							   REQUIRE(tbl["clients"]["hosts"].as<array>());
							   CHECK(tbl["clients"]["hosts"].as<array>()->size() == 2);
							   CHECK(tbl["clients"]["hosts"][0] == "alpha"sv);
							   CHECK(tbl["clients"]["hosts"][1] == "omega"sv);
						   });
}
