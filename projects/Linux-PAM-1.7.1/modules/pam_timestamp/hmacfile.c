/*
 * Copyright 2003,2004 Red Hat, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pam_inline.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hmacsha1.h"

static void
testvectors(void)
{
	void *hmac;
	size_t hmac_len;
	size_t i, j;
	char hex[3];
	struct vector {
		const char *key;
		int key_len;
		const char *data;
		int data_len;
		const char *hmac;
	} vectors[] = {
		{
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 20,
		"Hi There", 8,
		"b617318655057264e28bc0b6fb378c8ef146be00",
		},

#ifdef HMAC_ALLOW_SHORT_KEYS
		{
		"Jefe", 4,
		"what do ya want for nothing?", 28,
		"effcdf6ae5eb2fa2d27416d5f184df9c259a7c79",
		},
#endif

		{
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", 20,
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd", 50,
		"125d7342b9ac11cd91a39af48aa17b4f63f175d3",
		},

		{
		"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19", 25,
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
		50,
		"4c9007f4026250c6bc8414f9bf50c86c2d7235da",
		},

		{
		"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c", 20,
		"Test With Truncation", 20,
		"4c1a03424b55e07fe7f27be1d58bb9324a9a5a04",
		},

		{
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		80,
		"Test Using Larger Than Block-Size Key - Hash Key First", 54,
		"aa4ae5e15272d00e95705637ce8a3b55ed402112",
		},

		{
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		80,
		"Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data", 73,
		"e8e99d0f45237d786d6bbaa7965c7808bbff1a91",
		},
	};
	for (i = 0; i < PAM_ARRAY_SIZE(vectors); i++) {
		hmac = NULL;
		hmac_len = 0;
		hmac_sha1_generate(&hmac, &hmac_len,
				   vectors[i].key, vectors[i].key_len,
				   vectors[i].data, vectors[i].data_len);
		if (hmac != NULL) {
			unsigned char *hmacc = hmac;
			for (j = 0; j < hmac_len; j++) {
				pam_sprintf(hex, "%02x", hmacc[j] & 0xff);
				if (strncasecmp(hex,
						vectors[i].hmac + 2 * j,
						2) != 0) {
					printf("Incorrect result for vector %lu\n",
					       (unsigned long) i + 1);
					exit(1);

				}
			}
			free(hmac);
		} else {
			printf("Error in vector %lu.\n",
			       (unsigned long) i + 1);
			exit(1);
		}
	}
}

int
main(int argc, char **argv)
{
	void *hmac;
	size_t maclen;
	const char *keyfile;
	int i;
	size_t j;

	testvectors();

	keyfile = argv[1];
	for (i = 2; i < argc; i++) {
		hmac_sha1_generate_file(NULL, &hmac, &maclen, keyfile, -1, -1,
					argv[i], strlen(argv[i]));
		if (hmac != NULL) {
			unsigned char *hmacc = hmac;
			for (j = 0; j < maclen; j++) {
				printf("%02x", hmacc[j] & 0xff);
			}
			printf("  %s\n", argv[i]);
			free(hmac);
		}
	}
	return 0;
}
