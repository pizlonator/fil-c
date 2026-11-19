/*
 * test_rc4_common.c -- shared implementation of the RC4 family tests (API
 * in test_rc4_common.h; one binary per cipher: test_rc4.c, test_rc4_40.c,
 * plus the large-data regime binary test_rc4_large.c).
 *
 * Known-answer vectors: the three classic RC4 test vectors
 *
 *   key "Key"    (4b6579)          plaintext "Plaintext"      (9 bytes)
 *   key "Wiki"   (57696b69)        plaintext "pedia"          (5 bytes)
 *   key "Secret" (536563726574)    plaintext "Attack at dawn" (14 bytes)
 *
 * and the RC4-40 vector (fixed 5-byte key) live in the per-cipher
 * binaries.  They need SHORT keys, which the provider route reaches
 * through EVP_CIPHER_CTX_set_key_length() between a bare init and the
 * key init (the RC4 cipher carries the VARIABLE_LENGTH flag; `openssl
 * enc -K` always zero-pads to 16 bytes and cannot produce these
 * vectors).  The constants were verified with a scratch probe compiled
 * by build/bin/filcc against the pizfix 3.6.5 build and cross-checked
 * against the system 3.0.13 (identical).  The RC4-40 vector came from
 * `openssl enc -rc4-40 -provider legacy -provider default -K <5-byte
 * keyhex>` on both builds.
 *
 * Behavior: RC4 state (the i, j indices and the 256-byte permutation) is
 * carried across updates by the legacy provider's RC4 implementation, so
 * any chunk split must reproduce the one-shot output exactly; the bulk
 * path in rc4_enc.c processes 8 bytes at a time (with the i/j state
 * advanced 8 per step) before a tail of single-byte steps, which the
 * 8-byte-boundary sections exercise.  The >= 64 KiB regime leans on the
 * same state continuity: a long run of uneven updates must reproduce the
 * one-shot keystream exactly, and that equality is the oracle there (RC4
 * has no per-size KATs beyond the classic small vectors in the per-cipher
 * binaries).
 *
 * Providers: RC4 and RC4-40 live in the legacy provider; loading it
 * explicitly turns OFF default-provider auto-load, so rc4_load_providers()
 * loads both, keeping the environment uniform.
 *
 * Version note: a NULL fetch after rc4_load_providers() is a broken
 * build, not a version gate -- the RC4-family binaries never SKIP.
 */

#include "test_rc4_common.h"

#include <openssl/evp.h>

/* The 16-byte key the shared behavior sections run under.  The constant
 * is an array (not a pointer) so that it is an address constant. */
const char rc4_key16_hex[] = "00112233445566778899aabbccddeeff";

/* Loading the legacy provider disables default-provider auto-load, so both
 * are loaded explicitly (RC4 and RC4-40 live in the legacy provider). */
void rc4_load_providers(void)
{
    test_load_provider("legacy");
    test_load_provider("default");
}

/* Fetches an RC4-family cipher, CHECK()ing success (a NULL fetch after
 * rc4_load_providers() is a broken build, not a version gate). */
EVP_CIPHER *rc4_fetch(const char *ciphername)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* RC4-encrypts/decrypts `inlen` bytes one-shot with an explicit key length
 * (the key length is re-set between a bare init and the key init when it
 * differs from the default 16).  Returns the output length. */
size_t rc4_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                 size_t keylen, const unsigned char *in, size_t inlen,
                 unsigned char *out, size_t outcap, int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    if (keylen != 16) {
        CHECK(EVP_CIPHER_CTX_set_key_length(ctx, (int)keylen) == 1,
              "EVP_CIPHER_CTX_set_key_length failed");
        g_checks++;
    }
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL) == 1,
              "key EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, NULL) == 1,
              "key EVP_DecryptInit_ex failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (inlen)
        CHECK(EVP_CipherUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "EVP_CipherUpdate failed");
    CHECK(EVP_CipherFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_CipherFinal_ex failed");
    CHECK(flen == 0, "RC4 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated); same variable-key-length route as rc4_crypt(). */
size_t rc4_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
                   size_t keylen, const unsigned char *in, size_t inlen,
                   unsigned char *out, size_t outcap, int enc,
                   const size_t *chunks, size_t nchunks)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
    int len = 0, total = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    if (keylen != 16) {
        CHECK(EVP_CIPHER_CTX_set_key_length(ctx, (int)keylen) == 1,
              "EVP_CIPHER_CTX_set_key_length failed");
        g_checks++;
    }
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL) == 1,
              "key EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, NULL) == 1,
              "key EVP_DecryptInit_ex failed");
    CHECK(inlen <= outcap,
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
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &flen) == 1,
          "EVP_CipherFinal_ex failed in streaming test");
    total += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* (1) The classic KAT table: each row's one-shot ciphertext matches
 * through the variable-key-length route and decrypting the KAT ciphertext
 * (RC4 is symmetric) recovers the plaintext. */
void rc4_test_kats(const struct rc4_kat *kats, size_t nkats)
{
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *ct_want = ALLOCATE_OUTPUT(16);
    unsigned char *ct = ALLOCATE_OUTPUT(16);
    unsigned char *back = ALLOCATE_OUTPUT(16);
    size_t k, keylen, ptlen, ct_wantlen, ctlen, backlen;
    char msg[128];

    for (k = 0; k < nkats; k++) {
        const struct rc4_kat *kat = &kats[k];
        EVP_CIPHER *cipher = rc4_fetch("RC4");

        keylen = from_hex(kat->keyhex, key, 16);
        ptlen = from_hex(kat->pthex, pt, 16);
        ct_wantlen = from_hex(kat->cthex, ct_want, 16);

        ctlen = rc4_crypt(cipher, key, keylen, pt, ptlen, ct, 16, 1);
        snprintf(msg, sizeof(msg),
                 "classic RC4 KAT %zu: ciphertext does not match", k);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        backlen = rc4_crypt(cipher, key, keylen, ct_want, ct_wantlen, back,
                            16, 0);
        snprintf(msg, sizeof(msg), "classic RC4 KAT %zu: decrypt mismatch", k);
        expect_equal(msg, back, backlen, pt, ptlen);
        EVP_CIPHER_free(cipher);
    }

    free_poison_allocations();
}

/* (2) One-shot sizes 0..24 under keyhex at keylen: every output length
 * equals its input length and the 8-byte-boundary lengths roundtrip. */
void rc4_test_sizes(const char *ciphername, const char *keyhex, size_t keylen)
{
    EVP_CIPHER *cipher = rc4_fetch(ciphername);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(24);
    unsigned char *ct = ALLOCATE_OUTPUT(24);
    unsigned char *back = ALLOCATE_OUTPUT(24);
    size_t n, i, ctlen, backlen;
    char msg[128];

    from_hex(keyhex, key, 16);
    for (i = 0; i < 24; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    for (n = 0; n <= 24; n++) {
        ctlen = rc4_crypt(cipher, key, keylen, in, n, ct, 24, 1);
        snprintf(msg, sizeof(msg),
                 "%s one-shot encryption of %zu bytes gave %zu bytes",
                 ciphername, n, ctlen);
        CHECK(ctlen == n, msg);
        if (n % 8 == 0 || n == 1) {
            backlen = rc4_crypt(cipher, key, keylen, ct, ctlen, back, 24, 0);
            snprintf(msg, sizeof(msg), "the %zu-byte RC4 roundtrip failed", n);
            expect_equal(msg, back, backlen, in, n);
        }
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) Streaming {1,1,1,1,5} == one-shot: the i/j state carries across
 * updates, including the 8-byte bulk path boundary. */
void rc4_test_streaming(const char *ciphername, const char *keyhex,
                        size_t keylen)
{
    static const size_t chunks[] = { 1, 1, 1, 1, 5 };
    EVP_CIPHER *cipher = rc4_fetch(ciphername);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(24);
    unsigned char *oneshot = ALLOCATE_OUTPUT(24);
    unsigned char *chunked = ALLOCATE_OUTPUT(24);
    size_t n, olen, clen;
    char msg[128];

    from_hex(keyhex, key, 16);
    for (n = 0; n < 24; n++)
        in[n] = (unsigned char)(n * 3 + 7);

    for (n = 9; n <= 24; n++) {
        olen = rc4_crypt(cipher, key, keylen, in, n, oneshot, 24, 1);
        clen = rc4_chunked(cipher, key, keylen, in, n, chunked, 24, 1,
                           chunks, 5);
        snprintf(msg, sizeof(msg),
                 "the {1,1,1,1,5} chunked ciphertext differs from one-shot "
                 "at %zu bytes", n);
        expect_equal(msg, chunked, clen, oneshot, olen);
        clen = rc4_chunked(cipher, key, keylen, oneshot, olen, chunked, 24, 0,
                           chunks, 5);
        snprintf(msg, sizeof(msg),
                 "the {1,1,1,1,5} chunked decrypt did not recover the "
                 "plaintext at %zu bytes", n);
        expect_equal(msg, chunked, clen, in, n);
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) 8-byte-boundary behavior: chunk grids that straddle the bulk path's
 * 8-at-a-time steps (e.g. 7+1+8, 8+8, 15+1+7) must equal one-shot for
 * lengths around every multiple of 8. */
void rc4_test_eight_byte_boundaries(const char *ciphername,
                                    const char *keyhex, size_t keylen)
{
    static const size_t grids[][3] = {
        { 7, 1, 8 }, { 8, 8, 5 }, { 15, 1, 7 }, { 9, 7, 5 }
    };
    EVP_CIPHER *cipher = rc4_fetch(ciphername);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32);
    unsigned char *chunked = ALLOCATE_OUTPUT(32);
    static const size_t lens[] = { 7, 8, 9, 15, 16, 17, 23, 24 };
    size_t g, l, olen, clen;
    char msg[128];

    from_hex(keyhex, key, 16);
    for (l = 0; l < 32; l++)
        in[l] = (unsigned char)(l * 5 + 11);

    for (l = 0; l < sizeof(lens) / sizeof(lens[0]); l++) {
        olen = rc4_crypt(cipher, key, keylen, in, lens[l], oneshot, 32, 1);
        for (g = 0; g < sizeof(grids) / sizeof(grids[0]); g++) {
            clen = rc4_chunked(cipher, key, keylen, in, lens[l], chunked, 32,
                               1, grids[g], 3);
            snprintf(msg, sizeof(msg),
                     "the {%zu,%zu,%zu} grid differs from one-shot at %zu "
                     "bytes", grids[g][0], grids[g][1], grids[g][2], lens[l]);
            expect_equal(msg, chunked, clen, oneshot, olen);
        }
    }
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) Empty updates are no-ops (RC4 state untouched): a 0-byte update
 * between two real updates must not perturb the keystream. */
void rc4_test_empty_update(const char *ciphername, const char *keyhex,
                           size_t keylen)
{
    EVP_CIPHER *cipher = rc4_fetch(ciphername);
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *oneshot = ALLOCATE_OUTPUT(16);
    unsigned char *with_empty = ALLOCATE_OUTPUT(16);
    EVP_CIPHER_CTX *ctx;
    size_t i, olen;
    int len, flen;
    char msg[128];

    from_hex(keyhex, key, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0xC0 - i);

    olen = rc4_crypt(cipher, key, keylen, in, 16, oneshot, 16, 1);

    /* 8 bytes, an empty update, then the remaining 8. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, with_empty, &len, in, 8) == 1,
          "the first 8-byte update failed");
    CHECK(len == 8, "the first 8-byte update did not emit 8 bytes");
    CHECK(EVP_EncryptUpdate(ctx, with_empty + 8, &len, in + 8, 0) == 1,
          "the empty update failed");
    CHECK(len == 0, "the empty update emitted bytes");
    CHECK(EVP_EncryptUpdate(ctx, with_empty + 8, &len, in + 8, 8) == 1,
          "the second 8-byte update failed");
    CHECK(len == 8, "the second 8-byte update did not emit 8 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, with_empty + 16, &flen) == 1,
          "the final after the empty update failed");
    CHECK(flen == 0, "RC4 Final emitted bytes after the empty update");
    EVP_CIPHER_CTX_free(ctx);
    snprintf(msg, sizeof(msg), "an empty update perturbed the %s keystream",
             ciphername);
    expect_equal(msg, with_empty, 16, oneshot, olen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* ---- The large-data regime (test_rc4_large's sections). ---- */

#define RC4_LARGE_SIZE (64u * 1024u) /* 64 KiB */

/* One-shot RC4 with the default 16-byte key; returns the output length. */
static size_t rc4_large_oneshot(const EVP_CIPHER *cipher,
                                const unsigned char *key,
                                const unsigned char *in, size_t inlen,
                                unsigned char *out, size_t outcap, int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
              "EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
              "EVP_DecryptInit_ex failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (inlen)
        CHECK(EVP_CipherUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "one-shot EVP_CipherUpdate failed");
    CHECK(EVP_CipherFinal_ex(ctx, out + len, &flen) == 1,
          "one-shot EVP_CipherFinal_ex failed");
    CHECK(flen == 0, "RC4 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated).  Returns the output length and the update count. */
static size_t rc4_large_chunked(const EVP_CIPHER *cipher,
                                const unsigned char *key,
                                const unsigned char *in, size_t inlen,
                                unsigned char *out, size_t outcap, int enc,
                                const size_t *chunks, size_t nchunks,
                                size_t *updates)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0, ups = 0;
    int len = 0, total = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
              "EVP_EncryptInit_ex failed");
    else
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
              "EVP_DecryptInit_ex failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % nchunks];

        if (n > inlen - off)
            n = inlen - off;
        CHECK(EVP_CipherUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
              "EVP_CipherUpdate failed in the chunked run");
        total += len;
        off += n;
        ups++;
    }
    CHECK(EVP_CipherFinal_ex(ctx, out + total, &flen) == 1,
          "EVP_CipherFinal_ex failed in the chunked run");
    total += flen;
    *updates = ups;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* (6) 64 KiB through the odd grid {1000, 1, 64, 8} == one-shot, in both
 * directions, with 200+ updates proving the state continuity. */
void rc4_large_test_64k(void)
{
    EVP_CIPHER *cipher = rc4_fetch("RC4");
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(RC4_LARGE_SIZE);
    unsigned char *oneshot = ALLOCATE_OUTPUT(RC4_LARGE_SIZE);
    unsigned char *chunked = ALLOCATE_OUTPUT(RC4_LARGE_SIZE);
    unsigned char *back = ALLOCATE_OUTPUT(RC4_LARGE_SIZE);
    static const size_t chunks[] = { 1000, 1, 64, 8 };
    size_t i, olen, clen, backlen, ups;

    from_hex(rc4_key16_hex, key, 16);
    for (i = 0; i < RC4_LARGE_SIZE; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    olen = rc4_large_oneshot(cipher, key, in, RC4_LARGE_SIZE, oneshot,
                             RC4_LARGE_SIZE, 1);
    CHECK(olen == RC4_LARGE_SIZE,
          "the 64 KiB one-shot output length is wrong");

    clen = rc4_large_chunked(cipher, key, in, RC4_LARGE_SIZE, chunked,
                             RC4_LARGE_SIZE, 1, chunks,
                             sizeof(chunks) / sizeof(chunks[0]), &ups);
    CHECK(ups > 200, "too few updates to exercise state continuity");
    expect_equal("the 64 KiB chunked ciphertext differs from the one-shot "
                 "ciphertext", chunked, clen, oneshot, olen);

    backlen = rc4_large_chunked(cipher, key, oneshot, olen, back,
                                RC4_LARGE_SIZE, 0, chunks,
                                sizeof(chunks) / sizeof(chunks[0]), &ups);
    expect_equal("the 64 KiB chunked decrypt did not recover the plaintext",
                 back, backlen, in, RC4_LARGE_SIZE);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (7) 128 KiB one-shot roundtrip and a byte-at-a-time decrypt of the
 * first 64 bytes (state continuity across the 8-byte bulk boundary). */
void rc4_large_test_128k_and_bytewise(void)
{
    EVP_CIPHER *cipher = rc4_fetch("RC4");
    unsigned char *key = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(2 * RC4_LARGE_SIZE);
    unsigned char *ct = ALLOCATE_OUTPUT(2 * RC4_LARGE_SIZE);
    unsigned char *back = ALLOCATE_OUTPUT(2 * RC4_LARGE_SIZE);
    EVP_CIPHER_CTX *ctx;
    size_t i, ctlen;
    int len;

    from_hex("fedcba9876543210fedcba9876543210", key, 16);
    for (i = 0; i < 2 * RC4_LARGE_SIZE; i++)
        in[i] = (unsigned char)(i * 13 + 5);

    ctlen = rc4_large_oneshot(cipher, key, in, 2 * RC4_LARGE_SIZE, ct,
                              2 * RC4_LARGE_SIZE, 1);
    CHECK(ctlen == 2 * RC4_LARGE_SIZE,
          "the 128 KiB one-shot output length is wrong");
    rc4_large_oneshot(cipher, key, ct, ctlen, back, 2 * RC4_LARGE_SIZE, 0);
    unpoison_all();
    CHECK(memcmp(back, in, 2 * RC4_LARGE_SIZE) == 0,
          "the 128 KiB RC4 roundtrip failed");
    repoison_all();
    g_checks++;

    /* Byte-at-a-time decrypt of the first 64 bytes of the ciphertext. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, key, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    for (i = 0; i < 64; i++) {
        CHECK(EVP_DecryptUpdate(ctx, back + i, &len, ct + i, 1) == 1,
              "the byte-at-a-time RC4 decrypt update failed");
        CHECK(len == 1, "a 1-byte RC4 update did not emit 1 byte");
    }
    EVP_CIPHER_CTX_free(ctx);
    unpoison_all();
    CHECK(memcmp(back, in, 64) == 0,
          "the byte-at-a-time RC4 decrypt of the first 64 bytes failed");
    repoison_all();
    g_checks++;
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}