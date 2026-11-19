#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_128_gcm -- ARIA-128-GCM known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the AEAD ordering rules,
 * and the vector provenance live in test_aria_common.c).
 *
 * Known-answer provenance (ARIA-128-GCM): the tuple is the shared 16-byte
 * plaintext 000102...0f under the first 12 bytes of the family CBC IV
 * (0f1e2d3c4b5a69788796a5b4); AEAD tuples cannot come from `openssl
 * enc`, so the ciphertext and 16-byte tag below were derived with a
 * scratch EVP probe compiled by build/bin/filcc against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked byte for byte
 * under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key plus this cipher's no-AAD KAT tuple
 * (the AAD/streaming/tamper sections use the row's cipher and key with
 * the shared IV, AAD, and plaintext constants of the common module). */
static const struct aria_gcm_kat kat = {
    "ARIA-128-GCM",
    aria_key_128_hex,
    /* KAT ciphertext */
    "29981dedab64449e4266cfe65cb77657",
    /* KAT tag */
    "bc5d8f78978c319439447ebfc8d1e562",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    aria_test_gcm_kat(&kat);
    aria_test_gcm_aad_roundtrip(&kat);
    aria_test_gcm_streaming(&kat);
    aria_test_gcm_tamper(&kat);

    test_report_pass();
    return 0;
}
