// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

TEST_CASE("parsing - arrays")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								integers = [ 1, 2, 3 ]
								integers2 = [
									1, 2, 3
								]
								integers3 = [
									1,
									2, # this is ok
								]
								colors = [ "red", "yellow", "green" ]
								nested_array_of_int = [ [ 1, 2 ], [3, 4, 5] ]
								nested_mixed_array = [ [ 1, 2 ], ["a", "b", "c"] ]
								string_array = [ "all", 'strings', """are the same""", '''type''' ]
							)"sv,
						   [](table&& tbl)
						   {
							   REQUIRE(tbl["integers"].as<array>());
							   CHECK(tbl["integers"].is_homogeneous());
							   CHECK(tbl["integers"].is_homogeneous(node_type::integer));
							   CHECK(!tbl["integers"].is_homogeneous(node_type::floating_point));
							   CHECK(tbl["integers"].is_homogeneous<int64_t>());
							   CHECK(!tbl["integers"].is_homogeneous<double>());
							   CHECK(tbl["integers"].as<array>()->is_homogeneous());
							   CHECK(tbl["integers"].as<array>()->is_homogeneous(node_type::integer));
							   CHECK(!tbl["integers"].as<array>()->is_homogeneous(node_type::floating_point));
							   CHECK(tbl["integers"].as<array>()->is_homogeneous<int64_t>());
							   CHECK(!tbl["integers"].as<array>()->is_homogeneous<double>());
							   CHECK(tbl["integers"].as<array>()->size() == 3);
							   CHECK(tbl["integers"][0] == 1);
							   CHECK(tbl["integers"][1] == 2);
							   CHECK(tbl["integers"][2] == 3);

							   REQUIRE(tbl["integers2"].as<array>());
							   CHECK(tbl["integers2"].is_homogeneous());
							   CHECK(tbl["integers2"].is_homogeneous(node_type::integer));
							   CHECK(!tbl["integers2"].is_homogeneous(node_type::floating_point));
							   CHECK(tbl["integers2"].is_homogeneous<int64_t>());
							   CHECK(!tbl["integers2"].is_homogeneous<double>());
							   CHECK(tbl["integers2"].as<array>()->is_homogeneous());
							   CHECK(tbl["integers2"].as<array>()->is_homogeneous(node_type::integer));
							   CHECK(!tbl["integers2"].as<array>()->is_homogeneous(node_type::floating_point));
							   CHECK(tbl["integers2"].as<array>()->is_homogeneous<int64_t>());
							   CHECK(!tbl["integers2"].as<array>()->is_homogeneous<double>());
							   CHECK(tbl["integers2"].as<array>()->size() == 3);
							   CHECK(tbl["integers2"][0] == 1);
							   CHECK(tbl["integers2"][1] == 2);
							   CHECK(tbl["integers2"][2] == 3);

							   REQUIRE(tbl["integers3"].as<array>());
							   CHECK(tbl["integers3"].is_homogeneous());
							   CHECK(tbl["integers3"].is_homogeneous(node_type::integer));
							   CHECK(!tbl["integers3"].is_homogeneous(node_type::floating_point));
							   CHECK(tbl["integers3"].is_homogeneous<int64_t>());
							   CHECK(!tbl["integers3"].is_homogeneous<double>());
							   CHECK(tbl["integers3"].as<array>()->is_homogeneous());
							   CHECK(tbl["integers3"].as<array>()->is_homogeneous(node_type::integer));
							   CHECK(!tbl["integers3"].as<array>()->is_homogeneous(node_type::floating_point));
							   CHECK(tbl["integers3"].as<array>()->is_homogeneous<int64_t>());
							   CHECK(!tbl["integers3"].as<array>()->is_homogeneous<double>());
							   CHECK(tbl["integers3"].as<array>()->size() == 2);
							   CHECK(tbl["integers3"][0] == 1);
							   CHECK(tbl["integers3"][1] == 2);

							   REQUIRE(tbl["colors"].as<array>());
							   CHECK(tbl["colors"].is_homogeneous());
							   CHECK(tbl["colors"].is_homogeneous(node_type::string));
							   CHECK(!tbl["colors"].is_homogeneous(node_type::floating_point));
							   CHECK(tbl["colors"].is_homogeneous<std::string>());
							   CHECK(!tbl["colors"].is_homogeneous<double>());
							   CHECK(tbl["colors"].as<array>()->is_homogeneous());
							   CHECK(tbl["colors"].as<array>()->is_homogeneous(node_type::string));
							   CHECK(!tbl["colors"].as<array>()->is_homogeneous(node_type::floating_point));
							   CHECK(tbl["colors"].as<array>()->is_homogeneous<std::string>());
							   CHECK(!tbl["colors"].as<array>()->is_homogeneous<double>());
							   CHECK(tbl["colors"].as<array>()->size() == 3);
							   CHECK(tbl["colors"][0] == "red"sv);
							   CHECK(tbl["colors"][1] == "yellow"sv);
							   CHECK(tbl["colors"][2] == "green"sv);

							   REQUIRE(tbl["nested_array_of_int"].as<array>());
							   CHECK(tbl["nested_array_of_int"].as<array>()->is_homogeneous());
							   CHECK(tbl["nested_array_of_int"].as<array>()->size() == 2);
							   REQUIRE(tbl["nested_array_of_int"][0].as<array>());
							   CHECK(tbl["nested_array_of_int"][0].as<array>()->is_homogeneous());
							   CHECK(tbl["nested_array_of_int"][0].as<array>()->size() == 2);
							   CHECK(tbl["nested_array_of_int"][0][0] == 1);
							   CHECK(tbl["nested_array_of_int"][0][1] == 2);
							   REQUIRE(tbl["nested_array_of_int"][1].as<array>());
							   CHECK(tbl["nested_array_of_int"][1].as<array>()->is_homogeneous());
							   CHECK(tbl["nested_array_of_int"][1].as<array>()->size() == 3);
							   CHECK(tbl["nested_array_of_int"][1][0] == 3);
							   CHECK(tbl["nested_array_of_int"][1][1] == 4);
							   CHECK(tbl["nested_array_of_int"][1][2] == 5);

							   REQUIRE(tbl["nested_mixed_array"].as<array>());
							   CHECK(tbl["nested_mixed_array"].as<array>()->is_homogeneous());
							   CHECK(tbl["nested_mixed_array"].as<array>()->size() == 2);
							   REQUIRE(tbl["nested_mixed_array"][0].as<array>());
							   CHECK(tbl["nested_mixed_array"][0].as<array>()->is_homogeneous());
							   CHECK(tbl["nested_mixed_array"][0].as<array>()->size() == 2);
							   CHECK(tbl["nested_mixed_array"][0][0] == 1);
							   CHECK(tbl["nested_mixed_array"][0][1] == 2);
							   REQUIRE(tbl["nested_mixed_array"][1].as<array>());
							   CHECK(tbl["nested_mixed_array"][1].as<array>()->is_homogeneous());
							   CHECK(tbl["nested_mixed_array"][1].as<array>()->size() == 3);
							   CHECK(tbl["nested_mixed_array"][1][0] == "a"sv);
							   CHECK(tbl["nested_mixed_array"][1][1] == "b"sv);
							   CHECK(tbl["nested_mixed_array"][1][2] == "c"sv);

							   REQUIRE(tbl["string_array"].as<array>());
							   CHECK(tbl["string_array"].as<array>()->is_homogeneous());
							   CHECK(tbl["string_array"].as<array>()->size() == 4);
							   CHECK(tbl["string_array"][0] == "all"sv);
							   CHECK(tbl["string_array"][1] == "strings"sv);
							   CHECK(tbl["string_array"][2] == "are the same"sv);
							   CHECK(tbl["string_array"][3] == "type"sv);
							   REQUIRE(tbl["integers"].as<array>());
							   CHECK(tbl["integers"].as<array>()->is_homogeneous());
							   CHECK(tbl["integers"].as<array>()->size() == 3);
							   CHECK(tbl["integers"][0] == 1);
							   CHECK(tbl["integers"][1] == 2);
							   CHECK(tbl["integers"][2] == 3);
						   });

// toml/issues/665 (heterogeneous arrays)
#if TOML_LANG_AT_LEAST(1, 0, 0)

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# Mixed-type arrays are allowed
								numbers = [ 0.1, 0.2, 0.5, 1, 2, 5 ]
								contributors = [
								  "Foo Bar <foo@example.com>",
								  { name = "Baz Qux", email = "bazqux@example.com", url = "https://example.com/bazqux" }
								]
							)"sv,
						   [](table&& tbl)
						   {
							   REQUIRE(tbl["numbers"].as<array>());
							   CHECK(!tbl["numbers"].as<array>()->is_homogeneous());
							   CHECK(tbl["numbers"].as<array>()->size() == 6);
							   CHECK(tbl["numbers"][0].as<double>());
							   CHECK(tbl["numbers"][1].as<double>());
							   CHECK(tbl["numbers"][2].as<double>());
							   CHECK(tbl["numbers"][3].as<int64_t>());
							   CHECK(tbl["numbers"][4].as<int64_t>());
							   CHECK(tbl["numbers"][5].as<int64_t>());
							   CHECK(tbl["numbers"][0] == 0.1);
							   CHECK(tbl["numbers"][1] == 0.2);
							   CHECK(tbl["numbers"][2] == 0.5);
							   CHECK(tbl["numbers"][3] == 1);
							   CHECK(tbl["numbers"][4] == 2);
							   CHECK(tbl["numbers"][5] == 5);

							   REQUIRE(tbl["contributors"].as<array>());
							   CHECK(!tbl["contributors"].as<array>()->is_homogeneous());
							   CHECK(tbl["contributors"].as<array>()->size() == 2);
							   CHECK(tbl["contributors"][0].as<std::string>());
							   CHECK(tbl["contributors"][1].as<table>());
							   CHECK(tbl["contributors"][0] == "Foo Bar <foo@example.com>"sv);
							   CHECK(tbl["contributors"][1]["name"] == "Baz Qux"sv);
							   CHECK(tbl["contributors"][1]["email"] == "bazqux@example.com"sv);
							   CHECK(tbl["contributors"][1]["url"] == "https://example.com/bazqux"sv);
						   });

#else

	parsing_should_fail(FILE_LINE_ARGS, "numbers = [ 0.1, 0.2, 0.5, 1, 2, 5 ]"sv);

#endif
}
