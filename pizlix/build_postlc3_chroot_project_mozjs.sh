#!/bin/bash

set -e
set -x

# mozjs 115.10.0 (SpiderMonkey, the JavaScript engine used by gjs 1.80).
#
# This is the Fil-C port. The pizlonated tarball is packaged from
# projects/mozjs.projeny, which carries the Fil-C fixes:
#
# - js/public/Value.h: JS::Value keeps the punbox64 bit encoding but gains a
#   second "capability_" pointer member so cell payloads carry Fil-C
#   capabilities.
# - js/public/Id.h: PropertyKey (jsid) stores a real pointer so string and
#   symbol keys keep their capabilities; integer keys are pointer-shaped
#   values that are never dereferenced.
# - js/src/gc/*: capability-preserving pointer math (zmkptr) for chunk/arena
#   bases, the free-list allocator, mark stack tagged pointers, header-word
#   pointers, and the store buffer.
# - The nursery and compacting GC are disabled (cells must never move).
# - The rust glue (jsrust: encoding_c + wast) is not built; the shell's
#   wasm_text_to_binary is stubbed.
rm -rf pizlonated-mozjs
cd /sources
# mozjs's configure requires llvm-objdump (for symbol dumping); Fil-C's
# toolchain does not ship one. GNU objdump is compatible enough for the
# standalone js build (which does not invoke it). Put the symlink in /usr/bin:
# build_chroot_late.sh runs with PATH=/usr/bin:/usr/sbin (no /usr/local/bin).
if ! test -x /usr/bin/llvm-objdump; then
  ln -s /usr/bin/objdump /usr/bin/llvm-objdump
fi
if ! test -x /usr/local/bin/llvm-objdump; then
  ln -s /usr/bin/objdump /usr/local/bin/llvm-objdump
fi
tar -xf pizlonated-mozjs.tar.gz
cd pizlonated-mozjs
mkdir -p obj-filc
cd obj-filc
../js/src/configure            \
      --prefix=/usr            \
      --disable-jit            \
      --with-system-icu        \
      --disable-debug          \
      --enable-optimize        \
      --enable-shared-js       \
      --disable-tests
make -j32
make install
cd ../..
rm -rf pizlonated-mozjs
hash -r
