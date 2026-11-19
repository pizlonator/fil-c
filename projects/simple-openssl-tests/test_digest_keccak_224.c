#include "test_common.h"
#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/*
 * test_digest_keccak_224 -- KECCAK-224 known-answer and behavior tests, one
 * binary per digest (the shared SHA-3 machinery, the vector provenance, and
 * the KECCAK version note live in test_digest_sha3_common.c).
 *
 * Known-answer provenance (KECCAK-224): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-224
 *   printf '' | pizfix/bin/openssl dgst -keccak-224
 *
 * KECCAK-* differs from SHA3-* only in the domain-separation padding byte
 * (0x01 vs 0x06), so every vector is distinct from the SHA3-224 row.
 */

/* This digest's row: the "abc" and empty-message KATs. */
static const struct sha3_kat kat = {
    "KECCAK-224", 28,
    "c30411768506ebe1c2871b1ee2e87d38df342317300a9b97a95ec6a8",
    "f71837502ba8e10837bdd8d365adb85591895602fc552b48b7390abd",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* Whole-program version gate: KECCAK-* is absent from OpenSSL 3.0.x's
     * default provider (EVP_MD_fetch fails there) and present in newer
     * releases, so the binary skips when the digest does not fetch. */
    sha3_probe_or_skip("KECCAK-224");

    sha3_test_kat(&kat);
    /* The rates are 144/136/104/72 bytes for 224/256/384/512; the KECCAK
     * rates equal the SHA3 rates at each output size. */
    sha3_test_rate_boundaries("KECCAK-224", 144);

    test_report_pass();
    return 0;
}
