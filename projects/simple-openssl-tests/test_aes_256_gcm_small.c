#include "test_common.h"
#include "test_aes_gcm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_gcm_small -- AES-256-GCM known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared GCM machinery, the
 * payload-regime split, and the vector provenance live in
 * test_aes_gcm_common.c).  Every payload here is <= 64 bytes; the 2 MiB
 * streaming regime is test_aes_256_gcm_large's.
 *
 * This binary carries the AES-256 McGrew/Viega rows TC15/TC16 (TC3/TC4 with
 * an AES-256 key).
 */

/* The AES-256 KAT rows: TC15 (64 bytes, no AAD) and TC16 (60 bytes + 20
 * bytes of AAD). */
static const struct gcm_kat kats[] = {
    { "TC15", "AES-256-GCM", gcm_key_256_hex, gcm_iv12_hex, "", gcm_pt64_hex,
      "522dc1f099567d07f47f37a32a84427d"
      "643a8cdcbfe5c0c97598a2bd2555d1aa"
      "8cb08e48590dbb3da7b08b1056828838"
      "c5f61e6393ba7a0abcc9f662898015ad",
      "b094dac5d93471bdec1a502270e3cc6c" },
    { "TC16", "AES-256-GCM", gcm_key_256_hex, gcm_iv12_hex, gcm_aad20_hex,
      gcm_pt60_hex,
      "522dc1f099567d07f47f37a32a84427d"
      "643a8cdcbfe5c0c97598a2bd2555d1aa"
      "8cb08e48590dbb3da7b08b1056828838"
      "c5f61e6393ba7a0abcc9f662",
      "76fc6ece0f4e1768cddf8853bb2d551b" },
};

/* TC16's 16-byte tag is a verified KAT (see the row above); its first 12
 * bytes are the expected short (AEAD_TAGLEN) tag. */
static const char *const TC16_TAG12_HEX =
    "76fc6ece0f4e1768cddf8853";

/* AAD-only (zero-length payload) tag for the TC16 key/IV/AAD shape: no
 * published counterpart exists; derived with a scratch probe against the
 * authoritative pizfix OpenSSL 3.6.5 (via build/bin/filcc) and cross-checked
 * byte for byte against the system OpenSSL 3.0.13. */
static const char *const AAD_ONLY_TAG_HEX =
    "9f6be07603c0b0bd1272854063e9c9ba";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    gcm_small_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    gcm_small_test_decrypt_and_tamper("AES-256-GCM", gcm_key_256_hex);
    gcm_small_test_split_updates(&kats[0]);
    gcm_small_test_aad_ordering("AES-256-GCM", gcm_key_256_hex);
    gcm_small_test_iv_lengths("AES-256-GCM", gcm_key_256_hex);
    gcm_small_test_short_tag("AES-256-GCM", gcm_key_256_hex, TC16_TAG12_HEX);
    gcm_small_test_zero_length_payload("AES-256-GCM", gcm_key_256_hex,
                                       AAD_ONLY_TAG_HEX);
    gcm_small_test_output_buffer_contract("AES-256-GCM", gcm_key_256_hex);

    test_report_pass();
    return 0;
}
