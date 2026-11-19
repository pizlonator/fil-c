#include "test_common.h"

#include "test_digest_md5_common.h"

#include <openssl/evp.h>

/*

 * test_digest_md5 -- MD5 known-answer and behavior tests, one binary per

 * digest (the shared MD5/MD4/MD5-SHA1 machinery, the vector provenance, and

 * the legacy-provider situation live in test_digest_md5_common.c).

 *

 * Known-answer provenance (MD5): the constants below were verified with

 * pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -md5

 *

 *   MD5("")  = d41d8cd9...   MD5("abc") = 90015098...

 *

 * Coverage for this digest: the empty-message and "abc" KATs through

 * EVP_MD_fetch, the static EVP_md5() route and the

 * EVP_get_digestbyname("md5") route, and chunked-vs-one-shot equality

 * around the 64-byte block boundary.

 */

/* This digest's KAT rows: the empty message and "abc". */

static const struct md5_kat kats[] = {

    { "MD5", NULL, /* the empty message */

      "d41d8cd98f00b204e9800998ecf8427e" },

    { "MD5", md5_msg_abc,

      "900150983cd24fb0d6963f7d28e17f72" },

};

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    md5_load_providers();

    md5_test_kats(kats, sizeof(kats) / sizeof(kats[0]));

    /* kats[1] is the "abc" vector used by the routes. */

    md5_test_routes(kats[1].mdhex, EVP_md5(), "EVP_md5()", "md5");

    md5_test_chunked_equality("MD5");

    test_report_pass();

    return 0;

}

