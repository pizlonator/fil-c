#include "test_common.h"
#include "test_aes_ocb_common.h"

#include <openssl/evp.h>

/*

 * test_aes_192_ocb_large -- AES-192-OCB with large, multi-update payloads,
 * one binary per cipher (the shared OCB update rules, the chunk-grid
 * rationale, and the vector-derivation provenance live in
 * test_aes_ocb_common.c).

 * The helpers are anchored to the derived RFC 7253-format tuple for
 * AES-192 below (the RFC publishes no 192-bit-key sample; the tuple was
 * derived with a scratch probe against the authoritative pizfix OpenSSL
 * 3.6.5 and cross-checked byte for byte against the system OpenSSL 3.0.13,
 * as documented in test_aes_ocb_common.c).  The oracle for the large
 * payload is self-consistency: streaming must equal one-shot.

 */

/* The derived tuple's key (the 000102.. pattern continued to 24 bytes). */
static const char OCB_KEY192_HEX[] =
    "000102030405060708090A0B0C0D0E0F1011121314151617";

/* KAT anchor: the derived RFC 7253-format tuple (same inputs and constants
 * as the small binary's row). */
static const struct ocb_kat anchor = {
    "AES-192 (derived)", "AES-192-OCB", OCB_KEY192_HEX,
    ocb_nonce_anchor_hex, ocb_seq40_hex, ocb_seq40_hex,
    "86b418c98b14be157cad07cf20189dc8"
    "7a9e6fd19a07bec6d6acef811d9a860c"
    "9671666bf650dd2b",
    "6e9ca93fc80b9dcf1b25ae2fda9b2520",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ocb_test_kat_anchor(&anchor);
    ocb_test_large_stream_equals_one_shot("AES-192-OCB", 24);
    ocb_test_large_aad("AES-192-OCB", 24);
    ocb_test_tampered_final_block("AES-192-OCB", 24);

    test_report_pass();
    return 0;
}
