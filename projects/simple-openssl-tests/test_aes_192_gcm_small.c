#include "test_common.h"
#include "test_aes_gcm_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_gcm_small -- AES-192-GCM known-answer and behavior tests for
 * small payloads, one binary per cipher (the shared GCM machinery, the
 * payload-regime split, and the vector provenance live in
 * test_aes_gcm_common.c).  Every payload here is <= 64 bytes; the 2 MiB
 * streaming regime is test_aes_192_gcm_large's.
 *
 * Known-answer provenance (AES-192): the McGrew/Viega spec and the CAVS
 * data publish no AES-192 tuple for the TC3/TC4 key/IV/plaintext shapes, so
 * the "TC3-192 (derived)" and "TC4-192 (derived)" rows below were derived
 * with a scratch probe against the authoritative pizfix OpenSSL 3.6.5 (via
 * build/bin/filcc) and cross-checked byte for byte against the system
 * OpenSSL 3.0.13.
 */

/* The AES-192 KAT rows: TC3-192 (64 bytes, no AAD) and TC4-192 (60 bytes +
 * 20 bytes of AAD), both derived (see above). */
static const struct gcm_kat kats[] = {
    { "TC3-192 (derived)", "AES-192-GCM", gcm_key_192_hex, gcm_iv12_hex, "",
      gcm_pt64_hex,
      "3980ca0b3c00e841eb06fac4872a2757"
      "859e1ceaa6efd984628593b40ca1e19c"
      "7d773d00c144c525ac619d18c84a3f47"
      "18e2448b2fe324d9ccda2710acade256",
      "9924a7c8587336bfb118024db8674a14" },
    { "TC4-192 (derived)", "AES-192-GCM", gcm_key_192_hex, gcm_iv12_hex,
      gcm_aad20_hex, gcm_pt60_hex,
      "3980ca0b3c00e841eb06fac4872a2757"
      "859e1ceaa6efd984628593b40ca1e19c"
      "7d773d00c144c525ac619d18c84a3f47"
      "18e2448b2fe324d9ccda2710",
      "2519498e80f1478f37ba55bd6d27618c" },
};

/* TC4-192's 16-byte tag is a verified derived KAT (see the row above); its
 * first 12 bytes are the expected short (AEAD_TAGLEN) tag. */
static const char *const TC4_TAG12_HEX =
    "2519498e80f1478f37ba55bd";

/* AAD-only (zero-length payload) tag for the TC4-192 key/IV/AAD shape: no
 * published counterpart exists (the spec has no AES-192 tuples); derived
 * with a scratch probe against the authoritative pizfix OpenSSL 3.6.5 (via
 * build/bin/filcc) and cross-checked byte for byte against the system
 * OpenSSL 3.0.13. */
static const char *const AAD_ONLY_TAG_HEX =
    "c8253387e5f78673d538a60d50527a92";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    gcm_small_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    gcm_small_test_decrypt_and_tamper("AES-192-GCM", gcm_key_192_hex);
    gcm_small_test_split_updates(&kats[0]);
    gcm_small_test_aad_ordering("AES-192-GCM", gcm_key_192_hex);
    gcm_small_test_iv_lengths("AES-192-GCM", gcm_key_192_hex);
    gcm_small_test_short_tag("AES-192-GCM", gcm_key_192_hex, TC4_TAG12_HEX);
    gcm_small_test_zero_length_payload("AES-192-GCM", gcm_key_192_hex,
                                       AAD_ONLY_TAG_HEX);
    gcm_small_test_output_buffer_contract("AES-192-GCM", gcm_key_192_hex);

    test_report_pass();
    return 0;
}
