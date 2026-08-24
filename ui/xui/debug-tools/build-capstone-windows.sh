#!/usr/bin/env bash
# Compatibility entry point for the existing local Windows Docker builder.
# The actual target-aware implementation now lives in build-capstone.sh.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
exec bash "${script_dir}/build-capstone.sh" \
  --platform "${CAPSTONE_TARGET_PLATFORM:-win64-cross}" \
  --arch "${CAPSTONE_TARGET_ARCH:-$(uname -m)}" \
  "$@"
