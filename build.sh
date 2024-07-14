#!/usr/bin/env bash

set -e # exit if a command fails
set -o pipefail # Will return the exit status of make if it fails
set -o physical # Resolve symlinks when changing directory

project_source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

target_arch=$(uname -m)

package_windows() {
    rm -rf dist
    mkdir -p dist
    cp build/qemu-system-i386w.exe dist/xemu.exe
    python3 "${project_source_dir}/get_deps.py" dist/xemu.exe dist
}

package_wincross() {
    rm -rf dist
    mkdir -p dist
    cp build/qemu-system-i386w.exe dist/xemu.exe
    python3 ./scripts/gen-license.py --platform windows > dist/LICENSE.txt
}

package_macos() {
    rm -rf dist

    # Copy in executable
    mkdir -p dist/xemu.app/Contents/MacOS/
    exe_path=dist/xemu.app/Contents/MacOS/xemu
    lib_path=dist/xemu.app/Contents/Libraries/${target_arch}
    lib_rpath=../Libraries/${target_arch}
    cp build/qemu-system-i386 ${exe_path}

    # Copy in in executable dylib dependencies
    dylibbundler -cd -of -b -x dist/xemu.app/Contents/MacOS/xemu \
        -d ${lib_path}/ \
        -p "@executable_path/${lib_rpath}/" \
        -s ${PWD}/macos-libs/${target_arch}/opt/local/libexec/openssl11/lib/ \
        -s ${PWD}/macos-libs/${target_arch}/opt/local/lib/

    # Fixup some paths dylibbundler missed
    for dep in $(otool -L "$exe_path" | grep -e '/opt/local/' | cut -d' ' -f1); do
      dep_basename="$(basename $dep)"
      new_path="@executable_path/${lib_rpath}/${dep_basename}"
      echo "Fixing $exe_path dependency $dep_basename -> $new_path"
      install_name_tool -change "$dep" "$new_path" "$exe_path"
    done

    for lib_path in ${lib_path}/*.dylib; do
      for dep in $(otool -L "$lib_path" | grep -e '/opt/local/' | cut -d' ' -f1); do
        dep_basename="$(basename $dep)"
        new_path="@rpath/${dep_basename}"
        echo "Fixing $lib_path dependency $dep_basename -> $new_path"
        install_name_tool -change "$dep" "$new_path" "$lib_path"
        codesign -s - -f "${lib_path}"
      done
    done

    # Copy in runtime resources
    mkdir -p dist/xemu.app/Contents/Resources

    # Generate icon file
    mkdir -p xemu.iconset
    for r in 16 32 128 256 512; do cp "${project_source_dir}/ui/icons/xemu_${r}x${r}.png" "xemu.iconset/icon_${r}x${r}.png"; done
    iconutil --convert icns --output dist/xemu.app/Contents/Resources/xemu.icns xemu.iconset

    cp Info.plist dist/xemu.app/Contents/

    plutil -replace CFBundleShortVersionString -string $(cat ${project_source_dir}/XEMU_VERSION | cut -f1 -d-) dist/xemu.app/Contents/Info.plist
    plutil -replace CFBundleVersion            -string $(cat ${project_source_dir}/XEMU_VERSION | cut -f1 -d-) dist/xemu.app/Contents/Info.plist

    codesign --force --deep --preserve-metadata=entitlements,requirements,flags,runtime --sign - "${exe_path}"
    python3 ./scripts/gen-license.py --version-file=macos-libs/$target_arch/INSTALLED > dist/LICENSE.txt
}

package_linux() {
    rm -rf dist
    mkdir -p dist
    cp build/qemu-system-i386 dist/xemu
    if test -e "${project_source_dir}/XEMU_LICENSE"; then
      cp "${project_source_dir}/XEMU_LICENSE" dist/LICENSE.txt
    else
      python3 ./scripts/gen-license.py > dist/LICENSE.txt
    fi
}

postbuild=''
debug_opts=''
build_cflags=''
default_job_count='12'
sys_ldflags=''

get_job_count () {
	if command -v 'nproc' >/dev/null
	then
		nproc
	else
		case "$(uname -s)" in
			'Linux')
				egrep "^processor" /proc/cpuinfo | wc -l
				;;
			'FreeBSD')
				sysctl -n hw.ncpu
				;;
			'Darwin')
				sysctl -n hw.logicalcpu 2>/dev/null \
				|| sysctl -n hw.ncpu
				;;
			'MSYS_NT-'*|'CYGWIN_NT-'*|'MINGW'*'_NT-'*)
				if command -v 'wmic' >/dev/null
				then
					wmic cpu get NumberOfLogicalProcessors/Format:List \
						| grep -m1 '=' | cut -f2 -d'='
				else
					echo "${NUMBER_OF_PROCESSORS:-${default_job_count}}"
				fi
				;;
			*)
				echo "${default_job_count}"
				;;
		esac
	fi
}

job_count="$(get_job_count)" 2>/dev/null
job_count="${job_count:-${default_job_count}}"
debug=""
opts=""
platform="$(uname -s)"

while [ ! -z "${1}" ]
do
    case "${1}" in
    '-j'*)
        job_count="${1:2}"
        shift
        ;;
    '--debug')
        debug="y"
        shift
        ;;
    '-p'*)
        platform="${2}"
        shift 2
        ;;
    '-a'*)
        target_arch="${2}"
        shift 2
        ;;
    *)
        break
        ;;
    esac
done

target="qemu-system-i386"
if test ! -z "$debug"; then
    build_cflags='-DXEMU_DEBUG_BUILD=1'
    opts="--enable-debug --enable-trace-backends=log"
else
    opts="--enable-lto"
fi

most_recent_macosx_sdk_ver () {
  local min_ver="${1}"
  local macos_sdk_base=/Library/Developer/CommandLineTools/SDKs
  local sdks=("${macos_sdk_base}"/MacOSX[0-9]*.[0-9]*.sdk)
  for i in "${!sdks[@]}"; do
    local newval="${sdks[i]##${macos_sdk_base}/MacOSX}"
    sdks[$i]="${newval%%.sdk}"
  done

  IFS=$'\n' sdks=($(sort -nr <<<"${sdks[*]}"))
  unset IFS

  local newest_sdk_ver="${sdks[0]}"

  local sdk_path="${macos_sdk_base}/MacOSX${newest_sdk_ver}.sdk"
  if ! test -d "${sdk_path}"; then
    echo ""
    return
  fi

  if ! LC_ALL=C awk 'BEGIN {exit ('${newest_sdk_ver}' < '${min_ver}')}'; then
    echo ""
    return
  fi
  echo "${sdk_path}"
}

case "$platform" in # Adjust compilation options based on platform
    Linux)
        echo 'Compiling for Linux...'
        sys_cflags='-Wno-error=redundant-decls'
        opts="$opts --disable-werror"
        postbuild='package_linux'
        ;;
    Darwin)
        echo "Compiling for MacOS for $target_arch..."
        if [ "$target_arch" == "arm64" ]; then
            macos_min_ver=12.7.5
        elif [ "$target_arch" == "x86_64" ]; then
            macos_min_ver=12.7.5
        else
            echo "Unsupported arch $target_arch"
            exit 1
        fi

        sdk="$(most_recent_macosx_sdk_ver ${macos_min_ver})"
        if [[ -z "${sdk}" ]]; then
          echo "SDK >= ${macos_min_ver} not found. Install Xcode Command Line Tools"
          exit 1
        fi

        python3 ./scripts/download-macos-libs.py ${target_arch}
        lib_prefix=${PWD}/macos-libs/${target_arch}/opt/local
        export CFLAGS="-arch ${target_arch} \
                       -target ${target_arch}-apple-macos${macos_min_ver} \
                       -isysroot ${sdk} \
                       -I${lib_prefix}/include \
                       -mmacosx-version-min=$macos_min_ver"
        export LDFLAGS="-arch ${target_arch} \
                        -isysroot ${sdk}"
        if [ "$target_arch" == "x86_64" ]; then
            sys_cflags='-march=ivybridge'
        fi
        sys_ldflags='-headerpad_max_install_names'
        export PKG_CONFIG_PATH="${lib_prefix}/lib/pkgconfig"
        export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:${lib_prefix}/libexec/openssl11/lib/pkgconfig"
        opts="$opts --disable-cocoa --cross-prefix="
        postbuild='package_macos'
        ;;
    CYGWIN*|MINGW*|MSYS*)
        echo 'Compiling for Windows...'
        sys_cflags='-Wno-error'
        CFLAGS="${CFLAGS} -lIphlpapi -lCrypt32" # workaround for linking libs on mingw
        opts="$opts --disable-fortify-source"
        postbuild='package_windows' # set the above function to be called after build
        target="qemu-system-i386w.exe"
        ;;
    win64-cross)
        echo 'Cross-compiling for Windows...'
        export AR=${AR:-$CROSSAR}
        sys_cflags='-Wno-error'
        opts="$opts --cross-prefix=$CROSSPREFIX --static --disable-fortify-source"
        postbuild='package_wincross' # set the above function to be called after build
        target="qemu-system-i386w.exe"
        ;;
    *)
        echo "Unsupported platform $platform, aborting" >&2
        exit -1
        ;;
esac

# find absolute path (and resolve symlinks) to build out of tree
configure="${project_source_dir}/configure"

set -x # Print commands from now on

"${configure}" \
    --extra-cflags="-DXBOX=1 ${build_cflags} ${sys_cflags} ${CFLAGS}" \
    --extra-ldflags="${sys_ldflags}" \
    --target-list=i386-softmmu \
    ${opts} \
    "$@"

time make -j"${job_count}" ${target} 2>&1 | tee build.log

"${postbuild}" # call post build functions
