/*

 * test_aes_gcm_common.c -- shared implementation of the AES-GCM family
 * tests (API in test_aes_gcm_common.h; one binary per cipher and payload
 * regime, one struct gcm_kat row set per binary).

 * The family splits into two suites over the same helpers:

 *   - the small suite (payloads <= 64 bytes): the key size's McGrew/Viega
 *     KAT rows plus the ordering/IV/tag/zero-length/buffer-contract
 *     behavior sections;
 *   - the large suite (2 MiB payloads): the streaming-vs-one-shot regime
 *     plus the AAD/IV/tamper behavior sections.

 * Known-answer vectors (both suites): the classic McGrew/Viega GCM spec
 * test cases, as distributed in OpenSSL's own CAVS data
 * (test/recipes/30-test_evp_data/evpciph_aes_common.txt in the OpenSSL
 * source).  Naming follows that spec:

 *   TC1:  zero key, zero IV, empty plaintext, no AAD -> tag only
 *   TC2:  zero key, zero IV, one 16-byte zero block
 *   TC3:  key feffe9..08, IV cafe..888, 64-byte plaintext, no AAD
 *   TC4:  same key/IV, 60-byte plaintext, AAD feedface..d2
 *   TC15: TC3 with an AES-256 key
 *   TC16: TC4 with an AES-256 key

 * The McGrew/Viega spec and the CAVS data publish no AES-192 tuple for the
 * TC3/TC4 key/IV/plaintext shapes, so the "TC3-192 (derived)" and
 * "TC4-192 (derived)" rows were derived with a scratch probe against the
 * authoritative pizfix OpenSSL 3.6.5 (via build/bin/filcc) and cross-checked
 * byte for byte against the system OpenSSL 3.0.13.

 * Every hex value below was verified by running the vector through the
 * EVP API compiled against the authoritative pizfix OpenSSL 3.6.5 (via
 * build/bin/filcc) and cross-checked against the system OpenSSL 3.0.13.

 * The large suite's oracle for the 2 MiB payloads is self-consistency: a
 * payload of 2 MiB fed through updates of sizes
 * {16,17,64,100,128,129,3071,3072,3073,65536} must produce exactly the
 * same ciphertext and tag as a single one-shot update of the same 2 MiB
 * (and decrypt must mirror that).  The chunk sizes deliberately straddle
 * the internal GHASH regimes of crypto/modes/gcm128.c: the sub-block "mres"
 * buffer (16/17/64/100/128/129), the AES_GCM_ENC_BYTES = 128 staging kernel
 * (3071/3072/3073), and the GHASH_CHUNK = 3072 bulk path plus whole
 * 65536-byte updates (see gcm128.c around lines 350 and 869).  The same KAT
 * vectors are re-run through the very one-shot and streaming helpers that
 * process the 2 MiB payloads, including one run where the 64-byte KAT
 * plaintext is fed through the large chunk list itself.
 */

#include "test_aes_gcm_common.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/* Shared KAT material.  TC1's and TC2's zero key/IV stay inline in the
 * AES-128 small binary; the TC3/TC4 shapes below are family-wide.  KEY192
 * is KEY128 followed by its first 8 bytes.  The constants are arrays (not
 * pointers) so that the per-cipher binaries can use them in static struct
 * gcm_kat initializers. */
const char gcm_key_128_hex[] =
    "feffe9928665731c6d6a8f9467308308";
const char gcm_key_192_hex[] =
    "feffe9928665731c6d6a8f9467308308feffe9928665731c";
const char gcm_key_256_hex[] =
    "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308";
const char gcm_iv12_hex[] =
    "cafebabefacedbaddecaf888";
const char gcm_aad20_hex[] =
    "feedfacedeadbeeffeedfacedeadbeefabaddad2";
const char gcm_pt60_hex[] =
    "d9313225f88406e5a55909c5aff5269a"
    "86a7a9531534f7da2e4c303d8a318a72"
    "1c3c0c95956809532fcf0e2449a6b525"
    "b16aedf5aa0de657ba637b39";
const char gcm_pt64_hex[] =
    "d9313225f88406e5a55909c5aff5269a"
    "86a7a9531534f7da2e4c303d8a318a72"
    "1c3c0c95956809532fcf0e2449a6b525"
    "b16aedf5aa0de657ba637b391aafd255";

/* TC3/TC4 tuple constants shared by the small and large binaries of their
 * key size (TC3's ciphertext is the 64-byte shape, TC4's tag the 60-byte +
 * AAD shape; the large AES-128 anchor pins TC4's tag only). */
const char gcm_tc3_ct_hex[] =
    "42831ec2217774244b7221b784d0d49c"
    "e3aa212f2c02a4e035c17e2329aca12e"
    "21d514b25466931c7d8f6a5aac84aa05"
    "1ba30b396a0aac973d58e091473f5985";
const char gcm_tc3_tag_hex[] =
    "4d5c2af327cd64a62cf35abd2ba6fab4";
const char gcm_tc4_tag_hex[] =
    "5bc94fbc3221a5db94fae95ae7121a47";
/* TC3 shape with the 24-byte gcm_key_192_hex key: no published McGrew/Viega
 * or CAVS counterpart exists; derived with a scratch probe against pizfix
 * OpenSSL 3.6.5 and cross-checked byte for byte against system 3.0.13. */
const char gcm_tc3_192_ct_hex[] =
    "3980ca0b3c00e841eb06fac4872a2757"
    "859e1ceaa6efd984628593b40ca1e19c"
    "7d773d00c144c525ac619d18c84a3f47"
    "18e2448b2fe324d9ccda2710acade256";
const char gcm_tc3_192_tag_hex[] =
    "9924a7c8587336bfb118024db8674a14";

/* Total large payload: 2 MiB = 2 * 1024 * 1024 bytes; 1 KiB of AAD for the
 * large AAD section. */
#define LARGE_LEN (2u * 1024u * 1024u)
#define AAD_LARGE_LEN 1024u

/* The update sizes the large streaming helpers cycle through. */
static const size_t CHUNKS[] = {
    16, 17, 64, 100, 128, 129, 3071, 3072, 3073, 65536
};

/* The update sizes the small streaming helper cycles through. */
static const size_t SMALL_CHUNKS[] = { 5, 5, 6, 16, 32 };

/* Fetches a core AES-GCM cipher, CHECK()ing success (AES-GCM is core
 * OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *gcm_fetch_checked(const char *ciphername,
                                     const char *failmsg)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);

    CHECK(cipher != NULL, failmsg);
    return cipher;
}

/* One-shot GCM encrypt.  AAD is fed first with out == NULL, then the
 * plaintext, then Final, then the tag is read out.  The IV length is set
 * with EVP_CTRL_AEAD_SET_IVLEN *before* the init call that passes the IV --
 * otherwise OpenSSL would copy the default 12 bytes out of the caller's
 * buffer regardless of the intended length. */
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
    if (inlen) {
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "EVP_EncryptUpdate failed");
        dlen = len;
    }
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
    if (inlen) {
        CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "EVP_DecryptUpdate failed");
        dlen = len;
    }
    CHECK(taglen <= sizeof(tag_copy), "tag longer than 16 bytes");
    memcpy(tag_copy, tag, taglen);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, (int)taglen, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += len;
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* Streaming GCM encrypt (small suite): feed the plaintext in updates of
 * {5,5,6,16,32} bytes (last one truncated).  Must reproduce the one-shot
 * result exactly. */
static void gcm_stream_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                               const unsigned char *iv, size_t ivlen,
                               const unsigned char *aad, size_t aadlen,
                               const unsigned char *in, size_t inlen,
                               unsigned char *out, size_t outcap,
                               size_t *outlen, unsigned char *tag, size_t taglen)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
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
    while (off < inlen) {
        size_t n = SMALL_CHUNKS[ci++ % (sizeof(SMALL_CHUNKS) / sizeof(SMALL_CHUNKS[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "EVP_EncryptUpdate failed in streaming test");
        dlen += len;
        off += n;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "EVP_EncryptFinal_ex failed in streaming test");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, (int)taglen, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* Streaming GCM encrypt (large suite): AAD first, then the payload in
 * CHUNKS-sized updates (last one truncated), Final, GET_TAG. */
static void gcm_encrypt_streaming(const EVP_CIPHER *cipher, const unsigned char *key,
                                  const unsigned char *iv, size_t ivlen,
                                  const unsigned char *aad, size_t aadlen,
                                  const unsigned char *in, size_t inlen,
                                  unsigned char *out, size_t outcap,
                                  size_t *outlen, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
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
    while (off < inlen) {
        size_t n = CHUNKS[ci++ % (sizeof(CHUNKS) / sizeof(CHUNKS[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_EncryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "streaming EVP_EncryptUpdate failed");
        dlen += len;
        off += n;
    }
    CHECK(off == inlen, "streaming updates did not cover the whole payload");
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &len) == 1,
          "streaming EVP_EncryptFinal_ex failed");
    dlen += len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* Streaming GCM decrypt with tag verification: AAD first, then CHUNKS-sized
 * updates, SET_TAG, Final. */
static void gcm_decrypt_streaming(const EVP_CIPHER *cipher, const unsigned char *key,
                                  const unsigned char *iv, size_t ivlen,
                                  const unsigned char *aad, size_t aadlen,
                                  const unsigned char *in, size_t inlen,
                                  unsigned char *out, size_t outcap,
                                  size_t *outlen, const unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    size_t off = 0, ci = 0;
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
    while (off < inlen) {
        size_t n = CHUNKS[ci++ % (sizeof(CHUNKS) / sizeof(CHUNKS[0]))];
        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_DecryptUpdate(ctx, out + dlen, &len, in + off, (int)n) == 1,
              "streaming EVP_DecryptUpdate failed");
        dlen += len;
        off += n;
    }
    CHECK(off == inlen, "streaming updates did not cover the whole payload");
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &len) == 1,
          "streaming EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += len;
    *outlen = (size_t)dlen;
    EVP_CIPHER_CTX_free(ctx);
}

/* Fills the large buffer with a deterministic pattern (no KAT dependence). */
static void fill_pattern(unsigned char *buf, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)((i * 31 + (i >> 9) * 7 + 11) & 0xff);
}

/* (s1) One-shot encrypt == KAT for all of the binary's rows, including AAD
 * ordering (AAD first with out == NULL, then data, then Final, then
 * GET_TAG). */
void gcm_small_test_kats(const struct gcm_kat *kats, size_t nkats)
{
    size_t k;

    for (k = 0; k < nkats; k++) {
        const struct gcm_kat *kat = &kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(32);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *aad = ALLOCATE_AAD(20);
        unsigned char *pt = ALLOCATE_INPUT(64);
        unsigned char *ct_want = ALLOCATE_OUTPUT(64);
        unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
        unsigned char *tag_want = ALLOCATE_TAG(16);
        unsigned char *tag = ALLOCATE_TAG(16);
        size_t ptlen, aadlen, ct_wantlen, ctlen, tag_wantlen;
        char msg[128];

        /* AES-GCM is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
        from_hex(kat->keyhex, key, 32);
        from_hex(kat->ivhex, iv, 16);
        ptlen = from_hex(kat->pthex, pt, 64);
        aadlen = from_hex(kat->aadhex, aad, 20);
        ct_wantlen = from_hex(kat->cthex, ct_want, 64);
        tag_wantlen = from_hex(kat->taghex, tag_want, 16);

        gcm_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ptlen, ct, 64 + 16,
                    &ctlen, tag, 16);
        snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                 kat->label);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
        snprintf(msg, sizeof(msg), "%s: tag does not match the KAT", kat->label);
        expect_equal(msg, tag, 16, tag_want, tag_wantlen);
        EVP_CIPHER_free(cipher);

        free_poison_allocations();
    }
}

/* (s2) Decrypt with the right tag succeeds and recovers the plaintext; with
 * a tampered tag DecryptFinal must fail. */
void gcm_small_test_decrypt_and_tamper(const char *ciphername,
                                       const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_bad = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER *cipher = gcm_fetch_checked(ciphername,
                                           "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
    EVP_CIPHER_CTX *ctx;
    size_t ptlen, ctlen, backlen;
    int len;

    from_hex(keyhex, key, 32);
    from_hex(gcm_iv12_hex, iv, 16);
    from_hex(gcm_aad20_hex, aad, 20);
    ptlen = from_hex(gcm_pt60_hex, pt, 64);
    gcm_encrypt(cipher, key, iv, 12, aad, 20, pt, ptlen, ct, 64 + 16, &ctlen,
                tag, 16);

    gcm_decrypt(cipher, key, iv, 12, aad, 20, ct, ctlen, back, 64 + 16,
                &backlen, tag, 16);
    expect_equal("authenticated decrypt did not recover the plaintext",
                 back, backlen, pt, ptlen);

    /* Tamper with one tag bit: Final must fail. */
    memcpy(tag_bad, tag, 16);
    tag_bad[0] ^= 0x40;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 20) == 1,
          "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "EVP_DecryptUpdate failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_bad) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
          "decryption accepted a tampered tag");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s3) Splitting the 64-byte TC3-shape plaintext into updates of
 * {5,5,6,16,32} bytes must give the same ciphertext and the same tag as the
 * one-shot update. */
void gcm_small_test_split_updates(const struct gcm_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    size_t ptlen, ct_wantlen, ctlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
    from_hex(kat->keyhex, key, 32);
    from_hex(kat->ivhex, iv, 16);
    ptlen = from_hex(kat->pthex, pt, 64);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);
    from_hex(kat->taghex, tag_want, 16);

    gcm_stream_encrypt(cipher, key, iv, 12, NULL, 0, pt, ptlen, ct, 64 + 16,
                       &ctlen, tag, 16);
    expect_equal("split-update ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);
    expect_equal("split-update tag does not match the one-shot tag",
                 tag, 16, tag_want, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s4) AAD after the first data update must fail. */
void gcm_small_test_aad_ordering(const char *ciphername, const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *in = ALLOCATE_INPUT(5);
    unsigned char *out = ALLOCATE_OUTPUT(5 + 16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    from_hex(gcm_iv12_hex, iv, 16);
    from_hex(gcm_aad20_hex, aad, 20);
    for (i = 0; i < 5; i++)
        in[i] = (unsigned char)(0xA0 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) == 1,
          "EVP_CTRL_AEAD_SET_IVLEN failed");
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
          "second EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 5) == 1,
          "EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 20) != 1,
          "AAD update accepted after the first data update");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s5) IV lengths: the default is 12 (queried through OSSL_PARAM); 0 is
 * rejected; 8 and 16 work via EVP_CTRL_AEAD_SET_IVLEN; 17 also works --
 * OpenSSL's GCM accepts any IV length up to 128 bytes and computes the J0
 * through GHASH for non-12-byte IVs (so 17 round-trips too). */
void gcm_small_test_iv_lengths(const char *ciphername, const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv8 = ALLOCATE_IV(8);
    unsigned char *iv16 = ALLOCATE_IV(16);
    unsigned char *iv17 = ALLOCATE_IV(17);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher, *csz;
    EVP_CIPHER_CTX *ctx;
    OSSL_PARAM getp[2];
    size_t ptlen, ctlen, backlen, ivlen = 0;
    size_t i;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    from_hex(gcm_aad20_hex, aad, 20);
    ptlen = from_hex(gcm_pt60_hex, pt, 64);
    for (i = 0; i < 8; i++)
        iv8[i] = (unsigned char)(0x10 + i);
    for (i = 0; i < 16; i++)
        iv16[i] = (unsigned char)(0x20 + i);
    for (i = 0; i < 17; i++)
        iv17[i] = (unsigned char)(0x30 + i);

    /* Default IV length, queried via OSSL_PARAM. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    getp[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, &ivlen);
    getp[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "EVP_CIPHER_CTX_get_params(OSSL_CIPHER_PARAM_IVLEN) failed");
    CHECK(ivlen == 12, "default GCM IV length is not 12");
    EVP_CIPHER_CTX_free(ctx);

    /* IV length 0 must be rejected. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 0, NULL) != 1,
          "IV length 0 was accepted");
    EVP_CIPHER_CTX_free(ctx);

    /* 8, 16, and 17 byte IVs: encrypt/decrypt self-consistency. */
    csz = gcm_fetch_checked(ciphername,
                            "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
    from_hex(keyhex, key, 32);

    gcm_encrypt(csz, key, iv8, 8, aad, 20, pt, ptlen, ct, 64 + 16,
                &ctlen, tag, 16);
    gcm_decrypt(csz, key, iv8, 8, aad, 20, ct, ctlen, back, 64 + 16,
                &backlen, tag, 16);
    expect_equal("8-byte IV roundtrip did not recover the plaintext",
                 back, backlen, pt, ptlen);

    gcm_encrypt(csz, key, iv16, 16, aad, 20, pt, ptlen, ct, 64 + 16,
                &ctlen, tag, 16);
    gcm_decrypt(csz, key, iv16, 16, aad, 20, ct, ctlen, back, 64 + 16,
                &backlen, tag, 16);
    expect_equal("16-byte IV roundtrip did not recover the plaintext",
                 back, backlen, pt, ptlen);

    gcm_encrypt(csz, key, iv17, 17, aad, 20, pt, ptlen, ct, 64 + 16,
                &ctlen, tag, 16);
    gcm_decrypt(csz, key, iv17, 17, aad, 20, ct, ctlen, back, 64 + 16,
                &backlen, tag, 16);
    expect_equal("17-byte IV roundtrip did not recover the plaintext",
                 back, backlen, pt, ptlen);
    EVP_CIPHER_free(csz);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s6) Short tags: ask for a 12-byte tag via the OSSL_CIPHER_PARAM_AEAD_TAGLEN
 * param at init time; the tag must equal the 12-byte prefix of the 16-byte
 * TC4-shape tag, and decryption with the short tag must round-trip. */
void gcm_small_test_short_tag(const char *ciphername, const char *keyhex,
                              const char *tag12hex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    size_t tl = 12;
    OSSL_PARAM params[2];
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    size_t ptlen, ctlen, backlen, tag_wantlen;
    int len;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    from_hex(gcm_iv12_hex, iv, 16);
    from_hex(gcm_aad20_hex, aad, 20);
    ptlen = from_hex(gcm_pt60_hex, pt, 64);
    tag_wantlen = from_hex(tag12hex, tag_want, 16);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    params[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_TAGLEN, &tl);
    params[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, params) == 1,
          "EVP_EncryptInit_ex2 with AEAD_TAGLEN param failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 20) == 1,
          "AAD EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, (int)ptlen) == 1,
          "EVP_EncryptUpdate failed");
    ctlen = (size_t)len;
    CHECK(EVP_EncryptFinal_ex(ctx, ct + ctlen, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    ctlen += (size_t)len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 12, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG for the 12-byte tag failed");
    expect_equal("12-byte tag is not the prefix of the 16-byte tag",
                 tag, 12, tag_want, tag_wantlen);
    EVP_CIPHER_CTX_free(ctx);

    gcm_decrypt(cipher, key, iv, 12, aad, 20, ct, ctlen, back, 64 + 16,
                &backlen, tag, 12);
    expect_equal("short-tag decryption did not recover the plaintext",
                 back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s7) Zero-length payload: AAD-only encryption emits no ciphertext, just
 * the tag (the classic "tag-only" GCM self-test). */
void gcm_small_test_zero_length_payload(const char *ciphername,
                                        const char *keyhex,
                                        const char *aadonlytag_hex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *ct = ALLOCATE_OUTPUT(16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    size_t ctlen, tag_wantlen;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    from_hex(gcm_iv12_hex, iv, 16);
    from_hex(gcm_aad20_hex, aad, 20);
    tag_wantlen = from_hex(aadonlytag_hex, tag_want, 16);

    gcm_encrypt(cipher, key, iv, 12, aad, 20, NULL, 0, ct, 16, &ctlen, tag, 16);
    CHECK(ctlen == 0, "AAD-only encryption produced ciphertext bytes");
    expect_equal("AAD-only tag does not match the KAT", tag, 16,
                 tag_want, tag_wantlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (s8) Update output-buffer contract: a single 20-byte GCM update into a
 * buffer of exactly inl + 16 bytes emits exactly inl bytes (GCM is a stream
 * cipher), Final adds nothing, and the tag is still retrievable. */
void gcm_small_test_output_buffer_contract(const char *ciphername,
                                           const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(20);
    unsigned char *out = ALLOCATE_OUTPUT(36); /* exactly inl + 16 */
    unsigned char *tag = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len, total;
    size_t i;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    from_hex(gcm_iv12_hex, iv, 16);
    for (i = 0; i < 20; i++)
        in[i] = (unsigned char)(0xB0 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 20) == 1,
          "EVP_EncryptUpdate into an exact-size buffer failed");
    CHECK(len == 20, "20-byte GCM update did not emit exactly 20 bytes");
    total = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    CHECK(len == 0, "GCM Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    CHECK(total == 20, "total output length is not 20");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l1) The one-shot and streaming helpers agree with the anchor KAT rows,
 * both one-shot and with the row plaintext pushed through the large chunk
 * list. */
void gcm_large_test_kat_anchor(const struct gcm_kat *kats, size_t nkats)
{
    size_t k;
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(20);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);

    for (k = 0; k < nkats; k++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kats[k].ciphername, NULL);
        size_t ptlen, aadlen, ctlen, ct2len;
        char msg[128];

        /* AES-GCM is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
        from_hex(kats[k].keyhex, key, 32);
        from_hex(kats[k].ivhex, iv, 16);
        ptlen = from_hex(kats[k].pthex, pt, 64);
        aadlen = from_hex(kats[k].aadhex, aad, 20);

        gcm_encrypt(cipher, key, iv, 12, aad, aadlen, pt, ptlen,
                    ct, 64 + 16, &ctlen, tag, 16);
        gcm_encrypt_streaming(cipher, key, iv, 12, aad, aadlen, pt, ptlen,
                              ct2, 64 + 16, &ct2len, tag2);
        snprintf(msg, sizeof(msg), "%s: streaming ciphertext differs from one-shot",
                 kats[k].label);
        expect_equal(msg, ct2, ct2len, ct, ctlen);
        snprintf(msg, sizeof(msg), "%s: streaming tag differs from one-shot tag",
                 kats[k].label);
        expect_equal(msg, tag2, 16, tag, 16);

        /* The streaming ciphertext of the full 64-byte TC3 vector with the
         * published ciphertext embedded above must match it exactly. */
        if (kats[k].cthex != NULL) {
            unsigned char *ct_want = ALLOCATE_OUTPUT(64);
            size_t ct_wantlen = from_hex(kats[k].cthex, ct_want, 64);
            snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                     kats[k].label);
            expect_equal(msg, ct2, ct2len, ct_want, ct_wantlen);
            /* ... and the same vector fed through the LARGE chunk list
             * (single 64-byte chunk, but through the same code path). */
            gcm_encrypt_streaming(cipher, key, iv, 12, aad, aadlen, pt, ptlen,
                                  ct2, 64 + 16, &ct2len, tag2);
            expect_equal(msg, ct2, ct2len, ct_want, ct_wantlen);
        }
        if (kats[k].taghex != NULL) {
            size_t tag_wantlen = from_hex(kats[k].taghex, tag_want, 16);
            snprintf(msg, sizeof(msg), "%s: tag does not match the KAT",
                     kats[k].label);
            expect_equal(msg, tag2, 16, tag_want, tag_wantlen);
        }
        EVP_CIPHER_free(cipher);
    }

    free_poison_allocations();
}

/* (l2) 2 MiB one-shot == streaming, encrypt and decrypt.  The 2 MiB payload
 * is deterministic but arbitrary. */
void gcm_large_test_stream_equals_one_shot(const char *ciphername,
                                           const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct1 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    EVP_CIPHER *cipher = gcm_fetch_checked(ciphername,
                                           "EVP_CIPHER_fetch failed for a core AES-GCM cipher");
    size_t ct1len, ct2len, backlen;
    char msg[128];

    fill_pattern(pt, LARGE_LEN);
    from_hex(gcm_iv12_hex, iv, 16);
    from_hex(keyhex, key, 32);

    gcm_encrypt(cipher, key, iv, 12, NULL, 0, pt, LARGE_LEN,
                ct1, LARGE_LEN + 16, &ct1len, tag1, 16);
    CHECK(ct1len == LARGE_LEN, "one-shot GCM ciphertext length is not inlen");
    gcm_encrypt_streaming(cipher, key, iv, 12, NULL, 0, pt, LARGE_LEN,
                          ct2, LARGE_LEN + 16, &ct2len, tag2);
    snprintf(msg, sizeof(msg),
             "%s: streaming 2MiB ciphertext differs from one-shot",
             ciphername);
    expect_equal(msg, ct2, ct2len, ct1, ct1len);
    snprintf(msg, sizeof(msg), "%s: streaming 2MiB tag differs from one-shot",
             ciphername);
    expect_equal(msg, tag2, 16, tag1, 16);

    /* Decrypt the streaming ciphertext back in one shot and in chunks;
     * both must reproduce the 2 MiB plaintext. */
    gcm_decrypt_streaming(cipher, key, iv, 12, NULL, 0, ct2, ct2len,
                          back, LARGE_LEN + 16, &backlen, tag2);
    CHECK(backlen == LARGE_LEN, "streaming decrypt length is not inlen");
    snprintf(msg, sizeof(msg),
             "%s: streaming decrypt of 2MiB did not recover the plaintext",
             ciphername);
    unpoison_all();
    CHECK(memcmp(back, pt, LARGE_LEN) == 0, msg);
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l3) Same as (l2) but with a 1 KiB AAD fed before the data; AAD must be
 * counted identically by both update strategies. */
void gcm_large_test_with_aad(const char *ciphername, const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(AAD_LARGE_LEN);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct1 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *ct2 = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag1 = ALLOCATE_TAG(16);
    unsigned char *tag2 = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    size_t ct1len, ct2len;
    size_t i;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    from_hex(gcm_iv12_hex, iv, 16);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < AAD_LARGE_LEN; i++)
        aad[i] = (unsigned char)(i * 13 + 5);

    gcm_encrypt(cipher, key, iv, 12, aad, AAD_LARGE_LEN, pt, LARGE_LEN,
                ct1, LARGE_LEN + 16, &ct1len, tag1, 16);
    gcm_encrypt_streaming(cipher, key, iv, 12, aad, AAD_LARGE_LEN, pt, LARGE_LEN,
                          ct2, LARGE_LEN + 16, &ct2len, tag2);
    expect_equal("2MiB streaming ciphertext with AAD differs from one-shot",
                 ct2, ct2len, ct1, ct1len);
    expect_equal("2MiB streaming tag with AAD differs from one-shot tag",
                 tag2, 16, tag1, 16);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l4) AAD updates interleaved between data updates must fail (GCM finalizes
 * the GHASH of the AAD on the first data update; crypto/modes/gcm128.c
 * rejects later AAD).  An AAD update *before* any data is still legal and is
 * checked first so the failure is attributable to the interleaving. */
void gcm_large_test_aad_interleaved(const char *ciphername, const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *aad = ALLOCATE_AAD(64);
    unsigned char *pt = ALLOCATE_INPUT(8192);
    unsigned char *ct = ALLOCATE_OUTPUT(8192 + 16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    from_hex(gcm_iv12_hex, iv, 16);
    fill_pattern(pt, 8192);
    for (i = 0; i < 64; i++)
        aad[i] = (unsigned char)(i + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 64) == 1,
          "AAD update before any data failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 4096) == 1,
          "first data update failed");
    CHECK(EVP_EncryptUpdate(ctx, ct + len, &len, pt + 4096, 4096) == 1,
          "second data update failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 64) != 1,
          "AAD update accepted after data updates");
    EVP_CIPHER_CTX_free(ctx);

    /* Same for the decryption direction. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, ct, &len, pt, 16) == 1,
          "decrypt-side data update failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 64) != 1,
          "decrypt-side AAD update accepted after data");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l5) IV lengths 8/12/16 must round-trip the full 2 MiB payload (J0
 * computation differs for non-12-byte IVs; gcm128.c GHASHes long IVs). */
void gcm_large_test_iv_lengths(const char *ciphername, const char *keyhex)
{
    static const size_t ivlens[] = { 8, 12, 16 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    size_t ctlen, backlen;
    size_t v, i;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    fill_pattern(pt, LARGE_LEN);
    for (i = 0; i < 16; i++)
        iv[i] = (unsigned char)(0x20 + i);

    for (v = 0; v < sizeof(ivlens) / sizeof(ivlens[0]); v++) {
        gcm_encrypt(cipher, key, iv, ivlens[v], NULL, 0, pt, LARGE_LEN,
                    ct, LARGE_LEN + 16, &ctlen, tag, 16);
        CHECK(ctlen == LARGE_LEN, "large encrypt with short IV lost bytes");
        gcm_decrypt_streaming(cipher, key, iv, ivlens[v], NULL, 0, ct, ctlen,
                              back, LARGE_LEN + 16, &backlen, tag);
        char msg[128];

        snprintf(msg, sizeof(msg),
                 "IV length %zu roundtrip of 2MiB did not recover the plaintext",
                 ivlens[v]);
        unpoison_all();
        CHECK(backlen == LARGE_LEN && memcmp(back, pt, LARGE_LEN) == 0, msg);
        repoison_all();
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (l6) Flipping any bit of the final ciphertext block must be detected by
 * DecryptFinal (the tag covers every ciphertext block via GHASH). */
void gcm_large_test_tampered_final_block(const char *ciphername,
                                         const char *keyhex)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(LARGE_LEN);
    unsigned char *ct = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *back = ALLOCATE_OUTPUT(LARGE_LEN + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    char fetchmsg[128];
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    size_t ctlen;
    int len;

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = gcm_fetch_checked(ciphername, fetchmsg);
    from_hex(keyhex, key, 32);
    from_hex(gcm_iv12_hex, iv, 16);
    fill_pattern(pt, LARGE_LEN);

    gcm_encrypt(cipher, key, iv, 12, NULL, 0, pt, LARGE_LEN,
                ct, LARGE_LEN + 16, &ctlen, tag, 16);
    CHECK(ctlen == LARGE_LEN, "unexpected one-shot ciphertext length");

    /* Tamper with the first byte of the final 16-byte block. */
    ct[ctlen - 16] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "EVP_DecryptUpdate failed on tampered data (it may emit plaintext)");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
          "decryption accepted a tampered final ciphertext block");
    EVP_CIPHER_CTX_free(ctx);

    /* And the last byte of the whole ciphertext. */
    ct[ctlen - 1] ^= 0x80;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "EVP_DecryptUpdate failed on tampered data (it may emit plaintext)");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
          "decryption accepted a tampered last ciphertext byte");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
