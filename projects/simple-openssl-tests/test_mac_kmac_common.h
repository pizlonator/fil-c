/* test_mac_kmac_common.h -- shared machinery for the KMAC-128 and KMAC-256
 * MAC tests (EVP_MAC "KMAC-128" / "KMAC-256", NIST SP 800-185).
 *
 * One binary per MAC (test_mac_kmac128.c and test_mac_kmac256.c); each
 * binary defines one struct kmac_family row (its EVP_MAC name and the
 * provider default output size) and its own KAT rows, then calls the
 * per-MAC test routines below.  The big explanatory comment (the NIST
 * sample provenance and the "custom" OCTET_STRING portability note) lives
 * in test_mac_kmac_common.c.
 */

#ifndef TEST_MAC_KMAC_COMMON_H
#define TEST_MAC_KMAC_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One binary per MAC: the EVP_MAC fetch name and the provider default
 * output size (32 for KMAC-128, 64 for KMAC-256). */
struct kmac_family {
    const char *macname;
    size_t defsize;
};

/* One KAT row: the custom string (customhex == NULL: no custom parameter
 * at all; "" is the empty custom string), the XOF flag, the requested
 * output size, and the expected tag.  msghex documents the row's message
 * (NULL: the shared 200-byte 000102...c7 long sample message; otherwise
 * the 4-byte 00010203 short sample, which is the prefix of the same
 * pattern); the bytes always come from the shared 200-byte pattern
 * buffer, of which msglen bytes are used, exactly as the original
 * nine-sample table did.  `sample` keeps the row's number in that table,
 * for the failure messages. */
struct kmac_kat {
    size_t sample;
    const char *msghex;
    size_t msglen;
    const char *customhex;
    int xof;
    size_t size;
    const char *taghex;
};

/* Shared KAT material: the NIST sample key, the 32-byte 404142...5f
 * pattern (every row of both MACs uses it).  Declared as an array, not a
 * pointer, so that it is an address constant usable in the per-MAC
 * binaries. */
extern const char kmac_key32_hex[];

/* (1) The one-shot KATs of nkats rows (XOF rows included). */
void kmac_test_kats(const struct kmac_family *fam,
                    const struct kmac_kat *kats, size_t nkats);

/* (2) The absent-custom == empty-custom equivalence over the message of
 * the empty-custom KAT row empty_row (customhex == "" there), plus the
 * provider default output size (fam->defsize) read back after a plain
 * init. */
void kmac_test_absent_custom(const struct kmac_family *fam,
                             const struct kmac_kat *empty_row);

/* (3) Chunked updates: for each `a` in firsts, the message of KAT row kat
 * fed as updates of {a, rest} bytes must reproduce the row's one-shot
 * tag. */
void kmac_test_chunked(const struct kmac_family *fam,
                       const struct kmac_kat *kat,
                       const size_t *firsts, size_t nfirsts);

/* (4) The mac size tracks the requested output size `size`, and two
 * identical computations at that size agree (SP 800-185 encodes the
 * requested length in the KMAC input, so there is no truncation relation
 * to check -- only determinism and the length). */
void kmac_test_size_param(const struct kmac_family *fam, size_t size);

#endif /* TEST_MAC_KMAC_COMMON_H */
