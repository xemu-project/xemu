// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

// This example demonstrates how to use the toml::json_formatter to re-serialize TOML data as JSON.

#include "examples.h"
#include <toml++/toml.h>

using namespace std::string_view_literals;

int main(int argc, char** argv)
{
	const auto path = argc > 1 ? std::string_view{ argv[1] } : ""sv;

	toml::table table;
	try
	{
		// read directly from stdin
		if (path == "-"sv || path.empty())
			table = toml::parse(std::cin, "stdin"sv);

		// read from a file
		else
			table = toml::parse_file(argv[1]);
	}
	catch (const toml::parse_error& err)
	{
		std::cerr << err << "\n";
		return 1;
	}

	std::cout << toml::json_formatter{ table } << "\n";
	return 0;
}
