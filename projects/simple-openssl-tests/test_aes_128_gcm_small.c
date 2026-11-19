#include "test_common.h"
#include "test_aes_gcm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_gcm_small -- AES-128-GCM known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared GCM machinery, the
 * payload-regime split, and the vector provenance live in
 * test_aes_gcm_common.c).  Every payload here is <= 64 bytes; the 2 MiB
 * streaming regime is test_aes_128_gcm_large's.
 *
 * This binary carries the AES-128 McGrew/Viega rows: TC1/TC2 (zero key and
 * zero IV) and the TC3/TC4 tuples for KEY128 (TC3/TC4 constants are shared
 * with test_aes_128_gcm_large and live in the common module).
 */

/* The AES-128 KAT rows: TC1 (empty plaintext, tag only), TC2 (one zero
 * block), TC3 (64 bytes, no AAD), TC4 (60 bytes + 20 bytes of AAD). */
static const struct gcm_kat kats[] = {
    { "TC1", "AES-128-GCM",
      "00000000000000000000000000000000",
      "000000000000000000000000",
      "", "",
      "",
      "58e2fccefa7e3061367f1d57a4e7455a" },
    { "TC2", "AES-128-GCM",
      "00000000000000000000000000000000",
      "000000000000000000000000",
      "",
      "00000000000000000000000000000000",
      "0388dace60b6a392f328c2b971b2fe78",
      "ab6e47d42cec13bdf53a67b21257bddf" },
    { "TC3", "AES-128-GCM", gcm_key_128_hex, gcm_iv12_hex, "", gcm_pt64_hex,
      gcm_tc3_ct_hex, gcm_tc3_tag_hex },
    { "TC4", "AES-128-GCM", gcm_key_128_hex, gcm_iv12_hex, gcm_aad20_hex,
      gcm_pt60_hex,
      "42831ec2217774244b7221b784d0d49c"
      "e3aa212f2c02a4e035c17e2329aca12e"
      "21d514b25466931c7d8f6a5aac84aa05"
      "1ba30b396a0aac973d58e091",
      gcm_tc4_tag_hex },
};

/* TC4's 16-byte tag is a verified KAT (see the TC4 row above); its first 12
 * bytes are the expected short (AEAD_TAGLEN) tag. */
static const char *const TC4_TAG12_HEX =
    "5bc94fbc3221a5db94fae95a";

/* AAD-only (zero-length payload) tag for TC4's key/IV/AAD; every hex value
 * in this file was verified on pizfix OpenSSL 3.6.5 and cross-checked on
 * 3.0.13 (see test_aes_gcm_common.c). */
static const char *const AAD_ONLY_TAG_HEX =
    "346434fd51d5cd0c5887ec63e39b907a";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    gcm_small_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    gcm_small_test_decrypt_and_tamper("AES-128-GCM", gcm_key_128_hex);
    gcm_small_test_split_updates(&kats[2]);
    gcm_small_test_aad_ordering("AES-128-GCM", gcm_key_128_hex);
    gcm_small_test_iv_lengths("AES-128-GCM", gcm_key_128_hex);
    gcm_small_test_short_tag("AES-128-GCM", gcm_key_128_hex, TC4_TAG12_HEX);
    gcm_small_test_zero_length_payload("AES-128-GCM", gcm_key_128_hex,
                                       AAD_ONLY_TAG_HEX);
    gcm_small_test_output_buffer_contract("AES-128-GCM", gcm_key_128_hex);

    test_report_pass();
    return 0;
}
