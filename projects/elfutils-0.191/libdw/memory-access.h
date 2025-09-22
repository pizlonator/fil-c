/* Unaligned memory access functionality.
   Copyright (C) 2000-2014, 2018 Red Hat, Inc.
   This file is part of elfutils.

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

#ifndef _MEMORY_ACCESS_H
#define _MEMORY_ACCESS_H 1

#include <limits.h>
#include <stdint.h>

#include <system.h>

/* Number decoding macros.  See 7.6 Variable Length Data.  */

#define len_leb128(var) ((8 * sizeof (var) + 6) / 7)

static inline size_t
__libdw_max_len_leb128 (const size_t type_len,
			const unsigned char *addr, const unsigned char *end)
{
  const size_t pointer_len = likely (addr < end) ? end - addr : 0;
  return likely (type_len <= pointer_len) ? type_len : pointer_len;
}

static inline size_t
__libdw_max_len_uleb128 (const unsigned char *addr, const unsigned char *end)
{
  const size_t type_len = len_leb128 (uint64_t);
  return __libdw_max_len_leb128 (type_len, addr, end);
}

static inline size_t
__libdw_max_len_sleb128 (const unsigned char *addr, const unsigned char *end)
{
  /* Subtract one step, so we don't shift into sign bit.  */
  const size_t type_len = len_leb128 (int64_t) - 1;
  return __libdw_max_len_leb128 (type_len, addr, end);
}

#define get_uleb128_step(var, addr, nth)				      \
  do {									      \
    unsigned char __b = *(addr)++;					      \
    (var) |= (__typeof (var)) (__b & 0x7f) << ((nth) * 7);		      \
    if (likely ((__b & 0x80) == 0))					      \
      return (var);							      \
  } while (0)

static inline uint64_t
__libdw_get_uleb128 (const unsigned char **addrp, const unsigned char *end)
{
  const size_t max = __libdw_max_len_uleb128 (*addrp, end);
  if (unlikely (max == 0))
    return UINT64_MAX;

  uint64_t acc = 0;

  /* Unroll the first step to help the compiler optimize
     for the common single-byte case.  */
  get_uleb128_step (acc, *addrp, 0);

  for (size_t i = 1; i < max; ++i)
    get_uleb128_step (acc, *addrp, i);
  /* Other implementations set VALUE to UINT_MAX in this
     case.  So we better do this as well.  */
  return UINT64_MAX;
}

static inline uint64_t
__libdw_get_uleb128_unchecked (const unsigned char **addrp)
{
  uint64_t acc = 0;

  /* Unroll the first step to help the compiler optimize
     for the common single-byte case.  */
  get_uleb128_step (acc, *addrp, 0);

  const size_t max = len_leb128 (uint64_t);
  for (size_t i = 1; i < max; ++i)
    get_uleb128_step (acc, *addrp, i);
  /* Other implementations set VALUE to UINT_MAX in this
     case.  So we better do this as well.  */
  return UINT64_MAX;
}

/* Note, addr needs to me smaller than end. */
#define get_uleb128(var, addr, end) ((var) = __libdw_get_uleb128 (&(addr), end))
#define get_uleb128_unchecked(var, addr) ((var) = __libdw_get_uleb128_unchecked (&(addr)))

/* The signed case is similar, but we sign-extend the result.  */

#define get_sleb128_step(var, addr, nth)				      \
  do {									      \
    unsigned char __b = *(addr)++;					      \
    (var) |= (__typeof (var)) (__b & 0x7f) << ((nth) * 7);		      \
    if (likely ((__b & 0x80) == 0))					      \
      {									      \
	if ((__b & 0x40) != 0)						      \
	  (var) |= - ((__typeof (var)) 1 << (((nth) + 1) * 7));		      \
	return (var);							      \
      }									      \
  } while (0)

static inline int64_t
__libdw_get_sleb128 (const unsigned char **addrp, const unsigned char *end)
{
  const size_t max = __libdw_max_len_sleb128 (*addrp, end);
  if (unlikely (max == 0))
    return INT64_MAX;

  /* Do the work in an unsigned type, but use implementation-defined
     behavior to cast to signed on return.  This avoids some undefined
     behavior when shifting.  */
  uint64_t acc = 0;

  /* Unroll the first step to help the compiler optimize
     for the common single-byte case.  */
  get_sleb128_step (acc, *addrp, 0);

  for (size_t i = 1; i < max; ++i)
    get_sleb128_step (acc, *addrp, i);
  if (*addrp == end)
    return INT64_MAX;

  /* There might be one extra byte.  */
  unsigned char b = **addrp;
  ++*addrp;
  if (likely ((b & 0x80) == 0))
    {
      /* We only need the low bit of the final byte, and as it is the
	 sign bit, we don't need to do anything else here.  */
      acc |= ((__typeof (acc)) b) << 7 * max;
      return acc;
    }

  /* Other implementations set VALUE to INT_MAX in this
     case.  So we better do this as well.  */
  return INT64_MAX;
}

static inline int64_t
__libdw_get_sleb128_unchecked (const unsigned char **addrp)
{
  /* Do the work in an unsigned type, but use implementation-defined
     behavior to cast to signed on return.  This avoids some undefined
     behavior when shifting.  */
  uint64_t acc = 0;

  /* Unroll the first step to help the compiler optimize
     for the common single-byte case.  */
  get_sleb128_step (acc, *addrp, 0);

  /* Subtract one step, so we don't shift into sign bit.  */
  const size_t max = len_leb128 (int64_t) - 1;
  for (size_t i = 1; i < max; ++i)
    get_sleb128_step (acc, *addrp, i);

  /* There might be one extra byte.  */
  unsigned char b = **addrp;
  ++*addrp;
  if (likely ((b & 0x80) == 0))
    {
      /* We only need the low bit of the final byte, and as it is the
	 sign bit, we don't need to do anything else here.  */
      acc |= ((__typeof (acc)) b) << 7 * max;
      return acc;
    }

  /* Other implementations set VALUE to INT_MAX in this
     case.  So we better do this as well.  */
  return INT64_MAX;
}

#define get_sleb128(var, addr, end) ((var) = __libdw_get_sleb128 (&(addr), end))
#define get_sleb128_unchecked(var, addr) ((var) = __libdw_get_sleb128_unchecked (&(addr)))


/* We use simple memory access functions in case the hardware allows it.
   The caller has to make sure we don't have alias problems.  */
#if ALLOW_UNALIGNED

# define read_2ubyte_unaligned(Dbg, Addr) \
  (unlikely ((Dbg)->other_byte_order)					      \
   ? bswap_16 (*((const uint16_t *) (Addr)))				      \
   : *((const uint16_t *) (Addr)))
# define read_2sbyte_unaligned(Dbg, Addr) \
  (unlikely ((Dbg)->other_byte_order)					      \
   ? (int16_t) bswap_16 (*((const int16_t *) (Addr)))			      \
   : *((const int16_t *) (Addr)))

# define read_4ubyte_unaligned_noncvt(Addr) \
   *((const uint32_t *) (Addr))
# define read_4ubyte_unaligned(Dbg, Addr) \
  (unlikely ((Dbg)->other_byte_order)					      \
   ? bswap_32 (*((const uint32_t *) (Addr)))				      \
   : *((const uint32_t *) (Addr)))
# define read_4sbyte_unaligned(Dbg, Addr) \
  (unlikely ((Dbg)->other_byte_order)					      \
   ? (int32_t) bswap_32 (*((const int32_t *) (Addr)))			      \
   : *((const int32_t *) (Addr)))

# define read_8ubyte_unaligned_noncvt(Addr) \
   *((const uint64_t *) (Addr))
# define read_8ubyte_unaligned(Dbg, Addr) \
  (unlikely ((Dbg)->other_byte_order)					      \
   ? bswap_64 (*((const uint64_t *) (Addr)))				      \
   : *((const uint64_t *) (Addr)))
# define read_8sbyte_unaligned(Dbg, Addr) \
  (unlikely ((Dbg)->other_byte_order)					      \
   ? (int64_t) bswap_64 (*((const int64_t *) (Addr)))			      \
   : *((const int64_t *) (Addr)))

#else

union unaligned
  {
    void *p;
    uint16_t u2;
    uint32_t u4;
    uint64_t u8;
    int16_t s2;
    int32_t s4;
    int64_t s8;
  } attribute_packed;

# define read_2ubyte_unaligned(Dbg, Addr) \
  read_2ubyte_unaligned_1 ((Dbg)->other_byte_order, (Addr))
# define read_2sbyte_unaligned(Dbg, Addr) \
  read_2sbyte_unaligned_1 ((Dbg)->other_byte_order, (Addr))
# define read_4ubyte_unaligned(Dbg, Addr) \
  read_4ubyte_unaligned_1 ((Dbg)->other_byte_order, (Addr))
# define read_4sbyte_unaligned(Dbg, Addr) \
  read_4sbyte_unaligned_1 ((Dbg)->other_byte_order, (Addr))
# define read_8ubyte_unaligned(Dbg, Addr) \
  read_8ubyte_unaligned_1 ((Dbg)->other_byte_order, (Addr))
# define read_8sbyte_unaligned(Dbg, Addr) \
  read_8sbyte_unaligned_1 ((Dbg)->other_byte_order, (Addr))

static inline uint16_t
read_2ubyte_unaligned_1 (bool other_byte_order, const void *p)
{
  const union unaligned *up = p;
  if (unlikely (other_byte_order))
    return bswap_16 (up->u2);
  return up->u2;
}
static inline int16_t
read_2sbyte_unaligned_1 (bool other_byte_order, const void *p)
{
  const union unaligned *up = p;
  if (unlikely (other_byte_order))
    return (int16_t) bswap_16 (up->u2);
  return up->s2;
}

static inline uint32_t
read_4ubyte_unaligned_noncvt (const void *p)
{
  const union unaligned *up = p;
  return up->u4;
}
static inline uint32_t
read_4ubyte_unaligned_1 (bool other_byte_order, const void *p)
{
  const union unaligned *up = p;
  if (unlikely (other_byte_order))
    return bswap_32 (up->u4);
  return up->u4;
}
static inline int32_t
read_4sbyte_unaligned_1 (bool other_byte_order, const void *p)
{
  const union unaligned *up = p;
  if (unlikely (other_byte_order))
    return (int32_t) bswap_32 (up->u4);
  return up->s4;
}

static inline uint64_t
read_8ubyte_unaligned_noncvt (const void *p)
{
  const union unaligned *up = p;
  return up->u8;
}
static inline uint64_t
read_8ubyte_unaligned_1 (bool other_byte_order, const void *p)
{
  const union unaligned *up = p;
  if (unlikely (other_byte_order))
    return bswap_64 (up->u8);
  return up->u8;
}
static inline int64_t
read_8sbyte_unaligned_1 (bool other_byte_order, const void *p)
{
  const union unaligned *up = p;
  if (unlikely (other_byte_order))
    return (int64_t) bswap_64 (up->u8);
  return up->s8;
}

#endif	/* allow unaligned */


#define read_2ubyte_unaligned_inc(Dbg, Addr) \
  ({ uint16_t t_ = read_2ubyte_unaligned (Dbg, Addr);			      \
     Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 2);		      \
     t_; })
#define read_2sbyte_unaligned_inc(Dbg, Addr) \
  ({ int16_t t_ = read_2sbyte_unaligned (Dbg, Addr);			      \
     Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 2);		      \
     t_; })

#define read_4ubyte_unaligned_inc(Dbg, Addr) \
  ({ uint32_t t_ = read_4ubyte_unaligned (Dbg, Addr);			      \
     Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 4);		      \
     t_; })
#define read_4sbyte_unaligned_inc(Dbg, Addr) \
  ({ int32_t t_ = read_4sbyte_unaligned (Dbg, Addr);			      \
     Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 4);		      \
     t_; })

#define read_8ubyte_unaligned_inc(Dbg, Addr) \
  ({ uint64_t t_ = read_8ubyte_unaligned (Dbg, Addr);			      \
     Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 8);		      \
     t_; })
#define read_8sbyte_unaligned_inc(Dbg, Addr) \
  ({ int64_t t_ = read_8sbyte_unaligned (Dbg, Addr);			      \
     Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 8);		      \
     t_; })

/* 3ubyte reads are only used for DW_FORM_addrx3 and DW_FORM_strx3.
   And are probably very rare.  They are not optimized.  They are
   handled as if reading a 4byte value with the first (for big endian)
   or last (for little endian) byte zero.  */

static inline int
file_byte_order (bool other_byte_order)
{
#if BYTE_ORDER == LITTLE_ENDIAN
  return other_byte_order ? BIG_ENDIAN : LITTLE_ENDIAN;
#else
  return other_byte_order ? LITTLE_ENDIAN : BIG_ENDIAN;
#endif
}

static inline uint32_t
read_3ubyte_unaligned (Dwarf *dbg, const unsigned char *p)
{
  union
  {
    uint32_t u4;
    unsigned char c[4];
  } d;
  bool other_byte_order = dbg->other_byte_order;

  if (file_byte_order (other_byte_order) == BIG_ENDIAN)
    {
      d.c[0] = 0x00;
      d.c[1] = p[0];
      d.c[2] = p[1];
      d.c[3] = p[2];
    }
  else
    {
      d.c[0] = p[0];
      d.c[1] = p[1];
      d.c[2] = p[2];
      d.c[3] = 0x00;
    }

  if (other_byte_order)
    return bswap_32 (d.u4);
  else
    return d.u4;
}


#define read_3ubyte_unaligned_inc(Dbg, Addr) \
  ({ uint32_t t_ = read_3ubyte_unaligned (Dbg, Addr);			      \
     Addr = (__typeof (Addr)) (((uintptr_t) (Addr)) + 3);		      \
     t_; })

#define read_addr_unaligned_inc(Nbytes, Dbg, Addr)			\
  (assert ((Nbytes) == 4 || (Nbytes) == 8),				\
    ((Nbytes) == 4 ? read_4ubyte_unaligned_inc (Dbg, Addr)		\
     : read_8ubyte_unaligned_inc (Dbg, Addr)))

#endif	/* memory-access.h */
