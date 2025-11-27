#ifndef __CMPH_CHD_STRUCTS_H__
#define __CMPH_CHD_STRUCTS_H__

#include "chd_structs_ph.h"
#include "chd_ph.h"
#include "compressed_rank.h"

struct __chd_data_t
{
	cmph_uint32 packed_cr_size;
	cmph_uint8 * packed_cr; // packed compressed rank structure to control the number of zeros in a bit vector
	
	cmph_uint32 packed_chd_phf_size;
	cmph_uint8 * packed_chd_phf;
};

struct __chd_config_data_t
{	
	cmph_config_t *chd_ph;     // chd_ph algorithm must be used here
};
#endif
