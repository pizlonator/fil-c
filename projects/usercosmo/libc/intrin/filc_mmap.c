/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: memory management entry points.             │
│                                                                              │
│ Cosmo's public mmap()/munmap()/mprotect()/... wrappers do their own          │
│ bookkeeping in the lockless __maps radix/RB-tree (libc/intrin/maps.c).       │
│ That machinery packs tag bits into struct Map pointers (the ABA() macro),    │
│ which is fundamentally incompatible with Fil-C capabilities, so the          │
│ originals are excluded from this build.                                      │
│                                                                              │
│ Instead the public APIs here forward straight to libpizlo's zsys_* API.      │
│ The zsys_* calls land in the YOLO cosmo libc below libpizlo, whose own       │
│ __maps machinery (built with GCC, fully functional) tracks the mappings.     │
│ Pizlonated code just needs the calls to work, which they do.                 │
│                                                                              │
│ A zeroed pizlonated copy of `struct Maps __maps` plus no-op lock/track       │
│ stubs are provided because various other translation units reference them    │
│ for bookkeeping that has no meaning on this side of the boundary.            │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/calls.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/maps.h"
#include "libc/limits.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/mremap.h"
#include "libc/sysv/errfuns.h"
#include <stdfil.h>
#include <pizlonated_syscalls.h>
#include <pizlonated_runtime.h>
#include <stdarg.h>
#include <pizlonated_runtime.h>

/* The pizlonated copy of cosmo's memory map: intentionally zeroed.  Nothing
   on the Fil-C side populates or consumes it (kisdangerous() short-circuits
   to false under Fil-C; see libc/intrin/kisdangerous.c). */
struct Maps __maps;

/* No-op stubs for the map bookkeeping entry points that other translation
   units call; tracking kernel mappings on the Fil-C side has no consumer. */
void __maps_init(void) {
}

void __maps_lock(void) {
}

void __maps_unlock(void) {
}

bool __maps_track(char *addr, size_t size, int prot, int flags) {
  (void)addr;
  (void)size;
  (void)prot;
  (void)flags;
  return true;
}

int __maps_untrack(char *addr, size_t size) {
  (void)addr;
  (void)size;
  return true;
}

void __maps_wipe(void) {
}

/**
 * Allocates memory that is never freed.
 *
 * cosmo's implementation carves this out of the __maps pool; under Fil-C we
 * use GC memory, which is fine because permalloc'd allocations are never
 * released by definition.
 */
void *cosmo_permalloc(size_t size) {
  return zgc_alloc(size);
}

/**
 * Maps memory. Thin pizlonated forward to zsys_mmap().
 *
 * @return base address of mapping, or MAP_FAILED w/ errno
 */
void *mmap(void *addr, size_t len, int prot, int flags, int fd,
           int64_t offset) {
  void *res;
  if (!len) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  res = zsys_mmap(addr, len, prot, flags, fd, offset);
  if (res == MAP_FAILED)
    return MAP_FAILED;
  return res;
}

__weak_reference(mmap, mmap64);

/**
 * Unmaps memory.
 */
int munmap(void *addr, size_t length) {
  return zsys_munmap(addr, length);
}

/**
 * Changes memory protections.
 */
int mprotect(void *addr, size_t len, int prot) {
  return zsys_mprotect(addr, len, prot);
}

/**
 * Synchronizes memory with the backing file.
 */
int msync(void *addr, size_t length, int flags) {
  return zsys_msync(addr, length, flags);
}

/**
 * Advises the kernel about memory usage.
 */
int madvise(void *addr, size_t length, int advice) {
  return zsys_madvise(addr, length, advice);
}

/**
 * Determines which pages are resident.
 */
int mincore(void *addr, size_t length, unsigned char *vec) {
  return zsys_mincore(addr, length, vec);
}

/**
 * Re-allocates / grows a mapping (dlmalloc uses this).
 */
void *cosmo_mremap(void *old, size_t oldn, size_t newn, int flags, ...) {
  va_list va;
  void *new;
  va_start(va, flags);
  new = va_arg(va, void *);
  va_end(va);
  return zsys_mremap(old, oldn, newn, flags, new);
}

/* dlmalloc marks mappings it hands out as munlockable in the __maps tree;
   that bookkeeping has no consumer on the Fil-C side. */
void __maps_mark(void *addr, size_t size) {
  (void)addr;
  (void)size;
}

void __maps_unmark(void *addr, size_t size) {
  (void)addr;
  (void)size;
}
