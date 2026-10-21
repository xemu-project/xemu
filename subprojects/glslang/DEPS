# Copyright (C) 2020 The Khronos Group Inc.
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#    Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
#    Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
#
#    Neither the name of The Khronos Group Inc. nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# This file defines Glslang's build infra dependencies for GN builds.
# It assumes you have already have the source files for Glslang
# *and* its dependencies (as downloaded by ./update_glslang_sources.py)

# Usage:
#   1. Get Chromium depot_tools, and put it in your path.
#      See https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up
#
#   2. From the Glslang source tree:
#
#      # Get additional build tooling, using the info in the DEPS file.
#      gclient sync --gclientfile=standalone.gclient
#
#   3. One of the tools downloaded in the previous step is the binary for 'gn'.
#      Put it on your path.
#
#      # This is for Linux amd64 environments:
#      PATH=$(pwd)/build/linux64:$PATH
#
#   3. Create a build tree with Ninja build recipes.
#
#      gn gen out/Debug
#
#      # Or disable HLSL support:
#      gn gen out/Debug-no-hlsl --args="glslang_enable_hlsl=false"
#
#      # Or make a release build without HLSL support:
#      gn gen out/Release --args="glslang_enable_hlsl=false is_debug=false"
#
#   4. Use ninja to build the project
#
#      ninja -C out/Debug

use_relative_paths = True

gclient_gn_args_file = 'build/config/gclient_args.gni'

# Emit this vars for use in .gn and .gni files
gclient_gn_args = [
  # Needed by //build/toolchain/linux/BUILD.gn
  # which was needed by 'action' in ./BUILD.gn
  'build_with_chromium',
]

git_dependencies = 'SYNC'

vars = {
  # Repo sources
  'chromium_git': 'https://chromium.googlesource.com',

  # Building context
  'build_with_chromium': False,
  'glslang_standalone': True,

  # Commits
  'glslang_gn_version': 'git_revision:ec56d4d935a0e2ab9d52b88dd00c93ec51233055',
  'glslang_build_version': '2316930a88b15a036fa5f72e2b2c2ba08e2904a0',
}

deps = {
  # Get buildtools.
  # We need the 'gn' binary.
  'buildtools': {
    'url': '{chromium_git}/chromium/src/buildtools@11cc2bd83053cb790b7516aa3eb3f3935fb05a0e',
    'condition': 'glslang_standalone',
  },
  'buildtools/linux64': {
    'packages': [{
      'package': 'gn/gn/linux-${{arch}}',
      'version': Var('glslang_gn_version'),
    }],
    'dep_type': 'cipd',
    'condition': 'glslang_standalone and host_os == "linux"',
  },
  'buildtools/mac': {
    'packages': [{
      'package': 'gn/gn/mac-${{arch}}',
      'version': Var('glslang_gn_version'),
    }],
    'dep_type': 'cipd',
    'condition': 'glslang_standalone and host_os == "mac"',
  },
  'buildtools/win': {
    'packages': [{
      'package': 'gn/gn/windows-amd64',
      'version': Var('glslang_gn_version'),
    }],
    'dep_type': 'cipd',
    'condition': 'glslang_standalone and host_os == "win"',
  },

  # Get //build from Chromium.
  # It contains the generic //build/config/BUILDCONFIG.gn that sets up most rules
  # for C++ compilation.
  'build': {
  'url': '{chromium_git}/chromium/src/build@{glslang_build_version}',
    'condition': 'glslang_standalone',
  },
}

hooks = [
  # The Chromium BUILDCONFIG.gn has a false dependency on the sysroots.
  # Pull the compilers and system libraries for hermetic builds
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'glslang_standalone and checkout_linux and checkout_x86',
    'action': ['vpython3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'glslang_standalone and checkout_linux and checkout_x64',
    'action': ['vpython3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
]

recursedeps = [
  'buildtools',
]
