#include "fnv_hash.h"
#include <stdlib.h>

fnv_state_t *fnv_state_new()
{
	fnv_state_t *state = (fnv_state_t *)malloc(sizeof(fnv_state_t));
	state->hashfunc = CMPH_HASH_FNV;
	return state;
}

void fnv_state_destroy(fnv_state_t *state)
{
	free(state);
}

cmph_uint32 fnv_hash(fnv_state_t *state, const char *k, cmph_uint32 keylen)
{
	const unsigned char *bp = (const unsigned char *)k;	
	const unsigned char *be = bp + keylen;	
	static unsigned int hval = 0;	

	while (bp < be) 
	{
		
		//hval *= 0x01000193; good for non-gcc compiler
		hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24); //good for gcc

		hval ^= *bp++;
	}
	return hval;
}


void fnv_state_dump(fnv_state_t *state, char **buf, cmph_uint32 *buflen)
{
	*buf = NULL;
	*buflen = 0;
	return;
}

fnv_state_t * fnv_state_copy(fnv_state_t *src_state)
{
	fnv_state_t *dest_state = (fnv_state_t *)malloc(sizeof(fnv_state_t));
	dest_state->hashfunc = src_state->hashfunc;
	return dest_state;
}

fnv_state_t *fnv_state_load(const char *buf, cmph_uint32 buflen)
{
	fnv_state_t *state = (fnv_state_t *)malloc(sizeof(fnv_state_t));
	state->hashfunc = CMPH_HASH_FNV;
	return state;
}
