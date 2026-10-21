# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

assert(build_tt)

tt_deps = [ tomlplusplus_dep ]
if not get_option('use_vendored_libs')
	tt_deps += dependency('nlohmann_json', fallback: ['json', 'nlohmann_json_dep'])
endif

tt_args = []
tt_args += global_args
if get_option('use_vendored_libs')
	tt_args += cpp.get_supported_arguments('-DUSE_VENDORED_LIBS=1')
else
	tt_args += cpp.get_supported_arguments('-DUSE_VENDORED_LIBS=0')
endif

executable(
	'tt_encoder',
	files('tt_encoder.cpp'),
	cpp_args: tt_args,
	dependencies: tt_deps,
	override_options: global_overrides
)
executable(
	'tt_decoder',
	files('tt_decoder.cpp'),
	cpp_args: tt_args,
	dependencies: tt_deps,
	override_options: global_overrides
)
