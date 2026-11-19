#include "test_common.h"

#include "test_sm4_common.h"

#include <openssl/evp.h>

/*

 * test_sm4_xts -- SM4-XTS (3.1+): derived KAT, 32-byte roundtrip, a data
 * unit shorter than 16 bytes fails in BOTH directions, and the 21-byte
 * ciphertext-stealing path roundtrips (XTS stealing works in both
 * directions -- see test_aes_xts_small).  One binary per cipher (the
 * shared SM4 machinery and the family-wide commentary live in
 * test_sm4_common.c).

 * Version gate: SM4-XTS postdates OpenSSL 3.0 (it fetches on the
 * authoritative pizfix 3.6.5 build, fails on the system 3.0.13), so this
 * binary probes its cipher right after test_parse_args()
 * (sm4_probe_or_skip) and prints the documented SKIP line on builds that
 * lack it.

 * Known-answer provenance: `openssl enc` cannot do XTS, so the constants
 * below were derived with a scratch probe compiled by build/bin/filcc
 * against the pizfix 3.6.5 build.  The SM4-XTS key is two concatenated
 * SM4 keys and the IV is the data-unit tweak.
 */

/* SM4-XTS key = two concatenated SM4 keys; the IV is the data-unit
 * tweak. */
static const char *const XTS_KEY_HEX =
    "0f1e2d3c4b5a69788796a5b4c3d2e1f0"
    "00112233445566778899aabbccddeeff";
static const char *const XTS_IV_HEX =
    "0102030405060708090a0b0c0d0e0f10";
static const char *const XTS_PT32_HEX =
    "000102030405060708090a0b0c0d0e0f"
    "000102030405060708090a0b0c0d0e0f";
static const char *const XTS_CT32_HEX =
    "167437d385df1664e9dea08d61758673"
    "de4302fcd839179df8f5304d0b833042";

/* The XTS section. */
static void test_xts(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-XTS", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *tweak = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    size_t i, ctlen, backlen;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"SM4-XTS\") failed");
    from_hex(XTS_KEY_HEX, key, 32);
    from_hex(XTS_IV_HEX, tweak, 16);
    from_hex(XTS_PT32_HEX, pt, 64);
    from_hex(XTS_CT32_HEX, ct_want, 64);

    ctlen = sm4_crypt(cipher, key, tweak, pt, 32, ct, 64 + 16, 1);
    expect_equal("SM4-XTS 32-byte ciphertext does not match the derived KAT",
                 ct, ctlen, ct_want, 32);
    backlen = sm4_crypt(cipher, key, tweak, ct_want, 32, back, 64 + 16, 0);
    expect_equal("SM4-XTS 32-byte roundtrip did not recover the plaintext",
                 back, backlen, pt, 32);

    /* A data unit shorter than 16 bytes must fail (xts128.c: "if (len < 16)
     * return -1"), both when encrypting and when decrypting. */
    for (i = 0; i < 2; i++) {
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        if (i == 0)
            CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, tweak, NULL) == 1,
                  "XTS encrypt init failed");
        else
            CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, tweak, NULL) == 1,
                  "XTS decrypt init failed");
        CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
              "XTS set_padding(0) failed");
        if (i == 0)
            CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 15) != 1,
                  "XTS accepted a 15-byte data unit when encrypting");
        else
            CHECK(EVP_DecryptUpdate(ctx, back, &len, ct_want, 15) != 1,
                  "XTS accepted a 15-byte data unit when decrypting");
        EVP_CIPHER_CTX_free(ctx);
    }

    /* 21 bytes: ciphertext stealing, roundtrip in both directions (the
     * derived 21-byte KAT anchors the encrypt direction). */
    {
        static const char *const CT21_HEX =
            "a97f71f8cdf224dcec346ce9ad30f7d4167437d385";
        unsigned char *ct21_want = ALLOCATE_OUTPUT(21);

        from_hex(CT21_HEX, ct21_want, 21);
        ctlen = sm4_crypt(cipher, key, tweak, pt, 21, ct, 64 + 16, 1);
        expect_equal("SM4-XTS 21-byte ciphertext does not match the derived KAT",
                     ct, ctlen, ct21_want, 21);
        backlen = sm4_crypt(cipher, key, tweak, ct, ctlen, back, 64 + 16, 0);
        expect_equal("SM4-XTS 21-byte decrypt (stealing) roundtrip failed",
                     back, backlen, pt, 21);
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* SM4-XTS is version-gated (it postdates OpenSSL 3.0): on older builds
     * the fetch fails and the whole program skips. */
    sm4_probe_or_skip("SM4-XTS");

    test_xts();

    test_report_pass();
    return 0;
}
