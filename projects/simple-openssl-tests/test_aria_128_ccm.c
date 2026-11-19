#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_128_ccm -- ARIA-128-CCM known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the AEAD ordering rules,
 * and the vector provenance live in test_aria_common.c).
 *
 * Known-answer provenance (ARIA-128-CCM): the tuple is the shared 16-byte
 * plaintext 000102...0f under the first 12 bytes of the family CBC IV
 * (0f1e2d3c4b5a69788796a5b4) with ivlen 12 and taglen 12; AEAD tuples
 * cannot come from `openssl enc`, so the ciphertext and 12-byte tag
 * below were derived with a scratch EVP probe compiled by
 * build/bin/filcc against the authoritative pizfix OpenSSL 3.6.5 and
 * cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key plus this cipher's KAT tuple
 * (ivlen 12, taglen 12; the nonce is the shared 12-byte CBC IV
 * prefix). */
static const struct aria_ccm_kat kat = {
    "ARIA-128-CCM",
    aria_key_128_hex,
    /* KAT ciphertext */
    "f6cf49fbc5c1431fa1b569af438fa085",
    /* KAT tag */
    "15fcccc398b289c7e12a5cdd",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    aria_test_ccm_kat(&kat);
    aria_test_ccm_second_data_update(&kat);
    aria_test_ccm_roundtrips(&kat);

    test_report_pass();
    return 0;
}
