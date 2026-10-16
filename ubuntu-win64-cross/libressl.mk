# This file is part of MXE. See LICENSE.md for licensing information.

PKG             := libressl
$(PKG)_WEBSITE  := https://www.libressl.org/
$(PKG)_DESCR    := libressl
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 4.0.0
$(PKG)_SUBDIR   := libressl-$($(PKG)_VERSION)
$(PKG)_FILE     := libressl-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := 4d841955f0acc3dfc71d0e3dd35f283af461222350e26843fea9731c0246a1e4
$(PKG)_URL      := https://github.com/libressl/portable/releases/download/v$($(PKG)_VERSION)/$($(PKG)_FILE)
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
    cd '$(BUILD_DIR)' && $(TARGET)-cmake '$(SOURCE_DIR)' \
        -DVERBOSE=1
    $(MAKE) -C '$(BUILD_DIR)' -j '$(JOBS)'
    $(MAKE) -C '$(BUILD_DIR)' -j 1 install
endef
