#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_seed_cfb -- SEED-CFB (legacy provider) known-answer and behavior
 * tests, one binary per cipher (the shared table-driven runner, the
 * provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (SEED-CFB): the KAT ciphertext below was
 * verified with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -seed-cfb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 *
 * SEED's IV is the full 16-byte block (legacy_block_iv16_hex).  CFB here
 * is 128-bit feedback (SEED's full-block shift register) and is a stream
 * mode: the {5,3,8,2} streaming split must reproduce the one-shot
 * keystream exactly, over the 16-byte plaintext.
 */

/* This cipher's row: the family 16-byte key, SEED's full-block 16-byte
 * IV, and 16-byte plaintext plus this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "SEED-CFB",
    legacy_block_key16_hex, legacy_block_iv16_hex,
    legacy_block_pt16_hex,
    "9c75c7877b7936aec93575e06410768e",
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
