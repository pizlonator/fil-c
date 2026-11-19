#include "test_common.h"
#include "test_cbc_cts_common.h"

#include <openssl/evp.h>

/*
 * test_camellia_128_cbc_cts -- CBC ciphertext stealing (CS1/CS2/CS3) tests
 * for CAMELLIA-128-CBC-CTS, one binary per cipher (the shared CTS rules,
 * the version note, and the derivation provenance live in
 * test_cbc_cts_common.c).  Like AES-128, this row cross-checks that the
 * shared CTS helpers are cipher-independent.
 *
 * Known-answer provenance (Camellia-128): the constants below were derived
 * with a scratch probe against the authoritative pizfix OpenSSL 3.6.5 and
 * cross-checked byte for byte against the system OpenSSL 3.0.13 (the
 * CS1/CS2/CS3 differences included).  The 38-byte CS3 KAT below uses the
 * same key/IV convention as the other rows.
 */

/* The cipher under test and its 54-byte CS1/CS2/CS3 KATs for the shared
 * 54-byte plaintext pattern (key = the 000102... pattern continued to the
 * key size; CAMELLIA-128 shares its key with AES-128; IV = 000102..0F). */
static const struct cts_kat kat = {
    "CAMELLIA-128-CBC-CTS", "CAMELLIA-128-CBC", cts_key_128_hex,
    /* CS1 */ "477650012aa6284033e1b85321eef770"
              "70a35c28b52fb3c93853113529d46dbc"
              "831bb66b8d54ebe8919e8656f21eeec5"
              "8f7672d9aa6a",
    /* CS2 */ "477650012aa6284033e1b85321eef770"
              "70a35c28b52fb3c93853113529d46dbc"
              "ebe8919e8656f21eeec58f7672d9aa6a"
              "831bb66b8d54",
    /* CS3 */ "477650012aa6284033e1b85321eef770"
              "70a35c28b52fb3c93853113529d46dbc"
              "ebe8919e8656f21eeec58f7672d9aa6a"
              "831bb66b8d54",
};

/* Camellia-128-only 38-byte pattern KAT, CS3. */
static const char *const PT38_HEX =
    "000102030405060708090A0B0C0D0E0F1011121314151617"
    "1819202122232425262728293031";
static const char *const CS3_38_HEX =
    "477650012aa6284033e1b85321eef770"
    "49f2ab18de42f141e0bf054e47d52438"
    "70a35c28b52f";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cts_test_default_mode_readback(&kat);
    cts_test_kat_54(&kat);
    cts_test_kat_generic(&kat, PT38_HEX, CS3_38_HEX, "CS3");
    cts_test_length_sweep(&kat);
    cts_test_single_block(&kat);
    cts_test_update_rules(&kat);

    test_report_pass();
    return 0;
}
