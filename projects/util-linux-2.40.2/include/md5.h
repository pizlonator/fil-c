/*
 * No copyright is claimed. This code is in the public domain; do with it what
 * you wish.
 */
#ifndef UTIL_LINUX_MD5_H
#define UTIL_LINUX_MD5_H

#include <stdint.h>

#define UL_MD5LENGTH 16

struct UL_MD5Context {
	uint32_t buf[4];
	uint32_t bits[2];
	unsigned char in[64];
};

void ul_MD5Init(struct UL_MD5Context *ctx);
void ul_MD5Update(struct UL_MD5Context *ctx, unsigned char const *buf, unsigned len);
void ul_MD5Final(unsigned char digest[UL_MD5LENGTH], struct UL_MD5Context *ctx);
void ul_MD5Transform(uint32_t buf[4], uint32_t const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct UL_MD5Context UL_MD5_CTX;

#endif /* !UTIL_LINUX_MD5_H */
