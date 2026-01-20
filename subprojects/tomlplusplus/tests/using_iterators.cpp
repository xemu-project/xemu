#include "tests.hpp"

TOML_DISABLE_WARNINGS;
#include <algorithm>
TOML_ENABLE_WARNINGS;

TEST_CASE("using iterators")
{
	constexpr auto data = R"(array=[1,"Foo",true]
string="Bar"
number=5)"sv;
	parsing_should_succeed(
		FILE_LINE_ARGS,
		data,
		[](auto&& tbl)
		{
			const auto tbl_begin = tbl.begin();
			const auto tbl_end	 = tbl.end();

			auto count_table_lambda = [tbl_begin, tbl_end](node_type type) noexcept {
				return std::count_if(tbl_begin,
									 tbl_end,
									 [type](const auto& pair) noexcept { return pair.second.type() == type; });
			};

			CHECK(std::distance(tbl_begin, tbl_end) == 3);
			CHECK(count_table_lambda(node_type::table) == 0);
			CHECK(count_table_lambda(node_type::integer) == 1);
			CHECK(count_table_lambda(node_type::string) == 1);
			CHECK(std::next(tbl_begin, 3) == tbl_end);

			const auto arr_iter =
				std::find_if(tbl_begin, tbl_end, [](const auto& pair) noexcept { return pair.second.is_array(); });

			REQUIRE(arr_iter != tbl_end);
			const auto& arr		 = arr_iter->second.as_array();
			const auto arr_begin = arr->begin();
			const auto arr_end	 = arr->end();

			auto count_array_lambda = [arr_begin, arr_end](node_type type) noexcept {
				return std::count_if(arr_begin,
									 arr_end,
									 [type](const auto& node) noexcept { return node.type() == type; });
			};

			CHECK(std::distance(arr_begin, arr_end) == 3);
			CHECK(count_array_lambda(node_type::table) == 0);
			CHECK(count_array_lambda(node_type::integer) == 1);
			CHECK(count_array_lambda(node_type::string) == 1);
			CHECK(std::next(arr_begin, 2) != arr_end);
		});
}
