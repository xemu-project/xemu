// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

TEST_CASE("parsing - tables")
{
	// these are the examples from https://toml.io/en/v1.0.0#table

	// "Tables are defined by headers, with square brackets on a line by themselves."
	parsing_should_succeed(FILE_LINE_ARGS,
						   "[table]"sv,
						   [](table&& tbl)
						   {
							   REQUIRE(tbl["table"].as_table());
							   CHECK(tbl["table"].as_table()->empty());
							   CHECK(tbl["table"].as_table()->size() == 0u);
						   });
	parsing_should_fail(FILE_LINE_ARGS, "[]"sv);

	// "Under that, and until the next header or EOF, are the key/values of that table.
	//  Key/value pairs within tables are not guaranteed to be in any specific order."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								[table-1]
								key1 = "some string"
								key2 = 123

								[table-2]
								key1 = "another string"
								key2 = 456
							)"sv,
						   [](table&& tbl)
						   {
							   REQUIRE(tbl["table-1"].as_table());
							   CHECK(tbl["table-1"].as_table()->size() == 2u);
							   CHECK(tbl["table-1"]["key1"] == "some string"sv);
							   CHECK(tbl["table-1"]["key2"] == 123);

							   REQUIRE(tbl["table-2"].as_table());
							   CHECK(tbl["table-2"].as_table()->size() == 2u);
							   CHECK(tbl["table-2"]["key1"] == "another string"sv);
							   CHECK(tbl["table-2"]["key2"] == 456);
						   });

	// "Naming rules for tables are the same as for keys." (i.e. can be quoted)
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								[dog."tater.man"]
								type.name = "pug"
							)"sv,
						   [](table&& tbl)
						   {
							   REQUIRE(tbl["dog"].as_table());
							   CHECK(tbl["dog"].as_table()->size() == 1u);

							   REQUIRE(tbl["dog"]["tater.man"].as_table());
							   CHECK(tbl["dog"]["tater.man"].as_table()->size() == 1u);
							   CHECK(tbl["dog"]["tater.man"]["type"]["name"] == "pug"sv);
						   });

	// "Whitespace around the key is ignored. However, best practice is to not use any extraneous whitespace."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								[a.b.c]            # this is best practice
								[ d.e.f ]          # same as [d.e.f]
								[ g .  h  . i ]    # same as [g.h.i]
								[ j . "k" . 'l' ]  # same as [j."k".'l']
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["a"].as_table());
							   CHECK(tbl["a"]["b"].as_table());
							   CHECK(tbl["a"]["b"]["c"].as_table());

							   CHECK(tbl["d"].as_table());
							   CHECK(tbl["d"]["e"].as_table());
							   CHECK(tbl["d"]["e"]["f"].as_table());

							   CHECK(tbl["g"].as_table());
							   CHECK(tbl["g"]["h"].as_table());
							   CHECK(tbl["g"]["h"]["i"].as_table());

							   CHECK(tbl["j"].as_table());
							   CHECK(tbl["j"]["k"].as_table());
							   CHECK(tbl["j"]["k"]["l"].as_table());
						   });

	// "You don't need to specify all the super-tables if you don't want to. TOML knows how to do it for you."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# [x] you
								# [x.y] don't
								# [x.y.z] need these
								[x.y.z.w] # for this to work

								[x] # defining a super-table afterwards is ok
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["x"].as_table());
							   CHECK(tbl["x"]["y"].as_table());
							   CHECK(tbl["x"]["y"]["z"].as_table());
							   CHECK(tbl["x"]["y"]["z"]["w"].as_table());
						   });

	// "Like keys, you cannot define a table more than once. Doing so is invalid."
	parsing_should_fail(FILE_LINE_ARGS, R"(
		# DO NOT DO THIS

		[fruit]
		apple = "red"

		[fruit]
		orange = "orange"
	)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		# DO NOT DO THIS EITHER

		[fruit]
		apple = "red"

		[fruit.apple]
		texture = "smooth"
	)"sv);

	// "Defining tables out-of-order is discouraged."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# VALID BUT DISCOURAGED
								[fruit.apple]
								[animal]
								[fruit.orange]
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["fruit"].as_table());
							   CHECK(tbl["fruit"]["apple"].as_table());
							   CHECK(tbl["animal"].as_table());
							   CHECK(tbl["fruit"]["orange"].as_table());
						   });
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# RECOMMENDED
								[fruit.apple]
								[fruit.orange]
								[animal]
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["fruit"].as_table());
							   CHECK(tbl["fruit"]["apple"].as_table());
							   CHECK(tbl["fruit"]["orange"].as_table());
							   CHECK(tbl["animal"].as_table());
						   });

	// "The top-level table, also called the root table, starts at the beginning of the document
	//  and ends just before the first table header (or EOF)."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# Top-level table begins.
								name = "Fido"
								breed = "pug"

								# Top-level table ends.
								[owner]
								name = "Regina Dogman"
								member_since = 1999-08-04
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["name"].as_string());
							   CHECK(*tbl["name"].as_string() == "Fido"sv);
							   CHECK(tbl["breed"].as_string());
							   CHECK(*tbl["breed"].as_string() == "pug"sv);

							   CHECK(tbl["owner"].as_table());
							   CHECK(*tbl["owner"]["name"].as_string() == "Regina Dogman"sv);

							   static constexpr auto member_since = toml::date{ 1999, 8, 4 };
							   CHECK(*tbl["owner"]["member_since"].as_date() == member_since);
						   });

	// "Dotted keys create and define a table for each key part before the last one,
	//  provided that such tables were not previously created."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								fruit.apple.color = "red"
								# Defines a table named fruit
								# Defines a table named fruit.apple

								fruit.apple.taste.sweet = true
								# Defines a table named fruit.apple.taste
								# fruit and fruit.apple were already created
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["fruit"].as_table());
							   CHECK(tbl["fruit"]["apple"].as_table());
							   CHECK(tbl["fruit"]["apple"]["color"].as_string());
							   CHECK(*tbl["fruit"]["apple"]["color"].as_string() == "red"sv);

							   CHECK(tbl["fruit"]["apple"]["taste"].as_table());
							   CHECK(tbl["fruit"]["apple"]["taste"]["sweet"].as_boolean());
							   CHECK(*tbl["fruit"]["apple"]["taste"]["sweet"].as_boolean() == true);
						   });

	// "Since tables cannot be defined more than once, redefining such tables using a [table] header is not allowed."
	parsing_should_fail(FILE_LINE_ARGS, R"(
		[fruit]
		apple.color = "red"
		apple.taste.sweet = true

		[fruit.apple]  # INVALID
	)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		[fruit]
		apple.color = "red"
		apple.taste.sweet = true

		[fruit.apple.taste]  # INVALID
	)"sv);

	// "Likewise, using dotted keys to redefine tables already defined in [table] form is not allowed."
	parsing_should_fail(FILE_LINE_ARGS, R"(
		[fruit.apple.taste]
		sweet = true

		[fruit]
		apple.taste = { sweet = false }  # INVALID
	)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		[fruit.apple.taste]
		sweet = true

		[fruit]
		apple.taste.foo = "bar"  # INVALID
	)"sv);

	// "The [table] form can, however, be used to define sub-tables within tables defined via dotted keys."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								[fruit]
								apple.color = "red"
								apple.taste.sweet = true

								[fruit.apple.texture]  # you can add sub-tables
								smooth = true
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["fruit"].as_table());
							   CHECK(tbl["fruit"]["apple"].as_table());
							   CHECK(tbl["fruit"]["apple"]["color"].as_string());
							   CHECK(*tbl["fruit"]["apple"]["color"].as_string() == "red"sv);

							   CHECK(tbl["fruit"]["apple"]["texture"].as_table());
							   CHECK(tbl["fruit"]["apple"]["texture"]["smooth"].as_boolean());
							   CHECK(*tbl["fruit"]["apple"]["texture"]["smooth"].as_boolean() == true);
						   });
	parsing_should_fail(FILE_LINE_ARGS, R"(
		[fruit]
		apple.color = "red"
		apple.taste.sweet = true

		[fruit.apple]
		shape = "round"

		[fruit.apple.texture]
		smooth = true
	)"sv);

	// same as above but the table order is reversed.
	// see: https://github.com/toml-lang/toml/issues/769
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								[fruit.apple.texture]
								smooth = true

								[fruit]
								apple.color = "red"
								apple.taste.sweet = true
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["fruit"].as_table());
							   CHECK(tbl["fruit"]["apple"].as_table());
							   CHECK(tbl["fruit"]["apple"]["color"].as_string());
							   CHECK(*tbl["fruit"]["apple"]["color"].as_string() == "red"sv);

							   CHECK(tbl["fruit"]["apple"]["texture"].as_table());
							   CHECK(tbl["fruit"]["apple"]["texture"]["smooth"].as_boolean());
							   CHECK(*tbl["fruit"]["apple"]["texture"]["smooth"].as_boolean() == true);
						   });
}

TEST_CASE("parsing - inline tables")
{
	// these are the examples from https://toml.io/en/v1.0.0#inline-table

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								name = { first = "Tom", last = "Preston-Werner" }
								point = { x = 1, y = 2 }
								animal = { type.name = "pug" }
							)"sv,
						   [](table&& tbl)
						   {
							   REQUIRE(tbl["name"].as_table());
							   CHECK(tbl["name"].as_table()->size() == 2u);
							   CHECK(tbl["name"].as_table()->is_inline());
							   CHECK(tbl["name"]["first"] == "Tom"sv);
							   CHECK(tbl["name"]["last"] == "Preston-Werner"sv);

							   REQUIRE(tbl["point"].as_table());
							   CHECK(tbl["point"].as_table()->size() == 2u);
							   CHECK(tbl["point"].as_table()->is_inline());
							   CHECK(tbl["point"]["x"] == 1);
							   CHECK(tbl["point"]["y"] == 2);

							   REQUIRE(tbl["animal"].as_table());
							   CHECK(tbl["animal"].as_table()->size() == 1u);
							   CHECK(tbl["animal"].as_table()->is_inline());
							   REQUIRE(tbl["animal"]["type"].as_table());
							   CHECK(tbl["animal"]["type"].as_table()->size() == 1u);
							   CHECK(tbl["animal"]["type"]["name"] == "pug"sv);
						   });

	// "Inline tables are fully self-contained and define all keys and sub-tables within them.
	//  Keys and sub-tables cannot be added outside the braces."
	parsing_should_fail(FILE_LINE_ARGS, R"(
		[product]
		type = { name = "Nail" }
		type.edible = false  # INVALID
	)"sv);

	// "Similarly, inline tables cannot be used to add keys or sub-tables to an already-defined table."
	parsing_should_fail(FILE_LINE_ARGS, R"(
		[product]
		type.name = "Nail"
		type = { edible = false }  # INVALID
	)"sv);

	// "newlines are allowed between the curly braces [if] they are valid within a value."
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								test = { val1 = "foo", val2 = [
									1, 2,
									3
								], val3 = "bar" }
							)"sv,
						   [](table&& tbl)
						   {
							   REQUIRE(tbl["test"].as_table());
							   CHECK(tbl["test"].as_table()->size() == 3u);
							   CHECK(tbl["test"]["val1"] == "foo"sv);
							   REQUIRE(tbl["test"]["val2"].as<array>());
							   CHECK(tbl["test"]["val2"].as<array>()->size() == 3u);
							   CHECK(tbl["test"]["val2"][0] == 1);
							   CHECK(tbl["test"]["val2"][1] == 2);
							   CHECK(tbl["test"]["val2"][2] == 3);
							   CHECK(tbl["test"]["val3"] == "bar"sv);
						   });

// toml/issues/516 (newlines/trailing commas in inline tables)
#if TOML_LANG_UNRELEASED
	{
		parsing_should_succeed(FILE_LINE_ARGS,
							   R"(
									name = {
										first = "Tom",
										last = "Preston-Werner",
									}
								)"sv,
							   [](table&& tbl)
							   {
								   REQUIRE(tbl["name"].as_table());
								   CHECK(tbl["name"].as_table()->size() == 2u);
								   CHECK(tbl["name"]["first"] == "Tom"sv);
								   CHECK(tbl["name"]["last"] == "Preston-Werner"sv);
							   });
	}
#else
	{
		// "A terminating comma (also called trailing comma) is not permitted after the last key/value pair in an inline
		// table."
		parsing_should_fail(FILE_LINE_ARGS, R"(name = { first = "Tom", last = "Preston-Werner", })"sv);

		// "No newlines are allowed between the curly braces unless they are valid within a value."
		parsing_should_fail(FILE_LINE_ARGS, R"(
		name = {
			first = "Tom",
			last = "Preston-Werner"
		}
		)"sv);
	}
#endif
}

TEST_CASE("parsing - arrays-of-tables")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
							points = [ { x = 1, y = 2, z = 3 },
									   { x = 7, y = 8, z = 9 },
									   { x = 2, y = 4, z = 8 } ]

							[[products]]
							name = "Hammer"
							sku = 738594937

							[[products]]

							[[products]]
							name = "Nail"
							sku = 284758393

							color = "gray"

							[[fruit]]
							  name = "apple"

							  [fruit.physical]  # subtable
								color = "red"
								shape = "round"

							  [[fruit.variety]]  # nested array of tables
								name = "red delicious"

							  [[fruit.variety]]
								name = "granny smith"

							[[fruit]]
							  name = "banana"

							  [[fruit.variety]]
								name = "plantain"

							)"sv,
						   [](table&& tbl)
						   {
							   REQUIRE(tbl["points"].as<array>());
							   CHECK(tbl["points"].as<array>()->size() == 3u);
							   CHECK(tbl["points"].as<array>()->is_homogeneous());
							   CHECK(tbl["points"].as<array>()->is_array_of_tables());
							   CHECK(tbl["points"][0]["x"] == 1);
							   CHECK(tbl["points"][0]["y"] == 2);
							   CHECK(tbl["points"][0]["z"] == 3);
							   CHECK(tbl["points"][1]["x"] == 7);
							   CHECK(tbl["points"][1]["y"] == 8);
							   CHECK(tbl["points"][1]["z"] == 9);
							   CHECK(tbl["points"][2]["x"] == 2);
							   CHECK(tbl["points"][2]["y"] == 4);
							   CHECK(tbl["points"][2]["z"] == 8);

							   REQUIRE(tbl["products"].as<array>());
							   CHECK(tbl["products"].as<array>()->size() == 3u);
							   CHECK(tbl["products"].as<array>()->is_homogeneous());
							   CHECK(tbl["products"].as<array>()->is_array_of_tables());

							   REQUIRE(tbl["products"][0].as_table());
							   CHECK(tbl["products"][0].as_table()->size() == 2u);
							   CHECK(tbl["products"][0]["name"] == "Hammer"sv);
							   CHECK(tbl["products"][0]["sku"] == 738594937);

							   REQUIRE(tbl["products"][1].as_table());
							   CHECK(tbl["products"][1].as_table()->size() == 0u);

							   REQUIRE(tbl["products"][2].as_table());
							   CHECK(tbl["products"][2].as_table()->size() == 3u);
							   CHECK(tbl["products"][2]["name"] == "Nail"sv);
							   CHECK(tbl["products"][2]["sku"] == 284758393);
							   CHECK(tbl["products"][2]["color"] == "gray"sv);

							   REQUIRE(tbl["fruit"].as<array>());
							   CHECK(tbl["fruit"].as<array>()->size() == 2u);
							   CHECK(tbl["fruit"].as<array>()->is_homogeneous());
							   CHECK(tbl["fruit"].as<array>()->is_array_of_tables());

							   REQUIRE(tbl["fruit"][0].as_table());
							   CHECK(tbl["fruit"][0].as_table()->size() == 3u);
							   CHECK(tbl["fruit"][0]["name"] == "apple"sv);

							   REQUIRE(tbl["fruit"][0]["physical"].as_table());
							   CHECK(tbl["fruit"][0]["physical"].as_table()->size() == 2u);
							   CHECK(tbl["fruit"][0]["physical"]["color"] == "red"sv);
							   CHECK(tbl["fruit"][0]["physical"]["shape"] == "round"sv);

							   REQUIRE(tbl["fruit"][0]["variety"].as<array>());
							   CHECK(tbl["fruit"][0]["variety"].as<array>()->size() == 2u);
							   CHECK(tbl["fruit"][0]["variety"].as<array>()->is_homogeneous());
							   CHECK(tbl["fruit"][0]["variety"].as<array>()->is_array_of_tables());
							   CHECK(tbl["fruit"][0]["variety"][0]["name"] == "red delicious"sv);
							   CHECK(tbl["fruit"][0]["variety"][1]["name"] == "granny smith"sv);

							   REQUIRE(tbl["fruit"][1].as_table());
							   CHECK(tbl["fruit"][1].as_table()->size() == 2u);
							   CHECK(tbl["fruit"][1]["name"] == "banana"sv);

							   REQUIRE(tbl["fruit"][1]["variety"].as<array>());
							   CHECK(tbl["fruit"][1]["variety"].as<array>()->size() == 1u);
							   CHECK(tbl["fruit"][1]["variety"].as<array>()->is_homogeneous());
							   CHECK(tbl["fruit"][1]["variety"].as<array>()->is_array_of_tables());
							   CHECK(tbl["fruit"][1]["variety"][0]["name"] == "plantain"sv);
						   });

	parsing_should_fail(FILE_LINE_ARGS, R"(
# INVALID TOML DOC
[fruit.physical]  # subtable, but to which parent element should it belong?
  color = "red"
  shape = "round"

[[fruit]]  # parser must throw an error upon discovering that "fruit" is
           # an array rather than a table
  name = "apple"
)"sv);

	parsing_should_fail(FILE_LINE_ARGS, R"(
# INVALID TOML DOC
fruit = []

[[fruit]] # Not allowed
)"sv);

	parsing_should_fail(FILE_LINE_ARGS, R"(
# INVALID TOML DOC
[[fruit]]
  name = "apple"

  [[fruit.variety]]
    name = "red delicious"

  # INVALID: This table conflicts with the previous array of tables
  [fruit.variety]
    name = "granny smith"
)"sv);

	parsing_should_fail(FILE_LINE_ARGS, R"(
# INVALID TOML DOC
[[fruit]]
  name = "apple"

  [fruit.physical]
    color = "red"
    shape = "round"

  # INVALID: This array of tables conflicts with the previous table
  [[fruit.physical]]
    color = "green"
)"sv);
}

TEST_CASE("parsing - keys")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
[a.b]
c = "10.0.0.1"
d = "frontend"
e = { f.g = 79.5, h = 72.0 }
							)"sv,
						   [](table&& tbl)
						   {
							   // ensure types are sane first
							   REQUIRE(tbl["a"].is_table());
							   REQUIRE(tbl["a"]["b"].is_table());
							   REQUIRE(tbl["a"]["b"]["c"]);
							   REQUIRE(tbl["a"]["b"]["d"]);
							   REQUIRE(tbl["a"]["b"]["e"].is_table());
							   REQUIRE(tbl["a"]["b"]["e"]["f"].is_table());
							   REQUIRE(tbl["a"]["b"]["e"]["f"]["g"]);
							   REQUIRE(tbl["a"]["b"]["e"]["h"]);

							   const auto check_key =
								   [&](const auto& t, std::string_view k, source_position b, source_position e)
							   {
								   const toml::key& found_key = t.as_table()->find(k)->first;
								   CHECK(found_key.str() == k);
								   CHECK(found_key.source().begin == b);
								   CHECK(found_key.source().end == e);
								   CHECK(found_key.source().path == tbl.source().path);
							   };

							   check_key(tbl, "a", { 2, 2 }, { 2, 3 });
							   check_key(tbl["a"], "b", { 2, 4 }, { 2, 5 });
							   check_key(tbl["a"]["b"], "c", { 3, 1 }, { 3, 2 });
							   check_key(tbl["a"]["b"], "d", { 4, 1 }, { 4, 2 });
							   check_key(tbl["a"]["b"], "e", { 5, 1 }, { 5, 2 });
							   check_key(tbl["a"]["b"]["e"], "f", { 5, 7 }, { 5, 8 });
							   check_key(tbl["a"]["b"]["e"]["f"], "g", { 5, 9 }, { 5, 10 });
							   check_key(tbl["a"]["b"]["e"], "h", { 5, 19 }, { 5, 20 });
						   });
}
