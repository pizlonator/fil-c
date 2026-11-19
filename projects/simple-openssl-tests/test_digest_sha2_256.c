#include "test_common.h"
#include "test_digest_sha2_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha2_256 -- SHA2-256 known-answer and behavior tests, one
 * binary per digest (the shared SHA-2 machinery and the vector provenance
 * live in test_digest_sha2_common.c).
 *
 * Known-answer provenance (SHA2-256): the constants below were verified
 * against the authoritative build with
 *
 *   printf '<message>' | pizfix/bin/openssl dgst -sha256
 *
 * over the three FIPS messages (the one-block "abc", the 56-byte
 * two-block-boundary message, and the 112-byte message).
 */

/* This digest's FIPS 180 rows: "abc" (one block), the 56-byte
 * two-block-boundary message, and the 112-byte message. */
static const struct sha2_kat kats[] = {
    { "SHA2-256", sha2_msg_abc,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
    { "SHA2-256", sha2_msg_56,
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
    { "SHA2-256", sha2_msg_112,
      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha2_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    /* kats[0] is the "abc" vector used by the routes. */
    sha2_test_routes(kats[0].mdhex, EVP_sha256(), "EVP_sha256()", "sha256");
    sha2_test_chunked_equality("SHA2-256");
    sha2_test_digest_size("SHA2-256", 32);

    test_report_pass();
    return 0;
}
