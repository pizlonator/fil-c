/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: stragglers.                                 │
│                                                                              │
│  - bsearch(): cosmo gets it from third_party/musl which is not part of       │
│    this build.                                                               │
│  - llrintl(): cosmo only defines it as a weak alias of lrint when long       │
│    double is 64-bit; on x86_64 nothing provides it, but libpizlo and user    │
│    code want it.                                                             │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/stdlib.h"
#include "libc/math.h"
#include "libc/calls/calls.h"
#include "libc/runtime/zipos.internal.h"
#include "libc/str/str.h"
#include "libc/mem/mem.h"
#include <pizlonated_syscalls.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "libc/errno.h"

void *bsearch(const void *key, const void *base, size_t nel, size_t width,
              int (*cmp)(const void *, const void *)) {
  size_t lo = 0, hi = nel;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    const char *p = (const char *)base + mid * width;
    int c = cmp(key, p);
    if (c < 0) {
      hi = mid;
    } else if (c > 0) {
      lo = mid + 1;
    } else {
      return (void *)p;
    }
  }
  return 0;
}

long long llrintl(long double x) {
  return lrintl(x);
}

/* fork() is provided by cosmo's libc/proc/fork.c, which is now compiled into
   this libc: it runs the pthread_atfork prepare/parent/child handlers, locks
   down all of the libc-internal locks (stdio, maps, fds, pthreads, cxa, ...)
   while forking, and fixes up the child's TIB/thread-list state.  Its actual
   process duplication goes through sys_fork() -> __sys_fork() ->
   zsys_fork_impl(), i.e. libpizlo's GC-suspending fork.  (This file used to
   provide a bare `return zsys_fork()` fork(), which silently skipped every
   atfork handler and all of cosmo's fork-time lock quiescing.) */

/**
 * qsort()/qsort_r(): cosmo's introsort (libc/str/qsort.c) moves elements
 * through an unchecked element-sized pointer walk that the filc runtime
 * rejects on ordinary inputs (the compare callback is invoked with pointers
 * outside the array bounds mid-partition).  This is the textbook
 * insertion-sort-seeded quicksort; it is not the fastest sort in the world,
 * but it is honest about its pointers.
 */
static void filc_qsort_swap(char *a, char *b, size_t width) {
  /* NOTE: this has to go through memcpy(): Fil-C migrates the pointer
     metadata of a copied range only through real memcpy(), so swapping
     element bytes through a char loop corrupts any pointer-typed fields
     (the runtime later rejects the moved pointers). */
  char tmp[64];
  char *heap = 0;
  if (width > sizeof(tmp)) {
    heap = (char *)malloc(width);
    if (!heap)
      return;
    memcpy(heap, a, width);
  } else {
    memcpy(tmp, a, width);
  }
  memcpy(a, b, width);
  memcpy(b, heap ? heap : tmp, width);
  free(heap);
}

static void filc_qsort_impl(char *base, size_t nmemb, size_t width,
                            int (*cmp)(const void *, const void *, void *),
                            void *arg) {
  while (nmemb > 8) {
    /* median-of-three pivot into base[0] */
    char *mid = base + (nmemb / 2) * width;
    char *last = base + (nmemb - 1) * width;
    if (cmp(mid, base, arg) < 0)
      filc_qsort_swap(mid, base, width);
    if (cmp(last, base, arg) < 0)
      filc_qsort_swap(last, base, width);
    if (cmp(mid, last, arg) > 0)
      filc_qsort_swap(mid, last, width);
    /* partition */
    size_t left = 0, right = nmemb - 1;
    for (;;) {
      while (left < right && cmp(base + (++left) * width, base, arg) < 0) {
      }
      while (right && cmp(base + (--right) * width, base, arg) > 0) {
      }
      if (left >= right)
        break;
      filc_qsort_swap(base + left * width, base + right * width, width);
    }
    filc_qsort_swap(base, base + right * width, width);
    /* recurse into the smaller side, iterate on the larger one */
    if (right < nmemb - right - 1) {
      filc_qsort_impl(base, right, width, cmp, arg);
      base += (right + 1) * width;
      nmemb -= right + 1;
    } else {
      filc_qsort_impl(base + (right + 1) * width, nmemb - right - 1, width,
                      cmp, arg);
      nmemb = right;
    }
  }
  /* insertion sort for the rest; comparator arguments are always genuine
     element pointers (never a temp copy), and moves go through memcpy() */
  for (size_t i = 1; i < nmemb; i++) {
    size_t j = i;
    while (j && cmp(base + (j - 1) * width, base + j * width, arg) > 0) {
      filc_qsort_swap(base + (j - 1) * width, base + j * width, width);
      j--;
    }
  }
}

void qsort(void *base, size_t nmemb, size_t width,
           int (*compar)(const void *, const void *)) {
  if (nmemb > 1)
    filc_qsort_impl(base, nmemb, width,
                    (int (*)(const void *, const void *, void *))compar, NULL);
}

void qsort_r(void *base, size_t nmemb, size_t width,
             int (*compar)(const void *, const void *, void *), void *arg) {
  if (nmemb > 1)
    filc_qsort_impl(base, nmemb, width, compar, arg);
}

/**
 * vfork() can not be supported under Fil-C, period.  vfork's whole contract
 * is that the child runs on the PARENT's address space and stack (including
 * the parent's Fil-C thread state, GC view of memory, and pointer
 * capabilities) with only execve/_exit allowed; a pizlonated child doing
 * that would corrupt the runtime.  cosmo's vfork is also raw .s asm
 * (libc/proc/vfork.S), which can not be pizlonated.  We still have to provide
 * the symbol, because libc/proc/posix_spawn.c (and the log/ crash handlers)
 * reference vfork() and would otherwise fail to link; a call traps with a
 * clear safety error.  (cosmo's posix_spawn() itself only reaches vfork()
 * when POSIX_SPAWN_USEVFORK is requested; without the flag it uses the
 * sys_fork() path, which is fine under Fil-C.)
 */
int vfork(void) {
  zerrorf("vfork is not supported in Fil-C (the child cannot share the "
          "parent's address space, thread state, and capabilities); "
          "use fork() or posix_spawn() instead");
  return -1;
}

/**
 * Reads directory entries (libc/sysv has no .scall thunk for this; cosmo's
 * version is declared directly in libc/stdio/dirstream.c).
 */
int sys_getdents(unsigned fd, void *dirent, unsigned len, long *basep) {
  (void)basep;
  return zsys_getdents(fd, dirent, len);
}

/**
 * zipos stubs: dirstream.c references these for /zip/ directory iteration,
 * which is unreachable in a Fil-C cosmo program (no embedded zip, and the fd
 * table never tracks kFdZip).  The zipos machinery itself is excluded.
 */
struct Zipos;
struct ZiposUri;
uint64_t __zipos_inode(struct Zipos *zipos, int64_t cfile,
                       const void *path, size_t pathlen) {
  (void)zipos;
  (void)cfile;
  (void)path;
  (void)pathlen;
  return 0;
}

ssize_t __zipos_scan(struct Zipos *zipos, struct ZiposUri *prefix) {
  (void)zipos;
  (void)prefix;
  return 0;
}
