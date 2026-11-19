#include "test_common.h"
#include "test_aes_gcm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_gcm_large -- AES-256-GCM with LARGE, multi-update payloads,
 * one binary per cipher (the 2 MiB self-consistency oracle, the chunk-grid
 * rationale, and the vector provenance live in test_aes_gcm_common.c).
 *
 * This binary anchors to the AES-256 McGrew/Viega row TC4-256 (TC4 with an
 * AES-256 key; the published tag only -- the published CAVS data carries no
 * AES-256 ciphertext for the shape, exactly as in the original combined
 * program's anchor table).
 */

/* The AES-256 anchor row for the streaming-vs-one-shot helper checks. */
static const struct gcm_kat kats[] = {
    { "TC4-256", "AES-256-GCM", gcm_key_256_hex, gcm_iv12_hex, gcm_aad20_hex,
      gcm_pt60_hex, NULL, NULL },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    gcm_large_test_kat_anchor(kats, sizeof(kats) / sizeof(kats[0]));
    gcm_large_test_stream_equals_one_shot("AES-256-GCM", gcm_key_256_hex);
    gcm_large_test_with_aad("AES-256-GCM", gcm_key_256_hex);
    gcm_large_test_aad_interleaved("AES-256-GCM", gcm_key_256_hex);
    gcm_large_test_iv_lengths("AES-256-GCM", gcm_key_256_hex);
    gcm_large_test_tampered_final_block("AES-256-GCM", gcm_key_256_hex);

    test_report_pass();
    return 0;
}
