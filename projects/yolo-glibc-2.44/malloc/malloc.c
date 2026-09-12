/* Malloc implementation for multiple threads without lock contention.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

/*
  This is a version (aka ptmalloc2) of malloc/free/realloc written by
  Doug Lea and adapted to multiple threads/arenas by Wolfram Gloger.

  There have been substantial changes made after the integration into
  glibc in all parts of the code.  Do not look for much commonality
  with the ptmalloc2 version.

* Version ptmalloc2-20011215
  based on:
  VERSION 2.7.0 Sun Mar 11 14:14:06 2001  Doug Lea  (dl at gee)

* Quickstart

  In order to compile this implementation, a Makefile is provided with
  the ptmalloc2 distribution, which has pre-defined targets for some
  popular systems (e.g. "make posix" for Posix threads).  All that is
  typically required with regard to compiler flags is the selection of
  the thread package via defining one out of USE_PTHREADS, USE_THR or
  USE_SPROC.  Check the thread-m.h file for what effects this has.
  Many/most systems will additionally require USE_TSD_DATA_HACK to be
  defined, so this is the default for "make posix".

* Why use this malloc?

  This is not the fastest, most space-conserving, most portable, or
  most tunable malloc ever written. However it is among the fastest
  while also being among the most space-conserving, portable and tunable.
  Consistent balance across these factors results in a good general-purpose
  allocator for malloc-intensive programs.

  The main properties of the algorithms are:
  * For large (>= 512 bytes) requests, it is a pure best-fit allocator,
    with ties normally decided via FIFO (i.e. least recently used).
  * For small (<= 64 bytes by default) requests, it is a caching
    allocator, that maintains pools of quickly recycled chunks.
  * In between, and for combinations of large and small requests, it does
    the best it can trying to meet both goals at once.
  * For very large requests (>= 128KB by default), it relies on system
    memory mapping facilities, if supported.

  For a longer but slightly out of date high-level description, see
     http://gee.cs.oswego.edu/dl/html/malloc.html

  You may already by default be using a C library containing a malloc
  that is  based on some version of this malloc (for example in
  linux). You might still want to use the one in this file in order to
  customize settings or to avoid overheads associated with library
  versions.

* Contents, described in more detail in "description of public routines" below.

  Standard (ANSI/SVID/...)  functions:
    malloc(size_t n);
    calloc(size_t n_elements, size_t element_size);
    free(void* p);
    realloc(void* p, size_t n);
    memalign(size_t alignment, size_t n);
    valloc(size_t n);
    mallinfo()
    mallopt(int parameter_number, int parameter_value)

  Additional functions:
    independent_calloc(size_t n_elements, size_t size, void* chunks[]);
    independent_comalloc(size_t n_elements, size_t sizes[], void* chunks[]);
    pvalloc(size_t n);
    malloc_trim(size_t pad);
    malloc_usable_size(void* p);
    malloc_stats();

* Vital statistics:

  Supported pointer representation:       4 or 8 bytes
  Supported size_t  representation:       4 or 8 bytes
       Note that size_t is allowed to be 4 bytes even if pointers are 8.
       You can adjust this by defining INTERNAL_SIZE_T

  Alignment:                              2 * sizeof(size_t) (default)
       (i.e., 8 byte alignment with 4byte size_t). This suffices for
       nearly all current machines and C compilers. However, you can
       define MALLOC_ALIGNMENT to be wider than this if necessary.

  Minimum overhead per allocated chunk:   4 or 8 bytes
       Each malloced chunk has a hidden word of overhead holding size
       and status information.

  Minimum allocated size: 4-byte ptrs:  16 bytes    (including 4 overhead)
			  8-byte ptrs:  24/32 bytes (including, 4/8 overhead)

       When a chunk is freed, 12 (for 4byte ptrs) or 20 (for 8 byte
       ptrs but 4 byte size) or 24 (for 8/8) additional bytes are
       needed; 4 (8) for a trailing size field and 8 (16) bytes for
       free list pointers. Thus, the minimum allocatable size is
       16/24/32 bytes.

       Even a request for zero bytes (i.e., malloc(0)) returns a
       pointer to something of the minimum allocatable size.

       The maximum overhead wastage (i.e., number of extra bytes
       allocated than were requested in malloc) is less than or equal
       to the minimum size, except for requests >= mmap_threshold that
       are serviced via mmap(), where the worst case wastage is 2 *
       sizeof(size_t) bytes plus the remainder from a system page (the
       minimal mmap unit); typically 4096 or 8192 bytes.

  Maximum allocated size:  4-byte size_t: 2^32 minus about two pages
			   8-byte size_t: 2^64 minus about two pages

       It is assumed that (possibly signed) size_t values suffice to
       represent chunk sizes. `Possibly signed' is due to the fact
       that `size_t' may be defined on a system as either a signed or
       an unsigned type. The ISO C standard says that it must be
       unsigned, but a few systems are known not to adhere to this.
       Additionally, even when size_t is unsigned, sbrk (which is by
       default used to obtain memory from system) accepts signed
       arguments, and may not be able to handle size_t-wide arguments
       with negative sign bit.  Generally, values that would
       appear as negative after accounting for overhead and alignment
       are supported only via mmap(), which does not have this
       limitation.

       Requests for sizes outside the allowed range will perform an optional
       failure action and then return null. (Requests may also
       also fail because a system is out of memory.)

  Thread-safety: thread-safe

  Compliance: I believe it is compliant with the 1997 Single Unix Specification
       Also SVID/XPG, ANSI C, and probably others as well.

* Synopsis of compile-time options:

    People have reported using previous versions of this malloc on all
    versions of Unix, sometimes by tweaking some of the defines
    below. It has been tested most extensively on Solaris and Linux.
    People also report using it in stand-alone embedded systems.

    The implementation is in straight, hand-tuned ANSI C.  It is not
    at all modular. (Sorry!)  It uses a lot of macros.  To be at all
    usable, this code should be compiled using an optimizing compiler
    (for example gcc -O3) that can simplify expressions and control
    paths. (FAQ: some macros import variables as arguments rather than
    declare locals because people reported that some debuggers
    otherwise get confused.)

    OPTION                     DEFAULT VALUE

    Compilation Environment options:

    HAVE_MREMAP                0

    Changing default word sizes:

    INTERNAL_SIZE_T            size_t

    Configuration and functionality options:

    USE_PUBLIC_MALLOC_WRAPPERS NOT defined
    USE_MALLOC_LOCK            NOT defined
    MALLOC_DEBUG               NOT defined
    REALLOC_ZERO_BYTES_FREES   1

    Options for customizing MORECORE:

    MORECORE                   sbrk
    MORECORE_FAILURE           -1
    MORECORE_CONTIGUOUS        1
    MORECORE_CANNOT_TRIM       NOT defined
    MORECORE_CLEARS            1
    MMAP_AS_MORECORE_SIZE      (1024 * 1024)

    Tuning options that are also dynamically changeable via mallopt:

    DEFAULT_TRIM_THRESHOLD     128 * 1024
    DEFAULT_TOP_PAD            0
    DEFAULT_MMAP_THRESHOLD     128 * 1024
    DEFAULT_MMAP_MAX           65536

    There are several other #defined constants and macros that you
    probably don't want to touch unless you are extending or adapting malloc.  */

#include <stddef.h>   /* for size_t */
#include <stdlib.h>   /* for getenv(), abort() */
#include <unistd.h>   /* for __libc_enable_secure */

#include <atomic.h>
#include <_itoa.h>
#include <bits/wordsize.h>
#include <sys/sysinfo.h>

#include <ldsodefs.h>
#include <setvmaname.h>

#include <unistd.h>
#include <stdio.h>    /* needed for malloc_stats */
#include <errno.h>
#include <assert.h>
#include <intprops.h>

#include <shlib-compat.h>

/* For uintptr_t.  */
#include <stdint.h>

/* For stdc_count_ones.  */
#include <stdbit.h>

/* For va_arg, va_start, va_end.  */
#include <stdarg.h>

/* For MIN, MAX, powerof2.  */
#include <sys/param.h>

/* For ALIGN_UP et. al.  */
#include <libc-pointer-arith.h>

/* For internal malloc interfaces and declarations.  */
#include <malloc/malloc-internal.h>

/* For SINGLE_THREAD_P.  */
#include <sysdep-cancel.h>

#include <libc-internal.h>

/* For tcache double-free check.  */
#include <random-bits.h>
#include <sys/random.h>
#include <not-cancel.h>

verify (sizeof (unsigned long) == sizeof (size_t));
verify (sizeof (void *) == sizeof (size_t));
verify (sizeof (void *) == 4 || sizeof (void *) == 8);
verify (PTRDIFF_MAX <= SIZE_MAX / 2);

/*
  Debugging:

  Because freed chunks may be overwritten with bookkeeping fields, this
  malloc will often die when freed memory is overwritten by user
  programs.  This can be very effective (albeit in an annoying way)
  in helping track down dangling pointers.

  If you compile with -DMALLOC_DEBUG, a number of assertion checks are
  enabled that will catch more memory errors. You probably won't be
  able to make much sense of the actual assertion errors, but they
  should help you locate incorrectly overwritten memory.  The checking
  is fairly extensive, and will slow down execution
  noticeably. Calling malloc_stats or mallinfo with MALLOC_DEBUG set
  will attempt to check every non-mmapped allocated and free chunk in
  the course of computing the summaries. (By nature, mmapped regions
  cannot be checked very much automatically.)

  Setting MALLOC_DEBUG may also be helpful if you are trying to modify
  this code. The assertions in the check routines spell out in more
  detail the assumptions and invariants underlying the algorithms.

  Setting MALLOC_DEBUG does NOT provide an automated mechanism for
  checking that all accesses to malloced memory stay within their
  bounds. However, there are several add-ons and adaptations of this
  or other mallocs available that do this.
*/

#ifndef MALLOC_DEBUG
#define MALLOC_DEBUG 0
#endif

#if USE_TCACHE
/* We want 64 entries.  This is an arbitrary limit, which tunables can reduce.  */
# define TCACHE_SMALL_BINS		64
# define TCACHE_LARGE_BINS		12 /* Up to 4M chunks */
# define TCACHE_MAX_BINS	(TCACHE_SMALL_BINS + TCACHE_LARGE_BINS)
# define MAX_TCACHE_SMALL_SIZE	tidx2csize (TCACHE_SMALL_BINS-1)

# define tidx2csize(idx)	(((size_t) idx) * MALLOC_ALIGNMENT + MINSIZE)
# define tidx2usize(idx)	(((size_t) idx) * MALLOC_ALIGNMENT + MINSIZE - SIZE_SZ)

/* When "x" is from chunksize().  */
# define csize2tidx(x) (((x) - MINSIZE) / MALLOC_ALIGNMENT)
/* When "x" is a user-provided size.  */
# define usize2tidx(x) csize2tidx (checked_request2size (x))

/* With rounding and alignment, the bins are...
   idx 0   bytes 0..24 (64-bit) or 0..12 (32-bit)
   idx 1   bytes 25..40 or 13..20
   idx 2   bytes 41..56 or 21..28
   etc.  */

/* This is another arbitrary limit, which tunables can change.  Each
   tcache bin will hold at most this number of chunks.  */
# define TCACHE_FILL_COUNT 16

/* Maximum chunks in tcache bins for tunables.  This value must fit the range
   of tcache->num_slots[] entries, else they may overflow.  */
# define MAX_TCACHE_COUNT UINT16_MAX
#endif

/* Safe-Linking:
   Use randomness from ASLR (mmap_base) to protect single-linked lists
   of TCache.  That is, mask the "next" pointers of the
   lists' chunks, and also perform allocation alignment checks on them.
   This mechanism reduces the risk of pointer hijacking, as was done with
   Safe-Unlinking in the double-linked lists of Small-Bins.
   It assumes a minimum page size of 4096 bytes (12 bits).  Systems with
   larger pages provide less entropy, although the pointer mangling
   still works.  */
#define PROTECT_PTR(pos, ptr) \
  ((__typeof (ptr)) ((((size_t) pos) >> 12) ^ ((size_t) ptr)))
#define REVEAL_PTR(ptr)  PROTECT_PTR (&ptr, ptr)

/*
  The REALLOC_ZERO_BYTES_FREES macro controls the behavior of realloc (p, 0)
  when p is nonnull.  If the macro is nonzero, the realloc call returns NULL;
  otherwise, the call returns what malloc (0) would.  In either case,
  p is freed.  Glibc uses a nonzero REALLOC_ZERO_BYTES_FREES, which
  implements common historical practice.

  ISO C17 says the realloc call has implementation-defined behavior,
  and it might not even free p.
*/

#ifndef REALLOC_ZERO_BYTES_FREES
#define REALLOC_ZERO_BYTES_FREES 1
#endif

/* Definition for getting more memory from the OS.  */
#include "morecore.c"

#define MORECORE         (*__glibc_morecore)
#define MORECORE_FAILURE  NULL

static int extra_mmap_prot = 0;


/*
  MORECORE-related declarations. By default, rely on sbrk
*/


/*
  MORECORE is the name of the routine to call to obtain more memory
  from the system.  See below for general guidance on writing
  alternative MORECORE functions, as well as a version for WIN32 and a
  sample version for pre-OSX macos.
*/

#ifndef MORECORE
#define MORECORE sbrk
#endif

/*
  MORECORE_FAILURE is the value returned upon failure of MORECORE
  as well as mmap. Since it cannot be an otherwise valid memory address,
  and must reflect values of standard sys calls, you probably ought not
  try to redefine it.
*/

#ifndef MORECORE_FAILURE
#define MORECORE_FAILURE (-1)
#endif

/*
  If MORECORE_CONTIGUOUS is true, take advantage of fact that
  consecutive calls to MORECORE with positive arguments always return
  contiguous increasing addresses.  This is true of unix sbrk.  Even
  if not defined, when regions happen to be contiguous, malloc will
  permit allocations spanning regions obtained from different
  calls. But defining this when applicable enables some stronger
  consistency checks and space efficiencies.
*/

#ifndef MORECORE_CONTIGUOUS
#define MORECORE_CONTIGUOUS 1
#endif

/*
  Define MORECORE_CANNOT_TRIM if your version of MORECORE
  cannot release space back to the system when given negative
  arguments. This is generally necessary only if you are using
  a hand-crafted MORECORE function that cannot handle negative arguments.
*/

/* #define MORECORE_CANNOT_TRIM */

/*  MORECORE_CLEARS           (default 1)
     The degree to which the routine mapped to MORECORE zeroes out
     memory: never (0), only for newly allocated space (1) or always
     (2).  The distinction between (1) and (2) is necessary because on
     some systems, if the application first decrements and then
     increments the break value, the contents of the reallocated space
     are unspecified.
 */

#ifndef MORECORE_CLEARS
# define MORECORE_CLEARS 1
#endif


/*
   MMAP_AS_MORECORE_SIZE is the minimum mmap size argument to use if
   sbrk fails, and mmap is used as a backup.  The value must be a
   multiple of page size.  This backup strategy generally applies only
   when systems have "holes" in address space, so sbrk cannot perform
   contiguous expansion, but there is still space available on system.
   On systems for which this is known to be useful (i.e. most linux
   kernels), this occurs only when programs allocate huge amounts of
   memory.  Between this, and the fact that mmap regions tend to be
   limited, the size should be large, to avoid too many mmap calls and
   thus avoid running out of kernel resources.  */

#ifndef MMAP_AS_MORECORE_SIZE
#define MMAP_AS_MORECORE_SIZE (1024 * 1024)
#endif

/*
  Define HAVE_MREMAP to make realloc() use mremap() to re-allocate
  large blocks.
*/

#ifndef HAVE_MREMAP
#define HAVE_MREMAP 0
#endif

/*
  This version of malloc supports the standard SVID/XPG mallinfo
  routine that returns a struct containing usage properties and
  statistics. It should work on any SVID/XPG compliant system that has
  a /usr/include/malloc.h defining struct mallinfo. (If you'd like to
  install such a thing yourself, cut out the preliminary declarations
  as described above and below and save them in a malloc.h file. But
  there's no compelling reason to bother to do this.)

  The main declaration needed is the mallinfo struct that is returned
  (by-copy) by mallinfo().  The SVID/XPG mallinfo struct contains a
  bunch of fields that are not even meaningful in this version of
  malloc.  These fields are are instead filled by mallinfo() with
  other numbers that might be of interest.
*/


/* ---------- description of public routines ------------ */

#if IS_IN (libc)
/*
  malloc(size_t n)
  Returns a pointer to a newly allocated chunk of at least n bytes, or null
  if no space is available. Additionally, on failure, errno is
  set to ENOMEM on ANSI C systems.

  If n is zero, malloc returns a minimum-sized chunk. (The minimum
  size is 16 bytes on most 32bit systems, and 24 or 32 bytes on 64bit
  systems.)  On most systems, size_t is an unsigned type, so calls
  with negative arguments are interpreted as requests for huge amounts
  of space, which will often fail. The maximum supported value of n
  differs across systems, but is in all cases less than the maximum
  representable value of a size_t.
*/
void *__libc_malloc (size_t);
libc_hidden_proto (__libc_malloc)

static void *__libc_calloc2 (size_t);
static void *__libc_malloc2 (size_t);

/*
  free(void* p)
  Releases the chunk of memory pointed to by p, that had been previously
  allocated using malloc or a related routine such as realloc.
  It has no effect if p is null. It can have arbitrary (i.e., bad!)
  effects if p has already been freed.

  Unless disabled (using mallopt), freeing very large spaces will
  when possible, automatically trigger operations that give
  back unused memory to the system, thus reducing program footprint.
*/
void     __libc_free(void*);
libc_hidden_proto (__libc_free)

/*
  calloc(size_t n_elements, size_t element_size);
  Returns a pointer to n_elements * element_size bytes, with all locations
  set to zero.
*/
void*  __libc_calloc(size_t, size_t);

/*
  realloc(void* p, size_t n)
  Returns a pointer to a chunk of size n that contains the same data
  as does chunk p up to the minimum of (n, p's size) bytes, or null
  if no space is available.

  The returned pointer may or may not be the same as p. The algorithm
  prefers extending p when possible, otherwise it employs the
  equivalent of a malloc-copy-free sequence.

  If p is null, realloc is equivalent to malloc.

  If space is not available, realloc returns null, errno is set (if on
  ANSI) and p is NOT freed.

  if n is for fewer bytes than already held by p, the newly unused
  space is lopped off and freed if possible.  Unless the #define
  REALLOC_ZERO_BYTES_FREES is set, realloc with a size argument of
  zero (re)allocates a minimum-sized chunk.

  Large chunks that were internally obtained via mmap will always be
  grown using malloc-copy-free sequences unless the system supports
  MREMAP (currently only linux).

  The old unix realloc convention of allowing the last-free'd chunk
  to be used as an argument to realloc is not supported.
*/
void*  __libc_realloc(void*, size_t);
libc_hidden_proto (__libc_realloc)

/*
  memalign(size_t alignment, size_t n);
  Returns a pointer to a newly allocated chunk of n bytes, aligned
  in accord with the alignment argument.

  The alignment argument should be a power of two. If the argument is
  not a power of two, the nearest greater power is used.
  8-byte alignment is guaranteed by normal malloc calls, so don't
  bother calling memalign with an argument of 8 or less.

  Overreliance on memalign is a sure way to fragment space.
*/
void*  __libc_memalign(size_t, size_t);
libc_hidden_proto (__libc_memalign)

/*
  valloc(size_t n);
  Equivalent to memalign(pagesize, n), where pagesize is the page
  size of the system. If the pagesize is unknown, 4096 is used.
*/
void*  __libc_valloc(size_t);



/*
  mallinfo()
  Returns (by copy) a struct containing various summary statistics:

  arena:     current total non-mmapped bytes allocated from system
  ordblks:   the number of free chunks
  hblks:     current number of mmapped regions
  hblkhd:    total bytes held in mmapped regions
  usmblks:   always 0
  uordblks:  current total allocated space (normal or mmapped)
  fordblks:  total free space
  keepcost:  the maximum number of bytes that could ideally be released
	       back to system via malloc_trim. ("ideally" means that
	       it ignores page restrictions etc.)

  Because these fields are ints, but internal bookkeeping may
  be kept as longs, the reported values may wrap around zero and
  thus be inaccurate.
*/
struct mallinfo2 __libc_mallinfo2(void);
libc_hidden_proto (__libc_mallinfo2)

struct mallinfo __libc_mallinfo(void);


/*
  pvalloc(size_t n);
  Equivalent to valloc(minimum-page-that-holds(n)), that is,
  round up n to nearest pagesize.
 */
void*  __libc_pvalloc(size_t);

/*
  malloc_trim(size_t pad);

  If possible, gives memory back to the system (via negative
  arguments to sbrk) if there is unused memory at the `high' end of
  the malloc pool. You can call this after freeing large blocks of
  memory to potentially reduce the system-level memory requirements
  of a program. However, it cannot guarantee to reduce memory. Under
  some allocation patterns, some large free blocks of memory will be
  locked between two used chunks, so they cannot be given back to
  the system.

  The `pad' argument to malloc_trim represents the amount of free
  trailing space to leave untrimmed. If this argument is zero,
  only the minimum amount of memory to maintain internal data
  structures will be left (one page or less). Non-zero arguments
  can be supplied to maintain enough trailing space to service
  future expected allocations without having to re-obtain memory
  from the system.

  Malloc_trim returns 1 if it actually released any memory, else 0.
  On systems that do not support "negative sbrks", it will always
  return 0.
*/
int      __malloc_trim(size_t);

/*
  malloc_usable_size(void* p);

  Returns the number of bytes you can actually use in
  an allocated chunk, which may be more than you requested (although
  often not) due to alignment and minimum size constraints.
  You can use this many bytes without worrying about
  overwriting other allocated objects. This is not a particularly great
  programming practice. malloc_usable_size can be more useful in
  debugging and assertions, for example:

  p = malloc(n);
  assert(malloc_usable_size(p) >= 256);

*/
size_t   __malloc_usable_size(void*);

/*
  malloc_stats();
  Prints on stderr the amount of space obtained from the system (both
  via sbrk and mmap), the maximum amount (which may be more than
  current if malloc_trim and/or munmap got called), and the current
  number of bytes allocated via malloc (or realloc, etc) but not yet
  freed. Note that this is the number of bytes allocated, not the
  number requested. It will be larger than the number requested
  because of alignment and bookkeeping overhead. Because it includes
  alignment wastage as being in use, this figure may be greater than
  zero even when no user-level chunks are allocated.

  The reported current and maximum system memory can be inaccurate if
  a program makes other calls to system memory allocation functions
  (normally sbrk) outside of malloc.

  malloc_stats prints only the most commonly interesting statistics.
  More information can be obtained by calling mallinfo.

*/
void     __malloc_stats(void);

/*
  posix_memalign(void **memptr, size_t alignment, size_t size);

  POSIX wrapper like memalign(), checking for validity of size.
*/
int      __posix_memalign(void **, size_t, size_t);
#endif /* IS_IN (libc) */

/*
  mallopt(int parameter_number, int parameter_value)
  Sets tunable parameters The format is to provide a
  (parameter-number, parameter-value) pair.  mallopt then sets the
  corresponding parameter to the argument value if it can (i.e., so
  long as the value is meaningful), and returns 1 if successful else
  0.  SVID/XPG/ANSI defines four standard param numbers for mallopt,
  normally defined in malloc.h. These params (M_MXFAST, M_NLBLKS, M_GRAIN,
  M_KEEP) don't apply to our malloc, so setting them has no effect. But this
  malloc also supports four other options in mallopt. See below for details.
  Briefly, supported parameters are as follows (listed defaults are for
  "typical" configurations).

  Symbol            param #   default    allowed param values
  M_MXFAST          1         64         0-80  (deprecated)
  M_TRIM_THRESHOLD -1         128*1024   any   (-1U disables trimming)
  M_TOP_PAD        -2         0          any
  M_MMAP_THRESHOLD -3         128*1024   any   (or 0 if no MMAP support)
  M_MMAP_MAX       -4         65536      any   (0 disables use of mmap)
*/
int      __libc_mallopt(int, int);
#if IS_IN (libc)
libc_hidden_proto (__libc_mallopt)
#endif

/* mallopt tuning options */


/*
  M_TRIM_THRESHOLD is the maximum amount of unused top-most memory
  to keep before releasing via malloc_trim in free().

  Automatic trimming is mainly useful in long-lived programs.
  Because trimming via sbrk can be slow on some systems, and can
  sometimes be wasteful (in cases where programs immediately
  afterward allocate more large chunks) the value should be high
  enough so that your overall system performance would improve by
  releasing this much memory.

  The trim threshold and the mmap control parameters (see below)
  can be traded off with one another. Trimming and mmapping are
  two different ways of releasing unused memory back to the
  system. Between these two, it is often possible to keep
  system-level demands of a long-lived program down to a bare
  minimum. For example, in one test suite of sessions measuring
  the XF86 X server on Linux, using a trim threshold of 128K and a
  mmap threshold of 192K led to near-minimal long term resource
  consumption.

  If you are using this malloc in a long-lived program, it should
  pay to experiment with these values.  As a rough guide, you
  might set to a value close to the average size of a process
  (program) running on your system.  Releasing this much memory
  would allow such a process to run in memory.  Generally, it's
  worth it to tune for trimming rather than memory mapping when a
  program undergoes phases where several large chunks are
  allocated and released in ways that can reuse each other's
  storage, perhaps mixed with phases where there are no such
  chunks at all.  And in well-behaved long-lived programs,
  controlling release of large blocks via trimming versus mapping
  is usually faster.

  However, in most programs, these parameters serve mainly as
  protection against the system-level effects of carrying around
  massive amounts of unneeded memory. Since frequent calls to
  sbrk, mmap, and munmap otherwise degrade performance, the default
  parameters are set to relatively high values that serve only as
  safeguards.

  The trim value It must be greater than page size to have any useful
  effect.  To disable trimming completely, you can set to
  (unsigned long)(-1)

  You can force an attempted trim by calling malloc_trim.

  Also, trimming is not generally possible in cases where
  the main arena is obtained via mmap.

  Note that the trick some people use of mallocing a huge space and
  then freeing it at program startup, in an attempt to reserve system
  memory, doesn't have the intended effect under automatic trimming,
  since that memory will immediately be returned to the system.
*/

#define M_TRIM_THRESHOLD       -1

#ifndef DEFAULT_TRIM_THRESHOLD
#define DEFAULT_TRIM_THRESHOLD (128 * 1024)
#endif

/*
  M_TOP_PAD is the amount of extra `padding' space to allocate or
  retain whenever sbrk is called. It is used in two ways internally:

  * When sbrk is called to extend the top of the arena to satisfy
  a new malloc request, this much padding is added to the sbrk
  request.

  * When malloc_trim is called automatically from free(),
  it is used as the `pad' argument.

  In both cases, the actual amount of padding is rounded
  so that the end of the arena is always a system page boundary.

  The main reason for using padding is to avoid calling sbrk so
  often. Having even a small pad greatly reduces the likelihood
  that nearly every malloc request during program start-up (or
  after trimming) will invoke sbrk, which needlessly wastes
  time.

  Automatic rounding-up to page-size units is normally sufficient
  to avoid measurable overhead, so the default is 0.  However, in
  systems where sbrk is relatively slow, it can pay to increase
  this value, at the expense of carrying around more memory than
  the program needs.
*/

#define M_TOP_PAD              -2

#ifndef DEFAULT_TOP_PAD
#define DEFAULT_TOP_PAD        (0)
#endif

/*
  MMAP_THRESHOLD_MAX and _MIN are the bounds on the dynamically
  adjusted MMAP_THRESHOLD.
*/

#ifndef DEFAULT_MMAP_THRESHOLD_MIN
#define DEFAULT_MMAP_THRESHOLD_MIN (128 * 1024)
#endif

#ifndef DEFAULT_MMAP_THRESHOLD_MAX
  /* For 32-bit platforms we cannot increase the maximum mmap
     threshold much because it is also the minimum value for the
     maximum heap size and its alignment.  Going above 512k (i.e., 1M
     for new heaps) wastes too much address space.  */
# if __WORDSIZE == 32
#  define DEFAULT_MMAP_THRESHOLD_MAX (512 * 1024)
# else
#  define DEFAULT_MMAP_THRESHOLD_MAX (4 * 1024 * 1024 * sizeof(long))
# endif
#endif

/*
  M_MMAP_THRESHOLD is the request size threshold for using mmap()
  to service a request. Requests of at least this size that cannot
  be allocated using already-existing space will be serviced via mmap.
  (If enough normal freed space already exists it is used instead.)

  Using mmap segregates relatively large chunks of memory so that
  they can be individually obtained and released from the host
  system. A request serviced through mmap is never reused by any
  other request (at least not directly; the system may just so
  happen to remap successive requests to the same locations).

  Segregating space in this way has the benefits that:

   1. Mmapped space can ALWAYS be individually released back
      to the system, which helps keep the system level memory
      demands of a long-lived program low.
   2. Mapped memory can never become `locked' between
      other chunks, as can happen with normally allocated chunks, which
      means that even trimming via malloc_trim would not release them.
   3. On some systems with "holes" in address spaces, mmap can obtain
      memory that sbrk cannot.

  However, it has the disadvantages that:

   1. The space cannot be reclaimed, consolidated, and then
      used to service later requests, as happens with normal chunks.
   2. It can lead to more wastage because of mmap page alignment
      requirements
   3. It causes malloc performance to be more dependent on host
      system memory management support routines which may vary in
      implementation quality and may impose arbitrary
      limitations. Generally, servicing a request via normal
      malloc steps is faster than going through a system's mmap.

  The advantages of mmap nearly always outweigh disadvantages for
  "large" chunks, but the value of "large" varies across systems.  The
  default is an empirically derived value that works well in most
  systems.


  Update in 2006:
  The above was written in 2001. Since then the world has changed a lot.
  Memory got bigger. Applications got bigger. The virtual address space
  layout in 32 bit linux changed.

  In the new situation, brk() and mmap space is shared and there are no
  artificial limits on brk size imposed by the kernel. What is more,
  applications have started using transient allocations larger than the
  128Kb as was imagined in 2001.

  The price for mmap is also high now; each time glibc mmaps from the
  kernel, the kernel is forced to zero out the memory it gives to the
  application. Zeroing memory is expensive and eats a lot of cache and
  memory bandwidth. This has nothing to do with the efficiency of the
  virtual memory system, by doing mmap the kernel just has no choice but
  to zero.

  In 2001, the kernel had a maximum size for brk() which was about 800
  megabytes on 32 bit x86, at that point brk() would hit the first
  mmaped shared libraries and couldn't expand anymore. With current 2.6
  kernels, the VA space layout is different and brk() and mmap
  both can span the entire heap at will.

  Rather than using a static threshold for the brk/mmap tradeoff,
  we are now using a simple dynamic one. The goal is still to avoid
  fragmentation. The old goals we kept are
  1) try to get the long lived large allocations to use mmap()
  2) really large allocations should always use mmap()
  and we're adding now:
  3) transient allocations should use brk() to avoid forcing the kernel
     having to zero memory over and over again

  The implementation works with a sliding threshold, which is by default
  limited to go between 128Kb and 32Mb (64Mb for 64 bit machines) and starts
  out at 128Kb as per the 2001 default.

  This allows us to satisfy requirement 1) under the assumption that long
  lived allocations are made early in the process' lifespan, before it has
  started doing dynamic allocations of the same size (which will
  increase the threshold).

  The upperbound on the threshold satisfies requirement 2)

  The threshold goes up in value when the application frees memory that was
  allocated with the mmap allocator. The idea is that once the application
  starts freeing memory of a certain size, it's highly probable that this is
  a size the application uses for transient allocations. This estimator
  is there to satisfy the new third requirement.

*/

#define M_MMAP_THRESHOLD      -3

#ifndef DEFAULT_MMAP_THRESHOLD
#define DEFAULT_MMAP_THRESHOLD DEFAULT_MMAP_THRESHOLD_MIN
#endif

/*
  M_MMAP_MAX is the maximum number of requests to simultaneously
  service using mmap. This parameter exists because
  some systems have a limited number of internal tables for
  use by mmap, and using more than a few of them may degrade
  performance.

  The default is set to a value that serves only as a safeguard.
  Setting to 0 disables use of mmap for servicing large requests.
*/

#define M_MMAP_MAX             -4

#ifndef DEFAULT_MMAP_MAX
#define DEFAULT_MMAP_MAX       (65536)
#endif

#include <malloc.h>

#ifndef RETURN_ADDRESS
#define RETURN_ADDRESS(X_) (NULL)
#endif

/* Forward declarations.  */
struct malloc_chunk;
typedef struct malloc_chunk* mchunkptr;

/* Internal routines.  */

static void*  _int_malloc(mstate, size_t);
static void _int_free_chunk (mstate, mchunkptr, INTERNAL_SIZE_T);
static void _int_free_merge_chunk (mstate, mchunkptr, INTERNAL_SIZE_T);
static INTERNAL_SIZE_T _int_free_create_chunk (mstate,
					       mchunkptr, INTERNAL_SIZE_T,
					       mchunkptr, INTERNAL_SIZE_T);
static void _int_free_maybe_trim (mstate, INTERNAL_SIZE_T);
static void*  _int_realloc(mstate, mchunkptr, INTERNAL_SIZE_T,
			   INTERNAL_SIZE_T);
static void*  _int_memalign(mstate, size_t, size_t);
#if IS_IN (libc)
static void*  _mid_memalign(size_t, size_t);
#endif

#if USE_TCACHE
static void malloc_printerr_tail(const char *str);
#endif
static void malloc_printerr(const char *str) __attribute__ ((noreturn));

static void munmap_chunk(mchunkptr p);
#if HAVE_MREMAP
static mchunkptr mremap_chunk(mchunkptr p, size_t new_size);
#endif

static size_t musable (void *mem);

/* ------------------ MMAP support ------------------  */


#include <fcntl.h>
#include <sys/mman.h>

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
# define MAP_ANONYMOUS MAP_ANON
#endif

#define MMAP(addr, size, prot, flags) \
 __mmap((addr), (size), (prot), (flags)|MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)


/*
  -----------------------  Chunk representations -----------------------
*/


/*
  This struct declaration is misleading (but accurate and necessary).
  It declares a "view" into memory allowing access to necessary
  fields at known offsets from a given base. See explanation below.
*/

struct malloc_chunk {

  INTERNAL_SIZE_T      mchunk_prev_size;  /* Size of previous chunk (if free).  */
  INTERNAL_SIZE_T      mchunk_size;       /* Size in bytes, including overhead. */

  struct malloc_chunk* fd;         /* double links -- used only if free. */
  struct malloc_chunk* bk;

  /* Only used for large blocks: pointer to next larger size.  */
  struct malloc_chunk* fd_nextsize; /* double links -- used only if free. */
  struct malloc_chunk* bk_nextsize;
};


/*
   malloc_chunk details:

    (The following includes lightly edited explanations by Colin Plumb.)

    Chunks of memory are maintained using a `boundary tag' method as
    described in e.g., Knuth or Standish.  (See the paper by Paul
    Wilson ftp://ftp.cs.utexas.edu/pub/garbage/allocsrv.ps for a
    survey of such techniques.)  Sizes of free chunks are stored both
    in the front of each chunk and at the end.  This makes
    consolidating fragmented chunks into bigger chunks very fast.  The
    size fields also hold bits representing whether chunks are free or
    in use.

    An allocated chunk looks like this:


    chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Size of previous chunk, if unallocated (P clear)  |
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Size of chunk, in bytes                     |A|M|P|
      mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             User data starts here...                          .
	    .                                                               .
	    .             (malloc_usable_size() bytes)                      .
	    .                                                               |
nextchunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             (size of chunk, but used for application data)    |
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Size of next chunk, in bytes                |A|0|1|
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    Where "chunk" is the front of the chunk for the purpose of most of
    the malloc code, but "mem" is the pointer that is returned to the
    user.  "Nextchunk" is the beginning of the next contiguous chunk.

    Chunks always begin on even word boundaries, so the mem portion
    (which is returned to the user) is also on an even word boundary, and
    thus at least double-word aligned.

    Free chunks are stored in circular doubly-linked lists, and look like this:

    chunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Size of previous chunk, if unallocated (P clear)  |
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    `head:' |             Size of chunk, in bytes                     |A|0|P|
      mem-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Forward pointer to next chunk in list             |
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Back pointer to previous chunk in list            |
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Unused space (may be 0 bytes long)                .
	    .                                                               .
	    .                                                               |
nextchunk-> +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    `foot:' |             Size of chunk, in bytes                           |
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	    |             Size of next chunk, in bytes                |A|0|0|
	    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    The P (PREV_INUSE) bit, stored in the unused low-order bit of the
    chunk size (which is always a multiple of two words), is an in-use
    bit for the *previous* chunk.  If that bit is *clear*, then the
    word before the current chunk size contains the previous chunk
    size, and can be used to find the front of the previous chunk.
    The very first chunk allocated always has this bit set,
    preventing access to non-existent (or non-owned) memory. If
    prev_inuse is set for any given chunk, then you CANNOT determine
    the size of the previous chunk, and might even get a memory
    addressing fault when trying to do so.

    The A (NON_MAIN_ARENA) bit is cleared for chunks on the initial,
    main arena, described by the main_arena variable.  When additional
    threads are spawned, each thread receives its own arena (up to a
    configurable limit, after which arenas are reused for multiple
    threads), and the chunks in these arenas have the A bit set.  To
    find the arena for a chunk on such a non-main arena, heap_for_ptr
    performs a bit mask operation and indirection through the ar_ptr
    member of the per-heap header heap_info (see arena.c).

    Note that the `foot' of the current chunk is actually represented
    as the prev_size of the NEXT chunk. This makes it easier to
    deal with alignments etc but can be very confusing when trying
    to extend or adapt this code.

    The two exceptions to all this are:

     1. The special chunk `top' doesn't bother using the
	trailing size field since there is no next contiguous chunk
	that would have to index off it. After initialization, `top'
	is forced to always exist.  If it would become less than
	MINSIZE bytes long, it is replenished.

     2. Chunks allocated via mmap, which have the second-lowest-order
	bit M (IS_MMAPPED) set in their size fields.  Because they are
	allocated one-by-one, each must contain its own trailing size
	field.  If the M bit is set, the other bits are ignored
	(because mmapped chunks are neither in an arena, nor adjacent
	to a freed chunk).  The M bit is also used for chunks which
	originally came from a dumped heap via malloc_set_state in
	hooks.c.
*/

/*
  ---------- Size and alignment checks and conversions ----------
*/

/* The chunk header is two SIZE_SZ elements, but this is used widely, so
   we define it here for clarity later.  */
#define CHUNK_HDR_SZ (2 * SIZE_SZ)

/* Convert a chunk address to a user mem pointer.  */
#define chunk2mem(p) ((void*)((char*)(p) + CHUNK_HDR_SZ))

/* Convert a user mem pointer to a chunk address.  */
#define mem2chunk(mem) ((mchunkptr) (((char*)(mem) - CHUNK_HDR_SZ)))

/* The smallest possible chunk */
#define MIN_CHUNK_SIZE        (offsetof(struct malloc_chunk, fd_nextsize))

/* The smallest size we can malloc is an aligned minimal chunk */

#define MINSIZE  \
  (unsigned long)(((MIN_CHUNK_SIZE+MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK))

/* Check if m has acceptable alignment */

#define misaligned_mem(m)  ((uintptr_t)(m) & MALLOC_ALIGN_MASK)

#define misaligned_chunk(p) (misaligned_mem( chunk2mem (p)))

/* Check if REQ overflows when padded and aligned and if the resulting
   value is less than PTRDIFF_T.  Returns the requested size or
   MINSIZE in case the value is less than MINSIZE, or SIZE_MAX if any
   of the previous checks fail.  */
static __always_inline size_t
checked_request2size (size_t req)
{
  if (__glibc_unlikely (req > PTRDIFF_MAX))
    return SIZE_MAX;

  return (req + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE
	  ? MINSIZE
	  : (req + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK);
}

/*
   --------------- Physical chunk operations ---------------
 */


/* size field is or'ed with PREV_INUSE when previous adjacent chunk in use */
#define PREV_INUSE 0x1

/* extract inuse bit of previous chunk */
#define prev_inuse(p)       ((p)->mchunk_size & PREV_INUSE)


/* size field is or'ed with IS_MMAPPED if the chunk was obtained with mmap() */
#define IS_MMAPPED 0x2

/* check for mmap()'ed chunk */
#define chunk_is_mmapped(p) ((p)->mchunk_size & IS_MMAPPED)


/* size field is or'ed with NON_MAIN_ARENA if the chunk was obtained
   from a non-main arena.  This is only set immediately before handing
   the chunk to the user, if necessary.  */
#define NON_MAIN_ARENA 0x4

/* Check for chunk from main arena.  */
#define chunk_main_arena(p) (((p)->mchunk_size & NON_MAIN_ARENA) == 0)

/* Mark a chunk as not being on the main arena.  */
#define set_non_main_arena(p) ((p)->mchunk_size |= NON_MAIN_ARENA)


/*
   Bits to mask off when extracting size

   Note: IS_MMAPPED is intentionally not masked off from size field in
   macros for which mmapped chunks should never be seen. This should
   cause helpful core dumps to occur if it is tried by accident by
   people extending or adapting this malloc.
 */
#define SIZE_BITS (PREV_INUSE | IS_MMAPPED | NON_MAIN_ARENA)

/* Get size, ignoring use bits */
#define chunksize(p) (chunksize_nomask (p) & ~(SIZE_BITS))

/* Like chunksize, but do not mask SIZE_BITS.  */
#define chunksize_nomask(p)         ((p)->mchunk_size)

/* Ptr to next physical malloc_chunk. */
#define next_chunk(p) ((mchunkptr) (((char *) (p)) + chunksize (p)))

/* Size of the chunk below P.  Only valid if !prev_inuse (P).  */
#define prev_size(p) ((p)->mchunk_prev_size)

/* Set the size of the chunk below P.  Only valid if !prev_inuse (P).  */
#define set_prev_size(p, sz) ((p)->mchunk_prev_size = (sz))

/* Ptr to previous physical malloc_chunk.  Only valid if !prev_inuse (P).  */
#define prev_chunk(p) ((mchunkptr) (((char *) (p)) - prev_size (p)))

/* Treat space at ptr + offset as a chunk */
#define chunk_at_offset(p, s)  ((mchunkptr) (((char *) (p)) + (s)))

/* extract p's inuse bit */
#define inuse(p)							      \
  ((((mchunkptr) (((char *) (p)) + chunksize (p)))->mchunk_size) & PREV_INUSE)

/* set/clear chunk as being inuse without otherwise disturbing */
#define set_inuse(p)							      \
  ((mchunkptr) (((char *) (p)) + chunksize (p)))->mchunk_size |= PREV_INUSE

#define clear_inuse(p)							      \
  ((mchunkptr) (((char *) (p)) + chunksize (p)))->mchunk_size &= ~(PREV_INUSE)


/* check/set/clear inuse bits in known places */
#define inuse_bit_at_offset(p, s)					      \
  (((mchunkptr) (((char *) (p)) + (s)))->mchunk_size & PREV_INUSE)

#define set_inuse_bit_at_offset(p, s)					      \
  (((mchunkptr) (((char *) (p)) + (s)))->mchunk_size |= PREV_INUSE)

#define clear_inuse_bit_at_offset(p, s)					      \
  (((mchunkptr) (((char *) (p)) + (s)))->mchunk_size &= ~(PREV_INUSE))


/* Set size at head, without disturbing its use bit */
#define set_head_size(p, s)  ((p)->mchunk_size = (((p)->mchunk_size & SIZE_BITS) | (s)))

/* Set size/use field */
#define set_head(p, s)       ((p)->mchunk_size = (s))

/* Set size at footer (only when chunk is not in use) */
#define set_foot(p, s)       (((mchunkptr) ((char *) (p) + (s)))->mchunk_prev_size = (s))

#pragma GCC poison mchunk_size
#pragma GCC poison mchunk_prev_size

/* This is the size of the real usable data in the chunk.  Not valid for
   dumped heap chunks.  */
#define memsize(p) (chunksize (p) - CHUNK_HDR_SZ + SIZE_SZ)

/* Huge page used for an mmap chunk.  */
#define MMAP_HP 0x1

/* Return whether an mmap chunk uses huge pages.  */
static __always_inline bool
mmap_is_hp (mchunkptr p)
{
  return prev_size (p) & MMAP_HP;
}

/* Return the mmap chunk's offset from mmap base.  */
static __always_inline size_t
mmap_base_offset (mchunkptr p)
{
  return prev_size (p) & ~MMAP_HP;
}

/* Return pointer to mmap base from a chunk with IS_MMAPPED set.  */
static __always_inline uintptr_t
mmap_base (mchunkptr p)
{
  return (uintptr_t) p - mmap_base_offset (p);
}

/* Return total mmap size of a chunk with IS_MMAPPED set.  */
static __always_inline size_t
mmap_size (mchunkptr p)
{
  return mmap_base_offset (p) + chunksize (p) + CHUNK_HDR_SZ;
}

/* Return a new chunk from an mmap.  */
static __always_inline mchunkptr
mmap_set_chunk (uintptr_t mmap_base, size_t mmap_size, size_t offset, bool is_hp)
{
  mchunkptr p = (mchunkptr) (mmap_base + offset);
  prev_size (p) = offset | (is_hp ? MMAP_HP : 0);
  set_head (p, (mmap_size - offset - CHUNK_HDR_SZ) | IS_MMAPPED);
  return p;
}

/*
   -------------------- Internal data structures --------------------

   All internal state is held in an instance of malloc_state defined
   below. There are no other static variables, except in two optional
   cases:
 * If USE_MALLOC_LOCK is defined, the mALLOC_MUTEx declared above.
 * If mmap doesn't support MAP_ANONYMOUS, a dummy file descriptor
     for mmap.

   Beware of lots of tricks that minimize the total bookkeeping space
   requirements. The result is a little over 1K bytes (for 4byte
   pointers and size_t.)
 */

/*
   Bins

    An array of bin headers for free chunks. Each bin is doubly
    linked.  The bins are approximately proportionally (log) spaced.
    There are a lot of these bins (128). This may look excessive, but
    works very well in practice.  Most bins hold sizes that are
    unusual as malloc request sizes, but are more usual for fragments
    and consolidated sets of chunks, which is what these bins hold, so
    they can be found quickly.  All procedures maintain the invariant
    that no consolidated chunk physically borders another one, so each
    chunk in a list is known to be preceded and followed by either
    inuse chunks or the ends of memory.

    Chunks in bins are kept in size order, with ties going to the
    approximately least recently used chunk. Ordering isn't needed
    for the small bins, which all contain the same-sized chunks, but
    facilitates best-fit allocation for larger chunks. These lists
    are just sequential. Keeping them in order almost never requires
    enough traversal to warrant using fancier ordered data
    structures.

    Chunks of the same size are linked with the most
    recently freed at the front, and allocations are taken from the
    back.  This results in LRU (FIFO) allocation order, which tends
    to give each chunk an equal opportunity to be consolidated with
    adjacent freed chunks, resulting in larger free chunks and less
    fragmentation.

    To simplify use in double-linked lists, each bin header acts
    as a malloc_chunk. This avoids special-casing for headers.
    But to conserve space and improve locality, we allocate
    only the fd/bk pointers of bins, and then use repositioning tricks
    to treat these as the fields of a malloc_chunk*.
 */

typedef struct malloc_chunk *mbinptr;

/* addressing -- note that bin_at(0) does not exist */
#define bin_at(m, i) \
  (mbinptr) (((char *) &((m)->bins[((i) - 1) * 2]))			      \
             - offsetof (struct malloc_chunk, fd))

/* analog of ++bin */
#define next_bin(b)  ((mbinptr) ((char *) (b) + (sizeof (mchunkptr) << 1)))

/* Reminders about list directionality within bins */
#define first(b)     ((b)->fd)
#define last(b)      ((b)->bk)

/*
   Indexing

    Bins for sizes < 512 bytes contain chunks of all the same size, spaced
    8 bytes apart. Larger bins are approximately logarithmically spaced:

    64 bins of size       8
    32 bins of size      64
    16 bins of size     512
     8 bins of size    4096
     4 bins of size   32768
     2 bins of size  262144
     1 bin  of size what's left

    There is actually a little bit of slop in the numbers in bin_index
    for the sake of speed. This makes no difference elsewhere.

    The bins top out around 1MB because we expect to service large
    requests via mmap.

    Bin 0 does not exist.  Bin 1 is the unordered list; if that would be
    a valid chunk size the small bins are bumped up one.
 */

#define NBINS             128
#define NSMALLBINS         64
#define SMALLBIN_WIDTH    MALLOC_ALIGNMENT
#define SMALLBIN_CORRECTION (MALLOC_ALIGNMENT > CHUNK_HDR_SZ)
#define MIN_LARGE_SIZE    ((NSMALLBINS - SMALLBIN_CORRECTION) * SMALLBIN_WIDTH)

#define in_smallbin_range(sz)  \
  ((unsigned long) (sz) < (unsigned long) MIN_LARGE_SIZE)

#define smallbin_index(sz) \
  ((SMALLBIN_WIDTH == 16 ? (((unsigned) (sz)) >> 4) : (((unsigned) (sz)) >> 3))\
   + SMALLBIN_CORRECTION)

#define largebin_index_32(sz)                                                \
  (((((unsigned long) (sz)) >> 6) <= 38) ?  56 + (((unsigned long) (sz)) >> 6) :\
   ((((unsigned long) (sz)) >> 9) <= 20) ?  91 + (((unsigned long) (sz)) >> 9) :\
   ((((unsigned long) (sz)) >> 12) <= 10) ? 110 + (((unsigned long) (sz)) >> 12) :\
   ((((unsigned long) (sz)) >> 15) <= 4) ? 119 + (((unsigned long) (sz)) >> 15) :\
   ((((unsigned long) (sz)) >> 18) <= 2) ? 124 + (((unsigned long) (sz)) >> 18) :\
   126)

#define largebin_index_32_big(sz)                                            \
  (((((unsigned long) (sz)) >> 6) <= 45) ?  49 + (((unsigned long) (sz)) >> 6) :\
   ((((unsigned long) (sz)) >> 9) <= 20) ?  91 + (((unsigned long) (sz)) >> 9) :\
   ((((unsigned long) (sz)) >> 12) <= 10) ? 110 + (((unsigned long) (sz)) >> 12) :\
   ((((unsigned long) (sz)) >> 15) <= 4) ? 119 + (((unsigned long) (sz)) >> 15) :\
   ((((unsigned long) (sz)) >> 18) <= 2) ? 124 + (((unsigned long) (sz)) >> 18) :\
   126)

// XXX It remains to be seen whether it is good to keep the widths of
// XXX the buckets the same or whether it should be scaled by a factor
// XXX of two as well.
#define largebin_index_64(sz)                                                \
  (((((unsigned long) (sz)) >> 6) <= 48) ?  48 + (((unsigned long) (sz)) >> 6) :\
   ((((unsigned long) (sz)) >> 9) <= 20) ?  91 + (((unsigned long) (sz)) >> 9) :\
   ((((unsigned long) (sz)) >> 12) <= 10) ? 110 + (((unsigned long) (sz)) >> 12) :\
   ((((unsigned long) (sz)) >> 15) <= 4) ? 119 + (((unsigned long) (sz)) >> 15) :\
   ((((unsigned long) (sz)) >> 18) <= 2) ? 124 + (((unsigned long) (sz)) >> 18) :\
   126)

#define largebin_index(sz) \
  (SIZE_SZ == 8 ? largebin_index_64 (sz)                                     \
   : MALLOC_ALIGNMENT == 16 ? largebin_index_32_big (sz)                     \
   : largebin_index_32 (sz))

#define bin_index(sz) \
  ((in_smallbin_range (sz)) ? smallbin_index (sz) : largebin_index (sz))

/* Take a chunk off a bin list.  */
static void
unlink_chunk (mstate av, mchunkptr p)
{
  if (chunksize (p) != prev_size (next_chunk (p)))
    malloc_printerr ("corrupted size vs. prev_size");

  mchunkptr fd = p->fd;
  mchunkptr bk = p->bk;

  if (__glibc_unlikely (fd->bk != p || bk->fd != p))
    malloc_printerr ("corrupted double-linked list");

  fd->bk = bk;
  bk->fd = fd;
  if (!in_smallbin_range (chunksize_nomask (p)) && p->fd_nextsize != NULL)
    {
      if (p->fd_nextsize->bk_nextsize != p
	  || p->bk_nextsize->fd_nextsize != p)
	malloc_printerr ("corrupted double-linked list (not small)");

      if (fd->fd_nextsize == NULL)
	{
	  if (p->fd_nextsize == p)
	    fd->fd_nextsize = fd->bk_nextsize = fd;
	  else
	    {
	      fd->fd_nextsize = p->fd_nextsize;
	      fd->bk_nextsize = p->bk_nextsize;
	      p->fd_nextsize->bk_nextsize = fd;
	      p->bk_nextsize->fd_nextsize = fd;
	    }
	}
      else
	{
	  p->fd_nextsize->bk_nextsize = p->bk_nextsize;
	  p->bk_nextsize->fd_nextsize = p->fd_nextsize;
	}
    }
}

/*
   Unsorted chunks

    All remainders from chunk splits, as well as all returned chunks,
    are first placed in the "unsorted" bin. They are then placed
    in regular bins after malloc gives them ONE chance to be used before
    binning. So, basically, the unsorted_chunks list acts as a queue,
    with chunks being placed on it in free,
    and taken off (to be either used or placed in bins) in malloc.

    The NON_MAIN_ARENA flag is never set for unsorted chunks, so it
    does not have to be taken into account in size comparisons.
 */

/* The otherwise unindexable 1-bin is used to hold unsorted chunks. */
#define unsorted_chunks(M)          (bin_at (M, 1))

/*
   Top

    The top-most available chunk (i.e., the one bordering the end of
    available memory) is treated specially. It is never included in
    any bin, is used only if no other chunk is available, and is
    released back to the system if it is very large (see
    M_TRIM_THRESHOLD).  Because top initially
    points to its own bin with initial zero size, thus forcing
    extension on the first malloc request, we avoid having any special
    code in malloc to check whether it even exists yet. But we still
    need to do so when getting memory from system, so we make
    initial_top treat the bin as a legal but unusable chunk during the
    interval between initialization and the first call to
    sysmalloc. (This is somewhat delicate, since it relies on
    the 2 preceding words to be zero during this interval as well.)
 */

/* Conveniently, the unsorted bin can be used as dummy top on first call */
#define initial_top(M)              (unsorted_chunks (M))

/*
   Binmap

    To help compensate for the large number of bins, a one-level index
    structure is used for bin-by-bin searching.  `binmap' is a
    bitvector recording whether bins are definitely empty so they can
    be skipped over during during traversals.  The bits are NOT always
    cleared as soon as bins are empty, but instead only
    when they are noticed to be empty during traversal in malloc.
 */

/* Conservatively use 32 bits per map word, even if on 64bit system */
#define BINMAPSHIFT      5
#define BITSPERMAP       (1U << BINMAPSHIFT)
#define BINMAPSIZE       (NBINS / BITSPERMAP)

#define idx2block(i)     ((i) >> BINMAPSHIFT)
#define idx2bit(i)       ((1U << ((i) & ((1U << BINMAPSHIFT) - 1))))

#define mark_bin(m, i)    ((m)->binmap[idx2block (i)] |= idx2bit (i))
#define unmark_bin(m, i)  ((m)->binmap[idx2block (i)] &= ~(idx2bit (i)))
#define get_binmap(m, i)  ((m)->binmap[idx2block (i)] & idx2bit (i))


/*
   ATTEMPT_TRIMMING_THRESHOLD is the size of a chunk in free()
   that may attempt trimming of an arena's heap. This is a heuristic, so the
   exact value should not matter too much. It is defined at half the default
   trim threshold as a compromise heuristic to only attempt trimming if it is
   likely to release a significant amount of memory.
 */

#define ATTEMPT_TRIMMING_THRESHOLD  (65536UL)

/*
   NONCONTIGUOUS_BIT indicates that MORECORE does not return contiguous
   regions.  Otherwise, contiguity is exploited in merging together,
   when possible, results from consecutive MORECORE calls.

   The initial value comes from MORECORE_CONTIGUOUS, but is
   changed dynamically if mmap is ever used as an sbrk substitute.
 */

#define NONCONTIGUOUS_BIT     (2U)

#define contiguous(M)          (((M)->flags & NONCONTIGUOUS_BIT) == 0)
#define set_noncontiguous(M)   ((M)->flags |= NONCONTIGUOUS_BIT)
#define set_contiguous(M)      ((M)->flags &= ~NONCONTIGUOUS_BIT)


/*
   ----------- Internal state representation and initialization -----------
 */


struct malloc_state
{
  /* Serialize access.  */
  __libc_lock_define (, mutex);

  /* Flags  */
  int flags;

  /* Base of the topmost chunk -- not otherwise kept in a bin */
  mchunkptr top;

  /* The remainder from the most recent split of a small request */
  mchunkptr last_remainder;

  /* Normal bins packed as described above */
  mchunkptr bins[NBINS * 2 - 2];

  /* Bitmap of bins */
  unsigned int binmap[BINMAPSIZE];

  /* Linked list */
  struct malloc_state *next;

  /* Linked list for free arenas.  Access to this field is serialized
     by free_list_lock in arena.c.  */
  struct malloc_state *next_free;

  /* Number of threads attached to this arena.  0 if the arena is on
     the free list.  Access to this field is serialized by
     free_list_lock in arena.c.  */
  INTERNAL_SIZE_T attached_threads;

  /* Memory allocated from the system in this arena.  */
  INTERNAL_SIZE_T system_mem;
  INTERNAL_SIZE_T max_system_mem;
};

struct malloc_par
{
  /* Tunable parameters */
  unsigned long trim_threshold;
  INTERNAL_SIZE_T top_pad;
  INTERNAL_SIZE_T mmap_threshold;
  INTERNAL_SIZE_T arena_test;
  INTERNAL_SIZE_T arena_max;

  /* Transparent Large Page support.  */
  enum thp_mode_t thp_mode;
  INTERNAL_SIZE_T thp_pagesize;
  /* A value different than 0 means to align mmap allocation to hp_pagesize
     add hp_flags on flags.  */
  INTERNAL_SIZE_T hp_pagesize;
  int hp_flags;

  /* Memory map support */
  int n_mmaps;
  int n_mmaps_max;
  int max_n_mmaps;
  /* the mmap_threshold is dynamic, until the user sets
     it manually, at which point we need to disable any
     dynamic behavior. */
  int no_dyn_threshold;

  /* Statistics */
  INTERNAL_SIZE_T mmapped_mem;
  INTERNAL_SIZE_T max_mmapped_mem;

  /* First address handed out by MORECORE/sbrk.  */
  char *sbrk_base;

#if USE_TCACHE
  /* Maximum number of small buckets to use.  */
  size_t tcache_small_bins;
  size_t tcache_max_bytes;
  /* Maximum number of chunks in each bucket.  */
  size_t tcache_count;
#endif
};

/* There are several instances of this struct ("arenas") in this
   malloc.  If you are adapting this malloc in a way that does NOT use
   a static or mmapped malloc_state, you MUST explicitly zero-fill it
   before using. This malloc relies on the property that malloc_state
   is initialized to all zeroes (as is true of C statics).  */

static struct malloc_state main_arena =
{
  .mutex = _LIBC_LOCK_INITIALIZER,
  .next = &main_arena,
  .attached_threads = 1
};

/* There is only one instance of the malloc parameters.  */

static struct malloc_par mp_ =
{
  .top_pad = DEFAULT_TOP_PAD,
  .n_mmaps_max = DEFAULT_MMAP_MAX,
  .mmap_threshold = DEFAULT_MMAP_THRESHOLD,
  .trim_threshold = DEFAULT_TRIM_THRESHOLD,
  .arena_test = sizeof (long) == 4 ? 2 : 8,
  .thp_mode = thp_mode_unknown
#if USE_TCACHE
  ,
  .tcache_count = TCACHE_FILL_COUNT,
  .tcache_small_bins = TCACHE_SMALL_BINS,
  .tcache_max_bytes = MAX_TCACHE_SMALL_SIZE + 1,
#endif
};

/*
   Initialize a malloc_state struct.

   This is called from __ptmalloc_init () or from _int_new_arena ()
   when creating a new arena.
 */

static void
malloc_init_state (mstate av)
{
  int i;
  mbinptr bin;

  /* Establish circular links for normal bins */
  for (i = 1; i < NBINS; ++i)
    {
      bin = bin_at (av, i);
      bin->fd = bin->bk = bin;
    }

#if MORECORE_CONTIGUOUS
  if (av != &main_arena)
#endif
  set_noncontiguous (av);

  av->top = initial_top (av);
}

/*
   Other internal utilities operating on mstates
 */

static void *sysmalloc (INTERNAL_SIZE_T, mstate);
static int      systrim (size_t, mstate);


/* -------------- Early definitions for debugging hooks ---------------- */

/* This function is called from the arena shutdown hook, to free the
   thread cache (if it exists).  */
static void tcache_thread_shutdown (void);

/* ------------------ Testing support ----------------------------------*/

static int perturb_byte;

static void
alloc_perturb (char *p, size_t n)
{
  if (__glibc_unlikely (perturb_byte))
    memset (p, perturb_byte ^ 0xff, n);
}

static void
free_perturb (char *p, size_t n)
{
  if (__glibc_unlikely (perturb_byte))
    memset (p, perturb_byte, n);
}



#include <stap-probe.h>

/* ----------- Routines dealing with transparent huge pages ----------- */

static inline void
madvise_thp (void *p, INTERNAL_SIZE_T size)
{
#ifdef MADV_HUGEPAGE

  /* Only use __madvise if the system is using 'madvise' mode and the size
     is at least a huge page, otherwise the call is wasteful. */
  if (mp_.thp_mode != thp_mode_madvise || size < mp_.thp_pagesize)
    return;

  /* Linux requires the input address to be page-aligned, and unaligned
     inputs happens only for initial data segment.  */
  if (__glibc_unlikely (!PTR_IS_ALIGNED (p, GLRO (dl_pagesize))))
    {
      void *q = PTR_ALIGN_UP (p, GLRO (dl_pagesize));
      size -= PTR_DIFF (q, p);
      p = q;
    }

  __madvise (p, size, MADV_HUGEPAGE);
#endif
}

/* ------------------- Support for multiple arenas -------------------- */
#include "arena.c"

/*
   Debugging support

   These routines make a number of assertions about the states
   of data structures that should be true at all times. If any
   are not true, it's very likely that a user program has somehow
   trashed memory. (It's also possible that there is a coding error
   in malloc. In which case, please report it!)
 */

#if !MALLOC_DEBUG

# define check_chunk(A, P)
# define check_free_chunk(A, P)
# define check_inuse_chunk(A, P)
# define check_malloced_chunk(A, P, N)
# define check_malloc_state(A)

#else

# define check_chunk(A, P)              do_check_chunk (A, P)
# define check_free_chunk(A, P)         do_check_free_chunk (A, P)
# define check_inuse_chunk(A, P)        do_check_inuse_chunk (A, P)
# define check_malloced_chunk(A, P, N)   do_check_malloced_chunk (A, P, N)
# define check_malloc_state(A)         do_check_malloc_state (A)

/*
   Properties of all chunks
 */

static void
do_check_chunk (mstate av, mchunkptr p)
{
  unsigned long sz = chunksize (p);

  if (!chunk_is_mmapped (p))
    {
      /* min and max possible addresses assuming contiguous allocation */
      char *max_address = (char *) (av->top) + chunksize (av->top);
      char *min_address = max_address - av->system_mem;

      /* Has legal address ... */
      if (p != av->top)
        {
          if (contiguous (av))
            {
              assert (((char *) p) >= min_address);
              assert (((char *) p + sz) <= ((char *) (av->top)));
            }
        }
      else
        {
          /* top size is always at least MINSIZE */
          assert ((unsigned long) (sz) >= MINSIZE);
          /* top predecessor always marked inuse */
          assert (prev_inuse (p));
        }
    }
  else
    {
      /* chunk is page-aligned */
      assert ((mmap_size (p) & (GLRO (dl_pagesize) - 1)) == 0);
      /* mem is aligned */
      assert (!misaligned_chunk (p));
    }
}

/*
   Properties of free chunks
 */

static void
do_check_free_chunk (mstate av, mchunkptr p)
{
  INTERNAL_SIZE_T sz = chunksize_nomask (p) & ~(PREV_INUSE | NON_MAIN_ARENA);
  mchunkptr next = chunk_at_offset (p, sz);

  do_check_chunk (av, p);

  /* Chunk must claim to be free ... */
  assert (!inuse (p));
  assert (!chunk_is_mmapped (p));

  /* Unless a special marker, must have OK fields */
  if ((unsigned long) (sz) >= MINSIZE)
    {
      assert ((sz & MALLOC_ALIGN_MASK) == 0);
      assert (!misaligned_chunk (p));
      /* ... matching footer field */
      assert (prev_size (next_chunk (p)) == sz);
      /* ... and is fully consolidated */
      assert (prev_inuse (p));
      assert (next == av->top || inuse (next));

      /* ... and has minimally sane links */
      assert (p->fd->bk == p);
      assert (p->bk->fd == p);
    }
  else /* markers are always of size SIZE_SZ */
    assert (sz == SIZE_SZ);
}

/*
   Properties of inuse chunks
 */

static void
do_check_inuse_chunk (mstate av, mchunkptr p)
{
  mchunkptr next;

  do_check_chunk (av, p);

  if (chunk_is_mmapped (p))
    return; /* mmapped chunks have no next/prev */

  /* Check whether it claims to be in use ... */
  assert (inuse (p));

  next = next_chunk (p);

  /* ... and is surrounded by OK chunks.
     Since more things can be checked with free chunks than inuse ones,
     if an inuse chunk borders them and debug is on, it's worth doing them.
   */
  if (!prev_inuse (p))
    {
      /* Note that we cannot even look at prev unless it is not inuse */
      mchunkptr prv = prev_chunk (p);
      assert (next_chunk (prv) == p);
      do_check_free_chunk (av, prv);
    }

  if (next == av->top)
    {
      assert (prev_inuse (next));
      assert (chunksize (next) >= MINSIZE);
    }
  else if (!inuse (next))
    do_check_free_chunk (av, next);
}

/*
   Properties of chunks at the point they are malloced
 */

static void
do_check_malloced_chunk (mstate av, mchunkptr p, INTERNAL_SIZE_T s)
{
  INTERNAL_SIZE_T sz = chunksize_nomask (p) & ~(PREV_INUSE | NON_MAIN_ARENA);

  if (!chunk_is_mmapped (p))
    {
      assert (av == arena_for_chunk (p));
      if (chunk_main_arena (p))
        assert (av == &main_arena);
      else
        assert (av != &main_arena);
    }

  do_check_inuse_chunk (av, p);

  /* Legal size ... */
  assert ((sz & MALLOC_ALIGN_MASK) == 0);
  assert ((unsigned long) (sz) >= MINSIZE);
  /* ... and alignment */
  assert (!misaligned_chunk (p));
  /* chunk is less than MINSIZE more than request */
  assert ((long) (sz) - (long) (s) >= 0);
  assert ((long) (sz) - (long) (s + MINSIZE) < 0);

  /*
     ... plus,  must obey implementation invariant that prev_inuse is
     always true of any allocated chunk; i.e., that each allocated
     chunk borders either a previously allocated and still in-use
     chunk, or the base of its memory arena. This is ensured
     by making all allocations from the `lowest' part of any found
     chunk.
   */

  assert (prev_inuse (p));
}


/*
   Properties of malloc_state.

   This may be useful for debugging malloc, as well as detecting user
   programmer errors that somehow write into malloc_state.

   If you are extending or experimenting with this malloc, you can
   probably figure out how to hack this routine to print out or
   display chunk addresses, sizes, bins, and other instrumentation.
 */

static void
do_check_malloc_state (mstate av)
{
  int i;
  mchunkptr p;
  mchunkptr q;
  mbinptr b;
  unsigned int idx;
  INTERNAL_SIZE_T size;
  unsigned long total = 0;

  /* internal size_t must be no wider than pointer type */
  assert (sizeof (INTERNAL_SIZE_T) <= sizeof (char *));

  /* alignment is a power of 2 */
  assert ((MALLOC_ALIGNMENT & (MALLOC_ALIGNMENT - 1)) == 0);

  /* Check the arena is initialized. */
  assert (av->top != 0);

  /* No memory has been allocated yet, so doing more tests is not possible.  */
  if (av->top == initial_top (av))
    return;

  /* pagesize is a power of 2 */
  assert (powerof2(GLRO (dl_pagesize)));

  /* A contiguous main_arena is consistent with sbrk_base.  */
  if (av == &main_arena && contiguous (av))
    assert ((char *) mp_.sbrk_base + av->system_mem ==
            (char *) av->top + chunksize (av->top));

  /* check normal bins */
  for (i = 1; i < NBINS; ++i)
    {
      b = bin_at (av, i);

      /* binmap is accurate (except for bin 1 == unsorted_chunks) */
      if (i >= 2)
        {
          unsigned int binbit = get_binmap (av, i);
          int empty = last (b) == b;
          if (!binbit)
            assert (empty);
          else if (!empty)
            assert (binbit);
        }

      for (p = last (b); p != b; p = p->bk)
        {
          /* each chunk claims to be free */
          do_check_free_chunk (av, p);
          size = chunksize (p);
          total += size;
          if (i >= 2)
            {
              /* chunk belongs in bin */
              idx = bin_index (size);
              assert (idx == i);
              /* lists are sorted */
              assert (p->bk == b ||
                      (unsigned long) chunksize (p->bk) >= (unsigned long) chunksize (p));

              if (!in_smallbin_range (size))
                {
                  if (p->fd_nextsize != NULL)
                    {
                      if (p->fd_nextsize == p)
                        assert (p->bk_nextsize == p);
                      else
                        {
                          if (p->fd_nextsize == first (b))
                            assert (chunksize (p) < chunksize (p->fd_nextsize));
                          else
                            assert (chunksize (p) > chunksize (p->fd_nextsize));

                          if (p == first (b))
                            assert (chunksize (p) > chunksize (p->bk_nextsize));
                          else
                            assert (chunksize (p) < chunksize (p->bk_nextsize));
                        }
                    }
                  else
                    assert (p->bk_nextsize == NULL);
                }
            }
          else if (!in_smallbin_range (size))
            assert (p->fd_nextsize == NULL && p->bk_nextsize == NULL);
          /* chunk is followed by a legal chain of inuse chunks */
          for (q = next_chunk (p);
               (q != av->top && inuse (q) &&
                (unsigned long) (chunksize (q)) >= MINSIZE);
               q = next_chunk (q))
            do_check_inuse_chunk (av, q);
        }
    }

  /* top chunk is OK */
  check_chunk (av, av->top);
}
#endif /* MALLOC_DEBUG */


/* ----------------- Support for debugging hooks -------------------- */
#if IS_IN (libc)
#include "hooks.c"
#endif


/* ----------- Routines dealing with system allocation -------------- */

/* Allocate a mmap chunk - used for large block sizes or as a fallback.
   Round up size to nearest page.  Add padding if MALLOC_ALIGNMENT is
   larger than CHUNK_HDR_SZ.  Add CHUNK_HDR_SZ at the end so that mmap
   chunks have the same layout as regular chunks.  */

static void *
sysmalloc_mmap (INTERNAL_SIZE_T nb, size_t pagesize, int extra_flags)
{
  size_t padding = MALLOC_ALIGNMENT - CHUNK_HDR_SZ;
  size_t size = ALIGN_UP (nb + padding + CHUNK_HDR_SZ, pagesize);

  char *mm = (char *) MMAP (NULL, size,
			    extra_mmap_prot | PROT_READ | PROT_WRITE,
			    extra_flags);
  if (mm == MAP_FAILED)
    return mm;
  if (extra_flags == 0)
    madvise_thp (mm, size);

  __set_vma_name (mm, size, " glibc: malloc");

  mchunkptr p = mmap_set_chunk ((uintptr_t)mm, size, padding, extra_flags != 0);

  /* update statistics */
  int new = atomic_fetch_add_relaxed (&mp_.n_mmaps, 1) + 1;
  atomic_max (&mp_.max_n_mmaps, new);

  unsigned long sum;
  sum = atomic_fetch_add_relaxed (&mp_.mmapped_mem, size) + size;
  atomic_max (&mp_.max_mmapped_mem, sum);

  check_chunk (NULL, p);

  return chunk2mem (p);
}

/*
   Allocate memory using mmap() based on S and NB requested size, aligning to
   PAGESIZE if required.  The EXTRA_FLAGS is used on mmap() call.  If the call
   succeeds S is updated with the allocated size.  This is used as a fallback
   if MORECORE fails.
 */
static void *
sysmalloc_mmap_fallback (size_t *s, size_t size, size_t minsize,
			  size_t pagesize, int extra_flags)
{
  size = ALIGN_UP (size, pagesize);

  /* If we are relying on mmap as backup, then use larger units */
  if (size < minsize)
    size = minsize;

  char *mbrk = (char *) (MMAP (NULL, size,
			       extra_mmap_prot | PROT_READ | PROT_WRITE,
			       extra_flags));
  if (mbrk == MAP_FAILED)
    return MAP_FAILED;

  if (extra_flags == 0)
    madvise_thp (mbrk, size);

  *s = size;
  return mbrk;
}

static void *
sysmalloc (INTERNAL_SIZE_T nb, mstate av)
{
  mchunkptr old_top;              /* incoming value of av->top */
  INTERNAL_SIZE_T old_size;       /* its size */
  char *old_end;                  /* its end address */

  size_t size;                      /* arg to first MORECORE or mmap call */
  char *brk;                      /* return value from MORECORE */

  long correction;                /* arg to 2nd MORECORE call */
  char *snd_brk;                  /* 2nd return val */

  INTERNAL_SIZE_T front_misalign; /* unusable bytes at front of new space */
  INTERNAL_SIZE_T end_misalign;   /* partial page left at end of new space */
  char *aligned_brk;              /* aligned offset into brk */

  mchunkptr p;                    /* the allocated/returned chunk */
  mchunkptr remainder;            /* remainder from allocation */
  unsigned long remainder_size;   /* its size */


  size_t pagesize = GLRO (dl_pagesize);
  bool tried_mmap = false;


  /*
     If have mmap, and the request size meets the mmap threshold, and
     the system supports mmap, and there are few enough currently
     allocated mmapped regions, try to directly map this request
     rather than expanding top.
   */

  if (av == NULL
      || ((unsigned long) (nb) >= (unsigned long) (mp_.mmap_threshold)
	  && (mp_.n_mmaps < mp_.n_mmaps_max)))
    {
      char *mm;
      if (mp_.hp_pagesize > 0 && nb >= mp_.hp_pagesize)
	{
	  /* There is no need to issue the THP madvise call if Huge Pages are
	     used directly.  */
	  mm = sysmalloc_mmap (nb, mp_.hp_pagesize, mp_.hp_flags);
	  if (mm != MAP_FAILED)
	    return mm;
	}
      mm = sysmalloc_mmap (nb, pagesize, 0);
      if (mm != MAP_FAILED)
	return mm;
      tried_mmap = true;
    }

  /* There are no usable arenas and mmap also failed.  */
  if (av == NULL)
    return NULL;

  /* Record incoming configuration of top */

  old_top = av->top;
  old_size = chunksize (old_top);
  old_end = (char *) (chunk_at_offset (old_top, old_size));

  brk = snd_brk = (char *) (MORECORE_FAILURE);

  /*
     If not the first time through, we require old_size to be
     at least MINSIZE and to have prev_inuse set.
   */

  assert ((old_top == initial_top (av) && old_size == 0) ||
          ((unsigned long) (old_size) >= MINSIZE &&
           prev_inuse (old_top) &&
           ((unsigned long) old_end & (pagesize - 1)) == 0));

  /* Precondition: not enough current space to satisfy nb request */
  assert ((unsigned long) (old_size) < (unsigned long) (nb + MINSIZE));


  if (av != &main_arena)
    {
      heap_info *old_heap, *heap;
      size_t old_heap_size;

      /* First try to extend the current heap. */
      old_heap = heap_for_ptr (old_top);
      old_heap_size = old_heap->size;
      if ((long) (MINSIZE + nb - old_size) > 0
          && grow_heap (old_heap, MINSIZE + nb - old_size) == 0)
        {
          av->system_mem += old_heap->size - old_heap_size;
          set_head (old_top, (((char *) old_heap + old_heap->size) - (char *) old_top)
                    | PREV_INUSE);
        }
      else if ((heap = new_heap (nb + (MINSIZE + sizeof (*heap)), mp_.top_pad)))
        {
          /* Use a newly allocated heap.  */
          heap->ar_ptr = av;
          heap->prev = old_heap;
          av->system_mem += heap->size;
          /* Set up the new top.  */
          top (av) = chunk_at_offset (heap, sizeof (*heap));
          set_head (top (av), (heap->size - sizeof (*heap)) | PREV_INUSE);

          /* Setup fencepost and free the old top chunk with a multiple of
             MALLOC_ALIGNMENT in size. */
          /* The fencepost takes at least MINSIZE bytes, because it might
             become the top chunk again later.  Note that a footer is set
             up, too, although the chunk is marked in use. */
          old_size = (old_size - MINSIZE) & ~MALLOC_ALIGN_MASK;
          set_head (chunk_at_offset (old_top, old_size + CHUNK_HDR_SZ),
		    0 | PREV_INUSE);
          if (old_size >= MINSIZE)
            {
              set_head (chunk_at_offset (old_top, old_size),
			CHUNK_HDR_SZ | PREV_INUSE);
              set_foot (chunk_at_offset (old_top, old_size), CHUNK_HDR_SZ);
              set_head (old_top, old_size | PREV_INUSE | NON_MAIN_ARENA);
              _int_free_merge_chunk (av, old_top, chunksize (old_top));
            }
          else
            {
              set_head (old_top, (old_size + CHUNK_HDR_SZ) | PREV_INUSE);
              set_foot (old_top, (old_size + CHUNK_HDR_SZ));
            }
        }
      else if (!tried_mmap)
	{
	  /* We can at least try to use to mmap memory.  If new_heap fails
	     it is unlikely that trying to allocate huge pages will
	     succeed.  */
	  char *mm = sysmalloc_mmap (nb, pagesize, 0);
	  if (mm != MAP_FAILED)
	    return mm;
	}
    }
  else     /* av == main_arena */


    { /* Request enough space for nb + pad + overhead */
      size = nb + mp_.top_pad + MINSIZE;

      /*
         If contiguous, we can subtract out existing space that we hope to
         combine with new space. We add it back later only if
         we don't actually get contiguous space.
       */

      if (contiguous (av))
        size -= old_size;

      /*
         Round to a multiple of page size or huge page size.
         If MORECORE is not contiguous, this ensures that we only call it
         with whole-page arguments.  And if MORECORE is contiguous and
         this is not first time through, this preserves page-alignment of
         previous calls. Otherwise, we correct to page-align below.
       */

      if (__glibc_unlikely (mp_.thp_pagesize != 0))
	{
	  uintptr_t lastbrk = (uintptr_t) MORECORE (0);
	  uintptr_t top = ALIGN_UP (lastbrk + size, mp_.thp_pagesize);
	  size = top - lastbrk;
	}
      else
	size = ALIGN_UP (size, GLRO(dl_pagesize));

      /*
         Don't try to call MORECORE if argument is so big as to appear
         negative. Note that since mmap takes size_t arg, it may succeed
         below even if we cannot call MORECORE.
       */

      if ((ssize_t) size > 0)
        {
          brk = (char *) (MORECORE ((long) size));
	  if (brk != (char *) (MORECORE_FAILURE))
	    madvise_thp (brk, size);
          LIBC_PROBE (memory_sbrk_more, 2, brk, size);
        }

      if (brk == (char *) (MORECORE_FAILURE))
        {
          /*
             If have mmap, try using it as a backup when MORECORE fails or
             cannot be used. This is worth doing on systems that have "holes" in
             address space, so sbrk cannot extend to give contiguous space, but
             space is available elsewhere.  Note that we ignore mmap max count
             and threshold limits, since the space will not be used as a
             segregated mmap region.
           */

	  size_t fallback_size = nb + mp_.top_pad + MINSIZE;
	  char *mbrk = MAP_FAILED;
	  if (mp_.hp_pagesize > 0)
	    mbrk = sysmalloc_mmap_fallback (&size, fallback_size,
					    mp_.hp_pagesize,
					    mp_.hp_pagesize, mp_.hp_flags);
	  if (mbrk == MAP_FAILED)
	    mbrk = sysmalloc_mmap_fallback (&size, fallback_size,
	                                    MMAP_AS_MORECORE_SIZE,
	                                    pagesize, 0);
	  if (mbrk != MAP_FAILED)
	    {
	      __set_vma_name (mbrk, fallback_size, " glibc: malloc");

	      /* Record that we no longer have a contiguous sbrk region.  After the first
		 time mmap is used as backup, we do not ever rely on contiguous space
		 since this could incorrectly bridge regions.  */
	      set_noncontiguous (av);

	      /* We do not need, and cannot use, another sbrk call to find end */
	      brk = mbrk;
	      snd_brk = brk + size;
	    }
        }

      if (brk != (char *) (MORECORE_FAILURE))
        {
          if (mp_.sbrk_base == NULL)
            mp_.sbrk_base = brk;
          av->system_mem += size;

          /*
             If MORECORE extends previous space, we can likewise extend top size.
           */

          if (brk == old_end && snd_brk == (char *) (MORECORE_FAILURE))
            set_head (old_top, (size + old_size) | PREV_INUSE);

          else if (contiguous (av) && old_size && brk < old_end)
	    /* Oops!  Someone else killed our space..  Can't touch anything.  */
	    malloc_printerr ("break adjusted to free malloc space");

          /*
             Otherwise, make adjustments:

           * If the first time through or noncontiguous, we need to call sbrk
              just to find out where the end of memory lies.

           * We need to ensure that all returned chunks from malloc will meet
              MALLOC_ALIGNMENT

           * If there was an intervening foreign sbrk, we need to adjust sbrk
              request size to account for fact that we will not be able to
              combine new space with existing space in old_top.

           * Almost all systems internally allocate whole pages at a time, in
              which case we might as well use the whole last page of request.
              So we allocate enough more memory to hit a page boundary now,
              which in turn causes future contiguous calls to page-align.
           */

          else
            {
              front_misalign = 0;
              end_misalign = 0;
              correction = 0;
              aligned_brk = brk;

              /* handle contiguous cases */
              if (contiguous (av))
                {
                  /* Count foreign sbrk as system_mem.  */
                  if (old_size)
                    av->system_mem += brk - old_end;

                  /* Guarantee alignment of first new chunk made from this space */

                  front_misalign = (INTERNAL_SIZE_T) chunk2mem (brk) & MALLOC_ALIGN_MASK;
                  if (front_misalign > 0)
                    {
                      /*
                         Skip over some bytes to arrive at an aligned position.
                         We don't need to specially mark these wasted front bytes.
                         They will never be accessed anyway because
                         prev_inuse of av->top (and any chunk created from its start)
                         is always true after initialization.
                       */

                      correction = MALLOC_ALIGNMENT - front_misalign;
                      aligned_brk += correction;
                    }

                  /*
                     If this isn't adjacent to existing space, then we will not
                     be able to merge with old_top space, so must add to 2nd request.
                   */

                  correction += old_size;

                  /* Extend the end address to hit a page boundary */
                  end_misalign = (INTERNAL_SIZE_T) (brk + size + correction);
                  correction += (ALIGN_UP (end_misalign, pagesize)) - end_misalign;

                  assert (correction >= 0);
                  snd_brk = (char *) (MORECORE (correction));

                  /*
                     If can't allocate correction, try to at least find out current
                     brk.  It might be enough to proceed without failing.

                     Note that if second sbrk did NOT fail, we assume that space
                     is contiguous with first sbrk. This is a safe assumption unless
                     program is multithreaded but doesn't use locks and a foreign sbrk
                     occurred between our first and second calls.
                   */

                  if (snd_brk == (char *) (MORECORE_FAILURE))
                    {
                      correction = 0;
                      snd_brk = (char *) (MORECORE (0));
                    }
		  else
		    madvise_thp (snd_brk, correction);
                }

              /* handle non-contiguous cases */
              else
                {
                  if (MALLOC_ALIGNMENT == CHUNK_HDR_SZ)
                    /* MORECORE/mmap must correctly align */
                    assert (((unsigned long) chunk2mem (brk) & MALLOC_ALIGN_MASK) == 0);
                  else
                    {
                      front_misalign = (INTERNAL_SIZE_T) chunk2mem (brk) & MALLOC_ALIGN_MASK;
                      if (front_misalign > 0)
                        {
                          /*
                             Skip over some bytes to arrive at an aligned position.
                             We don't need to specially mark these wasted front bytes.
                             They will never be accessed anyway because
                             prev_inuse of av->top (and any chunk created from its start)
                             is always true after initialization.
                           */

                          aligned_brk += MALLOC_ALIGNMENT - front_misalign;
                        }
                    }

                  /* Find out current end of memory */
                  if (snd_brk == (char *) (MORECORE_FAILURE))
                    {
                      snd_brk = (char *) (MORECORE (0));
                    }
                }

              /* Adjust top based on results of second sbrk */
              if (snd_brk != (char *) (MORECORE_FAILURE))
                {
                  av->top = (mchunkptr) aligned_brk;
                  set_head (av->top, (snd_brk - aligned_brk + correction) | PREV_INUSE);
                  av->system_mem += correction;

                  /*
                     If not the first time through, we either have a
                     gap due to foreign sbrk or a non-contiguous region.  Insert a
                     double fencepost at old_top to prevent consolidation with space
                     we don't own. These fenceposts are artificial chunks that are
                     marked as inuse and are in any case too small to use.  We need
                     two to make sizes and alignments work out.
                   */

                  if (old_size != 0)
                    {
                      /*
                         Shrink old_top to insert fenceposts, keeping size a
                         multiple of MALLOC_ALIGNMENT. We know there is at least
                         enough space in old_top to do this.
                       */
                      old_size = (old_size - 2 * CHUNK_HDR_SZ) & ~MALLOC_ALIGN_MASK;
                      set_head (old_top, old_size | PREV_INUSE);

                      /*
                         Note that the following assignments completely overwrite
                         old_top when old_size was previously MINSIZE.  This is
                         intentional. We need the fencepost, even if old_top otherwise gets
                         lost.
                       */
		      set_head (chunk_at_offset (old_top, old_size),
				CHUNK_HDR_SZ | PREV_INUSE);
		      set_head (chunk_at_offset (old_top,
						 old_size + CHUNK_HDR_SZ),
				CHUNK_HDR_SZ | PREV_INUSE);

                      /* If possible, release the rest. */
                      if (old_size >= MINSIZE)
                        {
                          _int_free_merge_chunk (av, old_top, chunksize (old_top));
                        }
                    }
                }
            }
        }
    } /* if (av !=  &main_arena) */

  if ((unsigned long) av->system_mem > (unsigned long) (av->max_system_mem))
    av->max_system_mem = av->system_mem;
  check_malloc_state (av);

  /* finally, do the allocation */
  p = av->top;
  size = chunksize (p);

  /* check that one of the above allocation paths succeeded */
  if ((unsigned long) (size) >= (unsigned long) (nb + MINSIZE))
    {
      remainder_size = size - nb;
      remainder = chunk_at_offset (p, nb);
      av->top = remainder;
      set_head (p, nb | PREV_INUSE | (av != &main_arena ? NON_MAIN_ARENA : 0));
      set_head (remainder, remainder_size | PREV_INUSE);
      check_malloced_chunk (av, p, nb);
      return chunk2mem (p);
    }

  /* catch all failure paths */
  __set_errno (ENOMEM);
  return NULL;
}


/*
   systrim is an inverse of sorts to sysmalloc.  It gives memory back
   to the system (via negative arguments to sbrk) if there is unused
   memory at the `high' end of the malloc pool. It is called
   automatically by free() when top space exceeds the trim
   threshold. It is also called by the public malloc_trim routine.  It
   returns 1 if it actually released any memory, else 0.
 */

static int
systrim (size_t pad, mstate av)
{
  long top_size;         /* Amount of top-most memory */
  long extra;            /* Amount to release */
  long released;         /* Amount actually released */
  char *current_brk;     /* address returned by pre-check sbrk call */
  char *new_brk;         /* address returned by post-check sbrk call */
  long top_area;

  top_size = chunksize (av->top);

  top_area = top_size - MINSIZE - 1;
  if (top_area <= pad)
    return 0;

  /* Release in pagesize units and round down to the nearest page.  */
  if (__glibc_unlikely (mp_.thp_pagesize != 0))
    extra = ALIGN_DOWN (top_area - pad, mp_.thp_pagesize);
  else
    extra = ALIGN_DOWN (top_area - pad, GLRO(dl_pagesize));

  if (extra == 0)
    return 0;

  /*
     Only proceed if end of memory is where we last set it.
     This avoids problems if there were foreign sbrk calls.
   */
  current_brk = (char *) (MORECORE (0));
  if (current_brk == (char *) (av->top) + top_size)
    {
      /*
         Attempt to release memory. We ignore MORECORE return value,
         and instead call again to find out where new end of memory is.
         This avoids problems if first call releases less than we asked,
         of if failure somehow altered brk value. (We could still
         encounter problems if it altered brk in some very bad way,
         but the only thing we can do is adjust anyway, which will cause
         some downstream failure.)
       */

      MORECORE (-extra);
      new_brk = (char *) (MORECORE (0));

      LIBC_PROBE (memory_sbrk_less, 2, new_brk, extra);

      if (new_brk != (char *) MORECORE_FAILURE)
        {
          released = (long) (current_brk - new_brk);

          if (released != 0)
            {
              /* Success. Adjust top. */
              av->system_mem -= released;
              set_head (av->top, (top_size - released) | PREV_INUSE);
              check_malloc_state (av);
              return 1;
            }
        }
    }
  return 0;
}

static void
munmap_chunk (mchunkptr p)
{
  size_t pagesize = GLRO (dl_pagesize);

  assert (chunk_is_mmapped (p));

  uintptr_t mem = (uintptr_t) chunk2mem (p);
  uintptr_t block = mmap_base (p);
  size_t total_size = mmap_size (p);
  /* Unfortunately we have to do the compilers job by hand here.  Normally
     we would test BLOCK and TOTAL-SIZE separately for compliance with the
     page size.  But gcc does not recognize the optimization possibility
     (in the moment at least) so we combine the two values into one before
     the bit test.  */
  if (__glibc_unlikely ((block | total_size) & (pagesize - 1)) != 0
      || __glibc_unlikely (!powerof2 (mem & (pagesize - 1))))
    malloc_printerr ("munmap_chunk(): invalid pointer");

  atomic_fetch_add_relaxed (&mp_.n_mmaps, -1);
  atomic_fetch_add_relaxed (&mp_.mmapped_mem, -total_size);

  /* If munmap failed the process virtual memory address space is in a
     bad shape.  Just leave the block hanging around, the process will
     terminate shortly anyway since not much can be done.  */
  __munmap ((char *) block, total_size);
}

#if HAVE_MREMAP

static mchunkptr
mremap_chunk (mchunkptr p, size_t new_size)
{
  bool is_hp = mmap_is_hp (p);
  size_t pagesize = is_hp ? mp_.hp_pagesize : GLRO (dl_pagesize);
  INTERNAL_SIZE_T offset = mmap_base_offset (p);
  INTERNAL_SIZE_T size = chunksize (p);
  char *cp;

  assert (chunk_is_mmapped (p));

  uintptr_t block = mmap_base (p);
  uintptr_t mem = (uintptr_t) chunk2mem(p);
  size_t total_size = mmap_size (p);
  if (__glibc_unlikely ((block | total_size) & (pagesize - 1)) != 0
      || __glibc_unlikely (!powerof2 (mem & (pagesize - 1))))
    malloc_printerr("mremap_chunk(): invalid pointer");

  /* Note the extra CHUNK_HDR_SZ overhead as in mmap_chunk(). */
  new_size = ALIGN_UP (new_size + offset + CHUNK_HDR_SZ, pagesize);

  /* No need to remap if the number of pages does not change.  */
  if (total_size == new_size)
    return p;

  cp = (char *) __mremap ((char *) block, total_size, new_size,
                          MREMAP_MAYMOVE);

  if (cp == MAP_FAILED)
    return NULL;

  /* mremap preserves the region's flags - this means that if the old chunk
     was marked with MADV_HUGEPAGE, the new chunk will retain that.  */
  if (total_size < mp_.thp_pagesize)
    madvise_thp (cp, new_size);

  p = mmap_set_chunk ((uintptr_t) cp, new_size, offset, is_hp);

  INTERNAL_SIZE_T new;
  new = atomic_fetch_add_relaxed (&mp_.mmapped_mem, new_size - size - offset)
        + new_size - size - offset;
  atomic_max (&mp_.max_mmapped_mem, new);
  return p;
}
#endif /* HAVE_MREMAP */

/*------------------------ Public wrappers. --------------------------------*/

#if USE_TCACHE

/* We overlay this structure on the user-data portion of a chunk when
   the chunk is stored in the per-thread cache.  */
typedef struct tcache_entry
{
  struct tcache_entry *next;
  /* This field exists to detect double frees.  */
  uintptr_t key;
} tcache_entry;

/* There is one of these for each thread, which contains the
   per-thread cache (hence "tcache_perthread_struct").  Keeping
   overall size low is mildly important.  The 'entries' field is linked list of
   free blocks, while 'num_slots' contains the number of free blocks that can
   be added.  Each bin may allow a different maximum number of free blocks,
   and can be disabled by initializing 'num_slots' to zero.  */
typedef struct tcache_perthread_struct
{
  uint16_t num_slots[TCACHE_MAX_BINS];
  tcache_entry *entries[TCACHE_MAX_BINS];
} tcache_perthread_struct;

static const union
{
  struct tcache_perthread_struct inactive;
  struct
  {
    char pad;
    struct tcache_perthread_struct disabled;
  };
} __tcache_dummy;

/* TCACHE is never NULL; it's either "live" or points to one of the
   above dummy entries.  The dummy entries are all zero so act like an
   empty/unusable tcache.  */
static __thread tcache_perthread_struct *tcache =
  (tcache_perthread_struct *) &__tcache_dummy.inactive;

/* This is the default, and means "check to see if a real tcache
   should be allocated."  */
static __always_inline bool
tcache_inactive (void)
{
  return (tcache == &__tcache_dummy.inactive);
}

/* This means "the user has disabled the tcache but we have to point
   to something."  */
static __always_inline bool
tcache_disabled (void)
{
  return (tcache == &__tcache_dummy.disabled);
}

/* This means the tcache is active.  */
static __always_inline bool
tcache_enabled (void)
{
  return (! tcache_inactive () && ! tcache_disabled ());
}

/* Sets the tcache to DISABLED state.  */
static __always_inline void
tcache_set_disabled (void)
{
  tcache = (tcache_perthread_struct *) &__tcache_dummy.disabled;
}

/* Process-wide key to try and catch a double-free in the same thread.  */
static uintptr_t tcache_key;

/* The value of tcache_key does not really have to be a cryptographically
   secure random number.  It only needs to be arbitrary enough so that it does
   not collide with values present in applications.  If a collision does happen
   consistently enough, it could cause a degradation in performance since the
   entire list is checked to check if the block indeed has been freed the
   second time.  The odds of this happening are exceedingly low though, about 1
   in 2^wordsize.  There is probably a higher chance of the performance
   degradation being due to a double free where the first free happened in a
   different thread; that's a case this check does not cover.  */
static void
tcache_key_initialize (void)
{
  /* We need to use the _nostatus version here, see BZ 29624.  */
  if (__getrandom_nocancel_nostatus_direct (&tcache_key, sizeof(tcache_key),
					    GRND_NONBLOCK)
      != sizeof (tcache_key))
    tcache_key = 0;

  /* We need tcache_key to be non-zero (otherwise tcache_double_free_verify's
     clearing of e->key would go unnoticed and it would loop getting called
     through __libc_free), and we want tcache_key not to be a
     commonly-occurring value in memory, so ensure a minimum amount of one and
     zero bits.  */
  int minimum_bits = __WORDSIZE / 4;
  int maximum_bits = __WORDSIZE - minimum_bits;

  while (tcache_key <= 0x1000000
      || tcache_key >= ((uintptr_t) ULONG_MAX) - 0x1000000
      || stdc_count_ones (tcache_key) < minimum_bits
      || stdc_count_ones (tcache_key) > maximum_bits)
    {
      tcache_key = random_bits ();
#if __WORDSIZE == 64
      tcache_key = (tcache_key << 32) | random_bits ();
#endif
    }
}

static __always_inline size_t
large_csize2tidx(size_t nb)
{
  size_t idx = stdc_bit_width (nb) - stdc_bit_width (MAX_TCACHE_SMALL_SIZE);
  return idx + TCACHE_SMALL_BINS;
}

/* Caller must ensure that we know tc_idx is valid and there's room
   for more chunks.  */
static __always_inline void
tcache_put_n (mchunkptr chunk, size_t tc_idx, tcache_entry **ep, bool mangled)
{
  tcache_entry *e = (tcache_entry *) chunk2mem (chunk);

  /* Mark this chunk as "in the tcache" so the test in __libc_free will
     detect a double free.  */
  e->key = tcache_key;

  if (!mangled)
    {
      e->next = PROTECT_PTR (&e->next, *ep);
      *ep = e;
    }
  else
    {
      e->next = PROTECT_PTR (&e->next, REVEAL_PTR (*ep));
      *ep = PROTECT_PTR (ep, e);
    }
  --(tcache->num_slots[tc_idx]);
}

/* Caller must ensure that we know tc_idx is valid and there's
   available chunks to remove.  Removes chunk from the middle of the
   list.  */
static __always_inline void *
tcache_get_n (size_t tc_idx, tcache_entry **ep, bool mangled)
{
  tcache_entry *e;
  if (!mangled)
    e = *ep;
  else
    e = REVEAL_PTR (*ep);

  if (__glibc_unlikely (misaligned_mem (e)))
    malloc_printerr ("malloc(): unaligned tcache chunk detected");

  if (!mangled)
    *ep = REVEAL_PTR (e->next);
  else
    *ep = PROTECT_PTR (ep, REVEAL_PTR (e->next));

  ++(tcache->num_slots[tc_idx]);
  e->key = 0;
  return (void *) e;
}

static __always_inline void
tcache_put (mchunkptr chunk, size_t tc_idx)
{
  tcache_put_n (chunk, tc_idx, &tcache->entries[tc_idx], false);
}

/* Like the above, but removes from the head of the list.  */
static __always_inline void *
tcache_get (size_t tc_idx)
{
  return tcache_get_n (tc_idx, &tcache->entries[tc_idx], false);
}

static __always_inline tcache_entry **
tcache_location_large (size_t nb, size_t tc_idx,
		       bool *mangled, tcache_entry **demangled_ptr)
{
  tcache_entry **tep = &(tcache->entries[tc_idx]);
  tcache_entry *te = *tep;
  while (te != NULL
         && __glibc_unlikely (chunksize (mem2chunk (te)) < nb))
    {
      tep = & (te->next);
      te = REVEAL_PTR (te->next);
      *mangled = true;
    }

  *demangled_ptr = te;
  return tep;
}

static __always_inline void
tcache_put_large (mchunkptr chunk, size_t tc_idx)
{
  tcache_entry **entry;
  bool mangled = false;
  tcache_entry *te;
  entry = tcache_location_large (chunksize (chunk), tc_idx, &mangled, &te);

  return tcache_put_n (chunk, tc_idx, entry, mangled);
}

static __always_inline void *
tcache_get_large (size_t tc_idx, size_t nb)
{
  tcache_entry **entry;
  bool mangled = false;
  tcache_entry *te;
  entry = tcache_location_large (nb, tc_idx, &mangled, &te);

  if (te == NULL || nb != chunksize (mem2chunk (te)))
    return NULL;

  return tcache_get_n (tc_idx, entry, mangled);
}

static void tcache_init (mstate av);

static __always_inline void *
tcache_get_align (size_t nb, size_t alignment)
{
  if (nb < mp_.tcache_max_bytes)
    {
      size_t tc_idx = csize2tidx (nb);
      if (__glibc_unlikely (tc_idx >= TCACHE_SMALL_BINS))
        tc_idx = large_csize2tidx (nb);

      /* The tcache itself isn't encoded, but the chain is.  */
      tcache_entry **tep = & tcache->entries[tc_idx];
      tcache_entry *te = *tep;
      bool mangled = false;
      size_t csize = 0;

      while (te != NULL
	     && ((csize = chunksize (mem2chunk (te))) < nb
		 || (csize == nb
	             && !PTR_IS_ALIGNED (te, alignment))))
        {
          tep = & (te->next);
          te = REVEAL_PTR (te->next);
          mangled = true;
        }

      if (te != NULL
	  && csize == nb
	  && PTR_IS_ALIGNED (te, alignment))
	return tcache_get_n (tc_idx, tep, mangled);
    }
  return NULL;
}

/* Verify if the suspicious tcache_entry is double free.
   It's not expected to execute very often, mark it as noinline.  */
static __attribute__ ((noinline)) void
tcache_double_free_verify (tcache_entry *e)
{
  tcache_entry *tmp;
  for (size_t tc_idx = 0; tc_idx < TCACHE_MAX_BINS; ++tc_idx)
    {
      size_t cnt = 0;
      LIBC_PROBE (memory_tcache_double_free, 2, e, tc_idx);
      for (tmp = tcache->entries[tc_idx];
	   tmp;
	   tmp = REVEAL_PTR (tmp->next), ++cnt)
	{
	  if (cnt >= mp_.tcache_count)
	    malloc_printerr ("free(): too many chunks detected in tcache");
	  if (__glibc_unlikely (misaligned_mem (tmp)))
	    malloc_printerr ("free(): unaligned chunk detected in tcache 2");
	  if (tmp == e)
	    malloc_printerr ("free(): double free detected in tcache 2");
	}
    }
  /* No double free detected - it might be in a tcache of another thread,
     or user data that happens to match the key.  Since we are not sure,
     clear the key and retry freeing it.  */
  e->key = 0;
  __libc_free (e);
}

static void
tcache_thread_shutdown (void)
{
  int i;
  mchunkptr p;
  tcache_perthread_struct *tcache_tmp = tcache;
  int need_free = tcache_enabled ();

  /* Disable the tcache and prevent it from being reinitialized.  */
  tcache_set_disabled ();
  if (! need_free)
    return;

  /* Free all of the entries and the tcache itself back to the arena
     heap for coalescing.  */
  for (i = 0; i < TCACHE_MAX_BINS; ++i)
    {
      while (tcache_tmp->entries[i])
	{
	  tcache_entry *e = tcache_tmp->entries[i];
	  if (__glibc_unlikely (misaligned_mem (e)))
	    malloc_printerr ("tcache_thread_shutdown(): "
			     "unaligned tcache chunk detected");
	  tcache_tmp->entries[i] = REVEAL_PTR (e->next);
	  e->key = 0;
	  p = mem2chunk (e);
	  _int_free_chunk (arena_for_chunk (p), p, chunksize (p));
	}
    }

  p = mem2chunk (tcache_tmp);
  _int_free_chunk (arena_for_chunk (p), p, chunksize (p));
}

/* Initialize tcache.  In the rare case there isn't any memory available,
   later calls will retry initialization.  */
static void
tcache_init (mstate av)
{
  /* Set this unconditionally to avoid infinite loops.  */
  tcache_set_disabled ();
  if (mp_.tcache_count == 0)
    return;

  size_t bytes = sizeof (tcache_perthread_struct);
  if (av)
    tcache =
      (tcache_perthread_struct *) _int_malloc (av, bytes);
  else
    tcache = (tcache_perthread_struct *) __libc_malloc2 (bytes);

  if (tcache == NULL)
    {
      /* If the allocation failed, don't try again.  */
      tcache_set_disabled ();
    }
  else
    {
      memset (tcache, 0, bytes);
      for (int i = 0; i < TCACHE_MAX_BINS; i++)
	tcache->num_slots[i] = mp_.tcache_count;
    }
}

#else  /* !USE_TCACHE */

static void
tcache_thread_shutdown (void)
{
  /* Nothing to do if there is no thread cache.  */
}

#endif /* !USE_TCACHE  */

#if IS_IN (libc)

static void * __attribute_noinline__
__libc_malloc2 (size_t bytes)
{
  mstate ar_ptr;
  void *victim;

  if (SINGLE_THREAD_P)
    {
      victim = _int_malloc (&main_arena, bytes);
      assert (!victim || chunk_is_mmapped (mem2chunk (victim)) ||
	      &main_arena == arena_for_chunk (mem2chunk (victim)));
      return victim;
    }

  arena_get (ar_ptr, bytes);

  victim = _int_malloc (ar_ptr, bytes);
  /* Retry with another arena only if we were able to find a usable arena
     before.  */
  if (!victim && ar_ptr != NULL)
    {
      LIBC_PROBE (memory_malloc_retry, 1, bytes);
      ar_ptr = arena_get_retry (ar_ptr, bytes);
      victim = _int_malloc (ar_ptr, bytes);
    }

  if (ar_ptr != NULL)
    __libc_lock_unlock (ar_ptr->mutex);

  assert (!victim || chunk_is_mmapped (mem2chunk (victim)) ||
          ar_ptr == arena_for_chunk (mem2chunk (victim)));
  return victim;
}

void *
__libc_malloc (size_t bytes)
{
#if USE_TCACHE
  size_t nb = checked_request2size (bytes);

  if (nb < mp_.tcache_max_bytes)
    {
      size_t tc_idx = csize2tidx (nb);

      if (__glibc_likely (tc_idx < TCACHE_SMALL_BINS))
        {
	  if (tcache->entries[tc_idx] != NULL)
	    return tcache_get (tc_idx);
	}
      else
        {
	  tc_idx = large_csize2tidx (nb);
	  void *victim = tcache_get_large (tc_idx, nb);
	  if (victim != NULL)
	    return victim;
	}
    }
#endif

  return __libc_malloc2 (bytes);
}
libc_hidden_def (__libc_malloc)

static void __attribute_noinline__
tcache_free_init (void *mem)
{
  tcache_init (NULL);
  __libc_free (mem);
}

void
__libc_free (void *mem)
{
  mchunkptr p;                          /* chunk corresponding to mem */

  if (mem == NULL)                              /* free(0) has no effect */
    return;

  p = mem2chunk (mem);

  INTERNAL_SIZE_T size = chunksize (p);

  if (__glibc_unlikely (misaligned_chunk (p)))
    return malloc_printerr_tail ("free(): invalid pointer");

#if USE_TCACHE
  if (__glibc_likely (size < mp_.tcache_max_bytes))
    {
      /* Check to see if it's already in the tcache.  */
      tcache_entry *e = (tcache_entry *) chunk2mem (p);

      /* Check for double free - verify if the key matches.  */
      if (__glibc_unlikely (e->key == tcache_key))
        return tcache_double_free_verify (e);

      size_t tc_idx = csize2tidx (size);
      if (__glibc_likely (tc_idx < TCACHE_SMALL_BINS))
	{
          if (__glibc_likely (tcache->num_slots[tc_idx] != 0))
	    return tcache_put (p, tc_idx);
	}
      else
	{
	  tc_idx = large_csize2tidx (size);
	  if (size >= MINSIZE
              && __glibc_likely (tcache->num_slots[tc_idx] != 0))
	    return tcache_put_large (p, tc_idx);
	}

      if (__glibc_unlikely (tcache_inactive ()))
	return tcache_free_init (mem);
    }
#endif

  /* Check size >= MINSIZE and p + size does not overflow.  */
  if (__glibc_unlikely (INT_ADD_OVERFLOW ((uintptr_t) p,
					  size - MINSIZE)))
    return malloc_printerr_tail ("free(): invalid size");

  _int_free_chunk (arena_for_chunk (p), p, size);
}
libc_hidden_def (__libc_free)

void *
__libc_realloc (void *oldmem, size_t bytes)
{
  mstate ar_ptr;
  INTERNAL_SIZE_T nb;         /* padded request size */

  void *newp;             /* chunk to return */

  /* realloc of null is supposed to be same as malloc */
  if (oldmem == NULL)
    return __libc_malloc (bytes);

#if REALLOC_ZERO_BYTES_FREES
  if (bytes == 0)
    {
      __libc_free (oldmem); return NULL;
    }
#endif

  /* chunk corresponding to oldmem */
  const mchunkptr oldp = mem2chunk (oldmem);

  /* Return the chunk as is if the request grows within usable bytes, typically
     into the alignment padding.  We want to avoid reusing the block for
     shrinkages because it ends up unnecessarily fragmenting the address space.
     This is also why the heuristic misses alignment padding for THP for
     now.  */
  size_t usable = musable (oldmem);
  if (bytes <= usable)
    {
      size_t difference = usable - bytes;
      if ((unsigned long) difference < 2 * sizeof (INTERNAL_SIZE_T))
	return oldmem;
    }

  /* its size */
  const INTERNAL_SIZE_T oldsize = chunksize (oldp);

  /* Little security check which won't hurt performance: the allocator
     never wraps around at the end of the address space.  Therefore
     we can exclude some size values which might appear here by
     accident or by "design" from some intruder.  */
  if (__glibc_unlikely ((uintptr_t) oldp > (uintptr_t) -oldsize
                        || misaligned_chunk (oldp)))
      malloc_printerr ("realloc(): invalid pointer");

  if (bytes > PTRDIFF_MAX)
    {
      __set_errno (ENOMEM);
      return NULL;
    }
  nb = checked_request2size (bytes);

  if (chunk_is_mmapped (oldp))
    {
      void *newmem;

#if HAVE_MREMAP
      newp = mremap_chunk (oldp, nb);
      if (newp)
	return chunk2mem (newp);
#endif
      /* Return if shrinking and mremap was unsuccessful.  */
      if (bytes <= usable)
	return oldmem;

      /* Must alloc, copy, free. */
      newmem = __libc_malloc (bytes);
      if (newmem == NULL)
        return NULL;              /* propagate failure */

      memcpy (newmem, oldmem, oldsize - CHUNK_HDR_SZ);
      munmap_chunk (oldp);
      return newmem;
    }

  ar_ptr = arena_for_chunk (oldp);

  if (SINGLE_THREAD_P)
    {
      newp = _int_realloc (ar_ptr, oldp, oldsize, nb);
      assert (!newp || chunk_is_mmapped (mem2chunk (newp)) ||
	      ar_ptr == arena_for_chunk (mem2chunk (newp)));

      return newp;
    }

  __libc_lock_lock (ar_ptr->mutex);

  newp = _int_realloc (ar_ptr, oldp, oldsize, nb);

  __libc_lock_unlock (ar_ptr->mutex);
  assert (!newp || chunk_is_mmapped (mem2chunk (newp)) ||
          ar_ptr == arena_for_chunk (mem2chunk (newp)));

  if (newp == NULL)
    {
      /* Try harder to allocate memory in other arenas.  */
      LIBC_PROBE (memory_realloc_retry, 2, bytes, oldmem);
      newp = __libc_malloc (bytes);
      if (newp != NULL)
        {
	  memcpy (newp, oldmem, memsize (oldp));
	  _int_free_chunk (ar_ptr, oldp, chunksize (oldp));
        }
    }

  return newp;
}
libc_hidden_def (__libc_realloc)

void *
__libc_memalign (size_t alignment, size_t bytes)
{
  /* Round the alignment up to a power of 2.  Reject alignments that overflow
     when rounded up.  Zero alignment is handled by _mid_memalign.  */
  if (__glibc_unlikely (!powerof2 (alignment)))
    {
      if (alignment > SIZE_MAX / 2 + 1)
	{
	  __set_errno (EINVAL);
	  return NULL;
	}

      alignment = stdc_bit_ceil (alignment);
    }

  return _mid_memalign (alignment, bytes);
}
libc_hidden_def (__libc_memalign)

/* For ISO C17.  */
void *
weak_function
aligned_alloc (size_t alignment, size_t bytes)
{
/* Starting with ISO C17 the standard requires an error for alignments
   that are not supported.  Only integral powers of 2 are valid.  */
  if (!stdc_has_single_bit (alignment))
    {
      __set_errno (EINVAL);
      return NULL;
    }

  return _mid_memalign (alignment, bytes);
}

/* For ISO C23.  */
void
weak_function
free_sized (void *ptr, __attribute_maybe_unused__ size_t size)
{
  /* We do not perform validation that size is the same as the original
     requested size at this time. We leave that to the sanitizers.  We
     simply forward to `free`.  This allows existing malloc replacements
     to continue to work.  */

  free (ptr);
}

/* For ISO C23.  */
void
weak_function
free_aligned_sized (void *ptr, __attribute_maybe_unused__ size_t alignment,
                    __attribute_maybe_unused__ size_t size)
{
  /* We do not perform validation that size and alignment is the same as
     the original requested size and alignment at this time.  We leave that
     to the sanitizers.  We simply forward to `free`.  This allows existing
     malloc replacements to continue to work.  */

  free (ptr);
}

static void *
_mid_memalign (size_t alignment, size_t bytes)
{
  mstate ar_ptr;
  void *p;

  /* If we need less alignment than we give anyway, just relay to malloc.  */
  if (alignment <= MALLOC_ALIGNMENT)
    return __libc_malloc (bytes);

#if USE_TCACHE
  void *victim = tcache_get_align (checked_request2size (bytes), alignment);
  if (victim != NULL)
    return victim;
#endif

  if (SINGLE_THREAD_P)
    {
      p = _int_memalign (&main_arena, alignment, bytes);
      assert (!p || chunk_is_mmapped (mem2chunk (p)) ||
	      &main_arena == arena_for_chunk (mem2chunk (p)));
      return p;
    }

  arena_get (ar_ptr, bytes + alignment + MINSIZE);

  p = _int_memalign (ar_ptr, alignment, bytes);
  if (!p && ar_ptr != NULL)
    {
      LIBC_PROBE (memory_memalign_retry, 2, bytes, alignment);
      ar_ptr = arena_get_retry (ar_ptr, bytes);
      p = _int_memalign (ar_ptr, alignment, bytes);
    }

  if (ar_ptr != NULL)
    __libc_lock_unlock (ar_ptr->mutex);

  assert (!p || chunk_is_mmapped (mem2chunk (p)) ||
          ar_ptr == arena_for_chunk (mem2chunk (p)));
  return p;
}

void *
__libc_valloc (size_t bytes)
{
  return _mid_memalign (GLRO (dl_pagesize), bytes);
}

void *
__libc_pvalloc (size_t bytes)
{
  size_t pagesize = GLRO (dl_pagesize);
  size_t rounded_bytes;
  /* ALIGN_UP with overflow check.  */
  if (__glibc_unlikely (__builtin_add_overflow (bytes,
						pagesize - 1,
						&rounded_bytes)))
    {
      __set_errno (ENOMEM);
      return NULL;
    }

  return _mid_memalign (pagesize, rounded_bytes & -pagesize);
}

static void * __attribute_noinline__
__libc_calloc2 (size_t sz)
{
  mstate av;
  mchunkptr oldtop, p;
  INTERNAL_SIZE_T oldtopsize, csz;
  void *mem;

  if (SINGLE_THREAD_P)
    av = &main_arena;
  else
    arena_get (av, sz);

  if (av)
    {
      /* Check if we hand out the top chunk, in which case there may be no
	 need to clear. */
#if MORECORE_CLEARS
      oldtop = top (av);
      oldtopsize = chunksize (top (av));
# if MORECORE_CLEARS < 2
      /* Only newly allocated memory is guaranteed to be cleared.  */
      if (av == &main_arena &&
	  oldtopsize < mp_.sbrk_base + av->max_system_mem - (char *) oldtop)
	oldtopsize = (mp_.sbrk_base + av->max_system_mem - (char *) oldtop);
# endif
      if (av != &main_arena)
	{
	  heap_info *heap = heap_for_ptr (oldtop);
	  if (oldtopsize < (char *) heap + heap->mprotect_size - (char *) oldtop)
	    oldtopsize = (char *) heap + heap->mprotect_size - (char *) oldtop;
	}
#endif
    }
  else
    {
      /* No usable arenas.  */
      oldtop = NULL;
      oldtopsize = 0;
    }
  mem = _int_malloc (av, sz);

  assert (!mem || chunk_is_mmapped (mem2chunk (mem)) ||
          av == arena_for_chunk (mem2chunk (mem)));

  if (!SINGLE_THREAD_P)
    {
      if (mem == NULL && av != NULL)
	{
	  LIBC_PROBE (memory_calloc_retry, 1, sz);
	  av = arena_get_retry (av, sz);
	  mem = _int_malloc (av, sz);
	}

      if (av != NULL)
	__libc_lock_unlock (av->mutex);
    }

  /* Allocation failed even after a retry.  */
  if (mem == NULL)
    return NULL;

  p = mem2chunk (mem);

  csz = chunksize (p);

  /* Two optional cases in which clearing not necessary */
  if (chunk_is_mmapped (p))
    {
      if (__glibc_unlikely (perturb_byte))
        return memset (mem, 0, sz);

      return mem;
    }

#if MORECORE_CLEARS
  if (perturb_byte == 0 && (p == oldtop && csz > oldtopsize))
    {
      /* clear only the bytes from non-freshly-sbrked memory */
      csz = oldtopsize;
    }
#endif

  return clear_memory (mem, csz - SIZE_SZ);
}

void *
__libc_calloc (size_t n, size_t elem_size)
{
  size_t bytes;

  if (__glibc_unlikely (__builtin_mul_overflow (n, elem_size, &bytes)))
    {
       __set_errno (ENOMEM);
       return NULL;
    }

#if USE_TCACHE
  size_t nb = checked_request2size (bytes);

  if (nb < mp_.tcache_max_bytes)
    {
      size_t tc_idx = csize2tidx (nb);

      if (__glibc_likely (tc_idx < TCACHE_SMALL_BINS))
	{
	  if (tcache->entries[tc_idx] != NULL)
	    return clear_memory (tcache_get (tc_idx), tidx2usize (tc_idx));
	}
      else
	{
	  tc_idx = large_csize2tidx (nb);
	  void *mem = tcache_get_large (tc_idx, nb);
	  if (mem != NULL)
	    return memset (mem, 0, memsize (mem2chunk (mem)));
	}
    }
#endif
  return __libc_calloc2 (bytes);
}
#endif /* IS_IN (libc) */

/*
   ------------------------------ malloc ------------------------------
 */

static void *
_int_malloc (mstate av, size_t bytes)
{
  INTERNAL_SIZE_T nb;               /* normalized request size */
  unsigned int idx;                 /* associated bin index */
  mbinptr bin;                      /* associated bin */

  mchunkptr victim;                 /* inspected/selected chunk */
  INTERNAL_SIZE_T size;             /* its size */
  int victim_index;                 /* its bin index */

  mchunkptr remainder;              /* remainder from a split */
  unsigned long remainder_size;     /* its size */

  unsigned int block;               /* bit map traverser */
  unsigned int bit;                 /* bit map traverser */
  unsigned int map;                 /* current word of binmap */

  mchunkptr fwd;                    /* misc temp for linking */
  mchunkptr bck;                    /* misc temp for linking */

  /*
     Convert request size to internal form by adding SIZE_SZ bytes
     overhead plus possibly more to obtain necessary alignment and/or
     to obtain a size of at least MINSIZE, the smallest allocatable
     size. Also, checked_request2size returns false for request sizes
     that are so large that they wrap around zero when padded and
     aligned.
   */

  if (bytes > PTRDIFF_MAX)
    {
      __set_errno (ENOMEM);
      return NULL;
    }
  nb = checked_request2size (bytes);

  /* There are no usable arenas.  Fall back to sysmalloc to get a chunk from
     mmap.  */
  if (__glibc_unlikely (av == NULL))
    {
      void *p = sysmalloc (nb, av);
      if (p != NULL)
	alloc_perturb (p, bytes);
      return p;
    }

  /*
     If a small request, check regular bin.  Since these "smallbins"
     hold one size each, no searching within bins is necessary.
     (For a large request, we need to wait until unsorted chunks are
     processed to find best fit. But for small ones, fits are exact
     anyway, so we can check now, which is faster.)
   */

  if (in_smallbin_range (nb))
    {
      idx = smallbin_index (nb);
      bin = bin_at (av, idx);

      if ((victim = last (bin)) != bin)
        {
          bck = victim->bk;
	  if (__glibc_unlikely (bck->fd != victim))
	    malloc_printerr ("malloc(): smallbin double linked list corrupted");
          set_inuse_bit_at_offset (victim, nb);
          bin->bk = bck;
          bck->fd = bin;

          if (av != &main_arena)
	    set_non_main_arena (victim);
          check_malloced_chunk (av, victim, nb);
#if USE_TCACHE
	  /* While we're here, if we see other chunks of the same size,
	     stash them in the tcache.  */
	  size_t tc_idx = csize2tidx (nb);
	  if (tc_idx < mp_.tcache_small_bins)
	    {
	      mchunkptr tc_victim;

	      if (__glibc_unlikely (tcache_inactive ()))
		tcache_init (av);

	      /* While bin not empty and tcache not full, copy chunks over.  */
	      while (tcache->num_slots[tc_idx] != 0
		     && (tc_victim = last (bin)) != bin)
		{
		  if (tc_victim != NULL)
		    {
		      bck = tc_victim->bk;
		      set_inuse_bit_at_offset (tc_victim, nb);
		      if (av != &main_arena)
			set_non_main_arena (tc_victim);
		      bin->bk = bck;
		      bck->fd = bin;

		      tcache_put (tc_victim, tc_idx);
	            }
		}
	    }
#endif
          void *p = chunk2mem (victim);
          alloc_perturb (p, bytes);
          return p;
        }
    }

  else
    {
      idx = largebin_index (nb);
    }

  /*
     Process recently freed or remaindered chunks, taking one only if
     it is exact fit, or, if this a small request, the chunk is remainder from
     the most recent non-exact fit.  Place other traversed chunks in
     bins.  Note that this step is the only place in any routine where
     chunks are placed in bins.
   */

    {
      int iters = 0;
      while ((victim = unsorted_chunks (av)->bk) != unsorted_chunks (av))
        {
          bck = victim->bk;
          size = chunksize (victim);
          mchunkptr next = chunk_at_offset (victim, size);

          if (__glibc_unlikely (size <= CHUNK_HDR_SZ)
              || __glibc_unlikely (size > av->system_mem))
            malloc_printerr ("malloc(): invalid size (unsorted)");
          if (__glibc_unlikely (chunksize_nomask (next) < CHUNK_HDR_SZ)
              || __glibc_unlikely (chunksize_nomask (next) > av->system_mem))
            malloc_printerr ("malloc(): invalid next size (unsorted)");
          if (__glibc_unlikely ((prev_size (next) & ~(SIZE_BITS)) != size))
            malloc_printerr ("malloc(): mismatching next->prev_size (unsorted)");
          if (__glibc_unlikely (bck->fd != victim)
              || __glibc_unlikely (victim->fd != unsorted_chunks (av)))
            malloc_printerr ("malloc(): unsorted double linked list corrupted");
          if (__glibc_unlikely (prev_inuse (next)))
            malloc_printerr ("malloc(): invalid next->prev_inuse (unsorted)");

          /*
             If a small request, try to use last remainder if it is the
             only chunk in unsorted bin.  This helps promote locality for
             runs of consecutive small requests. This is the only
             exception to best-fit, and applies only when there is
             no exact fit for a small chunk.
           */

          if (in_smallbin_range (nb) &&
              bck == unsorted_chunks (av) &&
              victim == av->last_remainder &&
              (unsigned long) (size) > (unsigned long) (nb + MINSIZE))
            {
              /* split and reattach remainder */
              remainder_size = size - nb;
              remainder = chunk_at_offset (victim, nb);
              unsorted_chunks (av)->bk = unsorted_chunks (av)->fd = remainder;
              av->last_remainder = remainder;
              remainder->bk = remainder->fd = unsorted_chunks (av);
              if (!in_smallbin_range (remainder_size))
                {
                  remainder->fd_nextsize = NULL;
                  remainder->bk_nextsize = NULL;
                }

              set_head (victim, nb | PREV_INUSE |
                        (av != &main_arena ? NON_MAIN_ARENA : 0));
              set_head (remainder, remainder_size | PREV_INUSE);
              set_foot (remainder, remainder_size);

              check_malloced_chunk (av, victim, nb);
              void *p = chunk2mem (victim);
              alloc_perturb (p, bytes);
              return p;
            }

          /* remove from unsorted list */
          unsorted_chunks (av)->bk = bck;
          bck->fd = unsorted_chunks (av);

          /* Take now instead of binning if exact fit */

          if (size == nb)
            {
              set_inuse_bit_at_offset (victim, size);
              if (av != &main_arena)
		set_non_main_arena (victim);
              check_malloced_chunk (av, victim, nb);
              void *p = chunk2mem (victim);
              alloc_perturb (p, bytes);
              return p;
            }

          /* Place chunk in bin.  Only splitting can put
             small chunks into the unsorted bin. */
          if (__glibc_unlikely (in_smallbin_range (size)))
            {
              victim_index = smallbin_index (size);
              bck = bin_at (av, victim_index);
              fwd = bck->fd;
            }
          else
            {
              victim_index = largebin_index (size);
              bck = bin_at (av, victim_index);
              fwd = bck->fd;

              /* maintain large bins in sorted order */
              if (fwd != bck)
                {
                  /* Or with inuse bit to speed comparisons */
                  size |= PREV_INUSE;
                  /* if smaller than smallest, bypass loop below */
                  assert (chunk_main_arena (bck->bk));
                  if ((unsigned long) (size)
		      < (unsigned long) chunksize_nomask (bck->bk))
                    {
                      fwd = bck;
                      bck = bck->bk;

                      if (__glibc_unlikely (fwd->fd->bk_nextsize->fd_nextsize != fwd->fd))
                        malloc_printerr ("malloc(): largebin double linked list corrupted (nextsize)");

                      victim->fd_nextsize = fwd->fd;
                      victim->bk_nextsize = fwd->fd->bk_nextsize;
                      fwd->fd->bk_nextsize = victim->bk_nextsize->fd_nextsize = victim;
                    }
                  else
                    {
                      assert (chunk_main_arena (fwd));
                      while ((unsigned long) size < chunksize_nomask (fwd))
                        {
                          fwd = fwd->fd_nextsize;
			  assert (chunk_main_arena (fwd));
                        }

                      if ((unsigned long) size
			  == (unsigned long) chunksize_nomask (fwd))
                        /* Always insert in the second position.  */
                        fwd = fwd->fd;
                      else
                        {
                          victim->fd_nextsize = fwd;
                          victim->bk_nextsize = fwd->bk_nextsize;
                          if (__glibc_unlikely (fwd->bk_nextsize->fd_nextsize != fwd))
                            malloc_printerr ("malloc(): largebin double linked list corrupted (nextsize)");
                          fwd->bk_nextsize = victim;
                          victim->bk_nextsize->fd_nextsize = victim;
                        }
                      bck = fwd->bk;
                      if (bck->fd != fwd)
                        malloc_printerr ("malloc(): largebin double linked list corrupted (bk)");
                    }
                }
              else
                victim->fd_nextsize = victim->bk_nextsize = victim;
            }

          mark_bin (av, victim_index);
          victim->bk = bck;
          victim->fd = fwd;
          fwd->bk = victim;
          bck->fd = victim;

#define MAX_ITERS       10000
          if (++iters >= MAX_ITERS)
            break;
        }

      /*
         If a large request, scan through the chunks of current bin in
         sorted order to find smallest that fits.  Use the skip list for this.
       */

      if (!in_smallbin_range (nb))
        {
          bin = bin_at (av, idx);

          /* skip scan if empty or largest chunk is too small */
          if ((victim = first (bin)) != bin
	      && (unsigned long) chunksize_nomask (victim)
	        >= (unsigned long) (nb))
            {
              victim = victim->bk_nextsize;
              while (((unsigned long) (size = chunksize (victim)) <
                      (unsigned long) (nb)))
                victim = victim->bk_nextsize;

              /* Avoid removing the first entry for a size so that the skip
                 list does not have to be rerouted.  */
              if (victim != last (bin)
		  && chunksize_nomask (victim)
		    == chunksize_nomask (victim->fd))
                victim = victim->fd;

              remainder_size = size - nb;
              unlink_chunk (av, victim);

              /* Exhaust */
              if (remainder_size < MINSIZE)
                {
                  set_inuse_bit_at_offset (victim, size);
                  if (av != &main_arena)
		    set_non_main_arena (victim);
                }
              /* Split */
              else
                {
                  remainder = chunk_at_offset (victim, nb);
                  /* We cannot assume the unsorted list is empty and therefore
                     have to perform a complete insert here.  */
                  bck = unsorted_chunks (av);
                  fwd = bck->fd;
		  if (__glibc_unlikely (fwd->bk != bck))
		    malloc_printerr ("malloc(): corrupted unsorted chunks");
                  remainder->bk = bck;
                  remainder->fd = fwd;
                  bck->fd = remainder;
                  fwd->bk = remainder;
                  if (!in_smallbin_range (remainder_size))
                    {
                      remainder->fd_nextsize = NULL;
                      remainder->bk_nextsize = NULL;
                    }
                  set_head (victim, nb | PREV_INUSE |
                            (av != &main_arena ? NON_MAIN_ARENA : 0));
                  set_head (remainder, remainder_size | PREV_INUSE);
                  set_foot (remainder, remainder_size);
                }
              check_malloced_chunk (av, victim, nb);
              void *p = chunk2mem (victim);
              alloc_perturb (p, bytes);
              return p;
            }
        }

      /*
         Search for a chunk by scanning bins, starting with next largest
         bin. This search is strictly by best-fit; i.e., the smallest
         (with ties going to approximately the least recently used) chunk
         that fits is selected.

         The bitmap avoids needing to check that most blocks are nonempty.
         The particular case of skipping all bins during warm-up phases
         when no chunks have been returned yet is faster than it might look.
       */

      ++idx;
      bin = bin_at (av, idx);
      block = idx2block (idx);
      map = av->binmap[block];
      bit = idx2bit (idx);

      for (;; )
        {
          /* Skip rest of block if there are no more set bits in this block.  */
          if (bit > map || bit == 0)
            {
              do
                {
                  if (++block >= BINMAPSIZE) /* out of bins */
                    goto use_top;
                }
              while ((map = av->binmap[block]) == 0);

              bin = bin_at (av, (block << BINMAPSHIFT));
              bit = 1;
            }

          /* Advance to bin with set bit. There must be one. */
          while ((bit & map) == 0)
            {
              bin = next_bin (bin);
              bit <<= 1;
              assert (bit != 0);
            }

          /* Inspect the bin. It is likely to be non-empty */
          victim = last (bin);

          /*  If a false alarm (empty bin), clear the bit. */
          if (victim == bin)
            {
              av->binmap[block] = map &= ~bit; /* Write through */
              bin = next_bin (bin);
              bit <<= 1;
            }

          else
            {
              size = chunksize (victim);

              /*  We know the first chunk in this bin is big enough to use. */
              assert ((unsigned long) (size) >= (unsigned long) (nb));

              remainder_size = size - nb;

              /* unlink */
              unlink_chunk (av, victim);

              /* Exhaust */
              if (remainder_size < MINSIZE)
                {
                  set_inuse_bit_at_offset (victim, size);
                  if (av != &main_arena)
		    set_non_main_arena (victim);
                }

              /* Split */
              else
                {
                  remainder = chunk_at_offset (victim, nb);

                  /* We cannot assume the unsorted list is empty and therefore
                     have to perform a complete insert here.  */
                  bck = unsorted_chunks (av);
                  fwd = bck->fd;
		  if (__glibc_unlikely (fwd->bk != bck))
		    malloc_printerr ("malloc(): corrupted unsorted chunks 2");
                  remainder->bk = bck;
                  remainder->fd = fwd;
                  bck->fd = remainder;
                  fwd->bk = remainder;

                  /* advertise as last remainder */
                  if (in_smallbin_range (nb))
                    av->last_remainder = remainder;
                  if (!in_smallbin_range (remainder_size))
                    {
                      remainder->fd_nextsize = NULL;
                      remainder->bk_nextsize = NULL;
                    }
                  set_head (victim, nb | PREV_INUSE |
                            (av != &main_arena ? NON_MAIN_ARENA : 0));
                  set_head (remainder, remainder_size | PREV_INUSE);
                  set_foot (remainder, remainder_size);
                }
              check_malloced_chunk (av, victim, nb);
              void *p = chunk2mem (victim);
              alloc_perturb (p, bytes);
              return p;
            }
        }

    use_top:
      /*
         If large enough, split off the chunk bordering the end of memory
         (held in av->top). Note that this is in accord with the best-fit
         search rule.  In effect, av->top is treated as larger (and thus
         less well fitting) than any other available chunk since it can
         be extended to be as large as necessary (up to system
         limitations).

         We require that av->top always exists (i.e., has size >=
         MINSIZE) after initialization, so if it would otherwise be
         exhausted by current request, it is replenished. (The main
         reason for ensuring it exists is that we may need MINSIZE space
         to put in fenceposts in sysmalloc.)
       */

      victim = av->top;
      size = chunksize (victim);

      if (__glibc_unlikely (size > av->system_mem))
        malloc_printerr ("malloc(): corrupted top size");

      if ((unsigned long) (size) >= (unsigned long) (nb + MINSIZE))
        {
          remainder_size = size - nb;
          remainder = chunk_at_offset (victim, nb);
          av->top = remainder;
          set_head (victim, nb | PREV_INUSE |
                    (av != &main_arena ? NON_MAIN_ARENA : 0));
          set_head (remainder, remainder_size | PREV_INUSE);

          check_malloced_chunk (av, victim, nb);
          void *p = chunk2mem (victim);
          alloc_perturb (p, bytes);
          return p;
        }

      /*
         Otherwise, relay to handle system-dependent cases
       */
      else
        {
          void *p = sysmalloc (nb, av);
          if (p != NULL)
            alloc_perturb (p, bytes);
          return p;
        }
    }
}

/*
   ------------------------------ free ------------------------------
 */

/* Free chunk P of SIZE bytes to the arena AV (which is not locked).
   Caller must ensure chunk and size are valid.  */
static __attribute_maybe_unused__ void
_int_free_chunk (mstate av, mchunkptr p, INTERNAL_SIZE_T size)
{
  /*
    Consolidate other non-mmapped chunks as they arrive.
  */

  if (!chunk_is_mmapped(p)) {

    if (SINGLE_THREAD_P)
      _int_free_merge_chunk (av, p, size);
    else
      {
	__libc_lock_lock (av->mutex);
	_int_free_merge_chunk (av, p, size);
	__libc_lock_unlock (av->mutex);
      }
  }
  /*
    If the chunk was allocated via mmap, release via munmap().
  */

  else {

    /* Preserve errno in case munmap sets it.  */
    int err = errno;

    /* See if the dynamic brk/mmap threshold needs adjusting.
       Dumped fake mmapped chunks do not affect the threshold.  */
    if (!mp_.no_dyn_threshold
        && chunksize_nomask (p) > mp_.mmap_threshold
        && chunksize_nomask (p) <= DEFAULT_MMAP_THRESHOLD_MAX)
      {
        mp_.mmap_threshold = chunksize (p);
        mp_.trim_threshold = 2 * mp_.mmap_threshold;
        LIBC_PROBE (memory_mallopt_free_dyn_thresholds, 2,
		    mp_.mmap_threshold, mp_.trim_threshold);
      }

    munmap_chunk (p);

    __set_errno (err);
  }
}

/* Try to merge chunk P of SIZE bytes from locked arena AV with its neighbors.
   Put the resulting chunk on the appropriate bin list.  P must not be on a
   bin list yet, and it can be in use.  */
static void
_int_free_merge_chunk (mstate av, mchunkptr p, INTERNAL_SIZE_T size)
{
  mchunkptr nextchunk = chunk_at_offset(p, size);

  check_inuse_chunk (av, p);

  /* Lightweight tests: check whether the block is already the
     top block.  */
  if (__glibc_unlikely (p == av->top))
    malloc_printerr ("double free or corruption (top)");
  /* Or whether the next chunk is beyond the boundaries of the arena.  */
  if (__glibc_unlikely (contiguous (av)
			&& (char *) nextchunk
			>= ((char *) av->top + chunksize(av->top))))
    malloc_printerr ("double free or corruption (out)");
  /* Or whether the block is actually not marked used.  */
  if (__glibc_unlikely (!prev_inuse(nextchunk)))
    malloc_printerr ("double free or corruption (!prev)");

  INTERNAL_SIZE_T nextsize = chunksize(nextchunk);
  if (__glibc_unlikely (chunksize_nomask (nextchunk) <= CHUNK_HDR_SZ
                        || nextsize >= av->system_mem))
    malloc_printerr ("free(): invalid next size (normal)");

  free_perturb (chunk2mem(p), size - CHUNK_HDR_SZ);

  /* Consolidate backward.  */
  if (!prev_inuse(p))
    {
      INTERNAL_SIZE_T prevsize = prev_size (p);
      size += prevsize;
      p = chunk_at_offset(p, -((long) prevsize));
      if (__glibc_unlikely (chunksize(p) != prevsize))
        malloc_printerr ("corrupted size vs. prev_size while consolidating");
      unlink_chunk (av, p);
    }

  /* Write the chunk header, maybe after merging with the following chunk.  */
  size = _int_free_create_chunk (av, p, size, nextchunk, nextsize);
  _int_free_maybe_trim (av, size);
}

/* Create a chunk at P of SIZE bytes, with SIZE potentially increased
   to cover the immediately following chunk NEXTCHUNK of NEXTSIZE
   bytes (if NEXTCHUNK is unused).  The chunk at P is not actually
   read and does not have to be initialized.  After creation, it is
   placed on the appropriate bin list.  The function returns the size
   of the new chunk.  */
static INTERNAL_SIZE_T
_int_free_create_chunk (mstate av, mchunkptr p, INTERNAL_SIZE_T size,
			mchunkptr nextchunk, INTERNAL_SIZE_T nextsize)
{
  if (nextchunk != av->top)
    {
      /* get and clear inuse bit */
      bool nextinuse = inuse_bit_at_offset (nextchunk, nextsize);

      /* consolidate forward */
      if (!nextinuse) {
	unlink_chunk (av, nextchunk);
	size += nextsize;
      } else
	clear_inuse_bit_at_offset(nextchunk, 0);

      mchunkptr bck, fwd;

      if (!in_smallbin_range (size))
        {
          /* Place large chunks in unsorted chunk list.  Large chunks are
             not placed into regular bins until after they have
             been given one chance to be used in malloc.

             This branch is first in the if-statement to help branch
             prediction on consecutive adjacent frees. */
          bck = unsorted_chunks (av);
          fwd = bck->fd;
          if (__glibc_unlikely (fwd->bk != bck))
            malloc_printerr ("free(): corrupted unsorted chunks");
          p->fd_nextsize = NULL;
          p->bk_nextsize = NULL;
        }
      else
        {
          /* Place small chunks directly in their smallbin, so they
             don't pollute the unsorted bin. */
          int chunk_index = smallbin_index (size);
          bck = bin_at (av, chunk_index);
          fwd = bck->fd;

          if (__glibc_unlikely (fwd->bk != bck))
            malloc_printerr ("free(): chunks in smallbin corrupted");

          mark_bin (av, chunk_index);
        }

      p->bk = bck;
      p->fd = fwd;
      bck->fd = p;
      fwd->bk = p;

      set_head(p, size | PREV_INUSE);
      set_foot(p, size);

      check_free_chunk(av, p);
    }
  else
    {
      /* If the chunk borders the current high end of memory,
	 consolidate into top.  */
      size += nextsize;
      set_head(p, size | PREV_INUSE);
      av->top = p;
      check_chunk(av, p);
    }

  return size;
}

/* If the total unused topmost memory exceeds trim threshold, ask malloc_trim
   to reduce top.  */
static void
_int_free_maybe_trim (mstate av, INTERNAL_SIZE_T size)
{
  /* We don't want to trim on each free.  As a compromise, trimming is attempted
     if ATTEMPT_TRIMMING_THRESHOLD is reached.  */
  if (size >= ATTEMPT_TRIMMING_THRESHOLD)
    {
      /* Preserve errno.  */
      int err = errno;

      if (av == &main_arena)
	{
#ifndef MORECORE_CANNOT_TRIM
	  if (chunksize (av->top) >= mp_.trim_threshold)
	    systrim (mp_.top_pad, av);
#endif
	}
      else
	{
	  /* Always try heap_trim, even if the top chunk is not large,
	     because the corresponding heap might go away.  */
	  heap_info *heap = heap_for_ptr (top (av));

	  assert (heap->ar_ptr == av);
	  heap_trim (heap, mp_.top_pad);
	}

      __set_errno (err);
    }
}

/*
  ------------------------------ realloc ------------------------------
*/

static void *
_int_realloc (mstate av, mchunkptr oldp, INTERNAL_SIZE_T oldsize,
	     INTERNAL_SIZE_T nb)
{
  mchunkptr        newp;            /* chunk to return */
  INTERNAL_SIZE_T  newsize;         /* its size */
  void*          newmem;          /* corresponding user mem */

  mchunkptr        next;            /* next contiguous chunk after oldp */

  mchunkptr        remainder;       /* extra space at end of newp */
  unsigned long    remainder_size;  /* its size */

  /* oldmem size */
  if (__glibc_unlikely (chunksize_nomask (oldp) <= CHUNK_HDR_SZ
                        || oldsize >= av->system_mem
                        || oldsize != chunksize (oldp)))
    malloc_printerr ("realloc(): invalid old size");

  check_inuse_chunk (av, oldp);

  /* All callers already filter out mmap'ed chunks.  */
  assert (!chunk_is_mmapped (oldp));

  next = chunk_at_offset (oldp, oldsize);
  INTERNAL_SIZE_T nextsize = chunksize (next);
  if (__glibc_unlikely (chunksize_nomask (next) <= CHUNK_HDR_SZ
                        || nextsize >= av->system_mem))
    malloc_printerr ("realloc(): invalid next size");

  if ((unsigned long) (oldsize) >= (unsigned long) (nb))
    {
      /* already big enough; split below */
      newp = oldp;
      newsize = oldsize;
    }

  else
    {
      /* Try to expand forward into top */
      if (next == av->top &&
          (unsigned long) (newsize = oldsize + nextsize) >=
          (unsigned long) (nb + MINSIZE))
        {
          set_head_size (oldp, nb | (av != &main_arena ? NON_MAIN_ARENA : 0));
          av->top = chunk_at_offset (oldp, nb);
          set_head (av->top, (newsize - nb) | PREV_INUSE);
          check_inuse_chunk (av, oldp);
          return chunk2mem (oldp);
        }

      /* Try to expand forward into next chunk;  split off remainder below */
      else if (next != av->top &&
               !inuse (next) &&
               (unsigned long) (newsize = oldsize + nextsize) >=
               (unsigned long) (nb))
        {
          newp = oldp;
          unlink_chunk (av, next);
        }

      /* allocate, copy, free */
      else
        {
          newmem = _int_malloc (av, nb - MALLOC_ALIGN_MASK);
          if (newmem == NULL)
            return NULL; /* propagate failure */

          newp = mem2chunk (newmem);
          newsize = chunksize (newp);

          /*
             Avoid copy if newp is next chunk after oldp.
           */
          if (newp == next)
            {
              newsize += oldsize;
              newp = oldp;
            }
          else
            {
	      void *oldmem = chunk2mem (oldp);
	      memcpy (newmem, oldmem, memsize (oldp));
	      _int_free_merge_chunk (av, oldp, chunksize (oldp));
	      check_inuse_chunk (av, newp);
	      return newmem;
            }
        }
    }

  /* If possible, free extra space in old or extended chunk */

  assert ((unsigned long) (newsize) >= (unsigned long) (nb));

  remainder_size = newsize - nb;

  if (remainder_size < MINSIZE)   /* not enough extra to split off */
    {
      set_head_size (newp, newsize | (av != &main_arena ? NON_MAIN_ARENA : 0));
      set_inuse_bit_at_offset (newp, newsize);
    }
  else   /* split remainder */
    {
      remainder = chunk_at_offset (newp, nb);
      set_head_size (newp, nb | (av != &main_arena ? NON_MAIN_ARENA : 0));
      set_head (remainder, remainder_size | PREV_INUSE |
                (av != &main_arena ? NON_MAIN_ARENA : 0));
      /* Mark remainder as inuse so free() won't complain */
      set_inuse_bit_at_offset (remainder, remainder_size);
      _int_free_merge_chunk (av, remainder, chunksize (remainder));
    }

  check_inuse_chunk (av, newp);
  return chunk2mem (newp);
}

/*
   ------------------------------ memalign ------------------------------
 */

/* BYTES is user requested bytes, not requested chunksize bytes.
   ALIGNMENT is a power of 2 larger than or equal to MINSIZE.  */
static void *
_int_memalign (mstate av, size_t alignment, size_t bytes)
{
  mchunkptr p, newp;

  if (bytes > PTRDIFF_MAX || alignment > PTRDIFF_MAX)
    {
      __set_errno (ENOMEM);
      return NULL;
    }
  size_t nb = checked_request2size (bytes);

  /* Call malloc with worst case padding to hit alignment.  ALIGNMENT is a
     power of 2, so it tops out at (PTRDIFF_MAX >> 1) + 1, leaving plenty of
     space to add MINSIZE and whatever checked_request2size adds to BYTES to
     get NB.  Consequently, total below also does not overflow.  */
  void *m = _int_malloc (av, nb + alignment + MINSIZE);

  if (m == NULL)
    return NULL;

  p = mem2chunk (m);

  if (chunk_is_mmapped (p))
    {
      newp = mem2chunk (PTR_ALIGN_UP (m, alignment));
      p = mmap_set_chunk (mmap_base (p), mmap_size (p),
			  (uintptr_t)newp - mmap_base (p), mmap_is_hp (p));
      return chunk2mem (p);
    }

  size_t size = chunksize (p);

  /* If not already aligned, align the chunk.  Add MINSIZE before aligning
     so we can always free the alignment padding.  */
  if (!PTR_IS_ALIGNED (m, alignment))
    {
      newp = mem2chunk (ALIGN_UP ((uintptr_t)m + MINSIZE, alignment));
      size_t leadsize = PTR_DIFF (newp, p);
      size -= leadsize;

      /* Create a new chunk from the alignment padding and free it.  */
      int arena_flag = av != &main_arena ? NON_MAIN_ARENA : 0;
      set_head (newp, size | PREV_INUSE | arena_flag);
      set_inuse_bit_at_offset (newp, size);
      set_head_size (p, leadsize | arena_flag);
      _int_free_merge_chunk (av, p, leadsize);
      p = newp;
    }

  /* Free a chunk at the end if large enough.  */
  if (size - nb >= MINSIZE)
    {
      mchunkptr nextchunk = chunk_at_offset (p, size);
      mchunkptr remainder = chunk_at_offset (p, nb);
      set_head_size (p, nb);
      size = _int_free_create_chunk (av, remainder, size - nb, nextchunk,
				     chunksize (nextchunk));
      _int_free_maybe_trim (av, size);
    }

  check_inuse_chunk (av, p);
  return chunk2mem (p);
}


/*
   ------------------------------ malloc_trim ------------------------------
 */

static int
mtrim (mstate av, size_t pad)
{
  const size_t ps = GLRO (dl_pagesize);
  int psindex = bin_index (ps);
  const size_t psm1 = ps - 1;

  int result = 0;
  for (int i = 1; i < NBINS; ++i)
    if (i == 1 || i >= psindex)
      {
        mbinptr bin = bin_at (av, i);

        for (mchunkptr p = last (bin); p != bin; p = p->bk)
          {
            INTERNAL_SIZE_T size = chunksize (p);

            if (size > psm1 + sizeof (struct malloc_chunk))
              {
                /* See whether the chunk contains at least one unused page.  */
                char *paligned_mem = (char *) (((uintptr_t) p
                                                + sizeof (struct malloc_chunk)
                                                + psm1) & ~psm1);

                assert ((char *) chunk2mem (p) + 2 * CHUNK_HDR_SZ
			<= paligned_mem);
                assert ((char *) p + size > paligned_mem);

                /* This is the size we could potentially free.  */
                size -= paligned_mem - (char *) p;

                if (size > psm1)
                  {
#if MALLOC_DEBUG
                    /* When debugging we simulate destroying the memory
                       content.  */
                    memset (paligned_mem, 0x89, size & ~psm1);
#endif
                    __madvise (paligned_mem, size & ~psm1, MADV_DONTNEED);

                    result = 1;
                  }
              }
          }
      }

#ifndef MORECORE_CANNOT_TRIM
  return result | (av == &main_arena ? systrim (pad, av) : 0);

#else
  return result;
#endif
}


int
__malloc_trim (size_t s)
{
  int result = 0;

  mstate ar_ptr = &main_arena;
  do
    {
      __libc_lock_lock (ar_ptr->mutex);
      result |= mtrim (ar_ptr, s);
      __libc_lock_unlock (ar_ptr->mutex);

      ar_ptr = ar_ptr->next;
    }
  while (ar_ptr != &main_arena);

  return result;
}


/*
   ------------------------- malloc_usable_size -------------------------
 */

static size_t
musable (void *mem)
{
  mchunkptr p = mem2chunk (mem);

  if (chunk_is_mmapped (p))
    return memsize (p);
  else if (inuse (p))
    return memsize (p);

  return 0;
}

#if IS_IN (libc)
size_t
__malloc_usable_size (void *m)
{
  if (m == NULL)
    return 0;
  return musable (m);
}
#endif /* IS_IN (libc) */

/*
   ------------------------------ mallinfo ------------------------------
   Accumulate malloc statistics for arena AV into M.
 */
static void
int_mallinfo (mstate av, struct mallinfo2 *m)
{
  size_t i;
  mbinptr b;
  mchunkptr p;
  INTERNAL_SIZE_T avail;
  int nblocks;

  check_malloc_state (av);

  /* Account for top */
  avail = chunksize (av->top);
  nblocks = 1;  /* top always exists */

  /* traverse regular bins */
  for (i = 1; i < NBINS; ++i)
    {
      b = bin_at (av, i);
      for (p = last (b); p != b; p = p->bk)
        {
          ++nblocks;
          avail += chunksize (p);
        }
    }

  m->ordblks += nblocks;
  m->fordblks += avail;
  m->uordblks += av->system_mem - avail;
  m->arena += av->system_mem;
  if (av == &main_arena)
    {
      m->hblks = mp_.n_mmaps;
      m->hblkhd = mp_.mmapped_mem;
      m->usmblks = 0;
      m->keepcost = chunksize (av->top);
    }
}


struct mallinfo2
__libc_mallinfo2 (void)
{
  struct mallinfo2 m;
  mstate ar_ptr;

  memset (&m, 0, sizeof (m));
  ar_ptr = &main_arena;
  do
    {
      __libc_lock_lock (ar_ptr->mutex);
      int_mallinfo (ar_ptr, &m);
      __libc_lock_unlock (ar_ptr->mutex);

      ar_ptr = ar_ptr->next;
    }
  while (ar_ptr != &main_arena);

  return m;
}
libc_hidden_def (__libc_mallinfo2)

struct mallinfo
__libc_mallinfo (void)
{
  struct mallinfo m;
  struct mallinfo2 m2 = __libc_mallinfo2 ();

  m.arena = m2.arena;
  m.ordblks = m2.ordblks;
  m.smblks = 0;
  m.hblks = m2.hblks;
  m.hblkhd = m2.hblkhd;
  m.usmblks = m2.usmblks;
  m.fsmblks = 0;
  m.uordblks = m2.uordblks;
  m.fordblks = m2.fordblks;
  m.keepcost = m2.keepcost;

  return m;
}


/*
   ------------------------------ malloc_stats ------------------------------
 */

void
__malloc_stats (void)
{
  int i;
  mstate ar_ptr;
  unsigned int in_use_b = mp_.mmapped_mem, system_b = in_use_b;

  _IO_flockfile (stderr);
  int old_flags2 = stderr->_flags2;
  stderr->_flags2 |= _IO_FLAGS2_NOTCANCEL;
  for (i = 0, ar_ptr = &main_arena;; i++)
    {
      struct mallinfo2 mi;

      memset (&mi, 0, sizeof (mi));
      __libc_lock_lock (ar_ptr->mutex);
      int_mallinfo (ar_ptr, &mi);
      fprintf (stderr, "Arena %d:\n", i);
      fprintf (stderr, "system bytes     = %10u\n", (unsigned int) mi.arena);
      fprintf (stderr, "in use bytes     = %10u\n", (unsigned int) mi.uordblks);
#if MALLOC_DEBUG > 1
      if (i > 0)
        dump_heap (heap_for_ptr (top (ar_ptr)));
#endif
      system_b += mi.arena;
      in_use_b += mi.uordblks;
      __libc_lock_unlock (ar_ptr->mutex);
      ar_ptr = ar_ptr->next;
      if (ar_ptr == &main_arena)
        break;
    }
  fprintf (stderr, "Total (incl. mmap):\n");
  fprintf (stderr, "system bytes     = %10u\n", system_b);
  fprintf (stderr, "in use bytes     = %10u\n", in_use_b);
  fprintf (stderr, "max mmap regions = %10u\n", (unsigned int) mp_.max_n_mmaps);
  fprintf (stderr, "max mmap bytes   = %10lu\n",
           (unsigned long) mp_.max_mmapped_mem);
  stderr->_flags2 = old_flags2;
  _IO_funlockfile (stderr);
}


/*
   ------------------------------ mallopt ------------------------------
 */
static __always_inline int
do_set_trim_threshold (size_t value)
{
  LIBC_PROBE (memory_mallopt_trim_threshold, 3, value, mp_.trim_threshold,
	      mp_.no_dyn_threshold);
  mp_.trim_threshold = value;
  mp_.no_dyn_threshold = 1;
  return 1;
}

static __always_inline int
do_set_top_pad (size_t value)
{
  LIBC_PROBE (memory_mallopt_top_pad, 3, value, mp_.top_pad,
	      mp_.no_dyn_threshold);
  mp_.top_pad = value;
  mp_.no_dyn_threshold = 1;
  return 1;
}

static __always_inline int
do_set_mmap_threshold (size_t value)
{
  LIBC_PROBE (memory_mallopt_mmap_threshold, 3, value, mp_.mmap_threshold,
	      mp_.no_dyn_threshold);
  mp_.mmap_threshold = value;
  mp_.no_dyn_threshold = 1;
  return 1;
}

static __always_inline int
do_set_mmaps_max (int32_t value)
{
  LIBC_PROBE (memory_mallopt_mmap_max, 3, value, mp_.n_mmaps_max,
	      mp_.no_dyn_threshold);
  mp_.n_mmaps_max = value;
  mp_.no_dyn_threshold = 1;
  return 1;
}

static __always_inline int
do_set_mallopt_check (int32_t value)
{
  return 1;
}

static __always_inline int
do_set_perturb_byte (int32_t value)
{
  LIBC_PROBE (memory_mallopt_perturb, 2, value, perturb_byte);
  perturb_byte = value;
  return 1;
}

static __always_inline int
do_set_arena_test (size_t value)
{
  LIBC_PROBE (memory_mallopt_arena_test, 2, value, mp_.arena_test);
  mp_.arena_test = value;
  return 1;
}

static __always_inline int
do_set_arena_max (size_t value)
{
  LIBC_PROBE (memory_mallopt_arena_max, 2, value, mp_.arena_max);
  mp_.arena_max = value;
  return 1;
}

#if USE_TCACHE
static __always_inline int
do_set_tcache_max (size_t value)
{
  if (value > PTRDIFF_MAX)
    return 0;

  size_t nb = checked_request2size (value);
  size_t tc_idx = csize2tidx (nb);

  if (tc_idx >= TCACHE_SMALL_BINS)
    tc_idx = large_csize2tidx (nb);

  LIBC_PROBE (memory_tunable_tcache_max_bytes, 2, value, mp_.tcache_max_bytes);

  if (tc_idx < TCACHE_MAX_BINS)
    {
      if (tc_idx < TCACHE_SMALL_BINS)
	mp_.tcache_small_bins = tc_idx + 1;
      mp_.tcache_max_bytes = nb + 1;
      return 1;
    }

  return 0;
}

static __always_inline int
do_set_tcache_count (size_t value)
{
  if (value <= MAX_TCACHE_COUNT)
    {
      LIBC_PROBE (memory_tunable_tcache_count, 2, value, mp_.tcache_count);
      mp_.tcache_count = value;
      return 1;
    }
  return 0;
}

#endif

static __always_inline int
do_set_mxfast (size_t value)
{
  return 1;
}

#ifdef HAVE_THP
static __always_inline enum thp_mode_t
get_dl_thp_mode (void)
{
  return GL(dl_thp_mode);
}

static __always_inline unsigned long int
get_dl_elf_thp_pagesize (void)
{
  return GL(dl_elf_thp_pagesize);
}
#else
# define get_dl_thp_mode()		__get_thp_mode ()
# define get_dl_elf_thp_pagesize()	__get_thp_size ()
#endif

static __always_inline int
do_set_hugetlb (size_t value)
{
  /* If __get_thp_mode and __get_thp_size have been called during
     startup, don't call them again here.  */
  enum thp_mode_t thp_mode = get_dl_thp_mode ();

  /* Enable THP if MALLOC_DEFAULT_THP_PAGESIZE is non-zero.  */
  if (MALLOC_DEFAULT_THP_PAGESIZE > 0)
    {
      /* If thp_mode is unknown, THP segment load is disabled by
	 GLIBC_TUNABLES=glibc.elf.thp=0.  In this case, set
	 mp_.thp_mode to madvise.  Otherwise, set it to thp_mode to
	 keep mp_.thp_mode in sync with GL(dl_thp_mode).  */
      if (thp_mode == thp_mode_unknown)
	mp_.thp_mode = thp_mode_madvise;
      else
	mp_.thp_mode = thp_mode;
      mp_.thp_pagesize = MALLOC_DEFAULT_THP_PAGESIZE;
    }

  if (value == 0)
    {
      /* Turn off THP support completely.  */
      mp_.thp_mode = thp_mode_never;
      mp_.thp_pagesize = 0;
    }
  else if (value == 1)
    {
      /* Avoid querying the THP page size/mode since accessing /sys/kernel/mm
	 is relatively slow and might not be accessible in containers.  */
      if (MALLOC_DEFAULT_THP_PAGESIZE > 0)
	return 0;

      if (thp_mode == thp_mode_unknown)
	{
	  /* Call __get_thp_mode and __get_thp_size when THP segment load
	     is disabled.  */
	  mp_.thp_mode = __get_thp_mode ();
	  if (mp_.thp_mode == thp_mode_madvise
	      || mp_.thp_mode == thp_mode_always)
	    mp_.thp_pagesize = __get_thp_size ();
	}
      else
	{
	  /* THP segment load is enabled.  GL(dl_elf_thp_pagesize) is
	     set to DL_MAP_DEFAULT_THP_PAGESIZE if it isn't zero.  In
	     this case, call get_capped_thp_size () instead of using
	     DL_MAP_DEFAULT_THP_PAGESIZE for malloc.  */
	  mp_.thp_mode = thp_mode;
	  if (DL_MAP_DEFAULT_THP_PAGESIZE != 0)
	    mp_.thp_pagesize = get_capped_thp_size ();
	  else
	    mp_.thp_pagesize = get_dl_elf_thp_pagesize ();
	}
    }
  else if (value >= 2)
    __get_hugepage_config (value == 2 ? 0 : value, &mp_.hp_pagesize,
			      &mp_.hp_flags);
  return 0;
}

int
__libc_mallopt (int param_number, int value)
{
  mstate av = &main_arena;
  int res = 1;

  __libc_lock_lock (av->mutex);

  LIBC_PROBE (memory_mallopt, 2, param_number, value);

  /* Many of these helper functions take a size_t.  We do not worry
     about overflow here, because negative int values will wrap to
     very large size_t values and the helpers have sufficient range
     checking for such conversions.  Many of these helpers are also
     used by the tunables macros in arena.c.  */

  switch (param_number)
    {
    case M_MXFAST:
      res = do_set_mxfast (value);
      break;

    case M_TRIM_THRESHOLD:
      res = do_set_trim_threshold (value);
      break;

    case M_TOP_PAD:
      res = do_set_top_pad (value);
      break;

    case M_MMAP_THRESHOLD:
      res = do_set_mmap_threshold (value);
      break;

    case M_MMAP_MAX:
      res = do_set_mmaps_max (value);
      break;

    case M_CHECK_ACTION:
      res = do_set_mallopt_check (value);
      break;

    case M_PERTURB:
      res = do_set_perturb_byte (value);
      break;

    case M_ARENA_TEST:
      if (value > 0)
	res = do_set_arena_test (value);
      break;

    case M_ARENA_MAX:
      if (value > 0)
	res = do_set_arena_max (value);
      break;
    }
  __libc_lock_unlock (av->mutex);
  return res;
}
libc_hidden_def (__libc_mallopt)


/*
   -------------------- Alternative MORECORE functions --------------------
 */


/*
   General Requirements for MORECORE.

   The MORECORE function must have the following properties:

   If MORECORE_CONTIGUOUS is false:

 * MORECORE must allocate in multiples of pagesize. It will
      only be called with arguments that are multiples of pagesize.

 * MORECORE(0) must return an address that is at least
      MALLOC_ALIGNMENT aligned. (Page-aligning always suffices.)

   else (i.e. If MORECORE_CONTIGUOUS is true):

 * Consecutive calls to MORECORE with positive arguments
      return increasing addresses, indicating that space has been
      contiguously extended.

 * MORECORE need not allocate in multiples of pagesize.
      Calls to MORECORE need not have args of multiples of pagesize.

 * MORECORE need not page-align.

   In either case:

 * MORECORE may allocate more memory than requested. (Or even less,
      but this will generally result in a malloc failure.)

 * MORECORE must not allocate memory when given argument zero, but
      instead return one past the end address of memory from previous
      nonzero call. This malloc does NOT call MORECORE(0)
      until at least one call with positive arguments is made, so
      the initial value returned is not important.

 * Even though consecutive calls to MORECORE need not return contiguous
      addresses, it must be OK for malloc'ed chunks to span multiple
      regions in those cases where they do happen to be contiguous.

 * MORECORE need not handle negative arguments -- it may instead
      just return MORECORE_FAILURE when given negative arguments.
      Negative arguments are always multiples of pagesize. MORECORE
      must not misinterpret negative args as large positive unsigned
      args. You can suppress all such calls from even occurring by defining
      MORECORE_CANNOT_TRIM,

   There is some variation across systems about the type of the
   argument to sbrk/MORECORE. If size_t is unsigned, then it cannot
   actually be size_t, because sbrk supports negative args, so it is
   normally the signed type of the same width as size_t (sometimes
   declared as "intptr_t", and sometimes "ptrdiff_t").  It doesn't much
   matter though. Internally, we use "long" as arguments, which should
   work across all reasonable possibilities.

   Additionally, if MORECORE ever returns failure for a positive
   request, then mmap is used as a noncontiguous system allocator. This
   is a useful backup strategy for systems with holes in address spaces
   -- in this case sbrk cannot contiguously expand the heap, but mmap
   may be able to map noncontiguous space.

   If you'd like mmap to ALWAYS be used, you can define MORECORE to be
   a function that always returns MORECORE_FAILURE.

   If you are using this malloc with something other than sbrk (or its
   emulation) to supply memory regions, you probably want to set
   MORECORE_CONTIGUOUS as false.  As an example, here is a custom
   allocator kindly contributed for pre-OSX macOS.  It uses virtually
   but not necessarily physically contiguous non-paged memory (locked
   in, present and won't get swapped out).  You can use it by
   uncommenting this section, adding some #includes, and setting up the
   appropriate defines above:

 *#define MORECORE osMoreCore
 *#define MORECORE_CONTIGUOUS 0

   There is also a shutdown routine that should somehow be called for
   cleanup upon program exit.

 *#define MAX_POOL_ENTRIES 100
 *#define MINIMUM_MORECORE_SIZE  (64 * 1024)
   static int next_os_pool;
   void *our_os_pools[MAX_POOL_ENTRIES];

   void *osMoreCore(int size)
   {
    void *ptr = 0;
    static void *sbrk_top = 0;

    if (size > 0)
    {
      if (size < MINIMUM_MORECORE_SIZE)
         size = MINIMUM_MORECORE_SIZE;
      if (CurrentExecutionLevel() == kTaskLevel)
         ptr = PoolAllocateResident(size + RM_PAGE_SIZE, 0);
      if (ptr == 0)
      {
        return (void *) MORECORE_FAILURE;
      }
      // save ptrs so they can be freed during cleanup
      our_os_pools[next_os_pool] = ptr;
      next_os_pool++;
      ptr = (void *) ((((unsigned long) ptr) + RM_PAGE_MASK) & ~RM_PAGE_MASK);
      sbrk_top = (char *) ptr + size;
      return ptr;
    }
    else if (size < 0)
    {
      // we don't currently support shrink behavior
      return (void *) MORECORE_FAILURE;
    }
    else
    {
      return sbrk_top;
    }
   }

   // cleanup any allocated memory pools
   // called as last thing before shutting down driver

   void osCleanupMem(void)
   {
    void **ptr;

    for (ptr = our_os_pools; ptr < &our_os_pools[MAX_POOL_ENTRIES]; ptr++)
      if (*ptr)
      {
         PoolDeallocate(*ptr);
 * ptr = 0;
      }
   }

 */


/* Helper code.  */

extern char **__libc_argv attribute_hidden;

static void
malloc_printerr (const char *str)
{
#if IS_IN (libc)
  __libc_message ("%s\n", str);
#else
  __libc_fatal (str);
#endif
  __builtin_unreachable ();
}

#if USE_TCACHE

static volatile int dummy_var;

static __attribute_noinline__ void
malloc_printerr_tail (const char *str)
{
  /* Ensure this cannot be a no-return function.  */
  if (dummy_var)
    return;
  malloc_printerr (str);
}
#endif /* USE_TCACHE */

#if IS_IN (libc)
/* We need a wrapper function for one of the additions of POSIX.  */
int
__posix_memalign (void **memptr, size_t alignment, size_t size)
{
  /* Test whether the SIZE argument is valid.  It must be a power of
     two multiple of sizeof (void *) (which must be either 4 or 8).  */
  if (alignment < sizeof (void *) || !powerof2 (alignment))
    return EINVAL;

  void *mem = _mid_memalign (alignment, size);

  if (mem == NULL)
    return ENOMEM;

  *memptr = mem;
  return 0;
}
#endif /* IS_IN (libc) */


int
__malloc_info (int options, FILE *fp)
{
  /* For now, at least.  */
  if (options != 0)
    return EINVAL;

  int n = 0;
  size_t total_nblocks = 0;
  size_t total_avail = 0;
  size_t total_system = 0;
  size_t total_max_system = 0;
  size_t total_aspace = 0;
  size_t total_aspace_mprotect = 0;

  fputs ("<malloc version=\"1\">\n", fp);

  /* Iterate over all arenas currently in use.  */
  mstate ar_ptr = &main_arena;
  do
    {
      fprintf (fp, "<heap nr=\"%d\">\n<sizes>\n", n++);

      size_t nblocks = 0;
      size_t avail = 0;
      struct
      {
	size_t from;
	size_t to;
	size_t total;
	size_t count;
      } sizes[NBINS - 1];
#define nsizes (sizeof (sizes) / sizeof (sizes[0]))

      __libc_lock_lock (ar_ptr->mutex);

      /* Account for top chunk.  The top-most available chunk is
	 treated specially and is never in any bin. See "initial_top"
	 comments.  */
      avail = chunksize (ar_ptr->top);
      nblocks = 1;  /* Top always exists.  */

      mbinptr bin;
      struct malloc_chunk *r;

      for (size_t i = 1; i < NBINS; ++i)
	{
	  bin = bin_at (ar_ptr, i);
	  r = bin->fd;
	  sizes[i - 1].from = ~((size_t) 0);
	  sizes[i - 1].to = sizes[i - 1].total
					  = sizes[i - 1].count = 0;

	  if (r != NULL)
	    while (r != bin)
	      {
		size_t r_size = chunksize_nomask (r);
		++sizes[i - 1].count;
		sizes[i - 1].total += r_size;
		sizes[i - 1].from
		  = MIN (sizes[i - 1].from, r_size);
		sizes[i - 1].to = MAX (sizes[i - 1].to,
						   r_size);

		r = r->fd;
	      }

	  if (sizes[i - 1].count == 0)
	    sizes[i - 1].from = 0;
	  nblocks += sizes[i - 1].count;
	  avail += sizes[i - 1].total;
	}

      size_t heap_size = 0;
      size_t heap_mprotect_size = 0;
      size_t heap_count = 0;
      if (ar_ptr != &main_arena)
	{
	  /* Iterate over the arena heaps from back to front.  */
	  heap_info *heap = heap_for_ptr (top (ar_ptr));
	  do
	    {
	      heap_size += heap->size;
	      heap_mprotect_size += heap->mprotect_size;
	      heap = heap->prev;
	      ++heap_count;
	    }
	  while (heap != NULL);
	}

      __libc_lock_unlock (ar_ptr->mutex);

      total_nblocks += nblocks;
      total_avail += avail;

      for (size_t i = 1; i < nsizes; ++i)
	if (sizes[i].count != 0)
	  fprintf (fp, "\
  <size from=\"%zu\" to=\"%zu\" total=\"%zu\" count=\"%zu\"/>\n",
		   sizes[i].from, sizes[i].to, sizes[i].total, sizes[i].count);

      if (sizes[0].count != 0)
	fprintf (fp, "\
  <unsorted from=\"%zu\" to=\"%zu\" total=\"%zu\" count=\"%zu\"/>\n",
		 sizes[0].from, sizes[0].to,
		 sizes[0].total, sizes[0].count);

      total_system += ar_ptr->system_mem;
      total_max_system += ar_ptr->max_system_mem;

      fprintf (fp,
	       "</sizes>\n"
	       "<total type=\"rest\" count=\"%zu\" size=\"%zu\"/>\n"
	       "<system type=\"current\" size=\"%zu\"/>\n"
	       "<system type=\"max\" size=\"%zu\"/>\n",
	        nblocks, avail, ar_ptr->system_mem, ar_ptr->max_system_mem);

      if (ar_ptr != &main_arena)
	{
	  fprintf (fp,
		   "<aspace type=\"total\" size=\"%zu\"/>\n"
		   "<aspace type=\"mprotect\" size=\"%zu\"/>\n"
		   "<aspace type=\"subheaps\" size=\"%zu\"/>\n",
		   heap_size, heap_mprotect_size, heap_count);
	  total_aspace += heap_size;
	  total_aspace_mprotect += heap_mprotect_size;
	}
      else
	{
	  fprintf (fp,
		   "<aspace type=\"total\" size=\"%zu\"/>\n"
		   "<aspace type=\"mprotect\" size=\"%zu\"/>\n",
		   ar_ptr->system_mem, ar_ptr->system_mem);
	  total_aspace += ar_ptr->system_mem;
	  total_aspace_mprotect += ar_ptr->system_mem;
	}

      fputs ("</heap>\n", fp);
      ar_ptr = ar_ptr->next;
    }
  while (ar_ptr != &main_arena);

  fprintf (fp,
	   "<total type=\"rest\" count=\"%zu\" size=\"%zu\"/>\n"
	   "<total type=\"mmap\" count=\"%d\" size=\"%zu\"/>\n"
	   "<system type=\"current\" size=\"%zu\"/>\n"
	   "<system type=\"max\" size=\"%zu\"/>\n"
	   "<aspace type=\"total\" size=\"%zu\"/>\n"
	   "<aspace type=\"mprotect\" size=\"%zu\"/>\n"
	   "</malloc>\n",
	   total_nblocks, total_avail,
	   mp_.n_mmaps, mp_.mmapped_mem,
	   total_system, total_max_system,
	   total_aspace, total_aspace_mprotect);

  return 0;
}

#if IS_IN (libc)
strong_alias (__libc_malloc, malloc)
strong_alias (__libc_realloc, realloc)
strong_alias (__libc_free, free)
weak_alias (__libc_calloc, calloc)
weak_alias (__libc_memalign, memalign)
weak_alias (__posix_memalign, posix_memalign)
weak_alias (__libc_valloc, valloc)
weak_alias (__libc_pvalloc, pvalloc)
weak_alias (__malloc_usable_size, malloc_usable_size)

weak_alias (__malloc_info, malloc_info)
weak_alias (__libc_mallinfo, mallinfo)
weak_alias (__libc_mallinfo2, mallinfo2)
weak_alias (__libc_mallopt, mallopt)
weak_alias (__malloc_stats, malloc_stats)
weak_alias (__malloc_trim, malloc_trim)
#endif /* IS_IN (libc) */

#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_26)
compat_symbol (libc, __libc_free, cfree, GLIBC_2_0);
#endif

/* ------------------------------------------------------------
   History:

   [see ftp://g.oswego.edu/pub/misc/malloc.c for the history of dlmalloc]

 */
/*
 * Local variables:
 * c-basic-offset: 2
 * End:
 */
