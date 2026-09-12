/* Malloc debug DSO.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <atomic.h>
#include <libc-symbols.h>
#include <shlib-compat.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

/* Support only the glibc allocators.  */
extern void *__libc_malloc (size_t);
extern void __libc_free (void *);
extern void *__libc_realloc (void *, size_t);
extern void *__libc_memalign (size_t, size_t);
extern void *__libc_valloc (size_t);
extern void *__libc_pvalloc (size_t);
extern void *__libc_calloc (size_t, size_t);

#define DEBUG_FN(fn) \
  static __typeof (__libc_ ## fn) __debug_ ## fn

DEBUG_FN(malloc);
DEBUG_FN(free);
DEBUG_FN(realloc);
DEBUG_FN(memalign);
DEBUG_FN(valloc);
DEBUG_FN(pvalloc);
DEBUG_FN(calloc);

static int debug_initialized = -1;

enum malloc_debug_hooks
{
  MALLOC_NONE_HOOK = 0,
  MALLOC_MCHECK_HOOK = 1 << 0, /* mcheck()  */
  MALLOC_MTRACE_HOOK = 1 << 1, /* mtrace()  */
  MALLOC_CHECK_HOOK = 1 << 2,  /* MALLOC_CHECK_ or glibc.malloc.check.  */
};
static unsigned __malloc_debugging_hooks;

static __always_inline bool
__is_malloc_debug_enabled (enum malloc_debug_hooks flag)
{
  return __malloc_debugging_hooks & flag;
}

static __always_inline void
__malloc_debug_enable (enum malloc_debug_hooks flag)
{
  __malloc_debugging_hooks |= flag;
}

static __always_inline void
__malloc_debug_disable (enum malloc_debug_hooks flag)
{
  __malloc_debugging_hooks &= ~flag;
}

#include "mcheck.c"
#include "mtrace.c"
#include "malloc-check.c"

#if SHLIB_COMPAT (libc_malloc_debug, GLIBC_2_0, GLIBC_2_24)
extern void (*__malloc_initialize_hook) (void);
compat_symbol_reference (libc, __malloc_initialize_hook,
			 __malloc_initialize_hook, GLIBC_2_0);
#endif

static void *malloc_hook_ini (size_t, const void *) __THROW;
static void *realloc_hook_ini (void *, size_t, const void *) __THROW;
static void *memalign_hook_ini (size_t, size_t, const void *) __THROW;

void (*__free_hook) (void *, const void *) = NULL;
void *(*__malloc_hook) (size_t, const void *) = malloc_hook_ini;
void *(*__realloc_hook) (void *, size_t, const void *) = realloc_hook_ini;
void *(*__memalign_hook) (size_t, size_t, const void *) = memalign_hook_ini;

/* Hooks for debugging versions.  The initial hooks just call the
   initialization routine, then do the normal work. */

/* These hooks will get executed only through the interposed allocator
   functions in libc_malloc_debug.so.  This means that the calls to malloc,
   realloc, etc. will lead back into the interposed functions, which is what we
   want.

   These initial hooks are assumed to be called in a single-threaded context,
   so it is safe to reset all hooks at once upon initialization.  */

static void
generic_hook_ini (void)
{
  debug_initialized = 0;
  __malloc_hook = NULL;
  __realloc_hook = NULL;
  __memalign_hook = NULL;

  /* malloc check does not quite co-exist with libc malloc, so initialize
     either on or the other.  */
  if (!initialize_malloc_check ())
    /* The compiler does not know that these functions are allocators, so it
       will not try to optimize it away.  */
    __libc_free (__libc_malloc (0));

#if SHLIB_COMPAT (libc_malloc_debug, GLIBC_2_0, GLIBC_2_24)
  void (*hook) (void) = __malloc_initialize_hook;
  if (hook != NULL)
    (*hook)();
#endif

  debug_initialized = 1;
}

static void *
malloc_hook_ini (size_t sz, const void *caller)
{
  generic_hook_ini ();
  return __debug_malloc (sz);
}

static void *
realloc_hook_ini (void *ptr, size_t sz, const void *caller)
{
  generic_hook_ini ();
  return __debug_realloc (ptr, sz);
}

static void *
memalign_hook_ini (size_t alignment, size_t sz, const void *caller)
{
  generic_hook_ini ();
  return __debug_memalign (alignment, sz);
}

static size_t pagesize;

/* The allocator functions.  */

static void *
__debug_malloc (size_t bytes)
{
  void *(*hook) (size_t, const void *) = __malloc_hook;
  if (__glibc_unlikely (hook != NULL))
    return (*hook)(bytes, RETURN_ADDRESS (0));

  void *victim = NULL;
  size_t orig_bytes = bytes;
  if ((!__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK)
       || !malloc_mcheck_before (&bytes, &victim)))
    {
      victim = (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK)
		? malloc_check (bytes) : __libc_malloc (bytes));
    }
  if (__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK) && victim != NULL)
    victim = malloc_mcheck_after (victim, orig_bytes);
  if (__is_malloc_debug_enabled (MALLOC_MTRACE_HOOK))
    malloc_mtrace_after (victim, orig_bytes, RETURN_ADDRESS (0));

  return victim;
}
strong_alias (__debug_malloc, malloc)

static void
__debug_free (void *mem)
{
  void (*hook) (void *, const void *) = __free_hook;
  if (__glibc_unlikely (hook != NULL))
    {
      (*hook)(mem, RETURN_ADDRESS (0));
      return;
    }

  if (__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK))
    mem = free_mcheck (mem);

  if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
    free_check (mem);
  else
    __libc_free (mem);
  if (__is_malloc_debug_enabled (MALLOC_MTRACE_HOOK))
    free_mtrace (mem, RETURN_ADDRESS (0));
}
strong_alias (__debug_free, free)

static void
__debug_free_sized (void *mem, __attribute_maybe_unused__ size_t size)
{
  free (mem);
}
strong_alias (__debug_free_sized, free_sized)

static void
__debug_free_aligned_sized (void *mem, __attribute_maybe_unused__ size_t alignment,
                            __attribute_maybe_unused__ size_t size)
{
  free (mem);
}
strong_alias (__debug_free_aligned_sized, free_aligned_sized)

static void *
__debug_realloc (void *oldmem, size_t bytes)
{
  void *(*hook) (void *, size_t, const void *) = __realloc_hook;
  if (__glibc_unlikely (hook != NULL))
    return (*hook)(oldmem, bytes, RETURN_ADDRESS (0));

  size_t orig_bytes = bytes, oldsize = 0;
  void *victim = NULL;

  if ((!__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK)
       || !realloc_mcheck_before (&oldmem, &bytes, &oldsize, &victim)))
    {
      if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
	victim =  realloc_check (oldmem, bytes);
      else
	victim = __libc_realloc (oldmem, bytes);
    }
  if (__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK) && victim != NULL)
    victim = realloc_mcheck_after (victim, oldmem, orig_bytes,
				   oldsize);
  if (__is_malloc_debug_enabled (MALLOC_MTRACE_HOOK))
    realloc_mtrace_after (victim, oldmem, orig_bytes, RETURN_ADDRESS (0));

  return victim;
}
strong_alias (__debug_realloc, realloc)

static void *
_debug_mid_memalign (size_t alignment, size_t bytes, const void *address)
{
  void *(*hook) (size_t, size_t, const void *) = __memalign_hook;
  if (__glibc_unlikely (hook != NULL))
    return (*hook)(alignment, bytes, address);

  void *victim = NULL;
  size_t orig_bytes = bytes;

  if ((!__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK)
       || !memalign_mcheck_before (alignment, &bytes, &victim)))
    {
      victim = (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK)
		? memalign_check (alignment, bytes)
		: __libc_memalign (alignment, bytes));
    }
  if (__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK) && victim != NULL)
    victim = memalign_mcheck_after (victim, alignment, orig_bytes);
  if (__is_malloc_debug_enabled (MALLOC_MTRACE_HOOK))
    memalign_mtrace_after (victim, orig_bytes, address);

  return victim;
}

static void *
__debug_memalign (size_t alignment, size_t bytes)
{
  /* Round the alignment up to a power of 2.  Reject alignments that overflow
     when rounded up.  Zero alignment is accepted and handled later.  */
  if (alignment > SIZE_MAX / 2 + 1)
    {
      errno = EINVAL;
      return NULL;
    }

  alignment = stdc_bit_ceil (alignment);
  return _debug_mid_memalign (alignment, bytes, RETURN_ADDRESS (0));
}
strong_alias (__debug_memalign, memalign)
static void *
__debug_aligned_alloc (size_t alignment, size_t bytes)
{
  if (!powerof2 (alignment) || alignment == 0)
    return NULL;
  return _debug_mid_memalign (alignment, bytes, RETURN_ADDRESS (0));
}
strong_alias (__debug_aligned_alloc, aligned_alloc)

static void *
__debug_pvalloc (size_t bytes)
{
  size_t rounded_bytes;

  if (!pagesize)
    pagesize = sysconf (_SC_PAGESIZE);

  /* ALIGN_UP with overflow check.  */
  if (__glibc_unlikely (__builtin_add_overflow (bytes,
						pagesize - 1,
						&rounded_bytes)))
    {
      errno = ENOMEM;
      return NULL;
    }
  rounded_bytes = rounded_bytes & -(pagesize - 1);

  return _debug_mid_memalign (pagesize, rounded_bytes, RETURN_ADDRESS (0));
}
strong_alias (__debug_pvalloc, pvalloc)

static void *
__debug_valloc (size_t bytes)
{
  if (!pagesize)
    pagesize = sysconf (_SC_PAGESIZE);

  return _debug_mid_memalign (pagesize, bytes, RETURN_ADDRESS (0));
}
strong_alias (__debug_valloc, valloc)

static int
__debug_posix_memalign (void **memptr, size_t alignment, size_t bytes)
{
  /* Test whether the SIZE argument is valid.  It must be a power of
     two multiple of sizeof (void *).  */
  if (alignment % sizeof (void *) != 0
      || !powerof2 (alignment / sizeof (void *))
      || alignment == 0)
    return EINVAL;

  *memptr = _debug_mid_memalign (alignment, bytes, RETURN_ADDRESS (0));

  if (*memptr == NULL)
    return ENOMEM;

  return 0;
}
strong_alias (__debug_posix_memalign, posix_memalign)

static void *
__debug_calloc (size_t nmemb, size_t size)
{
  size_t bytes;

  if (__glibc_unlikely (__builtin_mul_overflow (nmemb, size, &bytes)))
    {
      errno = ENOMEM;
      return NULL;
    }

  void *(*hook) (size_t, const void *) = __malloc_hook;
  if (__glibc_unlikely (hook != NULL))
    {
      void *mem = (*hook)(bytes, RETURN_ADDRESS (0));

      if (mem != NULL)
	memset (mem, 0, bytes);

      return mem;
    }

  size_t orig_bytes = bytes;
  void *victim = NULL;

  if ((!__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK)
       || !malloc_mcheck_before (&bytes, &victim)))
    {
      victim = (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK)
		? malloc_check (bytes) : __libc_malloc (bytes));
    }
  if (victim != NULL)
    {
      if (__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK))
	victim = malloc_mcheck_after (victim, orig_bytes);
      memset (victim, 0, orig_bytes);
    }
  if (__is_malloc_debug_enabled (MALLOC_MTRACE_HOOK))
    malloc_mtrace_after (victim, orig_bytes, RETURN_ADDRESS (0));

  return victim;
}
strong_alias (__debug_calloc, calloc)

size_t
malloc_usable_size (void *mem)
{
  if (mem == NULL)
    return 0;

  if (__is_malloc_debug_enabled (MALLOC_MCHECK_HOOK))
    return mcheck_usable_size (mem);
  if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
    return malloc_check_get_size (mem);

  return musable (mem);
}

#define LIBC_SYMBOL(sym) libc_ ## sym
#define SYMHANDLE(sym) sym ## _handle

#define LOAD_SYM(sym) ({ \
  static void *SYMHANDLE (sym);						      \
  if (SYMHANDLE (sym) == NULL)						      \
    SYMHANDLE (sym) = dlsym (RTLD_NEXT, #sym);				      \
  SYMHANDLE (sym);							      \
})

int
malloc_info (int options, FILE *fp)
{
  if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
    return __malloc_info (options, fp);

  int (*LIBC_SYMBOL (malloc_info)) (int, FILE *) = LOAD_SYM (malloc_info);
  if (LIBC_SYMBOL (malloc_info) == NULL)
    return -1;

  return LIBC_SYMBOL (malloc_info) (options, fp);
}

int
mallopt (int param_number, int value)
{
  if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
    return __libc_mallopt (param_number, value);

  int (*LIBC_SYMBOL (mallopt)) (int, int) = LOAD_SYM (mallopt);
  if (LIBC_SYMBOL (mallopt) == NULL)
    return 0;

  return LIBC_SYMBOL (mallopt) (param_number, value);
}

void
malloc_stats (void)
{
  if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
    return __malloc_stats ();

  void (*LIBC_SYMBOL (malloc_stats)) (void) = LOAD_SYM (malloc_stats);
  if (LIBC_SYMBOL (malloc_stats) == NULL)
    return;

  LIBC_SYMBOL (malloc_stats) ();
}

struct mallinfo2
mallinfo2 (void)
{
  if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
    return __libc_mallinfo2 ();

  struct mallinfo2 (*LIBC_SYMBOL (mallinfo2)) (void) = LOAD_SYM (mallinfo2);
  if (LIBC_SYMBOL (mallinfo2) == NULL)
    {
      struct mallinfo2 ret = {0};
      return ret;
    }

  return LIBC_SYMBOL (mallinfo2) ();
}

struct mallinfo
mallinfo (void)
{
  if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
    return __libc_mallinfo ();

  struct mallinfo (*LIBC_SYMBOL (mallinfo)) (void) = LOAD_SYM (mallinfo);
  if (LIBC_SYMBOL (mallinfo) == NULL)
    {
      struct mallinfo ret = {0};
      return ret;
    }

  return LIBC_SYMBOL (mallinfo) ();
}

int
malloc_trim (size_t s)
{
  if (__is_malloc_debug_enabled (MALLOC_CHECK_HOOK))
    return __malloc_trim (s);

  int (*LIBC_SYMBOL (malloc_trim)) (size_t) = LOAD_SYM (malloc_trim);
  if (LIBC_SYMBOL (malloc_trim) == NULL)
    return 0;

  return LIBC_SYMBOL (malloc_trim) (s);
}

#if SHLIB_COMPAT (libc_malloc_debug, GLIBC_2_0, GLIBC_2_25)

/* Support for saving/restoring dumped heaps in old GLIBCs is no
   longer implemented - instead we provide dummy implementations
   which always fail.  We need to provide these symbol so that
   existing Emacs binaries continue to work with BIND_NOW.  */
void *
malloc_get_state (void)
{
  __set_errno (ENOSYS);
  return NULL;
}
compat_symbol (libc_malloc_debug, malloc_get_state, malloc_get_state,
	       GLIBC_2_0);

int
malloc_set_state (void *msptr)
{
  return -1;
}
compat_symbol (libc_malloc_debug, malloc_set_state, malloc_set_state,
	       GLIBC_2_0);
#endif

/* Do not allow linking against the library.  */
compat_symbol (libc_malloc_debug, aligned_alloc, aligned_alloc, GLIBC_2_16);
compat_symbol (libc_malloc_debug, calloc, calloc, GLIBC_2_0);
compat_symbol (libc_malloc_debug, free, free, GLIBC_2_0);
compat_symbol (libc_malloc_debug, free_aligned_sized, free_aligned_sized, GLIBC_2_43);
compat_symbol (libc_malloc_debug, free_sized, free_sized, GLIBC_2_43);
compat_symbol (libc_malloc_debug, mallinfo2, mallinfo2, GLIBC_2_33);
compat_symbol (libc_malloc_debug, mallinfo, mallinfo, GLIBC_2_0);
compat_symbol (libc_malloc_debug, malloc_info, malloc_info, GLIBC_2_10);
compat_symbol (libc_malloc_debug, malloc, malloc, GLIBC_2_0);
compat_symbol (libc_malloc_debug, malloc_stats, malloc_stats, GLIBC_2_0);
compat_symbol (libc_malloc_debug, malloc_trim, malloc_trim, GLIBC_2_0);
compat_symbol (libc_malloc_debug, malloc_usable_size, malloc_usable_size,
	       GLIBC_2_0);
compat_symbol (libc_malloc_debug, mallopt, mallopt, GLIBC_2_0);
compat_symbol (libc_malloc_debug, mcheck_check_all, mcheck_check_all,
	       GLIBC_2_2);
compat_symbol (libc_malloc_debug, mcheck, mcheck, GLIBC_2_0);
compat_symbol (libc_malloc_debug, mcheck_pedantic, mcheck_pedantic, GLIBC_2_2);
compat_symbol (libc_malloc_debug, memalign, memalign, GLIBC_2_0);
compat_symbol (libc_malloc_debug, mprobe, mprobe, GLIBC_2_0);
compat_symbol (libc_malloc_debug, mtrace, mtrace, GLIBC_2_0);
compat_symbol (libc_malloc_debug, muntrace, muntrace, GLIBC_2_0);
compat_symbol (libc_malloc_debug, posix_memalign, posix_memalign, GLIBC_2_2);
compat_symbol (libc_malloc_debug, pvalloc, pvalloc, GLIBC_2_0);
compat_symbol (libc_malloc_debug, realloc, realloc, GLIBC_2_0);
compat_symbol (libc_malloc_debug, valloc, valloc, GLIBC_2_0);
compat_symbol (libc_malloc_debug, __free_hook, __free_hook, GLIBC_2_0);
compat_symbol (libc_malloc_debug, __malloc_hook, __malloc_hook, GLIBC_2_0);
compat_symbol (libc_malloc_debug, __realloc_hook, __realloc_hook, GLIBC_2_0);
compat_symbol (libc_malloc_debug, __memalign_hook, __memalign_hook, GLIBC_2_0);
