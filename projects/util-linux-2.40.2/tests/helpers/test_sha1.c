/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2017 Philip Prindeville <philipp@redfish-solutions.com>
 */
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>

#include "sha1.h"

int main(void)
{
	int i, ret;
	UL_SHA1_CTX ctx;
	unsigned char digest[UL_SHA1LENGTH];
	unsigned char buf[BUFSIZ];

	ul_SHA1Init( &ctx );

	while(!feof(stdin) && !ferror(stdin)) {
		ret = fread(buf, 1, sizeof(buf), stdin);
		if (ret)
			ul_SHA1Update( &ctx, buf, ret );
	}

	if(freopen ("/dev/null", "r", stdin) == NULL)
		err(EXIT_FAILURE, "stdin->null failed!");

	ul_SHA1Final( digest, &ctx );

	for (i = 0; i < UL_SHA1LENGTH; i++)
		printf( "%02x", digest[i] );
	printf("\n");
	return 0;
}
