#include "test_common.h"

#include "test_digest_ripemd_sm3_common.h"

#include <openssl/evp.h>

/*

 * test_digest_ripemd160 -- RIPEMD-160 known-answer and behavior tests, one

 * binary per digest (the shared RIPEMD-160/SM3 machinery and the vector

 * provenance live in test_digest_ripemd_sm3_common.c).

 *

 * Known-answer provenance (RIPEMD-160): the constants below were verified

 * with pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -ripemd160

 *

 *   RIPEMD-160("") = 9c1185a5...   RIPEMD-160("abc") = 8eb208f7...

 *

 * RIPEMD160 is in the default provider in the OpenSSL 3.x builds exercised

 * here (no legacy load needed).  Coverage for this digest: the

 * empty-message and "abc" KATs through EVP_MD_fetch with the 20-byte size

 * check, the EVP_get_digestbyname("ripemd160") route, and chunked-vs-one-

 * shot equality including exact 64-byte block multiples (the underlying

 * compression function is 64-byte Merkle-Damgard).

 */

/* This digest's KAT rows: the empty message and "abc". */

static const struct ripemd_sm3_kat kats[] = {

    { "RIPEMD160", 20,

      "9c1185a5c5e9fc54612808977ee8f548b2258d31",

      "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc" },

};

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    ripemd_sm3_test_kats(kats, sizeof(kats) / sizeof(kats[0]));

    /* kats[0].abchex is the "abc" vector used by the byname route. */

    ripemd_sm3_test_byname_route("ripemd160", kats[0].abchex);

    ripemd_sm3_test_chunked_equality("RIPEMD160");

    test_report_pass();

    return 0;

}

