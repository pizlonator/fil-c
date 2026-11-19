#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_cast5_ecb -- CAST5-ECB (legacy provider) known-answer and behavior
 * tests, one binary per cipher (the shared table-driven runner, the
 * provider situation, the IDEA version-gate note, and the derivation
 * provenance of the CLI-derived constants live in
 * test_legacy_block_common.c).
 *
 * Known-answer provenance (CAST5-ECB): the KAT ciphertext below was
 * verified with
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -cast5-ecb \
 *       -nopad -provider legacy -provider default -K <keyhex>
 *
 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 *
 * ECB takes no IV (the "" field below); CAST5's block is 8 bytes, so the
 * 16-byte plaintext is two identical blocks and the KAT ciphertext shows
 * the equal-blocks-equal-ciphertexts property.
 */

/* This cipher's row: the family 16-byte key and 16-byte plaintext plus
 * this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "CAST5-ECB",
    legacy_block_key16_hex, "",
    legacy_block_pt16_hex,
    "c8678f70485e9c93c8678f70485e9c93",
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
