#include "test_common.h"
#include "test_cbc_cts_common.h"

#include <openssl/evp.h>

/*
 * test_aes_192_cbc_cts -- CBC ciphertext stealing (CS1/CS2/CS3) tests for
 * AES-192-CBC-CTS, one binary per cipher (the shared CTS rules, the version
 * note, and the derivation provenance live in test_cbc_cts_common.c).
 *
 * Known-answer provenance (AES-192): `openssl enc` accepts the CTS cipher
 * name but has no -cts_mode option, so the CS1 constant below was
 * additionally re-confirmed through `openssl enc -aes-192-cbc-cts`; the
 * CS2/CS3 constants were derived with the scratch EVP probe against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked byte for byte
 * against the system OpenSSL 3.0.13.
 */

/* The cipher under test and its 54-byte CS1/CS2/CS3 KATs for the shared
 * 54-byte plaintext pattern (key = the 000102... pattern continued to the
 * key size, shared via the common module; IV = 000102..0F). */
static const struct cts_kat kat = {
    "AES-192-CBC-CTS", "AES-192-CBC", cts_key_192_hex,
    /* CS1 */ "916251821c73a522c396d627380196071f443ad4cf0cfa11"
              "3d73bab6304ff9496fdd97aafa8ebd4142d53fb4de764066"
              "be1aa340ec2c",
    /* CS2 */ "916251821c73a522c396d627380196071f443ad4cf0cfa11"
              "3d73bab6304ff949bd4142d53fb4de764066be1aa340ec2c"
              "6fdd97aafa8e",
    /* CS3 */ "916251821c73a522c396d627380196071f443ad4cf0cfa11"
              "3d73bab6304ff949bd4142d53fb4de764066be1aa340ec2c"
              "6fdd97aafa8e",
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
