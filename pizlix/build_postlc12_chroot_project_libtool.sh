#!/bin/bash

set -e
set -x

# libtool 2.4.7 rebuild -- the libltdl Fil-C fix.  The part2 build of
# libtool produced a /usr/lib/libltdl.so whose only undefined symbol is
# `pizlonated_lt_libltdl_LTX_preloaded_symbols` (ltdl.c references the
# never-defined, ltmain-convenience-only data symbol from lt_dlinit's
# HAVE_LIBDLLOADER block, and the fil-C compiler routes the reference
# through a pizlonated_ forwarder).  A NON-WEAK undefined reference in a
# shared library poisons every executable link against it
# ("undefined reference to pizlonated_lt_libltdl_LTX_preloaded_symbols")
# and would poison every runtime load.  Nothing in the base system links
# libltdl, so the breakage hid until postlc12's libcanberra -- whose
# configure probes libltdl with an AC_CHECK_LIB link test -- became the
# first consumer.
#
# This is a same-version drop-in rebuild of part2's libtool block with
# pizlix/libtool-2.4.7-filc-ltdl-1.patch applied (the patch declares the
# reference weak; lt_dlpreload(NULL) -- which is what the standalone
# library then passes -- is the documented "clear the preloaded list"
# path, so the standalone library's behavior is unchanged).  It lives in
# postlc12 (not part2) because the recovery chain
# (build_recover_postlc9 -> postlc10 -> postlc11 -> postlc12) starts from
# a snapshot whose libltdl predates the fix, and postlc12 is the stage
# that first NEEDS libltdl; a from-scratch chain reaches postlc12 after
# part2, so it gets the fixed library either way.

cd /sources
rm -rf libtool-2.4.7
tar -xf libtool-2.4.7.tar.xz
cd libtool-2.4.7
patch -Np1 -i ../libtool-2.4.7-filc-ltdl-1.patch
# M4 is preset so configure's AC_PROG_M4-style search runs at all: the
# search loop probes each candidate m4 with `m4 -F conftest.m4f` and a
# --trace snippet and REQUIRES both to produce empty output.  Under fil-C
# that probe intermittently captures a foreign blob of xtrace-looking
# bash output ("++ /bin/universe", "./configure: line 2284: /bin/universe:
# No such file or directory") on m4's stdout -- observed 5 runs in a row
# during Sprint D, then 45 consecutive clean runs with no repro and no
# writer visible under strace (zero write() syscalls in the trace, and
# the string exists nowhere in the chroot's binaries).  With M4 preset,
# configure takes it verbatim and skips both the search and the
# never-mattered tracing check (m4's tracing is a libtoolize-time
# feature; nothing in this build runs autoreconf).
M4=/usr/bin/m4 \
./configure --prefix=/usr
make
make install
rm -fv /usr/lib/libltdl.a
cd ..
rm -rf libtool-2.4.7
hash -r

# --- gates ---------------------------------------------------------------------

echo "=== gate: the library + headers are installed ==="
test -e /usr/lib/libltdl.so
test -e /usr/lib/libltdl.so.7
test -e /usr/include/ltdl.h

echo "=== gate: the pizlonated preloaded-symbols reference is now WEAK ==="
if nm -D --undefined-only /usr/lib/libltdl.so.7.3.2 | grep -q " B pizlonated_lt_libltdl_LTX_preloaded_symbols\| U pizlonated_lt_libltdl_LTX_preloaded_symbols"; then
    echo "FATAL: the preloaded-symbols reference is still a hard undefined symbol:" >&2
    nm -D --undefined-only /usr/lib/libltdl.so.7.3.2 | grep preloaded >&2
    exit 1
fi
nm -D --undefined-only /usr/lib/libltdl.so.7.3.2 | grep preloaded

echo "=== gate: an executable links against -lltdl ==="
echo "int main (void) { return 0; }" > /tmp/ltdl-link-test.c
gcc /tmp/ltdl-link-test.c -lltdl -o /tmp/ltdl-link-test
/tmp/ltdl-link-test && echo "link+run: OK"
rm -f /tmp/ltdl-link-test.c /tmp/ltdl-link-test

echo "=== gate: ldd clean ==="
if ldd /usr/lib/libltdl.so 2>&1 | grep -q 'not found'; then
    echo "FATAL: libltdl has unresolved shared libraries:" >&2
    ldd /usr/lib/libltdl.so 2>&1 | grep 'not found' >&2
    exit 1
fi
echo "ldd: clean"

echo "postlc12 libtool (libltdl fix): ALL GATES GREEN"
