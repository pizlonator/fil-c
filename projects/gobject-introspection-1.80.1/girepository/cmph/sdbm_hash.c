#include "sdbm_hash.h"
#include <stdlib.h>

sdbm_state_t *sdbm_state_new()
{
	sdbm_state_t *state = (sdbm_state_t *)malloc(sizeof(sdbm_state_t));
	state->hashfunc = CMPH_HASH_SDBM;
	return state;
}

void sdbm_state_destroy(sdbm_state_t *state)
{
	free(state);
}

cmph_uint32 sdbm_hash(sdbm_state_t *state, const char *k, cmph_uint32 keylen)
{
	register cmph_uint32 hash = 0;
	const unsigned char *ptr = (unsigned char *)k;
	cmph_uint32 i = 0;

	while(i < keylen) {
		hash = *ptr + (hash << 6) + (hash << 16) - hash;
		++ptr, ++i;
	}
	return hash;
}


void sdbm_state_dump(sdbm_state_t *state, char **buf, cmph_uint32 *buflen)
{
	*buf = NULL;
	*buflen = 0;
	return;
}

sdbm_state_t *sdbm_state_copy(sdbm_state_t *src_state)
{
	sdbm_state_t *dest_state = (sdbm_state_t *)malloc(sizeof(sdbm_state_t));
	dest_state->hashfunc = src_state->hashfunc;
	return dest_state;
}

sdbm_state_t *sdbm_state_load(const char *buf, cmph_uint32 buflen)
{
	sdbm_state_t *state = (sdbm_state_t *)malloc(sizeof(sdbm_state_t));
	state->hashfunc = CMPH_HASH_SDBM;
	return state;
}
