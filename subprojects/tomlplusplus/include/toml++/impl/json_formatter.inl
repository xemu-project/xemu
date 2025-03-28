//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once

#include "preprocessor.h"
//# {{
#if !TOML_IMPLEMENTATION
#error This is an implementation-only header.
#endif
//# }}
#if TOML_ENABLE_FORMATTERS

#include "json_formatter.h"
#include "print_to_stream.h"
#include "table.h"
#include "array.h"
#include "header_start.h"

TOML_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	void json_formatter::print(const toml::table& tbl)
	{
		if (tbl.empty())
		{
			print_unformatted("{}"sv);
			return;
		}

		print_unformatted('{');

		if (indent_sub_tables())
			increase_indent();
		bool first = false;
		for (auto&& [k, v] : tbl)
		{
			if (first)
				print_unformatted(',');
			first = true;
			print_newline(true);
			print_indent();

			print_string(k.str(), false);
			if (terse_kvps())
				print_unformatted(":"sv);
			else
				print_unformatted(" : "sv);

			const auto type = v.type();
			TOML_ASSUME(type != node_type::none);
			switch (type)
			{
				case node_type::table: print(*reinterpret_cast<const table*>(&v)); break;
				case node_type::array: print(*reinterpret_cast<const array*>(&v)); break;
				default: print_value(v, type);
			}
		}
		if (indent_sub_tables())
			decrease_indent();
		print_newline(true);
		print_indent();

		print_unformatted('}');
	}

	TOML_EXTERNAL_LINKAGE
	void json_formatter::print(const toml::array& arr)
	{
		if (arr.empty())
		{
			print_unformatted("[]"sv);
			return;
		}

		print_unformatted('[');
		if (indent_array_elements())
			increase_indent();
		for (size_t i = 0; i < arr.size(); i++)
		{
			if (i > 0u)
				print_unformatted(',');
			print_newline(true);
			print_indent();

			auto& v			= arr[i];
			const auto type = v.type();
			TOML_ASSUME(type != node_type::none);
			switch (type)
			{
				case node_type::table: print(*reinterpret_cast<const table*>(&v)); break;
				case node_type::array: print(*reinterpret_cast<const array*>(&v)); break;
				default: print_value(v, type);
			}
		}
		if (indent_array_elements())
			decrease_indent();
		print_newline(true);
		print_indent();
		print_unformatted(']');
	}

	TOML_EXTERNAL_LINKAGE
	void json_formatter::print()
	{
		if (dump_failed_parse_result())
			return;

		switch (auto source_type = source().type())
		{
			case node_type::table: print(*reinterpret_cast<const table*>(&source())); break;
			case node_type::array: print(*reinterpret_cast<const array*>(&source())); break;
			default: print_value(source(), source_type);
		}
	}
}
TOML_NAMESPACE_END;

#include "header_end.h"
#endif // TOML_ENABLE_FORMATTERS
