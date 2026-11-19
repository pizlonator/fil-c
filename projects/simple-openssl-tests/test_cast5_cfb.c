#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_cast5_cfb -- CAST5-CFB (legacy provider) known-answer and behavior
 * tests, one binary per cipher (the shared table-driven runner, the
 * provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (CAST5-CFB): the KAT ciphertext below was
 * verified with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -cast5-cfb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 *
 * CFB (64-bit feedback, CAST5's 8-byte block) is a stream mode: the
 * {5,3,8,2} streaming split must reproduce the one-shot keystream
 * exactly (the shift register plus num offset carry across updates),
 * over the 32-byte plaintext.
 */

/* This cipher's row: the family 16-byte key, 8-byte IV, and 32-byte
 * stream-mode plaintext plus this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "CAST5-CFB",
    legacy_block_key16_hex, legacy_block_iv8_hex,
    legacy_block_pt32_hex,
    "cc02818217b150780809663e3b30ecc6"
    "ce894a59352cc46cf0f771a154e6647c",
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
