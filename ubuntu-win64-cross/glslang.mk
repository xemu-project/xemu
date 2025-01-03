PKG             := glslang
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/glslang
$(PKG)_DESCR    := glslang
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 15.0.0
$(PKG)_SUBDIR   := glslang-$($(PKG)_VERSION)
$(PKG)_FILE     := glslang-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := c31c8c2e89af907507c0631273989526ee7d5cdf7df95ececd628fd7b811e064
$(PKG)_URL      := https://github.com/KhronosGroup/glslang/archive/refs/tags/$($(PKG)_VERSION).tar.gz
$(PKG)_DEPS     := cc spirv-tools

define $(PKG)_BUILD
    $(TARGET)-cmake -B'$(BUILD_DIR)' -S'$(SOURCE_DIR)' \
        -G"Ninja" \
        -DBUILD_SHARED_LIBS=$(CMAKE_SHARED_BOOL) \
        -DENABLE_GLSLANG_BINARIES=OFF \
        -DGLSLANG_TESTS=OFF \
        -DBUILD_EXTERNAL=OFF \
        -DALLOW_EXTERNAL_SPIRV_TOOLS=ON \
        -DVERBOSE=1
    $(TARGET)-cmake --build '$(BUILD_DIR)'
    $(TARGET)-cmake --install '$(BUILD_DIR)'
endef

# FIXME: Shared libs
