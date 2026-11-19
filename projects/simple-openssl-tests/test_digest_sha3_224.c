#include "test_common.h"
#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha3_224 -- SHA3-224 known-answer and behavior tests, one
 * binary per digest (the shared SHA-3 machinery, the vector provenance, and
 * the KECCAK version note live in test_digest_sha3_common.c).
 *
 * Known-answer provenance (SHA3-224): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha3-224
 *   printf '' | pizfix/bin/openssl dgst -sha3-224
 */

/* This digest's row: the "abc" and empty-message KATs. */
static const struct sha3_kat kat = {
    "SHA3-224", 28,
    "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf",
    "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha3_test_kat(&kat);
    /* The rates are 144/136/104/72 bytes for 224/256/384/512. */
    sha3_test_rate_boundaries("SHA3-224", 144);

    test_report_pass();
    return 0;
}
