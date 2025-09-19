#!/bin/bash

set -ex

export PATH="/c/msys64/$MSYSTEM/bin:$PATH"

pacman --noconfirm -Suy

pacman --noconfirm -S --needed \
    base-devel \
    lcov \
    "${MINGW_PACKAGE_PREFIX}"-ccache \
    "${MINGW_PACKAGE_PREFIX}"-gettext \
    "${MINGW_PACKAGE_PREFIX}"-gi-docgen \
    "${MINGW_PACKAGE_PREFIX}"-gobject-introspection \
    "${MINGW_PACKAGE_PREFIX}"-libffi \
    "${MINGW_PACKAGE_PREFIX}"-meson \
    "${MINGW_PACKAGE_PREFIX}"-pcre2 \
    "${MINGW_PACKAGE_PREFIX}"-python3 \
    "${MINGW_PACKAGE_PREFIX}"-python-docutils \
    "${MINGW_PACKAGE_PREFIX}"-python-pip \
    "${MINGW_PACKAGE_PREFIX}"-toolchain \
    "${MINGW_PACKAGE_PREFIX}"-zlib \
    "${MINGW_PACKAGE_PREFIX}"-libelf

mkdir -p _coverage
mkdir -p _ccache
CCACHE_BASEDIR="$(pwd)"
CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"
export CCACHE_BASEDIR CCACHE_DIR

pip3 install --upgrade --user packaging==23.2

PATH="$(cygpath "$USERPROFILE")/.local/bin:$HOME/.local/bin:$PATH"
DIR="$(pwd)"
export PATH CFLAGS

# FIXME: We can’t use ${MESON_COMMON_OPTIONS} here because this script installs
# Meson 1.3. See the comment in .gitlab-ci.yml about the same problem on
# FreeBSD.
# shellcheck disable=SC2086
meson setup \
    --buildtype=debug \
    --wrap-mode=nodownload \
    --werror \
    -Ddocumentation=true \
    -Dintrospection=enabled \
    -Dman-pages=enabled \
    _build

meson compile -C _build

if [[ "$CFLAGS" == *"-coverage"* ]]; then
    lcov \
        --quiet \
        --config-file "${DIR}"/.lcovrc \
        --directory "${DIR}/_build" \
        --capture \
        --initial \
        --output-file "${DIR}/_coverage/${CI_JOB_NAME}-baseline.lcov"
fi

meson test -C _build -v --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}"
meson test -C _build -v --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
    --setup=unstable_tests --suite=failing --suite=flaky || true

if [[ "$CFLAGS" == *"-coverage"* ]]; then
    lcov \
        --quiet \
        --config-file "${DIR}"/.lcovrc \
        --directory "${DIR}/_build" \
        --capture \
        --output-file "${DIR}/_coverage/${CI_JOB_NAME}.lcov"
fi

# Copy the built documentation to an artifact directory. The build for docs.gtk.org
# can then pull it from there — see https://gitlab.gnome.org/GNOME/gtk/-/blob/docs-gtk-org/README.md
mkdir -p _reference/
mv _build/docs/reference/glib/glib-win32-2.0/ _reference/glib-win32/
mv _build/docs/reference/gio/gio-win32-2.0/ _reference/gio-win32/