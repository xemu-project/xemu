PKG             := spirv-headers
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/SPIRV-Headers
$(PKG)_DESCR    := SPIRV-Headers
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := vulkan-sdk-1.3.296.0
$(PKG)_SUBDIR   := SPIRV-Headers-$($(PKG)_VERSION)
$(PKG)_FILE     := spirv-headers-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := 1423d58a1171611d5aba2bf6f8c69c72ef9c38a0aca12c3493e4fda64c9b2dc6
$(PKG)_URL      := https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
    $(TARGET)-cmake -B'$(BUILD_DIR)' -S'$(SOURCE_DIR)'
    $(TARGET)-cmake --build '$(BUILD_DIR)'
    $(TARGET)-cmake --install '$(BUILD_DIR)'
endef
