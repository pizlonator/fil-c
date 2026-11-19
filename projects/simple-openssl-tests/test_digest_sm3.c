#include "test_common.h"

#include "test_digest_ripemd_sm3_common.h"

#include <openssl/evp.h>

/*

 * test_digest_sm3 -- SM3 known-answer and behavior tests, one binary per

 * digest (the shared RIPEMD-160/SM3 machinery and the vector provenance

 * live in test_digest_ripemd_sm3_common.c).

 *

 * Known-answer provenance (SM3): the constant below was verified with

 * pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -sm3

 *

 *   SM3("abc") = 66c7f0f4...   (the GB/T 32905-2016 sample vector)

 *

 * SM3 is in the default provider in the OpenSSL 3.x builds exercised here

 * (no legacy load needed).  Coverage for this digest: the "abc" KAT

 * through EVP_MD_fetch with the 32-byte size check, the

 * EVP_get_digestbyname("sm3") route, and chunked-vs-one-shot equality

 * including exact 64-byte block multiples (the underlying compression

 * function is 64-byte Merkle-Damgard).

 */

/* This digest's KAT rows: "abc" (no empty-message vector is carried for

 * SM3 here). */

static const struct ripemd_sm3_kat kats[] = {

    { "SM3", 32,

      NULL,

      "66c7f0f462eeedd9d1f2d46bdc10e4e24167c4875cf2f7a2297da02b8f4ba8e0" },

};

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    ripemd_sm3_test_kats(kats, sizeof(kats) / sizeof(kats[0]));

    /* kats[0].abchex is the "abc" vector used by the byname route. */

    ripemd_sm3_test_byname_route("sm3", kats[0].abchex);

    ripemd_sm3_test_chunked_equality("SM3");

    test_report_pass();

    return 0;

}

