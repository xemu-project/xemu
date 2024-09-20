PKG             := spirv-tools
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/SPIRV-Tools
$(PKG)_DESCR    := SPIRV-Tools
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := vulkan-sdk-1.3.283.0
$(PKG)_SUBDIR   := SPIRV-Tools-$($(PKG)_VERSION)
$(PKG)_FILE     := spirv-tools-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := 5e2e5158bdd7442f9e01e13b5b33417b06cddff4965c9c19aab9763ab3603aae
$(PKG)_URL      := https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc spirv-headers

define $(PKG)_BUILD
    $(TARGET)-cmake -B'$(BUILD_DIR)' -S'$(SOURCE_DIR)' \
        -G"Ninja" \
        -DBUILD_SHARED_LIBS=$(CMAKE_SHARED_BOOL) \
        -DSPIRV-Headers_SOURCE_DIR=$(PREFIX)/$(TARGET) \
        -DSPIRV_WERROR=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DSPIRV_TOOLS_BUILD_STATIC=ON \
        -DSPIRV_SKIP_EXECUTABLES=ON \
        -DSPIRV_SKIP_TESTS=ON \
        -DVERBOSE=1
    $(TARGET)-cmake --build '$(BUILD_DIR)'
    $(TARGET)-cmake --install '$(BUILD_DIR)'
endef

# FIXME: Shared libs
