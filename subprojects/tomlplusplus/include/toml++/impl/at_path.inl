//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once

//# {{
#include "preprocessor.hpp"
#if !TOML_IMPLEMENTATION
#error This is an implementation-only header.
#endif
//# }}

#include "at_path.hpp"
#include "array.hpp"
#include "table.hpp"
TOML_DISABLE_WARNINGS;
#if TOML_INT_CHARCONV
#include <charconv>
#else
#include <sstream>
#endif
TOML_ENABLE_WARNINGS;
#include "header_start.hpp"

TOML_IMPL_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	bool TOML_CALLCONV parse_path(const std::string_view path,
								  void* const data,
								  const parse_path_callback<std::string_view> on_key,
								  const parse_path_callback<size_t> on_index)
	{
		// a blank string is a valid path; it's just one component representing the "" key
		if (path.empty())
			return on_key(data, ""sv);

		size_t pos					= 0;
		const auto end				= path.length();
		bool prev_was_array_indexer = false;
		bool prev_was_dot			= true; // invisible root 'dot'

		while (pos < end)
		{
			// start of an array indexer
			if (path[pos] == '[')
			{
				// find first digit in index
				size_t index_start = pos + 1u;
				while (true)
				{
					if TOML_UNLIKELY(index_start >= path.length())
						return false;

					const auto c = path[index_start];
					if TOML_LIKELY(c >= '0' && c <= '9')
						break;
					else if (c == ' ' || c == '\t')
						index_start++;
					else
						return false;
				}
				TOML_ASSERT(path[index_start] >= '0');
				TOML_ASSERT(path[index_start] <= '9');

				// find end of index (first non-digit character)
				size_t index_end = index_start + 1u;
				while (true)
				{
					// if an array indexer is missing the trailing ']' at the end of the string, permissively accept it
					if TOML_UNLIKELY(index_end >= path.length())
						break;

					const auto c = path[index_end];
					if (c >= '0' && c <= '9')
						index_end++;
					else if (c == ']' || c == ' ' || c == '\t' || c == '.' || c == '[')
						break;
					else
						return false;
				}
				TOML_ASSERT(path[index_end - 1u] >= '0');
				TOML_ASSERT(path[index_end - 1u] <= '9');

				// move pos to after indexer (char after closing ']' or permissively EOL/subkey '.'/next opening '[')
				pos = index_end;
				while (true)
				{
					if TOML_UNLIKELY(pos >= path.length())
						break;

					const auto c = path[pos];
					if (c == ']')
					{
						pos++;
						break;
					}
					else if TOML_UNLIKELY(c == '.' || c == '[')
						break;
					else if (c == '\t' || c == ' ')
						pos++;
					else
						return false;
				}

				// get array index substring
				auto index_str = path.substr(index_start, index_end - index_start);

				// parse the actual array index to an integer type
				size_t index;
				if (index_str.length() == 1u)
					index = static_cast<size_t>(index_str[0] - '0');
				else
				{
#if TOML_INT_CHARCONV

					auto fc_result = std::from_chars(index_str.data(), index_str.data() + index_str.length(), index);
					if (fc_result.ec != std::errc{})
						return false;

#else

					std::stringstream ss;
					ss.imbue(std::locale::classic());
					ss.write(index_str.data(), static_cast<std::streamsize>(index_str.length()));
					if (!(ss >> index))
						return false;

#endif
				}

				prev_was_dot		   = false;
				prev_was_array_indexer = true;

				if (!on_index(data, index))
					return false;
			}

			// start of a new table child
			else if (path[pos] == '.')
			{
				// a dot immediately following another dot (or at the beginning of the string) is as if we'd asked
				// for an empty child in between, e.g.
				//
				//     foo..bar
				//
				// is equivalent to
				//
				//     "foo".""."bar"
				//
				if (prev_was_dot && !on_key(data, ""sv))
					return false;

				pos++;
				prev_was_dot		   = true;
				prev_was_array_indexer = false;
			}

			// an errant closing ']'
			else if TOML_UNLIKELY(path[pos] == ']')
				return false;

			// some regular subkey
			else
			{
				const auto subkey_start = pos;
				const auto subkey_len =
					impl::min(path.find_first_of(".[]"sv, subkey_start + 1u), path.length()) - subkey_start;
				const auto subkey = path.substr(subkey_start, subkey_len);

				// a regular subkey segment immediately after an array indexer is OK if it was all whitespace, e.g.:
				//
				//     "foo[0]  .bar"
				//            ^^ skip this
				//
				// otherwise its an error (since it would have to be preceeded by a dot)
				if (prev_was_array_indexer)
				{
					auto non_ws = subkey.find_first_not_of(" \t");
					if (non_ws == std::string_view::npos)
					{
						pos += subkey_len;
						prev_was_dot		   = false;
						prev_was_array_indexer = false;
						continue;
					}
					else
						return false;
				}

				pos += subkey_len;
				prev_was_dot		   = false;
				prev_was_array_indexer = false;

				if (!on_key(data, subkey))
					return false;
			}
		}

		// Last character was a '.', which implies an empty string key at the end of the path
		if (prev_was_dot && !on_key(data, ""sv))
			return false;

		return true;
	}
}
TOML_IMPL_NAMESPACE_END;

TOML_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	node_view<node> TOML_CALLCONV at_path(node & root, std::string_view path) noexcept
	{
		// early-exit sanity-checks
		if (root.is_value())
			return {};
		if (auto tbl = root.as_table(); tbl && tbl->empty())
			return {};
		if (auto arr = root.as_array(); arr && arr->empty())
			return {};

		node* current = &root;

		static constexpr auto on_key = [](void* data, std::string_view key) noexcept -> bool
		{
			auto& curr = *static_cast<node**>(data);
			TOML_ASSERT_ASSUME(curr);

			const auto current_table = curr->as<table>();
			if (!current_table)
				return false;

			curr = current_table->get(key);
			return curr != nullptr;
		};

		static constexpr auto on_index = [](void* data, size_t index) noexcept -> bool
		{
			auto& curr = *static_cast<node**>(data);
			TOML_ASSERT_ASSUME(curr);

			const auto current_array = curr->as<array>();
			if (!current_array)
				return false;

			curr = current_array->get(index);
			return curr != nullptr;
		};

		if (!impl::parse_path(path, &current, on_key, on_index))
			current = nullptr;

		return node_view{ current };
	}

	TOML_EXTERNAL_LINKAGE
	node_view<const node> TOML_CALLCONV at_path(const node& root, std::string_view path) noexcept
	{
		return node_view<const node>{ at_path(const_cast<node&>(root), path).node() };
	}

#if TOML_ENABLE_WINDOWS_COMPAT

	TOML_EXTERNAL_LINKAGE
	node_view<node> TOML_CALLCONV at_path(node & root, std::wstring_view path)
	{
		// these are the same top-level checks from the narrow-string version;
		// they're hoisted up here to avoid doing the wide -> narrow conversion where it would not be necessary
		// (avoids an allocation)
		if (root.is_value())
			return {};
		if (auto tbl = root.as_table(); tbl && tbl->empty())
			return {};
		if (auto arr = root.as_array(); arr && arr->empty())
			return {};

		return at_path(root, impl::narrow(path));
	}

	TOML_EXTERNAL_LINKAGE
	node_view<const node> TOML_CALLCONV at_path(const node& root, std::wstring_view path)
	{
		return node_view<const node>{ at_path(const_cast<node&>(root), path).node() };
	}

#endif // TOML_ENABLE_WINDOWS_COMPAT
}
TOML_NAMESPACE_END;

#include "header_end.hpp"
