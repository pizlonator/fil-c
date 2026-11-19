#include "test_common.h"
#include "test_aes_ccm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_ccm_small -- AES-256-CCM known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared CCM machinery, the
 * ordering rules, and the vector provenance live in
 * test_aes_ccm_common.c).  Every payload here is small; there is no 1 MiB
 * large sibling for this key size (the original combined program covered
 * the large regime for 128/192 only).
 *
 * Known-answer provenance (AES-256): RFC 3610 publishes only 128-bit key
 * vectors, so the "RFC3610#1-256 (derived)" row below (the #1 shape with a
 * 32-byte key continuing the C0C1.. byte pattern) was derived with a
 * scratch probe against the authoritative pizfix OpenSSL 3.6.5 (via
 * build/bin/filcc) and cross-checked byte for byte against the system
 * OpenSSL 3.0.13, giving the 256-bit family its own KAT anchor alongside
 * the published-128 and derived-192 rows.
 */

/* The derived AES-256 reshaping of RFC 3610 packet vector #1 (only this
 * binary uses it, so the constants stay here rather than in the common
 * module). */
static const char *const RFC3610_256_KEY_HEX =
    "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECFD0D1D2D3D4D5D6D7E0E1E2E3E4E5E6E7";
static const char *const RFC3610_256_CT_HEX =
    "4eaf5c7621132d5e47ab6965cdcef7944c776c8be7d8ad";
static const char *const RFC3610_256_TAG_HEX =
    "f27025c66170a1cd";

static const struct ccm_kat kats[] = {
    { "RFC3610#1-256 (derived)", "AES-256-CCM",
      RFC3610_256_KEY_HEX,
      ccm_rfc3610_nonce_hex,
      ccm_rfc3610_aad_hex,
      ccm_rfc3610_pt_hex,
      RFC3610_256_CT_HEX,
      RFC3610_256_TAG_HEX },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ccm_small_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    ccm_small_test_matrix("AES-256-CCM");
    ccm_small_test_second_data_update("AES-256-CCM");
    ccm_small_test_aad_only("AES-256-CCM");
    ccm_small_test_param_rules("AES-256-CCM");

    test_report_pass();
    return 0;
}
