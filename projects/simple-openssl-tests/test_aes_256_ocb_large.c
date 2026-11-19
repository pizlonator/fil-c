#include "test_common.h"
#include "test_aes_ocb_common.h"

#include <openssl/evp.h>

/*

 * test_aes_256_ocb_large -- AES-256-OCB with large, multi-update payloads,
 * one binary per cipher (the shared OCB update rules, the chunk-grid
 * rationale, and the vector-derivation provenance live in
 * test_aes_ocb_common.c).

 * The helpers are anchored to the RFC 7253 Appendix A tuple for AES-256
 * below (verified against the authoritative pizfix OpenSSL 3.6.5 and
 * cross-checked on 3.0.13; the 40/40-byte tuple is the RFC's largest
 * published sample, though the RFC itself publishes no 256-bit-key sample
 * -- the tuple was derived with a scratch probe and cross-checked on both
 * OpenSSL builds, as documented in test_aes_ocb_common.c).  The oracle for
 * the large payload is self-consistency: streaming must equal one-shot.

 */

/* The tuple's key (the RFC's second-key pattern continued into an ascending
 * 000102.. run). */
static const char OCB_KEY256_HEX[] =
    "0F0E0D0C0B0A090807060504030201000102030405060708090A0B0C0D0E0F10";

/* KAT anchor: the derived RFC 7253-format tuple (same inputs and constants
 * as the small binary's row). */
static const struct ocb_kat anchor = {
    "AES-256 (derived)", "AES-256-OCB", OCB_KEY256_HEX,
    ocb_nonce_anchor_hex, ocb_seq40_hex, ocb_seq40_hex,
    "B2AA438B428CDAC8AA35BDFE9D5ACF7A81A1524755A8DC8F"
    "A37F51CE2AF20CD6E706BF734678481C",
    "2777FC08E154387CD69217463EB31781",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ocb_test_kat_anchor(&anchor);
    ocb_test_large_stream_equals_one_shot("AES-256-OCB", 32);
    ocb_test_large_aad("AES-256-OCB", 32);
    ocb_test_tampered_final_block("AES-256-OCB", 32);

    test_report_pass();
    return 0;
}
