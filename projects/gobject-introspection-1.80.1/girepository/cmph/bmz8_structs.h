#ifndef __CMPH_BMZ8_STRUCTS_H__
#define __CMPH_BMZ8_STRUCTS_H__

#include "hash_state.h"

struct __bmz8_data_t
{
	cmph_uint8 m; //edges (words) count
	cmph_uint8 n; //vertex count
	cmph_uint8 *g;
	hash_state_t **hashes;
};


struct __bmz8_config_data_t
{
	CMPH_HASH hashfuncs[2];
	cmph_uint8 m; //edges (words) count
	cmph_uint8 n; //vertex count
	graph_t *graph;
	cmph_uint8 *g;
	hash_state_t **hashes;
};

#endif
