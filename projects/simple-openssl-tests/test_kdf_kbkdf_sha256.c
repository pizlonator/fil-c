#include "test_common.h"

#include "test_kdf_kbkdf_common.h"

#include <openssl/evp.h>

/*
 * test_kdf_kbkdf_sha256 -- SP 800-108 key-based KDF (KBKDF) through the
 * EVP_KDF public API for the SHA-256 HMAC PRF, one binary per digest (the
 * shared KBKDF machinery, the CAVS vector provenance, the parameter naming
 * quirk and the distribution decisions live in test_kdf_kbkdf_common.c).
 *
 * This binary is the family's representative digest, so besides its own
 * HMAC-SHA256
 * KAT rows it also carries the digest-independent CMAC-AES128 and KMAC128
 * PRF checks (those PRFs take a cipher or a MAC rather than a digest).
 *
 * Known-answer provenance: the CMAC-AES128 counter KAT (RLEN=32 bits) and
 * the CAVS r=8 vectors are NIST CAVS values as distributed in OpenSSL's own
 * test data; the HMAC-SHA256 switch values, the feedback-mode values and
 * the KMAC128 PRF value were derived with the authoritative pizfix OpenSSL
 * 3.6.5 and cross-checked under the system OpenSSL 3.0.13 (both builds
 * agree on every value below; see test_kdf_kbkdf_common.c for the command).
 */

/* This digest's family row: the HMAC PRF's digest param, its prose name for
 * the failure messages, and the PRF output size (the feedback-mode IV
 * length). */
static const struct kbkdf_family fam = { "SHA256", "SHA-256", 32 };

/* CAVS counter-mode vector, CMAC-AES128, RLEN=32 bits (digest-independent:
 * this binary runs it as the representative). */
static const char K32_HEX[] = "c10b152e8c97b77e18704e0f0bd38305";
static const char I32_HEX[] =
    "98cd4cbbbebe15d17dc86e6dbad800a2dcbd64f7c7ad0e78e9cf94ffdba89d03"
    "e97eadf6c4f7b806caf52aa38f09d0eb71d71f497bcc6906b48d36c4";
static const char OUT32_HEX[] = "26faf61908ad9ee881b8305c221db53f";

/* CAVS counter-mode vector, CMAC-AES128, RLEN=8 bits (needs "r"). */
static const char K8_HEX[] = "dff1e50ac0b69dc40f1051d46c2b069c";
static const char I8_HEX[] =
    "c16e6e02c5a3dcc8d78b9ac1306877761310455b4e41469951d9e6c2245a064b"
    "33fd8c3b01203a7824485bf0a64060c4648b707d2607935699316ea5";
static const char OUT8_HEX[] = "8be8f0869b3c0ba97b71863d1b9f7813";

/* CAVS counter-mode vector, HMAC-SHA256, RLEN=8 bits (needs "r"). */
static const char OUTSHA256R8_HEX[] =
    "9de01850c9d3d1cd0270b3ea9c9c683d";

/* HMAC-SHA256 switch KATs over the shared small material (both switches
 * off, use-l=1, use-separator=1; CLI-verified on both builds). */
static const char KAT_DEFAULT_HEX[] = "7ab5f99606c3801a1d4239b53bed3a51";
static const char KAT_USE_L_HEX[] = "00f64fac5b9c4de1dea987ab8bfb5cf8";
static const char KAT_USE_SEP_HEX[] = "d3958375825f4663e485093d936e8f45";

/* HMAC-SHA256 feedback KATs over the shared small material: without an IV
 * and with the 32-byte IV (== the SHA-256 PRF output size), itself the
 * 000102..1f pattern. */
static const char IV32_HEX[] =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
static const char KAT_NOIV_HEX[] =
    "7ab5f99606c3801a1d4239b53bed3a51ad39f7da479dc406e"
    "6bdb22d974c6ce6";
static const char KAT_WITHIV_HEX[] =
    "3979b30978322e221c0bb9a45d76ca1e6d54bd7dc8e8df46"
    "55c0cb82590eb391";

/* Feedback mode with CMAC-AES128 and a 16-byte IV (digest-independent). */
static const char CMAC_FB_OUT_HEX[] = "c498818b00696bf1fa9f289d61cbb627";

/* KMAC128 PRF KAT (digest-independent). */
static const char KMAC_KEY_HEX[] = "00112233445566778899aabbccddeeff";
static const char KMAC_LABEL_HEX[] = "0102030405060708";
static const char KMAC_INFO_HEX[] = "aabbccdd";
static const char KMAC_OUT_HEX[] =
    "9609c1c2b59258b88427e569c8f4ba9a1b2003274435e2"
    "97578890281a64694b";

/* This binary's r-width CAVS rows (the monolith's counter/HMAC section's
 * CMAC-AES128 and HMAC-SHA256 r=8 vectors, in the original order). */
static const struct kbkdf_r_kat rkats[] = {
    /* CAVS CMAC-AES128, r=8. */
    { "CMAC-AES128", "CMAC", "AES128", NULL, K8_HEX, I8_HEX, OUT8_HEX, 8 },
    /* CAVS HMAC-SHA256, r=8 (the shared small material). */
    { "HMAC-SHA256", "HMAC", NULL, "SHA256", kbkdf_ksmall_hex,
      kbkdf_ismall_hex, OUTSHA256R8_HEX, 8 },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    kbkdf_test_counter_cmac(K32_HEX, I32_HEX, OUT32_HEX);
    kbkdf_test_counter_hmac(&fam, KAT_DEFAULT_HEX, KAT_USE_L_HEX,
                            KAT_USE_SEP_HEX, rkats,
                            sizeof(rkats) / sizeof(rkats[0]));
    kbkdf_test_feedback_hmac(&fam, IV32_HEX, KAT_NOIV_HEX, KAT_WITHIV_HEX);
    kbkdf_test_feedback_cmac(K8_HEX, I8_HEX,
                             "000102030405060708090a0b0c0d0e0f",
                             CMAC_FB_OUT_HEX);
    kbkdf_test_kmac(KMAC_KEY_HEX, KMAC_LABEL_HEX, KMAC_INFO_HEX, KMAC_OUT_HEX);
    kbkdf_test_negative(&fam);

    test_report_pass();
    return 0;
}
