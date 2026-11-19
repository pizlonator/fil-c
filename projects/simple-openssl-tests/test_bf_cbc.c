#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_bf_cbc -- Blowfish-CBC (legacy provider) known-answer and behavior
 * tests, one binary per cipher (the shared table-driven runner, the
 * provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (BF-CBC): the KAT ciphertext below was verified
 * with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -bf-cbc \
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
    "BF-CBC",
    legacy_block_key16_hex, legacy_block_iv8_hex,
    legacy_block_pt16_hex,
    "b79bd4be4a34d3bab5e964b328ab48d5",
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
