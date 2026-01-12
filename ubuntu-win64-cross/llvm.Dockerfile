#
# Environment to cross-compile xemu for Windows
#

FROM docker.io/library/buildpack-deps:trixie

ENV MXE_PATH=/usr/local/mxe
ENV MXE_TAG=llvm-mingw-20251219
ENV MXE_REPO=https://github.com/kleisauke/mxe.git

ARG TARGETS="x86_64-w64-mingw32.static aarch64-w64-mingw32.static"
ARG JOBS=6

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive \
    apt-get -qy install \
        autopoint \
        bison \
        build-essential \
        flex \
        gettext \
        gperf \
        gtk-update-icon-cache \
        intltool \
        jq \
        libtool-bin \
        libxml-parser-perl \
        lzip \
        p7zip-full \
        python-is-python3 \
        python3-mako \
        python3-venv \
        python3-yaml \
        ruby \
        zip \
        zstd

WORKDIR /usr/local
RUN git clone -b ${MXE_TAG} --single-branch ${MXE_REPO}

WORKDIR /usr/local/mxe

# Bootstrap compilers and utilities
RUN --mount=type=cache,id=mxe-download,target=/usr/local/mxe/pkg \
  echo "MXE_TARGETS := x86_64-pc-linux-gnu" > settings.mk && \
  make autotools cargo-c cc meson nasm pe-util \
    MXE_VERBOSE=true \
    MXE_TMP="/var/tmp" \
    MXE_PLUGIN_DIRS="plugins/llvm-mingw" \
    JOBS=${JOBS}

RUN rm ${MXE_PATH}/src/sdl2*.patch
COPY vulkan-headers.mk \
     glib.mk \
     sdl2.mk \
     libsamplerate.mk \
     libressl.mk \
     curl.mk \
     ${MXE_PATH}/src/

RUN make \
    MXE_TARGETS="${TARGETS}" \
    MXE_VERBOSE=true \
    MXE_TMP="/var/tmp" \
    MXE_PLUGIN_DIRS="plugins/llvm-mingw" \
    JOBS=${JOBS} \
    CFLAGS=-O2 \
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

ENV CROSSPREFIX=aarch64-w64-mingw32.static-
ENV CROSSAR=${CROSSPREFIX}ar
ENV PATH="${MXE_PATH}/.ccache/bin:${MXE_PATH}/usr/x86_64-pc-linux-gnu/bin:${MXE_PATH}/usr/bin:${PATH}"
