//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) 2019-2020 Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

// this file is for boilerplate unrelated to the toml++ example learning outcomes.

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif

#include <cstdlib>
#include <ctime>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <chrono>
#include <fstream>
#ifdef _WIN32
#ifdef _MSC_VER
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int);
#pragma comment(lib, "Kernel32.lib")
#else
#include <Windows.h>
#endif
#endif

namespace
{
	static const auto initialize_environment_automagically = []() noexcept
	{
#ifdef _WIN32
		SetConsoleOutputCP(65001); // CP_UTF8
#endif

		std::ios_base::sync_with_stdio(false);
		std::cout << std::boolalpha;

		srand(static_cast<unsigned int>(time(nullptr)));

		return true;
	}();
}

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
