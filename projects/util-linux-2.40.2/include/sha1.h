/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_SHA1_H
#define UTIL_LINUX_SHA1_H

/*
   SHA-1 in C
   By Steve Reid <steve@edmweb.com>
   100% Public Domain
 */

#include "stdint.h"

#define UL_SHA1LENGTH		20

typedef struct
{
    uint32_t	state[5];
    uint32_t	count[2];
    unsigned	char buffer[64];
} UL_SHA1_CTX;

void ul_SHA1Transform(uint32_t state[5], const unsigned char buffer[64]);
void ul_SHA1Init(UL_SHA1_CTX *context);
void ul_SHA1Update(UL_SHA1_CTX *context, const unsigned char *data, uint32_t len);
void ul_SHA1Final(unsigned char digest[UL_SHA1LENGTH], UL_SHA1_CTX *context);
void ul_SHA1(char *hash_out, const char *str, unsigned len);

#endif /* UTIL_LINUX_SHA1_H */
