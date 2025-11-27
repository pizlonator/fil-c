#ifndef __DJB2_HASH_H__
#define __DJB2_HASH_H__

#include "hash.h"

typedef struct __djb2_state_t
{
	CMPH_HASH hashfunc;
} djb2_state_t;

djb2_state_t *djb2_state_new();
cmph_uint32 djb2_hash(djb2_state_t *state, const char *k, cmph_uint32 keylen);
void djb2_state_dump(djb2_state_t *state, char **buf, cmph_uint32 *buflen);
djb2_state_t *djb2_state_copy(djb2_state_t *src_state);
djb2_state_t *djb2_state_load(const char *buf, cmph_uint32 buflen);
void djb2_state_destroy(djb2_state_t *state);

#endif
