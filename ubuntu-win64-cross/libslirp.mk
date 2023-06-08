# This file is part of MXE. See LICENSE.md for licensing information.

PKG             := libslirp
$(PKG)_WEBSITE  := https://gitlab.freedesktop.org/slirp/libslirp
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 4.7.0
$(PKG)_SUBDIR   := libslirp-v$($(PKG)_VERSION)
$(PKG)_FILE     := libslirp-v$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := 9398f0ec5a581d4e1cd6856b88ae83927e458d643788c3391a39e61b75db3d3b
$(PKG)_URL      := https://gitlab.freedesktop.org/slirp/$(PKG)/-/archive/v$($(PKG)_VERSION)/$($(PKG)_FILE)
$(PKG)_DEPS     := cc glib meson-wrapper

define $(PKG)_BUILD
    '$(MXE_MESON_WRAPPER)' $(MXE_MESON_OPTS) \
        $(PKG_MESON_OPTS) \
            --buildtype=plain \
        '$(BUILD_DIR)' '$(SOURCE_DIR)'
    '$(MXE_NINJA)' -C '$(BUILD_DIR)' -j '$(JOBS)' install
endef
