#include "test_common.h"
#include "test_aes_gcm_siv_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_gcm_siv -- RFC 8452 AES-128-GCM-SIV tests, one binary per
 * cipher (the shared GCM-SIV machinery, the version gate, and the vector
 * provenance live in test_aes_gcm_siv_common.c).  This binary carries the
 * four RFC 8452 Appendix C.1 AES-128 rows; the nonce is the fixed 12-byte
 * RFC nonce and the key follows the RFC convention (01 followed by zero
 * bytes).
 */

/* RFC 8452 Appendix C.1 vectors (Key = 01 00..00 pattern of the key
 * length). */
static const struct gsiv_kat kats[] = {
    { "C.1 #1", "AES-128-GCM-SIV",
      "01000000000000000000000000000000",
      "", "", "",
      "DC20E2D83F25705BB49E439ECA56DE25" },
    { "C.1 #4", "AES-128-GCM-SIV",
      "01000000000000000000000000000000",
      "",
      "01000000000000000000000000000000",
      "743F7C8077AB25F8624E2E948579CF77",
      "303AAF90F6FE21199C6068577437A0C4" },
    { "C.1 4B+12AAD", "AES-128-GCM-SIV",
      "01000000000000000000000000000000",
      "010000000000000000000000",
      "02000000",
      "A8FE3E87",
      "07EB1F84FB28F8CB73DE8E99E2F48A14" },
    { "C.1 20B+18AAD", "AES-128-GCM-SIV",
      "01000000000000000000000000000000",
      "010000000000000000000000000000000200",
      "0300000000000000000000000000000004000000",
      "6BB0FECF5DED9B77F902C7D5DA236A4391DD0297",
      "24AFC9805E976F451E6D87F6FE106514" },
};

int main(int argc, char **argv)
{
    EVP_CIPHER *probe;

    test_parse_args(argc, argv);

    /* AES-GCM-SIV is version-gated (added in OpenSSL 3.5): on older builds
     * the fetch fails and the whole program skips. */
    probe = EVP_CIPHER_fetch(NULL, "AES-128-GCM-SIV", NULL);
    if (probe == NULL)
        test_skip("AES-128-GCM-SIV is unavailable");
    EVP_CIPHER_free(probe);

    gsiv_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    gsiv_test_aad_chunking(&kats[0]);
    gsiv_test_roundtrip_sizes(&kats[0]);
    gsiv_test_second_update_fails(&kats[0]);
    gsiv_test_tamper(&kats[0]);

    test_report_pass();
    return 0;
}
