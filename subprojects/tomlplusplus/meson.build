# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

project(
	'tomlplusplus',
	'cpp',
	license: 'MIT',
	version: '3.4.0',
	meson_version: '>=0.61.0',
	default_options: [
		# https://mesonbuild.com/Builtin-options.html
		# core options
		'buildtype=release',
		'default_library=shared',
		# base options
		'b_lto=false',
		'b_ndebug=if-release',
		# compiler options
		'cpp_std=c++17'
	]
)

#-----------------------------------------------------------------------------------------------------------------------
# global vars + imports
#-----------------------------------------------------------------------------------------------------------------------

compiler = meson.get_compiler('cpp')
message('target cpu_family: @0@'.format(host_machine.cpu_family()))
message('target cpu: @0@'.format(host_machine.cpu()))
message('target system: @0@'.format(host_machine.system()))
message('target endian: @0@'.format(host_machine.endian()))

is_devel = get_option('devel')
is_debug = get_option('debug')
is_release = not is_debug
is_pedantic = get_option('pedantic') or is_devel
is_windows = host_machine.system() == 'windows'
is_x64 = host_machine.cpu_family() == 'x86_64'
is_subproject = meson.is_subproject()

cpp = meson.get_compiler('cpp')
is_gcc = cpp.get_id() == 'gcc'
is_clang = cpp.get_id() == 'clang'
is_msvc = cpp.get_id() == 'msvc'
is_icc_cl = cpp.get_id() == 'intel-cl'
is_icc = is_icc_cl or cpp.get_id() == 'intel'
is_lld = cpp.get_linker_id() == 'ld.lld'
has_exceptions = get_option('cpp_eh') != 'none'
unreleased_features = get_option('unreleased_features')

build_tests = (get_option('build_tests') or is_devel) and not is_subproject
build_examples = (get_option('build_examples') or is_devel) and not is_subproject
build_tt = (get_option('build_tt') or is_devel) and not is_subproject and has_exceptions and not unreleased_features
build_lib = get_option('build_lib') or get_option('compile_library') or build_tests or build_examples or build_tt

#-----------------------------------------------------------------------------------------------------------------------
# global_args
#
# these are the arguments common to everything in the project
# *** they are not forwarded to dependents when using this as a submodule. ***
#-----------------------------------------------------------------------------------------------------------------------

global_args = cpp.get_supported_arguments(
	# clang/gcc
	'-ferror-limit=5',
	'-fmax-errors=5',
	'-Wno-unused-command-line-argument',
	'-Wno-reserved-macro-identifier',
	'-Wno-init-list-lifetime',
	'-fchar8_t',
	# msvc
	'/bigobj',
	'/Gy', # function-level linking
	'/GF', # string pooling
	'/openmp-',
	'/permissive-',
	'/utf-8',
	'/volatile:iso',
	'/Zc:__cplusplus',
	'/Zc:inline',
	'/Zc:externConstexpr',
	'/Zc:preprocessor'
)
if has_exceptions
	global_args += cpp.get_supported_arguments('/Zc:throwingNew', '-D_HAS_EXCEPTIONS=1')
else
	global_args += cpp.get_supported_arguments('-D_HAS_EXCEPTIONS=0')
endif
if is_pedantic
	global_args += cpp.get_supported_arguments(
		# clang
		'-Weverything',
		# gcc
		'-Wcast-align',
		'-Wcast-qual',
		'-Wctor-dtor-privacy',
		'-Wdisabled-optimization',
		'-Wfloat-equal',
		'-Wimport',
		'-Winit-self',
		'-Wlogical-op',
		'-Wmissing-declarations',
		'-Wmissing-field-initializers',
		'-Wmissing-format-attribute',
		'-Wmissing-include-dirs',
		'-Wmissing-noreturn',
		'-Wold-style-cast',
		'-Woverloaded-virtual',
		'-Wpacked',
		'-Wpointer-arith',
		'-Wredundant-decls',
		'-Wshadow',
		'-Wsign-conversion',
		'-Wsign-promo',
		'-Wstack-protector',
		'-Wstrict-null-sentinel',
		'-Wswitch-default',
		'-Wswitch-enum',
		'-Wundef',
		'-Wunreachable-code',
		'-Wunused',
		'-Wunused-parameter',
		'-Wuseless-cast',
		'-Wvariadic-macros',
		'-Wwrite-strings',
		'-Wmissing-noreturn'
	)
endif
# unnecessary pedantry:
global_args += cpp.get_supported_arguments(
	'-Wno-c++98-compat',
	'-Wno-c++98-compat-pedantic',
	'-Wno-documentation',
	'-Wno-documentation-unknown-command',
	'-Wno-switch-enum',
	'-Wno-covered-switch-default',
	'-Wno-padded',
	'-Wno-float-equal'
)
if get_option('time_trace')
	global_args += cpp.get_supported_arguments('-ftime-trace')
endif

#-----------------------------------------------------------------------------------------------------------------------
# global_link_args
#
# these are the linker arguments common to everything in the projectwhen compiling shared libraries and executables.
# *** they are not forwarded to dependents when using this as a submodule. ***
#-----------------------------------------------------------------------------------------------------------------------

global_link_args = []

if is_release
	global_link_args += cpp.get_supported_link_arguments(
		# msvc
		'/OPT:REF,ICF=3',
		'/INCREMENTAL:NO',
	)
endif

#-----------------------------------------------------------------------------------------------------------------------
# global_overrides
#
# these are the meson overrides common to everything in the project
# *** they are not forwarded to dependents when using this as a submodule. ***
#-----------------------------------------------------------------------------------------------------------------------

global_overrides = [ ]
if is_pedantic
	global_overrides += [
		'warning_level=3',
		'werror=true',
		]
endif

#-----------------------------------------------------------------------------------------------------------------------
# subdirectories + files
#-----------------------------------------------------------------------------------------------------------------------

public_headers = []
internal_headers = []

# Empty dependency that will be filled either in src/ or include/
tomlplusplus_dep = dependency('', required: false)

subdir('include')

if build_lib
	subdir('src')
endif

if build_tests
	subdir('tests')
endif

if build_examples
	subdir('examples')
endif

if build_tt
	subdir('toml-test')
endif

# Allow subproject usage
meson.override_dependency(meson.project_name(), tomlplusplus_dep)
