#include "test_common.h"

#include "test_sm4_common.h"

#include <openssl/evp.h>

/*

 * test_sm4_ccm -- SM4-CCM (3.1+): derived KAT (ivlen 12, taglen 12),
 * ivlen/taglen before the key+nonce init, NULL-update length announcement,
 * single payload update, second data update fails, and an authenticated
 * roundtrip.  One binary per cipher (the shared SM4 machinery, the AEAD
 * shared material, and the family-wide commentary live in
 * test_sm4_common.c).

 * Version gate: SM4-CCM postdates OpenSSL 3.0 (it fetches on the
 * authoritative pizfix 3.6.5 build, fails on the system 3.0.13), so this
 * binary probes its cipher right after test_parse_args()
 * (sm4_probe_or_skip) and prints the documented SKIP line on builds that
 * lack it.

 * Known-answer provenance: `openssl enc` cannot do AEAD, so the constants
 * below were derived with a scratch probe compiled by build/bin/filcc
 * against the pizfix 3.6.5 build.  The key, the 16-byte plaintext
 * 000102...0f, and the 12-byte nonce are the family-wide AEAD constants in
 * the common module; the ciphertext/tag KAT is this cipher's own.

 * CCM ordering rules mirror test_aes_ccm_small: ivlen AND taglen must be
 * set before the key+nonce init, the payload length is announced through a
 * NULL update, the payload is taken in ONE update (a second data update
 * fails), and the tag is retrieved via GET_TAG after Final.
 */

/* This cipher's derived KAT (ivlen 12, taglen 12). */
static const char *const CCM_CT_HEX =
    "b1d4bb0b908c33b3385e37d7111fe1c4";
static const char *const CCM_TAG_HEX =
    "2afb3c3d087ff69a0a15c9cd";

/* The CCM section. */
static void test_ccm(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-CCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(33 + 16);
    EVP_CIPHER_CTX *ctx;
    size_t ptlen, ctlen = 0;
    int len, flen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"SM4-CCM\") failed");
    from_hex(sm4_aead_key_hex, key, 16);
    from_hex(sm4_aead_nonce_hex, nonce, 16);
    ptlen = from_hex(sm4_aead_pt_hex, pt, 33);
    from_hex(CCM_CT_HEX, ct_want, 16);
    from_hex(CCM_TAG_HEX, tag_want, 16);

    /* KAT: ivlen/taglen before the key+nonce init, announcement, single
     * payload update. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, (int)ptlen) == 1,
          "payload-length announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)ptlen) == 1,
          "payload EVP_EncryptUpdate failed");
    CHECK((size_t)len == ptlen, "CCM update emitted a surprising length");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + ptlen, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    CHECK((size_t)flen == 0, "CCM Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 12, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    ctlen = ptlen; /* the KAT ciphertext is the announced payload */
    expect_equal("SM4-CCM ciphertext does not match the derived KAT",
                 ct, ptlen, ct_want, 16);
    expect_equal("SM4-CCM tag does not match the derived KAT",
                 tag, 12, tag_want, 12);

    /* A second data update after the announced single payload must fail. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, NULL, (int)ptlen) == 1,
          "payload-length announcement failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)ptlen) == 1,
          "first payload update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct + len, &len, pt, 1) != 1,
          "SM4-CCM accepted a second data update");
    EVP_CIPHER_CTX_free(ctx);

    /* Authenticated roundtrip. */
    {
        unsigned char tag_copy[16];
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
              "EVP_CTRL_AEAD_SET_IVLEN failed");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, NULL) == 1,
              "EVP_CTRL_AEAD_SET_TAG (tag length) failed");
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1,
              "second EVP_DecryptInit_ex failed");
        memcpy(tag_copy, tag, 12);
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, tag_copy) == 1,
              "EVP_CTRL_AEAD_SET_TAG failed");
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, NULL, (int)ctlen) == 1,
              "payload-length announcement failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
              "payload EVP_DecryptUpdate failed (tag mismatch?)");
        CHECK(EVP_DecryptFinal_ex(ctx, back + ctlen, &flen) == 1,
              "EVP_DecryptFinal_ex failed");
        EVP_CIPHER_CTX_free(ctx);
        expect_equal("SM4-CCM authenticated roundtrip did not recover the "
                     "plaintext", back, ctlen, pt, ptlen);
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* SM4-CCM is version-gated (it postdates OpenSSL 3.0): on older builds
     * the fetch fails and the whole program skips. */
    sm4_probe_or_skip("SM4-CCM");

    test_ccm();

    test_report_pass();
    return 0;
}
