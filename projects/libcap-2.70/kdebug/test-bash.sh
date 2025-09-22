#!/bin/sh
# bash is used in various headers so we need a wrapper to invoke sh
# instead.
exec sh "$@"
