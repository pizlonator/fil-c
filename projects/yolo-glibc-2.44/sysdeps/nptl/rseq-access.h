/* RSEQ_* accessors.  Generic version.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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

#include <atomic.h>

/* Read member of the RSEQ area directly.  */
#define RSEQ_GETMEM(member) \
  RSEQ_SELF()->member

/* Set member of the RSEQ area directly.  */
#define RSEQ_SETMEM(member, value) \
  RSEQ_SELF()->member = (value)

/* Static assert for types that can't be loaded/stored atomically on the
   current architecture.  */
#if HAVE_64B_ATOMICS
#define __RSEQ_ASSERT_ATOMIC(member) \
   _Static_assert (sizeof (RSEQ_SELF()->member) == 1			      \
		   || sizeof (RSEQ_SELF()->member) == 4			      \
		   || sizeof (RSEQ_SELF()->member) == 8,		      \
		   "size of rseq data")
#else
#define __RSEQ_ASSERT_ATOMIC(member) \
   _Static_assert (sizeof (RSEQ_SELF()->member) == 1			      \
		   || sizeof (RSEQ_SELF()->member) == 4,		      \
		   "size of rseq data")
#endif

/* Read member of the RSEQ area directly, with single-copy atomicity semantics.  */
#define RSEQ_GETMEM_ONCE(member) \
  ({									      \
     __RSEQ_ASSERT_ATOMIC(member);					      \
     (*(volatile __typeof (RSEQ_SELF()->member) *)&RSEQ_SELF()->member);      \
  })

/* Set member of the RSEQ area directly, with single-copy atomicity semantics.  */
#define RSEQ_SETMEM_ONCE(member, value) \
  ({									      \
     __RSEQ_ASSERT_ATOMIC(member);					      \
     (*(volatile __typeof (RSEQ_SELF()->member) *)&RSEQ_SELF()->member = (value)); \
  })
