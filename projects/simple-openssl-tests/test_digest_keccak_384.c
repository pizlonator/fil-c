#include "test_common.h"
#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/*
 * test_digest_keccak_384 -- KECCAK-384 known-answer and behavior tests, one
 * binary per digest (the shared SHA-3 machinery, the vector provenance, and
 * the KECCAK version note live in test_digest_sha3_common.c).
 *
 * Known-answer provenance (KECCAK-384): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-384
 *   printf '' | pizfix/bin/openssl dgst -keccak-384
 *
 * KECCAK-* differs from SHA3-* only in the domain-separation padding byte
 * (0x01 vs 0x06), so every vector is distinct from the SHA3-384 row.
 */

/* This digest's row: the "abc" and empty-message KATs. */
static const struct sha3_kat kat = {
    "KECCAK-384", 48,
    "f7df1165f033337be098e7d288ad6a2f74409d7a60b49c36642218de161b1f99f8c681e4afaf31a34db29fb763e3c28e",
    "2c23146a63a29acf99e73b88f8c24eaa7dc60aa771780ccc006afbfa8fe2479b2dd2b21362337441ac12b515911957ff",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* Whole-program version gate: KECCAK-* is absent from OpenSSL 3.0.x's
     * default provider (EVP_MD_fetch fails there) and present in newer
     * releases, so the binary skips when the digest does not fetch. */
    sha3_probe_or_skip("KECCAK-384");

    sha3_test_kat(&kat);
    /* The rates are 144/136/104/72 bytes for 224/256/384/512; the KECCAK
     * rates equal the SHA3 rates at each output size. */
    sha3_test_rate_boundaries("KECCAK-384", 104);

    test_report_pass();
    return 0;
}
