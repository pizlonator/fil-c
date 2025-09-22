/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.	This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#ifndef MD5_H_
#define MD5_H_

#define MD5_DIGEST_LENGTH	16
#define MD5_BLOCK_LENGTH	64

typedef struct MD5Context {
	uint32_t state[4];	/* state (ABCD) */
	uint64_t count;		/* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[MD5_BLOCK_LENGTH]; /* input buffer */
} MD5_CTX;

void	MD5Init(MD5_CTX *);
void	MD5Update(MD5_CTX *, const unsigned char *, size_t);
void	MD5Final(unsigned char[MD5_DIGEST_LENGTH], MD5_CTX *);
#endif
