#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_rc2_ofb -- RC2-OFB (legacy provider) known-answer and behavior
 * tests, one binary per cipher (the shared table-driven runner, the
 * provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (RC2-OFB): the KAT ciphertext below was
 * verified with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -rc2-ofb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 *
 * OFB is a stream mode: the {5,3,8,2} streaming split must reproduce the
 * one-shot keystream exactly (the shift register carries across
 * updates), over the 32-byte plaintext.
 */

/* This cipher's row: the family 16-byte key, 8-byte IV, and 32-byte
 * stream-mode plaintext plus this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "RC2-OFB",
    legacy_block_key16_hex, legacy_block_iv8_hex,
    legacy_block_pt32_hex,
    "f0eb8b77800b634050bc4ea886f58a3c"
    "6e7667f3bab1cae7c10ff01e39c67940",
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
