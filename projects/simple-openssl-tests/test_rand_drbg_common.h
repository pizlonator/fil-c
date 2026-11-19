/* test_rand_drbg_common.h -- shared machinery for the EVP_RAND family tests
 * (CTR-DRBG, HASH-DRBG, HMAC-DRBG and TEST-RAND).
 *
 * One binary per RAND (test_rand_ctr_drbg.c, test_rand_hash_drbg.c,
 * test_rand_hmac_drbg.c and test_rand_test_rand.c); each binary keeps its
 * own algorithm-specific test sections (the algorithm parameters, the
 * seeding-knob tests, TEST-RAND's entropy/generate modes) and calls the
 * shared helpers below.  The family explanatory comment (the covered
 * behavior and the never-SKIP note) lives in test_rand_drbg_common.c.
 */

#ifndef TEST_RAND_DRBG_COMMON_H
#define TEST_RAND_DRBG_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* Reads an unsigned integer context parameter ("state", "strength",
 * "reseed_counter", ...).  Returns 1 on success. */
int rand_get_uint_param(EVP_RAND_CTX *ctx, const char *name, unsigned int *out);

/* Reads a size_t context parameter ("max_request").  Returns 1 on success. */
int rand_get_size_param(EVP_RAND_CTX *ctx, const char *name, size_t *out);

/* Sets a single UTF8 string parameter (a static copy is made, as the
 * OSSL_PARAM utf8 constructor requests). */
void rand_set_string_param(EVP_RAND_CTX *ctx, const char *name,
                           const char *value);

/* Sets a single unsigned integer parameter. */
void rand_set_uint_param(EVP_RAND_CTX *ctx, const char *name,
                         unsigned int value);

/* The generate-and-compare flow the three DRBG binaries share: generates 1,
 * 32 and 1024 bytes, then a second 32-byte block, and CHECKs that the two
 * 32-byte blocks differ (under the unpoison_all()/repoison_all()
 * comparison discipline).  `name` ("CTR-DRBG", "HASH-DRBG", "HMAC-DRBG")
 * is interpolated into the CHECK message texts.  buf1, buf32, buf32b and
 * out1024 are caller-allocated working buffers (1, 32, 32 and 1024 bytes)
 * so that the poison-allocation lifetime stays with the caller, which
 * keeps free_poison_allocations() in the same logical place as the
 * original monolith. */
void rand_drbg_generate_flow(EVP_RAND_CTX *ctx, const char *name,
                             unsigned char *buf1, unsigned char *buf32,
                             unsigned char *buf32b, unsigned char *out1024);

#endif /* TEST_RAND_DRBG_COMMON_H */