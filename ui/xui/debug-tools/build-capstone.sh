#!/usr/bin/env bash
# Xemu Debug Tools - target-aware Capstone bootstrap.
#
# Builds a static Capstone library containing the X86 decoder for the host/target
# that is currently compiling xemu. The xemu guest CPU remains x86 regardless of
# whether the xemu executable itself targets x86_64, ARM64, Linux, macOS, or
# Windows.

set -euo pipefail

CAPSTONE_VERSION="${CAPSTONE_VERSION:-5.0.9}"
CAPSTONE_MIN_VERSION="${CAPSTONE_MIN_VERSION:-3.0.5}"
CAPSTONE_SOURCE_URL="${CAPSTONE_SOURCE_URL:-https://github.com/capstone-engine/capstone/archive/refs/tags/${CAPSTONE_VERSION}.tar.gz}"

platform="${CAPSTONE_TARGET_PLATFORM:-}"
arch="${CAPSTONE_TARGET_ARCH:-}"
env_file=""
macos_sdk="${CAPSTONE_MACOS_SDK:-}"
macos_min_version="${CAPSTONE_MACOS_MIN_VERSION:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --platform) platform="$2"; shift 2 ;;
    --arch) arch="$2"; shift 2 ;;
    --env-file) env_file="$2"; shift 2 ;;
    --macos-sdk) macos_sdk="$2"; shift 2 ;;
    --macos-min-version) macos_min_version="$2"; shift 2 ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [[ -z "$platform" ]]; then
  platform="$(uname -s)"
fi
if [[ -z "$arch" ]]; then
  arch="$(uname -m)"
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
project_root="$(cd "${script_dir}/../../.." >/dev/null 2>&1 && pwd)"

log() { printf '[Debug Tools / Capstone] %s\n' "$*" >&2; }
need_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Required command not found: $1" >&2
    exit 1
  }
}

# Turn CC='ccache clang-21' into a compiler + optional launcher that CMake can
# consume reliably.
compiler_from_spec() {
  local spec="$1"
  local first rest
  first="${spec%% *}"
  rest="${spec#"$first"}"
  rest="${rest# }"
  if [[ "$first" == "ccache" && -n "$rest" ]]; then
    CAPSTONE_CMAKE_LAUNCHER="ccache"
    printf '%s\n' "${rest%% *}"
  else
    printf '%s\n' "$first"
  fi
}

CAPSTONE_CMAKE_LAUNCHER=""
target_kind="native"
target_arch="$arch"
tool_prefix=""

case "$platform" in
  win64-cross)
    target_kind="windows-cross"
    tool_prefix="${CROSSPREFIX:-}"
    if [[ -z "$tool_prefix" ]]; then
      echo "win64-cross Capstone bootstrap requires CROSSPREFIX" >&2
      exit 1
    fi
    case "$(basename "${tool_prefix%-}")" in
      aarch64-*|arm64-*) target_arch="aarch64" ;;
      x86_64-*) target_arch="x86_64" ;;
    esac
    ;;
  CYGWIN*|MINGW*|MSYS*)
    target_kind="windows-native"
    ;;
  Darwin)
    target_kind="macos"
    ;;
  Linux)
    # Debian's cross-package path exposes the host/build triplets even though
    # build.sh itself is running on the build architecture.
    if [[ -n "${DEB_HOST_GNU_TYPE:-}" && -n "${DEB_BUILD_GNU_TYPE:-}" &&
          "${DEB_HOST_GNU_TYPE}" != "${DEB_BUILD_GNU_TYPE}" ]]; then
      target_kind="linux-cross"
      tool_prefix="${DEB_HOST_GNU_TYPE}-"
      case "${DEB_HOST_ARCH:-}" in
        arm64) target_arch="aarch64" ;;
        amd64) target_arch="x86_64" ;;
        *) target_arch="${DEB_HOST_ARCH:-$arch}" ;;
      esac
    fi
    ;;
  *)
    echo "Unsupported Capstone bootstrap platform: $platform" >&2
    exit 1
    ;;
esac

if [[ "$target_kind" == "windows-cross" || "$target_kind" == "linux-cross" ]]; then
  compiler_name="${tool_prefix}gcc"
  if ! command -v "$compiler_name" >/dev/null 2>&1; then
    compiler_name="${tool_prefix}clang"
  fi
  CAPSTONE_CC="${CAPSTONE_CC:-$(command -v "$compiler_name")}" 
  if [[ -n "${CROSSAR:-}" && "$target_kind" == "windows-cross" ]]; then
    CAPSTONE_AR="${CAPSTONE_AR:-$(command -v "${CROSSAR}")}" 
  else
    CAPSTONE_AR="${CAPSTONE_AR:-$(command -v "${tool_prefix}ar")}" 
  fi
  CAPSTONE_RANLIB="${CAPSTONE_RANLIB:-$(command -v "${tool_prefix}ranlib")}" 
  if command -v "${tool_prefix}pkg-config" >/dev/null 2>&1; then
    CAPSTONE_PKG_CONFIG="${CAPSTONE_PKG_CONFIG:-$(command -v "${tool_prefix}pkg-config")}"
  else
    CAPSTONE_PKG_CONFIG="${CAPSTONE_PKG_CONFIG:-$(command -v pkg-config)}"
  fi
else
  cc_spec="${CAPSTONE_CC:-${CC:-cc}}"
  CAPSTONE_CC="$(compiler_from_spec "$cc_spec")"
  CAPSTONE_CC="$(command -v "$CAPSTONE_CC")"
  CAPSTONE_AR="${CAPSTONE_AR:-${AR:-}}"
  CAPSTONE_RANLIB="${CAPSTONE_RANLIB:-${RANLIB:-}}"
  CAPSTONE_PKG_CONFIG="${CAPSTONE_PKG_CONFIG:-${PKG_CONFIG:-pkg-config}}"
  CAPSTONE_PKG_CONFIG="$(command -v "$CAPSTONE_PKG_CONFIG")"
  if [[ -n "$CAPSTONE_AR" ]]; then CAPSTONE_AR="$(command -v "$CAPSTONE_AR")"; fi
  if [[ -n "$CAPSTONE_RANLIB" ]]; then CAPSTONE_RANLIB="$(command -v "$CAPSTONE_RANLIB")"; fi
fi

need_command cmake
need_command curl
need_command tar
need_command ninja

case "$target_kind" in
  windows-cross)
    triplet="$(basename "${tool_prefix%-}")"
    CAPSTONE_PREFIX="${CAPSTONE_PREFIX:-/usr/local/mxe/usr/${triplet}}"
    ;;
  *)
    safe_platform="$(printf '%s' "$platform" | tr '/: ' '___')"
    CAPSTONE_PREFIX="${CAPSTONE_PREFIX:-${TMPDIR:-/tmp}/xemu-debug-tools-capstone/${safe_platform}-${target_arch}}"
    ;;
esac

pkg_paths=("${CAPSTONE_PREFIX}/lib/pkgconfig" "${CAPSTONE_PREFIX}/lib64/pkgconfig")
pkg_path_joined="${pkg_paths[0]}:${pkg_paths[1]}"
probe_pkg_path="$pkg_path_joined"
if [[ -n "${PKG_CONFIG_PATH:-}" ]]; then
  probe_pkg_path="${probe_pkg_path}:${PKG_CONFIG_PATH}"
fi

smoke_dir="${TMPDIR:-/tmp}/xemu-capstone-smoke-${$}"
mkdir -p "$smoke_dir"
trap 'rm -rf "$smoke_dir"' EXIT
printf '#include <capstone.h>\nint main(void) { csh h = 0; return (int)h; }\n' > "${smoke_dir}/capstone.c"

probe_capstone() {
  local cflags
  PKG_CONFIG_PATH="$probe_pkg_path" "$CAPSTONE_PKG_CONFIG" --atleast-version="$CAPSTONE_MIN_VERSION" capstone >/dev/null 2>&1 || return 1
  cflags="$(PKG_CONFIG_PATH="$probe_pkg_path" "$CAPSTONE_PKG_CONFIG" --cflags capstone)"
  # Intentional token splitting: pkg-config returns compiler flags.
  # shellcheck disable=SC2086
  "$CAPSTONE_CC" ${CFLAGS:-} $cflags -c "${smoke_dir}/capstone.c" -o "${smoke_dir}/capstone.o" >/dev/null 2>&1 || return 1
  test -s "${smoke_dir}/capstone.o"
}

emit_environment() {
  if [[ -n "$env_file" ]]; then
    {
      printf 'XEMU_CAPSTONE_PKG_CONFIG_PATH=%q\n' "$pkg_path_joined"
      printf 'XEMU_CAPSTONE_TARGET_KIND=%q\n' "$target_kind"
      printf 'XEMU_CAPSTONE_TARGET_ARCH=%q\n' "$target_arch"
    } > "$env_file"
  fi
}

if probe_capstone; then
  version="$(PKG_CONFIG_PATH="$probe_pkg_path" "$CAPSTONE_PKG_CONFIG" --modversion capstone)"
  log "using existing Capstone ${version} for ${target_kind}/${target_arch}"
  emit_environment
  exit 0
fi

log "building Capstone ${CAPSTONE_VERSION} for ${target_kind}/${target_arch}"
log "compiler: ${CAPSTONE_CC}"
log "install prefix: ${CAPSTONE_PREFIX}"

cache_dir="${XEMU_CAPSTONE_CACHE_DIR:-${TMPDIR:-/tmp}/xemu-capstone-source-cache}"
archive="${cache_dir}/capstone-${CAPSTONE_VERSION}.tar.gz"
source_dir="${TMPDIR:-/tmp}/capstone-${CAPSTONE_VERSION}"
build_dir="${TMPDIR:-/tmp}/capstone-build-${target_kind}-${target_arch}"
mkdir -p "$cache_dir"
if [[ ! -s "$archive" ]]; then
  curl -fL --retry 3 --retry-delay 2 "$CAPSTONE_SOURCE_URL" -o "${archive}.tmp"
  mv "${archive}.tmp" "$archive"
fi
rm -rf "$source_dir" "$build_dir"
tar -xzf "$archive" -C "${TMPDIR:-/tmp}"
test -f "${source_dir}/CMakeLists.txt"

cmake_args=(
  -S "$source_dir"
  -B "$build_dir"
  -G Ninja
  "-DCMAKE_C_COMPILER=${CAPSTONE_CC}"
  "-DCMAKE_INSTALL_PREFIX=${CAPSTONE_PREFIX}"
  -DCMAKE_BUILD_TYPE=Release
  -DBUILD_SHARED_LIBS=OFF
  -DBUILD_STATIC_LIBS=ON
  -DCAPSTONE_ARCHITECTURE_DEFAULT=OFF
  -DCAPSTONE_X86_SUPPORT=ON
  -DCAPSTONE_BUILD_TESTS=OFF
  -DCAPSTONE_BUILD_CSTOOL=OFF
  -DCAPSTONE_BUILD_CSTEST=OFF
)

if [[ -n "$CAPSTONE_CMAKE_LAUNCHER" ]]; then
  cmake_args+=("-DCMAKE_C_COMPILER_LAUNCHER=${CAPSTONE_CMAKE_LAUNCHER}")
fi
if [[ -n "$CAPSTONE_AR" ]]; then cmake_args+=("-DCMAKE_AR=${CAPSTONE_AR}"); fi
if [[ -n "$CAPSTONE_RANLIB" ]]; then cmake_args+=("-DCMAKE_RANLIB=${CAPSTONE_RANLIB}"); fi

case "$target_kind" in
  windows-cross)
    cmake_args+=(
      -DCMAKE_SYSTEM_NAME=Windows
      "-DCMAKE_SYSTEM_PROCESSOR=${target_arch}"
    )
    ;;
  linux-cross)
    cmake_args+=(
      -DCMAKE_SYSTEM_NAME=Linux
      "-DCMAKE_SYSTEM_PROCESSOR=${target_arch}"
    )
    ;;
  macos)
    cmake_args+=("-DCMAKE_OSX_ARCHITECTURES=${target_arch}")
    if [[ -n "$macos_sdk" ]]; then cmake_args+=("-DCMAKE_OSX_SYSROOT=${macos_sdk}"); fi
    if [[ -n "$macos_min_version" ]]; then cmake_args+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=${macos_min_version}"); fi
    ;;
esac

cmake "${cmake_args[@]}"
cmake --build "$build_dir" --parallel
cmake --install "$build_dir"

if ! probe_capstone; then
  echo "Capstone built, but target pkg-config/header verification failed" >&2
  exit 1
fi
version="$(PKG_CONFIG_PATH="$probe_pkg_path" "$CAPSTONE_PKG_CONFIG" --modversion capstone)"
log "verified Capstone ${version} for ${target_kind}/${target_arch}"
emit_environment
