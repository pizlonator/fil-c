/* test_mac_hmac_common.h -- shared machinery for the HMAC family tests
 * (EVP_MAC "HMAC" over MD5, SHA1, SHA256, SHA384 and SHA512).
 *
 * One binary per digest (test_mac_hmac_md5.c, test_mac_hmac_sha1.c,
 * test_mac_hmac_sha256.c, test_mac_hmac_sha384.c and test_mac_hmac_sha512.c);
 * each binary defines one struct hmac_family row (its "digest" parameter and
 * the digest size) and its own KAT rows, then calls the per-digest test
 * routines below.  The big explanatory comment (the two-way vector
 * verification provenance and the per-section distribution) lives in
 * test_mac_hmac_common.c.
 */

#ifndef TEST_MAC_HMAC_COMMON_H
#define TEST_MAC_HMAC_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One binary per digest: the "digest" parameter under test and the digest
 * size (== the tag length and the value EVP_MAC_CTX_get_mac_size must
 * return). */
struct hmac_family {
    const char *digestname;
    size_t digestsize;
};

/* One KAT row: the RFC test-case number (kept for the failure messages; the
 * key/data material of each case is shared and selected by the common
 * routines) and this digest's expected tag. */
struct hmac_kat {
    size_t tc;
    const char *taghex;
};

/* (0) EVP_MAC_CTX_get_mac_size == the digest size, probed after a plain
 * init with a 20-byte 0x0b key (the size is only meaningful once the
 * context has been initialized). */
void hmac_test_mac_size(const struct hmac_family *fam);

/* (1) RFC 4231 test cases 1-4, 6 and 7 for fam's digest: one-shot == KAT
 * and a chunked {1, 17} pass == one-shot, for each of nkats rows. */
void hmac_test_rfc4231(const struct hmac_family *fam,
                       const struct hmac_kat *kats, size_t nkats);

/* (2) RFC 2202 test cases 1-4, 6 and 7 for fam's digest: one-shot == KAT
 * (basekeylen is the TC1/TC3/TC4 key length: 20 for SHA1, 16 for MD5),
 * plus the chunked {1, 17} pass == one-shot for each row (added by the
 * split; the monolith ran chunked updates for the RFC 4231 digests only). */
void hmac_test_rfc2202(const struct hmac_family *fam,
                       const struct hmac_kat *kats, size_t nkats,
                       size_t basekeylen);

/* (3) Key sizes: the empty key (zero length, non-NULL buffer) must work and
 * must equal the 32-zero-byte and 64-zero-byte keys (HMAC zero-pads the key
 * to the block size).  emptykat_hex anchors the empty-key result to a KAT
 * (the SHA256 binary carries the constant the monolith verified); NULL
 * anchors the equivalence on the empty-key result itself (no published
 * empty-key vector). */
void hmac_test_key_sizes(const struct hmac_family *fam, const char *emptykat_hex);

/* (4) A nonexistent digest name must fail cleanly: either the parameter set
 * or the init rejects it, never a silent success.  (Independent of the
 * digest under test; every binary runs it.) */
void hmac_test_wrong_digest(void);

#endif /* TEST_MAC_HMAC_COMMON_H */
