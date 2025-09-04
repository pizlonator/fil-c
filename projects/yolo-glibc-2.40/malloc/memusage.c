/* Profile heap and stack memory usage of running program.
   Copyright (C) 1998-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <libintl.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <unistd_ext.h>

#include <hp-timing.h>
#include <machine-sp.h>
#include <stackinfo.h>  /* For _STACK_GROWS_UP  */

/* Pointer to the real functions.  These are determined used `dlsym'
   when really needed.  */
static void *(*mallocp)(size_t);
static void *(*reallocp) (void *, size_t);
static void *(*callocp) (size_t, size_t);
static void (*freep) (void *);

static void *(*mmapp) (void *, size_t, int, int, int, off_t);
static void *(*mmap64p) (void *, size_t, int, int, int, off64_t);
static int (*munmapp) (void *, size_t);
static void *(*mremapp) (void *, size_t, size_t, int, void *);

enum
{
  idx_malloc = 0,
  idx_realloc,
  idx_calloc,
  idx_free,
  idx_mmap_r,
  idx_mmap_w,
  idx_mmap_a,
  idx_mremap,
  idx_munmap,
  idx_last
};


struct header
{
  size_t length;
  size_t magic;
};

#define MAGIC 0xfeedbeaf


static _Atomic unsigned long int calls[idx_last];
static _Atomic unsigned long int failed[idx_last];
static _Atomic size_t total[idx_last];
static _Atomic size_t grand_total;
static _Atomic unsigned long int histogram[65536 / 16];
static _Atomic unsigned long int large;
static _Atomic unsigned long int calls_total;
static _Atomic unsigned long int inplace;
static _Atomic unsigned long int decreasing;
static _Atomic unsigned long int realloc_free;
static _Atomic unsigned long int inplace_mremap;
static _Atomic unsigned long int decreasing_mremap;
static _Atomic size_t current_heap;
static _Atomic size_t peak_use[3];
static __thread uintptr_t start_sp;

/* A few macros to make the source more readable.  */
#define peak_heap       peak_use[0]
#define peak_stack      peak_use[1]
#define peak_total      peak_use[2]

#define DEFAULT_BUFFER_SIZE     32768
static size_t buffer_size;

static int fd = -1;

static bool not_me;
static int initialized;
static bool trace_mmap;
extern const char *__progname;

struct entry
{
  uint64_t heap;
  uint64_t stack;
  uint32_t time_low;
  uint32_t time_high;
};

static struct entry buffer[2 * DEFAULT_BUFFER_SIZE];
static _Atomic uint32_t buffer_cnt;
static struct entry first;

static void
gettime (struct entry *e)
{
#if HP_TIMING_INLINE
  hp_timing_t now;
  HP_TIMING_NOW (now);
  e->time_low = now & 0xffffffff;
  e->time_high = now >> 32;
#else
  struct __timespec64 now;
  uint64_t usecs;
  __clock_gettime64 (CLOCK_REALTIME, &now);
  usecs = (uint64_t)now.tv_nsec / 1000 + (uint64_t)now.tv_sec * 1000000;
  e->time_low = usecs & 0xffffffff;
  e->time_high = usecs >> 32;
#endif
}

static inline void
peak_atomic_max (_Atomic size_t *peak, size_t val)
{
  size_t v;
  do
    {
      v = atomic_load_explicit (peak, memory_order_relaxed);
      if (v >= val)
	break;
    }
  while (! atomic_compare_exchange_weak (peak, &v, val));
}

/* Update the global data after a successful function call.  */
static void
update_data (struct header *result, size_t len, size_t old_len)
{
  if (result != NULL)
    {
      /* Record the information we need and mark the block using a
         magic number.  */
      result->length = len;
      result->magic = MAGIC;
    }

  /* Compute current heap usage and compare it with the maximum value.  */
  size_t heap
    = atomic_fetch_add_explicit (&current_heap, len - old_len,
				 memory_order_relaxed) + len - old_len;
  peak_atomic_max (&peak_heap, heap);

  /* Compute current stack usage and compare it with the maximum
     value.  The base stack pointer might not be set if this is not
     the main thread and it is the first call to any of these
     functions.  */
  if (__glibc_unlikely (!start_sp))
    start_sp = __thread_stack_pointer ();

  uintptr_t sp = __thread_stack_pointer ();
#if _STACK_GROWS_UP
  /* This can happen in threads where we didn't catch the thread's
     stack early enough.  */
  if (__glibc_unlikely (sp < start_sp))
    start_sp = sp;
  size_t current_stack = sp - start_sp;
#else
  /* This can happen in threads where we didn't catch the thread's
     stack early enough.  */
  if (__glibc_unlikely (sp > start_sp))
    start_sp = sp;
  size_t current_stack = start_sp - sp;
#endif
  peak_atomic_max (&peak_stack, current_stack);

  /* Add up heap and stack usage and compare it with the maximum value.  */
  peak_atomic_max (&peak_total, heap + current_stack);

  /* Store the value only if we are writing to a file.  */
  if (fd != -1)
    {
      uint32_t idx = atomic_fetch_add_explicit (&buffer_cnt, 1,
						memory_order_relaxed);
      if (idx + 1 >= 2 * buffer_size)
        {
          /* We try to reset the counter to the correct range.  If
             this fails because of another thread increasing the
             counter it does not matter since that thread will take
             care of the correction.  */
          uint32_t reset = (idx + 1) % (2 * buffer_size);
	  uint32_t expected = idx + 1;
	  atomic_compare_exchange_weak (&buffer_cnt, &expected, reset);
          if (idx >= 2 * buffer_size)
            idx = reset - 1;
        }
      assert (idx < 2 * DEFAULT_BUFFER_SIZE);

      buffer[idx].heap = current_heap;
      buffer[idx].stack = current_stack;
      gettime (&buffer[idx]);

      /* Write out buffer if it is full.  */
      if (idx + 1 == buffer_size || idx + 1 == 2 * buffer_size)
        {
	  uint32_t write_size = buffer_size * sizeof (buffer[0]);
	  write_all (fd, &buffer[idx + 1 - buffer_size], write_size);
        }
    }
}


/* Interrupt handler.  */
static void
int_handler (int signo)
{
  /* Nothing gets allocated.  Just record the stack pointer position.  */
  update_data (NULL, 0, 0);
}


/* Find out whether this is the program we are supposed to profile.
   For this the name in the variable `__progname' must match the one
   given in the environment variable MEMUSAGE_PROG_NAME.  If the variable
   is not present every program assumes it should be profiling.

   If this is the program open a file descriptor to the output file.
   We will write to it whenever the buffer overflows.  The name of the
   output file is determined by the environment variable MEMUSAGE_OUTPUT.

   If the environment variable MEMUSAGE_BUFFER_SIZE is set its numerical
   value determines the size of the internal buffer.  The number gives
   the number of elements in the buffer.  By setting the number to one
   one effectively selects unbuffered operation.

   If MEMUSAGE_NO_TIMER is not present an alarm handler is installed
   which at the highest possible frequency records the stack pointer.  */
static void
me (void)
{
  const char *env = getenv ("MEMUSAGE_PROG_NAME");
  size_t prog_len = strlen (__progname);

  initialized = -1;
  mallocp = (void *(*)(size_t))dlsym (RTLD_NEXT, "malloc");
  reallocp = (void *(*)(void *, size_t))dlsym (RTLD_NEXT, "realloc");
  callocp = (void *(*)(size_t, size_t))dlsym (RTLD_NEXT, "calloc");
  freep = (void (*)(void *))dlsym (RTLD_NEXT, "free");

  mmapp = (void *(*)(void *, size_t, int, int, int, off_t))dlsym (RTLD_NEXT,
                                                                  "mmap");
  mmap64p =
    (void *(*)(void *, size_t, int, int, int, off64_t))dlsym (RTLD_NEXT,
                                                              "mmap64");
  mremapp = (void *(*)(void *, size_t, size_t, int, void *))dlsym (RTLD_NEXT,
                                                                   "mremap");
  munmapp = (int (*)(void *, size_t))dlsym (RTLD_NEXT, "munmap");
  initialized = 1;

  if (env != NULL)
    {
      /* Check for program name.  */
      size_t len = strlen (env);
      if (len > prog_len || strcmp (env, &__progname[prog_len - len]) != 0
          || (prog_len != len && __progname[prog_len - len - 1] != '/'))
        not_me = true;
    }

  /* Only open the file if it's really us.  */
  if (!not_me && fd == -1)
    {
      const char *outname;

      if (!start_sp)
        start_sp = __thread_stack_pointer ();

      outname = getenv ("MEMUSAGE_OUTPUT");
      if (outname != NULL && outname[0] != '\0'
          && (access (outname, R_OK | W_OK) == 0 || errno == ENOENT))
        {
          fd = creat64 (outname, 0666);

          if (fd == -1)
            /* Don't do anything in future calls if we cannot write to
               the output file.  */
            not_me = true;
          else
            {
              /* Write the first entry.  */
              first.heap = 0;
              first.stack = 0;
              gettime (&first);
              /* Write it two times since we need the starting and end time. */
	      write_all (fd, &first, sizeof (first));
	      write_all (fd, &first, sizeof (first));

              /* Determine the buffer size.  We use the default if the
                 environment variable is not present.  */
              buffer_size = DEFAULT_BUFFER_SIZE;
              const char *str_buffer_size = getenv ("MEMUSAGE_BUFFER_SIZE");
              if (str_buffer_size != NULL)
                {
                  buffer_size = atoi (str_buffer_size);
                  if (buffer_size == 0 || buffer_size > DEFAULT_BUFFER_SIZE)
                    buffer_size = DEFAULT_BUFFER_SIZE;
                }

              /* Possibly enable timer-based stack pointer retrieval.  */
              if (getenv ("MEMUSAGE_NO_TIMER") == NULL)
                {
                  struct sigaction act;

                  act.sa_handler = (sighandler_t) &int_handler;
                  act.sa_flags = SA_RESTART;
                  sigfillset (&act.sa_mask);

                  if (sigaction (SIGPROF, &act, NULL) >= 0)
                    {
                      struct itimerval timer;

                      timer.it_value.tv_sec = 0;
                      timer.it_value.tv_usec = 1;
                      timer.it_interval = timer.it_value;
                      setitimer (ITIMER_PROF, &timer, NULL);
                    }
                }
            }
        }

      if (!not_me && getenv ("MEMUSAGE_TRACE_MMAP") != NULL)
        trace_mmap = true;
    }
}


/* Record the initial stack position.  */
static void
__attribute__ ((constructor))
init (void)
{
  start_sp = __thread_stack_pointer ();
  if (!initialized)
    me ();
}


/* `malloc' replacement.  We keep track of the memory usage if this is the
   correct program.  */
void *
malloc (size_t len)
{
  struct header *result = NULL;

  /* Determine real implementation if not already happened.  */
  if (__glibc_unlikely (initialized <= 0))
    {
      if (initialized == -1)
        return NULL;

      me ();
    }

  /* If this is not the correct program just use the normal function.  */
  if (not_me)
    return (*mallocp)(len);

  /* Keep track of number of calls.  */
  atomic_fetch_add_explicit (&calls[idx_malloc], 1, memory_order_relaxed);
  /* Keep track of total memory consumption for `malloc'.  */
  atomic_fetch_add_explicit (&total[idx_malloc], len, memory_order_relaxed);
  /* Keep track of total memory requirement.  */
  atomic_fetch_add_explicit (&grand_total, len, memory_order_relaxed);
  /* Remember the size of the request.  */
  if (len < 65536)
    atomic_fetch_add_explicit (&histogram[len / 16], 1, memory_order_relaxed);
  else
    atomic_fetch_add_explicit (&large, 1, memory_order_relaxed);
  /* Total number of calls of any of the functions.  */
  atomic_fetch_add_explicit (&calls_total, 1, memory_order_relaxed);

  /* Do the real work.  */
  result = (struct header *) (*mallocp)(len + sizeof (struct header));
  if (result == NULL)
    {
      atomic_fetch_add_explicit (&failed[idx_malloc], 1,
				 memory_order_relaxed);
      return NULL;
    }

  /* Update the allocation data and write out the records if necessary.  */
  update_data (result, len, 0);

  /* Return the pointer to the user buffer.  */
  return (void *) (result + 1);
}


/* `realloc' replacement.  We keep track of the memory usage if this is the
   correct program.  */
void *
realloc (void *old, size_t len)
{
  struct header *result = NULL;
  struct header *real;
  size_t old_len;

  /* Determine real implementation if not already happened.  */
  if (__glibc_unlikely (initialized <= 0))
    {
      if (initialized == -1)
        return NULL;

      me ();
    }

  /* If this is not the correct program just use the normal function.  */
  if (not_me)
    return (*reallocp)(old, len);

  if (old == NULL)
    {
      /* This is really a `malloc' call.  */
      real = NULL;
      old_len = 0;
    }
  else
    {
      real = ((struct header *) old) - 1;
      if (real->magic != MAGIC)
        /* This is no memory allocated here.  */
        return (*reallocp)(old, len);

      old_len = real->length;
    }

  /* Keep track of number of calls.  */
  atomic_fetch_add_explicit (&calls[idx_realloc], 1, memory_order_relaxed);
  if (len > old_len)
    {
      /* Keep track of total memory consumption for `realloc'.  */
      atomic_fetch_add_explicit (&total[idx_realloc], len - old_len,
				 memory_order_relaxed);
      /* Keep track of total memory requirement.  */
      atomic_fetch_add_explicit (&grand_total, len - old_len,
				 memory_order_relaxed);
    }

  if (len == 0 && old != NULL)
    {
      /* Special case.  */
      atomic_fetch_add_explicit (&realloc_free, 1, memory_order_relaxed);
      /* Keep track of total memory freed using `free'.  */
      atomic_fetch_add_explicit (&total[idx_free], real->length,
				 memory_order_relaxed);

      /* Update the allocation data and write out the records if necessary.  */
      update_data (NULL, 0, old_len);

      /* Do the real work.  */
      (*freep) (real);

      return NULL;
    }

  /* Remember the size of the request.  */
  if (len < 65536)
    atomic_fetch_add_explicit (&histogram[len / 16], 1, memory_order_relaxed);
  else
    atomic_fetch_add_explicit (&large, 1, memory_order_relaxed);
  /* Total number of calls of any of the functions.  */
  atomic_fetch_add_explicit (&calls_total, 1, memory_order_relaxed);

  /* Do the real work.  */
  result = (struct header *) (*reallocp)(real, len + sizeof (struct header));
  if (result == NULL)
    {
      atomic_fetch_add_explicit (&failed[idx_realloc], 1,
				 memory_order_relaxed);
      return NULL;
    }

  /* Record whether the reduction/increase happened in place.  */
  if (real == result)
    atomic_fetch_add_explicit (&inplace, 1, memory_order_relaxed);
  /* Was the buffer increased?  */
  if (old_len > len)
    atomic_fetch_add_explicit (&decreasing, 1, memory_order_relaxed);

  /* Update the allocation data and write out the records if necessary.  */
  update_data (result, len, old_len);

  /* Return the pointer to the user buffer.  */
  return (void *) (result + 1);
}


/* `calloc' replacement.  We keep track of the memory usage if this is the
   correct program.  */
void *
calloc (size_t n, size_t len)
{
  struct header *result;
  size_t size = n * len;

  /* Determine real implementation if not already happened.  */
  if (__glibc_unlikely (initialized <= 0))
    {
      if (initialized == -1)
        return NULL;

      me ();
    }

  /* If this is not the correct program just use the normal function.  */
  if (not_me)
    return (*callocp)(n, len);

  /* Keep track of number of calls.  */
  atomic_fetch_add_explicit (&calls[idx_calloc], 1, memory_order_relaxed);
  /* Keep track of total memory consumption for `calloc'.  */
  atomic_fetch_add_explicit (&total[idx_calloc], size, memory_order_relaxed);
  /* Keep track of total memory requirement.  */
  atomic_fetch_add_explicit (&grand_total, size, memory_order_relaxed);
  /* Remember the size of the request.  */
  if (size < 65536)
    atomic_fetch_add_explicit (&histogram[size / 16], 1,
			       memory_order_relaxed);
  else
    atomic_fetch_add_explicit (&large, 1, memory_order_relaxed);
  /* Total number of calls of any of the functions.  */
  ++calls_total;

  /* Do the real work.  */
  result = (struct header *) (*mallocp)(size + sizeof (struct header));
  if (result == NULL)
    {
      atomic_fetch_add_explicit (&failed[idx_calloc], 1,
				 memory_order_relaxed);
      return NULL;
    }

  /* Update the allocation data and write out the records if necessary.  */
  update_data (result, size, 0);

  /* Do what `calloc' would have done and return the buffer to the caller.  */
  return memset (result + 1, '\0', size);
}


/* `free' replacement.  We keep track of the memory usage if this is the
   correct program.  */
void
free (void *ptr)
{
  struct header *real;

  /* Determine real implementation if not already happened.  */
  if (__glibc_unlikely (initialized <= 0))
    {
      if (initialized == -1)
        return;

      me ();
    }

  /* If this is not the correct program just use the normal function.  */
  if (not_me)
    {
      (*freep) (ptr);
      return;
    }

  /* `free (NULL)' has no effect.  */
  if (ptr == NULL)
    {
      atomic_fetch_add_explicit (&calls[idx_free], 1, memory_order_relaxed);
      return;
    }

  /* Determine the pointer to the header.  */
  real = ((struct header *) ptr) - 1;
  if (real->magic != MAGIC)
    {
      /* This block wasn't allocated here.  */
      (*freep) (ptr);
      return;
    }

  /* Keep track of number of calls.  */
  atomic_fetch_add_explicit (&calls[idx_free], 1, memory_order_relaxed);
  /* Keep track of total memory freed using `free'.  */
  atomic_fetch_add_explicit (&total[idx_free], real->length,
			     memory_order_relaxed);

  /* Update the allocation data and write out the records if necessary.  */
  update_data (NULL, 0, real->length);

  /* Do the real work.  */
  (*freep) (real);
}


/* `mmap' replacement.  We do not have to keep track of the size since
   `munmap' will get it as a parameter.  */
void *
mmap (void *start, size_t len, int prot, int flags, int fd, off_t offset)
{
  void *result = NULL;

  /* Determine real implementation if not already happened.  */
  if (__glibc_unlikely (initialized <= 0))
    {
      if (initialized == -1)
        return NULL;

      me ();
    }

  /* Always get a block.  We don't need extra memory.  */
  result = (*mmapp)(start, len, prot, flags, fd, offset);

  if (!not_me && trace_mmap)
    {
      int idx = (flags & MAP_ANON
                 ? idx_mmap_a : prot & PROT_WRITE ? idx_mmap_w : idx_mmap_r);

      /* Keep track of number of calls.  */
      atomic_fetch_add_explicit (&calls[idx], 1, memory_order_relaxed);
      /* Keep track of total memory consumption for `malloc'.  */
      atomic_fetch_add_explicit (&total[idx], len, memory_order_relaxed);
      /* Keep track of total memory requirement.  */
      atomic_fetch_add_explicit (&grand_total, len, memory_order_relaxed);
      /* Remember the size of the request.  */
      if (len < 65536)
        atomic_fetch_add_explicit (&histogram[len / 16], 1,
				   memory_order_relaxed);
      else
        atomic_fetch_add_explicit (&large, 1, memory_order_relaxed);
      /* Total number of calls of any of the functions.  */
      atomic_fetch_add_explicit (&calls_total, 1, memory_order_relaxed);

      /* Check for failures.  */
      if (result == NULL)
        atomic_fetch_add_explicit (&failed[idx], 1, memory_order_relaxed);
      else if (idx == idx_mmap_w)
        /* Update the allocation data and write out the records if
           necessary.  Note the first parameter is NULL which means
           the size is not tracked.  */
        update_data (NULL, len, 0);
    }

  /* Return the pointer to the user buffer.  */
  return result;
}


/* `mmap64' replacement.  We do not have to keep track of the size since
   `munmap' will get it as a parameter.  */
void *
mmap64 (void *start, size_t len, int prot, int flags, int fd, off64_t offset)
{
  void *result = NULL;

  /* Determine real implementation if not already happened.  */
  if (__glibc_unlikely (initialized <= 0))
    {
      if (initialized == -1)
        return NULL;

      me ();
    }

  /* Always get a block.  We don't need extra memory.  */
  result = (*mmap64p)(start, len, prot, flags, fd, offset);

  if (!not_me && trace_mmap)
    {
      int idx = (flags & MAP_ANON
                 ? idx_mmap_a : prot & PROT_WRITE ? idx_mmap_w : idx_mmap_r);

      /* Keep track of number of calls.  */
      atomic_fetch_add_explicit (&calls[idx], 1, memory_order_relaxed);
      /* Keep track of total memory consumption for `malloc'.  */
      atomic_fetch_add_explicit (&total[idx], len, memory_order_relaxed);
      /* Keep track of total memory requirement.  */
      atomic_fetch_add_explicit (&grand_total, len, memory_order_relaxed);
      /* Remember the size of the request.  */
      if (len < 65536)
        atomic_fetch_add_explicit (&histogram[len / 16], 1,
				   memory_order_relaxed);
      else
        atomic_fetch_add_explicit (&large, 1, memory_order_relaxed);
      /* Total number of calls of any of the functions.  */
      atomic_fetch_add_explicit (&calls_total, 1, memory_order_relaxed);

      /* Check for failures.  */
      if (result == NULL)
        atomic_fetch_add_explicit (&failed[idx], 1, memory_order_relaxed);
      else if (idx == idx_mmap_w)
        /* Update the allocation data and write out the records if
           necessary.  Note the first parameter is NULL which means
           the size is not tracked.  */
        update_data (NULL, len, 0);
    }

  /* Return the pointer to the user buffer.  */
  return result;
}


/* `mremap' replacement.  We do not have to keep track of the size since
   `munmap' will get it as a parameter.  */
void *
mremap (void *start, size_t old_len, size_t len, int flags, ...)
{
  void *result = NULL;
  va_list ap;

  va_start (ap, flags);
  void *newaddr = (flags & MREMAP_FIXED) ? va_arg (ap, void *) : NULL;
  va_end (ap);

  /* Determine real implementation if not already happened.  */
  if (__glibc_unlikely (initialized <= 0))
    {
      if (initialized == -1)
        return NULL;

      me ();
    }

  /* Always get a block.  We don't need extra memory.  */
  result = (*mremapp)(start, old_len, len, flags, newaddr);

  if (!not_me && trace_mmap)
    {
      /* Keep track of number of calls.  */
      atomic_fetch_add_explicit (&calls[idx_mremap], 1, memory_order_relaxed);
      if (len > old_len)
        {
          /* Keep track of total memory consumption for `malloc'.  */
          atomic_fetch_add_explicit (&total[idx_mremap], len - old_len,
				     memory_order_relaxed);
          /* Keep track of total memory requirement.  */
          atomic_fetch_add_explicit (&grand_total, len - old_len,
				     memory_order_relaxed);
        }
      /* Remember the size of the request.  */
      if (len < 65536)
        atomic_fetch_add_explicit (&histogram[len / 16], 1,
				   memory_order_relaxed);
      else
        atomic_fetch_add_explicit (&large, 1, memory_order_relaxed);
      /* Total number of calls of any of the functions.  */
      atomic_fetch_add_explicit (&calls_total, 1, memory_order_relaxed);

      /* Check for failures.  */
      if (result == NULL)
        atomic_fetch_add_explicit (&failed[idx_mremap], 1,
				   memory_order_relaxed);
      else
        {
          /* Record whether the reduction/increase happened in place.  */
          if (start == result)
            atomic_fetch_add_explicit (&inplace_mremap, 1,
				       memory_order_relaxed);
          /* Was the buffer increased?  */
          if (old_len > len)
            atomic_fetch_add_explicit (&decreasing_mremap, 1,
				       memory_order_relaxed);

          /* Update the allocation data and write out the records if
             necessary.  Note the first parameter is NULL which means
             the size is not tracked.  */
          update_data (NULL, len, old_len);
        }
    }

  /* Return the pointer to the user buffer.  */
  return result;
}


/* `munmap' replacement.  */
int
munmap (void *start, size_t len)
{
  int result;

  /* Determine real implementation if not already happened.  */
  if (__glibc_unlikely (initialized <= 0))
    {
      if (initialized == -1)
        return -1;

      me ();
    }

  /* Do the real work.  */
  result = (*munmapp)(start, len);

  if (!not_me && trace_mmap)
    {
      /* Keep track of number of calls.  */
      atomic_fetch_add_explicit (&calls[idx_munmap], 1, memory_order_relaxed);

      if (__glibc_likely (result == 0))
        {
          /* Keep track of total memory freed using `free'.  */
          atomic_fetch_add_explicit (&total[idx_munmap], len,
				     memory_order_relaxed);

          /* Update the allocation data and write out the records if
             necessary.  */
          update_data (NULL, 0, len);
        }
      else
        atomic_fetch_add_explicit (&failed[idx_munmap], 1,
				   memory_order_relaxed);
    }

  return result;
}


/* Write some statistics to standard error.  */
static void
__attribute__ ((destructor))
dest (void)
{
  int percent, cnt;
  unsigned long int maxcalls;

  /* If we haven't done anything here just return.  */
  if (not_me)
    return;

  /* If we should call any of the memory functions don't do any profiling.  */
  not_me = true;

  /* Finish the output file.  */
  if (fd != -1)
    {
      /* Write the partially filled buffer.  */
      struct entry *start = buffer;
      uint32_t write_cnt = buffer_cnt;

      if (buffer_cnt > buffer_size)
        {
          start = buffer + buffer_size;
          write_cnt = buffer_cnt - buffer_size;
        }

      write_all (fd, start, write_cnt * sizeof (buffer[0]));

      /* Go back to the beginning of the file.  We allocated two records
         here when we opened the file.  */
      lseek (fd, 0, SEEK_SET);
      /* Write out a record containing the total size.  */
      first.stack = peak_total;
      write_all (fd, &first, sizeof (first));
      /* Write out another record containing the maximum for heap and
         stack.  */
      first.heap = peak_heap;
      first.stack = peak_stack;
      gettime (&first);
      write_all (fd, &first, sizeof (first));

      /* Close the file.  */
      close (fd);
      fd = -1;
    }

  /* Write a colorful statistic.  */
  fprintf (stderr, "\n\
\e[01;32mMemory usage summary:\e[0;0m heap total: %llu, heap peak: %lu, stack peak: %lu\n\
\e[04;34m         total calls   total memory   failed calls\e[0m\n\
\e[00;34m malloc|\e[0m %10lu   %12llu   %s%12lu\e[00;00m\n\
\e[00;34mrealloc|\e[0m %10lu   %12llu   %s%12lu\e[00;00m  (nomove:%ld, dec:%ld, free:%ld)\n\
\e[00;34m calloc|\e[0m %10lu   %12llu   %s%12lu\e[00;00m\n\
\e[00;34m   free|\e[0m %10lu   %12llu\n",
           (unsigned long long int) grand_total, (unsigned long int) peak_heap,
           (unsigned long int) peak_stack,
           (unsigned long int) calls[idx_malloc],
           (unsigned long long int) total[idx_malloc],
           failed[idx_malloc] ? "\e[01;41m" : "",
           (unsigned long int) failed[idx_malloc],
           (unsigned long int) calls[idx_realloc],
           (unsigned long long int) total[idx_realloc],
           failed[idx_realloc] ? "\e[01;41m" : "",
           (unsigned long int) failed[idx_realloc],
           (unsigned long int) inplace,
           (unsigned long int) decreasing,
           (unsigned long int) realloc_free,
           (unsigned long int) calls[idx_calloc],
           (unsigned long long int) total[idx_calloc],
           failed[idx_calloc] ? "\e[01;41m" : "",
           (unsigned long int) failed[idx_calloc],
           (unsigned long int) calls[idx_free],
           (unsigned long long int) total[idx_free]);

  if (trace_mmap)
    fprintf (stderr, "\
\e[00;34mmmap(r)|\e[0m %10lu   %12llu   %s%12lu\e[00;00m\n\
\e[00;34mmmap(w)|\e[0m %10lu   %12llu   %s%12lu\e[00;00m\n\
\e[00;34mmmap(a)|\e[0m %10lu   %12llu   %s%12lu\e[00;00m\n\
\e[00;34m mremap|\e[0m %10lu   %12llu   %s%12lu\e[00;00m  (nomove: %ld, dec:%ld)\n\
\e[00;34m munmap|\e[0m %10lu   %12llu   %s%12lu\e[00;00m\n",
             (unsigned long int) calls[idx_mmap_r],
             (unsigned long long int) total[idx_mmap_r],
             failed[idx_mmap_r] ? "\e[01;41m" : "",
             (unsigned long int) failed[idx_mmap_r],
             (unsigned long int) calls[idx_mmap_w],
             (unsigned long long int) total[idx_mmap_w],
             failed[idx_mmap_w] ? "\e[01;41m" : "",
             (unsigned long int) failed[idx_mmap_w],
             (unsigned long int) calls[idx_mmap_a],
             (unsigned long long int) total[idx_mmap_a],
             failed[idx_mmap_a] ? "\e[01;41m" : "",
             (unsigned long int) failed[idx_mmap_a],
             (unsigned long int) calls[idx_mremap],
             (unsigned long long int) total[idx_mremap],
             failed[idx_mremap] ? "\e[01;41m" : "",
             (unsigned long int) failed[idx_mremap],
             (unsigned long int) inplace_mremap,
             (unsigned long int) decreasing_mremap,
             (unsigned long int) calls[idx_munmap],
             (unsigned long long int) total[idx_munmap],
             failed[idx_munmap] ? "\e[01;41m" : "",
             (unsigned long int) failed[idx_munmap]);

  /* Write out a histoogram of the sizes of the allocations.  */
  fprintf (stderr, "\e[01;32mHistogram for block sizes:\e[0;0m\n");

  /* Determine the maximum of all calls for each size range.  */
  maxcalls = large;
  for (cnt = 0; cnt < 65536; cnt += 16)
    if (histogram[cnt / 16] > maxcalls)
      maxcalls = histogram[cnt / 16];

  for (cnt = 0; cnt < 65536; cnt += 16)
    /* Only write out the nonzero entries.  */
    if (histogram[cnt / 16] != 0)
      {
        percent = (histogram[cnt / 16] * 100) / calls_total;
        fprintf (stderr, "%5d-%-5d%12lu ", cnt, cnt + 15,
                 (unsigned long int) histogram[cnt / 16]);
        if (percent == 0)
          fputs (" <1% \e[41;37m", stderr);
        else
          fprintf (stderr, "%3d%% \e[41;37m", percent);

        /* Draw a bar with a length corresponding to the current
           percentage.  */
        percent = (histogram[cnt / 16] * 50) / maxcalls;
        while (percent-- > 0)
          fputc ('=', stderr);
        fputs ("\e[0;0m\n", stderr);
      }

  if (large != 0)
    {
      percent = (large * 100) / calls_total;
      fprintf (stderr, "   large   %12lu ", (unsigned long int) large);
      if (percent == 0)
        fputs (" <1% \e[41;37m", stderr);
      else
        fprintf (stderr, "%3d%% \e[41;37m", percent);
      percent = (large * 50) / maxcalls;
      while (percent-- > 0)
        fputc ('=', stderr);
      fputs ("\e[0;0m\n", stderr);
    }

  /* Any following malloc/free etc. calls should generate statistics again,
     because otherwise freeing something that has been malloced before
     this destructor (including struct header in front of it) wouldn't
     be properly freed.  */
  not_me = false;
}
