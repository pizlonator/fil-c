/*

 * test_des_common.c -- shared implementation of the single-DES family
 * tests (API in test_des_common.h; one binary per cipher, one struct
 * des_kat row per binary: DES-ECB/CBC/OFB/CFB/CFB1/CFB8, DESX-CBC, and
 * the two-key DES-EDE family in ECB/CBC/CFB/OFB).

 * Providers: the DES-* ciphers (and DESX-CBC) come from the LEGACY
 * provider, while the DES-EDE ciphers come from the default provider.
 * Loading the legacy provider explicitly turns OFF default-provider
 * auto-load, so both providers are loaded (des_load_providers, called by
 * every binary of the family right after test_parse_args(); see the
 * "Behavioral notes" section of README.md).

 * Known-answer vectors: verified against the authoritative pizfix
 * OpenSSL 3.6.5 CLI with the legacy provider enabled,

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ecb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>

 * and cross-checked against the system OpenSSL 3.0.13 CLI (identical).
 * Note `openssl enc -desx-cbc` also works with both providers loaded.
 * The two-key DES-EDE stream modes were verified the same way, with the
 * default provider alone supplying the cipher and no -nopad needed
 * (CFB/OFB emit one byte per input byte, so `enc` adds no padding):

 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -des-ede-cfb \
 *       -K <keyhex> -iv <ivhex>      (likewise -des-ede-ofb)

 * The family-wide constants (the single-DES key 0123456789abcdef, the IV
 * 0102030405060708, the 8/16/32-byte plaintexts, and the two-key EDE key
 * 0123456789abcdef23456789abcdef01) live in the arrays below; every
 * per-cipher constant (the DESX key/ciphertext, the EDE stream-mode
 * ciphertexts, and every row's KAT ciphertext) lives in that cipher's
 * binary.

 * 8-byte-block semantics: a nopad operation needs an exact multiple of 8;
 * padded operations roundtrip odd sizes; CBC decryption holds the last
 * block until Final; CFB8 runs one bit-byte per 8 input bits; CFB1 runs
 * in byte mode by default and stays in byte mode even through the
 * EVP_CIPH_FLAG_LENGTH_BITS route documented in test_aes_cfb1 (the DES
 * provider's CFB1 hardware hook has NO use_bits branch -- unlike the
 * generic ossl_cipher_hw_generic_cfb1 used by AES -- so the update length
 * remains a BYTE count; see des_test_cfb1_byte_mode below).  The EDE
 * stream modes DES-EDE-CFB/DES-EDE-OFB are 8-byte-block CFB64/OFB with a
 * 16-byte (two-key) key and an 8-byte IV; EVP reports block size 1 for
 * them, so every update streams its input through byte-for-byte and the
 * shift register plus num offset must carry across updates.  The
 * DESX-CBC 24-byte key is [8-byte DES key][8-byte input whitening]
 * [8-byte output whitening] (cipher_desx_hw.c: DES_set_key_unchecked on
 * key[0..7], ks2 = key[8..15], ks3 = key[16..23]); the composition check
 * (des_test_desx_composition, run by the DESX-CBC binary) rebuilds DESX
 * from per-block DES-ECB calls exactly as DES_xcbc_encrypt chains them
 * (chain_0 = IV, chain_i = the WHITENED previous ciphertext).
 */

#include "test_des_common.h"

#include <openssl/evp.h>

/* Shared KAT material.  The constants are arrays (not pointers) so that
 * they are address constants usable in the static struct des_kat
 * initializers of the per-cipher binaries. */

/* The single-DES 8-byte key and the family-wide 8-byte IV. */
const char des_key_hex[] =
    "0123456789abcdef";
const char des_iv_hex[] =
    "0102030405060708";

/* The family plaintexts: 8 bytes (the ECB rows), 16 bytes (the CBC rows),
 * and 32 bytes (the stream-mode rows). */
const char des_pt8_hex[] =
    "4142434445464748";
const char des_pt16_hex[] =
    "0123456789abcdef0123456789abcdef";
const char des_pt32_hex[] =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

/* The two-key DES-EDE 16-byte key. */
const char des_ede_key_hex[] =
    "0123456789abcdef23456789abcdef01";

/* Loading the legacy provider disables default-provider auto-load, so
 * both are loaded explicitly (DES-* here lives in legacy, the DES-EDE
 * family in default). */
void des_load_providers(void)
{
    test_load_provider("legacy");
    test_load_provider("default");
}

/* Fetches the cipher named by a KAT row, CHECK()ing success.  The legacy
 * provider is loaded by des_load_providers(); a NULL fetch here is a
 * broken build, not a version gate. */
static EVP_CIPHER *des_fetch_cipher(const struct des_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* Converts a row's IV constant into a labeled working buffer.  "" rows
 * stay NULL: ECB takes no IV (the original test passed NULL there). */
static const unsigned char *row_iv(const struct des_kat *kat,
                                   unsigned char *ivbuf)
{
    if (kat->ivhex[0] == '\0')
        return NULL;
    from_hex(kat->ivhex, ivbuf, 8);
    return ivbuf;
}

/* One-shot encrypt/decrypt with padding disabled.  Returns the total
 * output length written to `out` (room for inlen + block_size per the EVP
 * contract). */
size_t des_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 const unsigned char *iv, const unsigned char *in,
                 size_t inlen, unsigned char *out, size_t outcap, int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(inlen + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_CipherUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_CipherUpdate failed");
    total = len;
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &len) == 1,
          "EVP_CipherFinal_ex failed");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* Encrypt/decrypt with an explicit padding flag (mode-agnostic: the ECB
 * rows pass a NULL IV). */
size_t des_cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                     const unsigned char *iv, const unsigned char *in,
                     size_t inlen, unsigned char *out, size_t outcap,
                     int enc, int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, padding) == 1,
          "EVP_CIPHER_CTX_set_padding failed");
    CHECK(inlen + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_CipherUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_CipherUpdate failed");
    total = len;
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &len) == 1,
          "EVP_CipherFinal_ex failed");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated); padding disabled.  A split must reproduce the
 * one-shot result exactly. */
size_t des_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap,
                  int enc, const size_t *chunks, size_t nchunks)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
    int len = 0, total = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(inlen + 2 * (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % nchunks];

        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_CipherUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
              "EVP_CipherUpdate failed in streaming test");
        total += len;
        off += n;
    }
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &len) == 1,
          "EVP_CipherFinal_ex failed in streaming test");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* (1) EVP_CIPHER_fetch(ciphername): the row's key length, the one-shot
 * KAT, and decrypting the KAT ciphertext back to its plaintext. */
void des_test_kat(const struct des_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(32 + 16);
    const unsigned char *ivp;
    EVP_CIPHER *cipher;
    size_t ct_wantlen, ctlen, ptlen;
    char msg[128];

    cipher = des_fetch_cipher(kat);
    snprintf(msg, sizeof(msg), "fetched %s cipher has an unexpected key length",
             kat->ciphername);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) ==
              strlen(kat->keyhex) / 2,
          msg);
    from_hex(kat->keyhex, key, 24);
    ivp = row_iv(kat, iv);
    from_hex(kat->pthex, pt, 32);
    ct_wantlen = from_hex(kat->cthex, ct_want, 32);

    ctlen = des_crypt(cipher, key, ivp, pt, kat->ptlen, ct, 32 + 16, 1);
    snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    ptlen = des_crypt(cipher, key, ivp, ct_want, ct_wantlen, pt_back,
                      32 + 16, 0);
    snprintf(msg, sizeof(msg),
             "%s decrypting the KAT ciphertext did not give the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, ptlen, pt, kat->ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) 8-byte-block semantics: nopad exact-multiple roundtrip, padded
 * roundtrips of odd sizes, streaming {1,7,8,9} equality, and the 0-byte
 * no-op. */
void des_test_block_semantics(const struct des_kat *kat)
{
    static const size_t stream_chunks[] = { 1, 7, 8, 9 };
    static const size_t pad_sizes[] = { 1, 7, 8, 9, 33 };
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 24);
    unsigned char *pt_back = ALLOCATE_OUTPUT(33 + 24);
    unsigned char *oneshot = ALLOCATE_OUTPUT(33 + 24);
    const unsigned char *ivp;
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    size_t s, i, ctlen, backlen, olen;
    int len, flen;
    char msg[128];

    cipher = des_fetch_cipher(kat);
    from_hex(kat->keyhex, key, 24);
    ivp = row_iv(kat, iv);
    for (i = 0; i < 33; i++)
        pt[i] = (unsigned char)(i * 3 + 7);

    /* nopad: 16 bytes roundtrip as an exact multiple of 8. */
    ctlen = des_cbc_crypt(cipher, key, ivp, pt, 16, ct, 33 + 24, 1, 0);
    snprintf(msg, sizeof(msg), "nopad %s encryption of 16 bytes != 16 bytes",
             kat->ciphername);
    CHECK(ctlen == 16, msg);
    backlen = des_cbc_crypt(cipher, key, ivp, ct, 16, pt_back, 33 + 24, 0, 0);
    snprintf(msg, sizeof(msg), "nopad %s roundtrip failed", kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, 16);

    /* pad: odd sizes roundtrip (33 bytes -> 40 with a full pad block). */
    for (s = 0; s < sizeof(pad_sizes) / sizeof(pad_sizes[0]); s++) {
        size_t n = pad_sizes[s];
        size_t padded = (n / 8 + 1) * 8;

        ctlen = des_cbc_crypt(cipher, key, ivp, pt, n, ct, 33 + 24, 1, 1);
        snprintf(msg, sizeof(msg),
                 "padded %s encryption of %zu bytes gave %zu, not %zu",
                 kat->ciphername, n, ctlen, padded);
        CHECK(ctlen == padded, msg);
        backlen = des_cbc_crypt(cipher, key, ivp, ct, ctlen, pt_back,
                                33 + 24, 0, 1);
        snprintf(msg, sizeof(msg), "padded %s %zu-byte roundtrip failed",
                 kat->ciphername, n);
        expect_equal(msg, pt_back, backlen, pt, n);
    }

    /* Streaming {1,7,8,9}: equals one-shot in both directions (nopad). */
    olen = des_cbc_crypt(cipher, key, ivp, pt, 16, oneshot, 33 + 24, 1, 0);
    ctlen = des_stream(cipher, key, ivp, pt, 16, ct, 33 + 24, 1,
                       stream_chunks, 4);
    snprintf(msg, sizeof(msg),
             "streaming %s ciphertext does not match one-shot",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, oneshot, olen);
    backlen = des_stream(cipher, key, ivp, oneshot, olen, pt_back,
                         33 + 24, 0, stream_chunks, 4);
    snprintf(msg, sizeof(msg),
             "streaming %s decryption did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, 16);

    /* 0-byte no-op: an empty update succeeds, emits nothing, Final too. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, ivp, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    snprintf(msg, sizeof(msg), "0-byte %s update failed", kat->ciphername);
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 0) == 1, msg);
    snprintf(msg, sizeof(msg), "0-byte %s update emitted bytes",
             kat->ciphername);
    CHECK(len == 0, msg);
    snprintf(msg, sizeof(msg), "0-byte nopad %s final failed",
             kat->ciphername);
    CHECK(EVP_EncryptFinal_ex(ctx, ct, &flen) == 1, msg);
    snprintf(msg, sizeof(msg), "0-byte nopad %s final emitted bytes",
             kat->ciphername);
    CHECK(flen == 0, msg);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) Stream-mode details: partial updates (the chunk grid passed by the
 * caller) and additionally a 0-byte no-op; the stream modes report block
 * size 1, so every update streams its input through byte-for-byte. */
void des_test_stream_modes(const struct des_kat *kat)
{
    static const size_t stream_chunks[] = { 1, 7, 8, 9 };
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *streamed = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    const unsigned char *ivp;
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    size_t ct_wantlen, olen, slen, backlen;
    int len = 0, flen = 0;
    char msg[128];

    cipher = des_fetch_cipher(kat);
    snprintf(msg, sizeof(msg),
             "%s stream-mode key length is not %zu (see test_des_common.c "
             "for the key layout)", kat->ciphername, strlen(kat->keyhex) / 2);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) ==
              strlen(kat->keyhex) / 2, msg);
    snprintf(msg, sizeof(msg),
             "%s does not report the stream-mode block size 1",
             kat->ciphername);
    CHECK(EVP_CIPHER_get_block_size(cipher) == 1, msg);
    from_hex(kat->keyhex, key, 24);
    ivp = row_iv(kat, iv);
    from_hex(kat->pthex, pt, 32);
    ct_wantlen = from_hex(kat->cthex, ct_want, 32);

    /* One-shot encrypt == KAT. */
    olen = des_crypt(cipher, key, ivp, pt, 32, oneshot, 32 + 16, 1);
    snprintf(msg, sizeof(msg),
             "%s one-shot ciphertext does not match the KAT", kat->ciphername);
    expect_equal(msg, oneshot, olen, ct_want, ct_wantlen);

    /* Streaming {1,7,8,9}: equals one-shot in both directions. */
    slen = des_stream(cipher, key, ivp, pt, 32, streamed, 32 + 16, 1,
                      stream_chunks, 4);
    snprintf(msg, sizeof(msg),
             "%s streaming ciphertext does not match one-shot",
             kat->ciphername);
    expect_equal(msg, streamed, slen, oneshot, olen);
    backlen = des_stream(cipher, key, ivp, oneshot, olen, back,
                         32 + 16, 0, stream_chunks, 4);
    snprintf(msg, sizeof(msg),
             "%s streaming decryption did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, back, backlen, pt, 32);

    /* 0-byte update: a no-op that neither emits nor disturbs the shift
     * register -- the full update after it still gives the one-shot
     * ciphertext, and Final emits nothing. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, ivp, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    snprintf(msg, sizeof(msg), "0-byte %s stream-mode update failed",
             kat->ciphername);
    CHECK(EVP_EncryptUpdate(ctx, streamed, &len, pt, 0) == 1, msg);
    snprintf(msg, sizeof(msg), "0-byte %s stream-mode update emitted bytes",
             kat->ciphername);
    CHECK(len == 0, msg);
    snprintf(msg, sizeof(msg),
             "%s stream-mode update after the 0-byte update failed",
             kat->ciphername);
    CHECK(EVP_EncryptUpdate(ctx, streamed, &len, pt, 32) == 1, msg);
    snprintf(msg, sizeof(msg),
             "%s stream-mode update did not emit all 32 bytes",
             kat->ciphername);
    CHECK(len == 32, msg);
    snprintf(msg, sizeof(msg), "%s stream-mode final failed",
             kat->ciphername);
    CHECK(EVP_EncryptFinal_ex(ctx, streamed + len, &flen) == 1, msg);
    snprintf(msg, sizeof(msg), "%s stream-mode final emitted bytes",
             kat->ciphername);
    CHECK(flen == 0, msg);
    EVP_CIPHER_CTX_free(ctx);
    snprintf(msg, sizeof(msg),
             "%s ciphertext after a 0-byte update differs from one-shot",
             kat->ciphername);
    expect_equal(msg, streamed, (size_t)len, oneshot, olen);

    /* Roundtrip: one-shot decrypt of the one-shot ciphertext. */
    backlen = des_crypt(cipher, key, ivp, oneshot, olen, back, 32 + 16, 0);
    snprintf(msg, sizeof(msg), "%s roundtrip did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, back, backlen, pt, 32);

    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) CFB8: byte-at-a-time updates must reproduce the one-shot ciphertext
 * (the 8-bit shift register carries the byte boundary), for both
 * directions. */
void des_test_cfb8_bytewise(const struct des_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *bybyte = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    const unsigned char *ivp;
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    size_t i, olen, blen;
    int len;
    char msg[128];

    cipher = des_fetch_cipher(kat);
    from_hex(kat->keyhex, key, 24);
    ivp = row_iv(kat, iv);
    from_hex(kat->pthex, pt, 32);

    olen = des_crypt(cipher, key, ivp, pt, 32, oneshot, 32 + 16, 1);

    {
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, ivp, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
        CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
              "EVP_CIPHER_CTX_set_padding(0) failed");
        for (i = 0; i < 32; i++) {
            snprintf(msg, sizeof(msg),
                     "byte-at-a-time %s update failed", kat->ciphername);
            CHECK(EVP_EncryptUpdate(ctx, bybyte + i, &len, pt + i, 1) == 1,
                  msg);
            snprintf(msg, sizeof(msg),
                     "%s 1-byte update did not emit 1 byte", kat->ciphername);
            CHECK(len == 1, msg);
        }
        snprintf(msg, sizeof(msg), "byte-at-a-time %s final failed",
                 kat->ciphername);
        CHECK(EVP_EncryptFinal_ex(ctx, bybyte + 32, &len) == 1, msg);
        EVP_CIPHER_CTX_free(ctx);
    }
    snprintf(msg, sizeof(msg),
             "byte-at-a-time %s differs from the one-shot ciphertext",
             kat->ciphername);
    expect_equal(msg, bybyte, 32, oneshot, olen);

    /* And decrypting byte-at-a-time recovers the plaintext. */
    {
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, ivp, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
        CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
              "EVP_CIPHER_CTX_set_padding(0) failed");
        for (i = 0; i < 32; i++) {
            snprintf(msg, sizeof(msg),
                     "byte-at-a-time %s decrypt update failed",
                     kat->ciphername);
            CHECK(EVP_DecryptUpdate(ctx, back + i, &len, oneshot + i, 1) == 1,
                  msg);
        }
        snprintf(msg, sizeof(msg), "byte-at-a-time %s decrypt final failed",
                 kat->ciphername);
        CHECK(EVP_DecryptFinal_ex(ctx, back + 32, &len) == 1, msg);
        EVP_CIPHER_CTX_free(ctx);
    }
    blen = 32;
    snprintf(msg, sizeof(msg),
             "byte-at-a-time %s decrypt did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, back, blen, pt, 32);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) CFB1 byte mode, plus what EVP_CIPH_FLAG_LENGTH_BITS does for DES:
 * the flag is accepted (forwarded as "use-bits" to the provider), but the
 * DES provider's CFB1 hardware hook (cipher_des_hw.c
 * cipher_hw_des_cfb1_cipher) has NO use_bits branch -- unlike the generic
 * ossl_cipher_hw_generic_cfb1 (ciphercommon_hw.c:87) used by AES -- so
 * the update length remains a BYTE count and 8 "flagged" bytes produce
 * exactly the byte-mode result.  (This was pinned after a Fil-C run
 * trapped the buffer overread a *bit*-count update would cause: the
 * flagged-but-ignored path would read inl bytes for inl bits.  The test
 * therefore asserts the safe, real contract: flag set or not, inl is a
 * byte count and the outputs agree.) */
void des_test_cfb1_byte_mode(const struct des_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(8);
    unsigned char *byte_mode = ALLOCATE_OUTPUT(8 + 16);
    unsigned char *flag_mode = ALLOCATE_OUTPUT(8 + 16);
    const unsigned char *ivp;
    EVP_CIPHER *cipher;
    EVP_CIPHER_CTX *ctx;
    int len, i;
    char msg[128];

    cipher = des_fetch_cipher(kat);
    from_hex(kat->keyhex, key, 24);
    ivp = row_iv(kat, iv);
    for (i = 0; i < 8; i++)
        pt[i] = (unsigned char)(0x60 + i);

    /* Byte mode: 8 bytes = 64 one-bit CFB steps. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, ivp, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, byte_mode, &len, pt, 8) == 1,
          "byte-mode CFB1 update failed");
    CHECK(len == 8, "byte-mode CFB1 update did not emit 8 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, byte_mode + len, &len) == 1,
          "byte-mode CFB1 final failed");
    EVP_CIPHER_CTX_free(ctx);

    /* Flagged context (fresh, two-phase init per test_aes_cfb1): the same
     * 8-byte update must give the same 8 bytes, because the DES provider
     * ignores use-bits. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
    g_checks++; /* set_flags() reports no status; the checks below prove
                 * the flag was accepted but has no effect on DES. */
    snprintf(msg, sizeof(msg), "flagged key EVP_EncryptInit_ex failed for %s",
             kat->ciphername);
    CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, ivp) == 1, msg);
    snprintf(msg, sizeof(msg), "flagged CFB1 update failed for %s",
             kat->ciphername);
    CHECK(EVP_EncryptUpdate(ctx, flag_mode, &len, pt, 8) == 1, msg);
    CHECK(len == 8,
          "flagged CFB1 update did not emit 8 bytes (inl is a byte count)");
    snprintf(msg, sizeof(msg), "flagged CFB1 final failed for %s",
             kat->ciphername);
    CHECK(EVP_EncryptFinal_ex(ctx, flag_mode + 8, &len) == 1, msg);
    CHECK(len == 0, "flagged CFB1 final emitted bytes");
    snprintf(msg, sizeof(msg),
             "flagged CFB1 differs from byte mode (%s ignores use-bits)",
             kat->ciphername);
    expect_equal(msg, flag_mode, 8, byte_mode, 8);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (6) DESX-CBC: KAT, roundtrip, and the per-block composition
 * ct_i = DES-ECB(pt_i ^ in_w ^ chain_i) ^ out_w with chain_0 = IV and
 * chain_i = the WHITENED previous ciphertext (DES_xcbc_encrypt's chaining
 * order, pinned by byte-level experiments and the cipher_desx_hw.c key
 * layout: ks1 = key[0..7], in-whitening = key[8..15], out-whitening =
 * key[16..23]). */
void des_test_desx_composition(const struct des_kat *kat)
{
    unsigned char *key = ALLOCATE_KEY(24);
    unsigned char *iv = ALLOCATE_IV(8);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *composed = ALLOCATE_OUTPUT(16);
    unsigned char *back = ALLOCATE_OUTPUT(16 + 16);
    unsigned char blk[8], chain[8];
    const unsigned char *ivp;
    EVP_CIPHER *dx, *ecb;
    size_t i, b, ct_wantlen, ctlen, backlen;
    int len;
    char msg[128], fetchmsg[128];

    snprintf(fetchmsg, sizeof(fetchmsg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    dx = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(dx != NULL, fetchmsg);
    ecb = EVP_CIPHER_fetch(NULL, "DES-ECB", NULL);
    CHECK(ecb != NULL, "EVP_CIPHER_fetch(\"DES-ECB\") failed");
    snprintf(msg, sizeof(msg), "%s key length is not 24", kat->ciphername);
    CHECK((size_t)EVP_CIPHER_get_key_length(dx) == 24, msg);
    from_hex(kat->keyhex, key, 24);
    ivp = row_iv(kat, iv);
    from_hex(kat->pthex, pt, 16);
    ct_wantlen = from_hex(kat->cthex, ct_want, 16);

    ctlen = des_crypt(dx, key, ivp, pt, 16, ct, 16 + 16, 1);
    snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    /* Composition from per-block DES-ECB calls. */
    memcpy(chain, ivp, 8);
    for (b = 0; b < 2; b++) {
        EVP_CIPHER_CTX *e = EVP_CIPHER_CTX_new();

        CHECK(e != NULL, "EVP_CIPHER_CTX_new failed");
        for (i = 0; i < 8; i++)
            blk[i] = (unsigned char)(pt[b * 8 + i] ^ key[8 + i] ^ chain[i]);
        CHECK(EVP_EncryptInit_ex2(e, ecb, key, NULL, NULL) == 1,
              "composition DES-ECB init failed");
        CHECK(EVP_CIPHER_CTX_set_padding(e, 0) == 1,
              "composition DES-ECB set_padding failed");
        CHECK(EVP_EncryptUpdate(e, composed + b * 8, &len, blk, 8) == 1,
              "composition DES-ECB update failed");
        CHECK(len == 8, "composition DES-ECB update length mismatch");
        CHECK(EVP_EncryptFinal_ex(e, composed + b * 8 + len, &len) == 1,
              "composition DES-ECB final failed");
        EVP_CIPHER_CTX_free(e);
        for (i = 0; i < 8; i++) {
            composed[b * 8 + i] ^= key[16 + i];
            chain[i] = composed[b * 8 + i];
        }
    }
    snprintf(msg, sizeof(msg),
             "the %s composition differs from the stitched output",
             kat->ciphername);
    expect_equal(msg, composed, 16, ct, ctlen);

    backlen = des_crypt(dx, key, ivp, ct_want, ct_wantlen, back, 16 + 16, 0);
    snprintf(msg, sizeof(msg), "%s roundtrip did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, back, backlen, pt, 16);
    EVP_CIPHER_free(dx);
    EVP_CIPHER_free(ecb);

    free_poison_allocations();
}
