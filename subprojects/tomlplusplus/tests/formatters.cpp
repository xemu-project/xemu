// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

namespace
{
	template <typename Formatter, typename T>
	static auto format_to_string(const T& obj,
								 format_flags flags			= Formatter::default_flags,
								 format_flags exclude_flags = format_flags::none)
	{
		std::stringstream ss;
		ss << "*****\n" << Formatter{ obj, flags & ~(exclude_flags) } << "\n*****";
		return ss.str();
	}

	struct char32_printer
	{
		char32_t value;

		friend std::ostream& operator<<(std::ostream& os, const char32_printer& p)
		{
			if (p.value <= U'\x1F')
				return os << '\'' << impl::control_char_escapes[static_cast<size_t>(p.value)] << '\'';
			else if (p.value == U'\x7F')
				return os << "'\\u007F'"sv;
			else if (p.value < 127u)
				return os << '\'' << static_cast<char>(static_cast<uint8_t>(p.value)) << '\'';
			else
				return os << static_cast<uint_least32_t>(p.value);
		}
	};

	struct string_difference
	{
		source_position position;
		size_t index;
		char32_t a, b;

		friend std::ostream& operator<<(std::ostream& os, const string_difference& diff)
		{
			if (diff.a && diff.b && diff.a != diff.b)
				os << char32_printer{ diff.a } << " vs "sv << char32_printer{ diff.b } << " at "sv;
			return os << diff.position << ", index "sv << diff.index;
		}
	};

	static optional<string_difference> find_first_difference(std::string_view str_a, std::string_view str_b) noexcept
	{
		string_difference diff{ { 1u, 1u } };
		impl::utf8_decoder a, b;

		for (size_t i = 0, e = std::min(str_a.length(), str_b.length()); i < e; i++, diff.index++)
		{
			a(static_cast<uint8_t>(str_a[i]));
			b(static_cast<uint8_t>(str_b[i]));
			if (a.has_code_point() != b.has_code_point() || a.error() != b.error())
				return diff;

			if (a.error())
			{
				a.reset();
				b.reset();
				continue;
			}

			if (!a.has_code_point())
				continue;

			if (a.codepoint != b.codepoint)
			{
				diff.a = a.codepoint;
				diff.b = b.codepoint;
				return diff;
			}

			if (a.codepoint == U'\n')
			{
				diff.position.line++;
				diff.position.column = 1u;
			}
			else
				diff.position.column++;
		}
		if (str_a.length() != str_b.length())
			return diff;
		return {};
	}
}

#define CHECK_FORMATTER(formatter, data, expected)                                                                     \
	do                                                                                                                 \
	{                                                                                                                  \
		const auto str	= format_to_string<formatter>(data);                                                           \
		const auto diff = find_first_difference(str, expected);                                                        \
		if (diff)                                                                                                      \
			FORCE_FAIL("string mismatch: "sv << *diff);                                                                \
	}                                                                                                                  \
	while (false)

TEST_CASE("formatters")
{
	const auto data_date = toml::date{ 2021, 11, 2 };
	const auto data_time = toml::time{ 20, 33, 0 };
	const auto data		 = toml::table{
			 { "integers"sv,
			   toml::table{ { "zero"sv, 0 },
							{ "one"sv, 1 },
							{ "dec"sv, 10 },
							{ "bin"sv, 10, toml::value_flags::format_as_binary },
							{ "oct"sv, 10, toml::value_flags::format_as_octal },
							{ "hex"sv, 10, toml::value_flags::format_as_hexadecimal } } },
			 { "floats"sv,
			   toml::table{ { "pos_zero"sv, +0.0 },
							{ "neg_zero"sv, -0.0 },
							{ "one"sv, 1.0 },
							{ "pos_inf"sv, +std::numeric_limits<double>::infinity() },
							{ "neg_inf"sv, -std::numeric_limits<double>::infinity() },
							{ "pos_nan"sv, +std::numeric_limits<double>::quiet_NaN() },
							{ "neg_nan"sv, -std::numeric_limits<double>::quiet_NaN() }

		   } },

			 { "dates and times"sv,
			   toml::table{

				   { "dates"sv, toml::table{ { "val"sv, data_date } } },

				   { "times"sv, toml::table{ { "val"sv, data_time } } },

				   { "date-times"sv,
					 toml::table{

						 { "local"sv, toml::table{ { "val"sv, toml::date_time{ data_date, data_time } } } },
						 { "offset"sv,
						   toml::table{
							   { "val"sv, toml::date_time{ data_date, data_time, toml::time_offset{} } } } } } } } },

			 { "bools"sv,
			   toml::table{ { "true"sv, true }, //
							{ "false"sv, false } } },

			 {
			 "strings"sv,
			 toml::array{ R"()"sv,
						  R"(string)"sv,
						  R"(string with a single quote in it: ')"sv,
						  R"(string with a double quote in it: ")"sv,
						  "string with a tab: \t"sv,
						  R"(a long string to force the array over multiple lines)"sv },
		 },

			 { "a"sv,
			   toml::table{ { "val", true },
							{ "b"sv, toml::table{ { "val", true }, { "c"sv, toml::table{ { "val", true } } } } } } }

	};

	SECTION("toml_formatter")
	{
		static constexpr auto expected = R"(*****
strings = [
    '',
    'string',
    "string with a single quote in it: '",
    'string with a double quote in it: "',
    'string with a tab: 	',
    'a long string to force the array over multiple lines'
]

[a]
val = true

    [a.b]
    val = true

        [a.b.c]
        val = true

[bools]
false = false
true = true

['dates and times'.date-times.local]
val = 2021-11-02T20:33:00

['dates and times'.date-times.offset]
val = 2021-11-02T20:33:00Z

['dates and times'.dates]
val = 2021-11-02

['dates and times'.times]
val = 20:33:00

[floats]
neg_inf = -inf
neg_nan = nan
neg_zero = -0.0
one = 1.0
pos_inf = inf
pos_nan = nan
pos_zero = 0.0

[integers]
bin = 0b1010
dec = 10
hex = 0xA
oct = 0o12
one = 1
zero = 0
*****)"sv;

		CHECK_FORMATTER(toml_formatter, data, expected);
	}

	SECTION("json_formatter")
	{
		static constexpr auto expected = R"(*****
{
    "a" : {
        "b" : {
            "c" : {
                "val" : true
            },
            "val" : true
        },
        "val" : true
    },
    "bools" : {
        "false" : false,
        "true" : true
    },
    "dates and times" : {
        "date-times" : {
            "local" : {
                "val" : "2021-11-02T20:33:00"
            },
            "offset" : {
                "val" : "2021-11-02T20:33:00Z"
            }
        },
        "dates" : {
            "val" : "2021-11-02"
        },
        "times" : {
            "val" : "20:33:00"
        }
    },
    "floats" : {
        "neg_inf" : "-Infinity",
        "neg_nan" : "NaN",
        "neg_zero" : -0.0,
        "one" : 1.0,
        "pos_inf" : "Infinity",
        "pos_nan" : "NaN",
        "pos_zero" : 0.0
    },
    "integers" : {
        "bin" : 10,
        "dec" : 10,
        "hex" : 10,
        "oct" : 10,
        "one" : 1,
        "zero" : 0
    },
    "strings" : [
        "",
        "string",
        "string with a single quote in it: '",
        "string with a double quote in it: \"",
        "string with a tab: \t",
        "a long string to force the array over multiple lines"
    ]
}
*****)"sv;

		CHECK_FORMATTER(json_formatter, data, expected);
	}

	SECTION("yaml_formatter")
	{
		static constexpr auto expected = R"(*****
a: 
  b: 
    c: 
      val: true
    val: true
  val: true
bools: 
  false: false
  true: true
'dates and times': 
  date-times: 
    local: 
      val: '2021-11-02T20:33:00'
    offset: 
      val: '2021-11-02T20:33:00Z'
  dates: 
    val: '2021-11-02'
  times: 
    val: '20:33:00'
floats: 
  neg_inf: -.inf
  neg_nan: .NAN
  neg_zero: -0.0
  one: 1.0
  pos_inf: .inf
  pos_nan: .NAN
  pos_zero: 0.0
integers: 
  bin: 10
  dec: 10
  hex: 0xA
  oct: 0o12
  one: 1
  zero: 0
strings: 
  - ''
  - string
  - "string with a single quote in it: '"
  - 'string with a double quote in it: "'
  - "string with a tab: \t"
  - 'a long string to force the array over multiple lines'
*****)"sv;

		CHECK_FORMATTER(yaml_formatter, data, expected);
	}
}
