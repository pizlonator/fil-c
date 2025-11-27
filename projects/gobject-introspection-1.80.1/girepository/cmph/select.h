#ifndef __CMPH_SELECT_H__
#define __CMPH_SELECT_H__

#include "cmph_types.h"

struct _select_t
{
	cmph_uint32 n,m;
	cmph_uint32 * bits_vec;
	cmph_uint32 * select_table;
};

typedef struct _select_t select_t;

void select_init(select_t * sel);

void select_destroy(select_t * sel);
 
void select_generate(select_t * sel, cmph_uint32 * keys_vec, cmph_uint32 n, cmph_uint32 m);

cmph_uint32 select_query(select_t * sel, cmph_uint32 one_idx);

cmph_uint32 select_next_query(select_t * sel, cmph_uint32 vec_bit_idx);

cmph_uint32 select_get_space_usage(select_t * sel);

void select_dump(select_t *sel, char **buf, cmph_uint32 *buflen);

void select_load(select_t * sel, const char *buf, cmph_uint32 buflen);


/** \fn void select_pack(select_t *sel, void *sel_packed);
 *  \brief Support the ability to pack a select structure into a preallocated contiguous memory space pointed by sel_packed.
 *  \param sel points to the select structure
 *  \param sel_packed pointer to the contiguous memory area used to store the select structure. The size of sel_packed must be at least @see select_packed_size 
 */
void select_pack(select_t *sel, void *sel_packed);

/** \fn cmph_uint32 select_packed_size(select_t *sel);
 *  \brief Return the amount of space needed to pack a select structure.
 *  \return the size of the packed select structure or zero for failures
 */ 
cmph_uint32 select_packed_size(select_t *sel);


/** \fn cmph_uint32 select_query_packed(void * sel_packed, cmph_uint32 one_idx);
 *  \param sel_packed is a pointer to a contiguous memory area
 *  \param one_idx is the rank for which we want to calculate the inverse function select
 *  \return an integer that represents the select value of rank idx.
 */
cmph_uint32 select_query_packed(void * sel_packed, cmph_uint32 one_idx);


/** \fn cmph_uint32 select_next_query_packed(void * sel_packed, cmph_uint32 vec_bit_idx);
 *  \param sel_packed is a pointer to a contiguous memory area
 *  \param vec_bit_idx is a value prior computed by @see select_query_packed
 *  \return an integer that represents the next select value greater than @see vec_bit_idx.
 */
cmph_uint32 select_next_query_packed(void * sel_packed, cmph_uint32 vec_bit_idx);

#endif
