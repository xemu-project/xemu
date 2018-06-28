#!/bin/bash

set -e # exit if a command fails
set -o pipefail # Will return the exit status of make if it fails

CFLAGS_COMMON="-O0 -g -DXBOX=1" # Compilation flags for all platforms
POST_BUILD=""

package_windows() { # Script to prepare the windows exe
    mkdir -p dist
    cp i386-softmmu/qemu-system-i386.exe dist/xqemu.exe
    cp i386-softmmu/qemu-system-i386w.exe dist/xqemuw.exe
    python2 ./get_deps.py dist/xqemu.exe dist
    strip dist/xqemu.exe
    strip dist/xqemuw.exe
}

case "$(uname -s)" in # adjust compilation option based on platform
    Linux)
        echo "Compiling for Linux..."
        CFLAGS="-march=native -Wno-error=redundant-decls -Wno-error=unused-but-set-variable"
        CONFIGURE="--enable-kvm --disable-xen --disable-werror"
        ;;
    Darwin)
        echo "Compiling for MacOS..."
        CFLAGS="-march=native"
        CONFIGURE="--disable-cocoa"
        ;;
    CYGWIN*|MINGW*|MSYS*)
        echo "Compiling for Windows..."
        CFLAGS="-Wno-error"
        CONFIGURE="--python=python2 --disable-cocoa --disable-opengl"
        POST_BUILD="package_windows" # set the above function to be called after build
        ;;
    *)
        echo "Could not detect OS $(uname -s), aborting."
        exit -1
        ;;
esac

set -x # Print commands from now on

./configure \
	--enable-debug \
	--extra-cflags="$CFLAGS_COMMON $CFLAGS" \
	$CONFIGURE \
    --target-list=i386-softmmu \
	--enable-sdl \
	--with-sdlabi=2.0 \
	--disable-curl \
	--disable-vnc \
	--disable-docs \
	--disable-tools \
	--disable-guest-agent \
	--disable-tpm \
	--disable-live-block-migration \
	--disable-replication \
	--disable-capstone \
	--disable-fdt \
	--disable-libiscsi \
	--disable-spice \
	--disable-user \

time make -j4 2>&1 | tee build.log

$POST_BUILD # call post build functions

