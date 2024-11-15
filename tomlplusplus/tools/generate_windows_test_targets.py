#!/usr/bin/env python3
# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

import sys
import utils
import re
import itertools
from pathlib import Path
from uuid import UUID, uuid5


def main():

	mode_keys = [ '!!debug', '!x86', 'cpplatest', 'unrel', 'noexcept' ]
	modes = [ [] ]
	for n in range(1, len(mode_keys)):
		for combo in itertools.combinations(mode_keys, n):
			modes.append([i for i in combo])
	modes.append(mode_keys)
	for mode in modes:
		if '!x86' not in mode:
			mode.insert(0, '!x64')
		if '!!debug' not in mode:
			mode.insert(0, '!!release')
		mode.sort()
		for i in range(0, len(mode)):
			while mode[i].startswith('!'):
				mode[i] = mode[i][1:]
	modes.sort()

	test_root = Path(utils.entry_script_dir(), '..', 'tests', 'vs').resolve()
	uuid_namespace = UUID('{51C7001B-048C-4AF0-B598-D75E78FF31F0}')
	configuration_name = lambda x: 'Debug' if x.lower() == 'debug' else 'Release'
	platform_name = lambda x: 'Win32' if x == 'x86' else x
	for mode in modes:
		file_path = Path(test_root, 'test_{}.vcxproj'.format('_'.join(mode)))
		print(f"Writing to {file_path}")
		with open(file_path, 'w', encoding='utf-8-sig', newline='\r\n') as file:
			write = lambda txt: print(txt, file=file)
			write(r'''
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
	<ItemGroup Label="ProjectConfigurations">
		<ProjectConfiguration Include="{configuration}|{platform}">
			<Configuration>{configuration}</Configuration>
			<Platform>{platform}</Platform>
		</ProjectConfiguration>
	</ItemGroup>
	<PropertyGroup Label="Globals">
		<VCProjectVersion>16.0</VCProjectVersion>
		<ProjectGuid>{{{uuid}}}</ProjectGuid>
		<WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
		<PreferredToolArchitecture>x64</PreferredToolArchitecture>
	</PropertyGroup>
	<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
	<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|{platform}'" Label="Configuration">
		<ConfigurationType>Application</ConfigurationType>
		<UseDebugLibraries>true</UseDebugLibraries>
		<PlatformToolset>v143</PlatformToolset>
		<CharacterSet>MultiByte</CharacterSet>
	</PropertyGroup>
	<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|{platform}'" Label="Configuration">
		<ConfigurationType>Application</ConfigurationType>
		<UseDebugLibraries>false</UseDebugLibraries>
		<PlatformToolset>v143</PlatformToolset>
		<WholeProgramOptimization>true</WholeProgramOptimization>
		<CharacterSet>MultiByte</CharacterSet>
	</PropertyGroup>
	<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
	<ImportGroup Label="PropertySheets">
		<Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props"
			Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
	</ImportGroup>
	<Import Project="../../toml++.props" />
	<ItemDefinitionGroup>
		<ClCompile>
			<AdditionalIncludeDirectories>..\tests;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
			<ExceptionHandling>{exceptions}</ExceptionHandling>
			<PrecompiledHeader>Use</PrecompiledHeader>
			<PrecompiledHeaderFile>tests.h</PrecompiledHeaderFile>
			<PreprocessorDefinitions>TOML_ENABLE_UNRELEASED_FEATURES={unreleased_features};%(PreprocessorDefinitions)</PreprocessorDefinitions>
			<PreprocessorDefinitions>LEAK_TESTS=1;%(PreprocessorDefinitions)</PreprocessorDefinitions>
			<PreprocessorDefinitions Condition="'%(ExceptionHandling)'=='false'">_HAS_EXCEPTIONS=0;%(PreprocessorDefinitions)</PreprocessorDefinitions>
			<PreprocessorDefinitions Condition="'%(ExceptionHandling)'=='false'">SHOULD_HAVE_EXCEPTIONS=0;%(PreprocessorDefinitions)</PreprocessorDefinitions>
			<PreprocessorDefinitions Condition="'%(ExceptionHandling)'!='false'">SHOULD_HAVE_EXCEPTIONS=1;%(PreprocessorDefinitions)</PreprocessorDefinitions>
			<LanguageStandard>std{standard}</LanguageStandard>
			<MultiProcessorCompilation>true</MultiProcessorCompilation>
			<WarningLevel>EnableAllWarnings</WarningLevel>
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4127</DisableSpecificWarnings> <!-- conditional expr is constant -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4324</DisableSpecificWarnings> <!-- structure was padded due to alignment specifier -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4464</DisableSpecificWarnings> <!-- relative include path contains '..' -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4505</DisableSpecificWarnings> <!-- unreferenced local function removed -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4514</DisableSpecificWarnings> <!-- unreferenced inline function has been removed -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4577</DisableSpecificWarnings> <!-- 'noexcept' used with no exception handling mode specified -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4582</DisableSpecificWarnings> <!-- constructor is not implicitly called -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4623</DisableSpecificWarnings> <!-- default constructor was implicitly defined as deleted -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4625</DisableSpecificWarnings> <!-- copy constructor was implicitly defined as deleted -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4626</DisableSpecificWarnings> <!-- assignment operator was implicitly defined as deleted -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4710</DisableSpecificWarnings> <!-- function not inlined -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4711</DisableSpecificWarnings> <!-- function selected for automatic expansion -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4738</DisableSpecificWarnings> <!-- storing 32-bit float result in memory -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4820</DisableSpecificWarnings> <!-- N bytes padding added -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4866</DisableSpecificWarnings> <!-- compiler may not enforce ltr eval in operator[] -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4868</DisableSpecificWarnings> <!-- compiler may not enforce ltr eval in initializer list -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);4946</DisableSpecificWarnings> <!-- reinterpret_cast used between related classes -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);5026</DisableSpecificWarnings> <!-- move constructor was implicitly defined as deleted -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);5027</DisableSpecificWarnings> <!-- move assignment operator was implicitly defined as deleted -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);5039</DisableSpecificWarnings> <!-- potentially throwing function passed to 'extern "C"' -->
			<DisableSpecificWarnings>%(DisableSpecificWarnings);5045</DisableSpecificWarnings> <!-- Compiler will insert Spectre mitigation -->
		</ClCompile>
	</ItemDefinitionGroup>
	<PropertyGroup>
		<LocalDebuggerWorkingDirectory>$(ProjectDir)..\</LocalDebuggerWorkingDirectory>
	</PropertyGroup>
	<ItemGroup>
		<ClCompile Include="..\at_path.cpp" />
		<ClCompile Include="..\path.cpp" />
		<ClCompile Include="..\conformance_burntsushi_invalid.cpp" />
		<ClCompile Include="..\conformance_burntsushi_valid.cpp" />
		<ClCompile Include="..\conformance_iarna_invalid.cpp" />
		<ClCompile Include="..\conformance_iarna_valid.cpp" />
		<ClCompile Include="..\for_each.cpp" />
		<ClCompile Include="..\formatters.cpp" />
		<ClCompile Include="..\impl_toml.cpp">
			<PrecompiledHeader>NotUsing</PrecompiledHeader>
		</ClCompile>
		<ClCompile Include="..\main.cpp">
			<PrecompiledHeader>NotUsing</PrecompiledHeader>
		</ClCompile>
		<ClCompile Include="..\manipulating_arrays.cpp" />
		<ClCompile Include="..\manipulating_tables.cpp" />
		<ClCompile Include="..\manipulating_parse_result.cpp" />
		<ClCompile Include="..\manipulating_values.cpp" />
		<ClCompile Include="..\parsing_arrays.cpp" />
		<ClCompile Include="..\parsing_booleans.cpp" />
		<ClCompile Include="..\parsing_comments.cpp" />
		<ClCompile Include="..\parsing_dates_and_times.cpp" />
		<ClCompile Include="..\parsing_floats.cpp" />
		<ClCompile Include="..\parsing_integers.cpp" />
		<ClCompile Include="..\parsing_key_value_pairs.cpp" />
		<ClCompile Include="..\parsing_spec_example.cpp" />
		<ClCompile Include="..\parsing_strings.cpp" />
		<ClCompile Include="..\parsing_tables.cpp" />
		<ClCompile Include="..\tests.cpp">
			<PrecompiledHeader>Create</PrecompiledHeader>
		</ClCompile>
		<ClCompile Include="..\user_feedback.cpp" />
		<ClCompile Include="..\using_iterators.cpp" />
		<ClCompile Include="..\visit.cpp" />
		<ClCompile Include="..\windows_compat.cpp" />
	</ItemGroup>
	<ItemGroup>
		<Natvis Include="..\..\toml++.natvis" />
	</ItemGroup>
	<ItemGroup>
		<ClInclude Include="..\leakproof.h" />
		<ClInclude Include="..\lib_catch2.h" />
		<ClInclude Include="..\lib_tloptional.h" />
		<ClInclude Include="..\settings.h" />
		<ClInclude Include="..\tests.h" />
	</ItemGroup>
	<ItemGroup>
		<None Include="..\cpp.hint" />
		<None Include="..\meson.build" />
	</ItemGroup>
	<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
			'''.strip().format(
				configuration=next(configuration_name(x) for x in mode if x in ('debug', 'release')),
				platform=next(platform_name(x) for x in mode if x in ('x64', 'x86')),
				uuid=str(uuid5(uuid_namespace, '_'.join(mode))).upper(),
				exceptions='false' if 'noexcept' in mode else 'Sync',
				unreleased_features=1 if 'unrel' in mode else 0,
				standard='cpplatest' if 'cpplatest' in mode else 'cpp17'
			))

if __name__ == '__main__':
	utils.run(main, verbose=True)
