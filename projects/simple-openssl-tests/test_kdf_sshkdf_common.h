/* test_kdf_sshkdf_common.h -- shared machinery for the SSHKDF family tests
 * (EVP_KDF "SSHKDF", the SSH RFC 4253 exchange-hash KDF).
 *
 * One binary per digest (test_kdf_sshkdf_sha1.c and test_kdf_sshkdf_sha256.c);
 * each binary defines one struct sshkdf_family row (its OSSL_KDF_PARAM_DIGEST
 * value and its digest-sized xcghhash) and its own KAT rows, then calls the
 * per-digest test routines below.  The big explanatory comment (the NIST
 * CAVS 14.1 vector provenance, the CLI verification commands, the xcghash
 * spelling note and the wrong-length-xcghhash behavior note) lives in
 * test_kdf_sshkdf_common.c.
 */

#ifndef TEST_KDF_SSHKDF_COMMON_H
#define TEST_KDF_SSHKDF_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per digest: the OSSL_KDF_PARAM_DIGEST value passed to the
 * "SSHKDF" KDF and the digest-sized xcghhash the KAT/distinctness/negative
 * sections run with (RFC 4253 requires H to be the digest size; the
 * session id stays the shared 20-byte CAVS set 1 value in every binary,
 * exactly as the monolith's SHA-256 sections did). */
struct sshkdf_family {
    const char *digest;
    const char *h_hex;
};

/* One KAT row: the derivation type letter, the number of bytes derived
 * (>= outlen: the monolith's SHA-1 section derived the longest CAVS length,
 * 24, for every row, checking that the CAVS value is always a prefix and
 * that longer derives extend it) and the number of bytes compared (the
 * row's CAVS length), plus the expected output. */
struct ssh_kat {
    char type;
    size_t derivelen;
    size_t outlen;
    const char *outhex;
};

/* Shared KAT material: CAVS set 1 (a 129-byte K, and H == SID -- the
 * initial-IV case), plus the 15-byte xcghhash prefix the wrong-length
 * section feeds (the first 15 bytes of the family's H constants, which
 * share this prefix).  Declared as arrays, not pointers, so that they are
 * address constants usable in the per-digest binaries' static struct
 * initializers. */
extern const char sshkdf_k1_hex[];
extern const char sshkdf_h1_hex[];
extern const char sshkdf_sid1_hex[];
extern const char sshkdf_hshort_hex[];

/* One SSHKDF derive with a fresh context.  Returns 1 on success, 0 on
 * failure.  SSHKDF is core OpenSSL 3 functionality: a fetch failure fails
 * the program (never SKIP). */
int sshkdf_derive(const char *digest, const unsigned char *key,
                  size_t keylen, const unsigned char *xcghash,
                  size_t xlen, const unsigned char *session_id,
                  size_t sidlen, const char *type,
                  unsigned char *out, size_t outlen);

/* (1) The KAT rows of the calling binary's digest: each row is derived,
 * and its first outlen bytes must match the CAVS value (the CAVS value is
 * always a prefix, and longer derives extend it). */
void sshkdf_test_kats(const struct sshkdf_family *fam,
                      const struct ssh_kat *kats, size_t nkats);

/* (2) The six types produce six pairwise-distinct outputs, over the
 * binary's own digest and digest-sized H (24 bytes derived per type). */
void sshkdf_test_types_differ(const struct sshkdf_family *fam);

/* (3) Error paths and the wrong-length xcghhash behavior: a type outside
 * A..F, a missing xcghhash and a missing session id must be rejected; a
 * 15-byte xcghhash is accepted (the default provider does not enforce the
 * RFC 4253 digest-size requirement).  short_h_out_hex != NULL compares the
 * wrong-length derive against that verified output constant (the SHA-1
 * binary's 8-byte value); short_h_out_hex == NULL derives twice and
 * requires identical output (no verified wrong-length constant exists for
 * the digest -- the same deterministic acceptance, checked relatively). */
void sshkdf_test_negative(const struct sshkdf_family *fam,
                          const char *short_h_out_hex);

#endif /* TEST_KDF_SSHKDF_COMMON_H */