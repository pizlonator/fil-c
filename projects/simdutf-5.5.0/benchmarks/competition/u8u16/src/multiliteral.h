/*  multiliteral.h - XML Multicharacter Recognizers.
    Copyright (c) 2007, 2008, Robert D. Cameron.  
    Licensed to the public under the Open Software License 3.0.
    Licensed to International Characters, Inc., under the Academic
    Free License 3.0.

This file provides a library of routines for the efficient recognition 
of particular XML multicharacter sequences.  Sequences of length 2 are 
compared as 16 bit integers, sequences of length 3 or 4 are compared 
as 32 bit integers and other sequences of length up to 8 are compared as
64 bit integers.  The integer value for each XML multicharacter sequence
is determined as a compile-time constant for optimal efficiency.

All functions are declared inline; there is no corresponding multiliteral.c
file required.   */

#ifndef MULTILITERAL_H
#define MULTILITERAL_H

#include <assert.h>
#include <stdint.h>
#include "xmldecl.h"
#include "charsets/ASCII_EBCDIC.h"

#if BYTE_ORDER == BIG_ENDIAN
const int LOW_BYTE_SHIFT = 8;
const int HIGH_BYTE_SHIFT = 0;
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
const int LOW_BYTE_SHIFT = 0;
const int HIGH_BYTE_SHIFT = 8;
#endif

/*
Helper metafunctions.  Given 2, 4 or 8 characters comprising a sequence, 
the c2int16, c4int32, and c8int64 functions determine the corresponding 
16, 32 or 64 bit integer value.   These are template metafunctions that
must be instantiated with constant arguments to be applied at compile time.
The functions may be instantiated for ASCII or EBCDIC based byte
sequences.
For example, c2int16<ASCII, '<', '/'>::value produces the compile
time constant for the 16-bit value of an ASCII-based byte sequence
of the XML end tag opening delimiter.
*/

template <unsigned char byte1, unsigned char byte2>
struct b2int16 {
  static uint16_t const value =
    (((uint16_t) byte1) << LOW_BYTE_SHIFT) +
    (((uint16_t) byte2) << HIGH_BYTE_SHIFT);
};

template <CodeUnit_Base C, unsigned char c1, unsigned char c2>
struct c2int16 {
  static uint16_t const value = b2int16<Ord<C,c1>::value, Ord<C,c2>::value>::value;
};

template <CodeUnit_Base C, unsigned char c1, unsigned char c2,
                      unsigned char c3, unsigned char c4>
struct c4int32 {
  static uint32_t const value =
    (((uint32_t) c2int16<C,c1,c2>::value) << (2 * LOW_BYTE_SHIFT)) + 
    (((uint32_t) c2int16<C,c3,c4>::value) << (2 * HIGH_BYTE_SHIFT));
};

template <CodeUnit_Base C, unsigned char c1, unsigned char c2,
                      unsigned char c3, unsigned char c4,
                      unsigned char c5, unsigned char c6,
                      unsigned char c7, unsigned char c8>
struct c8int64 {
  static uint64_t const value =
    (((uint64_t) c4int32<C, c1, c2, c3, c4>::value) << (4 * LOW_BYTE_SHIFT)) + 
    (((uint64_t) c4int32<C, c5, c6, c7, c8>::value) << (4 * HIGH_BYTE_SHIFT));
};


/*  Specialized helpers for 3, 5, 6, and 7 character combinations. */

template <CodeUnit_Base C, unsigned char c1, unsigned char c2,
                      unsigned char c3>
struct c3int32 {
  static uint32_t const value = c4int32<C, c1, c2, c3, 0>::value;
};

template <CodeUnit_Base C, unsigned char c1, unsigned char c2,
                      unsigned char c3, unsigned char c4,
                      unsigned char c5>
struct c5int64 {
  static uint64_t const value = c8int64<C, c1, c2, c3, c4, c5, 0, 0, 0>::value;
};

template <CodeUnit_Base C, unsigned char c1, unsigned char c2,
                      unsigned char c3, unsigned char c4,
                      unsigned char c5, unsigned char c6>
struct c6int64 {
  static uint64_t const value = c8int64<C, c1, c2, c3, c4, c5, c6, 0, 0>::value;
};

template <CodeUnit_Base C, unsigned char c1, unsigned char c2,
                      unsigned char c3, unsigned char c4,
                      unsigned char c5, unsigned char c6,
                      unsigned char c7>
struct c7int64 {
  static uint64_t const value = c8int64<C, c1, c2, c3, c4, c5, c6, c7, 0>::value;
};


/* 
A second set of helper functions determines 16, 32, or 64 bit integer
values from character arrays.
Precondition:  the character array is allocated with at least the
number of required characters in each case. */
static inline uint16_t s2int16(unsigned char s[]) {
  return * ((uint16_t *) s);
}

static inline uint32_t s4int32(unsigned char s[]) {
  return * ((uint32_t *) s);
}

static inline uint64_t s8int64(unsigned char s[]) {
  return * ((uint64_t *) s);
}

static inline uint32_t s3int32(unsigned char s[]) {
  return s4int32(s) & (0xFFFFFF << LOW_BYTE_SHIFT);
}

static inline uint64_t s5int64(unsigned char s[]) {
  return s8int64(s) & (0xFFFFFFFFFFULL << (3 * LOW_BYTE_SHIFT));
}

static inline uint64_t s6int64(unsigned char s[]) {
  return s8int64(s) & (0xFFFFFFFFFFFFULL << (2 * LOW_BYTE_SHIFT));
}

static inline uint64_t s7int64(unsigned char s[]) {
  return s8int64(s) & (0xFFFFFFFFFFFFFFULL << LOW_BYTE_SHIFT);
}

template <CodeUnit_Base C, unsigned char c1, unsigned char c2>
static inline bool caseless_comp(unsigned char s[]) {
  const uint16_t lc = c2int16<C, UC2lc<c1>::value, UC2lc<c2>::value>::value;
  const uint16_t UC = c2int16<C, lc2UC<c1>::value, lc2UC<c2>::value>::value;
  const uint16_t case_mask = lc ^ UC;
  const uint16_t canon = lc & ~case_mask;
  return (s2int16(s) & ~case_mask) == canon;
}

template <CodeUnit_Base C, unsigned char c1, unsigned char c2, unsigned char c3>
static inline bool caseless_comp(unsigned char s[]) {
  const uint32_t lc = c3int32<C, UC2lc<c1>::value, UC2lc<c2>::value, UC2lc<c3>::value>::value;
  const uint32_t UC = c3int32<C, lc2UC<c1>::value, lc2UC<c2>::value, lc2UC<c3>::value>::value;
  const uint32_t case_mask = lc ^ UC;
  const uint32_t canon = lc & ~case_mask;
  return (s3int32(s) & ~case_mask) == canon;
}

template <CodeUnit_Base C, unsigned char c1, unsigned char c2, 
			   unsigned char c3, unsigned char c4>
static inline bool caseless_comp(unsigned char s[]) {
  const uint32_t lc = c4int32<C, UC2lc<c1>::value, UC2lc<c2>::value,
                                 UC2lc<c3>::value, UC2lc<c4>::value>::value;
  const uint32_t UC = c4int32<C, lc2UC<c1>::value, lc2UC<c2>::value, 
                                 lc2UC<c3>::value, lc2UC<c4>::value>::value; 
  const uint32_t case_mask = lc ^ UC;
  const uint32_t canon = lc & ~case_mask;
  return (s4int32(s) & ~case_mask) == canon;
}

template <CodeUnit_Base C, unsigned char c1, unsigned char c2, 
			   unsigned char c3, unsigned char c4,
			   unsigned char c5>
static inline bool caseless_comp(unsigned char s[]) {
  const uint64_t lc = c5int64<C, UC2lc<c1>::value, UC2lc<c2>::value,
                                 UC2lc<c3>::value, UC2lc<c4>::value,
				 UC2lc<c5>::value>::value;
  const uint64_t UC = c5int64<C, lc2UC<c1>::value, lc2UC<c2>::value, 
                                 lc2UC<c3>::value, lc2UC<c4>::value, 
				 lc2UC<c5>::value>::value; 
  const uint64_t case_mask = lc ^ UC;
  const uint64_t canon = lc & ~case_mask;
  return (s5int64(s) & ~case_mask) == canon;
}

template <CodeUnit_Base C, unsigned char c1, unsigned char c2, 
			   unsigned char c3, unsigned char c4,
			   unsigned char c5, unsigned char c6>
static inline bool caseless_comp(unsigned char s[]) {
  const uint64_t lc = c6int64<C, UC2lc<c1>::value, UC2lc<c2>::value,
                                 UC2lc<c3>::value, UC2lc<c4>::value,
				 UC2lc<c5>::value, UC2lc<c6>::value>::value;
  const uint64_t UC = c6int64<C, lc2UC<c1>::value, lc2UC<c2>::value, 
                                 lc2UC<c3>::value, lc2UC<c4>::value, 
				 lc2UC<c5>::value, lc2UC<c6>::value>::value; 
  const uint64_t case_mask = lc ^ UC;
  const uint64_t canon = lc & ~case_mask;
  return (s6int64(s) & ~case_mask) == canon;
}

template <CodeUnit_Base C, unsigned char c1, unsigned char c2, 
			   unsigned char c3, unsigned char c4,
			   unsigned char c5, unsigned char c6,
			   unsigned char c7>
static inline bool caseless_comp(unsigned char s[]) {
  const uint64_t lc = c7int64<C, UC2lc<c1>::value, UC2lc<c2>::value,
                                 UC2lc<c3>::value, UC2lc<c4>::value,
				 UC2lc<c5>::value, UC2lc<c6>::value,
				 UC2lc<c7>::value>::value;
  const uint64_t UC = c7int64<C, lc2UC<c1>::value, lc2UC<c2>::value, 
                                 lc2UC<c3>::value, lc2UC<c4>::value, 
				 lc2UC<c5>::value, lc2UC<c6>::value, 
				 lc2UC<c7>::value>::value; 
  const uint64_t case_mask = lc ^ UC;
  const uint64_t canon = lc & ~case_mask;
  return (s7int64(s) & ~case_mask) == canon;
}

template <CodeUnit_Base C, unsigned char c1, unsigned char c2, 
			   unsigned char c3, unsigned char c4,
			   unsigned char c5, unsigned char c6,
			   unsigned char c7, unsigned char c8>
static inline bool caseless_comp(unsigned char s[]) {
  const uint64_t lc = c8int64<C, UC2lc<c1>::value, UC2lc<c2>::value,
                                 UC2lc<c3>::value, UC2lc<c4>::value,
				 UC2lc<c5>::value, UC2lc<c6>::value,
				 UC2lc<c7>::value, UC2lc<c8>::value>::value;
  const uint64_t UC = c8int64<C, lc2UC<c1>::value, lc2UC<c2>::value, 
                                 lc2UC<c3>::value, lc2UC<c4>::value, 
				 lc2UC<c5>::value, lc2UC<c6>::value, 
				 lc2UC<c7>::value, lc2UC<c8>::value>::value; 
  const uint64_t case_mask = lc ^ UC;
  const uint64_t canon = lc & ~case_mask;
  return (s8int64(s) & ~case_mask) == canon;
}



#endif
