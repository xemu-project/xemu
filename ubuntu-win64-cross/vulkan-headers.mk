PKG             := vulkan-headers
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/Vulkan-Headers
$(PKG)_DESCR    := Vulkan-Headers
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := vulkan-sdk-1.4.309.0
$(PKG)_SUBDIR   := Vulkan-Headers-$($(PKG)_VERSION)
$(PKG)_FILE     := vulkan-headers-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := 2bc1b4127950badc80212abf1edfa5c3b5032f3425edf37255863ba7592c1969
$(PKG)_URL      := https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
    $(TARGET)-cmake -B'$(BUILD_DIR)' -S'$(SOURCE_DIR)'
    $(TARGET)-cmake --build '$(BUILD_DIR)'
    $(TARGET)-cmake --install '$(BUILD_DIR)'
endef
