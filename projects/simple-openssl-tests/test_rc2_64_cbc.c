#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_rc2_64_cbc -- RC2-64-CBC (legacy provider) known-answer and
 * behavior tests, one binary per cipher (the shared table-driven runner,
 * the provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (RC2-64-CBC): the KAT ciphertext below was
 * verified with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -rc2-64-cbc \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 *
 * RC2-64-CBC uses a fixed 8-byte key (this row carries its own key
 * constant, not the family 16-byte one; note RC2-64 is 8 bytes = 64
 * bits, not 6).  The runner's key-length CHECK pins
 * EVP_CIPHER_get_key_length == 8, and the init then takes the 8-byte key
 * directly (the cipher's intrinsic key length, so no set_key_length
 * dance is needed).
 */

/* This cipher's row: the fixed 8-byte key, the family 8-byte IV and
 * 16-byte plaintext plus this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "RC2-64-CBC",
    "0123456789abcdef", legacy_block_iv8_hex,
    legacy_block_pt16_hex,
    "b4cf0b582f283cc597d56bdf55abf287",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    legacy_block_load_providers();
    legacy_block_probe_or_skip(row.ciphername);
    legacy_block_test_row(&row);

    test_report_pass();
    return 0;
}
