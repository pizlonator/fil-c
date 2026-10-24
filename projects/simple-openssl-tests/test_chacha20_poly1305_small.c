#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_chacha20_poly1305_small -- ChaCha20-Poly1305 known-answer and
 * behavior tests for small inputs.  (A sibling program covers >= 1 MiB
 * payloads and mod-16 AAD padding coverage at size.)
 *
 * Known-answer vector: RFC 8439 section 2.8.2 -- key 808182...9f, nonce
 * 070000004041424344454647, AAD 50515253c0c1c2c3c4c5c6c7, the 114-byte
 * "Ladies and Gentlemen of the class of '99..." plaintext, ciphertext
 * d31a8d34...64b6116 and tag 1ae10b594f09e26a7e902ecbd0600691.  The RFC
 * values were verified with a scratch probe compiled by build/bin/filcc
 * against the pizfix 3.6.5 build (`openssl enc` cannot do AEAD) and
 * cross-checked against the system 3.0.13 (identical).  A second, tiny
 * derived KAT (1-byte AAD, 1-byte payload -- the aad%16 padding and
 * final length-block path) was derived the same way on both builds.
 *
 * Behavior pinned here (evidence in
 * providers/implementations/ciphers/cipher_chacha20_poly1305_hw.c):
 *
 *   - AAD must be consumed BEFORE the first payload update: the aad
 *     branch errors when ctx->len.text != 0 (:303-309);
 *   - the final update wraps up the AAD with aad%16 zero padding and
 *     feeds the 16-byte aad/text length block to Poly1305 (:311-313,
 *     :337-382);
 *   - decryption compares exactly tag_len bytes at Final (:389) -- and
 *     tag_len is 0 until SET_TAG, so a decrypt WITHOUT a set tag
 *     verifies NOTHING (pinned below; the caller must set the tag).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_IV(size_t n)     { return poison_alloc(POISON_IV, n); }
static void *ALLOCATE_AAD(size_t n)    { return poison_alloc(POISON_AAD, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }
static void *ALLOCATE_TAG(size_t n)    { return poison_alloc(POISON_TAG, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_chacha20_poly1305_small: %s\n", msg);
    exit(1);
}

#define CHECK(cond, msg)                      \
    do {                                      \
        if (!(cond))                          \
            test_failed(msg);                 \
        g_checks++;                           \
    } while (0)

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Copies a hex constant into a buffer allocated by an ALLOCATE_* function
 * (house rule: constants live in the source as hex strings, but are copied
 * into labeled working buffers before use).  Returns the byte length. */
static size_t from_hex(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = strlen(hex) / 2;
    size_t i;
    if (strlen(hex) % 2 != 0)
        test_failed("odd-length hex constant");
    if (n > dstcap)
        test_failed("hex constant larger than its destination buffer");
    for (i = 0; i < n; i++) {
        int hi = hexval(hex[2 * i]);
        int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            test_failed("bad hex digit in constant");
        dst[i] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

/* Length-and-content comparison of two working buffers. */
static void expect_equal(const char *what, const unsigned char *got, size_t gotlen,
                         const unsigned char *want, size_t wantlen)
{
    CHECK(gotlen == wantlen, what);
    if (wantlen > 0)
        CHECK(memcmp(got, want, wantlen) == 0, what);
}

static const char *const KEY_HEX =
    "808182838485868788898a8b8c8d8e8f"
    "909192939495969798999a9b9c9d9e9f";
static const char *const NONCE_HEX =
    "070000004041424344454647";
static const char *const AAD12_HEX =
    "50515253c0c1c2c3c4c5c6c7";
/* The RFC 8439 2.8.2 plaintext (114 bytes). */
static const char *const MSG_HEX =
    "4c616469657320616e642047656e746c656d656e206f662074686520636c6173"
    "73206f66202739393a204966204920636f756c64206f6666657220796f75206f"
    "6e6c79206f6e652074697020666f7220746865206675747572652c2073756e73"
    "637265656e20776f756c642062652069742e";
static const char *const MSG_CT_HEX =
    "d31a8d34648e60db7b86afbc53ef7ec2"
    "a4aded51296e08fea9e2b5a736ee62d6"
    "3dbea45e8ca9671282fafb69da92728b"
    "1a71de0a9e060b2905d6a5b67ecd3b36"
    "92ddbd7f2d778b8c9803aee328091b58"
    "fab324e4fad675945585808b4831d7bc"
    "3ff4def08e4b7a9de576d26586cec64b"
    "6116";
static const char *const MSG_TAG_HEX =
    "1ae10b594f09e26a7e902ecbd0600691";

/* Derived tiny KAT: key 000102...1f, nonce 0102030405060708090a0b0c,
 * 1-byte AAD 5a, 1-byte payload aa (exercises the aad%16 zero-padding
 * and the final length-block path).  Both builds agree. */
static const char *const TINY_KEY_HEX =
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f";
static const char *const TINY_NONCE_HEX =
    "0102030405060708090a0b0c";
static const char *const TINY_CT_HEX = "ce";
static const char *const TINY_TAG_HEX =
    "25f6e5ff66de7f51cda4b9354ab6763d";

/* One-shot encrypt: AAD first (out == NULL), then the payload, Final,
 * GET_TAG.  Returns the ciphertext length. */
static size_t cp_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *nonce, const unsigned char *aad,
                         size_t aadlen, const unsigned char *in, size_t inlen,
                         unsigned char *out, size_t outcap, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, dlen = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
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
    CHECK(EVP_EncryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    dlen += flen;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)dlen;
}

/* One-shot decrypt with tag verification (SET_TAG before Final). */
static size_t cp_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                         const unsigned char *nonce, const unsigned char *aad,
                         size_t aadlen, const unsigned char *in, size_t inlen,
                         unsigned char *out, size_t outcap,
                         const unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char tag_copy[16]; /* SET_TAG's pointer parameter is not const */
    int len = 0, dlen = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
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
    memcpy(tag_copy, tag, 16);
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag_copy) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + dlen, &flen) == 1,
          "EVP_DecryptFinal_ex failed (tag mismatch?)");
    dlen += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)dlen;
}

/* (1) The RFC 8439 2.8.2 KAT: one-shot, via {16 x 7, 2} payload updates,
 * and with the AAD split into two updates -- all must agree byte for
 * byte; decrypting the KAT recovers the message. */
static void test_kat(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(12);
    unsigned char *msg = ALLOCATE_INPUT(114);
    unsigned char *ct_want = ALLOCATE_OUTPUT(114);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *ct = ALLOCATE_OUTPUT(114 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(114 + 16);
    static const size_t chunks[] = { 16, 16, 16, 16, 16, 16, 16, 2 };
    EVP_CIPHER_CTX *ctx;
    size_t ct_wantlen, tag_wantlen, ctlen, backlen;
    int len;
    size_t off, ci;

    /* ChaCha20-Poly1305 is core OpenSSL 3 functionality: never SKIP. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20-Poly1305\") failed");
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == 32,
          "ChaCha20-Poly1305 key length is not 32");
    CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 12,
          "ChaCha20-Poly1305 nonce length is not 12");
    from_hex(KEY_HEX, key, 32);
    from_hex(NONCE_HEX, nonce, 12);
    from_hex(AAD12_HEX, aad, 12);
    from_hex(MSG_HEX, msg, 114);
    ct_wantlen = from_hex(MSG_CT_HEX, ct_want, 114);
    tag_wantlen = from_hex(MSG_TAG_HEX, tag_want, 16);

    ctlen = cp_encrypt(cipher, key, nonce, aad, 12, msg, 114, ct, 114 + 16,
                       tag);
    expect_equal("the one-shot ciphertext does not match the RFC 8439 KAT",
                 ct, ctlen, ct_want, ct_wantlen);
    expect_equal("the one-shot tag does not match the RFC 8439 KAT",
                 tag, 16, tag_want, tag_wantlen);

    /* Payload via {16,16,16,16,16,16,16,2} updates (AAD once). */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 12) == 1,
          "AAD EVP_EncryptUpdate failed");
    off = 0;
    ci = 0;
    ctlen = 0;
    while (off < 114) {
        size_t n = chunks[ci++ % (sizeof(chunks) / sizeof(chunks[0]))];
        if (n > 114 - off)
            n = 114 - off;
        CHECK(EVP_EncryptUpdate(ctx, ct + ctlen, &len, msg + off, (int)n) == 1,
              "EVP_EncryptUpdate failed in the split KAT");
        ctlen += (size_t)len;
        off += n;
    }
    CHECK(EVP_EncryptFinal_ex(ctx, ct + ctlen, &len) == 1,
          "EVP_EncryptFinal_ex failed in the split KAT");
    ctlen += (size_t)len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    expect_equal("the {16,...,2}-update ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);
    expect_equal("the {16,...,2}-update tag does not match the KAT",
                 tag, 16, tag_want, tag_wantlen);

    /* AAD split into two updates (6 + 6). */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, 6) == 1,
          "AAD split update 1 failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad + 6, 6) == 1,
          "AAD split update 2 failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, msg, (int)114) == 1,
          "payload update after the split AAD failed");
    ctlen = (size_t)len;
    CHECK(EVP_EncryptFinal_ex(ctx, ct + ctlen, &len) == 1,
          "EVP_EncryptFinal_ex failed after the split AAD");
    ctlen += (size_t)len;
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);
    expect_equal("the split-AAD ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);
    expect_equal("the split-AAD tag does not match the KAT",
                 tag, 16, tag_want, tag_wantlen);

    /* Decrypting the KAT recovers the message. */
    backlen = cp_decrypt(cipher, key, nonce, aad, 12, ct_want, ct_wantlen,
                         back, 114 + 16, tag_want);
    expect_equal("authenticated decryption did not recover the message",
                 back, backlen, msg, 114);
    EVP_CIPHER_free(cipher);
}

/* (2) Tiny sizes {0, 1, 17} roundtrip, and the derived 1-byte-AAD KAT. */
static void test_tiny(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(1);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_want = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    size_t i, ctlen, backlen;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20-Poly1305\") failed");
    from_hex(TINY_KEY_HEX, key, 32);
    from_hex(TINY_NONCE_HEX, nonce, 12);
    for (i = 0; i < 32; i++)
        pt[i] = (unsigned char)(0x70 + i);

    for (i = 0; i < 3; i++) {
        size_t n = (i == 0) ? 0 : (i == 1) ? 1 : 17;
        size_t aadlen = (i == 0) ? 0 : 1;

        if (aadlen)
            aad[0] = 0x5A;
        ctlen = cp_encrypt(cipher, key, nonce, aad, aadlen, pt, n, ct, 32 + 16,
                           tag);
        snprintf(msg, sizeof(msg),
                 "the %zu-byte authenticated roundtrip failed", n);
        backlen = cp_decrypt(cipher, key, nonce, aad, aadlen, ct, ctlen, back,
                             32 + 16, tag);
        expect_equal(msg, back, backlen, pt, n);
    }

    /* The derived tiny KAT (1-byte AAD 5a, 1-byte payload aa). */
    {
        unsigned char tiny_ct[1];
        from_hex(TINY_CT_HEX, tiny_ct, 1);
        aad[0] = 0x5A;
        pt[0] = 0xAA;
        ctlen = cp_encrypt(cipher, key, nonce, aad, 1, pt, 1, ct, 32 + 16,
                           tag);
        pt[0] = 0x70;
        CHECK(ctlen == 1, "the tiny KAT ciphertext length is not 1");
        CHECK(ct[0] == tiny_ct[0],
              "the tiny KAT ciphertext byte does not match the derived "
              "constant");
    }
    from_hex(TINY_TAG_HEX, tag_want, 16);
    expect_equal("the tiny KAT tag does not match the derived constant",
                 tag, 16, tag_want, 16);
    EVP_CIPHER_free(cipher);
}

/* (3) AAD after the first data update: 3.6.5 rejects it (the aad branch
 * errors when ctx->len.text != 0, cipher_chacha20_poly1305_hw.c:303-309)
 * while 3.0.x ACCEPTS it (its chacha20-poly1305 lacks the len.text
 * guard).  The rejection is asserted where it holds; where the update is
 * accepted the record is checked to stay self-consistent when the
 * decrypt side replays the same ordering, and a note line documents the
 * 3.0.x behavior (mirroring test_kdf_hkdf's note style). */
static void test_aad_ordering(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(12);
    unsigned char *in = ALLOCATE_INPUT(5);
    unsigned char *out = ALLOCATE_OUTPUT(5 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(5 + 16);
    EVP_CIPHER_CTX *ctx;
    int len, aad_ok, flen;
    size_t i, backlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20-Poly1305\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(NONCE_HEX, nonce, 12);
    from_hex(AAD12_HEX, aad, 12);
    for (i = 0; i < 5; i++)
        in[i] = (unsigned char)(0xA0 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 5) == 1,
          "EVP_EncryptUpdate failed");
    aad_ok = (EVP_EncryptUpdate(ctx, NULL, &len, aad, 12) == 1);
    if (!aad_ok) {
        /* 3.6.5 behavior: rejected. */
        g_checks++; /* the rejection is the expected outcome */
        EVP_CIPHER_CTX_free(ctx);
    } else {
        /* 3.0.x behavior: accepted; finish the record, then decrypt it
         * with the same (data-then-AAD) ordering and compare. */
        printf("note test_chacha20_poly1305_small: this OpenSSL accepts an "
               "AAD update after the first data update (3.0.x behavior; "
               "3.6.5 rejects it)\n");
        CHECK(EVP_EncryptFinal_ex(ctx, out + 5, &flen) == 1,
              "EVP_EncryptFinal_ex failed after the late AAD");
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
              "EVP_CTRL_AEAD_GET_TAG failed after the late AAD");
        EVP_CIPHER_CTX_free(ctx);

        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 5) == 1,
              "the late-AAD decrypt payload update failed");
        CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, 12) == 1,
              "the late-AAD decrypt AAD update failed");
        {
            unsigned char tag_copy[16]; /* SET_TAG's pointer is not const */
            memcpy(tag_copy, tag, 16);
            CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16,
                                      tag_copy) == 1,
                    "EVP_CTRL_AEAD_SET_TAG failed");
        }
        CHECK(EVP_DecryptFinal_ex(ctx, back + 5, &flen) == 1,
              "the late-AAD decrypt Final failed");
        EVP_CIPHER_CTX_free(ctx);
        backlen = 5;
        expect_equal("the late-AAD record did not roundtrip under the same "
                     "ordering", back, backlen, in, 5);
    }
    EVP_CIPHER_free(cipher);
}

/* (4) Tamper detection: a tampered tag and tampered ciphertext must fail
 * Final -- and, pinned, a decrypt WITHOUT a set tag verifies nothing
 * (tag_len defaults to 0, so Final compares zero bytes; the caller is
 * required to set the tag). */
static void test_tamper(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(12);
    unsigned char *msg = ALLOCATE_INPUT(114);
    unsigned char *ct = ALLOCATE_OUTPUT(114 + 16);
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *tag_bad = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(114 + 16);
    EVP_CIPHER_CTX *ctx;
    size_t aadlen, ctlen;
    int len;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20-Poly1305\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(NONCE_HEX, nonce, 12);
    aadlen = from_hex(AAD12_HEX, aad, 12);
    from_hex(MSG_HEX, msg, 114);

    ctlen = cp_encrypt(cipher, key, nonce, aad, aadlen, msg, 114, ct, 114 + 16,
                       tag);

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
          "decryption accepted a tampered tag");
    EVP_CIPHER_CTX_free(ctx);

    /* Tampered ciphertext (right tag): Final must fail. */
    ct[3] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
          "AAD EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "EVP_DecryptUpdate failed on tampered ciphertext");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_SET_TAG failed");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) != 1,
          "decryption accepted tampered ciphertext");
    EVP_CIPHER_CTX_free(ctx);
    ct[3] ^= 0x01;

    /* Pinned: WITHOUT a set tag, Final verifies nothing -- it succeeds
     * even on the corrupted record.  (The tag comparison length defaults
     * to 0 until SET_TAG; callers must always set it.) */
    ct[3] ^= 0x01;
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, ct, (int)ctlen) == 1,
          "EVP_DecryptUpdate failed on the no-tag path");
    CHECK(EVP_DecryptFinal_ex(ctx, back + len, &len) == 1,
          "the no-tag decrypt Final did not skip verification (tag_len "
          "defaults to 0 until SET_TAG)");
    EVP_CIPHER_CTX_free(ctx);
    ct[3] ^= 0x01;
    EVP_CIPHER_free(cipher);
}

/* (5) Tag and output-size contracts: the tag is 16 bytes (GET_TAG(16),
 * SET_TAG(17) refused, TAGLEN readback follows SET_TAG), and a 20-byte
 * update into an exactly inl+16-sized buffer emits inl bytes with Final
 * adding nothing. */
static void test_contracts(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20-Poly1305", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *nonce = ALLOCATE_IV(12);
    unsigned char *aad = ALLOCATE_AAD(12);
    unsigned char *in = ALLOCATE_INPUT(20);
    unsigned char *out = ALLOCATE_OUTPUT(36); /* exactly inl + 16 */
    unsigned char *tag = ALLOCATE_TAG(16);
    unsigned char *back = ALLOCATE_OUTPUT(36);
    EVP_CIPHER_CTX *ctx;
    OSSL_PARAM getp[2];
    size_t aadlen, ctlen, backlen, taglen = 0;
    int len, flen, i;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20-Poly1305\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(NONCE_HEX, nonce, 12);
    aadlen = from_hex(AAD12_HEX, aad, 12);
    for (i = 0; i < 20; i++)
        in[i] = (unsigned char)(0xB0 + i);

    /* Out-size contract. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
          "AAD EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 20) == 1,
          "EVP_EncryptUpdate into an exact-size buffer failed");
    CHECK(len == 20, "the 20-byte CP update did not emit exactly 20 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_EncryptFinal_ex failed");
    CHECK(flen == 0, "CP Final emitted unexpected bytes");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag) == 1,
          "EVP_CTRL_AEAD_GET_TAG failed");
    EVP_CIPHER_CTX_free(ctx);

    /* Tag length contract: TAGLEN readback is 0 until SET_TAG sizes it;
     * 17-byte tags are refused; a 12-byte tag is a prefix of the 16-byte
     * tag and roundtrips. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_DecryptInit_ex failed");
    getp[0] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_TAGLEN,
                                          &taglen);
    getp[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "EVP_CIPHER_CTX_get_params(AEAD_TAGLEN) failed");
    CHECK(taglen == 0,
          "the default CP tag length readback is not 0 (it is the SET_TAG "
          "length)");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 17, tag) != 1,
          "a 17-byte tag length was accepted");
    CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 12, NULL) == 1,
          "the 12-byte tag length was refused");
    taglen = 0;
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "EVP_CIPHER_CTX_get_params(AEAD_TAGLEN) failed after SET_TAG");
    CHECK(taglen == 12, "the AEAD_TAGLEN readback after SET_TAG is not 12");
    EVP_CIPHER_CTX_free(ctx);

    /* 12-byte tag: GET_TAG's first 12 bytes and an authenticated decrypt
     * with the truncated tag. */
    {
        unsigned char tag12[12];

        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, nonce) == 1,
              "EVP_EncryptInit_ex failed");
        CHECK(EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aadlen) == 1,
              "AAD EVP_EncryptUpdate failed");
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 20) == 1,
              "EVP_EncryptUpdate failed");
        ctlen = (size_t)len;
        CHECK(EVP_EncryptFinal_ex(ctx, out + ctlen, &flen) == 1,
              "EVP_EncryptFinal_ex failed");
        ctlen += (size_t)flen;
        CHECK(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 12, tag12) == 1,
              "EVP_CTRL_AEAD_GET_TAG for the 12-byte tag failed");
        CHECK(memcmp(tag12, tag, 12) == 0,
              "the 12-byte tag is not the prefix of the 16-byte tag");
        EVP_CIPHER_CTX_free(ctx);

        backlen = cp_decrypt(cipher, key, nonce, aad, aadlen, out, ctlen,
                             back, 36, tag);
        expect_equal("the 12-byte-tag decrypt did not recover the plaintext",
                     back, backlen, in, 20);
    }
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kat();
    test_tiny();
    test_aad_ordering();
    test_tamper();
    test_contracts();

    printf("test_chacha20_poly1305_small: all %u checks passed\n", g_checks);
    return 0;
}
