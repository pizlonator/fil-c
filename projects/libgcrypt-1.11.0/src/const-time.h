/* const-time.h  -  Constant-time functions
 *      Copyright (C) 2023  g10 Code GmbH
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
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GCRY_CONST_TIME_H
#define GCRY_CONST_TIME_H

#include "types.h"


#define ct_not_memequal _gcry_ct_not_memequal
#define ct_memequal _gcry_ct_memequal
#define ct_memmov_cond _gcry_ct_memmov_cond


#ifndef HAVE_GCC_ASM_VOLATILE_MEMORY
extern volatile unsigned int _gcry_ct_vzero;
extern volatile unsigned int _gcry_ct_vone;
#endif


/*
 * Return 0 if A is 0 and return 1 otherwise.
 */
static inline unsigned int
ct_is_not_zero (unsigned int a)
{
  /* Sign bit set if A != 0. */
  a = a | (-a);

  return a >> (sizeof(unsigned int) * 8 - 1);
}

/*
 * Return 1 if A is 0 and return 0 otherwise.
 */
static inline unsigned int
ct_is_zero (unsigned int a)
{
  /* Sign bit set if A == 0. */
  a = ~a & ~(-a);

  return a >> (sizeof(unsigned int) * 8 - 1);
}

/*
 * Return 1 if it's not same, 0 if same.
 */
static inline unsigned int
ct_not_equal_byte (unsigned char b0, unsigned char b1)
{
  unsigned int diff;

  diff = b0;
  diff ^= b1;

  return (0U - diff) >> (sizeof (unsigned int)*8 - 1);
}

/* Compare byte-arrays of length LEN, return 1 if it's not same, 0
   otherwise.  We use pointer of void *, so that it can be used with
   any structure.  */
unsigned int _gcry_ct_not_memequal (const void *b1, const void *b2, size_t len);

/* Compare byte-arrays of length LEN, return 0 if it's not same, 1
   otherwise.  We use pointer of void *, so that it can be used with
   any structure.  */
unsigned int _gcry_ct_memequal (const void *b1, const void *b2, size_t len);

/*
 * Return all bits set if A is 1 and return 0 otherwise.
 */
#ifdef HAVE_GCC_ASM_VOLATILE_MEMORY
#  define DEFINE_CT_TYPE_GEN_MASK(name, type) \
     static inline type \
     ct_##name##_gen_mask (unsigned long op_enable) \
     { \
       type mask = -(type)op_enable; \
       asm volatile ("\n" : "+r" (mask) :: "memory"); \
       return mask; \
     }
#else
#  define DEFINE_CT_TYPE_GEN_MASK(name, type) \
     static inline type \
     ct_##name##_gen_mask (unsigned long op_enable) \
     { \
       type mask = (type)_gcry_ct_vzero - (type)op_enable; \
       return mask; \
     }
#endif
DEFINE_CT_TYPE_GEN_MASK(uintptr, uintptr_t)
DEFINE_CT_TYPE_GEN_MASK(ulong, unsigned long)

/*
 * Return all bits set if A is 0 and return 1 otherwise.
 */
#ifdef HAVE_GCC_ASM_VOLATILE_MEMORY
#  define DEFINE_CT_TYPE_GEN_INV_MASK(name, type) \
     static inline type \
     ct_##name##_gen_inv_mask (unsigned long op_enable) \
     { \
       type mask = (type)op_enable - (type)1; \
       asm volatile ("\n" : "+r" (mask) :: "memory"); \
       return mask; \
     }
#else
#  define DEFINE_CT_TYPE_GEN_INV_MASK(name, type) \
     static inline type \
     ct_##name##_gen_inv_mask (unsigned long op_enable) \
     { \
       type mask = (type)op_enable - (type)_gcry_ct_vone; \
       return mask; \
     }
#endif
DEFINE_CT_TYPE_GEN_INV_MASK(uintptr, uintptr_t)
DEFINE_CT_TYPE_GEN_INV_MASK(ulong, unsigned long)

/*
 *  Return A when OP_ENABLED=1
 *  otherwise, return B
 */
#define DEFINE_CT_TYPE_SELECT_FUNC(name, type) \
  static inline type \
  ct_##name##_select (type a, type b, unsigned long op_enable) \
  { \
    type mask_b = ct_##name##_gen_inv_mask(op_enable); \
    type mask_a = ct_##name##_gen_mask(op_enable); \
    return (mask_a & a) | (mask_b & b); \
  }
DEFINE_CT_TYPE_SELECT_FUNC(uintptr, uintptr_t)
DEFINE_CT_TYPE_SELECT_FUNC(ulong, unsigned long)

/*
 *  Return NULL when OP_ENABLED=1
 *  otherwise, return W
 */
static inline gcry_sexp_t
sexp_null_cond (gcry_sexp_t w, unsigned long op_enable)
{
  uintptr_t o = ct_uintptr_select((uintptr_t)NULL, (uintptr_t)w, op_enable);
  return (gcry_sexp_t)(void *)o;
}

/*
 * Copy LEN bytes from memory area SRC to memory area DST, when
 * OP_ENABLED=1.  When DST <= SRC, the memory areas may overlap.  When
 * DST > SRC, the memory areas must not overlap.
 */
void _gcry_ct_memmov_cond (void *dst, const void *src, size_t len,
			   unsigned long op_enable);

#endif /*GCRY_CONST_TIME_H*/
