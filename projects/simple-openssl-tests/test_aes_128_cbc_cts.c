#include "test_common.h"
#include "test_cbc_cts_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_cbc_cts -- CBC ciphertext stealing (CS1/CS2/CS3) tests for
 * AES-128-CBC-CTS, one binary per cipher (the shared CTS rules, the version
 * note, and the derivation provenance live in test_cbc_cts_common.c).
 *
 * Known-answer provenance (AES-128): the constants below were derived with
 * a scratch probe against the authoritative pizfix OpenSSL 3.6.5 and
 * cross-checked byte for byte against the system OpenSSL 3.0.13 (the
 * CS1/CS2/CS3 differences included).
 */

/* The cipher under test and its 54-byte CS1/CS2/CS3 KATs for the shared
 * 54-byte plaintext pattern (key = the 000102... pattern continued to the
 * key size, shared via the common module; IV = 000102..0F). */
static const struct cts_kat kat = {
    "AES-128-CBC-CTS", "AES-128-CBC", cts_key_128_hex,
    /* CS1 */ "c6a13b37878f5b826f4f8162a1c8d879"
              "7b76b773f24d5b06a31ff04c02b96951"
              "715eab1c8033ce27882efe832ea04504"
              "0665e9ed2977",
    /* CS2 */ "c6a13b37878f5b826f4f8162a1c8d879"
              "7b76b773f24d5b06a31ff04c02b96951"
              "ce27882efe832ea045040665e9ed2977"
              "715eab1c8033",
    /* CS3 */ "c6a13b37878f5b826f4f8162a1c8d879"
              "7b76b773f24d5b06a31ff04c02b96951"
              "ce27882efe832ea045040665e9ed2977"
              "715eab1c8033",
};

/* 32-byte (block-aligned) pattern KAT: CS1 = plain CBC, CS3 swaps the last
 * two blocks. */
static const char *const CS1_32_HEX =
    "c6a13b37878f5b826f4f8162a1c8d879"
    "35d9dcdb829fec3352e7bf10b84be4a5";
static const char *const CS3_32_HEX =
    "35d9dcdb829fec3352e7bf10b84be4a5"
    "c6a13b37878f5b826f4f8162a1c8d879";
/* Single-block (16-byte) pattern KAT: all variants are plain CBC. */
static const char *const CS3_16_HEX =
    "c6a13b37878f5b826f4f8162a1c8d879";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    cts_test_default_mode_readback(&kat);
    cts_test_kat_54(&kat);
    cts_test_kat_32(&kat, CS1_32_HEX, CS3_32_HEX);
    cts_test_kat_16_cs3(&kat, CS3_16_HEX);
    cts_test_length_sweep(&kat);
    cts_test_single_block(&kat);
    cts_test_update_rules(&kat);

    test_report_pass();
    return 0;
}
