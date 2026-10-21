# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

assert(build_tests)

test_sources = files(
	'at_path.cpp',
	'conformance_burntsushi_invalid.cpp',
	'conformance_burntsushi_valid.cpp',
	'conformance_iarna_invalid.cpp',
	'conformance_iarna_valid.cpp',
	'formatters.cpp',
	'for_each.cpp',
	'impl_toml.cpp',
	'main.cpp',
	'manipulating_arrays.cpp',
	'manipulating_parse_result.cpp',
	'manipulating_tables.cpp',
	'manipulating_values.cpp',
	'parsing_arrays.cpp',
	'parsing_booleans.cpp',
	'parsing_comments.cpp',
	'parsing_dates_and_times.cpp',
	'parsing_floats.cpp',
	'parsing_integers.cpp',
	'parsing_key_value_pairs.cpp',
	'parsing_spec_example.cpp',
	'parsing_strings.cpp',
	'parsing_tables.cpp',
	'path.cpp',
	'tests.cpp',
	'user_feedback.cpp',
	'using_iterators.cpp',
	'visit.cpp',
	'windows_compat.cpp'
)

test_deps = [ tomlplusplus_dep ]
if not get_option('use_vendored_libs')
	test_deps += dependency('catch2')
endif

#-----------------------------------------------------------------------------------------------------------------------
# tests.exe
#-----------------------------------------------------------------------------------------------------------------------

test_args = []
test_args += global_args
if has_exceptions
	test_args += cpp.get_supported_arguments('-DSHOULD_HAVE_EXCEPTIONS=1')
else
	test_args += cpp.get_supported_arguments('-DSHOULD_HAVE_EXCEPTIONS=0')
endif
if get_option('use_vendored_libs')
	test_args += cpp.get_supported_arguments('-DUSE_VENDORED_LIBS=1')
else
	test_args += cpp.get_supported_arguments('-DUSE_VENDORED_LIBS=0')
endif


test_exe = executable(
	'tomlplusplus_tests',
	test_sources,
	cpp_args: test_args,
	dependencies: test_deps,
	override_options: global_overrides
)

#-----------------------------------------------------------------------------------------------------------------------
# per-locale invocations
#-----------------------------------------------------------------------------------------------------------------------

test_locales = [
	'C',
	'en_US.utf8',
	'ja_JP.utf8',
	'it_IT.utf8',
	'tr_TR.utf8',
	'fi_FI.utf8',
	'fr_FR.utf8',
	'zh_CN.utf8',
	'de_DE.utf8'
]

foreach locale : test_locales
	test(
		'tests - ' + locale,
		test_exe,
		env: ['LC_ALL=' + locale],
		workdir: meson.project_source_root()/'tests'
	)
endforeach

#-----------------------------------------------------------------------------------------------------------------------
# ODR build test
#-----------------------------------------------------------------------------------------------------------------------

# add the ODR-test build project
executable(
	'tomlplusplus_odr_test',
	files( 'odr_test_1.cpp', 'odr_test_2.cpp' ),
	cpp_args: test_args,
	dependencies: test_deps
)
