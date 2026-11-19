#include "test_common.h"
#include "test_aes_gcm_siv_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_gcm_siv -- RFC 8452 AES-192-GCM-SIV tests, one binary per
 * cipher (the shared GCM-SIV machinery, the version gate, and the vector
 * provenance live in test_aes_gcm_siv_common.c).
 *
 * Known-answer provenance (AES-192): RFC 8452 defines the algorithm only
 * for 128- and 256-bit keys and publishes no AES-192 vectors.  The key
 * below continues the RFC key convention (01 followed by zero bytes) to 24
 * bytes and the two "192 (derived)" rows were derived with a scratch probe
 * against the authoritative pizfix OpenSSL 3.6.5 (via build/bin/filcc);
 * there is nothing to cross-check on 3.0.13, which lacks the algorithm
 * entirely.
 */

/* This file's own extension of the RFC key convention to 192 bits. */
static const char *const KEY192_GSIV_HEX =
    "010000000000000000000000000000000000000000000000";

/* The derived AES-192 rows (see above). */
static const struct gsiv_kat kats[] = {
    { "192 4B+12AAD (derived)", "AES-192-GCM-SIV", KEY192_GSIV_HEX,
      "010000000000000000000000",
      "02000000",
      "7236a9d5",
      "9fca93a4e49ce098ad646bf0f408bade" },
    { "192 16B (derived)", "AES-192-GCM-SIV", KEY192_GSIV_HEX,
      "",
      "01000000000000000000000000000000",
      "75a96f1f1cbfa93e2cd69e8a18bf3bab",
      "82e6a81be803dc33f56a637fcaa70fec" },
};

int main(int argc, char **argv)
{
    EVP_CIPHER *probe;

    test_parse_args(argc, argv);

    /* AES-GCM-SIV is version-gated (added in OpenSSL 3.5): on older builds
     * the fetch fails and the whole program skips. */
    probe = EVP_CIPHER_fetch(NULL, "AES-192-GCM-SIV", NULL);
    if (probe == NULL)
        test_skip("AES-192-GCM-SIV is unavailable");
    EVP_CIPHER_free(probe);

    gsiv_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    gsiv_test_aad_chunking(&kats[0]);
    gsiv_test_roundtrip_sizes(&kats[0]);
    gsiv_test_second_update_fails(&kats[0]);
    gsiv_test_tamper(&kats[0]);

    test_report_pass();
    return 0;
}
