#!/usr/bin/env python3
# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

import sys
import re
from argparse import ArgumentParser
from pathlib import Path



def read_text_file(path):
	print(rf'Reading {path}')
	with open(path, r'r', encoding=r'utf-8') as f:
		return f.read()



def write_text_file(path, text):
	print(rf'Writing {path}')
	with open(path, r'w', encoding=r'utf-8', newline='\n') as f:
		f.write(text)



if __name__ == '__main__':

	args = ArgumentParser(r'version.py', description=r'Sets the project version in all the necessary places.')
	args.add_argument(r'version', type=str)
	args = args.parse_args()

	version = re.fullmatch(r'\s*[vV]?\s*([0-9]+)\s*[.,;]+\s*([0-9]+)\s*[.,;]+\s*([0-9]+)\s*', args.version)
	if not version:
		print(rf"Couldn't parse version triplet from '{args.version}'", file=sys.stderr)
		sys.exit(1)
	version = (int(version[1]), int(version[2]), int(version[3]))
	version_str = rf'{version[0]}.{version[1]}.{version[2]}'
	print(rf'version: {version_str}')

	root = Path(__file__).parent.parent.resolve()

	path = root / r'meson.build'
	text = read_text_file(path)
	text = re.sub(r'''(\s|^)version\s*:\s*['"].*?['"]''', rf"\1version: '{version_str}'", text, count=1)
	write_text_file(path, text)

	path = root / r'CMakeLists.txt'
	text = read_text_file(path)
	text = re.sub(r'''(\s|^)VERSION\s+[0-9](?:[.][0-9]){2}''', rf"\1VERSION {version_str}", text, count=1, flags=re.I)
	write_text_file(path, text)

	for path in (root / r'include/toml++/impl/version.hpp', root / r'toml.hpp'):
		text = read_text_file(path)
		text = re.sub(r'''(\s*#\s*define\s+TOML_LIB_MAJOR)\s+[0-9]+''', rf"\1 {version[0]}", text)
		text = re.sub(r'''(\s*#\s*define\s+TOML_LIB_MINOR)\s+[0-9]+''', rf"\1 {version[1]}", text)
		text = re.sub(r'''(\s*#\s*define\s+TOML_LIB_PATCH)\s+[0-9]+''', rf"\1 {version[2]}", text)
		write_text_file(path, text)

	noop_sub = r'#$%^nbsp^%$#'
	for file in (r'README.md', r'docs/pages/main_page.md'):
		path = root / file
		text = read_text_file(path)
		text = re.sub(r'''(toml(?:plusplus|\+\+|pp)\s*[/:^]\s*)[0-9](?:[.][0-9]){2}''', rf"\1{noop_sub}{version_str}", text, flags=re.I)
		text = re.sub(r'''(GIT_TAG\s+)(?:v\s*)?[0-9](?:[.][0-9]){2}''', rf"\1v{version_str}", text, flags=re.I)
		text = text.replace(noop_sub, '')
		write_text_file(path, text)
