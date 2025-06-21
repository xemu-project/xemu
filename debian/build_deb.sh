#!/usr/bin/env bash
# Run from xemu root
set -e
set -x

BUILD_DIR=/tmp/xemu-deb-build
XEMU_VERSION_TAG=0.0.0
PKG_NAME=xemu_${XEMU_VERSION_TAG}

# Remove previous build artifacts, create build dir
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# Create source tarball
tar -czf $BUILD_DIR/${PKG_NAME}.orig.tar.gz --transform "s#^#${PKG_NAME}/#" .
pushd $BUILD_DIR

# Build .deb
tar xf ${PKG_NAME}.orig.tar.gz
cd ${PKG_NAME}

# Create a changelog with current version
echo -e "xemu (1:${XEMU_VERSION_TAG}-0) unstable; urgency=medium\n" > debian/changelog
echo -e "  Built from $(git describe --match 'xemu-v*')\n" >> debian/changelog
echo " -- Matt Borgerson <contact@mborgerson.com>  $(date -R)" >> debian/changelog

# Place specific repo version in control file
echo -e " .\n This package was built from $(git describe --match 'xemu-v*') on $(date -R)\n" >> debian/control

debuild --preserve-env -us -uc
