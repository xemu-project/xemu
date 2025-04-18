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

#include "yaml_formatter.h"
#include "print_to_stream.h"
#include "table.h"
#include "array.h"
#include "header_start.h"

TOML_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	void yaml_formatter::print_yaml_string(const value<std::string>& str)
	{
		if (str->empty())
		{
			base::print(str);
			return;
		}

		bool contains_newline = false;
		for (auto c = str->c_str(), e = str->c_str() + str->length(); c < e && !contains_newline; c++)
			contains_newline = *c == '\n';

		if (contains_newline)
		{
			print_unformatted("|-"sv);

			increase_indent();

			auto line_end  = str->c_str() - 1u;
			const auto end = str->c_str() + str->length();
			while (line_end != end)
			{
				auto line_start = line_end + 1u;
				line_end		= line_start;
				for (; line_end != end && *line_end != '\n'; line_end++)
					;

				if TOML_LIKELY(line_start != line_end || line_end != end)
				{
					print_newline();
					print_indent();
					print_unformatted(std::string_view{ line_start, static_cast<size_t>(line_end - line_start) });
				}
			}

			decrease_indent();
		}
		else
			print_string(*str, false, true);
	}

	TOML_EXTERNAL_LINKAGE
	void yaml_formatter::print(const toml::table& tbl, bool parent_is_array)
	{
		if (tbl.empty())
		{
			print_unformatted("{}"sv);
			return;
		}

		increase_indent();

		for (auto&& [k, v] : tbl)
		{
			if (!parent_is_array)
			{
				print_newline();
				print_indent();
			}
			parent_is_array = false;

			print_string(k.str(), false, true);
			if (terse_kvps())
				print_unformatted(":"sv);
			else
				print_unformatted(": "sv);

			const auto type = v.type();
			TOML_ASSUME(type != node_type::none);
			switch (type)
			{
				case node_type::table: print(*reinterpret_cast<const table*>(&v)); break;
				case node_type::array: print(*reinterpret_cast<const array*>(&v)); break;
				case node_type::string: print_yaml_string(*reinterpret_cast<const value<std::string>*>(&v)); break;
				default: print_value(v, type);
			}
		}

		decrease_indent();
	}

	TOML_EXTERNAL_LINKAGE
	void yaml_formatter::print(const toml::array& arr, bool parent_is_array)
	{
		if (arr.empty())
		{
			print_unformatted("[]"sv);
			return;
		}

		increase_indent();

		for (auto&& v : arr)
		{
			if (!parent_is_array)
			{
				print_newline();
				print_indent();
			}
			parent_is_array = false;

			print_unformatted("- "sv);

			const auto type = v.type();
			TOML_ASSUME(type != node_type::none);
			switch (type)
			{
				case node_type::table: print(*reinterpret_cast<const table*>(&v), true); break;
				case node_type::array: print(*reinterpret_cast<const array*>(&v), true); break;
				case node_type::string: print_yaml_string(*reinterpret_cast<const value<std::string>*>(&v)); break;
				default: print_value(v, type);
			}
		}

		decrease_indent();
	}

	TOML_EXTERNAL_LINKAGE
	void yaml_formatter::print()
	{
		if (dump_failed_parse_result())
			return;

		switch (auto source_type = source().type())
		{
			case node_type::table:
				decrease_indent(); // so root kvps and tables have the same indent
				print(*reinterpret_cast<const table*>(&source()));
				break;

			case node_type::array: print(*reinterpret_cast<const array*>(&source())); break;

			case node_type::string: print_yaml_string(*reinterpret_cast<const value<std::string>*>(&source())); break;

			default: print_value(source(), source_type);
		}
	}
}
TOML_NAMESPACE_END;

#include "header_end.h"
#endif // TOML_ENABLE_FORMATTERS
