#include "test_common.h"
#include "test_aes_ccm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_ccm_small -- AES-192-CCM known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared CCM machinery, the
 * ordering rules, and the vector provenance live in
 * test_aes_ccm_common.c).  Every payload here is small; the 1 MiB
 * single-update regime is test_aes_192_ccm_large's.
 *
 * Known-answer provenance (AES-192): RFC 3610 publishes only 128-bit key
 * vectors, so the "RFC3610#1-192 (derived)" row below (the #1 shape with a
 * 24-byte key continuing the C0C1.. byte pattern) was derived with a
 * scratch probe against the authoritative pizfix OpenSSL 3.6.5 and
 * cross-checked byte for byte against the system OpenSSL 3.0.13.  Its
 * constants are shared with test_aes_192_ccm_large and live in the common
 * module.
 */

/* The derived AES-192 reshaping of RFC 3610 packet vector #1. */
static const struct ccm_kat kats[] = {
    { "RFC3610#1-192 (derived)", "AES-192-CCM",
      ccm_rfc3610_192_key_hex,
      ccm_rfc3610_nonce_hex,
      ccm_rfc3610_aad_hex,
      ccm_rfc3610_pt_hex,
      ccm_rfc3610_192_ct_hex,
      ccm_rfc3610_192_tag_hex },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ccm_small_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    ccm_small_test_matrix("AES-192-CCM");
    ccm_small_test_second_data_update("AES-192-CCM");
    ccm_small_test_aad_only("AES-192-CCM");
    ccm_small_test_param_rules("AES-192-CCM");

    test_report_pass();
    return 0;
}
