#include "test_common.h"
#include "test_aes_ocb_common.h"

#include <openssl/evp.h>

/*

 * test_aes_256_ocb_small -- AES-256-OCB known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared OCB update rules, the
 * >= 1 MiB regime, and the vector-derivation provenance live in
 * test_aes_ocb_common.c; a sibling binary covers the >= 1 MiB payload).

 * This binary carries the derived AES-256 tuple in the RFC 7253 Appendix A
 * format: the RFC publishes sample results only for 128-bit keys, so this
 * tuple was derived with a scratch probe against the authoritative pizfix
 * OpenSSL 3.6.5 and cross-checked byte for byte against the system OpenSSL
 * 3.0.13.

 */

/* The derived tuple's key (the RFC's second-key pattern continued into an
 * ascending 000102.. run). */
static const char OCB_KEY256_HEX[] =
    "0F0E0D0C0B0A090807060504030201000102030405060708090A0B0C0D0E0F10";

static const struct ocb_kat ocb_kats[] = {
    { "AES-256 (derived)", "AES-256-OCB", OCB_KEY256_HEX,
      ocb_nonce_anchor_hex, ocb_seq40_hex, ocb_seq40_hex,
      "B2AA438B428CDAC8AA35BDFE9D5ACF7A81A1524755A8DC8F"
      "A37F51CE2AF20CD6E706BF734678481C",
      "2777FC08E154387CD69217463EB31781" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ocb_test_kats(ocb_kats, sizeof(ocb_kats) / sizeof(ocb_kats[0]));
    ocb_test_iv_lengths("AES-256-OCB", OCB_KEY256_HEX);
    ocb_test_tag_lengths("AES-256-OCB", OCB_KEY256_HEX);
    ocb_test_partial_blocks("AES-256-OCB", OCB_KEY256_HEX);
    ocb_test_aad_after_data("AES-256-OCB", OCB_KEY256_HEX);
    ocb_test_tamper("AES-256-OCB", OCB_KEY256_HEX);

    test_report_pass();
    return 0;
}
