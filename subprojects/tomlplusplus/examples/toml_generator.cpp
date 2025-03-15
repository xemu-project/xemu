// This file is a part of toml++ and is subject to the the terms of the MIT license.
// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT

// This example demonstrates the use of some more advanced features to generate a tree of random TOML data.

#include "examples.h"
#include <toml++/toml.h>

using namespace std::string_view_literals;

namespace
{
	namespace random
	{
		inline constexpr std::string_view words[] = {
			"acceptable"sv, "contain"sv,	  "ghost"sv,		 "mark"sv,		  "respect"sv,		 "taboo"sv,
			"actually"sv,	"cream"sv,		  "gleaming"sv,		 "meaty"sv,		  "rest"sv,			 "tacky"sv,
			"addition"sv,	"creature"sv,	  "glorious"sv,		 "memory"sv,	  "rice"sv,			 "tank"sv,
			"adhesive"sv,	"crime"sv,		  "gold"sv,			 "messy"sv,		  "rich"sv,			 "tent"sv,
			"adorable"sv,	"cross"sv,		  "grandfather"sv,	 "miss"sv,		  "righteous"sv,	 "terrible"sv,
			"advise"sv,		"crowded"sv,	  "gusty"sv,		 "modern"sv,	  "room"sv,			 "threatening"sv,
			"afraid"sv,		"crown"sv,		  "haircut"sv,		 "morning"sv,	  "rotten"sv,		 "three"sv,
			"ancient"sv,	"cure"sv,		  "hard-to-find"sv,	 "naughty"sv,	  "royal"sv,		 "ticket"sv,
			"anxious"sv,	"curious"sv,	  "harm"sv,			 "neck"sv,		  "run"sv,			 "title"sv,
			"aromatic"sv,	"curtain"sv,	  "heavy"sv,		 "night"sv,		  "satisfy"sv,		 "torpid"sv,
			"attempt"sv,	"cycle"sv,		  "helpless"sv,		 "nondescript"sv, "scary"sv,		 "train"sv,
			"babies"sv,		"deadpan"sv,	  "high-pitched"sv,	 "overjoyed"sv,	  "scatter"sv,		 "umbrella"sv,
			"bake"sv,		"decisive"sv,	  "hilarious"sv,	 "page"sv,		  "scene"sv,		 "unadvised"sv,
			"ball"sv,		"deeply"sv,		  "history"sv,		 "partner"sv,	  "scintillating"sv, "unbecoming"sv,
			"bat"sv,		"delightful"sv,	  "hook"sv,			 "party"sv,		  "self"sv,			 "unbiased"sv,
			"behave"sv,		"deserted"sv,	  "ignore"sv,		 "pause"sv,		  "selfish"sv,		 "unite"sv,
			"best"sv,		"draconian"sv,	  "imperfect"sv,	 "pear"sv,		  "silky"sv,		 "uptight"sv,
			"birds"sv,		"dreary"sv,		  "impossible"sv,	 "picture"sv,	  "sisters"sv,		 "used"sv,
			"blind"sv,		"dull"sv,		  "incandescent"sv,	 "place"sv,		  "ski"sv,			 "vengeful"sv,
			"blood"sv,		"enthusiastic"sv, "influence"sv,	 "playground"sv,  "skip"sv,			 "versed"sv,
			"blue-eyed"sv,	"equable"sv,	  "innocent"sv,		 "popcorn"sv,	  "snow"sv,			 "vessel"sv,
			"boiling"sv,	"excuse"sv,		  "insidious"sv,	 "prefer"sv,	  "soap"sv,			 "view"sv,
			"bore"sv,		"experience"sv,	  "itch"sv,			 "productive"sv,  "spare"sv,		 "voyage"sv,
			"borrow"sv,		"fabulous"sv,	  "jail"sv,			 "profuse"sv,	  "spicy"sv,		 "wall"sv,
			"broken"sv,		"familiar"sv,	  "kindhearted"sv,	 "protective"sv,  "spiritual"sv,	 "want"sv,
			"capable"sv,	"finger"sv,		  "lackadaisical"sv, "pumped"sv,	  "sprout"sv,		 "weary"sv,
			"charming"sv,	"finicky"sv,	  "laughable"sv,	 "rabbit"sv,	  "squirrel"sv,		 "week"sv,
			"cheerful"sv,	"fix"sv,		  "leather"sv,		 "rapid"sv,		  "stale"sv,		 "whip"sv,
			"chubby"sv,		"flagrant"sv,	  "legal"sv,		 "regret"sv,	  "step"sv,			 "wilderness"sv,
			"clean"sv,		"flat"sv,		  "lewd"sv,			 "reject"sv,	  "stingy"sv,		 "wistful"sv,
			"close"sv,		"flimsy"sv,		  "license"sv,		 "rejoice"sv,	  "string"sv,		 "worried"sv,
			"cobweb"sv,		"fuel"sv,		  "light"sv,		 "relation"sv,	  "sulky"sv,		 "wretched"sv,
			"complex"sv,	"furtive"sv,	  "march"sv,		 "remarkable"sv,  "surprise"sv,		 "zealous"sv,
			"consist"sv,	"geese"sv
		};

		[[nodiscard]] static bool boolean() noexcept
		{
			return std::rand() % 2 == 0;
		}

		template <typename T>
		[[nodiscard]] static T integer(T excl_max) noexcept
		{
			return static_cast<T>(static_cast<T>(std::rand()) % excl_max);
		}

		template <typename T>
		[[nodiscard]] static T integer(T incl_min, T excl_max) noexcept
		{
			return static_cast<T>(incl_min + integer(excl_max - incl_min));
		}

		[[nodiscard]] static bool chance(float val) noexcept
		{
			val = (val < 0.0f ? 0.0f : (val > 1.0f ? 1.0f : val)) * 1000.0f;
			return static_cast<float>(integer(0, 1000)) <= val;
		}

		[[nodiscard]] static toml::date date() noexcept
		{
			return toml::date{ integer(1900, 2021), //
							   integer(1, 13),
							   integer(1, 29) };
		}

		[[nodiscard]] static toml::time time() noexcept
		{
			return toml::time{ integer(24), //
							   integer(60),
							   integer(60),
							   boolean() ? integer(1000000000u) : 0u };
		}

		[[nodiscard]] static toml::time_offset time_offset() noexcept
		{
			return toml::time_offset{ integer(-11, 12), integer(-45, +46) };
		}

		[[nodiscard]] static toml::date_time date_time() noexcept
		{
			return boolean() ? toml::date_time{ date(), time() } //
							 : toml::date_time{ date(), time(), time_offset() };
		}

		[[nodiscard]] static std::string_view word() noexcept
		{
			return words[integer(std::size(words))];
		}

		[[nodiscard]] static std::string string(size_t word_count, char sep = ' ')
		{
			std::string val;
			while (word_count-- > 0u)
			{
				if (!val.empty())
					val += sep;
				val.append(word());
			}
			return val;
		}

		[[nodiscard]] static std::string key()
		{
			return random::string(random::integer(1u, 4u), '-');
		}

	}

	template <typename T>
	static auto add_to(toml::table& tbl, T&& val) -> toml::inserted_type_of<T&&>&
	{
		while (true)
		{
			auto key = random::key();
			auto it	 = std::as_const(tbl).lower_bound(key);
			if (it == tbl.cend() || it->first != key)
			{
				return *tbl.emplace_hint(it, std::move(key), static_cast<T&&>(val))
							->second.template as<toml::inserted_type_of<T&&>>();
			}
		}
	}

	template <typename T>
	static auto add_to(toml::array& arr, T&& val) -> toml::inserted_type_of<T&&>&
	{
		return arr.emplace_back(static_cast<T&&>(val));
	}

	template <typename Container>
	static void add_value_to(Container& container)
	{
		static_assert(toml::is_container<Container>);

		switch (random::integer(7))
		{
			case 0: add_to(container, random::string(random::integer(8u))); break;
			case 1: add_to(container, random::integer(1000)); break;
			case 2: add_to(container, random::integer(10001) / 10000.0); break;
			case 3: add_to(container, random::boolean()); break;
			case 4: add_to(container, random::date()); break;
			case 5: add_to(container, random::time()); break;
			case 6: add_to(container, random::date_time()); break;
			default: break;
		}
	}

	static constexpr int max_inline_nesting		   = 2;
	static constexpr int default_max_inline_values = 4;

	template <typename Container>
	static void populate_inline_container(Container& container,
										  int& budget,
										  int inline_nesting = 0,
										  int max_values	 = default_max_inline_values)
	{
		static_assert(toml::is_container<Container>);

		auto values = random::integer(max_values);
		while (budget && values)
		{
			// inline array/table
			if (inline_nesting < max_inline_nesting && random::chance(0.25f))
			{
				// array
				if (random::boolean())
				{
					auto& arr = add_to(container, toml::array{});
					populate_inline_container(arr, budget, inline_nesting + 1, default_max_inline_values);
				}

				// table
				else
				{
					auto& tbl = add_to(container, toml::table{});
					tbl.is_inline(true);
					populate_inline_container(tbl, budget, inline_nesting + 1, default_max_inline_values);
				}
			}

			// regular value
			else
			{
				add_value_to(container);
				budget--;
			}

			values--;
		}
	}

	static constexpr int max_top_level_nesting		  = 5;
	static constexpr int max_array_of_tables_children = 4;
	static constexpr int max_table_children			  = 4;

	static void populate_table(toml::table& tbl, int& budget, int nesting = 0)
	{
		assert(!tbl.is_inline());

		// do simple values + inline tables/arrays first
		populate_inline_container(tbl, budget, 0, 10);

		// add a nested array-of-tables
		if (budget && nesting < max_top_level_nesting && random::chance(0.33f))
		{
			auto& arr = add_to(tbl, toml::array{});
			// note we don't subtract from the budget for the outer array;
			// it's "invisible" from a topological perspective when reading the output TOML

			auto children = random::integer(1, max_array_of_tables_children);
			while (budget && children)
			{
				auto& sub_tbl = add_to(arr, toml::table{});
				budget--;
				children--;

				populate_table(sub_tbl, budget, nesting + 1);
			}
		}

		// add nested tables
		if (budget && nesting < max_top_level_nesting && random::chance(0.33f))
		{
			auto children = random::integer(1, max_table_children);
			while (budget && children)
			{
				auto& sub_tbl = add_to(tbl, toml::table{});
				budget--;
				children--;

				populate_table(sub_tbl, budget, nesting + 1);
			}
		}
	}
}

int main(int argc, char** argv)
{
	int budget{};
	bool comments = true;
	for (int i = 1; i < argc; i++)
	{
		const auto arg = std::string_view{ argv[i] };
		if (arg == "--nocomments"sv)
			comments = false;
		else
		{
			std::istringstream ss{ std::string(arg) };
			int num;
			if ((ss >> num))
				budget += num;
			else
			{
				std::cerr << "Unknown argument '"sv << arg << "'\n";
				return 1;
			}
		}
	}
	if (budget <= 0)
		budget = 100;

	toml::table root;
	while (budget > 0)
		populate_table(root, budget);

	if (comments)
	{
		std::stringstream src;
		src << root;

		std::string line;
		std::ostringstream dest;
		while (std::getline(src, line))
		{
			if (line.empty())
			{
				// occasionally dump a paragraph comment at the top-level of the document
				if (random::chance(0.20f))
				{
					auto lines = random::integer(1, 8);
					while (lines--)
						dest << "\n# "sv << random::string(random::integer(4u, 8u));
					dest << "\n"sv;
				}
			}
			else
			{
				dest << line;

				// occasionally add short comments at end of non-blank lines
				if (random::chance(0.30f))
					dest << " # "sv << random::string(random::integer(1u, 3u));
			}

			dest << "\n"sv;
		}
		std::cout << dest.str();
	}
	else
	{
		std::cout << root << "\n";
	}
	return 0;
}
