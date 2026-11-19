#include "test_common.h"

#include "test_sm4_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_sm4_gcm -- SM4-GCM (3.1+): derived KAT, IV-length-before-init, AAD
 * ordering, roundtrip and tamper detection.  One binary per cipher (the
 * shared SM4 machinery, the AEAD shared material, and the family-wide
 * commentary live in test_sm4_common.c).

 * Version gate: SM4-GCM postdates OpenSSL 3.0 (it fetches on the
 * authoritative pizfix 3.6.5 build, fails on the system 3.0.13), so this
 * binary probes its cipher right after test_parse_args() (sm4_probe_or_
 * skip) and prints the documented SKIP line on builds that lack it.

 * Known-answer provenance: `openssl enc` cannot do AEAD, so the constants
 * below were derived with a scratch probe compiled by build/bin/filcc
 * against the pizfix 3.6.5 build.  The key, the 16-byte plaintext
 * 000102...0f, and the 12-byte nonce are the family-wide AEAD constants in
 * the common module; the no-AAD ciphertext/tag KAT and the AAD constant
 * are this cipher's own.

 * AEAD ordering rules mirror test_aes_gcm_small: the GCM IV length is set
 * before the init that passes the IV; AAD updates (out == NULL) come
 * first; the tag is retrieved via GET_TAG after Final and set via SET_TAG
 * before decrypting; verification happens at DecryptFinal.
 */

/* This cipher's derived KAT (no-AAD one-shot) and the AAD used by the
 * behavior section. */
static const char *const GCM_CT_HEX =
    "09f0ac99a5389f56a6882998c1e84a82";
static const char *const GCM_TAG_HEX =
    "d1b887050076313ee0124d639d0da40e";
static const char *const GCM_AAD_HEX =
    "feedfacedeadbeeffeedfacedeadbeefabaddad2";

/* One-shot GCM encrypt: SET_IVLEN before the key+IV init, AAD first with
 * out == NULL, then the payload, then Final, then GET_TAG. */
static void gcm_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, size_t ivlen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t outcap, size_t *outlen,
                        unsigned char *tag, size_t taglen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, dlen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_EncryptUpdate failed");
    dlen = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* One-shot GCM decrypt with tag verification. */
static void gcm_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, size_t ivlen,
                        const unsigned char *aad, size_t aadlen,
                        const unsigned char *in, size_t inlen,
                        unsigned char *out, size_t outcap, size_t *outlen,
                        const unsigned char *tag, size_t taglen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, dlen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, (int)ivlen, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_DecryptInit_ex failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (aadlen)
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_DecryptUpdate failed");
    dlen = len;
    CHECK(taglen <= sizeof(tag_copy), "tag longer than 16 bytes");
    memcpy(tag_copy, tag, taglen);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen,
                              tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += len;
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* The GCM section: no-AAD one-shot KAT, with-AAD authenticated roundtrip,
 * and tampered-tag detection. */
static void test_gcm(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "SM4-GCM", NULL);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *nonce = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(16 + 16);
    unsigned char tag_bad[16];
    EVP_CIPHER_CTX *ctx;
    size_t aadlen, ptlen, ctlen, backlen;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"SM4-GCM\") failed");
    from_hex(sm4_aead_key_hex, key, 16);
    from_hex(sm4_aead_nonce_hex, nonce, 16);
    aadlen = from_hex(GCM_AAD_HEX, aad, 20);
    ptlen = from_hex(sm4_aead_pt_hex, pt, 16);
    from_hex(GCM_CT_HEX, ct_want, 16);
    from_hex(GCM_TAG_HEX, tag_want, 16);

    /* No-AAD one-shot KAT (derived on 3.6.5; see the header comment). */
    gcm_encrypt(cipher, key, nonce, 12, NULL, 0, pt, ptlen, ct, 16 + 16,
                &ctlen, tag, 16);
    expect_equal("SM4-GCM ciphertext does not match the derived KAT",
                 ct, ctlen, ct_want, 16);
    expect_equal("SM4-GCM tag does not match the derived KAT",
                 tag, 16, tag_want, 16);

    /* With AAD: roundtrip. */
    gcm_encrypt(cipher, key, nonce, 12, aad, aadlen, pt, ptlen, ct, 16 + 16,
                &ctlen, tag, 16);
    gcm_decrypt(cipher, key, nonce, 12, aad, aadlen, ct, ctlen, back,
                16 + 16, &backlen, tag, 16);
    expect_equal("SM4-GCM authenticated decrypt did not recover the plaintext",
                 back, backlen, pt, ptlen);

    /* Tampered tag: Final must fail. */
    memcpy(tag_bad, tag, 16);
    tag_bad[0] ^= 0x40;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
          "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "EVP_DecryptUpdate failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
          "SM4-GCM decryption accepted a tampered tag");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* SM4-GCM is version-gated (it postdates OpenSSL 3.0): on older builds
     * the fetch fails and the whole program skips. */
    sm4_probe_or_skip("SM4-GCM");

    test_gcm();

    test_report_pass();
    return 0;
}
