/* Copyright (C) 1992-2026 Free Software Foundation, Inc.
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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <setenv.h>
#include <errno.h>
#if !_LIBC
# if !defined errno && !defined HAVE_ERRNO_DECL
extern int errno;
# endif
# define __set_errno(ev) ((errno) = (ev))
#endif

#if _LIBC || HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if _LIBC || HAVE_STRING_H
# include <string.h>
#endif
#if _LIBC || HAVE_UNISTD_H
# include <unistd.h>
#endif

#if !_LIBC
# define __environ	environ
# ifndef HAVE_ENVIRON_DECL
extern char **environ;
# endif
#endif

#if _LIBC
/* This lock protects against simultaneous modifications of `environ'.  */
# include <libc-lock.h>
__libc_lock_define_initialized (static, envlock)
# define LOCK	__libc_lock_lock (envlock)
# define UNLOCK	__libc_lock_unlock (envlock)
#else
# define LOCK
# define UNLOCK
#endif

/* In the GNU C library we must keep the namespace clean.  */
#ifdef _LIBC
# define setenv __setenv
# define unsetenv __unsetenv
# define clearenv __clearenv
# define tfind __tfind
# define tsearch __tsearch
#endif

/* In the GNU C library implementation we try to be more clever and
   allow arbitrarily many changes of the environment given that the used
   values are from a small set.  Outside glibc this will eat up all
   memory after a while.  */
#if defined _LIBC || (defined HAVE_SEARCH_H && defined HAVE_TSEARCH \
		      && defined __GNUC__)
# define USE_TSEARCH	1
# include <search.h>

/* This is a pointer to the root of the search tree with the known
   values.  */
static void *known_values;

# define KNOWN_VALUE(Str) \
  ({									      \
    void *value = tfind (Str, &known_values, (__compar_fn_t) strcmp);	      \
    value != NULL ? *(char **) value : NULL;				      \
  })
# define STORE_VALUE(Str) \
  tsearch (Str, &known_values, (__compar_fn_t) strcmp)

#else
# undef USE_TSEARCH

# define KNOWN_VALUE(Str) NULL
# define STORE_VALUE(Str) do { } while (0)

#endif

/* Allocate a new environment array and put it o the
   __environ_array_list.  Returns NULL on memory allocation
   failure.  */
static struct environ_array *
__environ_new_array (size_t required_size)
{
  /* No backing array yet, or insufficient room.  */
  size_t new_size;
  if (__environ_array_list == NULL
      || __environ_array_list->allocated * 2 < required_size)
    /* Add some unused space for future growth.  */
    new_size = required_size + 16;
  else
    new_size = __environ_array_list->allocated * 2;

  /* Zero-initialize everything, so that getenv can only
     observe valid or null pointers.  */
  char **new_array = calloc (new_size, sizeof (*new_array));
  if (new_array == NULL)
    return NULL;

  struct environ_array *target_array = malloc (sizeof (*target_array));
  if (target_array == NULL)
    {
      free (new_array);
      return NULL;
    }

  target_array->allocated = new_size;
  target_array->array = new_array;
  assert (new_size >= target_array->allocated);

  /* Put it onto the list.  */
  target_array->next = __environ_array_list;
  __environ_array_list = target_array;
  return target_array;
}

int
__add_to_environ (const char *name, const char *value, const char *combined,
		  int replace)
{
  /* Compute lengths before locking, so that the critical section is
     less of a performance bottleneck.  VALLEN is needed only if
     COMBINED is null.  Testing COMBINED instead of VALUE causes setenv
     (..., NULL, ...) to dump core now instead of corrupting memory later.  */
  const size_t namelen = strlen (name);
  size_t vallen;
  if (combined == NULL)
    vallen = strlen (value) + 1;

  LOCK;

  /* We have to get the pointer now that we have the lock and not earlier
     since another thread might have created a new environment.   */
  char **start_environ = atomic_load_relaxed (&__environ);
  char **ep = start_environ;

  /* This gets written to __environ in the end.  */
  char **result_environ = start_environ;

  /* Size of the environment if *ep == NULL.  */
  if (ep != NULL)
    for (; *ep != NULL; ++ep)
      if (strncmp (*ep, name, namelen) == 0 && (*ep)[namelen] == '=')
	break;

  if (ep == NULL || __glibc_likely (*ep == NULL))
    {
      /* The scanning loop above reached the end of the environment.
	 Add a new string to it.  */
      replace = true;

      /* + 2 for the new entry and the terminating NULL.  */
      size_t required_size = (ep - start_environ) + 2;
      if (__environ_is_from_array_list (start_environ)
	  && required_size <= __environ_array_list->allocated)
	/* The __environ array is ours, and we have room in it.  We
	   can use ep as-is.  Add a null terminator in case current
	   usage is less than previous usage.  */
	ep[1] = NULL;
      else
	{
	  /* We cannot use __environ as is and need to copy over the
	     __environ contents into an array managed via
	     __environ_array_list.  */

	  struct environ_array *target_array;
	  if (__environ_array_list != NULL
	      && required_size <= __environ_array_list->allocated)
	    /* Existing array has enough room.  Contents is copied below.  */
	    target_array = __environ_array_list;
	  else
	    {
	      /* Allocate a new array.  */
	      target_array = __environ_new_array (required_size);
	      if (target_array == NULL)
		{
		  UNLOCK;
		  return -1;
		}
	    }

	  /* Copy over the __environ array contents.  This forward
	     copy slides backwards part of the array if __environ
	     points into target_array->array.  This happens if an
	     application makes an assignment like:

	       environ = &environ[1];

	     The forward copy avoids clobbering values that still
	     needing copying.  This code handles the case
	     start_environ == ep == NULL, too.  */
	  size_t i;
	  for (i = 0; start_environ + i < ep; ++i)
	    /* Regular store because unless there has been direct
	       manipulation of the environment, target_array is still
	       a private copy.  */
	    target_array->array[i] = atomic_load_relaxed (start_environ + i);

	  /* This is the new place where we should add the element.  */
	  ep = target_array->array + i;

	  /* Add the null terminator in case there was a pointer there
	     previously.  */
	  ep[1] = NULL;

	  /* And __environ should be repointed to our array.  */
	  result_environ = target_array->array;
	}
    }

  if (replace || *ep == NULL)
    {
      char *np;

      /* Use the user string if given.  */
      if (combined != NULL)
	np = (char *) combined;
      else
	{
	  const size_t varlen = namelen + 1 + vallen;
#ifdef USE_TSEARCH
	  char *new_value = malloc (varlen);
	  if (new_value == NULL)
	    {
	      UNLOCK;
	      return -1;
	    }
# ifdef _LIBC
	  __mempcpy (__mempcpy (__mempcpy (new_value, name, namelen), "=", 1),
		     value, vallen);
# else
	  memcpy (new_value, name, namelen);
	  new_value[namelen] = '=';
	  memcpy (&new_value[namelen + 1], value, vallen);
# endif

	  np = KNOWN_VALUE (new_value);
	  if (__glibc_likely (np == NULL))
#endif
	    {
#ifdef USE_TSEARCH
	      np = new_value;
#endif
	      /* And remember the value.  */
	      STORE_VALUE (np);
	    }
#ifdef USE_TSEARCH
	  else
	    free (new_value);
#endif
	}

      /* Use release MO so that loads are sufficient to observe the
	 pointer contents because the CPU carries the dependency for
	 us.  This also acts as a thread fence, making getenv
	 async-signal-safe.  */
      atomic_store_release (ep, np);
      atomic_store_release (&__environ, result_environ);
    }

  UNLOCK;

  return 0;
}

int
setenv (const char *name, const char *value, int replace)
{
  if (name == NULL || *name == '\0' || strchr (name, '=') != NULL)
    {
      __set_errno (EINVAL);
      return -1;
    }

  return __add_to_environ (name, value, NULL, replace);
}

int
unsetenv (const char *name)
{
  size_t len;
  char **ep;

  if (name == NULL || *name == '\0' || strchr (name, '=') != NULL)
    {
      __set_errno (EINVAL);
      return -1;
    }

  len = strlen (name);

  LOCK;

  ep = atomic_load_relaxed (&__environ);
  if (ep != NULL)
    while (true)
      {
	char *entry = atomic_load_relaxed (ep);
	if (entry == NULL)
	  break;
	if (strncmp (entry, name, len) == 0 && entry[len] == '=')
	  {
	    /* Found it.  Remove this pointer by moving later ones back.  */
	    char **dp = ep;

	    while (true)
	      {
		char *next_value = atomic_load_relaxed (dp + 1);
		/* This store overwrites a value that has been
		   removed, or that has already been written to a
		   previous value.  Release MO so that this store does
		   not get reordered before the counter update in the
		   previous loop iteration.  */
		atomic_store_release (dp, next_value);
		/* Release store synchronizes with acquire loads in
		   getenv.  Non-atomic update because there is just
		   one writer due to the lock.

		   See discussion of the counter check in getenv for
		   an explanation why this is sufficient synchronization.  */
		atomic_store_release (&__environ_counter,
				      atomic_load_relaxed (&__environ_counter)
				      + 1);
		if (next_value == NULL)
		  break;
		++dp;
	      }
	    /* Continue the loop in case NAME appears again.  */
	  }
	else
	  ++ep;
      }

  UNLOCK;

  return 0;
}

/* The `clearenv' was planned to be added to POSIX.1 but probably
   never made it.  Nevertheless the POSIX.9 standard (POSIX bindings
   for Fortran 77) requires this function.  */
int
clearenv (void)
{
  LOCK;
  char **start_environ = atomic_load_relaxed (&__environ);
  if (__environ_is_from_array_list (start_environ))
    {
      /* Store null pointers to avoid strange effects when the array
	 is reused in setenv.  */
      for (char **ep = start_environ; *ep != NULL; ++ep)
	atomic_store_relaxed (ep, NULL);
      /* Update the counter similar to unsetenv, so that the writes in
	 setenv do not need to update the counter.  */
      atomic_store_release (&__environ_counter,
			    atomic_load_relaxed (&__environ_counter) + 1);
    }

  atomic_store_relaxed (&__environ, NULL);
  UNLOCK;

  return 0;
}
#ifdef _LIBC
void
__libc_setenv_freemem (void)
{
  /* Remove all traces.  */
  clearenv ();

  /* Clear all backing arrays.  */
  while (__environ_array_list != NULL)
    {
      free (__environ_array_list->array);
      void *ptr = __environ_array_list;
      __environ_array_list = __environ_array_list->next;
      free (ptr);
    }

  /* Now remove the search tree.  */
  __tdestroy (known_values, free);
  known_values = NULL;
}

# undef setenv
# undef unsetenv
# undef clearenv
weak_alias (__setenv, setenv)
weak_alias (__unsetenv, unsetenv)
weak_alias (__clearenv, clearenv)
#endif
