#ifndef __CMPH_HASHTREE_STRUCTS_H__
#define __CMPH_HASHTREE_STRUCTS_H__

#include "hash_state.h"

struct __hashtree_data_t
{
	cmph_uint32 m; //edges (words) count
	double c; //constant c
	cmph_uint8 *size; //size[i] stores the number of edges represented by g[i]
	cmph_uint32 **g;
	cmph_uint32 k; //number of components
	hash_state_t **h1; 
	hash_state_t **h2;
	hash_state_t *h3;
};

struct __hashtree_config_data_t
{
	CMPH_ALGO leaf_algo;
	CMPH_HASH hashfuncs[3];
	cmph_uint32 m; //edges (words) count
	cmph_uint8 *size; //size[i] stores the number of edges represented by g[i]
	cmph_uint32 *offset; //offset[i] stores the sum size[0] + ... size[i - 1]
	cmph_uint32 k; //number of components
	cmph_uint32 memory;
	hash_state_t **h1; 
	hash_state_t **h2;
	hash_state_t *h3;
};

#endif
