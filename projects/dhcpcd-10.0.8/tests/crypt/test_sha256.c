/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2023 Tobias Heider <tobias.heider@canonical.com>
 * Copyright (c) 2006-2018 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "test.h"

#ifdef SHA2_H
#include SHA2_H
#endif

# ifndef SHA256_DIGEST_LENGTH
#  define SHA256_DIGEST_LENGTH          32
# endif

static void
print_md(FILE *stream, const uint8_t *md)
{
	int i;

	fprintf(stream, "digest = 0x");
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
		fprintf(stream, "%02x", *md++);
	fprintf(stream, "\n");
}

static void
test_md(const uint8_t *md, const uint8_t *tst)
{
	print_md(stdout, md);
	if (memcmp(md, tst, SHA256_DIGEST_LENGTH) == 0)
		return;
	fprintf(stderr, "FAILED!\nExpected\t\t\t");
	print_md(stderr, tst);
	exit(EXIT_FAILURE);
}

static void
sha256_test1(void)
{
	const uint8_t text[] = "Hi There";
	const uint8_t expect[SHA256_DIGEST_LENGTH] = {
		0xcc, 0x6d, 0x58, 0x96, 0xd7, 0x70, 0x10, 0x1e,
		0xf0, 0x28, 0x0c, 0x94, 0x3a, 0x2d, 0x3c, 0x3f,
		0x24, 0xcd ,0x5b, 0x11, 0x46, 0x4a, 0x51, 0x86,
		0xda, 0xf7, 0xa2, 0x38, 0x47, 0x71, 0x62, 0xac
	};
	uint8_t digest[SHA256_DIGEST_LENGTH];
	SHA256_CTX ctx;

	printf ("SHA256 Test 1:\t\t");
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, text, 8);
	SHA256_Final(digest, &ctx);
	test_md(digest, expect);
}

static void
sha256_test2(void)
{
	const uint8_t text[] = "what do ya want for nothing?";
	const uint8_t expect[SHA256_DIGEST_LENGTH] = {
		0xb3, 0x81, 0xe7, 0xfe, 0xc6, 0x53, 0xfc, 0x3a,
		0xb9, 0xb1, 0x78, 0x27, 0x23, 0x66, 0xb8, 0xac,
		0x87, 0xfe, 0xd8, 0xd3, 0x1c, 0xb2, 0x5e, 0xd1,
		0xd0, 0xe1, 0xf3, 0x31, 0x86, 0x44, 0xc8, 0x9c,
	};
	uint8_t digest[SHA256_DIGEST_LENGTH];
	SHA256_CTX ctx;

	printf ("SHA256 Test 2:\t\t");
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, text, 28);
	SHA256_Final(digest, &ctx);
	test_md(digest, expect);
}

static void
sha256_test3(void)
{
	const uint8_t expect[SHA256_DIGEST_LENGTH] = {
		0x5c, 0xf6, 0x18, 0xb5, 0xb6, 0xd3, 0x8b, 0xd1,
		0x6c, 0x2e, 0x55, 0x8e, 0xef, 0x4d, 0x4b, 0x6d,
		0x52, 0x82, 0x84, 0x54, 0x7f, 0xd4, 0xa0, 0x9d,
		0xa2, 0xab, 0xb6, 0xf0, 0x98, 0xec, 0x61, 0x93,
	};
	uint8_t digest[SHA256_DIGEST_LENGTH];
	uint8_t text[50];
	int i;
	SHA256_CTX ctx;

	printf ("SHA256 Test 3:\t\t");
	for (i = 0; i < 50; i++)
		text[i] = 0xdd;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, text, 50);
	SHA256_Final(digest, &ctx);
	test_md(digest, expect);
}

int test_sha256(void)
{
	printf ("Starting SHA256 tests...\n\n");
	sha256_test1();
	sha256_test2();
	sha256_test3();
	printf("\nAll tests pass.\n");
	return 0;
}
