# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

example_args = []
example_args += global_args

examples = [
	'simple_parser',
	'toml_to_json_transcoder',
	'toml_generator',
	'error_printer',
	'parse_benchmark',
	'toml_merger',
]

example_executables = []
foreach example : examples
	example_executables += [[
		example,
		executable(
			example,
			[ example + '.cpp' ],
			cpp_args: example_args,
			dependencies: tomlplusplus_dep,
			override_options: global_overrides
		)
	]]
endforeach
