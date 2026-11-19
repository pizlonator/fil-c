#include "test_common.h"
#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha3_256 -- SHA3-256 known-answer and behavior tests, one
 * binary per digest (the shared SHA-3 machinery, the vector provenance, and
 * the KECCAK version note live in test_digest_sha3_common.c).
 *
 * Known-answer provenance (SHA3-256): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -sha3-256
 *   printf '' | pizfix/bin/openssl dgst -sha3-256
 */

/* This digest's row: the "abc" and empty-message KATs. */
static const struct sha3_kat kat = {
    "SHA3-256", 32,
    "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532",
    "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    sha3_test_kat(&kat);
    /* (2) The KAT via the EVP_get_digestbyname("sha3-256") route: the
     * original ran the byname route for sha3-256 only, and it stays here. */
    sha3_test_byname_route("sha3-256", kat.abchex);
    /* The rates are 144/136/104/72 bytes for 224/256/384/512. */
    sha3_test_rate_boundaries("SHA3-256", 136);

    test_report_pass();
    return 0;
}
