#
# Environment to cross-compile xemu for Windows
#

FROM ubuntu:24.04

ENV MXE_PATH=/usr/local/mxe
ENV MXE_REPO=https://github.com/mxe/mxe.git
ENV MXE_VERSION=9c716d7337fcec2b95eef7ed8f5970b4b8e97f68

ARG PLUGIN_DIRS="plugins/gcc15"
ARG TARGETS="x86_64-w64-mingw32.static"
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
        xz-utils \
        zstd

RUN git clone ${MXE_REPO} ${MXE_PATH} \
 && cd ${MXE_PATH} \
 && git checkout ${MXE_VERSION}

RUN make \
    MXE_PLUGIN_DIRS="${PLUGIN_DIRS}" \
    MXE_TARGETS="${TARGETS}" \
    JOBS=${JOBS} \
    -C ${MXE_PATH} \
        cc

RUN rm ${MXE_PATH}/src/sdl2*.patch
COPY vulkan-headers.mk \
     glib.mk \
     sdl2.mk \
     libsamplerate.mk \
     libressl.mk \
     curl.mk \
     ${MXE_PATH}/src/

RUN make \
    MXE_PLUGIN_DIRS="${PLUGIN_DIRS}" \
    MXE_TARGETS="${TARGETS}" \
    JOBS=${JOBS} \
    CFLAGS=-O2 \
    -C ${MXE_PATH} \
        glib \
        libepoxy \
        libusb1 \
        pixman \
        libsamplerate \
        libressl \
        cmake \
        libslirp \
        sdl2 \
        vulkan-headers \
        curl

RUN find ${MXE_PATH}/usr -executable -type f -exec chmod a+x {} \;

ENV CROSSPREFIX=x86_64-w64-mingw32.static-
ENV CROSSAR=${CROSSPREFIX}gcc-ar
ENV PATH="${MXE_PATH}/.ccache/bin:${MXE_PATH}/usr/x86_64-pc-linux-gnu/bin:${MXE_PATH}/usr/bin:${PATH}"
