#include "test_common.h"
#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/*
 * test_digest_keccak_256 -- KECCAK-256 known-answer and behavior tests, one
 * binary per digest (the shared SHA-3 machinery, the vector provenance, and
 * the KECCAK version note live in test_digest_sha3_common.c).
 *
 * Known-answer provenance (KECCAK-256): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-256
 *   printf '' | pizfix/bin/openssl dgst -keccak-256
 *
 * KECCAK-* differs from SHA3-* only in the domain-separation padding byte
 * (0x01 vs 0x06), so every vector is distinct from the SHA3-256 row.
 */

/* This digest's row: the "abc" and empty-message KATs. */
static const struct sha3_kat kat = {
    "KECCAK-256", 32,
    "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45",
    "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* Whole-program version gate: KECCAK-* is absent from OpenSSL 3.0.x's
     * default provider (EVP_MD_fetch fails there) and present in newer
     * releases, so the binary skips when the digest does not fetch. */
    sha3_probe_or_skip("KECCAK-256");

    sha3_test_kat(&kat);
    /* The rates are 144/136/104/72 bytes for 224/256/384/512; the KECCAK
     * rates equal the SHA3 rates at each output size. */
    sha3_test_rate_boundaries("KECCAK-256", 136);

    test_report_pass();
    return 0;
}
