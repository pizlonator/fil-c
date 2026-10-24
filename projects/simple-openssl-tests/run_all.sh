#!/bin/sh
# Runs every built test program, passing through any --poison-* options.
# Usage: ./run_all.sh [--poison-...]
set -u
cd "$(dirname "$0")"
pass=0
fail=0
for exe in test_*; do
    case "$exe" in
    *.c|*.h|*.o|*.sh) continue ;;
    esac
    [ -x "$exe" ] || continue
    if "./$exe" "$@" >/tmp/simple-openssl-tests.log 2>&1; then
        pass=$((pass + 1))
        printf 'PASS %s\n' "$exe"
    else
        rc=$?
        fail=$((fail + 1))
        printf 'FAIL %s (exit %d)\n' "$exe" "$rc"
        sed 's/^/    /' /tmp/simple-openssl-tests.log
    fi
done
printf '%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
