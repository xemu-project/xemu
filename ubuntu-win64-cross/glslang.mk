PKG             := glslang
$(PKG)_WEBSITE  := https://github.com/KhronosGroup/glslang
$(PKG)_DESCR    := glslang
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 14.3.0
$(PKG)_SUBDIR   := glslang-$($(PKG)_VERSION)
$(PKG)_FILE     := glslang-$($(PKG)_VERSION).tar.gz
$(PKG)_CHECKSUM := be6339048e20280938d9cb399fcdd06e04f8654d43e170e8cce5a56c9a754284
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
