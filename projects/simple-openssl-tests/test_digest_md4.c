#include "test_common.h"

#include "test_digest_md5_common.h"

#include <openssl/evp.h>

/*

 * test_digest_md4 -- MD4 (legacy provider) known-answer and behavior tests,

 * one binary per digest (the shared MD5/MD4/MD5-SHA1 machinery, the vector

 * provenance, and the legacy-provider situation live in

 * test_digest_md5_common.c).

 *

 * Known-answer provenance (MD4): the constant below was verified with

 * pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -provider legacy \

 *       -provider default -md4

 *

 *   MD4("abc") = a448017a...

 *

 * MD4 lives in the legacy provider, which md5_load_providers() loads

 * explicitly (the default provider hosts MD5 and MD5-SHA1).  Coverage for

 * this digest: the "abc" KAT through EVP_MD_fetch, the static EVP_md4()

 * route and the EVP_get_digestbyname("md4") route, and chunked-vs-one-shot

 * equality around the 64-byte block boundary.

 */

/* This digest's KAT rows: "abc". */

static const struct md5_kat kats[] = {

    { "MD4", md5_msg_abc,

      "a448017aaf21d8525fc10ae87aa6729d" },

};

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    md5_load_providers();

    md5_test_kats(kats, sizeof(kats) / sizeof(kats[0]));

    /* kats[0] is the "abc" vector used by the routes. */

    md5_test_routes(kats[0].mdhex, EVP_md4(), "EVP_md4()", "md4");

    md5_test_chunked_equality("MD4");

    test_report_pass();

    return 0;

}

