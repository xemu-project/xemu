// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "tests.h"

bool parsing_should_succeed(std::string_view test_file,
							uint32_t test_line,
							std::string_view toml_str,
							pss_func&& func,
							std::string_view source_path)
{
	INFO("["sv << test_file << ", line "sv << test_line << "] "sv
			   << "parsing_should_succeed(\""sv << toml_str << "\")"sv)

	constexpr auto validate_table = [](table&& tabl, std::string_view path) -> table&&
	{
		INFO("Validating table source information"sv)
		CHECK(tabl.source().begin != source_position{});
		CHECK(tabl.source().end != source_position{});
		if (path.empty())
			CHECK(tabl.source().path == nullptr);
		else
		{
			REQUIRE(tabl.source().path != nullptr);
			CHECK(*tabl.source().path == path);
		}
		return std::move(tabl);
	};

#if TOML_EXCEPTIONS

	try
	{
		{
			INFO("Parsing string directly"sv)
			if (func)
				func(validate_table(toml::parse(toml_str, source_path), source_path));
			else
				validate_table(toml::parse(toml_str, source_path), source_path);
		}
		{
			INFO("Parsing from a string stream"sv)
			std::stringstream ss;
			ss.write(toml_str.data(), static_cast<std::streamsize>(toml_str.length()));
			if (func)
				func(validate_table(toml::parse(ss, source_path), source_path));
			else
				validate_table(toml::parse(ss, source_path), source_path);
		}
	}
	catch (const parse_error& err)
	{
		FORCE_FAIL("Parse error on line "sv << err.source().begin.line << ", column "sv << err.source().begin.column
											<< ":\n"sv << err.description());
		return false;
	}

#else

	{
		INFO("Parsing string directly"sv)
		parse_result result = toml::parse(toml_str, source_path);
		if (result)
		{
			if (func)
				func(validate_table(std::move(result), source_path));
			else
				validate_table(std::move(result), source_path);
		}
		else
		{
			FORCE_FAIL("Parse error on line "sv << result.error().source().begin.line << ", column "sv
												<< result.error().source().begin.column << ":\n"sv
												<< result.error().description());
		}
	}

	{
		INFO("Parsing from a string stream"sv)
		std::stringstream ss;
		ss.write(toml_str.data(), static_cast<std::streamsize>(toml_str.length()));
		parse_result result = toml::parse(ss, source_path);
		if (result)
		{
			if (func)
				func(validate_table(std::move(result), source_path));
			else
				validate_table(std::move(result), source_path);
		}
		else
		{
			FORCE_FAIL("Parse error on line "sv << result.error().source().begin.line << ", column "sv
												<< result.error().source().begin.column << ":\n"sv
												<< result.error().description());
		}
	}

#endif

	return true;
}

bool parsing_should_fail(std::string_view test_file,
						 uint32_t test_line,
						 std::string_view toml_str,
						 source_index expected_failure_line,
						 source_index expected_failure_column)
{
	INFO("["sv << test_file << ", line "sv << test_line << "] "sv
			   << "parsing_should_fail(\""sv << toml_str << "\")"sv)

#if TOML_EXCEPTIONS

	static constexpr auto run_tests = [](source_index ex_line, source_index ex_col, auto&& fn)
	{
		try
		{
			fn();
		}
		catch (const parse_error& err)
		{
			if (ex_line != static_cast<source_index>(-1) && err.source().begin.line != ex_line)
			{
				FORCE_FAIL("Expected parse_error at line "sv << ex_line << ", actually occured at line "sv
															 << err.source().begin.line);
				return false;
			}

			if (ex_col != static_cast<source_index>(-1) && err.source().begin.column != ex_col)
			{
				FORCE_FAIL("Expected parse_error at column "sv << ex_col << ", actually occured at column "sv
															   << err.source().begin.column);
				return false;
			}

			SUCCEED("parse_error thrown OK"sv);
			return true;
		}
		catch (const std::exception& exc)
		{
			FORCE_FAIL("Expected parsing failure, saw exception: "sv << exc.what());
			return false;
		}
		catch (...)
		{
			FORCE_FAIL("Expected parsing failure, saw unspecified exception"sv);
			return false;
		}

		FORCE_FAIL("Expected parsing failure"sv);
		return false;
	};

	auto result = run_tests(expected_failure_line,
							expected_failure_column,
							[=]() { [[maybe_unused]] auto res = toml::parse(toml_str); });
	result		= result
		  && run_tests(expected_failure_line,
					   expected_failure_column,
					   [=]()
					   {
						   std::stringstream ss;
						   ss.write(toml_str.data(), static_cast<std::streamsize>(toml_str.length()));
						   [[maybe_unused]] auto res = toml::parse(ss);
					   });
	return result;

#else

	static constexpr auto run_tests = [](source_index ex_line, source_index ex_col, auto&& fn)
	{
		if (parse_result result = fn(); !result)
		{
			if (ex_line != static_cast<source_index>(-1) && result.error().source().begin.line != ex_line)
			{
				FORCE_FAIL("Expected parse_error at line "sv << ex_line << ", actually occured at line "sv
															 << result.error().source().begin.line);
			}

			if (ex_col != static_cast<source_index>(-1) && result.error().source().begin.column != ex_col)
			{
				FORCE_FAIL("Expected parse_error at column "sv << ex_col << ", actually occured at column "sv
															   << result.error().source().begin.column);
			}

			SUCCEED("parse_error generated OK"sv);
			return true;
		}

		FORCE_FAIL("Expected parsing failure"sv);
	};

	return run_tests(expected_failure_line, expected_failure_column, [=]() { return toml::parse(toml_str); })
		&& run_tests(expected_failure_line,
					 expected_failure_column,
					 [=]()
					 {
						 std::stringstream ss;
						 ss.write(toml_str.data(), static_cast<std::streamsize>(toml_str.length()));
						 return toml::parse(ss);
					 });

#endif
}

template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const int&);
template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const unsigned int&);
template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const bool&);
template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const float&);
template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const double&);
template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const std::string_view&);

namespace std
{
	template class unique_ptr<const Catch::IExceptionTranslator>;
}
namespace Catch
{
	template struct StringMaker<node_view<node>>;
	template struct StringMaker<node_view<const node>>;
	template ReusableStringStream& ReusableStringStream::operator<<(node_view<node> const&);
	template ReusableStringStream& ReusableStringStream::operator<<(node_view<const node> const&);
	namespace Detail
	{
		template std::string stringify(const node_view<node>&);
		template std::string stringify(const node_view<const node>&);
	}
}
