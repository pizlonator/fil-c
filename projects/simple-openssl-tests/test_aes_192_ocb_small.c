#include "test_common.h"
#include "test_aes_ocb_common.h"

#include <openssl/evp.h>

/*

 * test_aes_192_ocb_small -- AES-192-OCB known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared OCB update rules, the
 * >= 1 MiB regime, and the vector-derivation provenance live in
 * test_aes_ocb_common.c; a sibling binary covers the >= 1 MiB payload).

 * This binary carries the derived AES-192 tuple in the RFC 7253 Appendix A
 * format: the RFC publishes sample results only for 128-bit keys, so this
 * tuple was derived with a scratch probe against the authoritative pizfix
 * OpenSSL 3.6.5 and cross-checked byte for byte against the system OpenSSL
 * 3.0.13.

 */

/* The derived tuple's key (the 000102.. pattern continued to 24 bytes). */
static const char OCB_KEY192_HEX[] =
    "000102030405060708090A0B0C0D0E0F1011121314151617";

static const struct ocb_kat ocb_kats[] = {
    { "AES-192 (derived)", "AES-192-OCB", OCB_KEY192_HEX,
      ocb_nonce_anchor_hex, ocb_seq40_hex, ocb_seq40_hex,
      "86b418c98b14be157cad07cf20189dc8"
      "7a9e6fd19a07bec6d6acef811d9a860c"
      "9671666bf650dd2b",
      "6e9ca93fc80b9dcf1b25ae2fda9b2520" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ocb_test_kats(ocb_kats, sizeof(ocb_kats) / sizeof(ocb_kats[0]));
    ocb_test_iv_lengths("AES-192-OCB", OCB_KEY192_HEX);
    ocb_test_tag_lengths("AES-192-OCB", OCB_KEY192_HEX);
    ocb_test_partial_blocks("AES-192-OCB", OCB_KEY192_HEX);
    ocb_test_aad_after_data("AES-192-OCB", OCB_KEY192_HEX);
    ocb_test_tamper("AES-192-OCB", OCB_KEY192_HEX);

    test_report_pass();
    return 0;
}
