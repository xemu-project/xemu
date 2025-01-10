#
# Environment to cross-compile xemu for Windows
#

FROM ubuntu:24.04

ENV MXE_PATH=/opt/mxe
ENV MXE_REPO=https://github.com/mxe/mxe.git
ENV MXE_VERSION=ab676e8ce5ba921daaa80a123ff2e415aac4524a

ENV MXE_LLVM_MINGW_REPO=https://github.com/libvips/build-win64-mxe
ENV MXE_LLVM_MINGW_VERSION=21e02f87c282fcfe17c8376217b0a4f44f14d01b
ENV MXE_LLVM_MINGW_PATH=/opt/build-win64-mxe

ARG PLUGIN_DIRS="${MXE_LLVM_MINGW_PATH} ${MXE_LLVM_MINGW_PATH}/build/plugins/llvm-mingw"
ARG TARGETS="x86_64-w64-mingw32.static aarch64-w64-mingw32.static"
ARG JOBS=6

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive \
    apt-get -qy install \
        autoconf \
        automake \
        autopoint \
        bash \
        bison \
        bzip2 \
        flex \
        g++ \
        g++-multilib \
        gettext \
        git \
        gperf \
        intltool \
        libc6-dev-i386 \
        libgdk-pixbuf2.0-dev \
        libgl-dev \
        libltdl-dev \
        libssl-dev \
        libtool-bin \
        libxml-parser-perl \
        lsb-release \
        lzip \
        make \
        ninja-build \
        openssl \
        p7zip-full \
        patch \
        perl \
        python-is-python3 \
        python3 \
        python3-mako \
        python3-pip \
        python3-pkg-resources \
        python3-yaml \
        ruby \
        sed \
        software-properties-common \
        unzip \
        wget \
        xz-utils

RUN git clone ${MXE_LLVM_MINGW_REPO} ${MXE_LLVM_MINGW_PATH} \
 && cd ${MXE_LLVM_MINGW_PATH} \
 && git checkout ${MXE_LLVM_MINGW_VERSION} \
 \
 && git clone ${MXE_REPO} ${MXE_PATH} \
 && cd ${MXE_PATH} \
 && git checkout ${MXE_VERSION} \
 && git apply ${MXE_LLVM_MINGW_PATH}/build/patches/mxe-fixes.patch

RUN make \
    MXE_TARGETS="${TARGETS}" \
    MXE_PLUGIN_DIRS="${PLUGIN_DIRS}" \
    JOBS=${JOBS} \
    -C ${MXE_PATH} \
        cc

RUN rm ${MXE_PATH}/src/sdl2*.patch
COPY vulkan-headers.mk \
     spirv-headers.mk \
     spirv-tools.mk \
     glslang.mk \
     glib.mk \
     sdl2.mk \
     libsamplerate.mk \
     libressl.mk \
     ${MXE_PATH}/src/

RUN make \
    MXE_TARGETS="${TARGETS}" \
    MXE_PLUGIN_DIRS="${PLUGIN_DIRS}" \
    JOBS=${JOBS} \
    CFLAGS=-O2 \
    -C ${MXE_PATH} \
        glib \
        libepoxy \
        pixman \
        libsamplerate \
        libressl \
        cmake \
        libslirp \
        sdl2 \
        vulkan-headers \
        spirv-headers \
        spirv-tools \
        glslang

RUN find ${MXE_PATH}/usr -executable -type f -exec chmod a+x {} \;

ENV CROSSPREFIX=x86_64-w64-mingw32.static-
ENV CROSSAR=${CROSSPREFIX}ar
ENV PATH="${MXE_PATH}/.ccache/bin:${MXE_PATH}/usr/x86_64-pc-linux-gnu/bin:${MXE_PATH}/usr/bin:${PATH}"
