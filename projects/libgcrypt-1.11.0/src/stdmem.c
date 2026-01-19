/* stdmem.c  -	private memory allocator
 * Copyright (C) 1998, 2000, 2002, 2005, 2008 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Description of the layered memory management in Libgcrypt:
 *
 *                                  [User]
 *                                    |
 *                                    |
 *                                   \ /
 *                   global.c: [MM entrance points]   -----> [user callbacks]
 *                               |          |
 *                               |          |
 *                              \ /        \ /
 *
 *      stdmem.c: [non-secure handlers] [secure handlers]
 *
 *                               |         |
 *                               |         |
 *                              \ /       \ /
 *
 *                  stdmem.c: [ memory guard ]
 *
 *                               |         |
 *                               |         |
 *                              \ /       \ /
 *
 *           libc: [ MM functions ]     secmem.c: [ secure MM functions]
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "g10lib.h"
#include "stdmem.h"
#include "secmem.h"



/*
 * Allocate memory of size n.
 * Return NULL if we are out of memory.
 */
void *
_gcry_private_malloc (size_t n)
{
  if (!n)
    {
      gpg_err_set_errno (EINVAL);
      return NULL; /* Allocating 0 bytes is undefined - we better return
                      an error to detect such coding errors.  */
    }

  return malloc( n );
}


/*
 * Allocate memory of size N from the secure memory pool.  Return NULL
 * if we are out of memory.  XHINT tells the allocator that the caller
 * used an xmalloc style call.
 */
void *
_gcry_private_malloc_secure (size_t n, int xhint)
{
  if (!n)
    {
      gpg_err_set_errno (EINVAL);
      return NULL; /* Allocating 0 bytes is undefined - better return an
                      error to detect such coding errors.  */
    }

  return _gcry_secmem_malloc (n, xhint);
}


/*
 * Realloc and clear the old space.  XHINT tells the allocator that
 * the caller used an xmalloc style call.  Returns NULL if there is
 * not enough memory.
 */
void *
_gcry_private_realloc (void *a, size_t n, int xhint)
{
  if ( _gcry_private_is_secure(a) )
    {
      return _gcry_secmem_realloc (a, n, xhint);
    }
  else
    {
      return realloc( a, n );
    }
}


/*
 * Free a memory block allocated by this or the secmem module
 */
void
_gcry_private_free (void *a)
{
  unsigned char *p = a;
  unsigned char *freep;

  if (!p)
    return;

  freep = p;

  if (!_gcry_private_is_secure (freep) ||
      !_gcry_secmem_free (freep))
    {
      free (freep);
    }
}
