#include "test_common.h"
#include "test_aes_ccm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_ccm_large -- AES-128-CCM with a large single-update payload,
 * one binary per cipher (the single-update contract, the 1 MiB regime, and
 * the vector provenance live in test_aes_ccm_common.c).  CCM fixes the
 * payload length with the first data update and rejects a second one, so
 * -- unlike the GCM/OCB programs -- there is no streaming-vs-one-shot
 * comparison; the single update IS the streaming contract.
 *
 * This binary anchors to RFC 3610 packet vector #1 (its tuple constants are
 * shared with test_aes_128_ccm_small and live in the common module).
 */

/* The RFC 3610 #1 anchor row. */
static const struct ccm_kat kat = {
    "RFC3610#1", "AES-128-CCM",
    ccm_rfc3610_key_hex,
    ccm_rfc3610_nonce_hex,
    ccm_rfc3610_aad_hex,
    ccm_rfc3610_pt_hex,
    ccm_rfc3610_ct_hex,
    ccm_rfc3610_tag_hex,
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ccm_large_test_kat_anchor(&kat);
    ccm_large_test_roundtrip("AES-128-CCM");
    ccm_large_test_second_data_update("AES-128-CCM");
    ccm_large_test_zero_length_payload("AES-128-CCM");
    ccm_large_test_tamper("AES-128-CCM");
    ccm_large_test_output_buffer_contract("AES-128-CCM");

    test_report_pass();
    return 0;
}
