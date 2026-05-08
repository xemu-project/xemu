#!/bin/bash
set -euo pipefail

usage() {
  cat <<'EOF'
Sign and notarize xemu macOS releases.

Usage:
    sign-macos-release.sh [OPTIONS]

Options:
    --p12-file <PATH>
        Path to signing certificate

    --p12-password-file <PATH>
        Path to certificate password

    --api-key-path <PATH>
        Path to App Store Connect API key

    --tag latest|<TAG>
        GitHub release tag with .zip asset to sign and notarize

    --file <PATH>
        Local release .zip asset to sign and notarize
EOF
}

REPO=xemu-project/xemu
ENTITLEMENTS="$PWD/xemu.entitlements"

P12_FILE=""
P12_PASSWORD_FILE=""
API_KEY_PATH=""
INPUT_FILE_PATH=""
TAG=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --p12-file) P12_FILE="$(realpath "$2")"; shift 2 ;;
    --p12-password-file) P12_PASSWORD_FILE="$(realpath "$2")"; shift 2 ;;
    --api-key-path) API_KEY_PATH="$(realpath "$2")"; shift 2 ;;
    --file) INPUT_FILE_PATH="$(realpath "$2")"; shift 2 ;;
    --tag) TAG="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; usage; exit 2 ;;
  esac
done

if [[ -z "$P12_FILE" || -z "$P12_PASSWORD_FILE" || -z "$API_KEY_PATH" ]]; then
    echo "Missing required args." >&2
    usage
    exit 2
fi

if [[ -z "$INPUT_FILE_PATH" && -z "$TAG" ]]; then
    echo "Specify --file or --tag" >&2
    usage
    exit 2
fi

echo ">> Tool versions"
rcodesign --version
gh --version

if [[ "$TAG" == "latest" ]]; then
    echo "[*] Getting latest tag..."
    TAG="$(gh release view --repo "$REPO" --json tagName -q .tagName)"
    echo "    -> $TAG"
fi

if [[ "$INPUT_FILE_PATH" ]]; then
    UNSIGNED_ZIP_PATH="$INPUT_FILE_PATH"
    SIGNED_ZIP_PATH="$INPUT_FILE_PATH"
elif [[ "$TAG" ]]; then
    echo "[*] Downloading unsigned release archive for tag $TAG..."
    VERSION=${TAG#v}
    UNSIGNED_ZIP_PATH="$PWD/xemu-${VERSION}-macos-universal-unsigned.zip"
    SIGNED_ZIP_PATH="$PWD/xemu-${VERSION}-macos-universal.zip"
    gh release download "$TAG" -R "$REPO" -p "$(basename "$UNSIGNED_ZIP_PATH")"
fi

TMPDIR=codesign-tmp-dir
rm -rf $TMPDIR
mkdir $TMPDIR
pushd $TMPDIR

echo "[*] Extracting release"
unzip "$UNSIGNED_ZIP_PATH"

echo "[*] Signing xemu.app"
rcodesign sign \
    --p12-file "$P12_FILE" \
    --p12-password-file "$P12_PASSWORD_FILE" \
    --entitlements-xml-file "$ENTITLEMENTS" \
    --for-notarization \
    ./xemu.app

echo "[*] Notarizing..."
rcodesign notary-submit --api-key-path "$API_KEY_PATH" --staple ./xemu.app

echo "[*] Creating new release archive..."
zip -r "$SIGNED_ZIP_PATH" *

popd

if [[ "$TAG" ]]; then
    echo "[*] Uploading signed release archive..."
    gh release upload "$TAG" -R "$REPO" "$SIGNED_ZIP_PATH" --clobber
fi
