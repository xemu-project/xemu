# This file is part of MXE. See LICENSE.md for licensing information.

PKG             := sdl2
$(PKG)_WEBSITE  := https://www.libsdl.org/
$(PKG)_DESCR    := SDL2
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 2.30.10
$(PKG)_SUBDIR   := SDL2-$($(PKG)_VERSION)
$(PKG)_FILE     := SDL2-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := f59adf36a0fcf4c94198e7d3d776c1b3824211ab7aeebeb31fe19836661196aa
$(PKG)_URL      := https://github.com/libsdl-org/SDL/releases/download/release-$($(PKG)_VERSION)/$($(PKG)_FILE)
$(PKG)_GH_CONF  := libsdl-org/SDL/releases/tag,release-,,
$(PKG)_DEPS     := cc libiconv libsamplerate

define $(PKG)_BUILD
    cd '$(BUILD_DIR)' && $(TARGET)-cmake '$(SOURCE_DIR)' \
        -DSDL_SHARED=$(CMAKE_SHARED_BOOL) \
        -DSDL_STATIC=$(CMAKE_STATIC_BOOL) \
        -DVERBOSE=1
    $(MAKE) -C '$(BUILD_DIR)' -j '$(JOBS)'
    $(MAKE) -C '$(BUILD_DIR)' -j 1 install
endef
