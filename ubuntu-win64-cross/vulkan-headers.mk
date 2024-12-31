PKG             := vulkan-headers
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/Vulkan-Headers
$(PKG)_DESCR    := Vulkan-Headers
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := vulkan-sdk-1.3.296.0
$(PKG)_SUBDIR   := Vulkan-Headers-$($(PKG)_VERSION)
$(PKG)_FILE     := vulkan-headers-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := 1e872a0be3890784bbe68dcd89b7e017fed77ba95820841848718c98bda6dc33
$(PKG)_URL      := https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
    $(TARGET)-cmake -B'$(BUILD_DIR)' -S'$(SOURCE_DIR)'
    $(TARGET)-cmake --build '$(BUILD_DIR)'
    $(TARGET)-cmake --install '$(BUILD_DIR)'
endef
