#!/bin/bash

set -e

if [[ "$MSYSTEM" == "MINGW32" ]]; then
    export MSYS2_ARCH="i686"
else
    export MSYS2_ARCH="x86_64"
fi

# Update everything
pacman --noconfirm -Suy

# Install the required packages
pacman --noconfirm -S --needed \
    base-devel \
    git \
    mingw-w64-$MSYS2_ARCH-cc \
    mingw-w64-$MSYS2_ARCH-ccache \
    mingw-w64-$MSYS2_ARCH-pkgconf \
    mingw-w64-$MSYS2_ARCH-gobject-introspection \
    mingw-w64-$MSYS2_ARCH-meson \
    mingw-w64-$MSYS2_ARCH-adwaita-icon-theme \
    mingw-w64-$MSYS2_ARCH-atk \
    mingw-w64-$MSYS2_ARCH-cairo \
    mingw-w64-$MSYS2_ARCH-gdk-pixbuf2 \
    mingw-w64-$MSYS2_ARCH-glib2 \
    mingw-w64-$MSYS2_ARCH-graphene \
    mingw-w64-$MSYS2_ARCH-json-glib \
    mingw-w64-$MSYS2_ARCH-libepoxy \
    mingw-w64-$MSYS2_ARCH-pango \
    mingw-w64-$MSYS2_ARCH-fribidi \
    mingw-w64-$MSYS2_ARCH-gst-plugins-bad-libs \
    mingw-w64-$MSYS2_ARCH-shared-mime-info \
    mingw-w64-$MSYS2_ARCH-python-gobject \
    mingw-w64-$MSYS2_ARCH-shaderc \
    mingw-w64-$MSYS2_ARCH-vulkan \
    mingw-w64-$MSYS2_ARCH-vulkan-headers

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

# Build
ccache --zero-stats
ccache --show-stats
export CCACHE_DISABLE=true
meson setup \
    -Dx11-backend=false \
    -Dwayland-backend=false \
    -Dwin32-backend=true \
    -Dintrospection=enabled \
    -Dgtk:werror=true \
    _build
unset CCACHE_DISABLE

ninja -C _build
ccache --show-stats

tar zcf _build/gtkdll.tar.gz _build/gtk/libgtk*.dll
