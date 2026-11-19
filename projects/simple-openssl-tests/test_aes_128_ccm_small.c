#include "test_common.h"
#include "test_aes_ccm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_ccm_small -- AES-128-CCM known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared CCM machinery, the
 * ordering rules, and the vector provenance live in
 * test_aes_ccm_common.c).  Every payload here is small; the 1 MiB
 * single-update regime is test_aes_128_ccm_large's.
 *
 * This binary carries the RFC 3610 packet vectors #1 and #6 (M = 8, 13-byte
 * nonces, cleartext headers used as AAD); the #1 tuple constants are shared
 * with test_aes_128_ccm_large and live in the common module.
 */

/* The AES-128 RFC 3610 rows. */
static const struct ccm_kat kats[] = {
    { "RFC3610#1", "AES-128-CCM",
      ccm_rfc3610_key_hex,
      ccm_rfc3610_nonce_hex,
      ccm_rfc3610_aad_hex,
      ccm_rfc3610_pt_hex,
      ccm_rfc3610_ct_hex,
      ccm_rfc3610_tag_hex },
    { "RFC3610#6", "AES-128-CCM",
      "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF",
      "00000008070605A0A1A2A3A4A5",
      "000102030405060708090A0B",
      "0C0D0E0F101112131415161718191A1B1C1D1E1F20",
      "6FC1B011F006568B5171A42D953D469B2570A4BD87",
      "405A0443AC91CB94" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ccm_small_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    ccm_small_test_matrix("AES-128-CCM");
    ccm_small_test_second_data_update("AES-128-CCM");
    ccm_small_test_aad_only("AES-128-CCM");
    ccm_small_test_param_rules("AES-128-CCM");

    test_report_pass();
    return 0;
}
