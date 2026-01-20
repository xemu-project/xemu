#!/usr/bin/env python3
# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

import sys
import utils
import re
import json
import yaml
import math
import dateutil.parser
from pathlib import Path
from datetime import datetime, date, time
from io import StringIO



def sanitize(s):
	s = re.sub(r'[ _:;\/-]+', '_', s, 0, re.I | re.M)
	if s in ('bool', 'float', 'int', 'double', 'auto', 'array', 'table'):
		s = s + '_'
	return s



def is_problematic_control_char(val):
	if isinstance(val, str):
		val = ord(val)
	return (0x00 <= val <= 0x08) or (0x0B <= val <= 0x1F) or val == 0x7F



def has_problematic_control_chars(val):
	for c in val:
		if is_problematic_control_char(c):
			return True
	return False



def requires_unicode(s):
	for c in s:
		if ord(c) > 127:
			return True
	return False



def make_string_literal(val, escape_all = False, escape_any = False):
	get_ord = (lambda c: ord(c)) if isinstance(val, str) else (lambda c: c)
	if escape_all:
		with StringIO() as buf:
			line_len = 0
			for c in val:
				c_ord = get_ord(c)
				if not line_len:
					buf.write('\n\t\t"')
					line_len += 1
				if c_ord <= 0xFF:
					buf.write(rf'\x{c_ord:02X}')
					line_len += 4
				elif c_ord <= 0xFFFF:
					buf.write(rf'\u{c_ord:04X}')
					line_len += 6
				else:
					buf.write(rf'\U{c_ord:08X}')
					line_len += 10
				if line_len >= 100:
					buf.write('"')
					line_len = 0
			if line_len:
				buf.write('"')
			return buf.getvalue()
	elif escape_any:
		with StringIO() as buf:
			buf.write(r'"')
			for c in val:
				c_ord = get_ord(c)
				if c_ord == 0x22: # "
					buf.write(r'\"')
				elif c_ord == 0x5C: # \
					buf.write(r'\\')
				elif c_ord == 0x0A: # \n
					buf.write('\\n"\n\t\t"')
				elif c_ord == 0x0B: # \v
					buf.write(r'\v')
				elif c_ord == 0x0C: # \f
					buf.write(r'\f')
				elif c_ord == 0x0D: # \r
					buf.write(r'\r')
				elif is_problematic_control_char(c_ord):
					if c_ord <= 0xFF:
						buf.write(rf'\x{c_ord:02X}')
					elif c_ord <= 0xFFFF:
						buf.write(rf'\u{c_ord:04X}')
					else:
						buf.write(rf'\U{c_ord:08X}')
				else:
					buf.write(chr(c_ord))
			buf.write(r'"')
			return buf.getvalue()
	else:
		return rf'R"({val})"'




def python_value_to_tomlpp(val):
	if isinstance(val, str):
		if not val:
			return r'""sv'
		elif re.fullmatch(r'^[+-]?[0-9]+[eE][+-]?[0-9]+$', val, re.M):
			return str(float(val))
		else:
			return rf'{make_string_literal(val, escape_any = has_problematic_control_chars(val))}sv'
	elif isinstance(val, bool):
		return 'true' if val else 'false'
	elif isinstance(val, float):
		if math.isinf(val):
			return f'{"-" if val < 0.0 else ""}std::numeric_limits<double>::infinity()'
		elif math.isnan(val):
			return 'std::numeric_limits<double>::quiet_NaN()'
		else:
			return str(val)
	elif isinstance(val, int):
		if val == 9223372036854775807:
			return 'std::numeric_limits<int64_t>::max()'
		elif val == -9223372036854775808:
			return 'std::numeric_limits<int64_t>::min()'
		else:
			return str(val)
	elif isinstance(val, (TomlPPArray, TomlPPTable)):
		return str(val)
	elif isinstance(val, (date, time, datetime)):
		date_args = None
		if isinstance(val, (date, datetime)):
			date_args = rf'{val.year}, {val.month}, {val.day}'
		time_args = None
		if isinstance(val, (time, datetime)):
			time_args = rf'{val.hour}, {val.minute}'
			if val.second and val.microsecond:
				time_args = rf'{time_args}, {val.second}, {val.microsecond*1000}'
			elif val.second:
				time_args = rf'{time_args}, {val.second}'
			elif val.microsecond:
				time_args = rf'{time_args}, 0, {val.microsecond*1000}'
		if isinstance(val, datetime):
			offset_init = ''
			if val.tzinfo is not None:
				offset = val.tzinfo.utcoffset(val)
				mins = offset.total_seconds() / 60
				offset = (int(mins / 60), int(mins % 60))
				offset_init = rf', {{ {offset[0]}, {offset[1]} }}'
			return rf'toml::date_time{{ {{ {date_args} }}, {{ {time_args} }}{offset_init} }}'
		elif isinstance(val, time):
			return rf'toml::time{{ {time_args} }}'
		elif isinstance(val, date):
			return rf'toml::date{{ {date_args} }}'
	else:
		raise ValueError(str(type(val)))



class TomlPPArray:

	def __init__(self, init_data=None):
		self.values = init_data if init_data else list()

	def render(self, indent = '', indent_declaration = False):
		s = ''
		if indent_declaration:
			s += indent
		if len(self.values) == 0:
			s += 'toml::array{}'
		else:
			s += 'toml::array{'
			for val in self.values:
				s += '\n' + indent + '\t'
				if isinstance(val, TomlPPArray) and len(self.values) == 1:
					s += 'toml::inserter{'
				if isinstance(val, (TomlPPTable, TomlPPArray)) and len(val) > 0:
					s += val.render(indent + '\t')
				else:
					s += python_value_to_tomlpp(val)
				if isinstance(val, TomlPPArray) and len(self.values) == 1:
					s += '}'
				s += ','
			s += '\n' + indent + '}'
		return s

	def __str__(self):
		return self.render()

	def __len__(self):
		return len(self.values)



class TomlPPTable:

	def __init__(self, init_data=None):
		self.values = init_data if init_data else dict()

	def render(self, indent = '', indent_declaration = False):
		s = ''
		if indent_declaration:
			s += indent
		if len(self.values) == 0:
			s += 'toml::table{}'
		else:
			s += 'toml::table{'
			for key, val in self.values.items():
				s += '\n' + indent + '\t{ '
				if isinstance(val, (TomlPPTable, TomlPPArray)) and len(val) > 0:
					s += '\n' + indent + '\t\t{},'.format(python_value_to_tomlpp(str(key)))
					s += ' ' + val.render(indent + '\t\t')
					s += '\n' + indent + '\t'
				else:
					s += '{}, {} '.format(python_value_to_tomlpp(str(key)), python_value_to_tomlpp(val))
				s += '},'
			s += '\n' + indent + '}'
		return s

	def __str__(self):
		return self.render()

	def __len__(self):
		return len(self.values)



def json_to_python(val):

	if isinstance(val, dict):
		if len(val) == 2 and "type" in val and "value" in val:
			val_type = val["type"]
			if val_type == "integer":
				return int(val["value"])
			elif val_type == "float":
				return float(val["value"])
			elif val_type == "string":
				return str(val["value"])
			elif val_type == "bool":
				return True if val["value"].lower() == "true" else False
			elif val_type == "array":
				return json_to_python(val["value"])
			elif val_type in ("datetime", "date", "time", "datetime-local", "date-local", "time-local"):
				dt_val = dateutil.parser.parse(val["value"])
				if val_type in ("date", "date-local"):
					return dt_val.date()
				elif val_type in ("time", "time-local"):
					return dt_val.time()
				else:
					return dt_val
			else:
				raise ValueError(val_type)
		else:
			vals = dict()
			for k,v in val.items():
				vals[k] = json_to_python(v)
			return vals

	elif isinstance(val, list):
		vals = list()
		for v in val:
			vals.append(json_to_python(v))
		return vals

	else:
		raise ValueError(str(type(val)))



def python_to_tomlpp(node):
	if isinstance(node, dict):
		table = TomlPPTable()
		for key, val in node.items():
			table.values[key] = python_to_tomlpp(val)
		return table
	elif isinstance(node, (set, list, tuple)):
		array = TomlPPArray()
		for val in node:
			array.values.append(python_to_tomlpp(val))
		return array
	else:
		return node



class TomlTest:

	def __init__(self, file_path, name, is_valid_case):
		self.__name = name
		self.__identifier = sanitize(self.__name)
		self.__group = self.__identifier.strip('_').split('_')[0]

		# read file
		self.__raw = True
		self.__bytes = False
		with open(file_path, "rb") as f:
			self.__source = f.read()

		# if we find a utf-16 or utf-32 BOM, treat the file as bytes
		if len(self.__source) >= 4:
			prefix = self.__source[:4]
			if prefix == b'\x00\x00\xFE\xFF' or prefix == b'\xFF\xFE\x00\x00':
				self.__bytes = True
		if len(self.__source) >= 2:
			prefix = self.__source[:2]
			if prefix == b'\xFF\xFE' or prefix == b'\xFE\xFF':
				self.__bytes = True

		# if we find a utf-8 BOM, treat it as a string but don't use a raw string literal
		if not self.__bytes and len(self.__source) >= 3:
			prefix = self.__source[:3]
			if prefix == b'\xEF\xBB\xBF':
				self.__raw = False

		# if we're not treating it as bytes, decode the bytes into a utf-8 string
		if not self.__bytes:
			try:
				self.__source = str(self.__source, encoding='utf-8')

				# disable raw literals if the string contains some things that should be escaped
				for c in self.__source:
					if is_problematic_control_char(c):
						self.__raw = False
						break

				# disable raw literals if the string has trailing backslashes followed by whitespace on the same line
				# (GCC doesn't like it and generates some noisy warnings)
				if self.__raw and re.search(r'\\[ \t]+?\n', self.__source, re.S):
					self.__raw = False

			except UnicodeDecodeError:
				self.__bytes = True

		# strip off trailing newlines for non-byte strings (they're just noise)
		if not self.__bytes:
			while self.__source.endswith('\r\n'):
				self.__source = self.__source[:-2]
			self.__source = self.__source.rstrip('\n')

		# parse preprocessor conditions
		self.__conditions = []
		if is_valid_case:
			self.__expected = True
			path_base = str(Path(file_path.parent, file_path.stem))
			yaml_file = Path(path_base + r'.yaml')
			if yaml_file.exists():
				self.__expected = python_to_tomlpp(yaml.load(
					utils.read_all_text_from_file(yaml_file, logger=True),
					Loader=yaml.FullLoader
				))
			else:
				json_file = Path(path_base + r'.json')
				if json_file.exists():
					self.__expected = python_to_tomlpp(json_to_python(json.loads(
						utils.read_all_text_from_file(json_file, logger=True),
					)))

		else:
			self.__expected = False

	def name(self):
		return self.__name

	def identifier(self):
		return self.__identifier

	def group(self):
		return self.__group

	def add_condition(self, cond):
		self.__conditions.append(cond)
		return self

	def condition(self):
		if not self.__conditions or not self.__conditions[0]:
			return ''
		if len(self.__conditions) == 1:
			return rf'{self.__conditions[0]}'
		return rf'{" && ".join([rf"{c}" for c in self.__conditions])}'

	def expected(self):
		return self.__expected

	def __str__(self):
		return rf'static constexpr auto {self.__identifier} = {make_string_literal(self.__source, escape_all = self.__bytes, escape_any = not self.__raw)}sv;'



def load_tests(source_folder, is_valid_set, ignore_list = None):
	source_folder = source_folder.resolve()
	utils.assert_existing_directory(source_folder)
	files = utils.get_all_files(source_folder, all="*.toml", recursive=True)
	strip_source_folder_len = len(str(source_folder))
	files = [(f, str(f)[strip_source_folder_len+1:-5].replace('\\', '-').replace('/', '-').strip()) for f in files]
	if ignore_list:
		files_ = []
		for f,n in files:
			ignored = False
			for ignore in ignore_list:
				if ignore is None:
					continue
				if isinstance(ignore, str):
					if n == ignore:
						ignored = True
						break
				elif ignore.fullmatch(n) is not None: # regex
					ignored = True
					break
			if not ignored:
				files_.append((f, n))
		files = files_
	tests = []
	for f,n in files:
		tests.append(TomlTest(f, n, is_valid_set))
	return tests



def add_condition(tests, condition, names):
	for test in tests:
		matched = False
		for name in names:
			if isinstance(name, str):
				if test.name() == name:
					matched = True
					break
			elif name.fullmatch(test.name()) is not None: # regex
				matched = True
				break
		if matched:
			test.add_condition(condition)



def find_tests_dir(*relative_path):
	paths = (
		(Path.cwd(),),
		('.',),
		(utils.entry_script_dir(), '..', '..') # side-by-side with toml_++ repo folder
	)
	for p in paths:
		try:
			path = Path(*p, *relative_path).resolve()
			if path.exists() and path.is_dir():
				return path
		except:
			pass
	return None



def load_burnsushi_tests(tests):

	root_dir = find_tests_dir('toml-test', 'tests')
	if root_dir is None:
		raise Exception(r'could not find burntsushi/toml-test')

	tests['valid']['burntsushi'] = load_tests(Path(root_dir, 'valid'), True, (
		# broken by the json reader
		'key-alphanum',
	))
	add_condition(tests['valid']['burntsushi'], '!TOML_MSVC', (
		'inline-table-key-dotted', # causes MSVC to run out of heap space during compilation O_o
	))
	add_condition(tests['valid']['burntsushi'], 'TOML_LANG_UNRELEASED', (
		'string-escape-esc', # \e in strings
		'datetime-no-seconds', # omitting seconds from date-times
		'inline-table-newline',
		'key-unicode',
		'string-hex-escape'
	))

	tests['invalid']['burntsushi'] = load_tests(Path(root_dir, 'invalid'), False)
	add_condition(tests['invalid']['burntsushi'], '!TOML_LANG_UNRELEASED', (
		'datetime-no-secs',
		re.compile(r'inline-table-linebreak-.*'),
		'inline-table-trailing-comma',
		'key-special-character',
		'multi-line-inline-table',
		'string-basic-byte-escapes',
	))



def load_iarna_tests(tests):

	root_dir = find_tests_dir('toml-spec-tests')
	if root_dir is None:
		raise Exception(r'could not find iarni/toml-spec-tests')

	tests['invalid']['iarna'] = load_tests(Path(root_dir, 'errors'), False)
	add_condition(tests['invalid']['iarna'], '!TOML_LANG_UNRELEASED', (
		'inline-table-trailing-comma',
	))

	tests['valid']['iarna'] = load_tests(Path(root_dir, 'values'), True, (
		# these are stress-tests for 'large' datasets. I test these separately. Having them inline in C++ code is insane.
		'qa-array-inline-1000',
		'qa-array-inline-nested-1000',
		'qa-key-literal-40kb',
		'qa-key-string-40kb',
		'qa-scalar-literal-40kb',
		'qa-scalar-literal-multiline-40kb',
		'qa-scalar-string-40kb',
		'qa-scalar-string-multiline-40kb',
		'qa-table-inline-1000',
		'qa-table-inline-nested-1000',
		# bugged: https://github.com/iarna/toml-spec-tests/issues/3
		'spec-date-time-6',
		'spec-date-time-local-2',
		'spec-time-2',
	))



def write_test_file(name, all_tests):

	for test in all_tests:
		unicode = requires_unicode(str(test))
		if not unicode and not isinstance(test.expected(), bool):
			unicode = requires_unicode(test.expected().render())
		if unicode:
			test.add_condition(r'UNICODE_LITERALS_OK')

	tests_by_group = {}
	for test in all_tests:
		if test.group() not in tests_by_group:
			tests_by_group[test.group()] = {}
		cond = test.condition()
		if cond not in tests_by_group[test.group()]:
			tests_by_group[test.group()][cond] = []
		tests_by_group[test.group()][cond].append(test)
	all_tests = tests_by_group

	test_file_path = Path(utils.entry_script_dir(), '..', 'tests', rf'conformance_{sanitize(name.strip())}.cpp').resolve()
	with StringIO() as test_file_buffer:
		write = lambda txt,end='\n': print(txt, file=test_file_buffer, end=end)

		# preamble
		write(r'// This file is a part of toml++ and is subject to the the terms of the MIT license.')
		write(r'// Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>')
		write(r'// See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.')
		write(r'// SPDX-License-Identifier: MIT')
		write(r'//-----')
		write(r'// this file was generated by generate_conformance_tests.py - do not modify it directly')
		write(r'')
		write(r'#include "tests.hpp"')

		# test data
		write(r'')
		write('namespace')
		write('{', end='')
		for group, conditions in all_tests.items():
			for condition, tests in conditions.items():
				write('')
				if condition != '':
					write(f'#if {condition}');
					write('')
				for test in tests:
					write(f'\t{test}')
				if condition != '':
					write('')
					write(f'#endif // {condition}');
		write('}')

		# tests
		write('')
		write(f'TEST_CASE("conformance - {name}")')
		write('{', end='')
		for group, conditions in all_tests.items():
			for condition, tests in conditions.items():
				if condition != '':
					write('')
					write(f'#if {condition}');
				for test in tests:
					write('')
					write(f'\tSECTION("{test.name()}") {{')
					write('')
					expected = test.expected()
					if isinstance(expected, bool):
						if expected:
							write(f'\tparsing_should_succeed(FILE_LINE_ARGS, {test.identifier()}); // {test.name()}')
						else:
							write(f'\tparsing_should_fail(FILE_LINE_ARGS, {test.identifier()}); // {test.name()}')
					else:
						s = expected.render('\t\t')
						write(f'\tparsing_should_succeed(FILE_LINE_ARGS, {test.identifier()}, [](toml::table&& tbl) // {test.name()}')
						write('\t{')
						write(f'\t\tconst auto expected = {s};')
						write('\t\tREQUIRE(tbl == expected);')
						write('\t});')
					write('')
					write('\t}')
					write('')
				if condition != '':
					write('')
					write(f'#endif // {condition}');
		write('}')
		write('')

		test_file_content = test_file_buffer.getvalue()

		# clang-format
		print(f"Running clang-format for {test_file_path}")
		try:
			test_file_content = utils.apply_clang_format(test_file_content, cwd=test_file_path.parent)
		except Exception as ex:
			print(rf'Error running clang-format:', file=sys.stderr)
			utils.print_exception(ex)

		# write to disk
		print(rf'Writing {test_file_path}')
		with open(test_file_path, 'w', encoding='utf-8', newline='\n') as test_file:
			test_file.write(test_file_content)



def main():
	all_tests = { 'valid': dict(), 'invalid': dict() }
	load_burnsushi_tests(all_tests)
	load_iarna_tests(all_tests)
	for validity, sources in all_tests.items():
		for source, tests in sources.items():
			write_test_file('{}/{}'.format(source, validity), tests )



if __name__ == '__main__':
	utils.run(main, verbose=True)
