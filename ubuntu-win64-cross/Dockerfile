#
# Environment to cross-compile xemu for Windows
#

FROM ubuntu:24.04

ENV MXE_PATH=/opt/mxe
ENV MXE_REPO=https://github.com/kleisauke/mxe
ENV MXE_VERSION=llvm-mingw-20251119

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
        libclang-dev \
        libgdk-pixbuf2.0-dev \
        libltdl-dev \
        libgl-dev \
        libpcre2-dev \
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
        python3 \
        python3-mako \
        python3-packaging \
        python3-pkg-resources \
        python3-yaml \
        python3-setuptools \
        python-is-python3 \
        ruby \
        sed \
        sqlite3 \
        unzip \
        wget \
        xz-utils \
        zip

RUN git clone -b ${MXE_VERSION} ${MXE_REPO} ${MXE_PATH}

RUN echo "MXE_TARGETS := x86_64-pc-linux-gnu" > ${MXE_PATH}/settings.mk
RUN make \
    MXE_PLUGIN_DIRS="plugins/llvm-mingw" \
    MXE_VERBOSE=true \
    JOBS=${JOBS} \
    MXE_TMP="/var/tmp" \
    MXE_USE_CCACHE= \
    -C ${MXE_PATH} \
         autotools cargo-c cc meson nasm pe-util

RUN rm ${MXE_PATH}/src/sdl2*.patch
COPY vulkan-headers.mk \
     glib.mk \
     sdl2.mk \
     libsamplerate.mk \
     libressl.mk \
     ${MXE_PATH}/src/

RUN make \
    MXE_TARGETS="${TARGETS}" \
    MXE_PLUGIN_DIRS="plugins/llvm-mingw" \
    MXE_VERBOSE=true \
    JOBS=${JOBS} \
    CFLAGS=-O2 \
    MXE_USE_CCACHE= \
    -C ${MXE_PATH} \
        glib \
        libepoxy \
        pixman \
        libsamplerate \
        libressl \
        cmake \
        libslirp \
        sdl2 \
        vulkan-headers

RUN find ${MXE_PATH}/usr -executable -type f -exec chmod a+x {} \;

ENV CROSSPREFIX=x86_64-w64-mingw32.static-
ENV CROSSAR=${CROSSPREFIX}ar
ENV PATH="${MXE_PATH}/.ccache/bin:${MXE_PATH}/usr/x86_64-pc-linux-gnu/bin:${MXE_PATH}/usr/bin:${PATH}"
