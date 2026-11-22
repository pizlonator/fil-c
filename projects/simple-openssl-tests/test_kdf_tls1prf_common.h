/* test_kdf_tls1prf_common.h -- shared machinery for the TLS1-PRF MD5-SHA1 /

 * SHA256 family tests (EVP_KDF "TLS1-PRF", the TLS 1.0/1.1 and TLS 1.2
 * PRFs).

 *

 * One binary per digest (test_kdf_tls1prf_md5_sha1.c, the TLS 1.0/1.1
 * MD5-SHA1 combo, and test_kdf_tls1prf_sha256.c, the TLS 1.2 single-digest
 * PRF); each binary defines one struct tls1prf_family row (its
 * OSSL_KDF_PARAM_DIGEST value, the digest's prose name for the failure
 * messages, and the TLS version the digest instantiates) and one struct
 * tls1prf_kat row (its NIST master-secret KAT over the shared "master
 * secret" label), then calls the per-digest test routines below.  The big
 * explanatory comment (the RFC 2246/4346/5246/8446 framing, the NIST vector
 * provenance, the CLI verification commands and the distribution decisions)
 * lives in test_kdf_tls1prf_common.c.

 */



#ifndef TEST_KDF_TLS1PRF_COMMON_H

#define TEST_KDF_TLS1PRF_COMMON_H



#include "test_common.h"

#include <stddef.h>

#include <openssl/evp.h>



/* One row per digest: the OSSL_KDF_PARAM_DIGEST value passed to the bare

 * "TLS1-PRF" KDF, the digest's prose name (for the failure messages, where
 * the original monolith wrote "SHA-256" with the hyphen), and the TLS
 * version the digest instantiates ("TLS 1.1" for the MD5-SHA1 combo, which
 * splits the secret into two halves sharing the middle byte, and "TLS 1.2"
 * for SHA-256). */

struct tls1prf_family {

    const char *digest;

    const char *digest_prose;

    const char *tls_version;

};



/* One digest's NIST master-secret KAT: the 48-byte master secret, the

 * 32-byte client/server randoms and the expected 48-byte output, over the
 * shared ASCII "master secret" label (the KAT seed is label ||
 * client_random || server_random). */

struct tls1prf_kat {

    const char *secrethex;

    const char *label;   /* ASCII; the family-shared tls1prf_label */

    const char *crhex;

    const char *srhex;

    const char *outhex;

};



/* Shared KAT material: the ASCII "master secret" label that seeds every

 * master-secret KAT and the concat-seed/odd-secret sections, and the 7-byte
 * synthetic seed (01020304050607) of the empty-secret and missing-input
 * sections.  Declared as arrays, not pointers, so that they are address
 * constants usable in the per-digest binaries' static struct initializers
 * (each binary's tls1prf_kat label member references tls1prf_label). */

extern const char tls1prf_label[];

extern const char tls1prf_test_seed_hex[];



/* Copies an ASCII string (the "master secret" label) into a labeled

 * buffer; returns its length. */

size_t tls1prf_from_str(const char *s, unsigned char *dst, size_t dstcap);



/* One TLS1-PRF derive with a fresh context; up to three seed chunks.  A

 * NULL chunk pointer terminates the list.  Returns 1 on success, 0 if
 * EVP_KDF_derive failed.  TLS1-PRF is core OpenSSL 3 functionality: a
 * fetch failure fails the program (never SKIP). */

int tls1prf_derive(const char *digest, const unsigned char *secret,

                   size_t secretlen, const unsigned char *seed1,

                   size_t seed1len, const unsigned char *seed2,

                   size_t seed2len, const unsigned char *seed3,

                   size_t seed3len, unsigned char *out, size_t outlen);



/* (1) The NIST master-secret KAT of the calling binary's digest, with the

 * seed delivered as three chunks (label, client_random, server_random) --
 * TLS1-PRF accepts up to six seed chunks (TLSPRF_MAX_SEEDS) -- plus the
 * same KAT truncated to 16 bytes (prefix property). */

void tls1prf_test_kat_multi_seed(const struct tls1prf_family *fam,

                                 const struct tls1prf_kat *kat);



/* (2) Seed chunks may also be pre-concatenated: one seed with the label and

 * both randoms glued together must give the identical output (the binary's
 * own KAT). */

void tls1prf_test_concat_seed(const struct tls1prf_family *fam,

                              const struct tls1prf_kat *kat);



/* (3) An odd-length secret (TLS <= 1.1 splits it into two halves sharing

 * the middle byte) still derives deterministically: the secret is the KAT's
 * 48-byte value with its last byte dropped, the seed the label alone, 32
 * bytes out.  MD5-SHA1-only: the verified odd-secret output constant is a
 * TLS 1.1 value, so the other digest does not run this section. */

void tls1prf_test_odd_secret(const struct tls1prf_family *fam,

                             const struct tls1prf_kat *kat,

                             const char *outhex);



/* (4) An explicitly empty (zero-length) secret derives deterministically --

 * the OpenSSL edge case: an explicitly set zero-length secret is accepted
 * (both the 3.0.13 and 3.6.5 CLI agree on the result), while omitting the
 * secret entirely is rejected (see (5)).  SHA-256-only: the verified
 * empty-secret output constant is a SHA-256 value over the shared 7-byte
 * synthetic seed, so the other digest does not run this section. */

void tls1prf_test_empty_secret(const struct tls1prf_family *fam,

                               const char *outhex);



/* (5) Omitting the secret or the seed must fail.  The provider's

 * missing-input checks are digest-independent, so every binary runs this
 * section for its own digest; secrethex is the binary's own KAT secret,
 * whose value is irrelevant to the must-fail assertions. */

void tls1prf_test_missing_inputs(const struct tls1prf_family *fam,

                                 const char *secrethex);



#endif /* TEST_KDF_TLS1PRF_COMMON_H */