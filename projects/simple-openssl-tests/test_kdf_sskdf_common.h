/* test_kdf_sskdf_common.h -- shared machinery for the SSKDF (NIST SP 800-56C)
 * family tests (EVP_KDF "SSKDF").
 *
 * One binary per digest (test_kdf_sskdf_sha1.c and test_kdf_sskdf_sha256.c);
 * each binary defines one struct sskdf_family row (its OSSL_KDF_PARAM_DIGEST
 * value) and its own KAT rows, then calls the per-digest test routines
 * below.  The big explanatory comment (the NIST CAVS vector provenance, the
 * CLI verification commands and the CMAC-mode note) lives in
 * test_kdf_sskdf_common.c.
 */

#ifndef TEST_KDF_SSKDF_COMMON_H
#define TEST_KDF_SSKDF_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per digest: the OSSL_KDF_PARAM_DIGEST value passed to the "SSKDF"
 * KDF. */
struct sskdf_family {
    const char *digest;
};

/* One KAT row: the MAC name (NULL = the digest-based mode; "HMAC" selects
 * the MAC-based mode), the shared secret z, the optional salt (NULL = no
 * salt), the info, the requested output length and the expected output. */
struct ss_kat {
    const char *mac;
    const char *zhex;
    const char *salthex;  /* NULL = no salt */
    const char *infohex;
    size_t outlen;
    const char *outhex;
};

/* Shared KAT material: the 16-byte shared secret z and the 12-byte info of
 * the digest-based CAVS rows (every digest-based KAT row of both digests
 * uses them, as do the key/secret alias and missing-key sections), and the
 * 48-byte 000102...2f info that the multi-chunk info section splits into
 * 8/8/8/8/16-byte chunks in every binary.  Declared as arrays, not
 * pointers, so that they are address constants usable in the per-digest
 * binaries' static struct initializers. */
extern const char sskdf_z16_hex[];
extern const char sskdf_info12_hex[];
extern const char sskdf_info48_hex[];

/* One SSKDF derive with a fresh context.  `keyparam` selects the parameter
 * name for the shared secret ("key" or "secret" -- the provider accepts
 * both for the same field).  Returns 1 on success, 0 on failure.  SSKDF is
 * core OpenSSL 3 functionality: a fetch failure fails the program. */
int sskdf_derive(const char *digest, const char *mac, const char *keyparam,
                 const unsigned char *z, size_t zlen,
                 const unsigned char *salt, size_t saltlen,
                 unsigned char *info, size_t infolen,
                 unsigned char *out, size_t outlen);

/* (1) The KAT rows of the calling binary's digest, in both modes (the
 * digest-based rows and, for SHA-256, the HMAC row) and all output
 * lengths. */
void sskdf_test_kats(const struct sskdf_family *fam,
                     const struct ss_kat *kats, size_t nkats);

/* (2) The "secret" parameter is an alias of "key": both routes must give
 * the identical output, over the z/info of the binary's first KAT row and
 * the binary's own digest. */
void sskdf_test_key_secret_alias(const struct sskdf_family *fam,
                                 const struct ss_kat *kat);

/* (3) Up to five "info" params are accepted and, on OpenSSL releases that
 * concatenate repeated info params (3.2+), five chunks (8+8+8+8+16 bytes)
 * equal the single 48-byte info.  On 3.0.x only the first chunk is used;
 * the program prints a note instead of failing.  A sixth chunk is rejected
 * outright where SSKDF_MAX_INFOS == 5 applies (3.1+); older releases accept
 * it, which is noted rather than failed.  okmhex != NULL compares the
 * five-chunk derive against that verified OKM constant (the SHA-256
 * binary's 32-byte value); okmhex == NULL derives the reference once with
 * the full info as a single param and requires the chunks to reproduce it
 * (no multi-info OKM is verified for SHA-1 -- the same concatenation
 * behavior, checked relatively). */
void sskdf_test_multi_info(const struct sskdf_family *fam, const char *okmhex);

/* (4) Deriving without the shared secret must fail (over the info of the
 * binary's first KAT row, with a 16-byte output). */
void sskdf_test_missing_key(const struct sskdf_family *fam,
                            const struct ss_kat *kat);

#endif /* TEST_KDF_SSKDF_COMMON_H */