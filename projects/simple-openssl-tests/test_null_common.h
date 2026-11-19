/* test_null_common.h -- shared machinery for the NULL identity-object
 * tests (the "NULL" cipher, EVP_enc_null(), and the "NULL" digest,
 * EVP_md_null()).
 *
 * Two binaries, one object each (test_null_cipher.c and
 * test_null_digest.c); the big explanatory comment (the API-contract
 * probing rationale and the per-object behavior notes) lives in
 * test_null_common.c, and each binary carries its half of it.
 */

#ifndef TEST_NULL_COMMON_H
#define TEST_NULL_COMMON_H

#include "test_common.h"
#include <stddef.h>

/* Length of the shared deterministic pattern (both binaries feed the same
 * 64 bytes through their object). */
#define NULL_PATTERN_LEN 64

/* Fills buf[0..n) with the deterministic pattern i*3+1. */
void null_fill_pattern(unsigned char *buf, size_t n);

#endif /* TEST_NULL_COMMON_H */
