#ifndef _CMPH_CHD_H__
#define _CMPH_CHD_H__

#include "cmph.h"

typedef struct __chd_data_t chd_data_t;
typedef struct __chd_config_data_t chd_config_data_t;

/* Config API */
chd_config_data_t *chd_config_new(cmph_config_t * mph);
void chd_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs);

/** \fn void chd_config_set_keys_per_bin(cmph_config_t *mph, cmph_uint32 keys_per_bin);
 *  \brief Allows to set the number of keys per bin.
 *  \param mph pointer to the configuration structure
 *  \param keys_per_bin value for the number of keys per bin 
 */
void chd_config_set_keys_per_bin(cmph_config_t *mph, cmph_uint32 keys_per_bin);

/** \fn void chd_config_set_b(cmph_config_t *mph, cmph_uint32 keys_per_bucket);
 *  \brief Allows to set the number of keys per bucket.
 *  \param mph pointer to the configuration structure
 *  \param keys_per_bucket value for the number of keys per bucket 
 */
void chd_config_set_b(cmph_config_t *mph, cmph_uint32 keys_per_bucket);
void chd_config_destroy(cmph_config_t *mph);


/* Chd algorithm API */
cmph_t *chd_new(cmph_config_t *mph, double c);
void chd_load(FILE *fd, cmph_t *mphf);
int chd_dump(cmph_t *mphf, FILE *fd);
void chd_destroy(cmph_t *mphf);
cmph_uint32 chd_search(cmph_t *mphf, const char *key, cmph_uint32 keylen);

/** \fn void chd_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size() 
 */
void chd_pack(cmph_t *mphf, void *packed_mphf);

/** \fn cmph_uint32 chd_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */ 
cmph_uint32 chd_packed_size(cmph_t *mphf);

/** cmph_uint32 chd_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search. 
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 chd_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen);

#endif
