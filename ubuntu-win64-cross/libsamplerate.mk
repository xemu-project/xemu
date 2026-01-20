# This file is part of MXE. See LICENSE.md for licensing information.

PKG             := libsamplerate
$(PKG)_WEBSITE  := http://libsndfile.github.io/libsamplerate/
$(PKG)_DESCR    := libsamplerate
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 0.2.2
$(PKG)_SUBDIR   := libsamplerate-$($(PKG)_VERSION)
$(PKG)_FILE     := libsamplerate-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := 16e881487f184250deb4fcb60432d7556ab12cb58caea71ef23960aec6c0405a
$(PKG)_URL      := https://github.com/libsndfile/libsamplerate/archive/refs/tags/$($(PKG)_VERSION)/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
    cd '$(BUILD_DIR)' && $(TARGET)-cmake '$(SOURCE_DIR)' \
        -DLIBSAMPLERATE_EXAMPLES=OFF \
        -DVERBOSE=1
    $(MAKE) -C '$(BUILD_DIR)' -j '$(JOBS)'
    $(MAKE) -C '$(BUILD_DIR)' -j 1 install
endef
