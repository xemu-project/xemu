#!/usr/bin/env bash

set -e # exit if a command fails
set -o pipefail # Will return the exit status of make if it fails

package_windows() { # Script to prepare the windows exe
    mkdir -p dist
    cp i386-softmmu/qemu-system-i386.exe dist/xqemu.exe
    cp i386-softmmu/qemu-system-i386w.exe dist/xqemuw.exe
    python3 ./get_deps.py dist/xqemu.exe dist
    strip dist/xqemu.exe
    strip dist/xqemuw.exe
}

postbuild=''
debug_opts='--enable-debug'
user_opts=''
build_cflags='-O0 -g'
job_count='4'

while [ ! -z ${1} ]
do
    case "${1}" in
    '-j'*)
        job_count="${1:2}"
        shift
        ;;
    '--release')
        build_cflags='-O3'
        debug_opts=''
        shift
        ;;
    *)
        user_opts="${user_opts} ${1}"
        shift
        ;;
    esac
done


case "$(uname -s)" in # adjust compilation option based on platform
    Linux)
        echo 'Compiling for Linux…'
        sys_cflags='-march=native -Wno-error=redundant-decls -Wno-error=unused-but-set-variable'
        sys_opts='--enable-kvm --disable-xen --disable-werror'
        ;;
    Darwin)
        echo 'Compiling for MacOS…'
        sys_cflags='-march=native'
        sys_opts='--disable-cocoa'
        ;;
    CYGWIN*|MINGW*|MSYS*)
        echo 'Compiling for Windows…'
        sys_cflags='-Wno-error'
        sys_opts='--python=python3 --disable-cocoa --disable-opengl'
        postbuild='package_windows' # set the above function to be called after build
        ;;
    *)
        echo "could not detect OS $(uname -s), aborting" >&2
        exit -1
        ;;
esac

# find absolute path (and resolve symlinks) to build out of tree
configure="$(dirname "$(readlink -f "${0}")")/configure"

set -x # Print commands from now on

"${configure}" \
    --extra-cflags="-DXBOX=1 ${build_cflags} ${sys_cflags} ${CFLAGS}" \
    ${debug_opts} \
    ${sys_opts} \
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
    --disable-stack-protector \
    ${user_opts}

time make -j"${job_count}" 2>&1 | tee build.log

${postbuild} # call post build functions
