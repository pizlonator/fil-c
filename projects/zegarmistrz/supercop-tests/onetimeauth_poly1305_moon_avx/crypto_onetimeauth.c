#define poly1305_auth_fn CRYPTO_NAMESPACETOP

#include <stddef.h>
#include "crypto_onetimeauth.h"
#include "crypto_verify_16.h"

int crypto_onetimeauth_verify(const unsigned char *h, const unsigned char *in, unsigned long long inlen, const unsigned char *k) {
	unsigned char mac[16];
	poly1305_auth_fn(mac, in, (size_t)inlen, k);
	return crypto_verify_16(h, mac);
}

