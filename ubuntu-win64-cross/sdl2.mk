# This file is part of MXE. See LICENSE.md for licensing information.

PKG             := sdl2
$(PKG)_WEBSITE  := https://www.libsdl.org/
$(PKG)_DESCR    := SDL2
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 2.0.12
# $(PKG)_VERSION  := 2.0.14
$(PKG)_CHECKSUM := 349268f695c02efbc9b9148a70b85e58cefbbf704abd3e91be654db7f1e2c863
# $(PKG)_CHECKSUM := d8215b571a581be1332d2106f8036fcb03d12a70bae01e20f424976d275432bc
$(PKG)_SUBDIR   := SDL2-$($(PKG)_VERSION)
$(PKG)_FILE     := SDL2-$($(PKG)_VERSION).tar.gz
$(PKG)_URL      := https://www.libsdl.org/release/$($(PKG)_FILE)
$(PKG)_DEPS     := cc libiconv libsamplerate

define $(PKG)_UPDATE
    $(WGET) -q -O- 'https://hg.libsdl.org/SDL/tags' | \
    $(SED) -n 's,.*release-\([0-9][^<]*\).*,\1,p' | \
    head -1
endef

define $(PKG)_BUILD
    cd '$(BUILD_DIR)' && $(TARGET)-cmake '$(SOURCE_DIR)' \
        -DSDL_SHARED=$(CMAKE_SHARED_BOOL) \
        -DSDL_STATIC=$(CMAKE_STATIC_BOOL) \
        -DVERBOSE=1
    $(MAKE) -C '$(BUILD_DIR)' -j '$(JOBS)'
    $(MAKE) -C '$(BUILD_DIR)' -j 1 install

    '$(TARGET)-gcc' \
        -W -Wall -Werror -ansi -pedantic \
        '$(TEST_FILE)' -o '$(PREFIX)/$(TARGET)/bin/test-sdl2.exe' \
        `'$(TARGET)-pkg-config' sdl2 --cflags --libs`
endef
