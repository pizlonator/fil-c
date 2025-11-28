/* graphene-alloc.c: aligned allocator
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2014  Emmanuele Bassi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "graphene-private.h"
#include "graphene-alloc-private.h"

#if defined(HAVE_POSIX_MEMALIGN) && !defined(_XOPEN_SOURCE)
# define _XOPEN_SOURCE 600
#endif

#if defined(HAVE_MEMALIGN) || defined(HAVE__ALIGNED_MALLOC)
/* Required for _aligned_malloc() and _aligned_free() on Windows */
#include <malloc.h>
#endif

#ifdef HAVE__ALIGNED_MALLOC
/* _aligned_malloc() takes parameters of aligned_malloc() in reverse order */
# define aligned_alloc(alignment, size) _aligned_malloc (size, alignment)

/* _aligned_malloc()'ed memory must be freed by _align_free() on MSVC */
# define aligned_free(x) _aligned_free (x)
#else
# define aligned_free(x) free (x)
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/*< private >
 * graphene_aligned_alloc:
 * @size: the size of the memory to allocate
 * @number: the multiples of @size to allocate
 * @alignment: the alignment to be enforced, as a power of 2
 *
 * Allocates @number times @size memory, with the given @alignment.
 *
 * If the total requested memory overflows %G_MAXSIZE, this function
 * will abort.
 *
 * If allocation fails, this function will abort, in line with
 * the behaviour of GLib.
 *
 * Returns: (transfer full): the allocated memory
 */
void *
graphene_aligned_alloc (size_t size,
                        size_t number,
                        size_t alignment)
{
  void *res = NULL;
  size_t max_size = (size_t) -1;
  size_t real_size;

  if (size == 0 || number == 0)
    return NULL;

  if (size > 0 && number > max_size / size)
    {
#ifndef G_DISABLE_ASSERT
      fprintf (stderr,
               "Overflow in the allocation of (%lu x %lu) bytes\n",
               (unsigned long) size,
               (unsigned long) number);
      abort ();
#else
      return NULL;
#endif
    }

  real_size = size * number;

#ifndef G_DISABLE_ASSERT
  errno = 0;
#endif

#if defined(HAVE_POSIX_MEMALIGN)
  errno = posix_memalign (&res, alignment, real_size);
#elif defined(HAVE_ALIGNED_ALLOC) || defined(HAVE__ALIGNED_MALLOC)
  /* real_size must be a multiple of alignment */
  if (real_size % alignment != 0)
    {
      size_t offset = real_size % alignment;
      real_size += (alignment - offset);
    }

  res = aligned_alloc (alignment, real_size);
#elif defined(HAVE_MEMALIGN)
  res = memalign (alignment, real_size);
#else
  res = malloc (real_size);
#endif

#ifndef G_DISABLE_ASSERT
  if (errno != 0 || res == NULL)
    {
      fprintf (stderr, "Allocation error: %s\n", strerror (errno));
      abort ();
    }
#endif

  return res;
}

/*< private >
 * graphene_aligned_alloc:
 * @size: the size of the memory to allocate
 * @number: the multiples of @size to allocate
 * @alignment: the alignment to be enforced, as a power of 2
 *
 * Allocates @number times @size memory, with the given @alignment,
 * like graphene_aligned_alloc(), but it also clears the memory.
 *
 * Returns: (transfer full): the allocated, cleared memory
 */
void *
graphene_aligned_alloc0 (size_t size,
                         size_t number,
                         size_t alignment)
{
  void *res = graphene_aligned_alloc (size, number, alignment);

  if (res != NULL)
    memset (res, 0, size * number);

  return res;
}

/*< private >
 * graphene_aligned_free:
 * @mem: the memory to deallocate
 *
 * Frees the memory allocated by graphene_aligned_alloc().
 */
void
graphene_aligned_free (void *mem)
{
  aligned_free (mem);
}
