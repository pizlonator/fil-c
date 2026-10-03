/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2024 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/intrin/atomic.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/maps.h"
#include "libc/runtime/runtime.h"

/**
 * Returns true if memory address doesn't officially exist.
 *
 * This function is lockless. It uses a five layer atomic radix trie for
 * validating memory addresses. Ideally we would find a way to crawl the
 * rbtree without locking. Not locking is important, since we don't want
 * to affect the scalability of functions like read() and write(), which
 * consult this function in order to raise EFAULT.
 *
 * This function supports address spaces up to 56 bits. The highest byte
 * in `addr` is ignored. Your page size much be at least 4096 bytes, and
 * rounded to a two power.
 *
 * @see https://en.wikipedia.org/wiki/Intel_5-level_paging
 */
__privileged bool32 kisdangerous(const void *addr) {
#ifdef __FILC__
  /* Fil-C port: this walks the yolo cosmo boot's radix trie of kernel
     mappings (__maps.alive).  Pizlonated code has its own (never-populated)
     copy of the __maps globals, and populating it would require tracking
     every zsys_mmap() on this side of the boundary.  The check is a pure
     optimization (bad pointers just become kernel EFAULTs), so report
     "everything exists" in Fil-C land and don't even reference the __maps
     structures.  The yolo side keeps its own working kisdangerous(). */
  (void)addr;
  return false;
#else
  uintptr_t w = (uintptr_t)addr & -__pagesize;
  struct MapPageDirectory *pd;
  struct MapPageTable *pt;
  if (!(pd = atomic_load_explicit(&__maps.alive, memory_order_relaxed))
    return true;
  size_t i = (w >> 48) & 511;
  if (!(pd = atomic_load_explicit(&pd->p[i].pd, memory_order_relaxed))
    return true;
  i = (w >> 39) & 511;
  if (!(pd = atomic_load_explicit(&pd->p[i].pd, memory_order_relaxed))
    return true;
  i = (w >> 30) & 511;
  if (!(pd = atomic_load_explicit(&pd->p[i].pd, memory_order_relaxed))
    return true;
  i = (w >> 21) & 511;
  if (!(pt = atomic_load_explicit(&pd->p[i].pt, memory_order_relaxed))
    return true;
  i = (w >> 12) & 511;
  return !(atomic_load_explicit(&pt->p[i / 64], memory_order_acquire) &
           (1ull << (i & 63));
#endif /* !__FILC__ */
}
