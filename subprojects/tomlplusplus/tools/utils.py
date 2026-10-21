#!/usr/bin/env python3
# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

import sys
import subprocess
from pathlib import Path
from misk import *


def repeat_pattern(pattern, count):
	if len(pattern) == 1:
		return pattern * count
	text = ''
	for i in range(0, count):
		text = text + pattern[i % len(pattern)]
	return text



def make_divider(text = None, text_col = 40, pattern = '-', line_length = 120):
	if (text is None):
		return "//" + repeat_pattern(pattern, line_length-2)
	else:
		text = "//{}  {}  ".format(repeat_pattern(pattern, text_col - 2), text);
		if (len(text) < line_length):
			return text + repeat_pattern(pattern, line_length - len(text))
		else:
			return text



def apply_clang_format(text, cwd=None):
	return subprocess.run(
		'clang-format --style=file'.split(),
		check=True,
		capture_output=True,
		cwd=str(Path.cwd() if cwd is None else cwd),
		encoding='utf-8',
		input=text
	).stdout


def run(main_func, verbose=False):
	try:
		result = main_func()
		if result is None:
			sys.exit(0)
		else:
			sys.exit(int(result))
	except Exception as err:
		print_exception(err, include_type=verbose, include_traceback=verbose, skip_frames=1)
		sys.exit(-1)
