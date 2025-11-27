#!/bin/bash

set -e

pacman --noconfirm -Suy

pacman --noconfirm -S --needed \
    git \
    base-devel \
    ${MINGW_PACKAGE_PREFIX}-toolchain \
    ${MINGW_PACKAGE_PREFIX}-ccache \
    ${MINGW_PACKAGE_PREFIX}-meson \
    ${MINGW_PACKAGE_PREFIX}-python3 \
    ${MINGW_PACKAGE_PREFIX}-python3-pip \
    ${MINGW_PACKAGE_PREFIX}-python3-packaging \
    ${MINGW_PACKAGE_PREFIX}-python3-mako \
    ${MINGW_PACKAGE_PREFIX}-python3-markdown \
    ${MINGW_PACKAGE_PREFIX}-libffi \
    ${MINGW_PACKAGE_PREFIX}-pkgconf \
    ${MINGW_PACKAGE_PREFIX}-cairo \
    ${MINGW_PACKAGE_PREFIX}-pcre2 \
    ${MINGW_PACKAGE_PREFIX}-zlib \
    ${MINGW_PACKAGE_PREFIX}-gettext

export CCACHE_BASEDIR="${CI_PROJECT_DIR}"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

pip3 install --upgrade --user meson==1.2.3 flake8 mypy==0.931 types-Markdown
export PATH="$HOME/.local/bin:$PATH"

meson setup \
        -Dwerror=true \
        -Dglib:werror=false \
        -Dcairo=enabled \
        -Ddoctool=enabled \
        --buildtype debug \
        _build

.gitlab-ci/show-git-commits.sh || true

meson compile -C _build

meson test \
        --print-errorlogs \
        --suite=gobject-introspection \
        --no-suite=glib \
        -C _build

python3 -m flake8 --count
python3 -m mypy _build
