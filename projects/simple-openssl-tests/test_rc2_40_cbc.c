#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_rc2_40_cbc -- RC2-40-CBC (legacy provider) known-answer and
 * behavior tests, one binary per cipher (the shared table-driven runner,
 * the provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (RC2-40-CBC): the KAT ciphertext below was
 * verified with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -rc2-40-cbc \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 *
 * RC2-40-CBC uses a fixed 5-byte key (this row carries its own key
 * constant, not the family 16-byte one); the runner's key-length CHECK
 * pins EVP_CIPHER_get_key_length == 5, and the init then takes the
 * 5-byte key directly (the cipher's intrinsic key length, so no
 * set_key_length dance is needed).
 */

/* This cipher's row: the fixed 5-byte key, the family 8-byte IV and
 * 16-byte plaintext plus this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "RC2-40-CBC",
    "0123456789", legacy_block_iv8_hex,
    legacy_block_pt16_hex,
    "d42cf08b2780a01e7bd39f1c0435e957",
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
