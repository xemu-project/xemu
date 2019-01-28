#!/usr/bin/env python
"""
This script is used to collect all available DLLs
"""
from __future__ import print_function
import os
import os.path
import platform
import subprocess
import shutil
import sys
import argparse

def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('prog')
	ap.add_argument('dest')
	args = ap.parse_args()

	if not os.path.exists(args.dest):
		os.mkdir(args.dest)
	elif not os.path.isdir(args.dest):
		print('File exists with destination name')
		sys.exit(1)

	try:
		subprocess.check_output(['cygpath', '--help'])
	except OSError as err:
		print("Couldn't execute cygpath (Reason: '%s')." % err)
		print("Make sure you're using a recent version of MSYS2 and that it works correctly.")
		sys.exit(1)

	sout = subprocess.check_output(['ldd', args.prog]).decode('utf-8')
	for line in sout.splitlines():
		line = line.strip().split()
		dll_name, dll_path, dll_load_addr = line[0], line[2], line[3]
		if dll_name.startswith('???'):
			print('Unknown DLL?')
			continue
		# ldd on msys gives Unix-style paths, but mingw Python wants them Windows-style
		# Use cygpath to convert the paths, because both mingw and msys Python can handle them
		dll_path = subprocess.check_output(['cygpath', '-w', dll_path]).decode('utf-8').strip()
		if dll_path.lower().startswith('c:\\windows'):
			print('Skipping system DLL %s' % dll_path)
			continue

		dest_path = os.path.join(args.dest, dll_name)
		if os.path.normcase(os.path.realpath(dll_path)) == os.path.normcase(os.path.realpath(dest_path)):
			# If the DLL is already in the same folder as the executable,
			# ldd will return the path to that DLL and copyfile will raise an exception
			# because we try to copy a file over itself
			print('DLL %s is already next to executable. Skipping copy.' % dll_name)
		else:
			print('Copying %s...' % dll_path)
			shutil.copyfile(dll_path, dest_path)

if __name__ == '__main__':
	main()
