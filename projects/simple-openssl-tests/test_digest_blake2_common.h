/* test_digest_blake2_common.h -- shared machinery for the BLAKE2b-512 and

 * BLAKE2s-256 digest tests.

 *

 * One binary per digest (test_digest_blake2b.c and test_digest_blake2s.c);

 * each binary defines its own KAT rows (struct blake2_kat) and calls the

 * per-digest test routines below.  The big explanatory comment (the RFC

 * 7693-style vector provenance and the final-block-stash note) lives in

 * test_digest_blake2_common.c.

 */

#ifndef TEST_DIGEST_BLAKE2_COMMON_H

#define TEST_DIGEST_BLAKE2_COMMON_H

#include "test_common.h"

#include <stddef.h>

#include <openssl/evp.h>

/* One KAT row: the digest name (the EVP_MD_fetch name), the digest size in

 * bytes, and the empty-message and "abc" KATs. */

struct blake2_kat {

    const char *mdname;

    int mdsize;

    const char *emptyhex;

    const char *abchex;

};

/* The shared "abc" message (the KAT message of both digests).  Declared as

 * an array, not a pointer, so that it is an address constant usable where

 * the per-digest binaries need it. */

extern const char blake2_msg_abc[];

/* (1) KATs through EVP_MD_fetch (one-shot), plus EVP_MD_get_size checks. */

void blake2_test_kats(const struct blake2_kat *kats, size_t nkats);

/* (2) The "abc" KAT (abchex) via the EVP_get_digestbyname(byname) route,

 * chunked with the `nchunks` `chunks` update lengths. */

void blake2_test_byname_route(const char *byname, const char *abchex,

                              const size_t *chunks, size_t nchunks);

/* (3) Block-boundary chunking for the digest named mdname with block size

 * `block` (128 for BLAKE2b, 64 for BLAKE2s) over a 512-byte deterministic

 * input: exact block multiples, block +/- 1, mixed odd chunks, and

 * byte-at-a-time must all reproduce the one-shot digest. */

void blake2_test_block_boundary_chunks(const char *mdname, size_t block);

#endif /* TEST_DIGEST_BLAKE2_COMMON_H */

