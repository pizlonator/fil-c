#include "test_common.h"

#include "test_digest_blake2_common.h"

#include <openssl/evp.h>

/*

 * test_digest_blake2b -- BLAKE2b-512 known-answer and behavior tests, one

 * binary per digest (the shared BLAKE2 machinery and the vector provenance

 * live in test_digest_blake2_common.c).

 *

 * Known-answer provenance (BLAKE2b-512): the constants below are the

 * official RFC 7693-style vectors for the unkeyed hash, verified with

 * pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -blake2b512

 *

 *   BLAKE2b-512("") = 786a02f7...   BLAKE2b-512("abc") = ba80a53f...

 *

 * Coverage for this digest: the empty-message and "abc" KATs through

 * EVP_MD_fetch with the 64-byte size check, the

 * EVP_get_digestbyname("blake2b512") route, and chunked-vs-one-shot

 * equality including updates that are exact multiples of the 128-byte

 * block size -- the implementation stashes a complete final block away

 * for the final call, so those splits must still agree.

 */

/* This digest's KAT rows: the empty message and "abc". */

static const struct blake2_kat kats[] = {

    { "BLAKE2B-512", 64,

      "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce",

      "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923" },

};

/* The byname route's update split: {1,2}-byte chunks. */

static const size_t BYNAME_CHUNKS[] = { 1, 2 };

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    blake2_test_kats(kats, sizeof(kats) / sizeof(kats[0]));

    /* kats[0].abchex is the "abc" vector used by the byname route. */

    blake2_test_byname_route("blake2b512", kats[0].abchex,

                             BYNAME_CHUNKS, sizeof(BYNAME_CHUNKS) / sizeof(BYNAME_CHUNKS[0]));

    blake2_test_block_boundary_chunks("BLAKE2B-512", 128);

    test_report_pass();

    return 0;

}

