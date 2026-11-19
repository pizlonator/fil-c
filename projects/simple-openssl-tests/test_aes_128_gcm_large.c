#include "test_common.h"
#include "test_aes_gcm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_gcm_large -- AES-128-GCM with LARGE, multi-update payloads,
 * one binary per cipher (the 2 MiB self-consistency oracle, the chunk-grid
 * rationale, and the vector provenance live in test_aes_gcm_common.c).
 *
 * This binary anchors to the AES-128 McGrew/Viega rows: TC3 (full tuple)
 * and TC4 (the published tag only -- the TC4 ciphertext is the 60-byte
 * prefix of TC3's and is pinned by the small binary's rows).
 */

/* The AES-128 anchor rows for the streaming-vs-one-shot helper checks. */
static const struct gcm_kat kats[] = {
    { "TC3", "AES-128-GCM", gcm_key_128_hex, gcm_iv12_hex, "", gcm_pt64_hex,
      gcm_tc3_ct_hex, gcm_tc3_tag_hex },
    { "TC4", "AES-128-GCM", gcm_key_128_hex, gcm_iv12_hex, gcm_aad20_hex,
      gcm_pt60_hex, NULL, gcm_tc4_tag_hex },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    gcm_large_test_kat_anchor(kats, sizeof(kats) / sizeof(kats[0]));
    gcm_large_test_stream_equals_one_shot("AES-128-GCM", gcm_key_128_hex);
    gcm_large_test_with_aad("AES-128-GCM", gcm_key_128_hex);
    gcm_large_test_aad_interleaved("AES-128-GCM", gcm_key_128_hex);
    gcm_large_test_iv_lengths("AES-128-GCM", gcm_key_128_hex);
    gcm_large_test_tampered_final_block("AES-128-GCM", gcm_key_128_hex);

    test_report_pass();
    return 0;
}
