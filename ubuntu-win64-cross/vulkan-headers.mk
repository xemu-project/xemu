PKG             := vulkan-headers
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/Vulkan-Headers
$(PKG)_DESCR    := Vulkan-Headers
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := vulkan-sdk-1.4.304.0
$(PKG)_SUBDIR   := Vulkan-Headers-$($(PKG)_VERSION)
$(PKG)_FILE     := vulkan-headers-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := 46f8f5b6384a36c688e0c40d28d534df41d22de406493dfb5c9b7bcc29672613
$(PKG)_URL      := https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
    $(TARGET)-cmake -B'$(BUILD_DIR)' -S'$(SOURCE_DIR)'
    $(TARGET)-cmake --build '$(BUILD_DIR)'
    $(TARGET)-cmake --install '$(BUILD_DIR)'
endef
