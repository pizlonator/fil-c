#include "test_common.h"
#include "test_cbc_cts_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_256_cbc_cts -- CBC ciphertext stealing (CS1/CS2/CS3) tests
 * for CAMELLIA-256-CBC-CTS, one binary per cipher (the shared CTS rules,
 * the version note, and the derivation provenance live in
 * test_cbc_cts_common.c).
 *
 * Known-answer provenance (Camellia-256): `openssl enc` accepts the CTS
 * cipher name but has no -cts_mode option, so the CS1 constant below was
 * additionally re-confirmed through `openssl enc -camellia-256-cbc-cts`;
 * the CS2/CS3 constants were derived with the scratch EVP probe against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked byte for byte
 * against the system OpenSSL 3.0.13.
 */

/* The cipher under test and its 54-byte CS1/CS2/CS3 KATs for the shared
 * 54-byte plaintext pattern (key = the 000102... pattern continued to the
 * key size, shared via the common module; IV = 000102..0F). */
static const struct cts_kat kat = {
    "CAMELLIA-256-CBC-CTS", "CAMELLIA-256-CBC", cts_key_256_hex,
    /* CS1 */ "cecf4ffa14e51c625e58a8efc3ec3814"
              "97e385d962ea20e36674f0e1961a5da1"
              "ab24931b3aada424affaffc182fc54c4"
              "08efff4dd9ae",
    /* CS2 */ "cecf4ffa14e51c625e58a8efc3ec3814"
              "97e385d962ea20e36674f0e1961a5da1"
              "a424affaffc182fc54c408efff4dd9ae"
              "ab24931b3aad",
    /* CS3 */ "cecf4ffa14e51c625e58a8efc3ec3814"
              "97e385d962ea20e36674f0e1961a5da1"
              "a424affaffc182fc54c408efff4dd9ae"
              "ab24931b3aad",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cts_test_default_mode_readback(&kat);
    cts_test_kat_54(&kat);
    cts_test_length_sweep(&kat);
    cts_test_single_block(&kat);
    cts_test_update_rules(&kat);

    test_report_pass();
    return 0;
}
