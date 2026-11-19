#include "test_common.h"
#include "test_slh_dsa_common.h"

#include <openssl/evp.h>

/*
 * test_slh_dsa_sha2_128f -- SLH-DSA-SHA2-128f (FIPS 205, the "fast" 128-bit
 * SHA2 parameter set) tests: deterministic seed keygen, public-key known
 * answers, one-shot signing/verification of small and large (256 KiB)
 * messages, signature sizes, tamper rejection, and context-string binding
 * -- one binary per parameter set (the shared SLH-DSA machinery, the
 * determinism hooks, the spec adaptation note, the KAT provenance and the
 * version gate live in test_slh_dsa_common.c).
 *
 * Known-answer provenance (SLH-DSA-SHA2-128f), from the same scratch probe
 * as the sibling binaries: KAT_PUB_SHA2_128F_HEX is the raw public key
 * exported from the family-wide 48-byte (3n, n = 16) seed slh_seed_128_hex
 * (32 bytes, the FIPS 205 pk size).  The signature size (17088 bytes, the
 * FIPS 205 signature size for the f sets) is asserted via the one-shot
 * NULL-buffer EVP_DigestSign call in the shared routines, and run-to-run
 * signing determinism is asserted per parameter set.
 */

/* This parameter set's public-key KAT (the seed, the 16-byte KAT message
 * and the context string are family-wide and live in
 * test_slh_dsa_common.c). */
static const char KAT_PUB_SHA2_128F_HEX[] =
    "202122232425262728292a2b2c2d2e2f3b56e816847f000386aeec2e2bb9e1b5";

/* This parameter set's FIPS 205 row (the 128-bit sets share the 48-byte
 * seed). */
static const struct slh_dsa_set set = {
    "SLH-DSA-SHA2-128f", 48, 32, 17088,
    slh_seed_128_hex, KAT_PUB_SHA2_128F_HEX,
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* SLH-DSA is version-gated (OpenSSL 3.5+): if this parameter set is not
     * provided by this OpenSSL, take the SKIP path and exit 0. */
    slh_test_gate(&set);

    slh_test_seed_keygen_kat(&set);
    slh_test_sign_verify(&set);
    slh_test_context_string(&set);
    slh_test_large_message(&set);

    test_report_pass();
    return 0;
}