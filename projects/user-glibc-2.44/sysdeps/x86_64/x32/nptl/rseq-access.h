/* RSEQ_* accessors.  x32 version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

/* Read member of the RSEQ area directly, with single-copy atomicity semantics.  */
#define RSEQ_GETMEM_ONCE(member) \
  ({									      \
     __typeof ((struct rseq_area) {}.member) __value;			      \
     _Static_assert (sizeof (__value) == 1				      \
		     || sizeof (__value) == 4				      \
		     || sizeof (__value) == 8,				      \
		     "size of rseq data");				      \
     if (sizeof (__value) == 1)						      \
       asm volatile ("movzbl %%fs:%c1(%2),%k0"				      \
		     : "=r" (__value)					      \
		     : "i" (offsetof (struct rseq_area, member)),	      \
		       "r" ((long long int) __rseq_offset)		      \
		     : "memory" );					      \
     else if (sizeof (__value) == 4)					      \
       asm volatile ("movl %%fs:%c1(%2),%0"				      \
		     : "=r" (__value)					      \
		     : "i" (offsetof (struct rseq_area, member)),	      \
		       "r" ((long long int) __rseq_offset)		      \
		     : "memory");					      \
     else /* 8 */							      \
       asm volatile ("movq %%fs:%c1(%2),%0"				      \
		     : "=r" (__value)					      \
		     : "i" (offsetof (struct rseq_area, member)),	      \
		       "r" ((long long int) __rseq_offset)		      \
		     : "memory");					      \
     __value;								      \
  })

/* Read member of the RSEQ area directly.  */
#define RSEQ_GETMEM(member) RSEQ_GETMEM_ONCE(member)

/* Loading addresses of objects on x86-64 needs to be treated special
   when generating PIC code.  */
#ifdef __pic__
# define IMM_MODE "nr"
#else
# define IMM_MODE "ir"
#endif

/* Set member of the RSEQ area directly, with single-copy atomicity semantics.  */
#define RSEQ_SETMEM_ONCE(member, value) \
  ({									      \
     _Static_assert (sizeof ((struct rseq_area) {}.member) == 1		      \
		     || sizeof ((struct rseq_area) {}.member) == 4	      \
		     || sizeof ((struct rseq_area) {}.member) == 8,	      \
		     "size of rseq data");				      \
     if (sizeof ((struct rseq_area) {}.member) == 1)			      \
       asm volatile ("movb %0,%%fs:%c1(%2)"				      \
		     :							      \
		     : "iq" ((uint8_t) cast_to_integer (value)),	      \
		       "i" (offsetof (struct rseq_area, member)),	      \
		       "r" ((long long int) __rseq_offset)		      \
		     : "memory");					      \
     else if (sizeof ((struct rseq_area) {}.member) == 4)		      \
       asm volatile ("movl %0,%%fs:%c1(%2)"				      \
		     :							      \
		     : IMM_MODE ((uint32_t) cast_to_integer (value)),	      \
		       "i" (offsetof (struct rseq_area, member)),	      \
		       "r" ((long long int) __rseq_offset)		      \
		     : "memory");					      \
     else /* 8 */							      \
       /* Since movq takes a signed 32-bit immediate or a register source     \
	  operand, use "er" constraint for 32-bit signed integer constant     \
	  or register.  */						      \
       asm volatile ("movq %0,%%fs:%c1(%2)"				      \
		     :							      \
		     : "er" ((uint64_t) cast_to_integer (value)),	      \
		       "i" (offsetof (struct rseq_area, member)),	      \
		       "r" ((long long int) __rseq_offset)		      \
		     : "memory");					      \
  })

/* Set member of the RSEQ area directly.  */
#define RSEQ_SETMEM(member, value) RSEQ_SETMEM_ONCE(member, value)
