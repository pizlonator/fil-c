#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_128_cfb8 -- ARIA-128-CFB8 known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the derivation provenance
 * of the CLI-derived constants, and the family-wide commentary live in
 * test_aria_common.c).
 *
 * Known-answer provenance (ARIA-128-CFB8): the plaintext is the 32-byte
 * stream-mode vector and the IV is the family stream-mode IV
 * fedcba98765432100123456789abcdef (shared constants in the common module);
 * the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aria-128-cfb8 -nopad -K <keyhex> -iv <ivhex>
 *
 * decrypted back to its plaintext with the same CLI, and cross-checked byte
 * for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, IV, and 32-byte stream-mode plaintext
 * plus this cipher's KAT ciphertext. */
static const struct aria_kat kat = {
    "ARIA-128-CFB8",
    aria_key_128_hex,
    aria_stream_iv_hex,
    aria_pt32_hex, ARIA_PT32_LEN,
    /* KAT ciphertext */
    "397b71f1b509ae3d4a98c13dfae55430"
    "7975bb11a0ea27d0da966004d830091c",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    aria_test_kat(&kat);
    aria_test_streaming(&kat);

    test_report_pass();
    return 0;
}
