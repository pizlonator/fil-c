#include "test_common.h"
#include "test_cbc_cts_common.h"

#include <openssl/evp.h>

/*
 * test_aes_256_cbc_cts -- CBC ciphertext stealing (CS1/CS2/CS3) tests for
 * AES-256-CBC-CTS, one binary per cipher (the shared CTS rules, the version
 * note, and the derivation provenance live in test_cbc_cts_common.c).
 *
 * Known-answer provenance (AES-256): `openssl enc` accepts the CTS cipher
 * name but has no -cts_mode option, so the CS1 constant below was
 * additionally re-confirmed through `openssl enc -aes-256-cbc-cts`; the
 * CS2/CS3 constants were derived with the scratch EVP probe against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked byte for byte
 * against the system OpenSSL 3.0.13.
 */

/* The cipher under test and its 54-byte CS1/CS2/CS3 KATs for the shared
 * 54-byte plaintext pattern (key = the 000102... pattern continued to the
 * key size, shared via the common module; IV = 000102..0F). */
static const struct cts_kat kat = {
    "AES-256-CBC-CTS", "AES-256-CBC", cts_key_256_hex,
    /* CS1 */ "f29000b62a499fd0a9f39a6add2e77804df1a80a1ea488dd"
              "a0096bfba58fa9bc6f641e6b90df449564258400a3080a97"
              "b728e4624ce3",
    /* CS2 */ "f29000b62a499fd0a9f39a6add2e77804df1a80a1ea488dd"
              "a0096bfba58fa9bc449564258400a3080a97b728e4624ce3"
              "6f641e6b90df",
    /* CS3 */ "f29000b62a499fd0a9f39a6add2e77804df1a80a1ea488dd"
              "a0096bfba58fa9bc449564258400a3080a97b728e4624ce3"
              "6f641e6b90df",
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
