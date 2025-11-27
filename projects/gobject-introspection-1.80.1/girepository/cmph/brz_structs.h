#ifndef __CMPH_BRZ_STRUCTS_H__
#define __CMPH_BRZ_STRUCTS_H__

#include "hash_state.h"

struct __brz_data_t
{
	CMPH_ALGO algo;      // CMPH algo for generating the MPHFs for the buckets (Just CMPH_FCH and CMPH_BMZ8)
	cmph_uint32 m;       // edges (words) count
	double c;      // constant c
	cmph_uint8  *size;   // size[i] stores the number of edges represented by g[i][...]. 
	cmph_uint32 *offset; // offset[i] stores the sum: size[0] + size[1] + ... size[i-1].
	cmph_uint8 **g;      // g function. 
	cmph_uint32 k;       // number of components
	hash_state_t **h1;
	hash_state_t **h2;
	hash_state_t * h0;
};

struct __brz_config_data_t
{
	CMPH_HASH hashfuncs[3];
	CMPH_ALGO algo;      // CMPH algo for generating the MPHFs for the buckets (Just CMPH_FCH and CMPH_BMZ8)
	double c;      // constant c
	cmph_uint32 m;       // edges (words) count
	cmph_uint8  *size;   // size[i] stores the number of edges represented by g[i][...]. 
	cmph_uint32 *offset; // offset[i] stores the sum: size[0] + size[1] + ... size[i-1].
	cmph_uint8 **g;      // g function. 
	cmph_uint8  b;       // parameter b. 
	cmph_uint32 k;       // number of components
	hash_state_t **h1;
	hash_state_t **h2;
	hash_state_t * h0;    
	cmph_uint32 memory_availability; 
	cmph_uint8 * tmp_dir; // temporary directory 
	FILE * mphf_fd; // mphf file
};

#endif
