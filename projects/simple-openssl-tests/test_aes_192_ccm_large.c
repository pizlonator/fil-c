#include "test_common.h"
#include "test_aes_ccm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_ccm_large -- AES-192-CCM with a large single-update payload,
 * one binary per cipher (the single-update contract, the 1 MiB regime, and
 * the vector provenance live in test_aes_ccm_common.c).  CCM fixes the
 * payload length with the first data update and rejects a second one, so
 * -- unlike the GCM/OCB programs -- there is no streaming-vs-one-shot
 * comparison; the single update IS the streaming contract.
 *
 * Known-answer provenance (AES-192): RFC 3610 publishes only 128-bit key
 * vectors, so the "RFC3610#1-192 (derived)" anchor below (the #1 shape with
 * a 24-byte key continuing the C0C1.. byte pattern) was derived with a
 * scratch probe against the authoritative pizfix OpenSSL 3.6.5 and
 * cross-checked byte for byte against the system OpenSSL 3.0.13.  Its
 * constants are shared with test_aes_192_ccm_small and live in the common
 * module.
 */

/* The derived RFC 3610 #1-192 anchor row. */
static const struct ccm_kat kat = {
    "RFC3610#1-192 (derived)", "AES-192-CCM",
    ccm_rfc3610_192_key_hex,
    ccm_rfc3610_nonce_hex,
    ccm_rfc3610_aad_hex,
    ccm_rfc3610_pt_hex,
    ccm_rfc3610_192_ct_hex,
    ccm_rfc3610_192_tag_hex,
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ccm_large_test_kat_anchor(&kat);
    ccm_large_test_roundtrip("AES-192-CCM");
    ccm_large_test_second_data_update("AES-192-CCM");
    ccm_large_test_zero_length_payload("AES-192-CCM");
    ccm_large_test_tamper("AES-192-CCM");
    ccm_large_test_output_buffer_contract("AES-192-CCM");

    test_report_pass();
    return 0;
}
