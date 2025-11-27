#ifndef __CMPH_COMPRESSED_RANK_H__
#define __CMPH_COMPRESSED_RANK_H__

#include "select.h"

struct _compressed_rank_t
{
	cmph_uint32 max_val;
	cmph_uint32 n; // number of values stored in vals_rems
	// The length in bits of each value is decomposed into two compnents: the lg(n) MSBs are stored in rank_select data structure
	// the remaining LSBs are stored in a table of n cells, each one of rem_r bits.
	cmph_uint32 rem_r;
	select_t sel;
	cmph_uint32 * vals_rems;
};

typedef struct _compressed_rank_t compressed_rank_t;

void compressed_rank_init(compressed_rank_t * cr);

void compressed_rank_destroy(compressed_rank_t * cr);
 
void compressed_rank_generate(compressed_rank_t * cr, cmph_uint32 * vals_table, cmph_uint32 n);

cmph_uint32 compressed_rank_query(compressed_rank_t * cr, cmph_uint32 idx);

cmph_uint32 compressed_rank_get_space_usage(compressed_rank_t * cr);

void compressed_rank_dump(compressed_rank_t * cr, char **buf, cmph_uint32 *buflen);

void compressed_rank_load(compressed_rank_t * cr, const char *buf, cmph_uint32 buflen);


/** \fn void compressed_rank_pack(compressed_rank_t *cr, void *cr_packed);
 *  \brief Support the ability to pack a compressed_rank structure into a preallocated contiguous memory space pointed by cr_packed.
 *  \param cr points to the compressed_rank structure
 *  \param cr_packed pointer to the contiguous memory area used to store the compressed_rank structure. The size of cr_packed must be at least @see compressed_rank_packed_size 
 */
void compressed_rank_pack(compressed_rank_t *cr, void *cr_packed);

/** \fn cmph_uint32 compressed_rank_packed_size(compressed_rank_t *cr);
 *  \brief Return the amount of space needed to pack a compressed_rank structure.
 *  \return the size of the packed compressed_rank structure or zero for failures
 */ 
cmph_uint32 compressed_rank_packed_size(compressed_rank_t *cr);


/** \fn cmph_uint32 compressed_rank_query_packed(void * cr_packed, cmph_uint32 idx);
 *  \param cr_packed is a pointer to a contiguous memory area
 *  \param idx is an index to compute the rank
 *  \return an integer that represents the compressed_rank value.
 */
cmph_uint32 compressed_rank_query_packed(void * cr_packed, cmph_uint32 idx);

#endif
