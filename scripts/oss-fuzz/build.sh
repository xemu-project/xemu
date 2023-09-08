#!/bin/bash -e
#
# OSS-Fuzz build script. See:
# https://google.github.io/oss-fuzz/getting-started/new-project-guide/#buildsh
#
# The file is consumed by:
# https://github.com/google/oss-fuzz/blob/master/projects/qemu/Dockerfiles
#
# This code is licensed under the GPL version 2 or later.  See
# the COPYING file in the top-level directory.
#

# build project
# e.g.
# ./autogen.sh
# ./configure
# make -j$(nproc) all

# build fuzzers
# e.g.
# $CXX $CXXFLAGS -std=c++11 -Iinclude \
#     /path/to/name_of_fuzzer.cc -o $OUT/name_of_fuzzer \
#     -fsanitize=fuzzer /path/to/library.a

fatal () {
    echo "Error : ${*}, exiting."
    exit 1
}

OSS_FUZZ_BUILD_DIR="./build-oss-fuzz/"

# There seems to be a bug in clang-11 (used for builds on oss-fuzz) :
#   accel/tcg/cputlb.o: In function `load_memop':
#   accel/tcg/cputlb.c:1505: undefined reference to `qemu_build_not_reached'
#
# When building with optimization, the compiler is expected to prove that the
# statement cannot be reached, and remove it. For some reason clang-11 doesn't
# remove it, resulting in an unresolved reference to qemu_build_not_reached
# Undefine the __OPTIMIZE__ macro which compiler.h relies on to choose whether
# to " #define qemu_build_not_reached()  g_assert_not_reached() "
EXTRA_CFLAGS="$CFLAGS -U __OPTIMIZE__"

if ! { [ -e "./COPYING" ] &&
   [ -e "./MAINTAINERS" ] &&
   [ -e "./Makefile" ] &&
   [ -e "./docs" ] &&
   [ -e "./VERSION" ] &&
   [ -e "./linux-user" ] &&
   [ -e "./softmmu" ];} ; then
    fatal "Please run the script from the top of the QEMU tree"
fi

mkdir -p $OSS_FUZZ_BUILD_DIR || fatal "mkdir $OSS_FUZZ_BUILD_DIR failed"
cd $OSS_FUZZ_BUILD_DIR || fatal "cd $OSS_FUZZ_BUILD_DIR failed"


if [ -z ${OUT+x} ]; then
    DEST_DIR=$(realpath "./DEST_DIR")
else
    DEST_DIR=$OUT
fi

mkdir -p "$DEST_DIR/lib/"  # Copy the shared libraries here

# Build once to get the list of dynamic lib paths, and copy them over
../configure --disable-werror --cc="$CC" --cxx="$CXX" --enable-fuzzing \
    --prefix="/opt/qemu-oss-fuzz" \
    --extra-cflags="$EXTRA_CFLAGS" --target-list="i386-softmmu"

if ! make "-j$(nproc)" qemu-fuzz-i386; then
    fatal "Build failed. Please specify a compiler with fuzzing support"\
          "using the \$CC and \$CXX environment variables"\
          "\nFor example: CC=clang CXX=clang++ $0"
fi

if [ "$GITLAB_CI" != "true" ]; then
    for i in $(ldd ./qemu-fuzz-i386 | cut -f3 -d' '); do
        cp "$i" "$DEST_DIR/lib/"
    done
    rm qemu-fuzz-i386

    # Build a second time to build the final binary with correct rpath
    ../configure --disable-werror --cc="$CC" --cxx="$CXX" --enable-fuzzing \
        --prefix="/opt/qemu-oss-fuzz" \
        --extra-cflags="$EXTRA_CFLAGS" --extra-ldflags="-Wl,-rpath,\$ORIGIN/lib" \
        --target-list="i386-softmmu"
    make "-j$(nproc)" qemu-fuzz-i386 V=1
fi

# Place data files in the preinstall tree
make install DESTDIR=$DEST_DIR/qemu-bundle
rm -rf $DEST_DIR/qemu-bundle/opt/qemu-oss-fuzz/bin
rm -rf $DEST_DIR/qemu-bundle/opt/qemu-oss-fuzz/libexec

targets=$(./qemu-fuzz-i386 | grep generic-fuzz | awk '$1 ~ /\*/  {print $2}')
base_copy="$DEST_DIR/qemu-fuzz-i386-target-$(echo "$targets" | head -n 1)"

cp "./qemu-fuzz-i386" "$base_copy"

# Run the fuzzer with no arguments, to print the help-string and get the list
# of available fuzz-targets. Copy over the qemu-fuzz-i386, naming it according
# to each available fuzz target (See 05509c8e6d fuzz: select fuzz target using
# executable name)
for target in $(echo "$targets" | tail -n +2);
do
    # Ignore the generic-fuzz target, as it requires some environment variables
    # to be configured. We have some generic-fuzz-{pc-q35, floppy, ...} targets
    # that are thin wrappers around this target that set the required
    # environment variables according to predefined configs.
    if [[ $target == "generic-fuzz-"* ]]; then
        ln  $base_copy \
            "$DEST_DIR/qemu-fuzz-i386-target-$target"
    fi
done

echo "Done. The fuzzers are located in $DEST_DIR"
exit 0
