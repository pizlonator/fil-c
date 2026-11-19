#include "test_common.h"
#include "test_aes_gcm_siv_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_gcm_siv -- RFC 8452 AES-256-GCM-SIV tests, one binary per
 * cipher (the shared GCM-SIV machinery, the version gate, and the vector
 * provenance live in test_aes_gcm_siv_common.c).  This binary carries the
 * two RFC 8452 Appendix C.2 AES-256 rows; the nonce is the fixed 12-byte
 * RFC nonce and the key follows the RFC convention (01 followed by zero
 * bytes).
 */

/* RFC 8452 Appendix C.2 vectors (Key = 01 00..00 pattern of the key
 * length). */
static const struct gsiv_kat kats[] = {
    { "C.2 4B+12AAD", "AES-256-GCM-SIV",
      "01000000000000000000000000000000"
      "00000000000000000000000000000000",
      "010000000000000000000000",
      "02000000",
      "22B3F4CD",
      "1835E517741DFDDCCFA07FA4661B74CF" },
    { "C.2 #4", "AES-256-GCM-SIV",
      "01000000000000000000000000000000"
      "00000000000000000000000000000000",
      "",
      "01000000000000000000000000000000",
      "85A01B63025BA19B7FD3DDFC033B3E76",
      "C9EAC6FA700942702E90862383C6C366" },
};

int main(int argc, char **argv)
{
    EVP_CIPHER *probe;

    test_parse_args(argc, argv);

    /* AES-GCM-SIV is version-gated (added in OpenSSL 3.5): on older builds
     * the fetch fails and the whole program skips. */
    probe = EVP_CIPHER_fetch(NULL, "AES-256-GCM-SIV", NULL);
    if (probe == NULL)
        test_skip("AES-256-GCM-SIV is unavailable");
    EVP_CIPHER_free(probe);

    gsiv_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    gsiv_test_aad_chunking(&kats[0]);
    gsiv_test_roundtrip_sizes(&kats[0]);
    gsiv_test_second_update_fails(&kats[0]);
    gsiv_test_tamper(&kats[0]);

    test_report_pass();
    return 0;
}
