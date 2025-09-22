/* Copyright (C) 2000-2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <assert.h>
#include <stdlib.h>
#include <system.h>

/* Before including this file the following macros must be defined:

   NAME      name of the hash table structure.
   TYPE      data type of the hash table entries
   COMPARE   comparison function taking two pointers to TYPE objects

   The following macros if present select features:

   ITERATE   iterating over the table entries is possible
   REVERSE   iterate in reverse order of insert
 */


static size_t
lookup (NAME *htab, HASHTYPE hval, TYPE val __attribute__ ((unused)))
{
  /* First hash function: simply take the modul but prevent zero.  Small values
     can skip the division, which helps performance when this is common.  */
  size_t idx = 1 + (hval < htab->size ? hval : hval % htab->size);

  if (htab->table[idx].hashval != 0)
    {
      HASHTYPE hash;

      if (htab->table[idx].hashval == hval
	  && COMPARE (htab->table[idx].data, val) == 0)
	return idx;

      /* Second hash function as suggested in [Knuth].  */
      hash = 1 + hval % (htab->size - 2);

      do
	{
	  if (idx <= hash)
	    idx = htab->size + idx - hash;
	  else
	    idx -= hash;

	  /* If entry is found use it.  */
	  if (htab->table[idx].hashval == hval
	      && COMPARE (htab->table[idx].data, val) == 0)
	    return idx;
	}
      while (htab->table[idx].hashval);
    }
  return idx;
}


static void
insert_entry_2 (NAME *htab, HASHTYPE hval, size_t idx, TYPE data)
{
#ifdef ITERATE
  if (htab->table[idx].hashval == 0)
    {
# ifdef REVERSE
      htab->table[idx].next = htab->first;
      htab->first = &htab->table[idx];
# else
      /* Add the new value to the list.  */
      if (htab->first == NULL)
	htab->first = htab->table[idx].next = &htab->table[idx];
      else
	{
	  htab->table[idx].next = htab->first->next;
	  htab->first = htab->first->next = &htab->table[idx];
	}
# endif
    }
#endif

  htab->table[idx].hashval = hval;
  htab->table[idx].data = data;

  ++htab->filled;
  if (100 * htab->filled > 90 * htab->size)
    {
      /* Table is filled more than 90%.  Resize the table.  */
#ifdef ITERATE
      __typeof__ (htab->first) first;
# ifndef REVERSE
      __typeof__ (htab->first) runp;
# endif
#else
      size_t old_size = htab->size;
#endif
#define _TABLE(name) \
      name##_ent *table = htab->table
#define TABLE(name) _TABLE (name)
      TABLE(NAME);

      htab->size = next_prime (htab->size * 2);
      htab->filled = 0;
#ifdef ITERATE
      first = htab->first;
      htab->first = NULL;
#endif
      htab->table = calloc ((1 + htab->size), sizeof (htab->table[0]));
      if (htab->table == NULL)
	{
	  /* We cannot enlarge the table.  Live with what we got.  This
	     might lead to an infinite loop at some point, though.  */
	  htab->table = table;
	  return;
	}

      /* Add the old entries to the new table.  When iteration is
	 supported we maintain the order.  */
#ifdef ITERATE
# ifdef REVERSE
      while (first != NULL)
	{
	  insert_entry_2 (htab, first->hashval,
			  lookup (htab, first->hashval, first->data),
			  first->data);

	  first = first->next;
	}
# else
      assert (first != NULL);
      runp = first = first->next;
      do
	insert_entry_2 (htab, runp->hashval,
			lookup (htab, runp->hashval, runp->data), runp->data);
      while ((runp = runp->next) != first);
# endif
#else
      for (idx = 1; idx <= old_size; ++idx)
	if (table[idx].hashval != 0)
	  insert_entry_2 (htab, table[idx].hashval,
			  lookup (htab, table[idx].hashval, table[idx].data),
			  table[idx].data);
#endif

      free (table);
    }
}


int
#define INIT(name) _INIT (name)
#define _INIT(name) \
  name##_init
INIT(NAME) (NAME *htab, size_t init_size)
{
  /* We need the size to be a prime.  */
  init_size = next_prime (init_size);

  /* Initialize the data structure.  */
  htab->size = init_size;
  htab->filled = 0;
#ifdef ITERATE
  htab->first = NULL;
#endif
  htab->table = calloc ((init_size + 1), sizeof (htab->table[0]));
  if (htab->table == NULL)
    return -1;

  return 0;
}


int
#define FREE(name) _FREE (name)
#define _FREE(name) \
  name##_free
FREE(NAME) (NAME *htab)
{
  free (htab->table);
  return 0;
}


int
#define INSERT(name) _INSERT (name)
#define _INSERT(name) \
  name##_insert
INSERT(NAME) (NAME *htab, HASHTYPE hval, TYPE data)
{
  size_t idx;

  /* Make the hash value nonzero.  */
  hval = hval ?: 1;

  idx = lookup (htab, hval, data);

  if (htab->table[idx].hashval != 0)
    /* We don't want to overwrite the old value.  */
    return -1;

  /* An empty bucket has been found.  */
  insert_entry_2 (htab, hval, idx, data);
  return 0;
}


#ifdef OVERWRITE
int
#define INSERT(name) _INSERT (name)
#define _INSERT(name) \
  name##_overwrite
INSERT(NAME) (NAME *htab, HASHTYPE hval, TYPE data)
{
  size_t idx;

  /* Make the hash value nonzero.  */
  hval = hval ?: 1;

  idx = lookup (htab, hval, data);

  /* The correct bucket has been found.  */
  insert_entry_2 (htab, hval, idx, data);
  return 0;
}
#endif


TYPE
#define FIND(name) _FIND (name)
#define _FIND(name) \
  name##_find
FIND(NAME) (NAME *htab, HASHTYPE hval, TYPE val)
{
  size_t idx;

  /* Make the hash value nonzero.  */
  hval = hval ?: 1;

  idx = lookup (htab, hval, val);

  if (htab->table[idx].hashval == 0)
    return NULL;

  return htab->table[idx].data;
}


#ifdef ITERATE
# define ITERATEFCT(name) _ITERATEFCT (name)
# define _ITERATEFCT(name) \
  name##_iterate
TYPE
ITERATEFCT(NAME) (NAME *htab, void **ptr)
{
  void *p = *ptr;

# define TYPENAME(name) _TYPENAME (name)
# define _TYPENAME(name) name##_ent

# ifdef REVERSE
  if (p == NULL)
    p = htab->first;
  else
    p = ((TYPENAME(NAME) *) p)->next;

  if (p == NULL)
    {
      *ptr = NULL;
      return NULL;
    }
# else
  if (p == NULL)
    {
      if (htab->first == NULL)
	return NULL;
      p = htab->first->next;
    }
  else
    {
      if (p == htab->first)
	return NULL;

      p = ((TYPENAME(NAME) *) p)->next;
    }
# endif

  /* Prepare the next element.  If possible this will pull the data
     into the cache, for reading.  */
  __builtin_prefetch (((TYPENAME(NAME) *) p)->next, 0, 2);

  return ((TYPENAME(NAME) *) (*ptr = p))->data;
}
#endif
