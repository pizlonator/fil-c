#include "test_common.h"
#include "test_aes_ocb_common.h"

#include <openssl/evp.h>

/*

 * test_aes_128_ocb_small -- AES-128-OCB known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared OCB update rules, the
 * >= 1 MiB regime, and the vector-derivation provenance live in
 * test_aes_ocb_common.c; a sibling binary covers the >= 1 MiB payload).

 * This binary carries the RFC 7253 Appendix A sample tuples for AES-128 --
 * all verified through this EVP flow against the authoritative pizfix
 * OpenSSL 3.6.5 and cross-checked on 3.0.13 -- plus the RFC's 96-bit-tag
 * tuple with the second key.

 */

/* RFC 7253 Appendix A.  The nonce increments per tuple; A/P are 00..
 * sequences. */
static const char OCB_KEY128_HEX[] =
    "000102030405060708090A0B0C0D0E0F";
static const char OCB_KEY128B_HEX[] =  /* the 96-bit-tag tuple's key */
    "0F0E0D0C0B0A09080706050403020100";

static const struct ocb_kat ocb_kats[] = {
    { "A.1 #1", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221100", "", "", "",
      "785407BFFFC8AD9EDCC5520AC9111EE6" },
    { "A.1 #2", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221101",
      "0001020304050607", "0001020304050607",
      "6820B3657B6F615A",
      "5725BDA0D3B4EB3A257C9AF1F8F03009" },
    { "A.1 #3", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221102",
      "0001020304050607", "",
      "",
      "81017F8203F081277152FADE694A0A00" },
    { "A.1 #4", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221103",
      "", "0001020304050607",
      "45DD69F8F5AAE724",
      "14054CD1F35D82760B2CD00D2F99BFA9" },
    { "A.1 #5", "AES-128-OCB", OCB_KEY128_HEX,
      "BBAA99887766554433221104",
      "000102030405060708090A0B0C0D0E0F",
      "000102030405060708090A0B0C0D0E0F",
      "571D535B60B277188BE5147170A9A22C",
      "3AD7A4FF3835B8C5701C1CCEC8FC3358" },
    { "A.1 #13", "AES-128-OCB", OCB_KEY128_HEX,
      ocb_nonce_anchor_hex, ocb_seq40_hex, ocb_seq40_hex,
      "D5CA91748410C1751FF8A2F618255B68A0A12E093FF45460"
      "6E59F9C1D0DDC54B65E8628E568BAD7A",
      "ED07BA06A4A69483A7035490C5769E60" },
    { "A.2 96-bit tag", "AES-128-OCB", OCB_KEY128B_HEX,
      ocb_nonce_anchor_hex, ocb_seq40_hex, ocb_seq40_hex,
      "1792A4E31E0755FB03E31B22116E6C2DDF9EFD6E33D536F1"
      "A0124B0A55BAE884ED93481529C76B6A",
      "D0C515F4D1CDD4FDAC4F02AA" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    ocb_test_kats(ocb_kats, sizeof(ocb_kats) / sizeof(ocb_kats[0]));
    ocb_test_iv_lengths("AES-128-OCB", OCB_KEY128_HEX);
    ocb_test_tag_lengths("AES-128-OCB", OCB_KEY128_HEX);
    ocb_test_partial_blocks("AES-128-OCB", OCB_KEY128_HEX);
    ocb_test_aad_after_data("AES-128-OCB", OCB_KEY128_HEX);
    ocb_test_tamper("AES-128-OCB", OCB_KEY128_HEX);

    test_report_pass();
    return 0;
}
