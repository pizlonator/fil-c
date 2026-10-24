#!/bin/sh
# Builds the suite with Fil-C: <repo>/build/bin/filcc, which pizlonates the
# code and links this repo's pizfix/ OpenSSL 3.  Override the compiler with
# FILCC=/path/to/filcc.  Extra args are passed to make (e.g. "clean test").
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)
FILCC=${FILCC:-$R/build/bin/filcc}
if [ ! -x "$FILCC" ]; then
    echo "Fil-C compiler not found at $FILCC (set FILCC=...)" >&2
    exit 1
fi
exec make CC="$FILCC" "$@"
