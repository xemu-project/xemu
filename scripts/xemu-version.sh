#!/bin/sh

set -eu

dir="$1"
XEMU_DATE=$(date -u)
XEMU_COMMIT=$( \
  cd "$dir"; \
  if test -e .git; then \
    git rev-parse HEAD 2>/dev/null | tr -d '\n'; \
  elif test -e XEMU_COMMIT; then \
    cat XEMU_COMMIT; \
  fi)
XEMU_BRANCH=$( \
  cd "$dir"; \
  if test -e .git; then \
    git symbolic-ref --short HEAD || echo $XEMU_COMMIT; \
  elif test -e XEMU_BRANCH; then \
    cat XEMU_BRANCH; \
  fi)
XEMU_VERSION=$( \
  cd "$dir"; \
  if test -e .git; then \
    git describe --tags --match 'v*' | cut -c 2- | tr -d '\n'; \
  elif test -e XEMU_VERSION; then \
    cat XEMU_VERSION; \
  fi)

get_version_field() {
  echo ${XEMU_VERSION}-0 | cut -d- -f$1
}

get_version_dot () {
  echo $(get_version_field 1) | cut -d. -f$1
}

XEMU_VERSION_MAJOR=$(get_version_dot 1)
XEMU_VERSION_MINOR=$(get_version_dot 2)
XEMU_VERSION_PATCH=$(get_version_dot 3)
XEMU_VERSION_COMMIT=$(get_version_field 2)

cat <<EOF
#define XEMU_VERSION       "$XEMU_VERSION"
#define XEMU_VERSION_MAJOR $XEMU_VERSION_MAJOR
#define XEMU_VERSION_MINOR $XEMU_VERSION_MINOR
#define XEMU_VERSION_PATCH $XEMU_VERSION_PATCH
#define XEMU_VERSION_COMMIT $XEMU_VERSION_COMMIT
#define XEMU_BRANCH        "$XEMU_BRANCH"
#define XEMU_COMMIT        "$XEMU_COMMIT"
#define XEMU_DATE          "$XEMU_DATE"
EOF
