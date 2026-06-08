// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#include "settings.hpp"
#if !TOML_HEADER_ONLY && !TOML_SHARED_LIB
#define TOML_IMPLEMENTATION
#endif

#if USE_SINGLE_HEADER
#include "../toml.hpp"
#else
#include "../include/toml++/toml.hpp"
#endif

namespace toml
{
	using std::declval;
	using std::is_same_v;

#define CHECK_NODE_TYPE_MAPPING(T, expected)                                                                           \
	static_assert(impl::node_type_of<T> == expected);                                                                  \
	static_assert(impl::node_type_of<T&> == expected);                                                                 \
	static_assert(impl::node_type_of<T&&> == expected);                                                                \
	static_assert(impl::node_type_of<const T> == expected);                                                            \
	static_assert(impl::node_type_of<const T&> == expected);                                                           \
	static_assert(impl::node_type_of<const T&&> == expected);                                                          \
	static_assert(impl::node_type_of<volatile T> == expected);                                                         \
	static_assert(impl::node_type_of<volatile T&> == expected);                                                        \
	static_assert(impl::node_type_of<volatile T&&> == expected);                                                       \
	static_assert(impl::node_type_of<const volatile T> == expected);                                                   \
	static_assert(impl::node_type_of<const volatile T&> == expected);                                                  \
	static_assert(impl::node_type_of<const volatile T&&> == expected)

	CHECK_NODE_TYPE_MAPPING(int64_t, node_type::integer);
	CHECK_NODE_TYPE_MAPPING(double, node_type::floating_point);
	CHECK_NODE_TYPE_MAPPING(std::string, node_type::string);
	CHECK_NODE_TYPE_MAPPING(bool, node_type::boolean);
	CHECK_NODE_TYPE_MAPPING(toml::date, node_type::date);
	CHECK_NODE_TYPE_MAPPING(toml::time, node_type::time);
	CHECK_NODE_TYPE_MAPPING(toml::date_time, node_type::date_time);
	CHECK_NODE_TYPE_MAPPING(toml::array, node_type::array);
	CHECK_NODE_TYPE_MAPPING(toml::table, node_type::table);

#define CHECK_CAN_REPRESENT_NATIVE(T, expected)                                                                        \
	static_assert((impl::value_traits<T>::is_native || impl::value_traits<T>::can_represent_native) == expected)

	CHECK_CAN_REPRESENT_NATIVE(time, true);
	CHECK_CAN_REPRESENT_NATIVE(date, true);
	CHECK_CAN_REPRESENT_NATIVE(date_time, true);
	CHECK_CAN_REPRESENT_NATIVE(bool, true);
	CHECK_CAN_REPRESENT_NATIVE(int8_t, false);
	CHECK_CAN_REPRESENT_NATIVE(int16_t, false);
	CHECK_CAN_REPRESENT_NATIVE(int32_t, false);
	CHECK_CAN_REPRESENT_NATIVE(int64_t, true);
	CHECK_CAN_REPRESENT_NATIVE(uint8_t, false);
	CHECK_CAN_REPRESENT_NATIVE(uint16_t, false);
	CHECK_CAN_REPRESENT_NATIVE(uint32_t, false);
	CHECK_CAN_REPRESENT_NATIVE(uint64_t, false);
	CHECK_CAN_REPRESENT_NATIVE(float, false);
	CHECK_CAN_REPRESENT_NATIVE(double, true);
#ifdef TOML_INT128
	CHECK_CAN_REPRESENT_NATIVE(TOML_INT128, true);
	CHECK_CAN_REPRESENT_NATIVE(TOML_UINT128, false);
#endif
#if TOML_ENABLE_FLOAT16
	CHECK_CAN_REPRESENT_NATIVE(_Float16, false);
#endif
#ifdef TOML_FLOAT128
	CHECK_CAN_REPRESENT_NATIVE(TOML_FLOAT128, true);
#endif

	CHECK_CAN_REPRESENT_NATIVE(char*, false);
	CHECK_CAN_REPRESENT_NATIVE(char* const, false);
	CHECK_CAN_REPRESENT_NATIVE(char[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const char[2], false);
	CHECK_CAN_REPRESENT_NATIVE(char (&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const char (&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(char (&&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const char (&&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const char*, true);
	CHECK_CAN_REPRESENT_NATIVE(const char* const, true);
	CHECK_CAN_REPRESENT_NATIVE(std::string, true);
	CHECK_CAN_REPRESENT_NATIVE(std::string_view, true);
#if TOML_HAS_CHAR8
	CHECK_CAN_REPRESENT_NATIVE(char8_t*, false);
	CHECK_CAN_REPRESENT_NATIVE(char8_t* const, false);
	CHECK_CAN_REPRESENT_NATIVE(char8_t[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const char8_t[2], false);
	CHECK_CAN_REPRESENT_NATIVE(char8_t (&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const char8_t (&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(char (&&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const char8_t (&&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const char8_t*, true);
	CHECK_CAN_REPRESENT_NATIVE(const char8_t* const, true);
	CHECK_CAN_REPRESENT_NATIVE(std::u8string, true);
	CHECK_CAN_REPRESENT_NATIVE(std::u8string_view, true);
#endif
	CHECK_CAN_REPRESENT_NATIVE(wchar_t*, false);
	CHECK_CAN_REPRESENT_NATIVE(wchar_t* const, false);
	CHECK_CAN_REPRESENT_NATIVE(wchar_t[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const wchar_t[2], false);
	CHECK_CAN_REPRESENT_NATIVE(wchar_t (&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const wchar_t (&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(wchar_t (&&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const wchar_t (&&)[2], false);
	CHECK_CAN_REPRESENT_NATIVE(const wchar_t*, false);
	CHECK_CAN_REPRESENT_NATIVE(const wchar_t* const, false);
	CHECK_CAN_REPRESENT_NATIVE(std::wstring, !!TOML_ENABLE_WINDOWS_COMPAT);
	CHECK_CAN_REPRESENT_NATIVE(std::wstring_view, false);

#define CHECK_VALUE_EXACT(T, expected)                                                                                 \
	static_assert(is_same_v<decltype(declval<node>().value_exact<T>()), optional<expected>>);                          \
	static_assert(is_same_v<decltype(declval<node_view<node>>().value_exact<T>()), optional<expected>>);               \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().value_exact<T>()), optional<expected>>)

#define CHECK_VALUE_OR(T, expected)                                                                                    \
	static_assert(is_same_v<decltype(declval<node>().value_or(declval<T>())), expected>);                              \
	static_assert(is_same_v<decltype(declval<node_view<node>>().value_or(declval<T>())), expected>);                   \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().value_or(declval<T>())), expected>)

	CHECK_VALUE_EXACT(time, time);
	CHECK_VALUE_EXACT(date, date);
	CHECK_VALUE_EXACT(date_time, date_time);
	CHECK_VALUE_EXACT(bool, bool);
	CHECK_VALUE_EXACT(double, double);
	CHECK_VALUE_EXACT(int64_t, int64_t);
	CHECK_VALUE_EXACT(const char*, const char*);
	CHECK_VALUE_EXACT(std::string_view, std::string_view);
	CHECK_VALUE_EXACT(std::string, std::string);
#if TOML_HAS_CHAR8
	CHECK_VALUE_EXACT(const char8_t*, const char8_t*);
	CHECK_VALUE_EXACT(std::u8string_view, std::u8string_view);
	CHECK_VALUE_EXACT(std::u8string, std::u8string);
#endif

	CHECK_VALUE_OR(time, time);
	CHECK_VALUE_OR(time&, time);
	CHECK_VALUE_OR(time&&, time);
	CHECK_VALUE_OR(time const, time);
	CHECK_VALUE_OR(date, date);
	CHECK_VALUE_OR(date&, date);
	CHECK_VALUE_OR(date&&, date);
	CHECK_VALUE_OR(date const, date);
	CHECK_VALUE_OR(date_time, date_time);
	CHECK_VALUE_OR(date_time&, date_time);
	CHECK_VALUE_OR(date_time&&, date_time);
	CHECK_VALUE_OR(date_time const, date_time);
	CHECK_VALUE_OR(bool, bool);
	CHECK_VALUE_OR(bool&, bool);
	CHECK_VALUE_OR(bool&&, bool);
	CHECK_VALUE_OR(bool const, bool);
	CHECK_VALUE_OR(int32_t, int32_t);
	CHECK_VALUE_OR(int32_t&, int32_t);
	CHECK_VALUE_OR(int32_t&&, int32_t);
	CHECK_VALUE_OR(int32_t const, int32_t);
	CHECK_VALUE_OR(int64_t, int64_t);
	CHECK_VALUE_OR(int64_t&, int64_t);
	CHECK_VALUE_OR(int64_t&&, int64_t);
	CHECK_VALUE_OR(int64_t const, int64_t);
#ifdef TOML_INT128
	CHECK_VALUE_OR(TOML_INT128, TOML_INT128);
	CHECK_VALUE_OR(TOML_INT128&, TOML_INT128);
	CHECK_VALUE_OR(TOML_INT128&&, TOML_INT128);
	CHECK_VALUE_OR(TOML_INT128 const, TOML_INT128);
	CHECK_VALUE_OR(TOML_UINT128, TOML_UINT128);
	CHECK_VALUE_OR(TOML_UINT128&, TOML_UINT128);
	CHECK_VALUE_OR(TOML_UINT128&&, TOML_UINT128);
	CHECK_VALUE_OR(TOML_UINT128 const, TOML_UINT128);
#endif
	CHECK_VALUE_OR(float, float);
	CHECK_VALUE_OR(float&, float);
	CHECK_VALUE_OR(float&&, float);
	CHECK_VALUE_OR(float const, float);
	CHECK_VALUE_OR(double, double);
	CHECK_VALUE_OR(double&, double);
	CHECK_VALUE_OR(double&&, double);
	CHECK_VALUE_OR(double const, double);
#ifdef TOML_FLOAT128
	CHECK_VALUE_OR(TOML_FLOAT128, TOML_FLOAT128);
	CHECK_VALUE_OR(TOML_FLOAT128&, TOML_FLOAT128);
	CHECK_VALUE_OR(TOML_FLOAT128&&, TOML_FLOAT128);
	CHECK_VALUE_OR(TOML_FLOAT128 const, TOML_FLOAT128);
#endif
	CHECK_VALUE_OR(char*, const char*);
	CHECK_VALUE_OR(char*&, const char*);
	CHECK_VALUE_OR(char*&&, const char*);
	CHECK_VALUE_OR(char* const, const char*);
	CHECK_VALUE_OR(char[2], const char*);
	CHECK_VALUE_OR(char (&)[2], const char*);
	CHECK_VALUE_OR(char (&&)[2], const char*);
	CHECK_VALUE_OR(const char*, const char*);
	CHECK_VALUE_OR(const char*&, const char*);
	CHECK_VALUE_OR(const char*&&, const char*);
	CHECK_VALUE_OR(const char* const, const char*);
	CHECK_VALUE_OR(const char[2], const char*);
	CHECK_VALUE_OR(const char (&)[2], const char*);
	CHECK_VALUE_OR(const char (&&)[2], const char*);
	CHECK_VALUE_OR(std::string_view, std::string_view);
	CHECK_VALUE_OR(std::string_view&, std::string_view);
	CHECK_VALUE_OR(std::string_view&&, std::string_view);
	CHECK_VALUE_OR(const std::string_view, std::string_view);
	CHECK_VALUE_OR(const std::string_view&, std::string_view);
	CHECK_VALUE_OR(const std::string_view&&, std::string_view);
	CHECK_VALUE_OR(std::string, std::string);
	CHECK_VALUE_OR(std::string&, std::string);
	CHECK_VALUE_OR(std::string&&, std::string);
	CHECK_VALUE_OR(const std::string, std::string);
	CHECK_VALUE_OR(const std::string&, std::string);
	CHECK_VALUE_OR(const std::string&&, std::string);
#if TOML_HAS_CHAR8
	CHECK_VALUE_OR(char8_t*, const char8_t*);
	CHECK_VALUE_OR(char8_t*&, const char8_t*);
	CHECK_VALUE_OR(char8_t*&&, const char8_t*);
	CHECK_VALUE_OR(char8_t* const, const char8_t*);
	CHECK_VALUE_OR(char8_t[2], const char8_t*);
	CHECK_VALUE_OR(char8_t (&)[2], const char8_t*);
	CHECK_VALUE_OR(char8_t (&&)[2], const char8_t*);
	CHECK_VALUE_OR(const char8_t*, const char8_t*);
	CHECK_VALUE_OR(const char8_t*&, const char8_t*);
	CHECK_VALUE_OR(const char8_t*&&, const char8_t*);
	CHECK_VALUE_OR(const char8_t* const, const char8_t*);
	CHECK_VALUE_OR(const char8_t[2], const char8_t*);
	CHECK_VALUE_OR(const char8_t (&)[2], const char8_t*);
	CHECK_VALUE_OR(const char8_t (&&)[2], const char8_t*);
	CHECK_VALUE_OR(std::u8string_view, std::u8string_view);
	CHECK_VALUE_OR(std::u8string_view&, std::u8string_view);
	CHECK_VALUE_OR(std::u8string_view&&, std::u8string_view);
	CHECK_VALUE_OR(const std::u8string_view, std::u8string_view);
	CHECK_VALUE_OR(const std::u8string_view&, std::u8string_view);
	CHECK_VALUE_OR(const std::u8string_view&&, std::u8string_view);
	CHECK_VALUE_OR(std::u8string, std::u8string);
	CHECK_VALUE_OR(std::u8string&, std::u8string);
	CHECK_VALUE_OR(std::u8string&&, std::u8string);
	CHECK_VALUE_OR(const std::u8string, std::u8string);
	CHECK_VALUE_OR(const std::u8string&, std::u8string);
	CHECK_VALUE_OR(const std::u8string&&, std::u8string);
#endif
#if TOML_ENABLE_WINDOWS_COMPAT
	CHECK_VALUE_OR(wchar_t*, std::wstring);
	CHECK_VALUE_OR(wchar_t*&, std::wstring);
	CHECK_VALUE_OR(wchar_t*&&, std::wstring);
	CHECK_VALUE_OR(wchar_t* const, std::wstring);
	CHECK_VALUE_OR(wchar_t[2], std::wstring);
	CHECK_VALUE_OR(wchar_t (&)[2], std::wstring);
	CHECK_VALUE_OR(wchar_t (&&)[2], std::wstring);
	CHECK_VALUE_OR(const wchar_t*, std::wstring);
	CHECK_VALUE_OR(const wchar_t*&, std::wstring);
	CHECK_VALUE_OR(const wchar_t*&&, std::wstring);
	CHECK_VALUE_OR(const wchar_t* const, std::wstring);
	CHECK_VALUE_OR(const wchar_t[2], std::wstring);
	CHECK_VALUE_OR(const wchar_t (&)[2], std::wstring);
	CHECK_VALUE_OR(const wchar_t (&&)[2], std::wstring);
	CHECK_VALUE_OR(std::wstring_view, std::wstring);
	CHECK_VALUE_OR(std::wstring_view&, std::wstring);
	CHECK_VALUE_OR(std::wstring_view&&, std::wstring);
	CHECK_VALUE_OR(const std::wstring_view, std::wstring);
	CHECK_VALUE_OR(const std::wstring_view&, std::wstring);
	CHECK_VALUE_OR(const std::wstring_view&&, std::wstring);
	CHECK_VALUE_OR(std::wstring, std::wstring);
	CHECK_VALUE_OR(std::wstring&, std::wstring);
	CHECK_VALUE_OR(std::wstring&&, std::wstring);
	CHECK_VALUE_OR(const std::wstring, std::wstring);
	CHECK_VALUE_OR(const std::wstring&, std::wstring);
	CHECK_VALUE_OR(const std::wstring&&, std::wstring);
#endif

#define CHECK_INSERTED_AS(T, expected)                                                                                 \
	static_assert(std::is_same_v<toml::inserted_type_of<T>, expected>);                                                \
	static_assert(std::is_same_v<toml::inserted_type_of<const T>, expected>);                                          \
	static_assert(std::is_same_v<toml::inserted_type_of<T&>, expected>);                                               \
	static_assert(std::is_same_v<toml::inserted_type_of<const T&>, expected>);                                         \
	static_assert(std::is_same_v<toml::inserted_type_of<T&&>, expected>)

	CHECK_INSERTED_AS(table, table);
	CHECK_INSERTED_AS(array, array);
	CHECK_INSERTED_AS(node, node);
	CHECK_INSERTED_AS(time, value<time>);
	CHECK_INSERTED_AS(date, value<date>);
	CHECK_INSERTED_AS(date_time, value<date_time>);
	CHECK_INSERTED_AS(bool, value<bool>);
	CHECK_INSERTED_AS(int8_t, value<int64_t>);
	CHECK_INSERTED_AS(int16_t, value<int64_t>);
	CHECK_INSERTED_AS(int32_t, value<int64_t>);
	CHECK_INSERTED_AS(int64_t, value<int64_t>);
	CHECK_INSERTED_AS(uint8_t, value<int64_t>);
	CHECK_INSERTED_AS(uint16_t, value<int64_t>);
	CHECK_INSERTED_AS(uint32_t, value<int64_t>);
	CHECK_INSERTED_AS(float, value<double>);
	CHECK_INSERTED_AS(double, value<double>);
#if TOML_ENABLE_FLOAT16
	CHECK_INSERTED_AS(_Float16, value<double>);
#endif

#define CHECK_NODE_REF_TYPE(T)                                                                                         \
	static_assert(is_same_v<decltype(declval<node&>().ref<T>()), T&>);                                                 \
	static_assert(is_same_v<decltype(declval<node&>().ref<const T>()), const T&>);                                     \
	static_assert(is_same_v<decltype(declval<node&>().ref<volatile T>()), volatile T&>);                               \
	static_assert(is_same_v<decltype(declval<node&>().ref<const volatile T>()), const volatile T&>);                   \
	static_assert(is_same_v<decltype(declval<node&>().ref<T&>()), T&>);                                                \
	static_assert(is_same_v<decltype(declval<node&>().ref<const T&>()), const T&>);                                    \
	static_assert(is_same_v<decltype(declval<node&>().ref<volatile T&>()), volatile T&>);                              \
	static_assert(is_same_v<decltype(declval<node&>().ref<const volatile T&>()), const volatile T&>);                  \
	static_assert(is_same_v<decltype(declval<node&>().ref<T&&>()), T&&>);                                              \
	static_assert(is_same_v<decltype(declval<node&>().ref<const T&&>()), const T&&>);                                  \
	static_assert(is_same_v<decltype(declval<node&>().ref<volatile T&&>()), volatile T&&>);                            \
	static_assert(is_same_v<decltype(declval<node&>().ref<const volatile T&&>()), const volatile T&&>);                \
                                                                                                                       \
	static_assert(is_same_v<decltype(declval<node&&>().ref<T>()), T&&>);                                               \
	static_assert(is_same_v<decltype(declval<node&&>().ref<const T>()), const T&&>);                                   \
	static_assert(is_same_v<decltype(declval<node&&>().ref<volatile T>()), volatile T&&>);                             \
	static_assert(is_same_v<decltype(declval<node&&>().ref<const volatile T>()), const volatile T&&>);                 \
	static_assert(is_same_v<decltype(declval<node&&>().ref<T&>()), T&>);                                               \
	static_assert(is_same_v<decltype(declval<node&&>().ref<const T&>()), const T&>);                                   \
	static_assert(is_same_v<decltype(declval<node&&>().ref<volatile T&>()), volatile T&>);                             \
	static_assert(is_same_v<decltype(declval<node&&>().ref<const volatile T&>()), const volatile T&>);                 \
	static_assert(is_same_v<decltype(declval<node&&>().ref<T&&>()), T&&>);                                             \
	static_assert(is_same_v<decltype(declval<node&&>().ref<const T&&>()), const T&&>);                                 \
	static_assert(is_same_v<decltype(declval<node&&>().ref<volatile T&&>()), volatile T&&>);                           \
	static_assert(is_same_v<decltype(declval<node&&>().ref<const volatile T&&>()), const volatile T&&>);               \
                                                                                                                       \
	static_assert(is_same_v<decltype(declval<const node&>().ref<T>()), const T&>);                                     \
	static_assert(is_same_v<decltype(declval<const node&>().ref<const T>()), const T&>);                               \
	static_assert(is_same_v<decltype(declval<const node&>().ref<volatile T>()), const volatile T&>);                   \
	static_assert(is_same_v<decltype(declval<const node&>().ref<const volatile T>()), const volatile T&>);             \
	static_assert(is_same_v<decltype(declval<const node&>().ref<T&>()), const T&>);                                    \
	static_assert(is_same_v<decltype(declval<const node&>().ref<const T&>()), const T&>);                              \
	static_assert(is_same_v<decltype(declval<const node&>().ref<volatile T&>()), const volatile T&>);                  \
	static_assert(is_same_v<decltype(declval<const node&>().ref<const volatile T&>()), const volatile T&>);            \
	static_assert(is_same_v<decltype(declval<const node&>().ref<T&&>()), const T&&>);                                  \
	static_assert(is_same_v<decltype(declval<const node&>().ref<const T&&>()), const T&&>);                            \
	static_assert(is_same_v<decltype(declval<const node&>().ref<volatile T&&>()), const volatile T&&>);                \
	static_assert(is_same_v<decltype(declval<const node&>().ref<const volatile T&&>()), const volatile T&&>);          \
                                                                                                                       \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<T>()), const T&&>);                                   \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<const T>()), const T&&>);                             \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<volatile T>()), const volatile T&&>);                 \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<const volatile T>()), const volatile T&&>);           \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<T&>()), const T&>);                                   \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<const T&>()), const T&>);                             \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<volatile T&>()), const volatile T&>);                 \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<const volatile T&>()), const volatile T&>);           \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<T&&>()), const T&&>);                                 \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<const T&&>()), const T&&>);                           \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<volatile T&&>()), const volatile T&&>);               \
	static_assert(is_same_v<decltype(declval<const node&&>().ref<const volatile T&&>()), const volatile T&&>)

	CHECK_NODE_REF_TYPE(table);
	CHECK_NODE_REF_TYPE(array);
	CHECK_NODE_REF_TYPE(std::string);
	CHECK_NODE_REF_TYPE(int64_t);
	CHECK_NODE_REF_TYPE(double);
	CHECK_NODE_REF_TYPE(bool);
	CHECK_NODE_REF_TYPE(date);
	CHECK_NODE_REF_TYPE(time);
	CHECK_NODE_REF_TYPE(date_time);

#define CHECK_NODE_VIEW_REF_TYPE(T)                                                                                    \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<T>()), T&>);                                       \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<const T>()), const T&>);                           \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<volatile T>()), volatile T&>);                     \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<const volatile T>()), const volatile T&>);         \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<T&>()), T&>);                                      \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<const T&>()), const T&>);                          \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<volatile T&>()), volatile T&>);                    \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<const volatile T&>()), const volatile T&>);        \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<T&&>()), T&&>);                                    \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<const T&&>()), const T&&>);                        \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<volatile T&&>()), volatile T&&>);                  \
	static_assert(is_same_v<decltype(declval<node_view<node>>().ref<const volatile T&&>()), const volatile T&&>)

	CHECK_NODE_VIEW_REF_TYPE(table);
	CHECK_NODE_VIEW_REF_TYPE(array);
	CHECK_NODE_VIEW_REF_TYPE(std::string);
	CHECK_NODE_VIEW_REF_TYPE(int64_t);
	CHECK_NODE_VIEW_REF_TYPE(double);
	CHECK_NODE_VIEW_REF_TYPE(bool);
	CHECK_NODE_VIEW_REF_TYPE(date);
	CHECK_NODE_VIEW_REF_TYPE(time);
	CHECK_NODE_VIEW_REF_TYPE(date_time);

#define CHECK_CONST_NODE_VIEW_REF_TYPE(T)                                                                              \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<T>()), const T&>);                           \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<const T>()), const T&>);                     \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<volatile T>()), const volatile T&>);         \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<const volatile T>()), const volatile T&>);   \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<T&>()), const T&>);                          \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<const T&>()), const T&>);                    \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<volatile T&>()), const volatile T&>);        \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<const volatile T&>()), const volatile T&>);  \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<T&&>()), const T&&>);                        \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<const T&&>()), const T&&>);                  \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<volatile T&&>()), const volatile T&&>);      \
	static_assert(is_same_v<decltype(declval<node_view<const node>>().ref<const volatile T&&>()), const volatile T&&>)

	CHECK_CONST_NODE_VIEW_REF_TYPE(table);
	CHECK_CONST_NODE_VIEW_REF_TYPE(array);
	CHECK_CONST_NODE_VIEW_REF_TYPE(std::string);
	CHECK_CONST_NODE_VIEW_REF_TYPE(int64_t);
	CHECK_CONST_NODE_VIEW_REF_TYPE(double);
	CHECK_CONST_NODE_VIEW_REF_TYPE(bool);
	CHECK_CONST_NODE_VIEW_REF_TYPE(date);
	CHECK_CONST_NODE_VIEW_REF_TYPE(time);
	CHECK_CONST_NODE_VIEW_REF_TYPE(date_time);

}
