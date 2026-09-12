/* RSEQ_* accessors.  x86_64 version.
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

/* Read member of the RSEQ area directly.  */
#define RSEQ_GETMEM(member) \
  ({									      \
     _Static_assert (sizeof ((struct rseq_area) {}.member) == 1		      \
		     || sizeof ((struct rseq_area) {}.member) == 4	      \
		     || sizeof ((struct rseq_area) {}.member) == 8,	      \
		     "size of rseq data");				      \
     ((struct rseq_area __seg_fs *)__rseq_offset)->member;		      \
  })

/* Read member of the RSEQ area directly, with single-copy atomicity semantics.  */
#define RSEQ_GETMEM_ONCE(member) \
  ({									      \
     _Static_assert (sizeof ((struct rseq_area) {}.member) == 1		      \
		     || sizeof ((struct rseq_area) {}.member) == 4	      \
		     || sizeof ((struct rseq_area) {}.member) == 8,	      \
		     "size of rseq data");				      \
     ((volatile struct rseq_area __seg_fs *)__rseq_offset)->member;	      \
  })

/* Set member of the RSEQ area directly.  */
#define RSEQ_SETMEM(member, value) \
  ({									      \
     _Static_assert (sizeof ((struct rseq_area) {}.member) == 1		      \
		     || sizeof ((struct rseq_area) {}.member) == 4	      \
		     || sizeof ((struct rseq_area) {}.member) == 8,	      \
		     "size of rseq data");				      \
     ((struct rseq_area __seg_fs *)__rseq_offset)->member = (value);	      \
  })

/* Set member of the RSEQ area directly, with single-copy atomicity semantics.  */
#define RSEQ_SETMEM_ONCE(member, value) \
  ({									      \
     _Static_assert (sizeof ((struct rseq_area) {}.member) == 1		      \
		     || sizeof ((struct rseq_area) {}.member) == 4	      \
		     || sizeof ((struct rseq_area) {}.member) == 8,	      \
		     "size of rseq data");				      \
     ((volatile struct rseq_area __seg_fs *)__rseq_offset)->member = (value); \
  })
