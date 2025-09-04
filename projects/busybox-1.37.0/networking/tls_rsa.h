/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 *
 * Selected few declarations for RSA.
 */

typedef struct {
	pstm_int    e, d, N, qP, dP, dQ, p, q;
	uint32      size;   /* Size of the key in bytes */
	int32       optimized; /* 1 for optimized */
//bbox	psPool_t *pool;
} psRsaKey_t;

static ALWAYS_INLINE void psRsaKey_clear(psRsaKey_t *key)
{
	pstm_clear(&key->N);
	pstm_clear(&key->e);
	pstm_clear(&key->d);
	pstm_clear(&key->p);
	pstm_clear(&key->q);
	pstm_clear(&key->dP);
	pstm_clear(&key->dQ);
	pstm_clear(&key->qP);
}

#define psRsaEncryptPub(pool, key, in, inlen, out, outlen, data) \
        psRsaEncryptPub(      key, in, inlen, out, outlen)
int32 psRsaEncryptPub(psPool_t *pool, psRsaKey_t *key,
                                                unsigned char *in, uint32 inlen,
                                                unsigned char *out, uint32 outlen, void *data) FAST_FUNC;
