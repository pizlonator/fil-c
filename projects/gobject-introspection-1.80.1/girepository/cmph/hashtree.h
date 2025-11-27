#ifndef __CMPH_HASHTREE_H__
#define __CMPH_HASHTREE_H__

#include "cmph.h"

typedef struct __hashtree_data_t hashtree_data_t;
typedef struct __hashtree_config_data_t hashtree_config_data_t;

hashtree_config_data_t *hashtree_config_new();
void hashtree_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs);
void hashtree_config_set_leaf_algo(cmph_config_t *mph, CMPH_ALGO leaf_algo);
void hashtree_config_destroy(cmph_config_t *mph);
cmph_t *hashtree_new(cmph_config_t *mph, double c);

void hashtree_load(FILE *f, cmph_t *mphf);
int hashtree_dump(cmph_t *mphf, FILE *f);
void hashtree_destroy(cmph_t *mphf);
cmph_uint32 hashtree_search(cmph_t *mphf, const char *key, cmph_uint32 keylen);
#endif
