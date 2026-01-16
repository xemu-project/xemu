#!/usr/bin/env python3

import argparse
import sys
from io import StringIO
from typing import Union, TextIO
import os.path

def encode_bytes(data: Union[bytes, bytearray], outfile: TextIO):
	"""
	Encode data as a string of C-style escape sequences
	"""
	for i,v in enumerate(data):
		if i % 16 == 0:
			outfile.write('\t"')
		outfile.write('\\x%02x' % v)
		if i % 16 == 15:
			outfile.write('"\n')
	if i % 16 != 15:
		outfile.write('"')

def encode_bytes_str(data: Union[bytes, bytearray]):
	outfile = StringIO()
	encode_bytes(data, outfile)
	outfile.seek(0)
	return outfile.read()

def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('path')
	ap.add_argument('cout')
	ap.add_argument('hout')
	args = ap.parse_args()
	data = bytearray(open(args.path, 'rb').read())
	fname, _ = os.path.splitext(os.path.basename(args.path))
	ident = ''.join([c if c.isalnum() else '_' for c in fname])

	with open(args.hout, 'w') as f:
		f.write(
f'''\
#ifndef DATA_{ident.upper()}_H
#define DATA_{ident.upper()}_H

extern const unsigned int {ident}_size;
extern const unsigned char {ident}_data[];

#endif
'''
			)

	with open(args.cout, 'w') as f:
		f.write(
f'''\
const unsigned int {ident}_size = {len(data)};
const unsigned char {ident}_data[] =
{encode_bytes_str(data)};
'''
			)

if __name__ == '__main__':
	main()
