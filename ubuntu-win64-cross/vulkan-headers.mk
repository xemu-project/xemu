PKG             := vulkan-headers
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/Vulkan-Headers
$(PKG)_DESCR    := Vulkan-Headers
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := vulkan-sdk-1.3.283.0
$(PKG)_SUBDIR   := Vulkan-Headers-$($(PKG)_VERSION)
$(PKG)_FILE     := vulkan-headers-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := cf54a812911b4e3e4ff15716c222a8fb9a87c2771c0b86060cb0ca2570ea55a9
$(PKG)_URL      := https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
    $(TARGET)-cmake -B'$(BUILD_DIR)' -S'$(SOURCE_DIR)'
    $(TARGET)-cmake --build '$(BUILD_DIR)'
    $(TARGET)-cmake --install '$(BUILD_DIR)'
endef
