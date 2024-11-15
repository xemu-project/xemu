// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

// This example is just a short-n-shiny benchmark.

#include "examples.h"
#include <toml++/toml.h>

using namespace std::string_view_literals;

static constexpr size_t iterations = 10000;

int main(int argc, char** argv)
{
	const auto file_path = std::string(argc > 1 ? std::string_view{ argv[1] } : "benchmark_data.toml"sv);

	// read the file into a string first to remove file I/O from the benchmark
	std::string file_content;
	{
		std::ifstream file(file_path, std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
		if (!file)
		{
			std::cerr << "File '"sv << file_path << "'could not be opened for reading\n"sv;
			return -1;
		}

		const auto file_size = file.tellg();
		if (file_size == -1)
		{
			std::cerr << "File '"sv << file_path << "' could not be opened for reading\n"sv;
			return -1;
		}
		file.seekg(0, std::ifstream::beg);

		file_content.resize(static_cast<size_t>(file_size));
		file.read(file_content.data(), static_cast<std::streamsize>(file_size));
		if (!file.eof() && !file)
		{
			std::cerr << "Failed to read contents of file '"sv << file_path << "'\n"sv;
			return -1;
		}
	}

	// parse once to make sure it isn't garbage
	{
#if TOML_EXCEPTIONS
		try
		{
			const auto result = toml::parse(file_content, file_path);
		}
		catch (const toml::parse_error& err)
		{
			std::cerr << err << "\n";
			return 1;
		}
#else
		const auto result = toml::parse(file_content, file_path);
		if (!result)
		{
			std::cerr << result.error() << "\n";
			return 1;
		}
#endif
	}

	// run the benchmark
	std::cout << "Parsing '"sv << file_path << "' "sv << iterations << " times...\n"sv;

	const auto start = std::chrono::steady_clock::now();
	for (size_t i = 0; i < iterations; i++)
		std::ignore = toml::parse(file_content, file_path);
	const auto cumulative_sec =
		std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count();
	const auto mean_sec = cumulative_sec / static_cast<double>(iterations);
	std::cout << "  total: "sv << cumulative_sec << " s\n"sv
			  << "   mean: "sv << mean_sec << " s\n"sv;

	return 0;
}
