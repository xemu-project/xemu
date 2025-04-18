// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

#ifdef _MSC_VER
#pragma warning(disable : 4619)
#pragma warning(disable : 5262)
#pragma warning(disable : 5264)
#endif

#define CATCH_CONFIG_RUNNER
#include "lib_catch2.h"
#include <clocale>

#if LEAK_TESTS

#include <iostream>
#include <iomanip>
#include <atomic>
#include "leakproof.h"
using namespace std::string_view_literals;

namespace leakproof
{
	static std::atomic_llong total_created_ = 0LL;
	static std::atomic_llong tables_		= 0LL;
	static std::atomic_llong arrays_		= 0LL;
	static std::atomic_llong values_		= 0LL;

	void table_created() noexcept
	{
		tables_++;
		total_created_++;
	}

	void table_destroyed() noexcept
	{
		tables_--;
	}

	void array_created() noexcept
	{
		arrays_++;
		total_created_++;
	}

	void array_destroyed() noexcept
	{
		arrays_--;
	}

	void value_created() noexcept
	{
		values_++;
		total_created_++;
	}

	void value_destroyed() noexcept
	{
		values_--;
	}
}

#endif // LEAK_TESTS

int main(int argc, char* argv[])
{
#ifdef _WIN32
	SetConsoleOutputCP(65001);
#endif
	std::setlocale(LC_ALL, "");
	std::locale::global(std::locale(""));
	if (auto result = Catch::Session().run(argc, argv))
		return result;

#if LEAK_TESTS
	constexpr auto handle_leak_result = [](std::string_view name, long long count) noexcept
	{
		std::cout << "\n"sv << name << ": "sv << std::right << std::setw(6) << count;
		if (count > 0LL)
			std::cout << " *** LEAK DETECTED ***"sv;
		if (count < 0LL)
			std::cout << " *** UNBALANCED LIFETIME CALLS ***"sv;
		return count == 0LL;
	};
	std::cout << "\n---------- leak test results ----------"sv;
	bool ok = true;
	ok		= handle_leak_result("tables"sv, leakproof::tables_.load()) && ok;
	ok		= handle_leak_result("arrays"sv, leakproof::arrays_.load()) && ok;
	ok		= handle_leak_result("values"sv, leakproof::values_.load()) && ok;
	std::cout << "\n(total objects created: "sv << leakproof::total_created_.load() << ")"sv;
	std::cout << "\n---------------------------------------"sv;
	return ok ? 0 : -1;
#else
	return 0;
#endif
}
