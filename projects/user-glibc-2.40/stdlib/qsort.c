/* Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

/* If you consider tuning this algorithm, you should consult first:
   Engineering a sort function; Jon Bentley and M. Douglas McIlroy;
   Software - Practice and Experience; Vol. 23 (11), 1249-1265, 1993.  */

#include <errno.h>
#include <limits.h>
#include <memswap.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Swap SIZE bytes between addresses A and B.  These helpers are provided
   along the generic one as an optimization.  */

enum swap_type_t
  {
    SWAP_WORDS_64,
    SWAP_WORDS_32,
    SWAP_VOID_ARG,
    SWAP_BYTES
  };

typedef uint32_t __attribute__ ((__may_alias__)) u32_alias_t;
typedef uint64_t __attribute__ ((__may_alias__)) u64_alias_t;

static inline void
swap_words_64 (void * restrict a, void * restrict b, size_t n)
{
  do
   {
     n -= 8;
     u64_alias_t t = *(u64_alias_t *)(a + n);
     *(u64_alias_t *)(a + n) = *(u64_alias_t *)(b + n);
     *(u64_alias_t *)(b + n) = t;
   } while (n);
}

static inline void
swap_words_32 (void * restrict a, void * restrict b, size_t n)
{
  do
   {
     n -= 4;
     u32_alias_t t = *(u32_alias_t *)(a + n);
     *(u32_alias_t *)(a + n) = *(u32_alias_t *)(b + n);
     *(u32_alias_t *)(b + n) = t;
   } while (n);
}

/* Replace the indirect call with a serie of if statements.  It should help
   the branch predictor.  */
static void
do_swap (void * restrict a, void * restrict b, size_t size,
	 enum swap_type_t swap_type)
{
  if (swap_type == SWAP_WORDS_64)
    swap_words_64 (a, b, size);
  else if (swap_type == SWAP_WORDS_32)
    swap_words_32 (a, b, size);
  else
    __memswap (a, b, size);
}

/* Establish the heap condition at index K, that is, the key at K will
   not be less than either of its children, at 2 * K + 1 and 2 * K + 2
   (if they exist).  N is the last valid index. */
static inline void
siftdown (void *base, size_t size, size_t k, size_t n,
	  enum swap_type_t swap_type, __compar_d_fn_t cmp, void *arg)
{
  /* There can only be a heap condition violation if there are
     children.  */
  while (2 * k + 1 <= n)
    {
      /* Left child.  */
      size_t j = 2 * k + 1;
      /* If the right child is larger, use it.  */
      if (j < n && cmp (base + (j * size), base + ((j + 1) * size), arg) < 0)
	j++;

      /* If k is already >= to its children, we are done.  */
      if (j == k || cmp (base + (k * size), base + (j * size), arg) >= 0)
	break;

      /* Heal the violation.  */
      do_swap (base + (size * j), base + (k * size), size, swap_type);

      /* Swapping with j may have introduced a violation at j.  Fix
	 it in the next loop iteration.  */
      k = j;
    }
}

/* Establish the heap condition for the indices 0 to N (inclusive).  */
static inline void
heapify (void *base, size_t size, size_t n, enum swap_type_t swap_type,
	 __compar_d_fn_t cmp, void *arg)
{
  /* If n is odd, k = n / 2 has a left child at n, so this is the
     largest index that can have a heap condition violation regarding
     its children.  */
  size_t k = n / 2;
  while (1)
    {
      siftdown (base, size, k, n, swap_type, cmp, arg);
      if (k-- == 0)
	break;
    }
}

static enum swap_type_t
get_swap_type (void *const pbase, size_t size)
{
  if ((size & (sizeof (uint32_t) - 1)) == 0
      && ((uintptr_t) pbase) % __alignof__ (uint32_t) == 0)
    {
      if (size == sizeof (uint32_t))
	return SWAP_WORDS_32;
      else if (size == sizeof (uint64_t)
	       && ((uintptr_t) pbase) % __alignof__ (uint64_t) == 0)
	return SWAP_WORDS_64;
    }
  return SWAP_BYTES;
}


/* A non-recursive heapsort with worst-case performance of O(nlog n) and
   worst-case space complexity of O(1).  It sorts the array starting at
   BASE with n + 1 elements of SIZE bytes.  The SWAP_TYPE is the callback
   function used to swap elements, and CMP is the function used to compare
   elements.  */
static void
heapsort_r (void *base, size_t n, size_t size, __compar_d_fn_t cmp, void *arg)
{
  if (n == 0)
    return;

  enum swap_type_t swap_type = get_swap_type (base, size);

  /* Build the binary heap, largest value at the base[0].  */
  heapify (base, size, n, swap_type, cmp, arg);

  while (true)
    {
      /* Indices 0 .. n contain the binary heap.  Extract the largest
	 element put it into the final position in the array.  */
      do_swap (base, base + (n * size), size, swap_type);

      /* The heap is now one element shorter.  */
      n--;
      if (n == 0)
	break;

      /* By swapping in elements 0 and the previous value of n (now at
	 n + 1), we likely introduced a heap condition violation.  Fix
	 it for the reduced heap.  */
      siftdown (base, size, 0, n, swap_type, cmp, arg);
    }
}

/* The maximum size in bytes required by mergesort that will be provided
   through a buffer allocated in the stack.  */
#define QSORT_STACK_SIZE  1024

/* Elements larger than this value will be sorted through indirect sorting
   to minimize the need to memory swap calls.  */
#define INDIRECT_SORT_SIZE_THRES  32

struct msort_param
{
  size_t s;
  enum swap_type_t var;
  __compar_d_fn_t cmp;
  void *arg;
  char *t;
};

static void
msort_with_tmp (const struct msort_param *p, void *b, size_t n)
{
  char *b1, *b2;
  size_t n1, n2;

  if (n <= 1)
    return;

  n1 = n / 2;
  n2 = n - n1;
  b1 = b;
  b2 = (char *) b + (n1 * p->s);

  msort_with_tmp (p, b1, n1);
  msort_with_tmp (p, b2, n2);

  char *tmp = p->t;
  const size_t s = p->s;
  __compar_d_fn_t cmp = p->cmp;
  void *arg = p->arg;
  switch (p->var)
    {
    case SWAP_WORDS_32:
      while (n1 > 0 && n2 > 0)
	{
	  if (cmp (b1, b2, arg) <= 0)
	    {
	      *(u32_alias_t *) tmp = *(u32_alias_t *) b1;
	      b1 += sizeof (u32_alias_t);
	      --n1;
	    }
	  else
	    {
	      *(u32_alias_t *) tmp = *(u32_alias_t *) b2;
	      b2 += sizeof (u32_alias_t);
	      --n2;
	    }
	  tmp += sizeof (u32_alias_t);
	}
      break;
    case SWAP_WORDS_64:
      while (n1 > 0 && n2 > 0)
	{
	  if (cmp (b1, b2, arg) <= 0)
	    {
	      *(u64_alias_t *) tmp = *(u64_alias_t *) b1;
	      b1 += sizeof (u64_alias_t);
	      --n1;
	    }
	  else
	    {
	      *(u64_alias_t *) tmp = *(u64_alias_t *) b2;
	      b2 += sizeof (u64_alias_t);
	      --n2;
	    }
	  tmp += sizeof (u64_alias_t);
	}
      break;
    case SWAP_VOID_ARG:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (*(const void **) b1, *(const void **) b2, arg) <= 0)
	    {
	      *(void **) tmp = *(void **) b1;
	      b1 += sizeof (void *);
	      --n1;
	    }
	  else
	    {
	      *(void **) tmp = *(void **) b2;
	      b2 += sizeof (void *);
	      --n2;
	    }
	  tmp += sizeof (void *);
	}
      break;
    default:
      while (n1 > 0 && n2 > 0)
	{
	  if (cmp (b1, b2, arg) <= 0)
	    {
	      tmp = (char *) __mempcpy (tmp, b1, s);
	      b1 += s;
	      --n1;
	    }
	  else
	    {
	      tmp = (char *) __mempcpy (tmp, b2, s);
	      b2 += s;
	      --n2;
	    }
	}
      break;
    }

  if (n1 > 0)
    memcpy (tmp, b1, n1 * s);
  memcpy (b, p->t, (n - n2) * s);
}

static void
__attribute_used__
indirect_msort_with_tmp (const struct msort_param *p, void *b, size_t n,
			 size_t s)
{
  /* Indirect sorting.  */
  char *ip = (char *) b;
  void **tp = (void **) (p->t + n * sizeof (void *));
  void **t = tp;
  void *tmp_storage = (void *) (tp + n);

  while ((void *) t < tmp_storage)
    {
      *t++ = ip;
      ip += s;
    }
  msort_with_tmp (p, p->t + n * sizeof (void *), n);

  /* tp[0] .. tp[n - 1] is now sorted, copy around entries of
     the original array.  Knuth vol. 3 (2nd ed.) exercise 5.2-10.  */
  char *kp;
  size_t i;
  for (i = 0, ip = (char *) b; i < n; i++, ip += s)
    if ((kp = tp[i]) != ip)
      {
	size_t j = i;
	char *jp = ip;
	memcpy (tmp_storage, ip, s);

	do
	  {
	    size_t k = (kp - (char *) b) / s;
	    tp[j] = jp;
	    memcpy (jp, kp, s);
	    j = k;
	    jp = kp;
	    kp = tp[k];
	  }
	while (kp != ip);

	tp[j] = jp;
	memcpy (jp, tmp_storage, s);
      }
}

void
__qsort_r (void *const pbase, size_t total_elems, size_t size,
	   __compar_d_fn_t cmp, void *arg)
{
  if (total_elems <= 1)
    return;

  /* Align to the maximum size used by the swap optimization.  */
  _Alignas (uint64_t) char tmp[QSORT_STACK_SIZE];
  size_t total_size = total_elems * size;
  char *buf;

  if (size > INDIRECT_SORT_SIZE_THRES)
    total_size = 2 * total_elems * sizeof (void *) + size;

  if (total_size <= sizeof tmp)
    buf = tmp;
  else
    {
      int save = errno;
      buf = malloc (total_size);
      __set_errno (save);
      if (buf == NULL)
	{
	  /* Fallback to heapsort in case of memory failure.  */
	  heapsort_r (pbase, total_elems - 1, size, cmp, arg);
	  return;
	}
    }

  if (size > INDIRECT_SORT_SIZE_THRES)
    {
      const struct msort_param msort_param =
	{
	  .s = sizeof (void *),
	  .cmp = cmp,
	  .arg = arg,
	  .var = SWAP_VOID_ARG,
	  .t = buf,
	};
      indirect_msort_with_tmp (&msort_param, pbase, total_elems, size);
    }
  else
    {
      const struct msort_param msort_param =
	{
	  .s = size,
	  .cmp = cmp,
	  .arg = arg,
	  .var = get_swap_type (pbase, size),
	  .t = buf,
	};
      msort_with_tmp (&msort_param, pbase, total_elems);
    }

  if (buf != tmp)
    free (buf);
}
libc_hidden_def (__qsort_r)
weak_alias (__qsort_r, qsort_r)

void
qsort (void *b, size_t n, size_t s, __compar_fn_t cmp)
{
  return __qsort_r (b, n, s, (__compar_d_fn_t) cmp, NULL);
}
libc_hidden_def (qsort)
