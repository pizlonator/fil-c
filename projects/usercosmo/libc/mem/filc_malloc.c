/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: the malloc family.                          │
│                                                                              │
│ cosmo's allocator is dlmalloc with per-chunk headers, tmspace heap pinning   │
│ and rseq critical sections — none of which survive Fil-C capabilities        │
│ (chunk headers live *before* the user allocation, and every malloc'd block   │
│ has exact-bounds bounds).                                                    │
│                                                                              │
│ Following usermusl, the whole family forwards to libpizlo's GC allocator:    │
│ zgc_alloc() is zero-initialized, free() is a checked no-op (the GC reclaims  │
│ memory), and realloc() is zgc_realloc().  This also means that pointers in   │
│ malloc'd memory are fully GC-visible, which dlmalloc memory could never be.  │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/errno.h"
#include "libc/limits.h"
#include "libc/mem/mem.h"
#include <stdfil.h>
#include <pizlonated_runtime.h>

void *malloc(size_t size) {
  return zgc_alloc(size);
}

void *calloc(size_t m, size_t n) {
  if (n && m > (__SIZE_TYPE__)-1 / n) {
    errno = ENOMEM;
    return 0;
  }
  /* zgc_alloc zeroes memory already. */
  return zgc_alloc(n * m);
}

void *realloc(void *p, size_t n) {
  return zgc_realloc(p, n);
}

void free(void *p) {
  zgc_free(p);
}

size_t malloc_usable_size(void *p) {
  /* Fil-C has no direct "size of allocation" query.  Derive it from the
     object bounds; pointers that don't point into a GC object (or point
     past the object start) report zero, matching the expectations of
     callers that use this as a "how much can I still use" query. */
  if (!p || !zhasvalidcap(p) || !zinbounds(p))
    return 0;
  /* From an interior pointer, the usable size is what remains. */
  return (size_t)((char *)zgetupper(p) - (char *)p);
}

int posix_memalign(void **res, size_t align, size_t len) {
  if (align < sizeof(void *) || (align & (align - 1)))
    return EINVAL;
  void *mem = zgc_aligned_alloc(align, len);
  if (!mem)
    return ENOMEM;
  *res = mem;
  return 0;
}

void *memalign(size_t align, size_t len) {
  if (align < sizeof(void *) || (align & (align - 1))) {
    errno = EINVAL;
    return 0;
  }
  return zgc_aligned_alloc(align, len);
}

void *aligned_alloc(size_t align, size_t len) {
  if (align < sizeof(void *) || (align & (align - 1))) {
    errno = EINVAL;
    return 0;
  }
  return zgc_aligned_alloc(align, len);
}

void *valloc(size_t len) {
  return zgc_aligned_alloc(4096, len);
}

void *pvalloc(size_t len) {
  return zgc_aligned_alloc(4096, len);
}

void *reallocarray(void *p, size_t m, size_t n) {
  if (n && m > (__SIZE_TYPE__)-1 / n) {
    errno = ENOMEM;
    return 0;
  }
  return zgc_realloc(p, n * m);
}

void *realloc_in_place(void *p, size_t n) {
  /* The GC allocator cannot grow in place in general; emulate with the
     preserving-alignment realloc, which is a no-op when the size fits. */
  return zgc_realloc_preserving_alignment(p, n);
}

/* Fil-C port: cosmo's mallinfo()/mallopt()/malloc_trim() are dlmalloc
   internals (their files are excluded from this build, so the symbols were
   dangling and any program calling them failed to link).  dlmalloc isn't
   the allocator here — libpizlo's GC allocator is — so provide the same
   no-op surface musl gives programs that aren't using its allocator: an
   all-zero mallinfo(), an always-successful no-op mallopt(), and a
   malloc_trim() that never trims anything. */
struct mallinfo mallinfo(void) {
  struct mallinfo info;
  __builtin_memset(&info, 0, sizeof(info));
  return info;
}

int mallopt(int param, int value) {
  /* No tunable parameters in the GC allocator; accept and ignore. */
  (void)param;
  (void)value;
  return 1;
}

int malloc_trim(size_t pad) {
  /* The GC heap is mmap'd by libpizlo and cannot be returned incrementally. */
  (void)pad;
  return 0;
}
