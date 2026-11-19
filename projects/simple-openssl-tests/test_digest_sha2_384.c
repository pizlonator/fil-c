#include "test_common.h"
#include "test_digest_sha2_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha2_384 -- SHA2-384 known-answer and behavior tests, one
 * binary per digest (the shared SHA-2 machinery and the vector provenance
 * live in test_digest_sha2_common.c).
 *
 * Known-answer provenance (SHA2-384): the constant below was verified
 * against the authoritative build with
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha384
 */

/* This digest's FIPS 180 rows: the "abc" one-block vector. */
static const struct sha2_kat kats[] = {
    { "SHA2-384", sha2_msg_abc,
      "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
      "8086072ba1e7cc2358baeca134c825a7" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha2_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[0] is the "abc" vector used by the routes. */
    sha2_test_routes(kats[0].mdhex, EVP_sha384(), "EVP_sha384()", "sha384");
    sha2_test_chunked_equality("SHA2-384");
    sha2_test_digest_size("SHA2-384", 48);

    test_report_pass();
    return 0;
}
