#include "test_common.h"
#include "test_aria_common.h"

#include <openssl/evp.h>

/*
 * test_aria_256_cfb -- ARIA-256-CFB known-answer and behavior tests, one
 * binary per cipher (the shared ARIA machinery, the derivation provenance
 * of the CLI-derived constants, and the family-wide commentary live in
 * test_aria_common.c).
 *
 * Known-answer provenance (ARIA-256-CFB): the plaintext is the 32-byte
 * stream-mode vector and the IV is the family stream-mode IV
 * fedcba98765432100123456789abcdef (shared constants in the common module);
 * the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aria-256-cfb -nopad -K <keyhex> -iv <ivhex>
 *
 * decrypted back to its plaintext with the same CLI, and cross-checked byte
 * for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, IV, and 32-byte stream-mode plaintext
 * plus this cipher's KAT ciphertext. */
static const struct aria_kat kat = {
    "ARIA-256-CFB",
    aria_key_256_hex,
    aria_stream_iv_hex,
    aria_pt32_hex, ARIA_PT32_LEN,
    /* KAT ciphertext */
    "43cad67a3d5827f8bd63e90d01e6af7d"
    "6ef44ce53ec7e7c13506a353ab388824",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    aria_test_kat(&kat);
    aria_test_streaming(&kat);

    test_report_pass();
    return 0;
}
