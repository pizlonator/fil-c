#include "test_common.h"
#include "test_aes_gcm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_gcm_large -- AES-192-GCM with LARGE, multi-update payloads,
 * one binary per cipher (the 2 MiB self-consistency oracle, the chunk-grid
 * rationale, and the vector provenance live in test_aes_gcm_common.c).
 *
 * Known-answer provenance (AES-192): the McGrew/Viega spec and the CAVS
 * data publish no AES-192 tuple for the TC3 shape, so the "TC3-192
 * (derived)" anchor below was derived with a scratch probe against the
 * authoritative pizfix OpenSSL 3.6.5 (via build/bin/filcc) and
 * cross-checked byte for byte against the system OpenSSL 3.0.13.
 */

/* The AES-192 anchor row for the streaming-vs-one-shot helper checks (the
 * full derived TC3-192 tuple; its constants are shared with
 * test_aes_192_gcm_small and live in the common module). */
static const struct gcm_kat kats[] = {
    { "TC3-192 (derived)", "AES-192-GCM", gcm_key_192_hex, gcm_iv12_hex, "",
      gcm_pt64_hex, gcm_tc3_192_ct_hex, gcm_tc3_192_tag_hex },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    gcm_large_test_kat_anchor(kats, sizeof(kats) / sizeof(kats[0]));
    gcm_large_test_stream_equals_one_shot("AES-192-GCM", gcm_key_192_hex);
    gcm_large_test_with_aad("AES-192-GCM", gcm_key_192_hex);
    gcm_large_test_aad_interleaved("AES-192-GCM", gcm_key_192_hex);
    gcm_large_test_iv_lengths("AES-192-GCM", gcm_key_192_hex);
    gcm_large_test_tampered_final_block("AES-192-GCM", gcm_key_192_hex);

    test_report_pass();
    return 0;
}
