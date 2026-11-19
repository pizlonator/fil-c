#include "test_common.h"
#include "test_digest_sha2_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha2_512 -- SHA2-512 known-answer and behavior tests, one
 * binary per digest (the shared SHA-2 machinery and the vector provenance
 * live in test_digest_sha2_common.c).
 *
 * Known-answer provenance (SHA2-512): the constants below were verified
 * against the authoritative build with
 *
 *   printf '<message>' | pizfix/bin/openssl dgst -sha512
 *
 * over the three FIPS messages (the one-block "abc", the 56-byte
 * two-block-boundary message, and the 112-byte two-block SHA-512 message).
 */

/* This digest's FIPS 180 rows: "abc" (one block), the 56-byte
 * two-block-boundary message, and the 112-byte message. */
static const struct sha2_kat kats[] = {
    { "SHA2-512", sha2_msg_abc,
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
      "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f" },
    { "SHA2-512", sha2_msg_56,
      "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
      "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445" },
    { "SHA2-512", sha2_msg_112,
      "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
      "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha2_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[0] is the "abc" vector used by the routes. */
    sha2_test_routes(kats[0].mdhex, EVP_sha512(), "EVP_sha512()", "sha512");
    sha2_test_chunked_equality("SHA2-512");
    sha2_test_digest_size("SHA2-512", 64);

    test_report_pass();
    return 0;
}
