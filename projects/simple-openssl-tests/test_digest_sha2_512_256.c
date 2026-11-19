#include "test_common.h"
#include "test_digest_sha2_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha2_512_256 -- the SHA2-512/256 truncation, one binary per
 * digest (the shared SHA-2 machinery and the vector provenance live in
 * test_digest_sha2_common.c).
 *
 * Known-answer provenance (SHA2-512/256): the constant below was verified
 * against the authoritative build with
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha512-256
 *
 * (the CLI spells the digest sha512-256; the EVP name is SHA2-512/256).
 */

/* This digest's FIPS 180 rows: the "abc" one-block vector. */
static const struct sha2_kat kats[] = {
    { "SHA2-512/256", sha2_msg_abc,
      "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha2_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[0] is the "abc" vector used by the routes. */
    sha2_test_routes(kats[0].mdhex, EVP_sha512_256(), "EVP_sha512_256()",
                     "sha512-256");
    sha2_test_chunked_equality("SHA2-512/256");
    sha2_test_digest_size("SHA2-512/256", 32);

    test_report_pass();
    return 0;
}
