#!/bin/sh
set -eu

mkdir -vp "$(dirname "${DESTDIR:-}$2")"
printf '.so %s\n' "$1" > "${DESTDIR:-}$2"
