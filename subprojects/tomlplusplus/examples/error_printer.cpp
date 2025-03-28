// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

// This example shows the error messages the library produces by forcing a set of specific parsing
// failures and printing their results.

#include "examples.h"
#include <toml++/toml.h>

using namespace std::string_view_literals;

namespace
{
	inline constexpr auto invalid_parses = std::array{
		"########## comments and whitespace"sv,
		"# bar\rkek"sv,
		"# bar\bkek"sv,
		"# \xf1\x63"sv,
		"# val1 = 1\fval2 = 2"sv,
		"foo = 1\n\u2000\nbar = 2"sv,

		"########## inline tables"sv,
		"val = {,}"sv,
		"val = {a='b',}"sv, // allowed when TOML_ENABLE_UNRELEASED_FEATURES == 1
		"val = {a='b',,}"sv,
		"val = {a='b',"sv,
		"val = {a='b',\n c='d'}"sv, // allowed when TOML_ENABLE_UNRELEASED_FEATURES == 1
		"val = {?='b'}"sv,
		"foo = {} \n [foo.bar]"sv,

		"########## tables"sv,
		"[]"sv,
		"[foo"sv,
		"[foo] ?"sv,
		"[foo] [bar]"sv,
		"[foo]\n[foo]"sv,
		"? = 'foo' ?"sv,
		"[ [foo] ]"sv,

		"########## arrays"sv,
		"val = [,]"sv,
		"val = ['a',,]"sv,
		"val = ['a',"sv,

		"########## key-value pairs"sv,
		"val = 'foo' ?"sv,
		"val = "sv,
		"val "sv,
		"val ?"sv,
		"val = ]"sv,
		"[foo]\nbar = 'kek'\nbar = 'kek2'"sv,
		"[foo]\nbar = 'kek'\nbar = 7"sv,
		"[foo.bar]\n[foo]\nbar = 'kek'"sv,
		"[foo]\nbar = 'kek'\nbar.kek = 7"sv,
		"[foo]\nbar.? = 'kek'"sv,
		R"('''val''' = 1)"sv,
		R"(a."""val""" = 1)"sv,
		"1= 0x6cA#+\xf1"sv,
		R"(😂 = 3)"sv, // allowed when TOML_ENABLE_UNRELEASED_FEATURES == 1

		"########## values"sv,
		"val = _"sv,
		"val = G"sv,
		"PATHOLOGICALLY_NESTED"sv, // generated inline

		"########## strings"sv,
		"val = \" \r \""sv,
		R"(val = ")"sv,
		R"(val = "\g")"sv,
		R"(val = "\x20")"sv, // allowed when TOML_ENABLE_UNRELEASED_FEATURES == 1
		R"(val = "\uFFF")"sv,
		R"(val = "\uFFFG")"sv,
		R"(val = "\UFFFFFFF")"sv,
		R"(val = "\UFFFFFGF")"sv,
		R"(val = "\uD801")"sv,
		R"(val = "\U00110000")"sv,
		R"(val = """ """""")"sv,
		R"(val = ''' '''''')"sv,
		"val = '\n'"sv,

		"########## integers"sv,
		R"(val = -0b0)"sv,
		R"(val = -0o0)"sv,
		R"(val = -0x0)"sv,
		R"(val = +0b0)"sv,
		R"(val = +0o0)"sv,
		R"(val = +0x0)"sv,
		R"(val = 1-)"sv,
		R"(val = -1+)"sv,
		R"(val = -+1)"sv,
		R"(val = 1_0_)"sv,
		R"(val = 1_0_ )"sv,
		R"(val = 01 )"sv,
		R"(val = 0b10000000_00000000_00000000_00000000_00000000_00000000_00000000_00000000 )"sv,
		R"(val = 0o1000000000000000000000 )"sv,
		R"(val = 9223372036854775808 )"sv,
		R"(val = 0x8000000000000000 )"sv,

		"########## floats"sv,
		R"(val = 100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.0)"sv,
	};

	inline constexpr auto divider =
		"################################################################################"sv;
}

int main()
{
	const auto parse_and_print_if_error = [](std::string_view str)
	{
#if TOML_EXCEPTIONS
		try
		{
			auto result = toml::parse(str);
			static_cast<void>(result);
		}
		catch (const toml::parse_error& err)
		{
			std::cout << err << "\n\n"sv;
		}
#else
		if (auto result = toml::parse(str); !result)
			std::cout << result.error() << "\n\n"sv;
#endif
	};

	for (auto str : invalid_parses)
	{
		if (str.empty())
			continue;

		// section headings
		if (str.substr(0, 10) == "##########"sv)
		{
			std::cout << divider << '\n';
			std::cout << "#    "sv << str.substr(11) << '\n';
			std::cout << divider << "\n\n"sv;
		}

		// error messages
		else
		{
			toml::parse_result result;

			if (str == "PATHOLOGICALLY_NESTED"sv)
			{
				std::string s(1000u, '[');
				constexpr auto start = "array = "sv;
				memcpy(s.data(), start.data(), start.length());
				parse_and_print_if_error(s);
			}
			else
				parse_and_print_if_error(str);
		}
	}
	return 0;
}
