/* THREAD_* accessors.  i386 version.
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

/* Read member of the thread descriptor directly.  */
#define THREAD_GETMEM(descr, member) \
  ({									      \
     _Static_assert (sizeof (descr->member) == 1       			      \
		     || sizeof (descr->member) == 4			      \
		     || sizeof (descr->member) == 8,			      \
		     "size of per-thread data");			      \
     ((__typeof (*descr) __seg_gs *)0)->member;				      \
  })

#define THREAD_GETMEM_VOLATILE(descr, member) \
  ({									      \
     _Static_assert (sizeof (descr->member) == 1			      \
		     || sizeof (descr->member) == 4			      \
		     || sizeof (descr->member) == 8,			      \
		     "size of per-thread data");			      \
     ((volatile __typeof (*descr) __seg_gs *)0)->member;		      \
  })

/* Same as THREAD_GETMEM, but the member offset can be non-constant.  */
#define THREAD_GETMEM_NC(descr, member, idx) \
  ({									      \
     _Static_assert (sizeof (descr->member[0]) == 1			      \
		     || sizeof (descr->member[0]) == 4			      \
		     || sizeof (descr->member[0]) == 8,			      \
		     "size of per-thread data");			      \
     ((__typeof (*descr) __seg_gs *)0)->member[idx];			      \
  })

/* Set member of the thread descriptor directly.  */
#define THREAD_SETMEM(descr, member, value) \
  ({									      \
     _Static_assert (sizeof (descr->member) == 1			      \
		     || sizeof (descr->member) == 4			      \
		     || sizeof (descr->member) == 8,			      \
		     "size of per-thread data");			      \
     ((__typeof (*descr) __seg_gs *)0)->member = (value);		      \
  })

/* Same as THREAD_SETMEM, but the member offset can be non-constant.  */
#define THREAD_SETMEM_NC(descr, member, idx, value) \
  ({									      \
     _Static_assert (sizeof (descr->member[0]) == 1			      \
		     || sizeof (descr->member[0]) == 4			      \
		     || sizeof (descr->member[0]) == 8,			      \
		     "size of per-thread data");			      \
     ((__typeof (*descr) __seg_gs *)0)->member[idx] = (value);		      \
  })
