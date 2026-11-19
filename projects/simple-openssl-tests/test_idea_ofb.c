#include "test_common.h"

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/*
 * test_idea_ofb -- IDEA-OFB (legacy provider) known-answer and behavior
 * tests, one binary per cipher (the shared table-driven runner, the
 * provider situation, and the derivation provenance of the CLI-derived
 * constants live in test_legacy_block_common.c).
 *
 * Known-answer provenance (IDEA-OFB): the system OpenSSL 3.0.13 build
 * does not offer IDEA at all, so the KAT ciphertext below comes from the
 * pizfix 3.6.5 oracle alone,
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -idea-ofb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * Version gate: IDEA is absent from some builds (Ubuntu's 3.0.13
 * compiles it out); on those this binary prints the family SKIP line
 * (legacy_block_probe_or_skip) and exits 0.
 *
 * OFB is a stream mode: the {5,3,8,2} streaming split must reproduce the
 * one-shot keystream exactly (the shift register carries across
 * updates), over the 16-byte plaintext.
 */

/* This cipher's row: the family 16-byte key, 8-byte IV, and 16-byte
 * plaintext plus this cipher's KAT ciphertext. */
static const struct legacy_block_row row = {
    "IDEA-OFB",
    legacy_block_key16_hex, legacy_block_iv8_hex,
    legacy_block_pt16_hex,
    "84e59104b8a8c8298cb5af470fd9fbf4",
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
