/* THREAD_* accessors.  x86_64 version.
   Copyright (C) 2002-2024 Free Software Foundation, Inc.
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
# define THREAD_GETMEM(descr, member) \
  ({ __typeof (descr->member) __value;					      \
     _Static_assert (sizeof (__value) == 1				      \
		     || sizeof (__value) == 4				      \
		     || sizeof (__value) == 8,				      \
		     "size of per-thread data");			      \
     if (sizeof (__value) == 1)						      \
       asm volatile ("movb %%fs:%P2,%b0"				      \
		     : "=q" (__value)					      \
		     : "0" (0), "i" (offsetof (struct pthread, member)));     \
     else if (sizeof (__value) == 4)					      \
       asm volatile ("movl %%fs:%P1,%0"					      \
		     : "=r" (__value)					      \
		     : "i" (offsetof (struct pthread, member)));	      \
     else /* 8 */								      \
       {								      \
	 asm volatile ("movq %%fs:%P1,%q0"				      \
		       : "=r" (__value)					      \
		       : "i" (offsetof (struct pthread, member)));	      \
       }								      \
     __value; })

/* THREAD_GETMEM already forces a read.  */
#define THREAD_GETMEM_VOLATILE(descr, member) THREAD_GETMEM (descr, member)

/* Same as THREAD_GETMEM, but the member offset can be non-constant.  */
# define THREAD_GETMEM_NC(descr, member, idx) \
  ({ __typeof (descr->member[0]) __value;				      \
     _Static_assert (sizeof (__value) == 1				      \
		     || sizeof (__value) == 4				      \
		     || sizeof (__value) == 8,				      \
		     "size of per-thread data");			      \
     if (sizeof (__value) == 1)						      \
       asm volatile ("movb %%fs:%P2(%q3),%b0"				      \
		     : "=q" (__value)					      \
		     : "0" (0), "i" (offsetof (struct pthread, member[0])),   \
		       "r" (idx));					      \
     else if (sizeof (__value) == 4)					      \
       asm volatile ("movl %%fs:%P1(,%q2,4),%0"				      \
		     : "=r" (__value)					      \
		     : "i" (offsetof (struct pthread, member[0])), "r" (idx));\
     else /* 8 */							      \
       {								      \
	 asm volatile ("movq %%fs:%P1(,%q2,8),%q0"			      \
		       : "=r" (__value)					      \
		       : "i" (offsetof (struct pthread, member[0])),	      \
			 "r" (idx));					      \
       }								      \
     __value; })


/* Loading addresses of objects on x86-64 needs to be treated special
   when generating PIC code.  */
#ifdef __pic__
# define IMM_MODE "nr"
#else
# define IMM_MODE "ir"
#endif


/* Set member of the thread descriptor directly.  */
# define THREAD_SETMEM(descr, member, value) \
  ({									      \
     _Static_assert (sizeof (descr->member) == 1			      \
		     || sizeof (descr->member) == 4			      \
		     || sizeof (descr->member) == 8,			      \
		     "size of per-thread data");			      \
     if (sizeof (descr->member) == 1)					      \
       asm volatile ("movb %b0,%%fs:%P1" :				      \
		     : "iq" (value),					      \
		       "i" (offsetof (struct pthread, member)));	      \
     else if (sizeof (descr->member) == 4)				      \
       asm volatile ("movl %0,%%fs:%P1" :				      \
		     : IMM_MODE (value),				      \
		       "i" (offsetof (struct pthread, member)));	      \
     else /* 8 */							      \
       {								      \
	 /* Since movq takes a signed 32-bit immediate or a register source   \
	    operand, use "er" constraint for 32-bit signed integer constant   \
	    or register.  */						      \
	 asm volatile ("movq %q0,%%fs:%P1" :				      \
		       : "er" ((uint64_t) cast_to_integer (value)),	      \
			 "i" (offsetof (struct pthread, member)));	      \
       }})


/* Same as THREAD_SETMEM, but the member offset can be non-constant.  */
# define THREAD_SETMEM_NC(descr, member, idx, value) \
  ({									      \
     _Static_assert (sizeof (descr->member[0]) == 1			      \
		     || sizeof (descr->member[0]) == 4			      \
		     || sizeof (descr->member[0]) == 8,			      \
		     "size of per-thread data");			      \
     if (sizeof (descr->member[0]) == 1)				      \
       asm volatile ("movb %b0,%%fs:%P1(%q2)" :				      \
		     : "iq" (value),					      \
		       "i" (offsetof (struct pthread, member[0])),	      \
		       "r" (idx));					      \
     else if (sizeof (descr->member[0]) == 4)				      \
       asm volatile ("movl %0,%%fs:%P1(,%q2,4)" :			      \
		     : IMM_MODE (value),				      \
		       "i" (offsetof (struct pthread, member[0])),	      \
		       "r" (idx));					      \
     else /* 8 */							      \
       {								      \
	 /* Since movq takes a signed 32-bit immediate or a register source   \
	    operand, use "er" constraint for 32-bit signed integer constant   \
	    or register.  */						      \
	 asm volatile ("movq %q0,%%fs:%P1(,%q2,8)" :			      \
		       : "er" ((uint64_t) cast_to_integer (value)),	      \
			 "i" (offsetof (struct pthread, member[0])),	      \
			 "r" (idx));					      \
       }})
