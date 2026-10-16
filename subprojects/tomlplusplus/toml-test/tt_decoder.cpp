//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) 2019-2020 Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tt.hpp"

using nlohmann::json;
using namespace std::string_view_literals;

TOML_NAMESPACE_START
{
	static void to_json(json & j, const value<std::string>& val)
	{
		j["type"]  = "string";
		j["value"] = *val;
	}

	template <typename T>
	static void to_json_via_stream(json & j, const value<T>& val)
	{
		static constexpr auto flags = toml_formatter::default_flags
									& ~(format_flags::allow_binary_integers		   //
										| format_flags::allow_hexadecimal_integers //
										| format_flags::allow_octal_integers);

		std::ostringstream ss;
		ss << toml_formatter{ val, flags };
		j["value"] = ss.str();
	}

	static void to_json(json & j, const value<int64_t>& val)
	{
		j["type"] = "integer";
		to_json_via_stream(j, val);
	}

	static void to_json(json & j, const value<double>& val)
	{
		j["type"] = "float";
		to_json_via_stream(j, val);
	}

	static void to_json(json & j, const value<bool>& val)
	{
		j["type"] = "bool";
		to_json_via_stream(j, val);
	}

	static void to_json(json & j, const value<date>& val)
	{
		j["type"] = "date-local";
		to_json_via_stream(j, val);
	}

	static void to_json(json & j, const value<time>& val)
	{
		j["type"] = "time-local";
		to_json_via_stream(j, val);
	}

	static void to_json(json & j, const value<date_time>& val)
	{
		j["type"] = val->is_local() ? "datetime-local" : "datetime";
		to_json_via_stream(j, val);
	}

	static void to_json(json&, const array&);

	static void to_json(json & j, const table& tbl)
	{
		j = json::object();
		for (auto& [k_, v_] : tbl)
			v_.visit([&, k = &k_](auto& v) { j[std::string{ k->str() }] = v; });
	}

	static void to_json(json & j, const array& arr)
	{
		j = json::array();
		for (auto& v_ : arr)
			v_.visit([&](auto& v) { j.push_back(v); });
	}
}
TOML_NAMESPACE_END;

int main()
{
	try
	{
		const std::string str(std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{});

		json j = toml::parse(str, "stdin"sv);

		std::cout << j << "\n";
	}
	catch (const toml::parse_error& err)
	{
		std::cerr << "\n\n" << err << "\n";
		return 1;
	}
	catch (const std::exception& exc)
	{
		std::cerr << "\n\n" << exc.what() << "\n";
		return 1;
	}
	catch (...)
	{
		std::cerr << "\n\nAn unspecified error occurred.\n";
		return 1;
	}

	return 0;
}
