# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

assert(build_lib) # not header-only mode

# These are the arguments needed to compile and consume the library, and
# are exposed to users with  the `compile_args` kwarg of declare_dependency()
lib_args = cpp.get_supported_arguments('-DTOML_HEADER_ONLY=0')
if get_option('default_library') != 'static'
	lib_args += cpp.get_supported_arguments('-DTOML_SHARED_LIB=1')
endif
if unreleased_features
	lib_args += cpp.get_supported_arguments('-DTOML_ENABLE_UNRELEASED_FEATURES=1')
endif

# these are the _internal_ args, just for compiling the lib
lib_internal_args = []
lib_internal_args += global_args
lib_internal_args += lib_args
if is_pedantic and is_release
	lib_internal_args += cpp.get_supported_arguments(
		'-Wsuggest-attribute=const',
		'-Wsuggest-attribute=pure'
	)
endif

tomlplusplus_lib = library(
	meson.project_name(),
	files('toml.cpp'),
	cpp_args: lib_internal_args,
	gnu_symbol_visibility: get_option('default_library') == 'static' ? '' : 'hidden',
	include_directories: include_dir,
	install: not is_subproject,
	version: meson.project_version(),
	override_options: global_overrides
)

tomlplusplus_dep = declare_dependency(
	compile_args: lib_args,
	include_directories: include_dir,
	link_with: tomlplusplus_lib
)

if not is_subproject
	import('pkgconfig').generate(
		tomlplusplus_lib,
		description: 'TOML config file parser and serializer for C++',
		extra_cflags: lib_args,
		url: 'https://marzer.github.io/tomlplusplus'
	)
endif

# cmake
if get_option('generate_cmake_config') and not is_subproject and not is_devel
	cmake = import('cmake')
	cmake.write_basic_package_version_file(
		name: meson.project_name(),
		version: meson.project_version(),
	)

	# This gets the full path of the library, then considers just the last
	# component (i.e. the actual file name), and finally removes the
	# version suffix from it, because users _should_ link against the .so
	# file, as opposed to the .so.x.y.z one.
	lib_name = tomlplusplus_lib.full_path().split('/')[-1]
	lib_name = lib_name.replace('.' + meson.project_version(), '')

	# CMake needs space-separated values since it doesn't have types
	cmake_compile_options = ''
	foreach arg : lib_args
		cmake_compile_options += arg + ' '
	endforeach
	cmake_compile_options = cmake_compile_options.strip()

	cmake.configure_package_config_file(
		name: meson.project_name(),
		input: '..'/'cmake'/'tomlplusplusConfig.cmake.meson.in',
		configuration: configuration_data({
			'compile_library': true,
			'compile_options': cmake_compile_options,
			'includedir': get_option('includedir'),
			'libdir': get_option('libdir'),
			'lib_name': lib_name
		})
	)
endif
