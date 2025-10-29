/*
    __ _____ _____ _____
 __|  |   __|     |   | |  JSON for Modern C++ (test suite)
|  |  |__   |  |  | | | |  version 3.2.0
|_____|_____|_____|_|___|  https://github.com/nlohmann/json

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2013-2018 Niels Lohmann <http://nlohmann.me>.

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "catch.hpp"

#define private public
#include <nlohmann/json.hpp>
using nlohmann::json;

TEST_CASE("JSON pointers")
{
    SECTION("errors")
    {
        CHECK_THROWS_AS(json::json_pointer("foo"), json::parse_error&);
        CHECK_THROWS_WITH(json::json_pointer("foo"),
                          "[json.exception.parse_error.107] parse error at 1: JSON pointer must be empty or begin with '/' - was: 'foo'");

        CHECK_THROWS_AS(json::json_pointer("/~~"), json::parse_error&);
        CHECK_THROWS_WITH(json::json_pointer("/~~"),
                          "[json.exception.parse_error.108] parse error: escape character '~' must be followed with '0' or '1'");

        CHECK_THROWS_AS(json::json_pointer("/~"), json::parse_error&);
        CHECK_THROWS_WITH(json::json_pointer("/~"),
                          "[json.exception.parse_error.108] parse error: escape character '~' must be followed with '0' or '1'");

        json::json_pointer p;
        CHECK_THROWS_AS(p.top(), json::out_of_range&);
        CHECK_THROWS_WITH(p.top(),
                          "[json.exception.out_of_range.405] JSON pointer has no parent");
        CHECK_THROWS_AS(p.pop_back(), json::out_of_range&);
        CHECK_THROWS_WITH(p.pop_back(),
                          "[json.exception.out_of_range.405] JSON pointer has no parent");

        SECTION("array index error")
        {
            json v = {1, 2, 3, 4};
            json::json_pointer ptr("/10e");
            CHECK_THROWS_AS(v[ptr], json::out_of_range&);
            CHECK_THROWS_WITH(v[ptr],
                              "[json.exception.out_of_range.404] unresolved reference token '10e'");
        }
    }

    SECTION("examples from RFC 6901")
    {
        SECTION("nonconst access")
        {
            json j = R"(
            {
                "foo": ["bar", "baz"],
                "": 0,
                "a/b": 1,
                "c%d": 2,
                "e^f": 3,
                "g|h": 4,
                "i\\j": 5,
                "k\"l": 6,
                " ": 7,
                "m~n": 8
            }
            )"_json;

            // the whole document
            CHECK(j[json::json_pointer()] == j);
            CHECK(j[json::json_pointer("")] == j);

            // array access
            CHECK(j[json::json_pointer("/foo")] == j["foo"]);
            CHECK(j[json::json_pointer("/foo/0")] == j["foo"][0]);
            CHECK(j[json::json_pointer("/foo/1")] == j["foo"][1]);
            CHECK(j["/foo/1"_json_pointer] == j["foo"][1]);

            // checked array access
            CHECK(j.at(json::json_pointer("/foo/0")) == j["foo"][0]);
            CHECK(j.at(json::json_pointer("/foo/1")) == j["foo"][1]);

            // empty string access
            CHECK(j[json::json_pointer("/")] == j[""]);

            // other cases
            CHECK(j[json::json_pointer("/ ")] == j[" "]);
            CHECK(j[json::json_pointer("/c%d")] == j["c%d"]);
            CHECK(j[json::json_pointer("/e^f")] == j["e^f"]);
            CHECK(j[json::json_pointer("/g|h")] == j["g|h"]);
            CHECK(j[json::json_pointer("/i\\j")] == j["i\\j"]);
            CHECK(j[json::json_pointer("/k\"l")] == j["k\"l"]);

            // checked access
            CHECK(j.at(json::json_pointer("/ ")) == j[" "]);
            CHECK(j.at(json::json_pointer("/c%d")) == j["c%d"]);
            CHECK(j.at(json::json_pointer("/e^f")) == j["e^f"]);
            CHECK(j.at(json::json_pointer("/g|h")) == j["g|h"]);
            CHECK(j.at(json::json_pointer("/i\\j")) == j["i\\j"]);
            CHECK(j.at(json::json_pointer("/k\"l")) == j["k\"l"]);

            // escaped access
            CHECK(j[json::json_pointer("/a~1b")] == j["a/b"]);
            CHECK(j[json::json_pointer("/m~0n")] == j["m~n"]);

            // unescaped access
            // access to nonexisting values yield object creation
            CHECK_NOTHROW(j[json::json_pointer("/a/b")] = 42);
            CHECK(j["a"]["b"] == json(42));
            CHECK_NOTHROW(j[json::json_pointer("/a/c/1")] = 42);
            CHECK(j["a"]["c"] == json({nullptr, 42}));
            CHECK_NOTHROW(j[json::json_pointer("/a/d/-")] = 42);
            CHECK(j["a"]["d"] == json::array({42}));
            // "/a/b" works for JSON {"a": {"b": 42}}
            CHECK(json({{"a", {{"b", 42}}}})[json::json_pointer("/a/b")] == json(42));

            // unresolved access
            json j_primitive = 1;
            CHECK_THROWS_AS(j_primitive["/foo"_json_pointer], json::out_of_range&);
            CHECK_THROWS_WITH(j_primitive["/foo"_json_pointer],
                              "[json.exception.out_of_range.404] unresolved reference token 'foo'");
            CHECK_THROWS_AS(j_primitive.at("/foo"_json_pointer), json::out_of_range&);
            CHECK_THROWS_WITH(j_primitive.at("/foo"_json_pointer),
                              "[json.exception.out_of_range.404] unresolved reference token 'foo'");
        }

        SECTION("const access")
        {
            const json j = R"(
            {
                "foo": ["bar", "baz"],
                "": 0,
                "a/b": 1,
                "c%d": 2,
                "e^f": 3,
                "g|h": 4,
                "i\\j": 5,
                "k\"l": 6,
                " ": 7,
                "m~n": 8
            }
            )"_json;

            // the whole document
            CHECK(j[json::json_pointer()] == j);
            CHECK(j[json::json_pointer("")] == j);

            // array access
            CHECK(j[json::json_pointer("/foo")] == j["foo"]);
            CHECK(j[json::json_pointer("/foo/0")] == j["foo"][0]);
            CHECK(j[json::json_pointer("/foo/1")] == j["foo"][1]);
            CHECK(j["/foo/1"_json_pointer] == j["foo"][1]);

            // checked array access
            CHECK(j.at(json::json_pointer("/foo/0")) == j["foo"][0]);
            CHECK(j.at(json::json_pointer("/foo/1")) == j["foo"][1]);

            // empty string access
            CHECK(j[json::json_pointer("/")] == j[""]);

            // other cases
            CHECK(j[json::json_pointer("/ ")] == j[" "]);
            CHECK(j[json::json_pointer("/c%d")] == j["c%d"]);
            CHECK(j[json::json_pointer("/e^f")] == j["e^f"]);
            CHECK(j[json::json_pointer("/g|h")] == j["g|h"]);
            CHECK(j[json::json_pointer("/i\\j")] == j["i\\j"]);
            CHECK(j[json::json_pointer("/k\"l")] == j["k\"l"]);

            // checked access
            CHECK(j.at(json::json_pointer("/ ")) == j[" "]);
            CHECK(j.at(json::json_pointer("/c%d")) == j["c%d"]);
            CHECK(j.at(json::json_pointer("/e^f")) == j["e^f"]);
            CHECK(j.at(json::json_pointer("/g|h")) == j["g|h"]);
            CHECK(j.at(json::json_pointer("/i\\j")) == j["i\\j"]);
            CHECK(j.at(json::json_pointer("/k\"l")) == j["k\"l"]);

            // escaped access
            CHECK(j[json::json_pointer("/a~1b")] == j["a/b"]);
            CHECK(j[json::json_pointer("/m~0n")] == j["m~n"]);

            // unescaped access
            CHECK_THROWS_AS(j.at(json::json_pointer("/a/b")), json::out_of_range&);
            CHECK_THROWS_WITH(j.at(json::json_pointer("/a/b")),
                              "[json.exception.out_of_range.403] key 'a' not found");

            // unresolved access
            const json j_primitive = 1;
            CHECK_THROWS_AS(j_primitive["/foo"_json_pointer], json::out_of_range&);
            CHECK_THROWS_WITH(j_primitive["/foo"_json_pointer],
                              "[json.exception.out_of_range.404] unresolved reference token 'foo'");
            CHECK_THROWS_AS(j_primitive.at("/foo"_json_pointer), json::out_of_range&);
            CHECK_THROWS_WITH(j_primitive.at("/foo"_json_pointer),
                              "[json.exception.out_of_range.404] unresolved reference token 'foo'");
        }

        SECTION("user-defined string literal")
        {
            json j = R"(
            {
                "foo": ["bar", "baz"],
                "": 0,
                "a/b": 1,
                "c%d": 2,
                "e^f": 3,
                "g|h": 4,
                "i\\j": 5,
                "k\"l": 6,
                " ": 7,
                "m~n": 8
            }
            )"_json;

            // the whole document
            CHECK(j[""_json_pointer] == j);

            // array access
            CHECK(j["/foo"_json_pointer] == j["foo"]);
            CHECK(j["/foo/0"_json_pointer] == j["foo"][0]);
            CHECK(j["/foo/1"_json_pointer] == j["foo"][1]);
        }
    }

    SECTION("array access")
    {
        SECTION("nonconst access")
        {
            json j = {1, 2, 3};
            const json j_const = j;

            // check reading access
            CHECK(j["/0"_json_pointer] == j[0]);
            CHECK(j["/1"_json_pointer] == j[1]);
            CHECK(j["/2"_json_pointer] == j[2]);

            // assign to existing index
            j["/1"_json_pointer] = 13;
            CHECK(j[1] == json(13));

            // assign to nonexisting index
            j["/3"_json_pointer] = 33;
            CHECK(j[3] == json(33));

            // assign to nonexisting index (with gap)
            j["/5"_json_pointer] = 55;
            CHECK(j == json({1, 13, 3, 33, nullptr, 55}));

            // error with leading 0
            CHECK_THROWS_AS(j["/01"_json_pointer], json::parse_error&);
            CHECK_THROWS_WITH(j["/01"_json_pointer],
                              "[json.exception.parse_error.106] parse error: array index '01' must not begin with '0'");
            CHECK_THROWS_AS(j_const["/01"_json_pointer], json::parse_error&);
            CHECK_THROWS_WITH(j_const["/01"_json_pointer],
                              "[json.exception.parse_error.106] parse error: array index '01' must not begin with '0'");
            CHECK_THROWS_AS(j.at("/01"_json_pointer), json::parse_error&);
            CHECK_THROWS_WITH(j.at("/01"_json_pointer),
                              "[json.exception.parse_error.106] parse error: array index '01' must not begin with '0'");
            CHECK_THROWS_AS(j_const.at("/01"_json_pointer), json::parse_error&);
            CHECK_THROWS_WITH(j_const.at("/01"_json_pointer),
                              "[json.exception.parse_error.106] parse error: array index '01' must not begin with '0'");

            // error with incorrect numbers
            CHECK_THROWS_AS(j["/one"_json_pointer] = 1, json::parse_error&);
            CHECK_THROWS_WITH(j["/one"_json_pointer] = 1,
                              "[json.exception.parse_error.109] parse error: array index 'one' is not a number");
            CHECK_THROWS_AS(j_const["/one"_json_pointer] == 1, json::parse_error&);
            CHECK_THROWS_WITH(j_const["/one"_json_pointer] == 1,
                              "[json.exception.parse_error.109] parse error: array index 'one' is not a number");

            CHECK_THROWS_AS(j.at("/one"_json_pointer) = 1, json::parse_error&);
            CHECK_THROWS_WITH(j.at("/one"_json_pointer) = 1,
                              "[json.exception.parse_error.109] parse error: array index 'one' is not a number");
            CHECK_THROWS_AS(j_const.at("/one"_json_pointer) == 1, json::parse_error&);
            CHECK_THROWS_WITH(j_const.at("/one"_json_pointer) == 1,
                              "[json.exception.parse_error.109] parse error: array index 'one' is not a number");

            CHECK_THROWS_AS(json({{"/list/0", 1}, {"/list/1", 2}, {"/list/three", 3}}).unflatten(), json::parse_error&);
            CHECK_THROWS_WITH(json({{"/list/0", 1}, {"/list/1", 2}, {"/list/three", 3}}).unflatten(),
            "[json.exception.parse_error.109] parse error: array index 'three' is not a number");

            // assign to "-"
            j["/-"_json_pointer] = 99;
            CHECK(j == json({1, 13, 3, 33, nullptr, 55, 99}));

            // error when using "-" in const object
            CHECK_THROWS_AS(j_const["/-"_json_pointer], json::out_of_range&);
            CHECK_THROWS_WITH(j_const["/-"_json_pointer],
                              "[json.exception.out_of_range.402] array index '-' (3) is out of range");

            // error when using "-" with at
            CHECK_THROWS_AS(j.at("/-"_json_pointer), json::out_of_range&);
            CHECK_THROWS_WITH(j.at("/-"_json_pointer),
                              "[json.exception.out_of_range.402] array index '-' (7) is out of range");
            CHECK_THROWS_AS(j_const.at("/-"_json_pointer), json::out_of_range&);
            CHECK_THROWS_WITH(j_const.at("/-"_json_pointer),
                              "[json.exception.out_of_range.402] array index '-' (3) is out of range");
        }

        SECTION("const access")
        {
            const json j = {1, 2, 3};

            // check reading access
            CHECK(j["/0"_json_pointer] == j[0]);
            CHECK(j["/1"_json_pointer] == j[1]);
            CHECK(j["/2"_json_pointer] == j[2]);

            // assign to nonexisting index
            CHECK_THROWS_AS(j.at("/3"_json_pointer), json::out_of_range&);
            CHECK_THROWS_WITH(j.at("/3"_json_pointer),
                              "[json.exception.out_of_range.401] array index 3 is out of range");

            // assign to nonexisting index (with gap)
            CHECK_THROWS_AS(j.at("/5"_json_pointer), json::out_of_range&);
            CHECK_THROWS_WITH(j.at("/5"_json_pointer),
                              "[json.exception.out_of_range.401] array index 5 is out of range");

            // assign to "-"
            CHECK_THROWS_AS(j["/-"_json_pointer], json::out_of_range&);
            CHECK_THROWS_WITH(j["/-"_json_pointer],
                              "[json.exception.out_of_range.402] array index '-' (3) is out of range");
            CHECK_THROWS_AS(j.at("/-"_json_pointer), json::out_of_range&);
            CHECK_THROWS_WITH(j.at("/-"_json_pointer),
                              "[json.exception.out_of_range.402] array index '-' (3) is out of range");
        }

    }

    SECTION("flatten")
    {
        json j =
        {
            {"pi", 3.141},
            {"happy", true},
            {"name", "Niels"},
            {"nothing", nullptr},
            {
                "answer", {
                    {"everything", 42}
                }
            },
            {"list", {1, 0, 2}},
            {
                "object", {
                    {"currency", "USD"},
                    {"value", 42.99},
                    {"", "empty string"},
                    {"/", "slash"},
                    {"~", "tilde"},
                    {"~1", "tilde1"}
                }
            }
        };

        json j_flatten =
        {
            {"/pi", 3.141},
            {"/happy", true},
            {"/name", "Niels"},
            {"/nothing", nullptr},
            {"/answer/everything", 42},
            {"/list/0", 1},
            {"/list/1", 0},
            {"/list/2", 2},
            {"/object/currency", "USD"},
            {"/object/value", 42.99},
            {"/object/", "empty string"},
            {"/object/~1", "slash"},
            {"/object/~0", "tilde"},
            {"/object/~01", "tilde1"}
        };

        // check if flattened result is as expected
        CHECK(j.flatten() == j_flatten);

        // check if unflattened result is as expected
        CHECK(j_flatten.unflatten() == j);

        // error for nonobjects
        CHECK_THROWS_AS(json(1).unflatten(), json::type_error&);
        CHECK_THROWS_WITH(json(1).unflatten(),
                          "[json.exception.type_error.314] only objects can be unflattened");

        // error for nonprimitve values
        CHECK_THROWS_AS(json({{"/1", {1, 2, 3}}}).unflatten(), json::type_error&);
        CHECK_THROWS_WITH(json({{"/1", {1, 2, 3}}}).unflatten(),
        "[json.exception.type_error.315] values in object must be primitive");

        // error for conflicting values
        json j_error = {{"", 42}, {"/foo", 17}};
        CHECK_THROWS_AS(j_error.unflatten(), json::type_error&);
        CHECK_THROWS_WITH(j_error.unflatten(),
                          "[json.exception.type_error.313] invalid value to unflatten");

        // explicit roundtrip check
        CHECK(j.flatten().unflatten() == j);

        // roundtrip for primitive values
        json j_null;
        CHECK(j_null.flatten().unflatten() == j_null);
        json j_number = 42;
        CHECK(j_number.flatten().unflatten() == j_number);
        json j_boolean = false;
        CHECK(j_boolean.flatten().unflatten() == j_boolean);
        json j_string = "foo";
        CHECK(j_string.flatten().unflatten() == j_string);

        // roundtrip for empty structured values (will be unflattened to null)
        json j_array(json::value_t::array);
        CHECK(j_array.flatten().unflatten() == json());
        json j_object(json::value_t::object);
        CHECK(j_object.flatten().unflatten() == json());
    }

    SECTION("string representation")
    {
        for (auto ptr :
                {"", "/foo", "/foo/0", "/", "/a~1b", "/c%d", "/e^f", "/g|h", "/i\\j", "/k\"l", "/ ", "/m~0n"
                })
        {
            CHECK(json::json_pointer(ptr).to_string() == ptr);
        }
    }

    SECTION("conversion")
    {
        SECTION("array")
        {
            json j;
            // all numbers -> array
            j["/12"_json_pointer] = 0;
            CHECK(j.is_array());
        }

        SECTION("object")
        {
            json j;
            // contains a number, but is not a number -> object
            j["/a12"_json_pointer] = 0;
            CHECK(j.is_object());
        }
    }
}
