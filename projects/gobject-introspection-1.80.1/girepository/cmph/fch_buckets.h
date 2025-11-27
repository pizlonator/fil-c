#ifndef __CMPH_FCH_BUCKETS_H__
#define __CMPH_FCH_BUCKETS_H__

#include "cmph_types.h"
typedef struct __fch_buckets_t fch_buckets_t;

fch_buckets_t * fch_buckets_new(cmph_uint32 nbuckets);

cmph_uint8 fch_buckets_is_empty(fch_buckets_t * buckets, cmph_uint32 index);

void fch_buckets_insert(fch_buckets_t * buckets, cmph_uint32 index, char * key, cmph_uint32 length);

cmph_uint32 fch_buckets_get_size(fch_buckets_t * buckets, cmph_uint32 index);

char * fch_buckets_get_key(fch_buckets_t * buckets, cmph_uint32 index, cmph_uint32 index_key);

cmph_uint32 fch_buckets_get_keylength(fch_buckets_t * buckets, cmph_uint32 index, cmph_uint32 index_key);

// returns the size of biggest bucket.
cmph_uint32 fch_buckets_get_max_size(fch_buckets_t * buckets);

// returns the number of buckets.
cmph_uint32 fch_buckets_get_nbuckets(fch_buckets_t * buckets);

cmph_uint32 * fch_buckets_get_indexes_sorted_by_size(fch_buckets_t * buckets);

void fch_buckets_print(fch_buckets_t * buckets);

void fch_buckets_destroy(fch_buckets_t * buckets);
#endif
