//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) 2019-2020 Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tt.hpp"

using nlohmann::json;
using namespace std::string_view_literals;

[[nodiscard]] static bool is_tt_value(const json& j) noexcept
{
	return j.is_object()		 //
		&& j.size() == 2u		 //
		&& j.contains("type")	 //
		&& j["type"].is_string() //
		&& j.contains("value")	 //
		&& j["value"].is_string();
}

TOML_NAMESPACE_START
{
	static void from_json(const json& j, value<std::string>& val)
	{
		assert(is_tt_value(j));
		assert(j["type"] == "string");

		*val = j["value"];
	}

	template <typename T>
	static void from_json_via_parse(const json& j, value<T>& val)
	{
		assert(is_tt_value(j));

		std::stringstream ss;
		ss.imbue(std::locale::classic());
		ss << "value = " << j["value"].get_ref<const std::string&>();

		auto tbl = toml::parse(ss);
		tbl["value"sv].visit(
			[&](auto& v)
			{
				if constexpr (is_value<decltype(v)>)
				{
					using value_type = typename std::remove_cv_t<std::remove_reference_t<decltype(v)>>::value_type;
					if constexpr (std::is_same_v<T, value_type>)
						val = std::move(v);
					else if constexpr (std::is_constructible_v<T, value_type>)
						*val = T(*v);
					else if constexpr (std::is_convertible_v<value_type, T>)
						*val = static_cast<T>(*v);
				}
			});
	}

	static void from_json(const json& j, value<int64_t>& val)
	{
		assert(is_tt_value(j));
		assert(j["type"] == "integer");

		from_json_via_parse(j, val);
	}

	static void from_json(const json& j, value<double>& val)
	{
		assert(is_tt_value(j));
		assert(j["type"] == "float");

		from_json_via_parse(j, val);
	}

	static void from_json(const json& j, value<bool>& val)
	{
		assert(is_tt_value(j));
		assert(j["type"] == "bool");

		from_json_via_parse(j, val);
	}

	static void from_json(const json& j, value<date>& val)
	{
		assert(is_tt_value(j));
		assert(j["type"] == "date-local");

		from_json_via_parse(j, val);
	}

	static void from_json(const json& j, value<time>& val)
	{
		assert(is_tt_value(j));
		assert(j["type"] == "time-local");

		from_json_via_parse(j, val);
	}

	static void from_json(const json& j, value<date_time>& val)
	{
		assert(is_tt_value(j));
		assert(j["type"] == "datetime-local" || j["type"] == "datetime");

		from_json_via_parse(j, val);
	}

	static void from_json(const json&, array&);

	template <typename T, typename Key>
	static void insert_from_json(table & tbl, Key && key, const json& val)
	{
		T v;
		from_json(val, v);
		tbl.insert_or_assign(static_cast<Key&&>(key), std::move(v));
	}

	template <typename T>
	static void insert_from_json(array & arr, const json& val)
	{
		T v;
		from_json(val, v);
		arr.push_back(std::move(v));
	}

	static void from_json(const json& j, table& tbl)
	{
		assert(j.is_object());
		assert(!is_tt_value(j));

		for (auto& [k, v] : j.items())
		{
			if (v.is_object())
			{
				if (is_tt_value(v))
				{
					if (v["type"] == "string")
						insert_from_json<toml::value<std::string>>(tbl, k, v);
					else if (v["type"] == "integer")
						insert_from_json<toml::value<int64_t>>(tbl, k, v);
					else if (v["type"] == "float")
						insert_from_json<toml::value<double>>(tbl, k, v);
					else if (v["type"] == "bool")
						insert_from_json<toml::value<bool>>(tbl, k, v);
					else if (v["type"] == "date-local")
						insert_from_json<toml::value<date>>(tbl, k, v);
					else if (v["type"] == "time-local")
						insert_from_json<toml::value<time>>(tbl, k, v);
					else if (v["type"] == "datetime-local" || v["type"] == "datetime")
						insert_from_json<toml::value<date_time>>(tbl, k, v);
				}
				else
					insert_from_json<toml::table>(tbl, k, v);
			}
			if (v.is_array())
				insert_from_json<toml::array>(tbl, k, v);
		}
	}

	static void from_json(const json& j, array& arr)
	{
		assert(j.is_array());

		for (auto& v : j)
		{
			if (v.is_object())
			{
				if (is_tt_value(v))
				{
					if (v["type"] == "string")
						insert_from_json<toml::value<std::string>>(arr, v);
					else if (v["type"] == "integer")
						insert_from_json<toml::value<int64_t>>(arr, v);
					else if (v["type"] == "float")
						insert_from_json<toml::value<double>>(arr, v);
					else if (v["type"] == "bool")
						insert_from_json<toml::value<bool>>(arr, v);
					else if (v["type"] == "date-local")
						insert_from_json<toml::value<date>>(arr, v);
					else if (v["type"] == "time-local")
						insert_from_json<toml::value<time>>(arr, v);
					else if (v["type"] == "datetime-local" || v["type"] == "datetime")
						insert_from_json<toml::value<date_time>>(arr, v);
				}
				else
					insert_from_json<toml::table>(arr, v);
			}
			if (v.is_array())
				insert_from_json<toml::array>(arr, v);
		}
	}
}
TOML_NAMESPACE_END;

int main()
{
	try
	{
		const std::string str(std::istreambuf_iterator<char>{ std::cin }, std::istreambuf_iterator<char>{});

		toml::table tbl = json::parse(str);

		std::cout << tbl << "\n";
	}
	catch (const std::exception& exc)
	{
		std::cerr << exc.what() << "\n";
		return 1;
	}
	catch (...)
	{
		std::cerr << "An unspecified error occurred.\n";
		return 1;
	}

	return 0;
}
