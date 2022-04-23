#
# Environment to cross-compile xemu for Windows
#

FROM ubuntu:20.04

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive \
    apt-get -qy install \
        software-properties-common \
        lsb-release \
        git \
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
        libssl-dev \
        libtool-bin \
        libxml-parser-perl \
        lzip \
        make \
        openssl \
        p7zip-full \
        patch \
        perl \
        python \
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
 && make -C /opt/mxe \
        MXE_TARGETS=x86_64-w64-mingw32.static \
        MXE_PLUGIN_DIRS=plugins/gcc10 \
            cc \
            glib \
            libepoxy \
            pixman \
            libsamplerate \
            openssl \
            cmake

COPY sdl2.mk                                       /opt/mxe/src/sdl2.mk
COPY sdl2-2-link-order.patch                       /opt/mxe/src/sdl2-2-link-order.patch
RUN V=1 MXE_VERBOSE=1 make -C /opt/mxe \
        MXE_TARGETS=x86_64-w64-mingw32.static \
        MXE_PLUGIN_DIRS=plugins/gcc10 \
            sdl2

ENV CROSSPREFIX=x86_64-w64-mingw32.static-
ENV CROSSAR=${CROSSPREFIX}gcc-ar
ENV PATH="/opt/mxe/.ccache/bin:/opt/mxe/usr/x86_64-pc-linux-gnu/bin:/opt/mxe/usr/bin:${PATH}"
