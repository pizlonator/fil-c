/* test_kdf_pkcs12kdf_common.h -- shared machinery for the PKCS12KDF family
 * tests (EVP_KDF "PKCS12KDF", RFC 7292 appendix B).
 *
 * One binary per digest (test_kdf_pkcs12kdf_sha1.c and
 * test_kdf_pkcs12kdf_sha256.c); each binary defines one struct pkcs12_family
 * row (its OSSL_KDF_PARAM_DIGEST value) and its own KAT rows, then calls the
 * per-digest test routines below.  The big explanatory comment (the RFC
 * 7292 note, the CLI verification commands and the constants' provenance)
 * lives in test_kdf_pkcs12kdf_common.c.
 */

#ifndef TEST_KDF_PKCS12KDF_COMMON_H
#define TEST_KDF_PKCS12KDF_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per digest: the OSSL_KDF_PARAM_DIGEST value passed to the
 * "PKCS12KDF" KDF. */
struct pkcs12_family {
    const char *digest;
};

/* One KAT row: the material id (1 = key, 2 = IV, 3 = MAC secret), the
 * iteration count and the expected derived key (all rows derive the full
 * 32 bytes). */
struct pkcs12_kat {
    int id;
    unsigned long iter;
    const char *dkhex;
};

/* Shared KAT material: pass = "password" (70617373776f7264), salt = "salt"
 * (73616c74) -- every row and every section of both digests derives from
 * the same pass and salt.  Declared as arrays, not pointers, so that they
 * are address constants usable in the per-digest binaries' static struct
 * initializers. */
extern const char pkcs12_pass_hex[];
extern const char pkcs12_salt_hex[];

/* One PKCS12KDF derive with a fresh context.  pass or salt may be NULL to
 * exercise the "missing pass/salt" error paths.  Returns 1 on success, 0 on
 * failure.  PKCS12KDF is core OpenSSL 3 functionality: a fetch failure
 * fails the program (never SKIP). */
int pkcs12_derive(const char *digest, const unsigned char *pass,
                  size_t passlen, const unsigned char *salt,
                  size_t saltlen, unsigned long iter, int id,
                  unsigned char *out, size_t outlen);

/* (1) The KAT rows of the calling binary's digest: the material ids and
 * (for SHA-256) the iteration-count-of-1 row.  All 32 bytes of output. */
void pkcs12_test_kats(const struct pkcs12_family *fam,
                      const struct pkcs12_kat *kats, size_t nkats);

/* (2) The three material ids (key / IV / MAC secret) must all be derived
 * from the same pass and salt yet produce different output, for the
 * binary's own digest. */
void pkcs12_test_ids_differ(const struct pkcs12_family *fam);

/* (3) The pass and the salt are both mandatory: omitting either must fail,
 * for the binary's own digest. */
void pkcs12_test_missing_inputs(const struct pkcs12_family *fam);

#endif /* TEST_KDF_PKCS12KDF_COMMON_H */