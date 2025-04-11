// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

TEST_CASE("parsing - key-value pairs")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								key = "value"
								bare_key = "value"
								bare-key = "value"
								1234 = "value"
								"" = "blank"
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl.size() == 5);
							   CHECK(tbl["key"] == "value"sv);
							   CHECK(tbl["bare_key"] == "value"sv);
							   CHECK(tbl["bare-key"] == "value"sv);
							   CHECK(tbl["1234"] == "value"sv);
							   CHECK(tbl[""] == "blank"sv);
						   });

	parsing_should_fail(FILE_LINE_ARGS, R"(key = # INVALID)"sv);

#if UNICODE_LITERALS_OK
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								"127.0.0.1" = "value"
								"character encoding" = "value"
								"ʎǝʞ" = "value"
								'key2' = "value"
								'quoted "value"' = "value"
								'' = 'blank'
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["127.0.0.1"] == "value"sv);
							   CHECK(tbl["character encoding"] == "value"sv);
							   CHECK(tbl["ʎǝʞ"] == "value"sv);
							   CHECK(tbl["key2"] == "value"sv);
							   CHECK(tbl["quoted \"value\""] == "value"sv);
							   CHECK(tbl[""] == "blank"sv);
						   });
#endif // UNICODE_LITERALS_OK

	parsing_should_fail(FILE_LINE_ARGS, R"(= "no key name")"sv);

	parsing_should_fail(FILE_LINE_ARGS, R"(
		# DO NOT DO THIS
		name = "Tom"
		name = "Pradyun"
	)"sv);
}

TEST_CASE("parsing - key-value pairs (dotted)")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								name = "Orange"
								physical.color = "orange"
								physical.shape = "round"
								site."google.com" = true
								3.14159 = "pi"
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl.size() == 4);
							   CHECK(tbl["name"] == "Orange"sv);
							   CHECK(tbl["physical"]["color"] == "orange"sv);
							   CHECK(tbl["physical"]["shape"] == "round"sv);
							   CHECK(tbl["site"]["google.com"] == true);
							   CHECK(tbl["3"]["14159"] == "pi"sv);
						   });

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								fruit.apple.smooth = true
								fruit.orange = 2
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["fruit"]["apple"]["smooth"] == true);
							   CHECK(tbl["fruit"]["orange"] == 2);
						   });

	parsing_should_fail(FILE_LINE_ARGS, R"(
		# THIS IS INVALID
		fruit.apple = 1
		fruit.apple.smooth = true
	)"sv);

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# VALID BUT DISCOURAGED

								apple.type = "fruit"
								orange.type = "fruit"

								apple.skin = "thin"
								orange.skin = "thick"

								apple.color = "red"
								orange.color = "orange"
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["apple"]["type"] == "fruit"sv);
							   CHECK(tbl["apple"]["skin"] == "thin"sv);
							   CHECK(tbl["apple"]["color"] == "red"sv);
							   CHECK(tbl["orange"]["type"] == "fruit"sv);
							   CHECK(tbl["orange"]["skin"] == "thick"sv);
							   CHECK(tbl["orange"]["color"] == "orange"sv);
						   });

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								# RECOMMENDED

								apple.type = "fruit"
								apple.skin = "thin"
								apple.color = "red"

								orange.type = "fruit"
								orange.skin = "thick"
								orange.color = "orange"
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["apple"]["type"] == "fruit"sv);
							   CHECK(tbl["apple"]["skin"] == "thin"sv);
							   CHECK(tbl["apple"]["color"] == "red"sv);
							   CHECK(tbl["orange"]["type"] == "fruit"sv);
							   CHECK(tbl["orange"]["skin"] == "thick"sv);
							   CHECK(tbl["orange"]["color"] == "orange"sv);
						   });

// toml/issues/644 ('+' in bare keys)
#if TOML_LANG_UNRELEASED
	parsing_should_succeed(FILE_LINE_ARGS, "key+1 = 0"sv, [](table&& tbl) { CHECK(tbl["key+1"] == 0); });
#else
	parsing_should_fail(FILE_LINE_ARGS, "key+1 = 0"sv);
#endif

// toml/pull/891 (unicode bare keys)
// clang-format off
#if UNICODE_LITERALS_OK
#if TOML_LANG_UNRELEASED
	parsing_should_succeed(FILE_LINE_ARGS,	R"(ʎǝʞ = 1)"sv,			[](table&& tbl) { CHECK(tbl[R"(ʎǝʞ)"] == 1); });
	parsing_should_succeed(FILE_LINE_ARGS,	R"(Fuß = 2)"sv,			[](table&& tbl) { CHECK(tbl[R"(Fuß)"] == 2); });
	parsing_should_succeed(FILE_LINE_ARGS,	R"(😂 = 3)"sv,			[](table&& tbl) { CHECK(tbl[R"(😂)"] == 3); });
	parsing_should_succeed(FILE_LINE_ARGS,	R"(汉语大字典 = 4)"sv,	[](table&& tbl) { CHECK(tbl[R"(汉语大字典)"] == 4); });
	parsing_should_succeed(FILE_LINE_ARGS,	R"(辭源 = 5)"sv,			[](table&& tbl) { CHECK(tbl[R"(辭源)"] == 5); });
	parsing_should_succeed(FILE_LINE_ARGS,	R"(பெண்டிரேம் = 6)"sv,	[](table&& tbl) { CHECK(tbl[R"(பெண்டிரேம்)"] == 6); });
#else
	parsing_should_fail(FILE_LINE_ARGS,		R"(ʎǝʞ = 1)"sv);
	parsing_should_fail(FILE_LINE_ARGS,		R"(Fuß = 2)"sv);
	parsing_should_fail(FILE_LINE_ARGS,		R"(😂 = 3)"sv);
	parsing_should_fail(FILE_LINE_ARGS,		R"(汉语大字典 = 4)"sv);
	parsing_should_fail(FILE_LINE_ARGS,		R"(辭源 = 5)"sv);
	parsing_should_fail(FILE_LINE_ARGS,		R"(பெண்டிரேம் = 6)"sv);
#endif
#endif // UNICODE_LITERALS_OK
	// clang-format on
}

TEST_CASE("parsing - key-value pairs (string keys)")
{
	// these are all derived from the discussion at
	// https://github.com/toml-lang/toml/issues/733.

	// whitespace stripped, fail duplicate keys
	parsing_should_fail(FILE_LINE_ARGS, R"(
		a     = 2
		a = 3
	)"sv);

	// only surrounding whitespace is stripped, fail: illegal key name or syntax error
	parsing_should_fail(FILE_LINE_ARGS, "a b = 3"sv);

	// whitespace is allowed when quoted, fail duplicate key
	parsing_should_succeed(FILE_LINE_ARGS, "\"a b\" = 3"sv);
	parsing_should_succeed(FILE_LINE_ARGS, "'a b' = 3"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		"a b" = 3
		'a b' = 3
	)"sv);

	// whitespace is allowed when quoted, but not collapsed, success
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		"a b" = 3
		'a  b' = 3
	)"sv);

	// whitespace relevant, but fail: duplicate key
	parsing_should_fail(FILE_LINE_ARGS, R"(
		"a " = 2
		'a ' = 3
	)"sv);

	// whitespace relevant, and not collapsed, success
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		"a " = 2
		"a  " = 3
	)"sv);

	// whitespace can be escaped, success, different keys (whitespace escapes are not normalized)
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		"a\n" = 2
		"a\r" = 3
		"a\t" = 3
		"a\f" = 3
	)"sv);

	// valid keys composed of various string/non-string mixes types
	parsing_should_succeed(FILE_LINE_ARGS, R"(a = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"('a' = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("a" = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"(a.b = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"('a'.b = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("a".b = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"(a.'b' = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"('a'.'b' = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("a".'b' = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"(a."b" = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"('a'."b" = 3)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("a"."b" = 3)"sv);

	// multi-line strings can't be used in keys
	parsing_should_fail(FILE_LINE_ARGS, R"('''a''' = 3)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"("""a""" = 3)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(a.'''b''' = 3)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(a."""b""" = 3)"sv);

	// whitespace relevant (success test, values are NOTE equal)
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								a = " to do "
								b = "to do"
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["a"] == " to do "sv);
							   CHECK(tbl["b"] == "to do"sv);
						   });

	// values must be quoted, syntax error
	parsing_should_fail(FILE_LINE_ARGS, R"(
		a = to do
		b = todo
	)"sv);

	// different quotes, fail duplicate keys
	parsing_should_fail(FILE_LINE_ARGS, R"(
		a = 2
		'a' = 2
	)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		'a' = 2
		"a" = 2
	)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		'a' = 2
		"""a""" = 2
	)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		'''a''' = 2
		"""a""" = 2
	)"sv);

	// success test, capital not equal to small
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		a = 2
		A = 3
	)"sv);

	// inner quotes are not stripped from value, a & b are equal, value surrounded by quotes
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
								a = "\"quoted\""
								b = """"quoted""""
							)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["a"] == "\"quoted\""sv);
							   CHECK(tbl["b"] == "\"quoted\""sv);
						   });

	// quote correction is not applied, fail syntax error
	parsing_should_fail(FILE_LINE_ARGS, R"("a = "test")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"('a = 'test')"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"("a = 'test")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"('a = "test')"sv);

	// quotes cannot appear in keys this way, fail syntax error
	parsing_should_fail(FILE_LINE_ARGS, R"("a'b = 3)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"("a"b = 3)"sv);

	// escaped quotes and single quotes can appear this way, fail duplicate keys
	parsing_should_succeed(FILE_LINE_ARGS, R"("a'b" = 2)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("a\u0027b" = 4)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		"a'b" = 2
		"a\u0027b" = 4
	)"sv);

	// literal strings, escapes are not escaped, success, since keys are valid and not equal
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		'a"b' = 2
		'a\"b' = 4
	)"sv);

	// escapes must be compared after unescaping, fail duplicate key
	parsing_should_succeed(FILE_LINE_ARGS, R"(a = 1)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("\u0061" = 2)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		a = 1
		"\u0061" = 2
	)"sv);

	// escaping requires quotes, syntax error
	parsing_should_fail(FILE_LINE_ARGS, R"(\u0061 = 2)"sv);

	// empty keys are allowed, but can only appear once, fail duplicate key
	parsing_should_succeed(FILE_LINE_ARGS, R"("" = 2)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"('' = 3)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		"" = 2
		'' = 3
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, fail duplicate key
	parsing_should_succeed(FILE_LINE_ARGS, R"(1234 = 5)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("1234" = 5)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		1234 = 5
		"1234" = 5
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, fail duplicate key
	parsing_should_succeed(FILE_LINE_ARGS, R"(1234 = 5)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"('1234' = 5)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		1234 = 5
		'1234' = 5
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, valid, different keys
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		1234 = 5
		01234 = 5
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, valid, different keys
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		12e3 = 4
		12000 = 5
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, valid, different keys, one dotted
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		1.2e3 = 4
		1200 = 5
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, success, cause one is dotted
	parsing_should_succeed(FILE_LINE_ARGS, R"(
		1.2e3 = 4
		"1.2e3" = 5
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, fail duplicate keys
	parsing_should_succeed(FILE_LINE_ARGS, R"(12e3 = 4)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("12e3" = 5)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		12e3 = 4
		"12e3" = 5
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, fail duplicate dotted keys
	parsing_should_succeed(FILE_LINE_ARGS, R"(1.2e3 = 4)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"(1."2e3" = 5)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		1.2e3 = 4
		1."2e3" = 5
	)"sv);

	// bare keys can be numerals, but are interpreted as strings, fail duplicate dotted keys
	parsing_should_succeed(FILE_LINE_ARGS, R"(1.2e3 = 4)"sv);
	parsing_should_succeed(FILE_LINE_ARGS, R"("1".2e3 = 5)"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(
		1.2e3 = 4
		"1".2e3 = 5
	)"sv);
}
