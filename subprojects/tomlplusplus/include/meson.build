# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

include_dir = include_directories('.')

if not is_subproject
	install_subdir('toml++', install_dir: get_option('includedir'))
endif

if not build_lib # header-only mode

	tomlplusplus_dep = declare_dependency(include_directories: include_dir)

	if not is_subproject
		import('pkgconfig').generate(
			name: meson.project_name(),
			description: 'Header-only TOML config file parser and serializer for C++',
			install_dir: get_option('datadir')/'pkgconfig',
			url: 'https://marzer.github.io/tomlplusplus'
		)
	endif

	# cmake
	if get_option('generate_cmake_config') and not is_subproject and not is_devel
		cmake = import('cmake')
		# Can't use until Meson 0.62.0, see https://github.com/mesonbuild/meson/pull/9916
		# and https://github.com/marzer/tomlplusplus/issues/140
		#cmake.write_basic_package_version_file(
		#	name: meson.project_name(),
		#	version: meson.project_version(),
		#	install_dir: get_option('datadir')/'cmake'/meson.project_name(),
		#	arch_independent: true
		#)
		# In the meantime, install a pre-generated Package Version file
		configure_file(
			configuration: {'version': meson.project_version()},
			input: '..'/'cmake'/'tomlplusplusConfigVersion.cmake.meson.in',
			output: 'tomlplusplusConfigVersion.cmake',
			install_dir: get_option('datadir')/'cmake'/meson.project_name()
		)

		cmake.configure_package_config_file(
			name: meson.project_name(),
			input: '..'/'cmake'/'tomlplusplusConfig.cmake.meson.in',
			configuration: configuration_data({'includedir': get_option('includedir')}),
			install_dir: get_option('datadir')/'cmake'/meson.project_name(),
		)
	endif

endif
