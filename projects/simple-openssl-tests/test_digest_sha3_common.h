/* test_digest_sha3_common.h -- shared machinery for the SHA-3 and KECCAK
 * digest family tests: SHA3-224/256/384/512 and KECCAK-224/256/384/512.
 *
 * One binary per digest (test_digest_sha3_{224,256,384,512}.c and
 * test_digest_keccak_{224,256,384,512}.c); each binary defines one struct
 * sha3_kat row with its own KAT constants and then calls the per-digest
 * test routines below.  The big explanatory comment (the padding-byte
 * difference between SHA3-* and KECCAK-*, the vector provenance, and the
 * KECCAK version gate) lives in test_digest_sha3_common.c.
 */

#ifndef TEST_DIGEST_SHA3_COMMON_H
#define TEST_DIGEST_SHA3_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per digest: the digest name (the EVP_MD_fetch name), the digest
 * size in bytes (28/32/48/64), and the "abc" and empty-message KATs (SHA3-*
 * and KECCAK-* differ only in the domain-separation padding byte, 0x06 vs
 * 0x01, so every vector is distinct). */
struct sha3_kat {
    const char *mdname;
    int mdsize;
    const char *abchex;
    const char *emptyhex;
};

/* Whole-program version gate for the KECCAK-* binaries: KECCAK-* is absent
 * from OpenSSL 3.0.x's default provider (EVP_MD_fetch fails there) and
 * present in newer releases.  SKIP with the documented reason ("%s is
 * unavailable") when the digest does not fetch, release the probe handle
 * otherwise.  The SHA3-* binaries never gate: SHA3-* is core OpenSSL 3
 * functionality, so a failed fetch there is a FAIL, not a SKIP. */
void sha3_probe_or_skip(const char *mdname);

/* (1) One-shot EVP_Digest with EVP_MD_fetch == KAT for the row's two
 * messages ("abc" and the empty message); also checks EVP_MD_get_size
 * against the row's mdsize. */
void sha3_test_kat(const struct sha3_kat *kat);

/* (2) The KAT (abchex) via the EVP_get_digestbyname(byname) route, chunked
 * with one-byte updates for good measure.  The original ran this for
 * "sha3-256" only, and it belongs to test_digest_sha3_256.c. */
void sha3_test_byname_route(const char *byname, const char *abchex);

/* (3) Rate-boundary chunking for the digest named mdname with sponge rate
 * `rate` (the rates are 144/136/104/72 bytes for 224/256/384/512, and the
 * KECCAK rates equal the SHA3 rates at each output size): updates of
 * {rate-1, rate, rate+1} bytes (plus a byte-at-a-time pass) must reproduce
 * the one-shot digest of a 2*rate+2-byte deterministic input. */
void sha3_test_rate_boundaries(const char *mdname, size_t rate);

#endif /* TEST_DIGEST_SHA3_COMMON_H */
