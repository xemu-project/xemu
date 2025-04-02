// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

TEST_CASE("parsing - strings")
{
	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
str = "I'm a string. \"You can quote me\". Name\tJos\u00E9\nLocation\tSF."

str1 = """
Roses are red
Violets are blue"""

str2 = """

Roses are red
Violets are blue"""
)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["str"]
									 == "I'm a string. \"You can quote me\". Name\tJos\u00E9\nLocation\tSF."sv);
							   CHECK(tbl["str1"] == "Roses are red\nViolets are blue"sv);
							   CHECK(tbl["str2"] == "\nRoses are red\nViolets are blue"sv);
						   });

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
# The following strings are byte-for-byte equivalent:
str1 = "The quick brown fox jumps over the lazy dog."

str2 = """
The quick brown \


  fox jumps over \
    the lazy dog."""

str3 = """\
       The quick brown \
       fox jumps over \
       the lazy dog.\
       """

str4 = """Here are two quotation marks: "". Simple enough."""
# str5 = """Here are three quotation marks: """."""  # INVALID
str5 = """Here are three quotation marks: ""\"."""
str6 = """Here are fifteen quotation marks: ""\"""\"""\"""\"""\"."""

# "This," she said, "is just a pointless statement."
str7 = """"This," she said, "is just a pointless statement.""""
)"sv,
						   [](table&& tbl)
						   {
							   static constexpr auto quick_brown_fox = "The quick brown fox jumps over the lazy dog."sv;
							   CHECK(tbl["str1"] == quick_brown_fox);
							   CHECK(tbl["str2"] == quick_brown_fox);
							   CHECK(tbl["str3"] == quick_brown_fox);
							   CHECK(tbl["str4"] == R"(Here are two quotation marks: "". Simple enough.)"sv);
							   CHECK(tbl["str5"] == R"(Here are three quotation marks: """.)"sv);
							   CHECK(tbl["str6"] == R"(Here are fifteen quotation marks: """"""""""""""".)"sv);
							   CHECK(tbl["str7"] == R"("This," she said, "is just a pointless statement.")"sv);
						   });

	parsing_should_fail(FILE_LINE_ARGS, R"(str5 = """Here are three quotation marks: """.""")"sv);

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
# What you see is what you get.
winpath  = 'C:\Users\nodejs\templates'
winpath2 = '\\ServerX\admin$\system32\'
quoted   = 'Tom "Dubs" Preston-Werner'
regex    = '<\i\c*\s*>'
regex2 = '''I [dw]on't need \d{2} apples'''
lines  = '''
The first newline is
trimmed in raw strings.
   All other whitespace
   is preserved.
'''
lines2  = '''

The first newline is
trimmed in raw strings.
   All other whitespace
   is preserved.
'''
)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["winpath"] == R"(C:\Users\nodejs\templates)"sv);
							   CHECK(tbl["winpath2"] == R"(\\ServerX\admin$\system32\)"sv);
							   CHECK(tbl["quoted"] == R"(Tom "Dubs" Preston-Werner)"sv);
							   CHECK(tbl["regex"] == R"(<\i\c*\s*>)"sv);
							   CHECK(tbl["regex2"] == R"(I [dw]on't need \d{2} apples)"sv);
							   CHECK(tbl["lines"] == R"(The first newline is
trimmed in raw strings.
   All other whitespace
   is preserved.
)"sv);
							   CHECK(tbl["lines2"] == R"(
The first newline is
trimmed in raw strings.
   All other whitespace
   is preserved.
)"sv);
						   });

	parsing_should_succeed(FILE_LINE_ARGS,
						   R"(
quot15 = '''Here are fifteen quotation marks: """""""""""""""'''

# apos15 = '''Here are fifteen apostrophes: ''''''''''''''''''  # INVALID
apos15 = "Here are fifteen apostrophes: '''''''''''''''"

# 'That's still pointless', she said.
str = ''''That's still pointless', she said.'''
)"sv,
						   [](table&& tbl)
						   {
							   CHECK(tbl["quot15"] == R"(Here are fifteen quotation marks: """"""""""""""")"sv);
							   CHECK(tbl["apos15"] == R"(Here are fifteen apostrophes: ''''''''''''''')"sv);
							   CHECK(tbl["str"] == R"('That's still pointless', she said.)"sv);
						   });

	parsing_should_fail(FILE_LINE_ARGS, R"(apos15 = '''Here are fifteen apostrophes: ''''''''''''''''''  # INVALID)"sv);

	// value tests
	parse_expected_value(FILE_LINE_ARGS,
						 R"("The quick brown fox jumps over the lazy dog")"sv,
						 "The quick brown fox jumps over the lazy dog"sv);
	parse_expected_value(FILE_LINE_ARGS,
						 R"('The quick brown fox jumps over the lazy dog')"sv,
						 "The quick brown fox jumps over the lazy dog"sv);
	parse_expected_value(FILE_LINE_ARGS,
						 R"("""The quick brown fox jumps over the lazy dog""")"sv,
						 "The quick brown fox jumps over the lazy dog"sv);
	parse_expected_value(FILE_LINE_ARGS,
						 R"('''The quick brown fox jumps over the lazy dog''')"sv,
						 "The quick brown fox jumps over the lazy dog"sv);

#if UNICODE_LITERALS_OK
	parse_expected_value(FILE_LINE_ARGS, R"("Ýôú'ℓℓ λáƭè ₥è áƒƭèř ƭλïƨ - #")"sv, R"(Ýôú'ℓℓ λáƭè ₥è áƒƭèř ƭλïƨ - #)"sv);
	parse_expected_value(FILE_LINE_ARGS,
						 R"(" Âñδ ωλèñ \"'ƨ ářè ïñ ƭλè ƨƭřïñϱ, áℓôñϱ ωïƭλ # \"")"sv,
						 R"( Âñδ ωλèñ "'ƨ ářè ïñ ƭλè ƨƭřïñϱ, áℓôñϱ ωïƭλ # ")"sv);
	parse_expected_value(FILE_LINE_ARGS,
						 R"("Ýôú δôñ'ƭ ƭλïñƙ ƨô₥è úƨèř ωôñ'ƭ δô ƭλáƭ?")"sv,
						 R"(Ýôú δôñ'ƭ ƭλïñƙ ƨô₥è úƨèř ωôñ'ƭ δô ƭλáƭ?)"sv);
#endif // UNICODE_LITERALS_OK

	parse_expected_value(FILE_LINE_ARGS, R"("\"\u03B1\u03B2\u03B3\"")"sv, "\"\u03B1\u03B2\u03B3\""sv);

// toml/pull/796 (\xHH unicode scalars)
#if TOML_LANG_UNRELEASED
	parse_expected_value(FILE_LINE_ARGS,
						 R"("\x00\x10\x20\x30\x40\x50\x60\x70\x80\x90\x11\xFF\xEE")"sv,
						 "\u0000\u0010\u0020\u0030\u0040\u0050\u0060\u0070\u0080\u0090\u0011\u00FF\u00EE"sv);
#else
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\x00\x10\x20\x30\x40\x50\x60\x70\x80\x90\x11\xFF\xEE")"sv);
#endif

	// toml/pull/790
#if TOML_LANG_UNRELEASED
	parse_expected_value(FILE_LINE_ARGS, R"("\e[31mfoo\e[0m")"sv, "\x1B[31mfoo\x1B[0m"sv);

#else
	parsing_should_fail(FILE_LINE_ARGS, R"("\e[31mfoo\e[0m")"sv);
#endif

	// check 8-digit \U scalars with insufficient digits
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\U1234567")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\U123456")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\U12345")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\U1234")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\U123")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\U12")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\U1")"sv);

	// check 4-digit \u scalars with insufficient digits
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\u123")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\u12")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\u1")"sv);

	// check 2-digit \x scalars with insufficient digits
	parsing_should_fail(FILE_LINE_ARGS, R"(str = "\x1")"sv);

	// ML string examples from https://github.com/toml-lang/toml/issues/725
	parse_expected_value(FILE_LINE_ARGS, R"( """ """          )"sv, R"( )"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( """ """"         )"sv, R"( ")"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( """ """""        )"sv, R"( "")"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(v= """ """"""       )"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( ''' '''          )"sv, R"( )"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( ''' ''''         )"sv, R"( ')"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( ''' '''''        )"sv, R"( '')"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(v= ''' ''''''       )"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( """"""           )"sv, R"()"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( """" """         )"sv, R"(" )"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( """"" """        )"sv, R"("" )"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(v= """""" """       )"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( ''''''           )"sv, R"()"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( '''' '''         )"sv, R"(' )"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( ''''' '''        )"sv, R"('' )"sv);
	parsing_should_fail(FILE_LINE_ARGS, R"(v= '''''' '''       )"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( """""\""""""     )"sv, R"(""""")"sv);
	parse_expected_value(FILE_LINE_ARGS, R"( """""\"""\"""""" )"sv, R"("""""""")"sv);
}
