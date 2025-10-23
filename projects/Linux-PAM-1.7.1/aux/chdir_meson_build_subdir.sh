#!/bin/sh -efu

exe=$1; shift
exe=$(readlink -ev -- "$exe")
cd "$MESON_BUILD_SUBDIR"
exec "$exe" "$@"
