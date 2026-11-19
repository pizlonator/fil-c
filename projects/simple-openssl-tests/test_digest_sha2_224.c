#include "test_common.h"
#include "test_digest_sha2_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha2_224 -- SHA2-224 known-answer and behavior tests, one
 * binary per digest (the shared SHA-2 machinery and the vector provenance
 * live in test_digest_sha2_common.c).
 *
 * Known-answer provenance (SHA2-224): the constant below was verified
 * against the authoritative build with
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha224
 *
 * (the sibling binaries cover -sha256/-sha384/-sha512/-sha512-224/
 * -sha512-256 over the same FIPS message set).
 */

/* This digest's FIPS 180 rows: the "abc" one-block vector. */
static const struct sha2_kat kats[] = {
    { "SHA2-224", sha2_msg_abc,
      "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha2_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[0] is the "abc" vector used by the routes. */
    sha2_test_routes(kats[0].mdhex, EVP_sha224(), "EVP_sha224()", "sha224");
    sha2_test_chunked_equality("SHA2-224");
    sha2_test_digest_size("SHA2-224", 28);

    test_report_pass();
    return 0;
}
