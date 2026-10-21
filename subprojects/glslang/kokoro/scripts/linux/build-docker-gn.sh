#!/bin/bash

# Copyright 2026 Google LLC.
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
#    Neither the name of Google Inc. nor the names of its
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

set -e # Fail on any error.

# 'true' or 'false'
GLSLANG_ENABLE_HLSL=${GLSLANG_ENABLE_HLSL-true}

. /bin/using.sh # Declare the bash `using` function for configuring toolchains.

using ninja-1.10.0
using python-3.12.4

# Don't set the compiler here
# The GN build uses the g++ compiler that it downloads from the sysroot.
# See DEPS for details.

set -x # Display commands being run.

# Disable git's "detected dubious ownership" error - kokoro checks out the repo
# with a different user, and we don't care about this warning.
git config --global --add safe.directory '*'

echo "Fetching external projects..."
./update_glslang_sources.py

echo "Fetching depot_tools..."
rm -rf /tmp/depot_tools
mkdir -p /tmp/depot_tools
git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git /tmp/depot_tools
export PATH="/tmp/depot_tools:$PATH"


echo "Syncing client..."

# Silence gn related babble
export GCLIENT_SUPPRESS_GIT_VERSION_WARNING=1
# For the 'root' user, silence gclient metrics collection.
if [[ -z "$USER" ]]; then
  mkdir -p $HOME/.config/depot_tools
  cat  >$HOME/.config/depot_tools/metrics.cfg <<'EOF'
  {"is-googler": false, "countdown": 0, "opt-in": false, "version": 1}
EOF
fi

# Erase the GN args from any previous run of gclient.
# This is important for local testing.
rm -f build/config/gclient_args.gni

# Sync dependencies and generate default GN args from the DEPS file.
gclient sync -D --gclientfile=standalone.gclient

echo "Generate ninja build plan..."

# Ensure the gn binary is on the path
export PATH=$(pwd)/buildtools/linux64:$PATH
# which -a gn  # There should be two

BUILD_DIR=out/Default-hlsl-${GLSLANG_ENABLE_HLSL}

rm -rf "$BUILD_DIR"
gn gen "$BUILD_DIR" --args="glslang_enable_hlsl=${GLSLANG_ENABLE_HLSL}"

echo "Building..."
ninja -v -C "$BUILD_DIR"
