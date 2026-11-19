#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_256_ecb -- ARIA-256-ECB known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the derivation provenance
 * of the CLI-derived constants, and the family-wide commentary live in
 * test_aria_common.c).
 *
 * Known-answer provenance (ARIA-256-ECB): the plaintext is the RFC 3713-style
 * block 0123456789abcdeffedcba9876543210 and the key is the family pattern
 * 00112233... (shared constants in the common module); RFC 5794 itself
 * specifies GCM (not ECB) vectors, so the KAT ciphertext below was derived
 * with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aria-256-ecb -nopad -K <keyhex>
 *
 * decrypted back to its plaintext with the same CLI, and cross-checked byte
 * for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key and plaintext plus this cipher's KAT
 * ciphertext (ECB takes no IV). */
static const struct aria_kat kat = {
    "ARIA-256-ECB",
    aria_key_256_hex,
    NULL,
    aria_pt16_hex, ARIA_PT16_LEN,
    /* KAT ciphertext */
    "06700293e2b6ba0eaaf5f6da89c93ed3",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    aria_test_kat(&kat);
    aria_test_streaming(&kat);

    test_report_pass();
    return 0;
}
