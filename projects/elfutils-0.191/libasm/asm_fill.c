/* Determine fill pattern for a section.
   Copyright (C) 2002 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <stdlib.h>
#include <string.h>

#include <libasmP.h>
#include <system.h>


int
asm_fill (AsmScn_t *asmscn, void *bytes, size_t len)
{
  struct FillPattern *pattern;
  struct FillPattern *old_pattern;

  if (asmscn == NULL)
    /* Some earlier error.  */
    return -1;

  if (bytes == NULL)
    /* Use the default pattern.  */
    pattern = (struct FillPattern *) __libasm_default_pattern;
  else
    {
      /* Allocate appropriate memory.  */
      pattern = malloc (sizeof (struct FillPattern) + len);
      if (pattern == NULL)
	return -1;

      pattern->len = len;
      memcpy (pattern->bytes, bytes, len);
    }

  old_pattern = asmscn->pattern;
  asmscn->pattern = pattern;

  /* Free the old data structure if we have allocated it.  */
  if (old_pattern != __libasm_default_pattern)
    free (old_pattern);

  return 0;
}
