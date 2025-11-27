#include "vqueue.h"
#include "fch_buckets.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
//#define DEBUG
#include "debug.h"

typedef struct __fch_bucket_entry_t
{
  char * value;
  cmph_uint32 length;
} fch_bucket_entry_t;

typedef struct __fch_bucket_t
{
  fch_bucket_entry_t * entries;
  cmph_uint32 capacity, size;
} fch_bucket_t;



static void fch_bucket_new(fch_bucket_t *bucket) 
{
	assert(bucket);
	bucket->size = 0;
	bucket->entries = NULL;
	bucket->capacity = 0;
}

static void fch_bucket_destroy(fch_bucket_t *bucket)
{
	cmph_uint32 i;
	assert(bucket);
	for (i = 0; i < bucket->size; i++)
	{
		free((bucket->entries + i)->value);
	}
	free(bucket->entries);
}


static void fch_bucket_reserve(fch_bucket_t *bucket, cmph_uint32 size)
{
	assert(bucket);
	if (bucket->capacity < size)
	{
		cmph_uint32 new_capacity = bucket->capacity + 1;
		DEBUGP("Increasing current capacity %u to %u\n", bucket->capacity, size);
		while (new_capacity < size)
		{
			new_capacity *= 2;
		}
		bucket->entries = (fch_bucket_entry_t *)realloc(bucket->entries, sizeof(fch_bucket_entry_t)*new_capacity);
		assert(bucket->entries);
		bucket->capacity = new_capacity;
		DEBUGP("Increased\n");
	}
}

static void fch_bucket_insert(fch_bucket_t *bucket, char *val, cmph_uint32 val_length)
{
	assert(bucket);
	fch_bucket_reserve(bucket, bucket->size + 1);
	(bucket->entries + bucket->size)->value = val;
	(bucket->entries + bucket->size)->length = val_length;
	++(bucket->size);
}


static cmph_uint8 fch_bucket_is_empty(fch_bucket_t *bucket)
{
	assert(bucket);
	return (cmph_uint8)(bucket->size == 0);
}

static cmph_uint32 fch_bucket_size(fch_bucket_t *bucket)
{
	assert(bucket);
	return bucket->size;
}

static char * fch_bucket_get_key(fch_bucket_t *bucket, cmph_uint32 index_key)
{
	assert(bucket); assert(index_key < bucket->size);
	return (bucket->entries + index_key)->value;
}

static cmph_uint32 fch_bucket_get_length(fch_bucket_t *bucket, cmph_uint32 index_key)
{
	assert(bucket); assert(index_key < bucket->size);
	return (bucket->entries + index_key)->length;
}

static void fch_bucket_print(fch_bucket_t * bucket, cmph_uint32 index)
{
	cmph_uint32 i;
	assert(bucket);
	fprintf(stderr, "Printing bucket %u ...\n", index);
	for (i = 0; i < bucket->size; i++)
	{
		fprintf(stderr, "  key: %s\n", (bucket->entries + i)->value);
	}
}

//////////////////////////////////////////////////////////////////////////////////////

struct __fch_buckets_t
{
  fch_bucket_t * values;
  cmph_uint32 nbuckets, max_size;
  
};

fch_buckets_t * fch_buckets_new(cmph_uint32 nbuckets)
{
	cmph_uint32 i;
	fch_buckets_t *buckets = (fch_buckets_t *)malloc(sizeof(fch_buckets_t));
	assert(buckets);
	buckets->values = (fch_bucket_t *)calloc((size_t)nbuckets, sizeof(fch_bucket_t));
	for (i = 0; i < nbuckets; i++) fch_bucket_new(buckets->values + i); 
	assert(buckets->values);
	buckets->nbuckets = nbuckets;
	buckets->max_size = 0;
	return buckets;
}

cmph_uint8 fch_buckets_is_empty(fch_buckets_t * buckets, cmph_uint32 index)
{
	assert(index < buckets->nbuckets);
	return fch_bucket_is_empty(buckets->values + index);
}

void fch_buckets_insert(fch_buckets_t * buckets, cmph_uint32 index, char * key, cmph_uint32 length)
{
	assert(index < buckets->nbuckets);
	fch_bucket_insert(buckets->values + index, key, length);
	if (fch_bucket_size(buckets->values + index) > buckets->max_size) 
	{
		buckets->max_size = fch_bucket_size(buckets->values + index);
	}
}

cmph_uint32 fch_buckets_get_size(fch_buckets_t * buckets, cmph_uint32 index)
{
	assert(index < buckets->nbuckets);
	return fch_bucket_size(buckets->values + index);
}


char * fch_buckets_get_key(fch_buckets_t * buckets, cmph_uint32 index, cmph_uint32 index_key)
{
	assert(index < buckets->nbuckets);
	return fch_bucket_get_key(buckets->values + index, index_key);
}

cmph_uint32 fch_buckets_get_keylength(fch_buckets_t * buckets, cmph_uint32 index, cmph_uint32 index_key)
{
	assert(index < buckets->nbuckets);
	return fch_bucket_get_length(buckets->values + index, index_key);
}

cmph_uint32 fch_buckets_get_max_size(fch_buckets_t * buckets)
{
	return buckets->max_size;
}

cmph_uint32 fch_buckets_get_nbuckets(fch_buckets_t * buckets)
{
	return buckets->nbuckets;
}

cmph_uint32 * fch_buckets_get_indexes_sorted_by_size(fch_buckets_t * buckets) 
{
	cmph_uint32 i = 0;
	cmph_uint32 sum = 0, value;
	cmph_uint32 *nbuckets_size = (cmph_uint32 *) calloc((size_t)buckets->max_size + 1, sizeof(cmph_uint32));
	cmph_uint32 * sorted_indexes = (cmph_uint32 *) calloc((size_t)buckets->nbuckets, sizeof(cmph_uint32));
	
	// collect how many buckets for each size.
	for(i = 0; i < buckets->nbuckets; i++) nbuckets_size[fch_bucket_size(buckets->values + i)] ++;
	
	// calculating offset considering a decreasing order of buckets size.
	value = nbuckets_size[buckets->max_size];
	nbuckets_size[buckets->max_size] = sum;
	for(i = (int)buckets->max_size - 1; i >= 0; i--)
	{
		sum += value;
		value = nbuckets_size[i];
		nbuckets_size[i] = sum;
		
	}
	for(i = 0; i < buckets->nbuckets; i++) 
	{
		sorted_indexes[nbuckets_size[fch_bucket_size(buckets->values + i)]] = (cmph_uint32)i;
		nbuckets_size[fch_bucket_size(buckets->values + i)] ++;
	}	
	free(nbuckets_size);
	return sorted_indexes;
}

void fch_buckets_print(fch_buckets_t * buckets)
{
	cmph_uint32 i;
	for (i = 0; i < buckets->nbuckets; i++) fch_bucket_print(buckets->values + i, i);
}

void fch_buckets_destroy(fch_buckets_t * buckets)
{
	cmph_uint32 i;
	for (i = 0; i < buckets->nbuckets; i++) fch_bucket_destroy(buckets->values + i); 
	free(buckets->values);
	free(buckets);
}
