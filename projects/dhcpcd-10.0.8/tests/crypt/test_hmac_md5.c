/*
 * dhcpcd - DHCP client daemon
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

#ifdef HAVE_HMAC_H
#include <hmac.h>
#endif

static void
print_hmac(FILE *stream, const uint8_t *hmac)
{
	int i;

	fprintf(stream, "digest = 0x");
	for (i = 0; i < 16; i++)
		fprintf(stream, "%02x", *hmac++);
	fprintf(stream, "\n");
}

static void
test_hmac(const uint8_t *hmac, const uint8_t *tst)
{
	print_hmac(stdout, hmac);
	if (memcmp(hmac, tst, 16) == 0)
		return;
	fprintf(stderr, "FAILED!\nExpected\t\t\t");
	print_hmac(stderr, tst);
	exit(EXIT_FAILURE);
}

static void
hmac_md5_test1(void)
{
	const uint8_t text[] = "Hi There";
	uint8_t key[16];
	const uint8_t expect[16] = {
	    0x92, 0x94, 0x72, 0x7a, 0x36, 0x38, 0xbb, 0x1c,
	    0x13, 0xf4, 0x8e, 0xf8, 0x15, 0x8b, 0xfc, 0x9d,
	};
	uint8_t digest[16];
	int i;

	printf ("HMAC MD5 Test 1:\t\t");
	for (i = 0; i < 16; i++)
		key[i] = 0x0b;
	hmac("md5", key, 16, text, 8, digest, sizeof(digest));
	test_hmac(digest, expect);
}

static void
hmac_md5_test2(void)
{
	const uint8_t text[] = "what do ya want for nothing?";
	const uint8_t key[] = "Jefe";
	const uint8_t expect[16] = {
	    0x75, 0x0c, 0x78, 0x3e, 0x6a, 0xb0, 0xb5, 0x03,
	    0xea, 0xa8, 0x6e, 0x31, 0x0a, 0x5d, 0xb7, 0x38,
	};
	uint8_t digest[16];

	printf("HMAC MD5 Test 2:\t\t");
	hmac("md5", key, 4, text, 28, digest, sizeof(digest));
	test_hmac(digest, expect);
}

static void
hmac_md5_test3(void)
{
	const uint8_t expect[16] = {
	    0x56, 0xbe, 0x34, 0x52, 0x1d, 0x14, 0x4c, 0x88,
	    0xdb, 0xb8, 0xc7, 0x33, 0xf0, 0xe8, 0xb3, 0xf6,
	};
	uint8_t digest[16];
	uint8_t text[50];
	uint8_t key[16];
	int i;

	printf ("HMAC MD5 Test 3:\t\t");
	for (i = 0; i < 50; i++)
		text[i] = 0xdd;
	for (i = 0; i < 16; i++)
		key[i] = 0xaa;
	hmac("md5", key, 16, text, 50, digest, sizeof(digest));
	test_hmac(digest, expect);
}

static void
hmac_md5_test4(void)
{
	const uint8_t expect[16] = {
	    0x69, 0x7e, 0xaf, 0x0a, 0xca, 0x3a, 0x3a, 0xea,
	    0x3a, 0x75, 0x16, 0x47, 0x46, 0xff, 0xaa, 0x79,
	};
	uint8_t digest[16];
	uint8_t text[50];
	uint8_t key[25];
	uint8_t i;

	printf ("HMAC MD5 Test 4:\t\t");
	for (i = 0; i < 50; i++)
		text[i] = 0xcd;
	for (i = 0; i < 25; i++)
		key[i] = (uint8_t)(i + 1);
	hmac("md5", key, 25, text, 50, digest, sizeof(digest));
	test_hmac(digest, expect);
}

static void
hmac_md5_test5(void)
{
	const uint8_t text[] = "Test With Truncation";
	const uint8_t expect[] = {
	    0x56, 0x46, 0x1e, 0xf2, 0x34, 0x2e, 0xdc, 0x00,
	    0xf9, 0xba, 0xb9, 0x95, 0x69, 0x0e, 0xfd, 0x4c,
	};
	uint8_t digest[16];
	uint8_t key[16];
	int i;

	printf ("HMAC MD5 Test 5:\t\t");
	for (i = 0; i < 16; i++)
		key[i] = 0x0c;
	hmac("md5", key, 16, text, 20, digest, sizeof(digest));
	test_hmac(digest, expect);
}

static void
hmac_md5_test6(void)
{
	const uint8_t text[] = "Test Using Larger Than Block-Size Key - Hash Key First";
	const uint8_t expect[] = {
	    0x6b, 0x1a, 0xb7, 0xfe, 0x4b, 0xd7, 0xbf, 0x8f,
	    0x0b, 0x62, 0xe6, 0xce, 0x61, 0xb9, 0xd0, 0xcd,
	};
	uint8_t digest[16];
	uint8_t key[80];
	int i;

	printf ("HMAC MD5 Test 6:\t\t");
	for (i = 0; i < 80; i++)
		key[i] = 0xaa;
	hmac("md5", key, 80, text, 54, digest, sizeof(digest));
	test_hmac(digest, expect);
}

static void
hmac_md5_test7(void)
{
	const uint8_t text[] = "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data";
	const uint8_t expect[] = {
	    0x6f, 0x63, 0x0f, 0xad, 0x67, 0xcd, 0xa0, 0xee,
	    0x1f, 0xb1, 0xf5, 0x62, 0xdb, 0x3a, 0xa5, 0x3e,
	};
	uint8_t digest[16];
	uint8_t key[80];
	int i;

	printf ("HMAC MD5 Test 7:\t\t");
	for (i = 0; i < 80; i++)
		key[i] = 0xaa;
	hmac("md5", key, 80, text, 73, digest, sizeof(digest));
	test_hmac(digest, expect);
}

int test_hmac_md5(void)
{

	printf ("Starting RFC2202 HMAC MD5 tests...\n\n");
	hmac_md5_test1();
	hmac_md5_test2();
	hmac_md5_test3();
	hmac_md5_test4();
	hmac_md5_test5();
	hmac_md5_test6();
	hmac_md5_test7();
	printf("\nAll tests pass.\n");
	return 0;
}
