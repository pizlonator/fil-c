#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_cast5_cbc -- CAST5-CBC (legacy provider) known-answer and behavior
 * tests, one binary per cipher (the shared table-driven runner, the
 * provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (CAST5-CBC): the KAT ciphertext below was
 * verified with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -cast5-cbc \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 *
 * CBC mode over the block-aligned 16-byte plaintext, padding disabled:
 * the KAT roundtrip and the {5,3,8,2} streaming split must both recover
 * the plaintext.
 */

/* This cipher's row: the family 16-byte key, 8-byte IV, and 16-byte
 * plaintext plus this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "CAST5-CBC",
    legacy_block_key16_hex, legacy_block_iv8_hex,
    legacy_block_pt16_hex,
    "04e2684cc4d83b767722ea1d8c4771dd",
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
