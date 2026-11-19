/* test_digest_sha2_common.h -- shared machinery for the SHA-2 family digest
 * tests: SHA2-224/256/384/512, the SHA2-512/224 and SHA2-512/256
 * truncations, and the version-gated SHA2-256/192.
 *
 * One binary per digest (test_digest_sha2_224.c, test_digest_sha2_256.c,
 * test_digest_sha2_384.c, test_digest_sha2_512.c,
 * test_digest_sha2_512_224.c, test_digest_sha2_512_256.c and
 * test_digest_sha2_256_192.c); each binary defines its own FIPS 180 KAT
 * rows (struct sha2_kat) and then calls the per-digest test routines below.
 * The big explanatory comment (the vector provenance and the SHA2-256/192
 * version note) lives in test_digest_sha2_common.c.
 */

#ifndef TEST_DIGEST_SHA2_COMMON_H
#define TEST_DIGEST_SHA2_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One KAT row: the digest name (the EVP_MD_fetch name, e.g.
 * "SHA2-512/224"), the message (one of the shared FIPS message constants
 * below), and the expected digest. */
struct sha2_kat {
    const char *mdname;
    const char *msg;
    const char *mdhex;
};

/* The classic FIPS messages (the "abc" one-block vector, the 56-byte
 * two-block-boundary vector, and the 112-byte two-block SHA-512 vector).
 * Declared as arrays, not pointers, so that they are address constants
 * usable in the static struct sha2_kat initializers of the per-digest
 * binaries. */
extern const char sha2_msg_abc[];
extern const char sha2_msg_56[];
extern const char sha2_msg_112[];

/* Whole-program version gate for SHA2-256/192 (no current OpenSSL 3.x
 * release ships it; newer builds do): SKIP with the documented reason
 * ("%s is unavailable") when the digest does not fetch, release the probe
 * handle otherwise. */
void sha2_probe_or_skip(const char *mdname);

/* (1) One-shot EVP_Digest with EVP_MD_fetch == KAT for every row; also
 * checks EVP_MD_get_size against the KAT digest length. */
void sha2_test_kats(const struct sha2_kat *kats, size_t nkats);

/* (2) + (3) The "abc" KAT (abchex; rows[0] of the binary's table) via the
 * legacy static route (`legacy`, spelled legacy_name in the CHECK
 * messages) and via EVP_get_digestbyname(byname); the byname route digests
 * chunked. */
void sha2_test_routes(const char *abchex, const EVP_MD *legacy,
                      const char *legacy_name, const char *byname);

/* (4) Chunked digesting must equal one-shot digesting for input lengths
 * around every block boundary (and one long input), for the digest fetched
 * by mdname. */
void sha2_test_chunked_equality(const char *mdname);

/* (5) EVP_MD_get_size for the digest named mdname must be `size`. */
void sha2_test_digest_size(const char *mdname, int size);

/* (6) The SHA2-256/192 fallback for builds that DO offer the digest: there
 * is no verified KAT for it here, so only the digest size (`size`) and the
 * chunked/one-shot self-consistency on the "abc" message are checked.
 * Called by test_digest_sha2_256_192.c after its whole-program gate. */
void sha2_test_256_192_selfcheck(const char *mdname, int size);

#endif /* TEST_DIGEST_SHA2_COMMON_H */
