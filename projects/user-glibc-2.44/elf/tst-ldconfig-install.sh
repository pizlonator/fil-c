#!/bin/sh
# Test that ldconfig --install installs a pre-built cache file.
# Copyright (C) 2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

set -e

common_objpfx=$1
test_wrapper_env=$2
run_program_env=$3

testroot="${common_objpfx}elf/tst-ldconfig-install-directory"
cleanup () {
    rm -rf "$testroot"
}
trap cleanup 0

rm -rf "$testroot"
mkdir -p "$testroot/etc"

ldconfig="${common_objpfx}elf/ldconfig"
run_ldconfig () {
    ${test_wrapper_env} ${run_program_env} "$ldconfig" "$@"
}

errors=0
fail () {
    echo "error: $1"
    errors=1
}

# Build a pre-built cache to install.
source="$testroot/prebuilt-ld.so.cache"
mkdir -p "$testroot/lib"
run_ldconfig -X -f /dev/null -C "$source" "$testroot/lib"
test -r "$source" || fail "ldconfig did not create the pre-built cache"

# Pad the source past the 512-byte internal copy buffer so that the multi-block
# copy path is exercised, while leaving the cache magic at the start intact.
dd if=/dev/zero bs=1024 count=4 >> "$source" 2>/dev/null

dest="$testroot/etc/ld.so.cache"
temp="$dest~"

run_ldconfig --install -f /dev/null -C "$dest" "$source"

# The destination must exist and be byte-identical to the source.
if test -r "$dest"; then
  if cmp -s "$source" "$dest"; then
    echo "info: installed cache matches the source"
  else
    fail "installed cache differs from the source"
  fi
else
  fail "destination cache file was not created"
fi

# The temporary file used during the atomic rename must not be left behind.
if test -e "$temp"; then
  fail "temporary file $temp was left behind"
fi

# The installed cache must be world-readable (0644).
if test -r "$dest"; then
  mode=$(ls -l "$dest" | cut -c1-10)
  case "$mode" in
    (-rw-r--r--) echo "info: installed cache has expected permissions" ;;
    (*) fail "installed cache has unexpected permissions: $mode" ;;
  esac
fi

# A second install over an existing cache must also succeed.
run_ldconfig --install -f /dev/null -C "$dest" "$source"
if cmp -s "$source" "$dest"; then
  echo "info: re-install over an existing cache works"
else
  fail "re-install produced a different cache"
fi

# Error case: a source file that is not a cache must be rejected, and no
# destination must be produced.
rm -f "$dest"
notcache="$testroot/not-a-cache"
echo "this is not an ld.so.cache file" > "$notcache"
if run_ldconfig --install -f /dev/null -C "$dest" "$notcache" 2>"$testroot/err"; then
  fail "ldconfig accepted a file that is not a cache"
else
  if grep -q "does not look like an ld.so.cache file" "$testroot/err"; then
    echo "info: non-cache source correctly rejected"
  else
    fail "unexpected error message for non-cache source"
    cat "$testroot/err"
  fi
fi
test -e "$dest" && fail "destination created from an invalid source"

# Error case: a missing source argument must be diagnosed.
if run_ldconfig --install -f /dev/null -C "$dest" 2>"$testroot/err"; then
  fail "ldconfig accepted --install without a source file"
else
  grep -q "Missing source file name" "$testroot/err" \
    || fail "unexpected error message for missing source"
fi

# Error case: a nonexistent source must be diagnosed.
if run_ldconfig --install -f /dev/null -C "$dest" \
       "$testroot/does-not-exist" 2>"$testroot/err"; then
  fail "ldconfig accepted a nonexistent source file"
fi

exit $errors
