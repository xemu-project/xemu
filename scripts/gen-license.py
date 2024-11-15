#!/usr/bin/env python3
"""
Produces an outbound LICENSE.txt file for xemu. Also attempts to track versions
used to build the binary.
"""

# FIXME: Ideally these licenses would pulled out of whatever source tarball they
# originate. For now the licenses from the project's website/repo and cached
# under licenses/

import subprocess
import os.path
import sys

gplv2 = 'gplv2'
mit = 'mit'
bsd = 'bsd'
bsd_2clause = 'bsd-2clause'
bsd_3clause = 'bsd-3clause'
zlib = 'zlib'
lgplv2_1 = 'lgplv2_1'
apache2 = 'apache2'
unlicense = 'unlicense'
multi = 'multi'


windows = 'windows'
macos = 'darwin'
linux = 'linux'
all_platforms = { windows, macos, linux }
current_platform = linux

versions = {}

def banner(s):
	space = 1
	width = 80
	mid_len = 2*space+len(s)
	left_dashes = (width-mid_len)//2
	right_dashes = width-mid_len-left_dashes
	return '-'*left_dashes + ' ' + s + ' ' + '-'*right_dashes


class Lib:
	def __init__(self, name, url,
		               license, license_url, license_lines=None,
		               submodule=None,
		               version=None,
		               ships_static=set(), ships_dynamic=set(),
		               platform=all_platforms, pkgconfig=None,
		               pkg_win=None, pkg_mac=None, pkg_ubuntu=None):
		self.name = name
		self.url = url
		self.license = license
		self.license_url = license_url
		self.license_lines = license_lines
		self.submodule = submodule
		self._version = version
		self.ships_static = ships_static
		self.ships_dynamic = ships_dynamic
		self.platform = platform
		self.pkgconfig = pkgconfig
		self.pkg_win = pkg_win
		self.pkg_mac = pkg_mac
		self.pkg_ubuntu = pkg_ubuntu

	@property
	def version(self):
		if self._version:
			return self._version

		if self.submodule:
			self._version = self.submodule.head
			return self._version

		if self.pkgconfig:
			self._version = self.pkgconfig.modversion
			return self._version

		if current_platform == windows and self.pkg_win:
			self._version = subprocess.run(r"grep -e '_VERSION\s*:=' /opt/mxe/src/" + self.pkg_win + ".mk | cut -d'=' -f2",
	                 capture_output=True, shell=True,
	                 check=True).stdout.decode('utf-8').strip()
			return self._version
		elif current_platform == macos and self.pkg_mac:
			self._version = versions[self.pkg_mac]
			return self._version
		elif current_platform == linux and self.pkg_ubuntu:
			self._version = subprocess.run(r"dpkg -s " + self.pkg_ubuntu + " | grep Version | cut -d: -f2",
	                 capture_output=True, shell=True,
	                 check=True).stdout.decode('utf-8').strip()
			return self._version

		assert False, 'Failed to get version info for ' + self.name

	@property
	def license_text(self):
		fname = os.path.join('licenses', self.name + '.license.txt')
		if os.path.exists(fname):
			with open(fname, 'r', encoding='utf-8') as f:
				return f.read()
		import requests
		d = requests.get(self.license_url).content.decode('utf-8')
		if self.license_lines:
			start, end = self.license_lines
			d = '\n'.join(d.splitlines()[start-1:end+1])
		with open(fname, 'w') as f:
			f.write(d)
		return d

	@property
	def is_active(self):
		return current_platform in self.platform

	@property
	def does_ship_static(self):
		return current_platform in self.ships_static

	@property
	def does_ship_dynamic(self):
		return current_platform in self.ships_dynamic

	@property
	def does_ship(self):
		return self.is_active and (self.does_ship_static or self.does_ship_dynamic)


class PkgConfig:
	def __init__(self, name):
		self.name = name

	@property
	def modversion(self):
		pkg_config = {
			linux: 'pkg-config',
			windows: 'x86_64-w64-mingw32.static-pkg-config',
			macos: 'pkg-config',
		}[current_platform]
		ver = subprocess.run([pkg_config, '--modversion', self.name],
			                 capture_output=True, check=True)
		return ver.stdout.decode('utf-8').strip()

class Submodule:
	def __init__(self, path):
		self.path = path

	@property
	def head(self):
		try:
			return subprocess.run(['git', 'rev-parse', 'HEAD'],
				                 cwd=self.path, capture_output=True,
				                 check=True).stdout.decode('utf-8').strip()
		except subprocess.CalledProcessError:
			pass

		commit_file_path = os.path.join(self.path, 'HEAD')
		if os.path.exists(commit_file_path):
			return open(commit_file_path).read().strip()

		raise Exception('Failed to determine submodule revision')
		return ''

LIBS = [

Lib('qemu', 'https://www.qemu.org/',
	gplv2, 'https://raw.githubusercontent.com/mborgerson/xemu/master/LICENSE',
	version='6.0.0'
	),

#
# Built from source with xemu
#

Lib('slirp', 'https://gitlab.freedesktop.org/slirp',
	bsd_3clause, 'https://gitlab.freedesktop.org/slirp/libslirp/-/raw/master/COPYRIGHT', license_lines=(16,39),
	ships_static=all_platforms,
	pkgconfig=PkgConfig('slirp'), pkg_win='libslirp', pkg_mac='libslirp', pkg_ubuntu='libslirp-dev'
	),

Lib('imgui', 'https://github.com/ocornut/imgui',
	mit, 'https://raw.githubusercontent.com/ocornut/imgui/master/LICENSE.txt',
	ships_static=all_platforms,
	submodule=Submodule('ui/thirdparty/imgui')
	),

Lib('implot', 'https://github.com/epezent/implot',
	mit, 'https://raw.githubusercontent.com/epezent/implot/master/LICENSE',
	ships_static=all_platforms,
	submodule=Submodule('ui/thirdparty/implot')
	),

Lib('httplib', 'https://github.com/yhirose/cpp-httplib',
	mit, 'https://raw.githubusercontent.com/yhirose/cpp-httplib/master/LICENSE',
	ships_static=all_platforms,
	submodule=Submodule('ui/thirdparty/httplib')
	),

Lib('noc', 'https://github.com/guillaumechereau/noc/blob/master/noc_file_dialog.h',
	mit, 'https://raw.githubusercontent.com/mborgerson/xemu/master/ui/noc_file_dialog.h', license_lines=(1,22),
	ships_static=all_platforms,
	version='78b2e7b22506429dd1755ffff197c7da11507fd9'
	),

Lib('stb_image', 'https://github.com/nothings/stb',
	mit, 'https://raw.githubusercontent.com/nothings/stb/master/LICENSE', license_lines=(4,19),
	ships_static=all_platforms,
	version='2.25'
	),

Lib('tomlplusplus', 'https://github.com/marzer/tomlplusplus',
	mit, 'https://raw.githubusercontent.com/marzer/tomlplusplus/master/LICENSE',
	ships_static=all_platforms,
	submodule=Submodule('tomlplusplus')
	),

Lib('xxHash', 'https://github.com/Cyan4973/xxHash.git',
	bsd, 'https://raw.githubusercontent.com/Cyan4973/xxHash/dev/LICENSE', license_lines=(1,26),
	ships_static=all_platforms,
	submodule=Submodule('util/xxHash')
	),

Lib('fpng', 'https://github.com/richgel999/fpng',
	unlicense, 'https://github.com/richgel999/fpng/blob/main/README.md',
	ships_static=all_platforms,
	version='6926f5a0a78f22d42b074a0ab8032e07736babd4'
	),

Lib('nv2a_vsh_cpu', 'https://github.com/abaire/nv2a_vsh_cpu',
	unlicense, 'https://raw.githubusercontent.com/abaire/nv2a_vsh_cpu/main/LICENSE',
	ships_static=all_platforms,
	submodule=Submodule('hw/xbox/nv2a/thirdparty/nv2a_vsh_cpu')
	),

#
# Data files included with xemu
#

Lib('roboto', 'https://github.com/googlefonts/roboto',
	apache2, 'https://raw.githubusercontent.com/googlefonts/roboto/main/LICENSE',
	ships_static=all_platforms,
	version='2.138'
	),

Lib('fontawesome', 'https://fontawesome.com',
	multi, '',
	ships_static=all_platforms,
	version='6.1.1'
	),

#
# Libraries either linked statically, dynamically linked & shipped, or dynamically linked with system-installed libraries only
#

Lib('sdl2', 'https://www.libsdl.org/',
	zlib, 'https://raw.githubusercontent.com/libsdl-org/SDL/main/LICENSE.txt',
	ships_static={windows}, ships_dynamic={macos},
	pkgconfig=PkgConfig('sdl2'), pkg_win='sdl2', pkg_mac='sdl2', pkg_ubuntu='libsdl2-dev'
	),

Lib('glib-2.0', 'https://gitlab.gnome.org/GNOME/glib',
	lgplv2_1, 'https://gitlab.gnome.org/GNOME/glib/-/raw/master/COPYING',
	ships_static={windows}, ships_dynamic={macos},
	pkgconfig=PkgConfig('glib-2.0'), pkg_win='glib', pkg_mac='glib', pkg_ubuntu='libglib2.0-dev'
	),

# glib dep
Lib('pcre', 'http://pcre.org/',
	bsd, 'http://www.pcre.org/original/license.txt',
	pkgconfig=PkgConfig('libpcre'), pkg_ubuntu='libpcre3-dev'
	),
Lib('pcre2', 'http://pcre.org/',
	bsd, 'https://www.pcre.org/licence.txt',
	ships_static={windows},	ships_dynamic={macos},
	pkgconfig=PkgConfig('libpcre2-8'), pkg_win='pcre2', pkg_mac='pcre2', pkg_ubuntu='libpcre2-dev'
	),

# glib dep
Lib('gettext', 'https://www.gnu.org/software/gettext/',
	lgplv2_1, 'https://git.savannah.gnu.org/gitweb/?p=gettext.git;a=blob_plain;f=gettext-runtime/intl/COPYING.LIB;hb=HEAD',
	ships_static={windows}, ships_dynamic={macos},
	pkg_win='gettext', pkg_mac='gettext-runtime',
	),

# glib dep
Lib('iconv', 'https://www.gnu.org/software/libiconv/',
	lgplv2_1, 'https://git.savannah.gnu.org/gitweb/?p=libiconv.git;a=blob_plain;f=COPYING.LIB;hb=HEAD',
	ships_static={windows}, ships_dynamic={macos},
	pkg_win='libiconv', pkg_mac='libiconv'
	),

Lib('libepoxy', 'https://github.com/anholt/libepoxy',
	mit, 'https://raw.githubusercontent.com/anholt/libepoxy/master/COPYING',
	ships_static={windows}, ships_dynamic={macos},
	pkgconfig=PkgConfig('epoxy'), pkg_win='libepoxy', pkg_mac='libepoxy', pkg_ubuntu='libepoxy-dev'
	),

Lib('pixman', 'http://www.pixman.org/',
	mit, 'https://cgit.freedesktop.org/pixman/plain/COPYING',
	ships_static={windows}, ships_dynamic={macos},
	pkgconfig=PkgConfig('pixman-1'),
	pkg_win='pixman',
	pkg_mac='pixman',
	pkg_ubuntu='libpixman-1-dev'
	),

Lib('libsamplerate', 'https://github.com/libsndfile/libsamplerate',
	bsd_2clause, 'https://raw.githubusercontent.com/libsndfile/libsamplerate/master/COPYING',
	ships_static={windows}, ships_dynamic={macos},
	pkgconfig=PkgConfig('samplerate'),
	pkg_win='libsamplerate',
	pkg_mac='libsamplerate',
	),

Lib('openssl', 'https://www.openssl.org/',
	apache2, 'https://raw.githubusercontent.com/openssl/openssl/master/LICENSE.txt',
	ships_static={windows}, ships_dynamic={macos},
	pkgconfig=PkgConfig('openssl'),
	pkg_win='openssl',
	pkg_mac='openssl',
	pkg_ubuntu='libssl-dev'
	),

# openssl dep
Lib('zlib', 'https://zlib.net/',
	zlib, 'https://raw.githubusercontent.com/madler/zlib/master/README', license_lines=(87,106),
	ships_static={windows}, ships_dynamic={macos},
	pkgconfig=PkgConfig('zlib'), pkg_win='zlib', pkg_mac='zlib', pkg_ubuntu='zlib1g-dev'
	),

Lib('libmingw32', 'http://mingw-w64.org/',
	multi, 'https://sourceforge.net/p/mingw-w64/mingw-w64/ci/master/tree/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt?format=raw',
	ships_static={windows}, platform={windows},
	pkg_win='mingw-w64',
	),

Lib('gtk', 'https://www.gtk.org/',
	lgplv2_1, 'https://gitlab.gnome.org/GNOME/gtk/-/raw/master/COPYING',
	platform={linux},
	pkgconfig=PkgConfig('gtk+-3.0'), pkg_ubuntu='libgtk-3-dev'
	),

Lib('miniz', 'https://github.com/richgel999/miniz',
	lgplv2_1, 'https://raw.githubusercontent.com/richgel999/miniz/master/LICENSE',
	ships_static={windows},	platform={windows},
	version='2.1.0'
	),
]

def gen_license():
	print(f'''\
xemu is free and open source software. This binary of xemu has been made
available to you under the terms of the GNU General Public License, version 2.

The source code used to build this version of xemu is available at:

                                https://xemu.app

-------------------------------------------------------------------------------

{open('COPYING','r').read()}

===============================================================================
===============================================================================

xemu depends on several great packages/libraries which are also free and open
source. The respective licenses of these packages are provided below.
''')
	for lib in LIBS:
		if lib.does_ship:
			print(banner(lib.name))
			print('')
			print('Project URL:      ' + lib.url)
			print('Version Included: ' + lib.version)
			print('Project License:')

			print('')
			print('\n'.join([(' | ' + l) for l in lib.license_text.splitlines()]))
			print('')

def main():
	import argparse
	ap = argparse.ArgumentParser()
	ap.add_argument('--platform', default='')
	ap.add_argument('--version-file', default='')
	args = ap.parse_args()

	if args.platform == '':
		args.platform = sys.platform.lower()
	global current_platform
	current_platform = args.platform
	if args.version_file != '':
		with open(args.version_file, 'r') as f:
			global versions
			versions = {pkg: ver
				for pkg, ver in map(lambda l: l.strip().split('='), f.readlines())}
	gen_license()

if __name__ == '__main__':
	main()
