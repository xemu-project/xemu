#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)

# While Python 3 is the default, it's also possible to invoke
# this setup.py script with Python 2.

"""
setup.py file for SWIG libfdt
Copyright (C) 2017 Google, Inc.
Written by Simon Glass <sjg@chromium.org>
"""

from distutils.core import setup, Extension
import os
import re
import sys


VERSION_PATTERN = '^#define DTC_VERSION "DTC ([^"]*)"$'


def get_top_builddir():
    if '--top-builddir' in sys.argv:
        index = sys.argv.index('--top-builddir')
        sys.argv.pop(index)
        return sys.argv.pop(index)
    else:
        return os.getcwd()


srcdir = os.path.dirname(os.path.abspath(sys.argv[0]))
top_builddir = get_top_builddir()


def get_version():
    version_file = os.path.join(top_builddir, 'version_gen.h')
    f = open(version_file, 'rt')
    m = re.match(VERSION_PATTERN, f.readline())
    return m.group(1)


libfdt_module = Extension(
    '_libfdt',
    sources=[os.path.join(srcdir, 'libfdt.i')],
    include_dirs=[os.path.join(srcdir, '../libfdt')],
    libraries=['fdt'],
    library_dirs=[os.path.join(top_builddir, 'libfdt')],
    swig_opts=['-I' + os.path.join(srcdir, '../libfdt')],
)

setup(
    name='libfdt',
    version=get_version(),
    author='Simon Glass <sjg@chromium.org>',
    description='Python binding for libfdt',
    ext_modules=[libfdt_module],
    package_dir={'': srcdir},
    py_modules=['libfdt'],
)
