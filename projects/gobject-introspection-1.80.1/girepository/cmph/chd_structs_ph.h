#ifndef __CMPH_CHD_PH_STRUCTS_H__
#define __CMPH_CHD_PH_STRUCTS_H__

#include "hash_state.h"
#include "compressed_seq.h"

struct __chd_ph_data_t
{
	compressed_seq_t * cs;	// compressed displacement values
	cmph_uint32 nbuckets;	// number of buckets
	cmph_uint32 n;		// number of bins
	hash_state_t *hl;	// linear hash function
};

struct __chd_ph_config_data_t
{
	CMPH_HASH hashfunc;	// linear hash function to be used
	compressed_seq_t * cs;	// compressed displacement values
	cmph_uint32 nbuckets;	// number of buckets
	cmph_uint32 n;		// number of bins
	hash_state_t *hl;	// linear hash function
	
	cmph_uint32 m;		// number of keys
	cmph_uint8 use_h;	// flag to indicate the of use of a heuristic (use_h = 1)
	cmph_uint32 keys_per_bin;//maximum number of keys per bin 
	cmph_uint32 keys_per_bucket; // average number of keys per bucket
	cmph_uint8 *occup_table;     // table that indicates occupied positions	
};
#endif
