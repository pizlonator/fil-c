#include "test_common.h"
#include "test_digest_sha2_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha2_512_224 -- the SHA2-512/224 truncation, one binary per
 * digest (the shared SHA-2 machinery and the vector provenance live in
 * test_digest_sha2_common.c).
 *
 * Known-answer provenance (SHA2-512/224): the constant below was verified
 * against the authoritative build with
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha512-224
 *
 * (the CLI spells the digest sha512-224; the EVP name is SHA2-512/224).
 */

/* This digest's FIPS 180 rows: the "abc" one-block vector. */
static const struct sha2_kat kats[] = {
    { "SHA2-512/224", sha2_msg_abc,
      "4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha2_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[0] is the "abc" vector used by the routes. */
    sha2_test_routes(kats[0].mdhex, EVP_sha512_224(), "EVP_sha512_224()",
                     "sha512-224");
    sha2_test_chunked_equality("SHA2-512/224");
    sha2_test_digest_size("SHA2-512/224", 28);

    test_report_pass();
    return 0;
}
