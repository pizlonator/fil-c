#ifndef __CMPH_BMZ_STRUCTS_H__
#define __CMPH_BMZ_STRUCTS_H__

#include "hash_state.h"

struct __bmz_data_t
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 *g;
	hash_state_t **hashes;
};


struct __bmz_config_data_t
{
	CMPH_HASH hashfuncs[2];
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	graph_t *graph;
	cmph_uint32 *g;
	hash_state_t **hashes;
};

#endif
