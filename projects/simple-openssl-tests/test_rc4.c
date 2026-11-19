#include "test_common.h"

#include "test_rc4_common.h"

#include <openssl/evp.h>

/*
 * test_rc4 -- RC4 (legacy provider) known-answer and behavior tests, one
 * binary per cipher (the shared RC4 machinery, the variable-key-length
 * route, the classic-vector provenance, and the family-wide commentary
 * live in test_rc4_common.c).
 *
 * Known-answer vectors: the three classic RC4 test vectors
 *
 *   key "Key"    (4b6579)          plaintext "Plaintext"      (9 bytes)
 *   key "Wiki"   (57696b69)        plaintext "pedia"          (5 bytes)
 *   key "Secret" (536563726574)    plaintext "Attack at dawn" (14 bytes)
 *
 * with the expected ciphertexts below.  They need SHORT keys, which the
 * provider route reaches through EVP_CIPHER_CTX_set_key_length() between
 * a bare init and the key init (the RC4 cipher carries the
 * VARIABLE_LENGTH flag; `openssl enc -K` always zero-pads to 16 bytes
 * and cannot produce these vectors).  The constants were verified with a
 * scratch probe compiled by build/bin/filcc against the pizfix 3.6.5
 * build and cross-checked against the system 3.0.13 (identical).
 *
 * Behavior (every section below runs for this binary's own cipher): RC4
 * state carries across updates, so any chunk split reproduces the
 * one-shot output; the bulk path in rc4_enc.c processes 8 bytes at a
 * time before a tail of single-byte steps, which the 8-byte-boundary
 * sections exercise; and an empty update leaves the keystream alone.
 */

/* The three classic RC4 KATs. */
static const struct rc4_kat kats[] = {
    { "4b6579",                          /* "Key" */
      "506c61696e74657874",              /* "Plaintext" */
      "bbf316e8d940af0ad3" },
    { "57696b69",                        /* "Wiki" */
      "7065646961",                      /* "pedia" */
      "1021bf0420" },
    { "536563726574",                    /* "Secret" */
      "41747461636b206174206461776e",    /* "Attack at dawn" */
      "45a01f645fc35b383552544b9bf5" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    rc4_load_providers();

    rc4_test_kats(kats, sizeof(kats) / sizeof(kats[0]));
    rc4_test_sizes("RC4", rc4_key16_hex, 16);
    rc4_test_streaming("RC4", rc4_key16_hex, 16);
    rc4_test_eight_byte_boundaries("RC4", rc4_key16_hex, 16);
    rc4_test_empty_update("RC4", rc4_key16_hex, 16);

    test_report_pass();
    return 0;
}
