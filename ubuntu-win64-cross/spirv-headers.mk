PKG             := spirv-headers
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/SPIRV-Headers
$(PKG)_DESCR    := SPIRV-Headers
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := vulkan-sdk-1.3.283.0
$(PKG)_SUBDIR   := SPIRV-Headers-$($(PKG)_VERSION)
$(PKG)_FILE     := spirv-headers-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := a68a25996268841073c01514df7bab8f64e2db1945944b45087e5c40eed12cb9
$(PKG)_URL      := https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
    $(TARGET)-cmake -B'$(BUILD_DIR)' -S'$(SOURCE_DIR)'
    $(TARGET)-cmake --build '$(BUILD_DIR)'
    $(TARGET)-cmake --install '$(BUILD_DIR)'
endef
