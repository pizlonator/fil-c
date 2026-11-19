#include "test_common.h"
#include "test_camellia_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_256_ctr -- CAMELLIA-256-CTR known-answer and behavior tests,
 * one binary per cipher (the shared Camellia machinery, the derivation
 * provenance of the CLI-derived constants, and the family-wide commentary
 * live in test_camellia_common.c).
 *
 * Known-answer provenance (CAMELLIA-256-CTR): the plaintext is the 64-byte
 * SP 800-38A shared vector, the initial counter is the family-wide
 * 000102...0f, and the key is the RFC 3713 128-bit key extended with
 * 00112233445566778899aabbccddeeff (shared constants in the common module);
 * the KAT ciphertext below was derived with
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -camellia-256-ctr -nopad -K <keyhex> -iv <ivhex>
 *
 * and cross-checked byte for byte under the system OpenSSL 3.0.13.
 */

/* This cipher's row: the shared key, initial counter, and 64-byte plaintext
 * plus this cipher's KAT ciphertext. */
static const struct camellia_kat kat = {
    "CAMELLIA-256-CTR",
    camellia_key_256_hex,
    camellia_ctr_iv_hex,
    camellia_pt64_hex, CAMELLIA_PT64_LEN,
    /* KAT ciphertext */
    "cabc1be5af6a98e74af5be7d4bf6861a"
    "f38316b10a965ba7851e4806b3a0a840"
    "6ca2ffa0b6a5f528c0de5c6c2b8aa124"
    "ef540b600d143a10b27d83703e4d74df",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    camellia_test_kat(&kat);
    camellia_test_alternate_routes(&kat, EVP_camellia_256_ctr(),
                                   "EVP_camellia_256_ctr()", "camellia-256-ctr");
    camellia_test_streaming(&kat);

    test_report_pass();
    return 0;
}
