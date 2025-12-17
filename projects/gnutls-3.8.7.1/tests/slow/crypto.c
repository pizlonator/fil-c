#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <gnutls/crypto.h>

/* this tests whether including crypto.h is sufficient to use
 * its functionality */

int main(int argc, char **argv)
{
	char output[64];
	gnutls_global_init();
	assert(gnutls_hash_fast(GNUTLS_DIG_SHA256, "abc", 3, output) >= 0);
	gnutls_global_deinit();
	return 0;
}
