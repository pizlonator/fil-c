#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_256_ccm -- ARIA-256-CCM known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the AEAD ordering rules,
 * and the vector provenance live in test_aria_common.c).
 *
 * Known-answer provenance (ARIA-256-CCM): the tuple is the shared 16-byte
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
    "ARIA-256-CCM",
    aria_key_256_hex,
    /* KAT ciphertext */
    "72e66f705df3e51087274c7fc426abee",
    /* KAT tag */
    "478d61d4083321e9d834a541",
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
