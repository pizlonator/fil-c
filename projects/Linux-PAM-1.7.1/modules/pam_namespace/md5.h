
#ifndef MD5_H
#define MD5_H

#include "pam_cc_compat.h"

typedef unsigned int uint32;

struct MD5Context {
	union {
		uint32 i[4];
		unsigned char c[16] PAM_ATTRIBUTE_ALIGNED(4);
	} buf;
	uint32 bits[2];
	union {
		uint32 i[16];
		unsigned char c[64] PAM_ATTRIBUTE_ALIGNED(4);
	} in;
};

#define MD5_DIGEST_LENGTH 16

void MD5Init(struct MD5Context *);
void MD5Update(struct MD5Context *, unsigned const char *, unsigned);
void MD5Final(unsigned char digest[MD5_DIGEST_LENGTH], struct MD5Context *);
void MD5Transform(uint32 buf[4], uint32 const in[MD5_DIGEST_LENGTH]);
void MD5(unsigned const char *, unsigned, unsigned char digest[MD5_DIGEST_LENGTH]);


/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */

typedef struct MD5Context MD5_CTX;

#endif				/* MD5_H */
