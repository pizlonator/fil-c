#include "test_common.h"
#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha3_384 -- SHA3-384 known-answer and behavior tests, one
 * binary per digest (the shared SHA-3 machinery, the vector provenance, and
 * the KECCAK version note live in test_digest_sha3_common.c).
 *
 * Known-answer provenance (SHA3-384): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha3-384
 *   printf '' | pizfix/bin/openssl dgst -sha3-384
 */

/* This digest's row: the "abc" and empty-message KATs. */
static const struct sha3_kat kat = {
    "SHA3-384", 48,
    "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25",
    "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha3_test_kat(&kat);
    /* The rates are 144/136/104/72 bytes for 224/256/384/512. */
    sha3_test_rate_boundaries("SHA3-384", 104);

    test_report_pass();
    return 0;
}
