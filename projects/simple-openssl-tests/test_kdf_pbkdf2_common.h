/* test_kdf_pbkdf2_common.h -- shared machinery for the PBKDF2 family tests
 * (EVP_KDF "PBKDF2", RFC 6070 / SP 800-132).
 *
 * One binary per digest (test_kdf_pbkdf2_sha1.c and test_kdf_pbkdf2_sha256.c);
 * each binary defines one struct pbkdf2_family row (its OSSL_KDF_PARAM_DIGEST
 * value) and its own KAT rows, then calls the per-digest test routines
 * below.  The big explanatory comment (the RFC 6070 vector provenance, the
 * CLI verification commands and the not-run c=16777216 note) lives in
 * test_kdf_pbkdf2_common.c.
 */

#ifndef TEST_KDF_PBKDF2_COMMON_H
#define TEST_KDF_PBKDF2_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per digest: the OSSL_KDF_PARAM_DIGEST value passed to the
 * "PBKDF2" KDF. */
struct pbkdf2_family {
    const char *digest;
};

/* One KAT row: the password, the salt, the iteration count, the requested
 * dkLen and the expected derived key. */
struct pbkdf2_kat {
    const char *passhex;
    const char *salthex;
    unsigned long iter;
    size_t dklen;
    const char *dkhex;
};

/* Shared KAT material: the RFC 6070 passwords and salts ("password" 8
 * bytes, "passwordPASSWORDpassword" 24 bytes, "salt" 4 bytes,
 * "saltSALTsaltSALTsaltSALTsaltSALTsalt" 36 bytes) and the 16- and 15-byte
 * pattern salts the pkcs5 bounds section feeds.  Declared as arrays, not
 * pointers, so that they are address constants usable in the per-digest
 * binaries' static struct initializers. */
extern const char pbkdf2_pass_hex[];
extern const char pbkdf2_long_pass_hex[];
extern const char pbkdf2_salt_hex[];
extern const char pbkdf2_long_salt_hex[];
extern const char pbkdf2_salt16_hex[];
extern const char pbkdf2_salt15_hex[];

/* One PBKDF2 derive with a fresh context.  Returns 1 on success, 0 if
 * EVP_KDF_derive failed.  PBKDF2 is core OpenSSL 3 functionality, so a
 * fetch failure fails the program (never SKIP). */
int pbkdf2_derive(const char *digest, unsigned char *pass, size_t passlen,
                  unsigned char *salt, size_t saltlen, unsigned long iter,
                  int pkcs5, unsigned char *out, size_t outlen);

/* (1) Every KAT row of the calling binary's digest through the default
 * (pkcs5=1, SP 800-132 checks off) path. */
void pbkdf2_test_kats(const struct pbkdf2_family *fam,
                      const struct pbkdf2_kat *kats, size_t nkats);

/* (2) The SP 800-132 lower bounds behind "pkcs5"=0: iteration count >=
 * 1000, salt >= 128 bits, derived key >= 112 bits.  With pkcs5=1 (the
 * default) the same weak inputs are accepted.  strong_hex != NULL
 * additionally compares the compliant pkcs5=0 derive against that
 * CLI-verified value (the SHA-256 binary's 16-byte 25eb86ac...); weak_hex
 * != NULL additionally compares the pkcs5=1 c=1 derive against that
 * 16-byte prefix of the digest's c=1 KAT (the SHA-256 binary's well-known
 * 120fb6cf... prefix, and the first 16 bytes of the SHA-1 RFC 6070 c=1 KAT
 * -- the same construction the monolith used for SHA-256); a NULL skips
 * the value comparison where no constant was verified, keeping the
 * accept/reject checks. */
void pbkdf2_test_pkcs5_bounds(const struct pbkdf2_family *fam,
                              const char *strong_hex, const char *weak_hex);

#endif /* TEST_KDF_PBKDF2_COMMON_H */