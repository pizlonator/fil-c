/* test_kdf_argon2_common.h -- shared machinery for the Argon2d / Argon2i /
 * Argon2id family tests.
 *
 * One binary per variant (test_kdf_argon2d.c, test_kdf_argon2i.c and
 * test_kdf_argon2id.c); each binary defines one struct argon2_variant row
 * with its own RFC 9106 section 5 tag (plus, for Argon2id, the extra
 * draft-vector and version 1.0 KATs) and calls the per-variant test
 * routines below.  The big explanatory comment (the RFC 9106 vector
 * provenance, the KAT correction note and the version gate) lives in
 * test_kdf_argon2_common.c.
 */

#ifndef TEST_KDF_ARGON2_COMMON_H
#define TEST_KDF_ARGON2_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per Argon2 variant: the fetchable algorithm name (ARGON2D /
 * ARGON2I / ARGON2ID are separate fetchable algorithms; the variant is not
 * a settable parameter) and the RFC 9106 section 5 tag for the shared
 * section 5 inputs. */
struct argon2_variant {
    const char *algname;
    const char *rfctaghex;
};

/* Shared KAT material: the RFC 9106 section 5 vector inputs (P = 01 x 32,
 * S = 02 x 16, secret = 03 x 8, associated data = 04 x 12).  Declared as
 * arrays, not pointers, so that they are address constants usable where
 * the per-variant binaries need them. */
extern const char argon2_rfc_pass_hex[];
extern const char argon2_rfc_salt_hex[];
extern const char argon2_rfc_secret_hex[];
extern const char argon2_rfc_ad_hex[];

/* The whole-program availability gate: Argon2 is newer than the OpenSSL
 * 3.0 series, so on older releases (e.g.
 * the system OpenSSL 3.0.13) the fetch fails and the binary takes the SKIP
 * path with the original reason text. */
void argon2_test_gate(const struct argon2_variant *v);

/* One Argon2 derive with a fresh context fetched by algorithm name.
 * Returns 1 on success, 0 if EVP_KDF_derive failed.  The SKIP decision has
 * already been made. */
int argon2_derive(const char *algname, const unsigned char *pass,
                  size_t passlen, const unsigned char *salt,
                  size_t saltlen, const unsigned char *secret,
                  size_t secretlen, const unsigned char *ad,
                  size_t adlen, unsigned long lanes, unsigned long threads,
                  unsigned long memcost, unsigned long iter, long version,
                  unsigned char *out, size_t outlen);

/* (1) The RFC 9106 section 5 tag for v's variant with t=3, m=32, p=4. */
void argon2_test_rfc_kat(const struct argon2_variant *v);

/* (2) Without the secret and associated data the three variants still
 * produce three different tags. */
void argon2_test_variants_distinct(const struct argon2_variant *v);

/* (4) Validation for v's variant: a missing salt, lanes=0, memcost<8,
 * iter=0, a short salt and a too-short output must all fail. */
void argon2_test_validation(const struct argon2_variant *v);

#endif /* TEST_KDF_ARGON2_COMMON_H */
