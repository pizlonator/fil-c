#ifndef __CMPH_BRZ_H__
#define __CMPH_BRZ_H__

#include "cmph.h"

typedef struct __brz_data_t brz_data_t;
typedef struct __brz_config_data_t brz_config_data_t;

brz_config_data_t *brz_config_new(void);
void brz_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs);
void brz_config_set_tmp_dir(cmph_config_t *mph, cmph_uint8 *tmp_dir);
void brz_config_set_mphf_fd(cmph_config_t *mph, FILE *mphf_fd);
void brz_config_set_b(cmph_config_t *mph, cmph_uint32 b);
void brz_config_set_algo(cmph_config_t *mph, CMPH_ALGO algo);
void brz_config_set_memory_availability(cmph_config_t *mph, cmph_uint32 memory_availability);
void brz_config_destroy(cmph_config_t *mph);
cmph_t *brz_new(cmph_config_t *mph, double c);

void brz_load(FILE *f, cmph_t *mphf);
int brz_dump(cmph_t *mphf, FILE *f);
void brz_destroy(cmph_t *mphf);
cmph_uint32 brz_search(cmph_t *mphf, const char *key, cmph_uint32 keylen);

/** \fn void brz_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size() 
 */
void brz_pack(cmph_t *mphf, void *packed_mphf);

/** \fn cmph_uint32 brz_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */ 
cmph_uint32 brz_packed_size(cmph_t *mphf);

/** cmph_uint32 brz_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search. 
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 brz_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen);

#endif
