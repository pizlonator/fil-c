/* test_digest_ripemd_sm3_common.h -- shared machinery for the RIPEMD-160

 * and SM3 digest tests.

 *

 * One binary per digest (test_digest_ripemd160.c and test_digest_sm3.c);

 * each binary defines its own KAT rows (struct ripemd_sm3_kat) and calls

 * the per-digest test routines below.  The big explanatory comment (the

 * vector provenance and the Merkle-Damgard block-boundary note) lives in

 * test_digest_ripemd_sm3_common.c.

 */

#ifndef TEST_DIGEST_RIPEMD_SM3_COMMON_H

#define TEST_DIGEST_RIPEMD_SM3_COMMON_H

#include "test_common.h"

#include <stddef.h>

#include <openssl/evp.h>

/* One KAT row: the digest name (the EVP_MD_fetch name), the digest size in

 * bytes, the empty-message KAT (NULL when no empty-message vector is

 * carried for this digest), and the "abc" KAT. */

struct ripemd_sm3_kat {

    const char *mdname;

    int mdsize;

    const char *emptyhex;

    const char *abchex;

};

/* The shared "abc" message (the KAT message of both digests).  Declared as

 * an array, not a pointer, so that it is an address constant usable where

 * the per-digest binaries need it. */

extern const char ripemd_sm3_msg_abc[];

/* (1) KATs through EVP_MD_fetch (one-shot), plus EVP_MD_get_size checks. */

void ripemd_sm3_test_kats(const struct ripemd_sm3_kat *kats, size_t nkats);

/* (2) The "abc" KAT (abchex) via the EVP_get_digestbyname(byname) route,

 * chunked. */

void ripemd_sm3_test_byname_route(const char *byname, const char *abchex);

/* (3) Chunked-vs-one-shot equality for the digest named mdname around the

 * 64-byte block boundary and at exact block multiples (block-stashed-at-

 * final behavior must not change the result): the sizes sweep, plus 256

 * bytes fed as exact 64-byte updates. */

void ripemd_sm3_test_chunked_equality(const char *mdname);

#endif /* TEST_DIGEST_RIPEMD_SM3_COMMON_H */

