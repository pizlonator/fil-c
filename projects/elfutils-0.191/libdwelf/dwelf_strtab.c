/* ELF/DWARF string table handling.
   Copyright (C) 2000, 2001, 2002, 2005, 2016 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <inttypes.h>
#include <libelf.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "libdwelfP.h"
#include <system.h>


struct Dwelf_Strent
{
  const char *string;
  size_t len;
  struct Dwelf_Strent *next;
  struct Dwelf_Strent *left;
  struct Dwelf_Strent *right;
  size_t offset;
  char reverse[0];
};


struct memoryblock
{
  struct memoryblock *next;
  char memory[0];
};


struct Dwelf_Strtab
{
  struct Dwelf_Strent *root;
  struct memoryblock *memory;
  char *backp;
  size_t left;
  size_t total;
  bool nullstr;

  struct Dwelf_Strent null;
};


/* Cache for the pagesize.  */
static size_t ps;
/* We correct this value a bit so that `malloc' is not allocating more
   than a page. */
#define MALLOC_OVERHEAD (2 * sizeof (void *))


Dwelf_Strtab *
dwelf_strtab_init (bool nullstr)
{
  if (ps == 0)
    {
      ps = sysconf (_SC_PAGESIZE);
      assert (sizeof (struct memoryblock) < ps - MALLOC_OVERHEAD);
    }

  Dwelf_Strtab *ret = calloc (1, sizeof (struct Dwelf_Strtab));
  if (ret != NULL)
    {
      ret->nullstr = nullstr;

      if (nullstr)
	{
	  ret->null.len = 1;
	  ret->null.string = "";
	}
    }

  return ret;
}


static int
morememory (Dwelf_Strtab *st, size_t len)
{
  size_t overhead = offsetof (struct memoryblock, memory);
  len += overhead + MALLOC_OVERHEAD;

  /* Allocate nearest multiple of pagesize >= len.  */
  len = ((len / ps) + (len % ps != 0)) * ps - MALLOC_OVERHEAD;

  struct memoryblock *newmem = malloc (len);
  if (newmem == NULL)
    return 1;

  newmem->next = st->memory;
  st->memory = newmem;
  st->backp = newmem->memory;
  st->left = len - overhead;

  return 0;
}


void
dwelf_strtab_free (Dwelf_Strtab *st)
{
  struct memoryblock *mb = st->memory;

  while (mb != NULL)
    {
      void *old = mb;
      mb = mb->next;
      free (old);
    }

  free (st);
}


static Dwelf_Strent *
newstring (Dwelf_Strtab *st, const char *str, size_t len)
{
  /* Compute the amount of padding needed to make the structure aligned.  */
  size_t align = ((__alignof__ (struct Dwelf_Strent)
		   - (((uintptr_t) st->backp)
		      & (__alignof__ (struct Dwelf_Strent) - 1)))
		  & (__alignof__ (struct Dwelf_Strent) - 1));

  /* Make sure there is enough room in the memory block.  */
  if (st->left < align + sizeof (struct Dwelf_Strent) + len)
    {
      if (morememory (st, sizeof (struct Dwelf_Strent) + len))
	return NULL;

      align = 0;
    }

  /* Create the reserved string.  */
  Dwelf_Strent *newstr = (Dwelf_Strent *) (st->backp + align);
  newstr->string = str;
  newstr->len = len;
  newstr->next = NULL;
  newstr->left = NULL;
  newstr->right = NULL;
  newstr->offset = 0;
  for (int i = len - 2; i >= 0; --i)
    newstr->reverse[i] = str[len - 2 - i];
  newstr->reverse[len - 1] = '\0';
  st->backp += align + sizeof (struct Dwelf_Strent) + len;
  st->left -= align + sizeof (struct Dwelf_Strent) + len;

  return newstr;
}


/* XXX This function should definitely be rewritten to use a balancing
   tree algorithm (AVL, red-black trees).  For now a simple, correct
   implementation is enough.  */
static Dwelf_Strent **
searchstring (Dwelf_Strent **sep, Dwelf_Strent *newstr)
{
  /* More strings?  */
  if (*sep == NULL)
    {
      *sep = newstr;
      return sep;
    }

  /* Compare the strings.  */
  int cmpres = memcmp ((*sep)->reverse, newstr->reverse,
		       MIN ((*sep)->len, newstr->len) - 1);
  if (cmpres == 0)
    /* We found a matching string.  */
    return sep;
  else if (cmpres > 0)
    return searchstring (&(*sep)->left, newstr);
  else
    return searchstring (&(*sep)->right, newstr);
}


/* Add new string.  The actual string is assumed to be permanent.  */
static Dwelf_Strent *
strtab_add (Dwelf_Strtab *st, const char *str, size_t len)
{
  /* Make sure all "" strings get offset 0 but only if the table was
     created with a special null entry in mind.  */
  if (len == 1 && st->null.string != NULL)
    return &st->null;

  /* Allocate memory for the new string and its associated information.  */
  Dwelf_Strent *newstr = newstring (st, str, len);
  if (newstr == NULL)
    return NULL;

  /* Search in the array for the place to insert the string.  If there
     is no string with matching prefix and no string with matching
     leading substring, create a new entry.  */
  Dwelf_Strent **sep = searchstring (&st->root, newstr);
  if (*sep != newstr)
    {
      /* This is not the same entry.  This means we have a prefix match.  */
      if ((*sep)->len > newstr->len)
	{
	  /* Check whether we already know this string.  */
	  for (Dwelf_Strent *subs = (*sep)->next; subs != NULL;
	       subs = subs->next)
	    if (subs->len == newstr->len)
	      {
		/* We have an exact match with a substring.  Free the memory
		   we allocated.  */
		st->left += st->backp - (char *) newstr;
		st->backp = (char *) newstr;

		return subs;
	      }

	  /* We have a new substring.  This means we don't need the reverse
	     string of this entry anymore.  */
	  st->backp -= newstr->len;
	  st->left += newstr->len;

	  newstr->next = (*sep)->next;
	  (*sep)->next = newstr;
	}
      else if ((*sep)->len != newstr->len)
	{
	  /* When we get here it means that the string we are about to
	     add has a common prefix with a string we already have but
	     it is longer.  In this case we have to put it first.  */
	  st->total += newstr->len - (*sep)->len;
	  newstr->next = *sep;
	  newstr->left = (*sep)->left;
	  newstr->right = (*sep)->right;
	  *sep = newstr;
	}
      else
	{
	  /* We have an exact match.  Free the memory we allocated.  */
	  st->left += st->backp - (char *) newstr;
	  st->backp = (char *) newstr;

	  newstr = *sep;
	}
    }
  else
    st->total += newstr->len;

  return newstr;
}

Dwelf_Strent *
dwelf_strtab_add (Dwelf_Strtab *st, const char *str)
{
  return strtab_add (st, str, strlen (str) + 1);
}

Dwelf_Strent *
dwelf_strtab_add_len (Dwelf_Strtab *st, const char *str, size_t len)
{
  return strtab_add (st, str, len);
}

static void
copystrings (Dwelf_Strent *nodep, char **freep, size_t *offsetp)
{
  if (nodep->left != NULL)
    copystrings (nodep->left, freep, offsetp);

  /* Process the current node.  */
  nodep->offset = *offsetp;
  *freep = (char *) mempcpy (*freep, nodep->string, nodep->len);
  *offsetp += nodep->len;

  for (Dwelf_Strent *subs = nodep->next; subs != NULL; subs = subs->next)
    {
      assert (subs->len < nodep->len);
      subs->offset = nodep->offset + nodep->len - subs->len;
      assert (subs->offset != 0 || subs->string[0] == '\0');
    }

  if (nodep->right != NULL)
    copystrings (nodep->right, freep, offsetp);
}


Elf_Data *
dwelf_strtab_finalize (Dwelf_Strtab *st, Elf_Data *data)
{
  size_t nulllen = st->nullstr ? 1 : 0;

  /* Fill in the information.  */
  data->d_buf = malloc (st->total + nulllen);
  if (data->d_buf == NULL)
    return NULL;

  /* The first byte must always be zero if we created the table with a
     null string.  */
  if (st->nullstr)
    *((char *) data->d_buf) = '\0';

  data->d_type = ELF_T_BYTE;
  data->d_size = st->total + nulllen;
  data->d_off = 0;
  data->d_align = 1;
  data->d_version = EV_CURRENT;

  /* Now run through the tree and add all the string while also updating
     the offset members of the elfstrent records.  */
  char *endp = (char *) data->d_buf + nulllen;
  size_t copylen = nulllen;
  if (st->root)
    copystrings (st->root, &endp, &copylen);
  assert (copylen == st->total + nulllen);

  return data;
}


size_t
dwelf_strent_off (Dwelf_Strent *se)
{
  return se->offset;
}


const char *
dwelf_strent_str (Dwelf_Strent *se)
{
  return se->string;
}
