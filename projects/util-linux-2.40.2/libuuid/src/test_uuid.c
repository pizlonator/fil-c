/*
 * tst_uuid.c --- test program from the UUID library
 *
 * Copyright (C) 1996, 1997, 1998 Theodore Ts'o.
 *
 * %Begin-Header%
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * %End-Header%
 */

#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#define UUID MYUUID
#endif

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "c.h"
#include "uuid.h"

static int test_uuid(const char * uuid, int isValid)
{
	static const char * validStr[2] = {"invalid", "valid"};
	uuid_t uuidBits;
	int parsedOk;

	parsedOk = uuid_parse(uuid, uuidBits) == 0;

	printf("%s is %s", uuid, validStr[isValid]);
	if (parsedOk != isValid) {
		printf(" but uuid_parse says %s\n", validStr[parsedOk]);
		return 1;
	}
	printf(", OK\n");
	return 0;
}

static int check_uuids_in_file(const char *file)
{
	int ret = 0;
	size_t alloc = 0;
	ssize_t sz;
	char *str = NULL;
	FILE *f;
	uuid_t uuidBits;

	if ((f = fopen(file, "r")) == NULL) {
		warn("%s", file);
		return 1;
	}
	while ((sz = getline(&str, &alloc, f)) != -1) {
		if (sz == 0 || str[0] == '\n')
			continue;
		if (str[sz - 1] == '\n')
			str[sz - 1] = '\0';
		if (uuid_parse(str, uuidBits)) {
			warnx("%s: %s", file, str);
			ret++;
		}
	}

	fclose(f);
	free(str);
	return ret;
}

int
main(int argc, char **argv)
{
	int failed = 0;

	if (argc < 2) {
		failed += test_uuid("84949cc5-4701-4a84-895b-354c584a981b", 1);
		failed += test_uuid("84949CC5-4701-4A84-895B-354C584A981B", 1);
		failed += test_uuid("84949cc5-4701-4a84-895b-354c584a981bc", 0);
		failed += test_uuid("84949cc5-4701-4a84-895b-354c584a981", 0);
		failed += test_uuid("84949cc5x4701-4a84-895b-354c584a981b", 0);
		failed += test_uuid("84949cc504701-4a84-895b-354c584a981b", 0);
		failed += test_uuid("84949cc5-470104a84-895b-354c584a981b", 0);
		failed += test_uuid("84949cc5-4701-4a840895b-354c584a981b", 0);
		failed += test_uuid("84949cc5-4701-4a84-895b0354c584a981b", 0);
		failed += test_uuid("g4949cc5-4701-4a84-895b-354c584a981b", 0);
		failed += test_uuid("84949cc5-4701-4a84-895b-354c584a981g", 0);
		failed += test_uuid("00000000-0000-0000-0000-000000000000", 1);
		failed += test_uuid("01234567-89ab-cdef-0134-567890abcedf", 1);
		failed += test_uuid("ffffffff-ffff-ffff-ffff-ffffffffffff", 1);
	} else {
		int i;

		for (i = 1; i < argc; i++)
			failed += check_uuids_in_file(argv[i]);
	}
	if (failed) {
		printf("%d failures.\n", failed);
		exit(1);
	}
	return 0;
}
