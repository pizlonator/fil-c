#include "test_common.h"
#include "test_slh_dsa_common.h"

#include <openssl/evp.h>

/*
 * test_slh_dsa_sha2_192f -- SLH-DSA-SHA2-192f (FIPS 205, the "fast" 192-bit
 * SHA2 parameter set) tests: deterministic seed keygen, public-key known
 * answers, one-shot signing/verification of small and large (256 KiB)
 * messages, signature sizes, tamper rejection, and context-string binding
 * -- one binary per parameter set (the shared SLH-DSA machinery, the
 * determinism hooks, the spec adaptation note, the KAT provenance and the
 * version gate live in test_slh_dsa_common.c).
 *
 * Known-answer provenance (SLH-DSA-SHA2-192f), from the same scratch probe
 * as the sibling binaries: KAT_PUB_SHA2_192F_HEX is the raw public key
 * exported from the family-wide 72-byte (3n, n = 24) seed slh_seed_192_hex
 * (48 bytes, the FIPS 205 pk size).  The signature size (35664 bytes, the
 * FIPS 205 signature size for the f sets) is asserted via the one-shot
 * NULL-buffer EVP_DigestSign call in the shared routines, and run-to-run
 * signing determinism is asserted per parameter set.
 */

/* This parameter set's public-key KAT (the seed, the 16-byte KAT message
 * and the context string are family-wide and live in
 * test_slh_dsa_common.c). */
static const char KAT_PUB_SHA2_192F_HEX[] =
    "303132333435363738393a3b3c3d3e3f40414243444546479236ccebbb3a90ac"
    "2452dd89de49dab1340ec02419a2870e";

/* This parameter set's FIPS 205 row (the 192-bit set uses the 72-byte
 * seed). */
static const struct slh_dsa_set set = {
    "SLH-DSA-SHA2-192f", 72, 48, 35664,
    slh_seed_192_hex, KAT_PUB_SHA2_192F_HEX,
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