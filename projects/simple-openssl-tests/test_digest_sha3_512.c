#include "test_common.h"
#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha3_512 -- SHA3-512 known-answer and behavior tests, one
 * binary per digest (the shared SHA-3 machinery, the vector provenance, and
 * the KECCAK version note live in test_digest_sha3_common.c).
 *
 * Known-answer provenance (SHA3-512): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha3-512
 *   printf '' | pizfix/bin/openssl dgst -sha3-512
 */

/* This digest's row: the "abc" and empty-message KATs. */
static const struct sha3_kat kat = {
    "SHA3-512", 64,
    "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0",
    "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha3_test_kat(&kat);
    /* The rates are 144/136/104/72 bytes for 224/256/384/512. */
    sha3_test_rate_boundaries("SHA3-512", 72);

    test_report_pass();
    return 0;
}
