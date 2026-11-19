/* test_mac_blake2_common.h -- shared machinery for the keyed BLAKE2 MAC
 * tests (EVP_MAC "BLAKE2BMAC" / "BLAKE2SMAC").
 *
 * One binary per MAC (test_mac_blake2b.c and test_mac_blake2s.c); each
 * binary defines one struct b2mac_family row (its EVP_MAC name, its key
 * lengths and output sizes) and its own KAT rows, then calls the per-MAC
 * test routines below.  The big explanatory comment (the vector
 * provenance and the section coverage) lives in test_mac_blake2_common.c.
 */

#ifndef TEST_MAC_BLAKE2_COMMON_H
#define TEST_MAC_BLAKE2_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One KAT row: the key hex (one of the shared 000102... key patterns
 * below), the requested output size (0: the provider default), the
 * default size (for the failure message), and the expected tag. */
struct b2mac_kat {
    const char *keyhex;
    size_t size;
    size_t defsize;
    const char *taghex;
};

/* One binary per MAC: the EVP_MAC fetch name, the MAC's maximum key (the
 * shared 000102... pattern of maxkeylen bytes -- 64 for BLAKE2BMAC, 32
 * for BLAKE2SMAC), the over-long key length that must be rejected
 * (maxkeylen + 1), the provider default output size, and the unsupported
 * output size that must be rejected (defsize + 1). */
struct b2mac_family {
    const char *macname;
    const char *keyhex;
    size_t maxkeylen;
    size_t overlong_len;
    size_t defsize;
    size_t badsize;
};

/* Shared KAT material: the 000102... key patterns (64/32/16 bytes) and
 * the "abc" KAT message.  Declared as arrays, not pointers, so that they
 * are address constants usable in the static struct b2mac_kat and
 * struct b2mac_family initializers of the per-MAC binaries. */
extern const char b2mac_key64_hex[];
extern const char b2mac_key32_hex[];
extern const char b2mac_key16_hex[];
extern const char b2mac_msg_abc[];

/* (1) The one-shot KATs of nkats rows, plus an EVP_MAC_CTX_get_mac_size
 * cross-check at the requested output size sizetrack (queried after
 * init, where it is meaningful). */
void b2mac_test_kats(const struct b2mac_family *fam,
                     const struct b2mac_kat *kats, size_t nkats,
                     size_t sizetrack);

/* (2) Chunked updates over a 200-byte deterministic message must equal
 * the one-shot tag, at the provider default output size. */
void b2mac_test_chunked(const struct b2mac_family *fam,
                        const size_t *chunks, size_t nchunks);

/* (3) Negative tests for fam: a zero-length key, the over-long key
 * (fam->overlong_len bytes) and the unsupported output size
 * (fam->badsize) must all fail cleanly. */
void b2mac_test_negative(const struct b2mac_family *fam);

#endif /* TEST_MAC_BLAKE2_COMMON_H */
