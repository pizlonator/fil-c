#include "test_common.h"

#include "test_rc4_common.h"

#include <openssl/evp.h>

/*
 * test_rc4_40 -- RC4-40 (legacy provider) known-answer and behavior tests,
 * one binary per cipher (the shared RC4 machinery, the variable-key-length
 * route, the vector provenance, and the family-wide commentary live in
 * test_rc4_common.c).
 *
 * Known-answer vector: the RC4-40 vector (fixed 5-byte key) came from
 * `openssl enc -rc4-40 -provider legacy -provider default -K <5-byte
 * keyhex>` on both the pizfix 3.6.5 build and the system 3.0.13 build
 * (identical).
 *
 * RC4-40 is exactly plain RC4 under a 5-byte key -- the 40-bit export
 * variant with no key-stretching at the EVP level -- so the KAT section
 * below also asserts equality with RC4 under the same key.  The 5-byte
 * set_key_length handling (the variable-key-length route; RC4-40's
 * EVP_CIPHER_get_key_length is 5) is exercised by every section, and the
 * shared RC4 behavior sections (sizes, streaming, 8-byte boundaries,
 * empty updates) run here for this binary's own cipher as well.
 */

/* RC4-40's own constants: the 5-byte export key, the 16-byte plaintext,
 * and the CLI-derived KAT ciphertext. */
static const char *const RC4_40_KEY_HEX = "0123456789";
static const char *const RC4_40_PT_HEX =
    "0102030405060708090a0b0c0d0e0f10";
static const char *const RC4_40_CT_HEX =
    "1a37602e040e72d424d3dc63072af795";

/* (1) RC4-40 (fixed 5-byte key): KAT plus equality with plain RC4 under
 * the same 5-byte key. */
static void test_rc4_40_kat(void)
{
    EVP_CIPHER *c40 = rc4_fetch("RC4-40");
    EVP_CIPHER *crc4 = rc4_fetch("RC4");
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct40 = ALLOCATE_OUTPUT(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16);
    size_t keylen, ptlen, l40, lrc4;

    CHECK((size_t)EVP_CIPHER_get_key_length(c40) == 5,
          "RC4-40 key length is not 5");
    keylen = from_hex(RC4_40_KEY_HEX, key, 16);
    ptlen = from_hex(RC4_40_PT_HEX, pt, 16);
    from_hex(RC4_40_CT_HEX, ct_want, 16);

    l40 = rc4_crypt(c40, key, keylen, pt, ptlen, ct40, 16, 1);
    expect_equal("the RC4-40 ciphertext does not match the CLI-derived KAT",
                 ct40, l40, ct_want, 16);

    lrc4 = rc4_crypt(crc4, key, keylen, pt, ptlen, ct, 16, 1);
    expect_equal("RC4-40 differs from RC4 with the same 5-byte key",
                 ct40, l40, ct, lrc4);
    EVP_CIPHER_free(c40);
    EVP_CIPHER_free(crc4);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    rc4_load_providers();

    test_rc4_40_kat();
    rc4_test_sizes("RC4-40", RC4_40_KEY_HEX, 5);
    rc4_test_streaming("RC4-40", RC4_40_KEY_HEX, 5);
    rc4_test_eight_byte_boundaries("RC4-40", RC4_40_KEY_HEX, 5);
    rc4_test_empty_update("RC4-40", RC4_40_KEY_HEX, 5);

    test_report_pass();
    return 0;
}
