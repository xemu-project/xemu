@mainpage toml++
@image html banner.svg width=1280px
@tableofcontents

<!-- --------------------------------------------------------------------------------------------------------------- -->

@section mainpage-features Features

-   Header-only (optional!)
-   Supports the latest [TOML](https://toml.io/) release ([v1.0.0](https://toml.io/en/v1.0.0)), plus
    optional support for some unreleased TOML features
-   Passes all tests in the [toml-test](https://github.com/BurntSushi/toml-test) suite
-   Supports serializing to JSON and YAML
-   Proper UTF-8 handling (incl. BOM)
-   C++17 (plus some C++20 features where available, e.g. experimental support for char8_t strings)
-   Doesn't require RTTI
-   Works with or without exceptions
-   Tested on Clang (8+), GCC (8+) and MSVC (VS2019)
-   Tested on x64, x86 and ARM

<!-- --------------------------------------------------------------------------------------------------------------- -->

@section mainpage-api-documentation API documentation

You're looking at it! Browse the docs using the links at the top of the page.
You can search from anywhere by pressing the TAB key.

<!-- --------------------------------------------------------------------------------------------------------------- -->

@section mainpage-example Basic examples

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-example-parsing-files Parsing files
Call toml::parse_file() and work with the toml::table you get back, or handle any toml::parse_error that gets thrown:

@cpp
#include <iostream>
#include <toml++/toml.hpp>

int main(int argc, char\*\* argv)
{
toml::table tbl;
try
{
tbl = toml::parse_file(argv[1]);
std::cout << tbl << "\n";
}
catch (const toml::parse_error& err)
{
std::cerr << "Parsing failed:\n" << err << "\n";
return 1;
}

    return 0;

}

@endcpp

@see

-   toml::parse_file()
-   toml::table
-   toml::parse_error

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-example-parsing-strings Parsing strings and iostreams

Call toml::parse() and work with the toml::table you get back, or handle any toml::parse_error that gets thrown:

@godbolt{NsR-xf}

@cpp
#include <iostream>
#include <sstream>
#include <toml++/toml.hpp>
using namespace std::string_view_literals;

int main()
{
static constexpr std::string_view some_toml = R"(
[library]
name = "toml++"
authors = ["Mark Gillard <mark.gillard@outlook.com.au>"]
cpp = 17
)"sv;

    try
    {

// parse directly from a string view:
{
toml::table tbl = toml::parse(some_toml);
std::cout << tbl << "\n";
}

// parse from a string stream:
{
std::stringstream ss{ std::string{ some_toml } };
toml::table tbl = toml::parse(ss);
std::cout << tbl << "\n";
}
}
catch (const toml::parse_error& err)
{
std::cerr << "Parsing failed:\n" << err << "\n";
return 1;
}

    return 0;

}
@endcpp

@out
[library]
authors = [ 'Mark Gillard <mark.gillard@outlook.com.au>' ]
cpp = 17
name = 'toml++'

[library]
authors = [ 'Mark Gillard <mark.gillard@outlook.com.au>' ]
cpp = 17
name = 'toml++'
@endout

@see

-   toml::parse_file()
-   toml::table
-   toml::parse_error

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-example-parsing-without-exceptions Handling errors without exceptions

Can't (or won't) use exceptions? That's fine too. You can disable exceptions in your compiler flags and/or
explicitly disable the library's use of them by setting the option #TOML_EXCEPTIONS to `0`. In either case,
the parsing functions return a toml::parse_result instead of a toml::table:

@cpp
#include <iostream>

#define TOML_EXCEPTIONS 0 // only necessary if you've left them enabled in your compiler
#include <toml++/toml.hpp>

int main()
{
toml::parse_result result = toml::parse_file("configuration.toml");
if (!result)
{
std::cerr << "Parsing failed:\n" << result.error() << "\n";
return 1;
}

    do_stuff_with_your_config(std::move(result).table()); // 'steal' the table from the result
    return 0;

}
@endcpp

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-example-custom-error-formatting Custom error formatting

The examples above use an overloaded `operator<<` with ostreams to print basic error messages, and look like this:
@out
Error while parsing key: expected bare key starting character or string delimiter, saw '?'
(error occurred at line 2, column 5)
@endout

The library doesn't natively support error colouring in TTY environments, but instead provides the requisite information
for you to build that and any other custom error handling yourself if necessary via toml::parse_error's source()
and description() members:

@cpp
toml::table tbl;
try
{
tbl = toml::parse_file("configuration.toml");
}
catch (const toml::parse_error& err)
{
std::cerr
<< "Error parsing file '" << \*err.source().path
<< "':\n" << err.description()
<< "\n (" << err.source().begin << ")\n";
return 1;
}
@endcpp

@see

-   toml::parse_error
-   toml::source_region
-   toml::source_position

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-example-manipulations Working with TOML data

A TOML document is a tree of values, arrays and tables, represented as the toml::value, toml::array
and toml::table, respectively. All three inherit from toml::node, and can be easily accessed via
the toml::node_view:

@godbolt{TnevafTKd}

@cpp
#include <iostream>
#include <toml++/toml.hpp>
using namespace std::string_view_literals;

int main()
{
static constexpr auto source = R"(
str = "hello world"

numbers = [ 1, 2, 3, "four", 5.0 ]
vegetables = [ "tomato", "onion", "mushroom", "lettuce" ]
minerals = [ "quartz", "iron", "copper", "diamond" ]

[animals]
cats = [ "tiger", "lion", "puma" ]
birds = [ "macaw", "pigeon", "canary" ]
fish = [ "salmon", "trout", "carp" ]

    )"sv;
    toml::table tbl = toml::parse(source);

    // different ways of directly querying data
    std::optional<std::string_view> str1 = tbl["str"].value<std::string_view>();
    std::optional<std::string>      str2 = tbl["str"].value<std::string>();
    std::string_view                str3 = tbl["str"].value_or(""sv);
    std::string&                    str4 = tbl["str"].ref<std::string>(); // ~~dangerous~~

    std::cout << *str1 << "\n";
    std::cout << *str2 << "\n";
    std::cout << str3 << "\n";
    std::cout << str4 << "\n";

    // get a toml::node_view of the element 'numbers' using operator[]
    auto numbers = tbl["numbers"];
    std::cout << "table has 'numbers': " << !!numbers << "\n";
    std::cout << "numbers is an: " << numbers.type() << "\n";
    std::cout << "numbers: " << numbers << "\n";

    // get the underlying array object to do some more advanced stuff
    if (toml::array* arr = numbers.as_array())
    {
    	// visitation with for_each() helps deal with heterogeneous data
    	arr->for_each([](auto&& el)
    	{
    		if constexpr (toml::is_number<decltype(el)>)
    			(*el)++;
    		else if constexpr (toml::is_string<decltype(el)>)
    			el = "five"sv;
    	});

// arrays are very similar to std::vector
arr->push_back(7);
arr->emplace_back<toml::array>(8, 9);
std::cout << "numbers: " << numbers << "\n";
}

    // node-views can be chained to quickly query deeper
    std::cout << "cats: " << tbl["animals"]["cats"] << "\n";
    std::cout << "fish[1]: " << tbl["animals"]["fish"][1] << "\n";

    // can also be retrieved via absolute path
    std::cout << "cats: " << tbl.at_path("animals.cats") << "\n";
    std::cout << "fish[1]: " << tbl.at_path("animals.fish[1]") << "\n";

    // ...even if the element doesn't exist
    std::cout << "dinosaurs: " << tbl["animals"]["dinosaurs"] << "\n"; //no dinosaurs :(

    return 0;

}
@endcpp

@out
hello world
hello world
hello world
hello world
table has 'numbers': 1
numbers is an: array
numbers: [ 1, 2, 3, 'four', 5.0 ]
numbers: [ 2, 3, 4, 'five', 6.0, 7, [ 8, 9 ] ]
cats: [ 'tiger', 'lion', 'puma' ]
fish[1]: 'trout'
dinosaurs:
@endout

@see

-   toml::node
-   toml::node_view
-   toml::value
-   toml::array
-   toml::table

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-example-serialization Serializing as TOML, JSON and YAML

All toml++ data types have overloaded `operator<<` for ostreams, so 'serializing' a set of TOML data to actual
TOML is done just by printing it to an ostream. Converting it to JSON and YAML is done in much the same way,
but via a toml::json_formatter and toml::yaml_formatter.

@godbolt{srdfoWMq6}

@cpp
#include <iostream>
#include <toml++/toml.hpp>

int main()
{
auto tbl = toml::table{
{ "lib", "toml++" },
{ "cpp", toml::array{ 17, 20, "and beyond" } },
{ "toml", toml::array{ "1.0.0", "and beyond" } },
{ "repo", "https://github.com/marzer/tomlplusplus/" },
{ "author", toml::table{
{ "name", "Mark Gillard" },
{ "github", "https://github.com/marzer" },
{ "twitter", "https://twitter.com/marzer8789" }
}
},
};

    // serializing as TOML
    std::cout << "###### TOML ######" << "\n\n";
    std::cout << tbl << "\n\n";

    // serializing as JSON using toml::json_formatter:
    std::cout << "###### JSON ######" << "\n\n";
    std::cout << toml::json_formatter{ tbl } << "\n\n";

    // serializing as YAML using toml::yaml_formatter:
    std::cout << "###### YAML ######" << "\n\n";
    std::cout << toml::yaml_formatter{ tbl } << "\n\n";

    return 0;

}
@endcpp

@out

###### TOML

cpp = [ 17, 20, 'and beyond' ]
lib = 'toml++'
repo = 'https://github.com/marzer/tomlplusplus/'
toml = [ '1.0.0', 'and beyond' ]

[author]
github = 'https://github.com/marzer'
name = 'Mark Gillard'
twitter = 'https://twitter.com/marzer8789'

###### JSON

{
"author" : {
"github" : "https://github.com/marzer",
"name" : "Mark Gillard",
"twitter" : "https://twitter.com/marzer8789"
},
"cpp" : [
17,
20,
"and beyond"
],
"lib" : "toml++",
"repo" : "https://github.com/marzer/tomlplusplus/",
"toml" : [
"1.0.0",
"and beyond"
]
}

###### YAML

author:
github: 'https://github.com/marzer'
name: 'Mark Gillard'
twitter: 'https://twitter.com/marzer8789'
cpp:

-   17
-   20
-   'and beyond'
    lib: 'toml++'
    repo: 'https://github.com/marzer/tomlplusplus/'
    toml:
-   '1.0.0'
-   'and beyond'
    @endout

@see

-   toml::toml_formatter
-   toml::json_formatter
-   toml::yaml_formatter

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-example-speed-up-compilation Speeding up compilation

Because toml++ is a header-only library of nontrivial size you might find that compilation times noticeably
increase after you add it to your project, especially if you add the library's header somewhere that's visible from
a large number of translation units. You can counter this by disabling header-only mode and explicitly controlling
where the library's implementation is compiled.

<strong>Step 1: Set #TOML_HEADER_ONLY to [code]0[/code] before including toml++</strong>

This must be the same everywhere, so either set it as a global `#define` in your build system, or
do it manually before including toml++ in some global header that's used everywhere in your project:
@cpp
// global_header_that_includes_toml++.h

#define TOML_HEADER_ONLY 0
#include <toml.hpp>
@endcpp

<strong>Step 2: Define #TOML_IMPLEMENTATION before including toml++ in one specific translation unit</strong>

@cpp
// some_code_file.cpp

#define TOML_IMPLEMENTATION
#include "global_header_that_includes_toml++.hpp"
@endcpp

<strong>Bonus Step: Disable any library features you don't need</strong>

Some library features can be disabled wholesale so you can avoid paying their the compilation cost if you don't need them.
For example, if all you need to do is serialize some code-generated TOML and don't actually need the parser at all you, can
set #TOML_ENABLE_PARSER to `0` to disable the parser altogether. This can yield fairly significant compilation
speedups since the parser accounts for a good chunk of the library's code.

@see @ref configuration

<!-- --------------------------------------------------------------------------------------------------------------- -->

@section mainpage-adding-lib Adding toml++ to your project

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-old-school "The old fashioned way"

@m_class{m-note m-default}

The library comes in two flavours, [emoji icecream] Single-header
and [emoji sundae] Regular. The API is the same for both.

<h3>[emoji icecream] Single-header flavour</h3>
1. Drop [toml.hpp](https://raw.githubusercontent.com/marzer/tomlplusplus/master/toml.hpp) wherever you like in your source tree
2. There is no step two

<h3>[emoji sundae] Regular flavour</h3>
1. Clone \github{marzer/tomlplusplus, the repository} from GitHub
2. Add `tomlplusplus/include` to your include paths
3. `#include <toml++/toml.hpp>`

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-conan Conan

Add `tomlplusplus/3.4.0` to your conanfile.

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-dds DDS
Add `tomlpp` to your `package.json5`, e.g.:
@json
depends: [
'tomlpp^3.4.0',
]
@endjson

@see [What is DDS?](https://dds.pizza/)

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-meson Meson
You can install the wrap with:

@shell
meson wrap install tomlplusplus
@endshell

After that, you can use it like a regular dependency:

@meson
tomlplusplus_dep = dependency('tomlplusplus')
@endmeson

You can also add it as a subproject directly.

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-tipi Tipi.build

`tomlplusplus` can be easily used in [tipi.build](https://tipi.build) projects by adding the following entry to your `.tipi/deps`:

@json
{
"marzer/tomlplusplus": { }
}
@endjson

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-vcpkg Vcpkg

@shell
vcpkg install tomlplusplus
@endshell

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-cmake-fetch-content CMake FetchContent

@cmake
include(FetchContent)
FetchContent_Declare(
tomlplusplus
GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)
@endcmake

@see [What is FetchContent?](https://cmake.org/cmake/help/latest/module/FetchContent.html)

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-git-submodules Git submodules
@shell
git submodule add --depth 1 https://github.com/marzer/tomlplusplus.git tomlplusplus
@endshell

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-other Other environments and package managers

The C++ tooling ecosystem is a fractal nightmare of unbridled chaos so naturally I'm not up-to-speed with all of the
available packaging and integration options. I'm always happy to see new ones supported, though! If there's some
integration you'd like to see and have the technical know-how to make it happen, feel free to make a pull request.

<!-- --------------------------------------------------------------------------------------------------------------- -->

@subsection mainpage-adding-lib-python Special mention: Python

There exists a python wrapper library built around toml++ called
\github{bobfang1992/pytomlpp, pytomlpp} which is, at the time of writing, one of only two natively-compiled
TOML libraries available for python, and thus one of the fastest options available:

@out
Parsing data.toml 5000 times:
pytomlpp: 0.694 s
rtoml: 0.871 s ( 1.25x)
tomli: 2.625 s ( 3.78x)
toml: 5.642 s ( 8.12x)
qtoml: 7.760 s (11.17x)
tomlkit: 32.708 s (47.09x)
@endout

Install it using `pip`:

@shell
pip install pytomlpp
@endshell

Note that I'm not the owner of that project, so if you wish to report a bug relating to the python
implementation please do so at their repository, not on the main toml++ one.

<!-- --------------------------------------------------------------------------------------------------------------- -->

@section mainpage-configuration Library configuration options

The library exposes a number of configuration options in the form of compiler `#defines`. Things like
changing the `optional<T>` type, disabling header-only mode, et cetera. The full list of
configurables can be found on the @ref configuration page.

@see @ref configuration

<!-- --------------------------------------------------------------------------------------------------------------- -->

@section mainpage-contributing Contributing

Contributions are very welcome! Either by \github{marzer/tomlplusplus/issues, reporting issues}
or submitting pull requests. If you wish to submit a pull request,
please see \github{marzer/tomlplusplus/blob/master/CONTRIBUTING.md, CONTRIBUTING}
for all the details you need to get going.

<!-- --------------------------------------------------------------------------------------------------------------- -->

@section mainpage-license License

toml++ is licensed under the terms of the MIT license - see
[LICENSE](https://github.com/marzer/tomlplusplus/blob/master/LICENSE).

@m_class{m-note m-default}

If you're using the single-header version of the library you don't need to explicitly distribute the license file;
it is embedded in the preamble at the top of the header.

<!-- --------------------------------------------------------------------------------------------------------------- -->

@section mainpage-contact Contacting the author

For bug reports and feature requests please use the \github{marzer/tomlplusplus/issues, Github Issues}
system. For anything else you're welcome to reach out via other means. In order of likely response speed:

-   Twitter: [marzer8789](https://twitter.com/marzer8789)
-   Gitter: [marzer/tomlplusplus](https://gitter.im/marzer/tomlplusplus) ("Discord for repos")
-   Email: [mark.gillard@outlook.com.au](mailto:mark.gillard@outlook.com.au)
