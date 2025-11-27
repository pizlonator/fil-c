#ifndef __CMPH_HASH_H__
#define __CMPH_HASH_H__

#include "cmph_types.h"

typedef union __hash_state_t hash_state_t;

hash_state_t *hash_state_new(CMPH_HASH, cmph_uint32 hashsize);

/** \fn cmph_uint32 hash(hash_state_t *state, const char *key, cmph_uint32 keylen);
 *  \param state is a pointer to a hash_state_t structure
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \return an integer that represents a hash value of 32 bits.
 */
cmph_uint32 hash(hash_state_t *state, const char *key, cmph_uint32 keylen);

/** \fn void hash_vector(hash_state_t *state, const char *key, cmph_uint32 keylen, cmph_uint32 * hashes);
 *  \param state is a pointer to a hash_state_t structure
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \param hashes is a pointer to a memory large enough to fit three 32-bit integers.
 */
void hash_vector(hash_state_t *state, const char *key, cmph_uint32 keylen, cmph_uint32 * hashes);

void hash_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen);

hash_state_t * hash_state_copy(hash_state_t *src_state);

hash_state_t *hash_state_load(const char *buf, cmph_uint32 buflen);

void hash_state_destroy(hash_state_t *state);

/** \fn void hash_state_pack(hash_state_t *state, void *hash_packed);
 *  \brief Support the ability to pack a hash function into a preallocated contiguous memory space pointed by hash_packed.
 *  \param state points to the hash function
 *  \param hash_packed pointer to the contiguous memory area used to store the hash function. The size of hash_packed must be at least hash_state_packed_size()
 *  
 * Support the ability to pack a hash function into a preallocated contiguous memory space pointed by hash_packed.
 * However, the hash function type must be packed outside.
 */
void hash_state_pack(hash_state_t *state, void *hash_packed);

/** \fn cmph_uint32 hash_packed(void *hash_packed, CMPH_HASH hashfunc, const char *k, cmph_uint32 keylen);
 *  \param hash_packed is a pointer to a contiguous memory area
 *  \param hashfunc is the type of the hash function packed in hash_packed
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \return an integer that represents a hash value of 32 bits.
 */
cmph_uint32 hash_packed(void *hash_packed, CMPH_HASH hashfunc, const char *k, cmph_uint32 keylen);

/** \fn cmph_uint32 hash_state_packed_size(CMPH_HASH hashfunc)
 *  \brief Return the amount of space needed to pack a hash function.
 *  \param hashfunc function type
 *  \return the size of the packed function or zero for failures
 */ 
cmph_uint32 hash_state_packed_size(CMPH_HASH hashfunc);


/** \fn hash_vector_packed(void *hash_packed, CMPH_HASH hashfunc, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
 *  \param hash_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \param hashes is a pointer to a memory large enough to fit three 32-bit integers.
 */
void hash_vector_packed(void *hash_packed, CMPH_HASH hashfunc, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);


/** \fn CMPH_HASH hash_get_type(hash_state_t *state);
 *  \param state is a pointer to a hash_state_t structure
 *  \return the hash function type pointed by state
 */
CMPH_HASH hash_get_type(hash_state_t *state);

#endif
