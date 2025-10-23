#!/bin/sh -efu
# stdin stdout ...

exec < "$1"; shift
exec > "$1"; shift
exec "$@"
