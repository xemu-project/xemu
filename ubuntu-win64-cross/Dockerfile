#
# Environment to cross-compile xemu for Windows
#

FROM ubuntu:22.04

ENV MXE_VERSION=ea373fe51c02f60111ef0b7597f52dc380c5c162

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive \
    apt-get -qy install \
        software-properties-common \
        lsb-release \
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
        libltdl-dev \
        libgl-dev \
        libssl-dev \
        libtool-bin \
        libxml-parser-perl \
        lzip \
        make \
        openssl \
        p7zip-full \
        patch \
        perl \
        python3 \
        python3-mako \
        python3-pkg-resources \
        python-is-python3 \
        ruby \
        sed \
        unzip \
        wget \
        xz-utils \
        ninja-build \
        python3-pip \
        python3-yaml

RUN cd /opt \
 && git clone https://github.com/mxe/mxe.git \
 && cd mxe \
 && git checkout ${MXE_VERSION} \
 && make MXE_TARGETS=x86_64-w64-mingw32.static \
         MXE_PLUGIN_DIRS=plugins/gcc10 \
            cc \
            glib \
            libepoxy \
            libusb1 \
            pixman \
            libsamplerate \
            openssl \
            cmake

RUN rm /opt/mxe/src/sdl2*.patch
COPY sdl2.mk /opt/mxe/src/sdl2.mk
RUN V=1 MXE_VERBOSE=1 make -C /opt/mxe \
        MXE_TARGETS=x86_64-w64-mingw32.static \
        MXE_PLUGIN_DIRS=plugins/gcc10 \
            sdl2

COPY libslirp.mk /opt/mxe/src/libslirp.mk
COPY libslirp-1.patch /opt/mxe/src/libslirp-1.patch
RUN V=1 MXE_VERBOSE=1 make -C /opt/mxe \
        MXE_TARGETS=x86_64-w64-mingw32.static \
        MXE_PLUGIN_DIRS=plugins/gcc10 \
            libslirp

RUN find /opt/mxe/usr -executable -type f -exec chmod a+x {} \;

ENV CROSSPREFIX=x86_64-w64-mingw32.static-
ENV CROSSAR=${CROSSPREFIX}gcc-ar
ENV PATH="/opt/mxe/.ccache/bin:/opt/mxe/usr/x86_64-pc-linux-gnu/bin:/opt/mxe/usr/bin:${PATH}"
