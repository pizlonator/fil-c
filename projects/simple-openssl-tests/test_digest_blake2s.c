#include "test_common.h"

#include "test_digest_blake2_common.h"

#include <openssl/evp.h>

/*

 * test_digest_blake2s -- BLAKE2s-256 known-answer and behavior tests, one

 * binary per digest (the shared BLAKE2 machinery and the vector provenance

 * live in test_digest_blake2_common.c).

 *

 * Known-answer provenance (BLAKE2s-256): the constants below are the

 * official RFC 7693-style vectors for the unkeyed hash, verified with

 * pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -blake2s256

 *

 *   BLAKE2s-256("") = 69217a30...   BLAKE2s-256("abc") = 508c5e8c...

 *

 * Coverage for this digest: the empty-message and "abc" KATs through

 * EVP_MD_fetch with the 32-byte size check, the

 * EVP_get_digestbyname("blake2s256") route, and chunked-vs-one-shot

 * equality including updates that are exact multiples of the 64-byte

 * block size -- the implementation stashes a complete final block away

 * for the final call, so those splits must still agree.

 */

/* This digest's KAT rows: the empty message and "abc". */

static const struct blake2_kat kats[] = {

    { "BLAKE2S-256", 32,

      "69217a3079908094e11121d042354a7c1f55b6482ca1a51e1b250dfd1ed0eef9",

      "508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982" },

};

/* The byname route's update split: {2,1}-byte chunks. */

static const size_t BYNAME_CHUNKS[] = { 2, 1 };

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    blake2_test_kats(kats, sizeof(kats) / sizeof(kats[0]));

    /* kats[0].abchex is the "abc" vector used by the byname route. */

    blake2_test_byname_route("blake2s256", kats[0].abchex,

                             BYNAME_CHUNKS, sizeof(BYNAME_CHUNKS) / sizeof(BYNAME_CHUNKS[0]));

    blake2_test_block_boundary_chunks("BLAKE2S-256", 64);

    test_report_pass();

    return 0;

}

