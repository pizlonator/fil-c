/* Memory handling for libdw.
   Copyright (C) 2003, 2004, 2006 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include "libdwP.h"
#include "system.h"
#include "atomics.h"
#if USE_VG_ANNOTATIONS == 1
#include <helgrind.h>
#else
#define ANNOTATE_HAPPENS_BEFORE(X)
#define ANNOTATE_HAPPENS_AFTER(X)
#endif

#define THREAD_ID_UNSET ((size_t) -1)
static __thread size_t thread_id = THREAD_ID_UNSET;
static atomic_size_t next_id = ATOMIC_VAR_INIT(0);

struct libdw_memblock *
__libdw_alloc_tail (Dwarf *dbg)
{
  if (thread_id == THREAD_ID_UNSET)
    thread_id = atomic_fetch_add (&next_id, 1);

  pthread_rwlock_rdlock (&dbg->mem_rwl);
  if (thread_id >= dbg->mem_stacks)
    {
      pthread_rwlock_unlock (&dbg->mem_rwl);
      pthread_rwlock_wrlock (&dbg->mem_rwl);

      /* Another thread may have already reallocated. In theory using an
         atomic would be faster, but given that this only happens once per
         thread per Dwarf, some minor slowdown should be fine.  */
      if (thread_id >= dbg->mem_stacks)
        {
          dbg->mem_tails = realloc (dbg->mem_tails, (thread_id+1)
                                    * sizeof (struct libdw_memblock *));
          if (dbg->mem_tails == NULL)
            {
              pthread_rwlock_unlock (&dbg->mem_rwl);
              dbg->oom_handler();
            }
          for (size_t i = dbg->mem_stacks; i <= thread_id; i++)
            dbg->mem_tails[i] = NULL;
          dbg->mem_stacks = thread_id + 1;
          ANNOTATE_HAPPENS_BEFORE (&dbg->mem_tails);
        }

      pthread_rwlock_unlock (&dbg->mem_rwl);
      pthread_rwlock_rdlock (&dbg->mem_rwl);
    }

  /* At this point, we have an entry in the tail array.  */
  ANNOTATE_HAPPENS_AFTER (&dbg->mem_tails);
  struct libdw_memblock *result = dbg->mem_tails[thread_id];
  if (result == NULL)
    {
      result = malloc (dbg->mem_default_size);
      if (result == NULL)
	{
	  pthread_rwlock_unlock (&dbg->mem_rwl);
	  dbg->oom_handler();
	}
      result->size = dbg->mem_default_size
                     - offsetof (struct libdw_memblock, mem);
      result->remaining = result->size;
      result->prev = NULL;
      dbg->mem_tails[thread_id] = result;
    }
  pthread_rwlock_unlock (&dbg->mem_rwl);
  return result;
}

/* Can only be called after a allocation for this thread has already
   been done, to possibly undo it.  */
struct libdw_memblock *
__libdw_thread_tail (Dwarf *dbg)
{
  struct libdw_memblock *result;
  pthread_rwlock_rdlock (&dbg->mem_rwl);
  result = dbg->mem_tails[thread_id];
  pthread_rwlock_unlock (&dbg->mem_rwl);
  return result;
}

void *
__libdw_allocate (Dwarf *dbg, size_t minsize, size_t align)
{
  size_t size = MAX (dbg->mem_default_size,
		     (align - 1 +
		      2 * minsize + offsetof (struct libdw_memblock, mem)));
  struct libdw_memblock *newp = malloc (size);
  if (newp == NULL)
    dbg->oom_handler ();

  uintptr_t result = ((uintptr_t) newp->mem + align - 1) & ~(align - 1);

  newp->size = size - offsetof (struct libdw_memblock, mem);
  newp->remaining = (uintptr_t) newp + size - (result + minsize);

  pthread_rwlock_rdlock (&dbg->mem_rwl);
  newp->prev = dbg->mem_tails[thread_id];
  dbg->mem_tails[thread_id] = newp;
  pthread_rwlock_unlock (&dbg->mem_rwl);

  return (void *) result;
}


Dwarf_OOM
dwarf_new_oom_handler (Dwarf *dbg, Dwarf_OOM handler)
{
  Dwarf_OOM old = dbg->oom_handler;
  dbg->oom_handler = handler;
  return old;
}


void
__attribute ((noreturn)) attribute_hidden
__libdw_oom (void)
{
  while (1)
    error (EXIT_FAILURE, ENOMEM, "libdw");
}
