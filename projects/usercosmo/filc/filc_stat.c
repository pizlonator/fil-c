/* Pizlonated sys_fstat / sys_fstatat for the usercosmo flavor.
 *
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Cosmo's own sys_fstat/sys_fstatat (libc/calls/fstat{,at}-sysv.c) funnel
 * through __sys_fstat{,at} assuming the kernel's raw struct stat layout,
 * and then run __stat2cosmo() to convert into the cosmo ABI.  In the Fil-C
 * cosmo flavor that assumption is false on aarch64: __sys_fstatat is not a
 * raw syscall thunk but a gen_shims shim forwarding to libpizlo's
 * zsys_fstatat, whose runtime (filc_native_zsys_fstatat) calls cosmo's own
 * yolo fstatat() -- which already delivers a fully converted cosmo-ABI
 * struct stat.  (libpizlo's plain fstatat reference resolves to the yolo
 * libc below it; pizlonated code can't provide it.)  Running __stat2cosmo
 * on top of that re-converts already-converted data: on aarch64, where the
 * kernel stat layout differs from the cosmo ABI, st_mode/st_nlink/st_uid/
 * st_gid come out permuted (e.g. stat("/lib/terminfo") reported st_mode ==
 * the link count), which broke every S_ISDIR/S_ISREG user (ncurses'
 * terminfo directory scan, etc.).  On x86_64 the kernel and cosmo layouts
 * coincide, so the double conversion was always an identity there.
 *
 * These pass-through definitions skip the bogus re-conversion: the shim
 * already hands us cosmo-ABI bytes.  SUPPORT_VECTOR=1 makes this build
 * Linux-only, which is the only OS the metastat dance supported anyway. */

#include "libc/calls/struct/stat.internal.h"
#include "libc/calls/syscall-sysv.internal.h"

int sys_fstat(int fd, struct stat *st) {
  /* Pass st through untouched: a null st must reach the __sys_ shim so the
   * runtime panics with "cannot write pointer with null object", exactly like
   * the musl flavor (filc/tests/statfail1 and fstatfail1 require this). */
  return __sys_fstat(fd, st);
}

int sys_fstatat(int dirfd, const char *path, struct stat *st, int flags) {
  return __sys_fstatat(dirfd, path, st, flags);
}
