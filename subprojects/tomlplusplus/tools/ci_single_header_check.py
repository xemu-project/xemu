#!/usr/bin/env python3
# This file is a part of toml++ and is subject to the the terms of the MIT license.
# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
# SPDX-License-Identifier: MIT

import sys
import utils
from pathlib import Path


def main():
	hpp_path = Path(Path(__file__).resolve().parents[1], 'toml.hpp').resolve()
	hash1 = utils.sha1(utils.read_all_text_from_file(hpp_path, logger=True))
	print(rf'Hash 1: {hash1}')
	utils.run_python_script(r'generate_single_header.py')
	hash2 = utils.sha1(utils.read_all_text_from_file(hpp_path, logger=True))
	print(rf'Hash 2: {hash2}')
	if (hash1 != hash2):
		print(
			"toml.hpp wasn't up-to-date!\nRun generate_single_header.py before your commit to prevent this error.",
			file=sys.stderr
		)
		return 1
	print("toml.hpp was up-to-date")
	return 0



if __name__ == '__main__':
	utils.run(main, verbose=True)
