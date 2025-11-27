#ifndef __CMPH_BDZ_STRUCTS_PH_H__
#define __CMPH_BDZ_STRUCTS_PH_H__

#include "hash_state.h"

struct __bdz_ph_data_t
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 r; //partition vertex count
	cmph_uint8 *g;
	hash_state_t *hl; // linear hashing
};


struct __bdz_ph_config_data_t
{
	CMPH_HASH hashfunc;
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 r; //partition vertex count
	cmph_uint8 *g;
	hash_state_t *hl; // linear hashing
};

#endif
