#ifndef _CMPH_BITBOOL_H__
#define _CMPH_BITBOOL_H__
#include "cmph_types.h" 

static const cmph_uint8 bitmask[] = { 1, 1 << 1,  1 << 2,  1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7 };

static const cmph_uint32 bitmask32[] = { 1,       1 << 1,  1 << 2,  1 << 3,  1 << 4,  1 << 5,  1 << 6, 1 << 7,
                                         1 << 8,  1 << 9,  1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15,
                                         1 << 16, 1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22, 1 << 23,
                                         1 << 24, 1 << 25, 1 << 26, 1 << 27, 1 << 28, 1 << 29, 1 << 30, 1U << 31
				       };

static const cmph_uint8 valuemask[] = { 0xfc, 0xf3, 0xcf, 0x3f};


/** \def GETBIT(array, i)
 *  \brief get the value of an 1-bit integer stored in an array. 
 *  \param array to get 1-bit integer values from
 *  \param i is the index in array to get the 1-bit integer value from
 * 
 * GETBIT(array, i) is a macro that gets the value of an 1-bit integer stored in array.
 */
#define GETBIT(array, i) ((array[i >> 3] & bitmask[i & 0x00000007]) >> (i & 0x00000007))

/** \def SETBIT(array, i)
 *  \brief set 1 to an 1-bit integer stored in an array. 
 *  \param array to store 1-bit integer values
 *  \param i is the index in array to set the the bit to 1
 * 
 * SETBIT(array, i) is a macro that sets 1 to an 1-bit integer stored in an array.
 */
#define SETBIT(array, i) (array[i >> 3] |= bitmask[i & 0x00000007])

/** \def UNSETBIT(array, i)
 *  \brief set 0 to an 1-bit integer stored in an array. 
 *  \param array to store 1-bit integer values
 *  \param i is the index in array to set the the bit to 0
 * 
 * UNSETBIT(array, i) is a macro that sets 0 to an 1-bit integer stored in an array.
 */
#define UNSETBIT(array, i) (array[i >> 3] ^= ((bitmask[i & 0x00000007])))

//#define GETBIT(array, i) (array[(i) / 8] & bitmask[(i) % 8])
//#define SETBIT(array, i) (array[(i) / 8] |= bitmask[(i) % 8])
//#define UNSETBIT(array, i) (array[(i) / 8] ^= ((bitmask[(i) % 8])))


/** \def SETVALUE1(array, i, v)
 *  \brief set a value for a 2-bit integer stored in an array initialized with 1s. 
 *  \param array to store 2-bit integer values
 *  \param i is the index in array to set the value v
 *  \param v is the value to be set
 * 
 * SETVALUE1(array, i, v) is a macro that set a value for a 2-bit integer stored in an array.
 * The array should be initialized with all bits set to 1. For example:
 * memset(array, 0xff, arraySize);
 */
#define SETVALUE1(array, i, v) (array[i >> 2] &= (cmph_uint8)((v << ((i & 0x00000003) << 1)) | valuemask[i & 0x00000003]))

/** \def SETVALUE0(array, i, v)
 *  \brief set a value for a 2-bit integer stored in an array initialized with 0s. 
 *  \param array to store 2-bit integer values
 *  \param i is the index in array to set the value v
 *  \param v is the value to be set
 * 
 * SETVALUE0(array, i, v) is a macro that set a value for a 2-bit integer stored in an array.
 * The array should be initialized with all bits set to 0. For example:
 * memset(array, 0, arraySize);
 */
#define SETVALUE0(array, i, v) (array[i >> 2] |= (cmph_uint8)(v << ((i & 0x00000003) << 1)))


/** \def GETVALUE(array, i)
 *  \brief get a value for a 2-bit integer stored in an array. 
 *  \param array to get 2-bit integer values from
 *  \param i is the index in array to get the value from
 * 
 * GETVALUE(array, i) is a macro that get a value for a 2-bit integer stored in an array.
 */
#define GETVALUE(array, i) ((cmph_uint8)((array[i >> 2] >> ((i & 0x00000003U) << 1U)) & 0x00000003U))



/** \def SETBIT32(array, i)
 *  \brief set 1 to an 1-bit integer stored in an array of 32-bit words. 
 *  \param array to store 1-bit integer values. The entries are 32-bit words.
 *  \param i is the index in array to set the the bit to 1
 * 
 * SETBIT32(array, i) is a macro that sets 1 to an 1-bit integer stored in an array of 32-bit words.
 */
#define SETBIT32(array, i) (array[i >> 5] |= bitmask32[i & 0x0000001f])

/** \def GETBIT32(array, i)
 *  \brief get the value of an 1-bit integer stored in an array of 32-bit words. 
 *  \param array to get 1-bit integer values from. The entries are 32-bit words.
 *  \param i is the index in array to get the 1-bit integer value from
 * 
 * GETBIT32(array, i) is a macro that gets the value of an 1-bit integer stored in an array of 32-bit words.
 */
#define GETBIT32(array, i) (array[i >> 5] & bitmask32[i & 0x0000001f])

/** \def UNSETBIT32(array, i)
 *  \brief set 0 to an 1-bit integer stored in an array of 32-bit words. 
 *  \param array to store 1-bit integer values. The entries ar 32-bit words
 *  \param i is the index in array to set the the bit to 0
 * 
 * UNSETBIT32(array, i) is a macro that sets 0 to an 1-bit integer stored in an array of 32-bit words.
 */
#define UNSETBIT32(array, i) (array[i >> 5] ^= ((bitmask32[i & 0x0000001f])))

#define BITS_TABLE_SIZE(n, bits_length) ((n * bits_length + 31) >> 5)

static inline void set_bits_value(cmph_uint32 * bits_table, cmph_uint32 index, cmph_uint32 bits_string, 
                                  cmph_uint32 string_length, cmph_uint32 string_mask)
{
	register cmph_uint32 bit_idx = index * string_length;
	register cmph_uint32 word_idx = bit_idx >> 5;
	register cmph_uint32 shift1 = bit_idx & 0x0000001f;
	register cmph_uint32 shift2 = 32 - shift1;
	
	bits_table[word_idx] &= ~((string_mask) << shift1);
	bits_table[word_idx] |= bits_string << shift1;
	
	if(shift2 < string_length)
	{
		bits_table[word_idx+1] &= ~((string_mask) >> shift2);
		bits_table[word_idx+1] |= bits_string >> shift2;
	};
};

static inline cmph_uint32 get_bits_value(cmph_uint32 * bits_table,cmph_uint32 index, cmph_uint32 string_length, cmph_uint32 string_mask)
{
	register cmph_uint32 bit_idx = index * string_length;
	register cmph_uint32 word_idx = bit_idx >> 5;
	register cmph_uint32 shift1 = bit_idx & 0x0000001f;
	register cmph_uint32 shift2 = 32-shift1;
	register cmph_uint32 bits_string;
	
	bits_string = (bits_table[word_idx] >> shift1) & string_mask;
	
	if(shift2 < string_length)
		bits_string |= (bits_table[word_idx+1] << shift2) & string_mask;

	return bits_string;
};

static inline void set_bits_at_pos(cmph_uint32 * bits_table, cmph_uint32 pos, cmph_uint32 bits_string, cmph_uint32 string_length)
{
	register cmph_uint32 word_idx = pos >> 5;
	register cmph_uint32 shift1 = pos & 0x0000001f;
	register cmph_uint32 shift2 = 32-shift1;
	register cmph_uint32 string_mask = (1U << string_length) - 1;
	
	bits_table[word_idx] &= ~((string_mask) << shift1);
	bits_table[word_idx] |= bits_string << shift1;
	if(shift2 < string_length)
	{
		bits_table[word_idx+1] &= ~((string_mask) >> shift2);
		bits_table[word_idx+1] |= bits_string >> shift2;
	}
};

static inline cmph_uint32 get_bits_at_pos(cmph_uint32 * bits_table,cmph_uint32 pos,cmph_uint32 string_length)
{
	register cmph_uint32 word_idx = pos >> 5;
	register cmph_uint32 shift1 = pos & 0x0000001f;
	register cmph_uint32 shift2 = 32 - shift1;
	register cmph_uint32 string_mask = (1U << string_length) - 1;
	register cmph_uint32 bits_string;
	
	bits_string = (bits_table[word_idx] >> shift1) & string_mask;

	if(shift2 < string_length)
		bits_string |= (bits_table[word_idx+1] << shift2) & string_mask;
	return bits_string;
}


#endif
