/* test_kdf_hkdf_common.h -- shared machinery for the HKDF-SHA1 / HKDF-SHA256
 * / HKDF-SHA512 family tests (EVP_KDF "HKDF", RFC 5869).
 *
 * One binary per digest (test_kdf_hkdf_sha1.c, test_kdf_hkdf_sha256.c and
 * test_kdf_hkdf_sha512.c); each binary defines one struct hkdf_family row
 * (its OSSL_KDF_PARAM_DIGEST value, the digest's prose name for the failure
 * messages, and the digest output size), its own RFC 5869 KAT rows, its own
 * EXTRACT_ONLY PRK rows and its own concrete HKDF-SHA* rows, then calls the
 * per-digest test routines below.  The big explanatory comment (the RFC
 * 5869 vector provenance, the CLI verification commands and the concrete-
 * name version gate) lives in test_kdf_hkdf_common.c.
 */

#ifndef TEST_KDF_HKDF_COMMON_H
#define TEST_KDF_HKDF_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per digest: the OSSL_KDF_PARAM_DIGEST value passed to the bare
 * "HKDF" KDF, the digest's prose name (for the failure messages, where the
 * original monolith wrote "SHA-256" with the hyphen), and the digest output
 * size in bytes (the EXTRACT_ONLY PRK size). */
struct hkdf_family {
    const char *digest;
    const char *digest_prose;
    size_t mdsize;
};

/* One RFC 5869 test case in default EXTRACT_AND_EXPAND mode: the input
 * keying material ("key" param), the salt and the info ("" means absent),
 * the requested output length and the expected OKM.  `rfc_case` keeps the
 * row's RFC 5869 test case number for the failure messages (the original
 * monolith derived it from the row's array position; the per-digest tables
 * are no longer contiguous, so it is carried explicitly -- the SHA-1 and
 * SHA-512 rows are RFC case 1, exactly as the original's comment said). */
struct hkdf_kat {
    size_t rfc_case;
    const char *ikmhex;
    const char *salthex;
    const char *infohex;
    size_t outlen;
    const char *okmhex;
};

/* One EXTRACT_ONLY row: PRK = HMAC-Hash(salt, IKM).  prkhex == NULL marks a
 * digest with no verified PRK constant in the original monolith (it carried
 * only the SHA-256/SHA-384/SHA-512 PRKs): the row then still checks the
 * EXTRACT_ONLY mode, the PRK size and the derive success, but not the PRK
 * value. */
struct hkdf_prk_kat {
    size_t rfc_case;
    const char *ikmhex;
    const char *salthex;
    const char *prkhex;
};

/* One concrete HKDF-SHA* name row (OpenSSL 3.5+): the fetchable name, its
 * preset digest (for the bare-HKDF route of the equality check), the digest
 * size, the 42-byte EXTRACT_AND_EXPAND OKM and the EXTRACT_ONLY PRK, all
 * over the shared RFC 5869 TC1 inputs. */
struct hkdf_concrete {
    const char *kdfname;   /* the fetchable concrete name */
    const char *digest;    /* its preset digest, for the bare-HKDF route */
    size_t mdsize;         /* 32 / 48 / 64 */
    const char *okmhex;    /* 42-byte EXTRACT_AND_EXPAND OKM */
    const char *prkhex;    /* EXTRACT_ONLY PRK = HMAC-Hash(salt, IKM) */
};

/* Shared KAT material: the RFC 5869 TC1 inputs (IKM = 0b x 22, salt =
 * 000102..0c, info = f0f1..f9) and the TC2 inputs (80-byte IKM = 00..4f,
 * salt = 60..af, info = b0..ff).  Every digest's TC1 KAT row, the concrete-
 * name section and the negatives use the TC1 inputs; the multi-info section
 * splits the 80-byte TC2 info into five chunks in every binary, and only
 * the SHA-256 binary carries a verified TC2 OKM.  Declared as arrays, not
 * pointers, so that they are address constants usable in the per-digest
 * binaries' static struct initializers. */
extern const char hkdf_tc1_ikm_hex[];
extern const char hkdf_tc1_salt_hex[];
extern const char hkdf_tc1_info_hex[];
extern const char hkdf_tc2_ikm_hex[];
extern const char hkdf_tc2_salt_hex[];
extern const char hkdf_tc2_info_hex[];

/* Fetches `kdfname`, derives `outlen` bytes with `params`.  Returns 1 on
 * success, 0 if EVP_KDF_derive failed.  Fails the program if the KDF itself
 * cannot be fetched (HKDF is core OpenSSL 3 functionality: never SKIP). */
int hkdf_derive(const char *kdfname, const OSSL_PARAM params[],
                unsigned char *out, size_t outlen);

/* (1) EXTRACT_AND_EXPAND (the default mode) must reproduce every RFC 5869
 * OKM row of the calling binary's digest.  Salt/info of length zero are
 * simply not passed at all (TC3), which the RFC treats the same as an empty
 * string. */
void hkdf_test_extract_and_expand(const struct hkdf_family *fam,
                                  const struct hkdf_kat *kats, size_t nkats);

/* (2) EXTRACT_ONLY returns the PRK = HMAC-Hash(salt, IKM); its length must
 * be the digest size, requested through EVP_KDF_CTX_get_kdf_size(), and
 * every row with a verified PRK constant must reproduce it. */
void hkdf_test_extract_only(const struct hkdf_family *fam,
                            const struct hkdf_prk_kat *prks, size_t nprks);

/* (3) EXPAND_ONLY: the "key" is the PRK from a previous extract, and the
 * expand stage alone must reproduce the TC1 OKM of the binary's digest.
 * prkhex != NULL passes that PRK constant (the SHA-256 and SHA-512
 * binaries); prkhex == NULL derives the PRK with EXTRACT_ONLY first (the
 * SHA-1 binary, which has no verified PRK constant), so that the expand
 * stage is still checked against the same TC1 OKM with the true PRK as its
 * key -- the composition the constant-based form also relies on. */
void hkdf_test_expand_only(const struct hkdf_family *fam,
                           const struct hkdf_kat *tc1_row,
                           const char *prkhex);

/* (4) The "info" parameter may be delivered as several octet-string params,
 * which the provider concatenates: five 16-byte chunks over the shared
 * 80-byte TC2 inputs.  okmhex != NULL compares the five-chunk derive
 * against that OKM constant (the SHA-256 binary's verified TC2 KAT);
 * okmhex == NULL derives the reference once with the full info as a single
 * param and requires the chunks to reproduce it (no TC2 OKM is verified for
 * the other digests -- the same concatenation behavior, checked
 * relatively).  On OpenSSL releases that cap the number of info params
 * (HKDF_MAX_INFOS == 5, 3.1+), a sixth chunk must be rejected -- older
 * releases simply concatenate all of them, which is noted rather than
 * failed. */
void hkdf_test_multi_info(const struct hkdf_family *fam, const char *okmhex);

/* (5) Error paths: a zero output length and an EXTRACT_ONLY output of the
 * wrong (non-digest-size) length must fail. */
void hkdf_test_negative(const struct hkdf_family *fam);

/* (6) The separately registered concrete HKDF-SHA256/HKDF-SHA384/HKDF-SHA512
 * KDFs (3.5+; absent from 3.0.13, where each name takes a per-name mid-run
 * SKIP): each presets its digest, so it must derive with NO "digest" param
 * at all and agree byte for byte with the bare "HKDF" KDF plus the matching
 * digest param over the same inputs -- the primary assertion.  Each binary
 * passes its own digest's rows (the HKDF-SHA384 row lived with the SHA-256
 * binary until the test_kdf_hkdf_sha384 sibling was added). */
void hkdf_test_concrete_names(const struct hkdf_concrete *concretes,
                              size_t nconcretes);

#endif /* TEST_KDF_HKDF_COMMON_H */
