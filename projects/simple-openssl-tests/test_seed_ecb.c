#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_seed_ecb -- SEED-ECB (legacy provider) known-answer and behavior
 * tests, one binary per cipher (the shared table-driven runner, the
 * provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (SEED-ECB): the KAT ciphertext below was
 * verified with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -seed-ecb \
 *       -nopad -provider legacy -provider default -K <keyhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 *
 * ECB takes no IV (the "" field below); SEED's block is the full 16
 * bytes, so the 16-byte plaintext is exactly one block.
 */

/* This cipher's row: the family 16-byte key and 16-byte plaintext plus
 * this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "SEED-ECB",
    legacy_block_key16_hex, "",
    legacy_block_pt16_hex,
    "f36e01a46033fa56fd7b1a40ed0bf836",
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
