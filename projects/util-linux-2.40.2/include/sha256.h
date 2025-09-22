/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_SHA256_H
#define UTIL_LINUX_SHA256_H

#include <stddef.h>

#define UL_SHA256LENGTH		32

extern void ul_SHA256(unsigned char hash_out[UL_SHA256LENGTH], const unsigned char *str, size_t len);

#endif /* UTIL_LINUX_SHA256_H */
