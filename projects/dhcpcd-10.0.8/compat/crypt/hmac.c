/*	$NetBSD: hmac.c,v 1.5 2017/10/05 09:59:04 roy Exp $	*/

/*-
 * Copyright (c) 2016 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <stdlib.h>

#include "config.h"

#if defined(HAVE_MD5_H) && !defined(DEPGEN)
#include <md5.h>
#endif

#ifdef SHA2_H
#  include SHA2_H
#endif

#ifndef __arraycount
#define	__arraycount(__x)       (sizeof(__x) / sizeof(__x[0]))
#endif

#if 0
#include <md2.h>
#include <md4.h>
#include <md5.h>
#include <rmd160.h>
#include <sha1.h>
#include <sha2.h>
#endif

#ifndef MD5_BLOCK_LENGTH
#define	MD5_BLOCK_LENGTH	64
#endif
#ifndef SHA256_BLOCK_LENGTH
#define	SHA256_BLOCK_LENGTH	64
#endif

#define HMAC_SIZE	128
#define HMAC_IPAD	0x36
#define HMAC_OPAD	0x5C

static const struct hmac {
	const char *name;
	size_t ctxsize;
	size_t digsize;
	size_t blocksize;
	void (*init)(void *);
	void (*update)(void *, const uint8_t *, unsigned int);
	void (*final)(uint8_t *, void *);
} hmacs[] = {
#if 0
	{
		"md2", sizeof(MD2_CTX), MD2_DIGEST_LENGTH, MD2_BLOCK_LENGTH,
		(void *)MD2Init, (void *)MD2Update, (void *)MD2Final,
	},
	{
		"md4", sizeof(MD4_CTX), MD4_DIGEST_LENGTH, MD4_BLOCK_LENGTH,
		(void *)MD4Init, (void *)MD4Update, (void *)MD4Final,
	},
#endif
	{
		"md5", sizeof(MD5_CTX), MD5_DIGEST_LENGTH, MD5_BLOCK_LENGTH,
		(void *)MD5Init, (void *)MD5Update, (void *)MD5Final,
	},
#if 0
	{
		"rmd160", sizeof(RMD160_CTX), RMD160_DIGEST_LENGTH,
		RMD160_BLOCK_LENGTH,
		(void *)RMD160Init, (void *)RMD160Update, (void *)RMD160Final,
	},
	{
		"sha1", sizeof(SHA1_CTX), SHA1_DIGEST_LENGTH, SHA1_BLOCK_LENGTH,
		(void *)SHA1Init, (void *)SHA1Update, (void *)SHA1Final,
	},
	{
		"sha224", sizeof(SHA224_CTX), SHA224_DIGEST_LENGTH,
		SHA224_BLOCK_LENGTH,
		(void *)SHA224_Init, (void *)SHA224_Update,
		(void *)SHA224_Final,
	},
#endif
	{
		"sha256", sizeof(SHA256_CTX), SHA256_DIGEST_LENGTH,
		SHA256_BLOCK_LENGTH,
		(void *)SHA256_Init, (void *)SHA256_Update,
		(void *)SHA256_Final,
	},
#if 0
	{
		"sha384", sizeof(SHA384_CTX), SHA384_DIGEST_LENGTH,
		SHA384_BLOCK_LENGTH,
		(void *)SHA384_Init, (void *)SHA384_Update,
		(void *)SHA384_Final,
	},
	{
		"sha512", sizeof(SHA512_CTX), SHA512_DIGEST_LENGTH,
		SHA512_BLOCK_LENGTH,
		(void *)SHA512_Init, (void *)SHA512_Update,
		(void *)SHA512_Final,
	},
#endif
};

static const struct hmac *
hmac_find(const char *name)
{
	for (size_t i = 0; i < __arraycount(hmacs); i++) {
		if (strcmp(hmacs[i].name, name) != 0)
			continue;
		return &hmacs[i];
	}
	return NULL;
}

ssize_t
hmac(const char *name,
    const void *key, size_t klen,
    const void *text, size_t tlen,
    void *digest, size_t dlen)
{
	uint8_t ipad[HMAC_SIZE], opad[HMAC_SIZE], d[HMAC_SIZE];
	const uint8_t *k = key;
	const struct hmac *h;
	uint64_t c[32];
	void *p;

	if ((h = hmac_find(name)) == NULL)
		return -1;


	if (klen > h->blocksize) {
		(*h->init)(c);
		(*h->update)(c, k, (unsigned int)klen);
		(*h->final)(d, c);
		k = (void *)d;
		klen = h->digsize;
	}

	/* Form input and output pads for the digests */
	for (size_t i = 0; i < sizeof(ipad); i++) {
		ipad[i] = (i < klen ? k[i] : 0) ^ HMAC_IPAD;
		opad[i] = (i < klen ? k[i] : 0) ^ HMAC_OPAD;
	}

	p = dlen >= h->digsize ? digest : d;
	if (p != digest) {
		memcpy(p, digest, dlen);
		memset((char *)p + dlen, 0, h->digsize - dlen);
	}
	(*h->init)(c);
	(*h->update)(c, ipad, (unsigned int)h->blocksize);
	(*h->update)(c, text, (unsigned int)tlen);
	(*h->final)(p, c);

	(*h->init)(c);
	(*h->update)(c, opad, (unsigned int)h->blocksize);
	(*h->update)(c, digest, (unsigned int)h->digsize);
	(*h->final)(p, c);

	if (p != digest)
		memcpy(digest, p, dlen);

	return (ssize_t)h->digsize;
}
