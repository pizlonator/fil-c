/* Copyright (C) 2002-2024 Free Software Foundation, Inc.
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

#include <libc-lock.h>
#include <stdbool.h>
#include <register-atfork.h>
#include <intprops.h>
#include <stdio.h>

#define DYNARRAY_ELEMENT           struct fork_handler
#define DYNARRAY_STRUCT            fork_handler_list
#define DYNARRAY_PREFIX            fork_handler_list_
#define DYNARRAY_INITIAL_SIZE      48
#include <malloc/dynarray-skeleton.c>

static struct fork_handler_list fork_handlers;
static uint64_t fork_handler_counter;

static int atfork_lock = LLL_LOCK_INITIALIZER;

int
__register_atfork (void (*prepare) (void), void (*parent) (void),
		   void (*child) (void), void *dso_handle)
{
  lll_lock (atfork_lock, LLL_PRIVATE);

  if (fork_handler_counter == 0)
    fork_handler_list_init (&fork_handlers);

  struct fork_handler *newp = fork_handler_list_emplace (&fork_handlers);
  if (newp != NULL)
    {
      newp->prepare_handler = prepare;
      newp->parent_handler = parent;
      newp->child_handler = child;
      newp->dso_handle = dso_handle;

      /* IDs assigned to handlers start at 1 and increment with handler
         registration.  Un-registering a handlers discards the corresponding
         ID.  It is not reused in future registrations.  */
      if (INT_ADD_OVERFLOW (fork_handler_counter, 1))
        __libc_fatal ("fork handler counter overflow");
      newp->id = ++fork_handler_counter;
    }

  /* Release the lock.  */
  lll_unlock (atfork_lock, LLL_PRIVATE);

  return newp == NULL ? ENOMEM : 0;
}
libc_hidden_def (__register_atfork)

static struct fork_handler *
fork_handler_list_find (struct fork_handler_list *fork_handlers,
			void *dso_handle)
{
  for (size_t i = 0; i < fork_handler_list_size (fork_handlers); i++)
    {
      struct fork_handler *elem = fork_handler_list_at (fork_handlers, i);
      if (elem->dso_handle == dso_handle)
	return elem;
    }
  return NULL;
}

void
__unregister_atfork (void *dso_handle)
{
  lll_lock (atfork_lock, LLL_PRIVATE);

  struct fork_handler *first = fork_handler_list_find (&fork_handlers,
						       dso_handle);
  /* Removing is done by shifting the elements in the way the elements
     that are not to be removed appear in the beginning in dynarray.
     This avoid the quadradic run-time if a naive strategy to remove and
     shift one element at time.  */
  if (first != NULL)
    {
      struct fork_handler *new_end = first;
      first++;
      for (; first != fork_handler_list_end (&fork_handlers); ++first)
	{
	  if (first->dso_handle != dso_handle)
	    {
	      *new_end = *first;
	      ++new_end;
	    }
	}

      ptrdiff_t removed = first - new_end;
      for (size_t i = 0; i < removed; i++)
	fork_handler_list_remove_last (&fork_handlers);
    }

  lll_unlock (atfork_lock, LLL_PRIVATE);
}

uint64_t
__run_prefork_handlers (_Bool do_locking)
{
  uint64_t lastrun;

  if (do_locking)
    lll_lock (atfork_lock, LLL_PRIVATE);

  /* We run prepare handlers from last to first.  After fork, only
     handlers up to the last handler found here (pre-fork) will be run.
     Handlers registered during __run_prefork_handlers or
     __run_postfork_handlers will be positioned after this last handler, and
     since their prepare handlers won't be run now, their parent/child
     handlers should also be ignored.  */
  lastrun = fork_handler_counter;

  size_t sl = fork_handler_list_size (&fork_handlers);
  for (size_t i = sl; i > 0;)
    {
      struct fork_handler *runp
        = fork_handler_list_at (&fork_handlers, i - 1);

      uint64_t id = runp->id;

      if (runp->prepare_handler != NULL)
        {
          if (do_locking)
            lll_unlock (atfork_lock, LLL_PRIVATE);

          runp->prepare_handler ();

          if (do_locking)
            lll_lock (atfork_lock, LLL_PRIVATE);
        }

      /* We unlocked, ran the handler, and locked again.  In the
         meanwhile, one or more deregistrations could have occurred leading
         to the current (just run) handler being moved up the list or even
         removed from the list itself.  Since handler IDs are guaranteed to
         to be in increasing order, the next handler has to have:  */

      /* A. An earlier position than the current one has.  */
      i--;

      /* B. A lower ID than the current one does.  The code below skips
         any newly added handlers with higher IDs.  */
      while (i > 0
             && fork_handler_list_at (&fork_handlers, i - 1)->id >= id)
        i--;
    }

  return lastrun;
}

void
__run_postfork_handlers (enum __run_fork_handler_type who, _Bool do_locking,
                         uint64_t lastrun)
{
  size_t sl = fork_handler_list_size (&fork_handlers);
  for (size_t i = 0; i < sl;)
    {
      struct fork_handler *runp = fork_handler_list_at (&fork_handlers, i);
      uint64_t id = runp->id;

      /* prepare handlers were not run for handlers with ID > LASTRUN.
         Thus, parent/child handlers will also not be run.  */
      if (id > lastrun)
        break;

      if (do_locking)
        lll_unlock (atfork_lock, LLL_PRIVATE);

      if (who == atfork_run_child && runp->child_handler)
        runp->child_handler ();
      else if (who == atfork_run_parent && runp->parent_handler)
        runp->parent_handler ();

      if (do_locking)
        lll_lock (atfork_lock, LLL_PRIVATE);

      /* We unlocked, ran the handler, and locked again.  In the meanwhile,
         one or more [de]registrations could have occurred.  Due to this,
         the list size must be updated.  */
      sl = fork_handler_list_size (&fork_handlers);

      /* The just-run handler could also have moved up the list. */

      if (sl > i && fork_handler_list_at (&fork_handlers, i)->id == id)
        /* The position of the recently run handler hasn't changed.  The
           next handler to be run is an easy increment away.  */
        i++;
      else
        {
          /* The next handler to be run is the first handler in the list
             to have an ID higher than the current one.  */
          for (i = 0; i < sl; i++)
            {
              if (fork_handler_list_at (&fork_handlers, i)->id > id)
                break;
            }
        }
    }

  if (do_locking)
    lll_unlock (atfork_lock, LLL_PRIVATE);
}


void
__libc_atfork_freemem (void)
{
  lll_lock (atfork_lock, LLL_PRIVATE);

  fork_handler_list_free (&fork_handlers);

  lll_unlock (atfork_lock, LLL_PRIVATE);
}
