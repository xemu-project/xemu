// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#pragma once
#include "settings.hpp"
#include <toml++/toml.hpp>

#if TOML_COMPILER_HAS_EXCEPTIONS ^ SHOULD_HAVE_EXCEPTIONS
#error TOML_COMPILER_HAS_EXCEPTIONS was not deduced correctly
#endif
#if TOML_COMPILER_HAS_EXCEPTIONS ^ TOML_EXCEPTIONS
#error TOML_EXCEPTIONS does not match TOML_COMPILER_HAS_EXCEPTIONS (default behaviour should be to match)
#endif
#if defined(_WIN32) ^ TOML_ENABLE_WINDOWS_COMPAT
#error TOML_ENABLE_WINDOWS_COMPAT does not match _WIN32 (default behaviour should be to match)
#endif
#if TOML_LIB_SINGLE_HEADER ^ USE_SINGLE_HEADER
#error TOML_LIB_SINGLE_HEADER was not set correctly
#endif

#if TOML_ICC
#define UNICODE_LITERALS_OK 0
#else
#define UNICODE_LITERALS_OK 1
#endif

TOML_DISABLE_SPAM_WARNINGS;
TOML_DISABLE_ARITHMETIC_WARNINGS;
#if TOML_CLANG == 13
#pragma clang diagnostic ignored "-Wreserved-identifier" // false-positive
#endif

TOML_DISABLE_WARNINGS;
#include "lib_catch2.hpp"
#include <sstream>
namespace toml
{
}
using namespace Catch::literals;
using namespace toml;
TOML_ENABLE_WARNINGS;

TOML_NODISCARD
TOML_ATTR(const)
TOML_ALWAYS_INLINE
constexpr size_t operator"" _sz(unsigned long long n) noexcept
{
	return static_cast<size_t>(n);
}

#define FILE_LINE_ARGS trim_file_path(std::string_view{ __FILE__ }), __LINE__
#define BOM_PREFIX	   "\xEF\xBB\xBF"

#if TOML_EXCEPTIONS
#define FORCE_FAIL(...) FAIL(__VA_ARGS__)
#else
#define FORCE_FAIL(...)                                                                                                \
	do                                                                                                                 \
	{                                                                                                                  \
		FAIL(__VA_ARGS__);                                                                                             \
		std::exit(-1);                                                                                                 \
		TOML_UNREACHABLE;                                                                                              \
	}                                                                                                                  \
	while (false)
#endif

#define CHECK_SYMMETRIC_RELOP(lhs, op, rhs, result)                                                                    \
	CHECK(((lhs)op(rhs)) == (result));                                                                                 \
	CHECK(((rhs)op(lhs)) == (result))

#define CHECK_SYMMETRIC_EQUAL(lhs, rhs)                                                                                \
	CHECK_SYMMETRIC_RELOP(lhs, ==, rhs, true);                                                                         \
	CHECK_SYMMETRIC_RELOP(lhs, !=, rhs, false)

#define CHECK_SYMMETRIC_INEQUAL(lhs, rhs)                                                                              \
	CHECK_SYMMETRIC_RELOP(lhs, ==, rhs, false);                                                                        \
	CHECK_SYMMETRIC_RELOP(lhs, !=, rhs, true)

template <typename T>
struct type_tag
{
	using type = T;
};

// function_view - adapted from here: https://vittorioromeo.info/index/blog/passing_functions_to_functions.html
template <typename Func>
class function_view;
template <typename R, typename... P>
class function_view<R(P...)> final
{
  private:
	using func_type		   = R(P...);
	using eraser_func_type = R(void*, P&&...);

	mutable void* ptr_				 = {};
	mutable eraser_func_type* eraser = {};

  public:
	function_view() noexcept = default;

	template <typename T>
	function_view(T&& x) noexcept : ptr_{ reinterpret_cast<void*>(std::addressof(x)) }
	{
		eraser = [](void* ptr, P&&... xs) -> R
		{ return (*reinterpret_cast<std::add_pointer_t<std::remove_reference_t<T>>>(ptr))(std::forward<P>(xs)...); };
	}

	decltype(auto) operator()(P&&... xs) const
	{
		return eraser(ptr_, std::forward<P>(xs)...);
	}

	TOML_NODISCARD
	operator bool() const noexcept
	{
		return !!ptr_;
	}
};

using pss_func = function_view<void(table&&)>;

bool parsing_should_succeed(std::string_view test_file,
							uint32_t test_line,
							std::string_view toml_str,
							pss_func&& func				 = {},
							std::string_view source_path = {});

bool parsing_should_fail(std::string_view test_file,
						 uint32_t test_line,
						 std::string_view toml_str,
						 source_index expected_failure_line	  = static_cast<source_index>(-1),
						 source_index expected_failure_column = static_cast<source_index>(-1));

TOML_PURE_GETTER
constexpr std::string_view trim_file_path(std::string_view sv) noexcept
{
	const auto src = std::min(sv.rfind("\\"sv), sv.rfind("/"sv));
	if (src != std::string_view::npos)
		sv = sv.substr(src + 1_sz);
	return sv;
}

template <typename T>
inline bool parse_expected_value(std::string_view test_file,
								 uint32_t test_line,
								 std::string_view value_str,
								 const T& expected)
{
	INFO("["sv << test_file << ", line "sv << test_line << "] "sv
			   << "parse_expected_value(\""sv << value_str << "\")"sv)

	std::string val;
	static constexpr auto key = "val = "sv;
	val.reserve(key.length() + value_str.length());
	val.append(key);
	val.append(value_str);

	static constexpr auto is_val = [](char32_t codepoint)
	{
		if constexpr (impl::node_type_of<T> == node_type::string)
			return codepoint == U'"' || codepoint == U'\'';
		else
			return !impl::is_whitespace(codepoint);
	};

	source_position pos{ 1, static_cast<source_index>(key.length()) };
	source_position begin{}, end{};
	{
		impl::utf8_decoder decoder;
		for (auto c : value_str)
		{
			decoder(static_cast<uint8_t>(c));
			if (!decoder.has_code_point())
				continue;

			if (impl::is_ascii_vertical_whitespace(decoder.codepoint))
			{
				if (decoder.codepoint == U'\n')
				{
					pos.line++;
					pos.column = source_index{ 1 };
				}
				continue;
			}

			pos.column++;
			if (is_val(decoder.codepoint))
			{
				if (!begin)
					begin = pos;
				else
					end = pos;
			}
		}
		if (!end)
			end = begin;
		end.column++;
	}

	using value_type = impl::native_type_of<impl::remove_cvref<T>>;
	value<value_type> val_parsed;
	{
		INFO("["sv << test_file << ", line "sv << test_line << "] "sv
				   << "parse_expected_value: Checking initial parse"sv)

		bool stolen_value = false; // parsing_should_succeed invokes the functor more than once
		const auto result = parsing_should_succeed(
			test_file,
			test_line,
			std::string_view{ val },
			[&](table&& tbl)
			{
				REQUIRE(tbl.size() == 1);
				auto nv = tbl["val"sv];
				REQUIRE(nv);
				REQUIRE(nv.is<value_type>());
				REQUIRE(nv.as<value_type>());
				REQUIRE(nv.type() == impl::node_type_of<T>);
				REQUIRE(nv.node());
				REQUIRE(nv.node()->is<value_type>());
				REQUIRE(nv.node()->as<value_type>());
				REQUIRE(nv.node()->type() == impl::node_type_of<T>);

				// check homogeneity
				REQUIRE(nv.is_homogeneous());
				REQUIRE(nv.is_homogeneous(node_type::none));
				REQUIRE(nv.is_homogeneous(impl::node_type_of<T>));
				REQUIRE(nv.is_homogeneous<value_type>());
				REQUIRE(nv.node()->is_homogeneous());
				REQUIRE(nv.node()->is_homogeneous(node_type::none));
				REQUIRE(nv.node()->is_homogeneous(impl::node_type_of<T>));
				REQUIRE(nv.node()->is_homogeneous<value_type>());
				for (auto nt = impl::unwrap_enum(node_type::table); nt <= impl::unwrap_enum(node_type::date_time); nt++)
				{
					if (node_type{ nt } == impl::node_type_of<T>)
						continue;
					node* first_nonmatch{};
					REQUIRE(!nv.is_homogeneous(node_type{ nt }));
					REQUIRE(!nv.is_homogeneous(node_type{ nt }, first_nonmatch));
					REQUIRE(first_nonmatch == nv.node());
					REQUIRE(!nv.node()->is_homogeneous(node_type{ nt }));
					REQUIRE(!nv.node()->is_homogeneous(node_type{ nt }, first_nonmatch));
					REQUIRE(first_nonmatch == nv.node());
				}

				// check the raw value
				REQUIRE(nv.node()->value<value_type>() == expected);
				REQUIRE(nv.node()->value_or(T{}) == expected);
				REQUIRE(nv.as<value_type>()->get() == expected);
				REQUIRE(nv.value<value_type>() == expected);
				REQUIRE(nv.value_or(T{}) == expected);
				REQUIRE(nv.ref<value_type>() == expected);
				REQUIRE(nv.node()->ref<value_type>() == expected);

				// check the table relops
				REQUIRE(tbl == table{ { { "val"sv, expected } } });
				REQUIRE(!(tbl != table{ { { "val"sv, expected } } }));

				// check value/node relops
				CHECK_SYMMETRIC_EQUAL(*nv.as<value_type>(), *nv.as<value_type>());
				CHECK_SYMMETRIC_EQUAL(*nv.as<value_type>(), expected);
				CHECK_SYMMETRIC_EQUAL(nv, expected);

				// make sure source info is correct
				CHECK_SYMMETRIC_EQUAL(nv.node()->source().begin, begin);
				CHECK_SYMMETRIC_EQUAL(nv.node()->source().end, end);

				// check float identities etc
				if constexpr (std::is_same_v<value_type, double>)
				{
					auto& float_node = *nv.as<value_type>();
					const auto fpcls = impl::fpclassify(*float_node);
					if (fpcls == impl::fp_class::nan)
					{
						CHECK_SYMMETRIC_EQUAL(float_node, std::numeric_limits<double>::quiet_NaN());
						CHECK_SYMMETRIC_INEQUAL(float_node, std::numeric_limits<double>::infinity());
						CHECK_SYMMETRIC_INEQUAL(float_node, -std::numeric_limits<double>::infinity());
						CHECK_SYMMETRIC_INEQUAL(float_node, 1.0);
						CHECK_SYMMETRIC_INEQUAL(float_node, 0.0);
						CHECK_SYMMETRIC_INEQUAL(float_node, -1.0);
					}
					else if (fpcls == impl::fp_class::neg_inf || fpcls == impl::fp_class::pos_inf)
					{
						CHECK_SYMMETRIC_INEQUAL(float_node, std::numeric_limits<double>::quiet_NaN());
						if (fpcls == impl::fp_class::neg_inf)
						{
							CHECK_SYMMETRIC_EQUAL(float_node, -std::numeric_limits<double>::infinity());
							CHECK_SYMMETRIC_INEQUAL(float_node, std::numeric_limits<double>::infinity());
						}
						else
						{
							CHECK_SYMMETRIC_EQUAL(float_node, std::numeric_limits<double>::infinity());
							CHECK_SYMMETRIC_INEQUAL(float_node, -std::numeric_limits<double>::infinity());
						}
						CHECK_SYMMETRIC_INEQUAL(float_node, 1.0);
						CHECK_SYMMETRIC_INEQUAL(float_node, 0.0);
						CHECK_SYMMETRIC_INEQUAL(float_node, -1.0);
					}
					else
					{
						CHECK_SYMMETRIC_INEQUAL(float_node, std::numeric_limits<double>::quiet_NaN());
						CHECK_SYMMETRIC_INEQUAL(float_node, std::numeric_limits<double>::infinity());
						CHECK_SYMMETRIC_INEQUAL(float_node, -std::numeric_limits<double>::infinity());
						CHECK_SYMMETRIC_EQUAL(float_node, *float_node);
						if (std::abs(*float_node) <= 1e10)
						{
							CHECK_SYMMETRIC_INEQUAL(float_node, *float_node + 100.0);
							CHECK_SYMMETRIC_INEQUAL(float_node, *float_node - 100.0);
						}
						CHECK(float_node < std::numeric_limits<double>::infinity());
						CHECK(float_node > -std::numeric_limits<double>::infinity());
					}
				}

				// steal the val for round-trip tests
				if (!stolen_value)
				{
					val_parsed	 = std::move(*nv.as<value_type>());
					stolen_value = true;
				}
			});

		if (!result)
			return false;
	}

	// check round-tripping
	{
		INFO("["sv << test_file << ", line "sv << test_line << "] "sv
				   << "parse_expected_value: Checking round-trip"sv)
		{
			std::string str;
			{
				auto tbl = table{ { { "val"sv, *val_parsed } } };
				std::ostringstream ss;
				ss << tbl;
				str = std::move(ss).str();
			}

			bool value_ok = true;
			const auto parse_ok =
				parsing_should_succeed(test_file,
									   test_line,
									   std::string_view{ str },
									   [&](table&& tbl)
									   {
										   REQUIRE(tbl.size() == 1);
										   auto nv = tbl["val"sv];
										   REQUIRE(nv);
										   REQUIRE(nv.as<value_type>());
										   REQUIRE(nv.node()->type() == impl::node_type_of<T>);

										   if (value_ok && nv.ref<value_type>() != expected)
										   {
											   value_ok = false;
											   FORCE_FAIL("Value was not the same after round-tripping"sv);
										   }
									   });

			if (!parse_ok || value_ok)
				return false;
		}
	}

	return true;
}

// manually instantiate some templates to reduce obj bloat and test compilation time

extern template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const int&);
extern template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const unsigned int&);
extern template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const bool&);
extern template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const float&);
extern template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const double&);
extern template bool parse_expected_value(std::string_view, uint32_t, std::string_view, const std::string_view&);
namespace std
{
	extern template class unique_ptr<const Catch::IExceptionTranslator>;
}
namespace Catch
{
	extern template struct StringMaker<node_view<node>>;
	extern template struct StringMaker<node_view<const node>>;
	extern template ReusableStringStream& ReusableStringStream::operator<<(node_view<node> const&);
	extern template ReusableStringStream& ReusableStringStream::operator<<(node_view<const node> const&);
	namespace Detail
	{
		extern template std::string stringify(const node_view<node>&);
		extern template std::string stringify(const node_view<const node>&);
	}
}

#if TOML_CPP >= 20 && TOML_CLANG && TOML_CLANG <= 14 // https://github.com/llvm/llvm-project/issues/55560

TOML_PUSH_WARNINGS;
TOML_DISABLE_WARNINGS;

namespace
{
	[[maybe_unused]] static std::u8string clang_string_workaround(const char8_t* a, const char8_t* b)
	{
		return { a, b };
	}
}

TOML_POP_WARNINGS;

#endif
