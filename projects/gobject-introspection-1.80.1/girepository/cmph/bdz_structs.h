#ifndef __CMPH_BDZ_STRUCTS_H__
#define __CMPH_BDZ_STRUCTS_H__

#include "hash_state.h"

struct __bdz_data_t
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 r; //partition vertex count
	cmph_uint8 *g;
	hash_state_t *hl; // linear hashing

	cmph_uint32 k; //kth index in ranktable, $k = log_2(n=3r)/\varepsilon$
	cmph_uint8 b; // number of bits of k
	cmph_uint32 ranktablesize; //number of entries in ranktable, $n/k +1$
	cmph_uint32 *ranktable; // rank table
};


struct __bdz_config_data_t
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 r; //partition vertex count
	cmph_uint8 *g;
	hash_state_t *hl; // linear hashing

	cmph_uint32 k; //kth index in ranktable, $k = log_2(n=3r)/\varepsilon$
	cmph_uint8 b; // number of bits of k
	cmph_uint32 ranktablesize; //number of entries in ranktable, $n/k +1$
	cmph_uint32 *ranktable; // rank table
	CMPH_HASH hashfunc;
};

#endif
