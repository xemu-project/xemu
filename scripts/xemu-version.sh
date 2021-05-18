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
    git describe --match 'xemu-v*' | cut -c 7- | tr -d '\n'; \
  elif test -e XEMU_VERSION; then \
    cat XEMU_VERSION; \
  fi)

cat <<EOF
const char *xemu_version = "$XEMU_VERSION";
const char *xemu_branch  = "$XEMU_BRANCH";
const char *xemu_commit  = "$XEMU_COMMIT";
const char *xemu_date    = "$XEMU_DATE";
EOF
