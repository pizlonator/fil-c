#ifndef __CMPH_COMPRESSED_SEQ_H__
#define __CMPH_COMPRESSED_SEQ_H__

#include"select.h"

struct _compressed_seq_t
{
	cmph_uint32 n; // number of values stored in store_table
	// The length in bits of each value is decomposed into two compnents: the lg(n) MSBs are stored in rank_select data structure
	// the remaining LSBs are stored in a table of n cells, each one of rem_r bits.
	cmph_uint32 rem_r;
	cmph_uint32 total_length; // total length in bits of stored_table
	select_t sel;
	cmph_uint32 * length_rems;
	cmph_uint32 * store_table;
};

typedef struct _compressed_seq_t compressed_seq_t;

/** \fn void compressed_seq_init(compressed_seq_t * cs);
 *  \brief Initialize a compressed sequence structure.
 *  \param cs points to the compressed sequence structure to be initialized
 */
void compressed_seq_init(compressed_seq_t * cs);

/** \fn void compressed_seq_destroy(compressed_seq_t * cs);
 *  \brief Destroy a compressed sequence given as input.
 *  \param cs points to the compressed sequence structure to be destroyed
 */
void compressed_seq_destroy(compressed_seq_t * cs);

/** \fn void compressed_seq_generate(compressed_seq_t * cs, cmph_uint32 * vals_table, cmph_uint32 n);
 *  \brief Generate a compressed sequence from an input array with n values.
 *  \param cs points to the compressed sequence structure
 *  \param vals_table poiter to the array given as input
 *  \param n number of values in @see vals_table
 */
void compressed_seq_generate(compressed_seq_t * cs, cmph_uint32 * vals_table, cmph_uint32 n);


/** \fn cmph_uint32 compressed_seq_query(compressed_seq_t * cs, cmph_uint32 idx);
 *  \brief Returns the value stored at index @see idx of the compressed sequence structure.
 *  \param cs points to the compressed sequence structure
 *  \param idx index to retrieve the value from
 *  \return the value stored at index @see idx of the compressed sequence structure
 */
cmph_uint32 compressed_seq_query(compressed_seq_t * cs, cmph_uint32 idx);


/** \fn cmph_uint32 compressed_seq_get_space_usage(compressed_seq_t * cs);
 *  \brief Returns amount of space (in bits) to store the compressed sequence.
 *  \param cs points to the compressed sequence structure
 *  \return the amount of space (in bits) to store @see cs
 */
cmph_uint32 compressed_seq_get_space_usage(compressed_seq_t * cs);

void compressed_seq_dump(compressed_seq_t * cs, char ** buf, cmph_uint32 * buflen);

void compressed_seq_load(compressed_seq_t * cs, const char * buf, cmph_uint32 buflen);


/** \fn void compressed_seq_pack(compressed_seq_t *cs, void *cs_packed);
 *  \brief Support the ability to pack a compressed sequence structure into a preallocated contiguous memory space pointed by cs_packed.
 *  \param cs points to the compressed sequence structure
 *  \param cs_packed pointer to the contiguous memory area used to store the compressed sequence structure. The size of cs_packed must be at least @see compressed_seq_packed_size 
 */
void compressed_seq_pack(compressed_seq_t *cs, void *cs_packed);

/** \fn cmph_uint32 compressed_seq_packed_size(compressed_seq_t *cs);
 *  \brief Return the amount of space needed to pack a compressed sequence structure.
 *  \return the size of the packed compressed sequence structure or zero for failures
 */ 
cmph_uint32 compressed_seq_packed_size(compressed_seq_t *cs);


/** \fn cmph_uint32 compressed_seq_query_packed(void * cs_packed, cmph_uint32 idx);
 *  \brief Returns the value stored at index @see idx of the packed compressed sequence structure.
 *  \param cs_packed is a pointer to a contiguous memory area
 *  \param idx is the index to retrieve the value from
 *  \return the value stored at index @see idx of the packed compressed sequence structure
 */
cmph_uint32 compressed_seq_query_packed(void * cs_packed, cmph_uint32 idx);

#endif
