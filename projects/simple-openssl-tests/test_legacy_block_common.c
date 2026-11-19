/*
 * test_legacy_block_common.c -- shared implementation of the legacy block
 * cipher family tests (API in test_legacy_block_common.h; one binary per
 * cipher, one struct legacy_block_row per binary: CAST5, Blowfish, IDEA,
 * SEED and RC2 in their ECB/CBC/OFB/CFB variants, plus RC2-40-CBC and
 * RC2-64-CBC).
 *
 * Every row is {cipher name, key, IV, plaintext, ciphertext} and gets a
 * KAT one-shot, a roundtrip, and one streaming check (the {5,3,8,2} grid)
 * per direction.  All hex constants were verified against the
 * authoritative pizfix OpenSSL 3.6.5 CLI with the legacy provider enabled,
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -cast5-ecb \
 *       -nopad -provider legacy -provider default -K <keyhex> -iv <ivhex>
 *
 * (each per-cipher binary spells out the command for its own cipher) and
 * cross-checked against the system OpenSSL 3.0.13 CLI (identical values,
 * except IDEA which the system 3.0.13 build does not offer at all -- the
 * IDEA binaries print the family SKIP line there; the IDEA constants come
 * from the 3.6.5 oracle alone).  RC2-40-CBC uses a fixed 5-byte key and
 * RC2-64-CBC a fixed 8-byte key (EVP_CIPHER_get_key_length; note RC2-64
 * is 8 bytes = 64 bits, not 6) -- the row runner CHECKs the fetched
 * cipher's key length against the row's key, which pins both.
 *
 * The legacy provider is loaded EXPLICITLY together with the default
 * provider: loading legacy alone would disable default-provider
 * auto-load, and a row whose cipher is missing still needs the default
 * provider for nothing here but keeps the environment uniform.
 *
 * Version gates: whole-program.  A cipher that the build lacks makes its
 * OWN binary skip via legacy_block_probe_or_skip() right after
 * test_parse_args(); on a build that has the cipher the sections run and
 * a NULL fetch in the runner is a broken build, not a version gate.
 */

#include "test_legacy_block_common.h"

#include <openssl/evp.h>

/* Shared KAT material.  The constants are arrays (not pointers) so that
 * they are address constants usable in the static struct legacy_block_row
 * initializers of the per-cipher binaries. */

/* The default 16-byte key and 8-byte IV shared by most rows. */
const char legacy_block_key16_hex[] =
    "0123456789abcdeffedcba9876543210";
const char legacy_block_iv8_hex[] = "0102030405060708";
/* SEED's IV is the full 16-byte block. */
const char legacy_block_iv16_hex[] =
    "01020304050607080102030405060708";
const char legacy_block_pt16_hex[] =
    "0123456789abcdef0123456789abcdef";
const char legacy_block_pt32_hex[] =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

/* Loading the legacy provider disables default-provider auto-load, so both
 * are loaded explicitly (every cipher of this family lives in legacy). */
void legacy_block_load_providers(void)
{
    test_load_provider("legacy");
    test_load_provider("default");
}

/* Whole-program version gate: SKIP with the documented reason when the
 * build lacks the cipher, release the probe handle otherwise. */
void legacy_block_probe_or_skip(const char *ciphername)
{
    char msg[128];
    EVP_CIPHER *probe = EVP_CIPHER_fetch(NULL, ciphername, NULL);

    if (probe == NULL) {
        snprintf(msg, sizeof(msg), "%s is unavailable", ciphername);
        test_skip(msg);
    }
    EVP_CIPHER_free(probe);
}

/* Fetches the cipher named by a row, CHECK()ing success (the version gate
 * ran in main(); a NULL fetch here is a broken build, not a gate). */
static EVP_CIPHER *row_fetch(const struct legacy_block_row *row)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             row->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, row->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot encrypt/decrypt with padding disabled.  Returns the total
 * output length written to `out` (room for inlen + block_size per the EVP
 * contract). */
static size_t row_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                        const unsigned char *iv, const unsigned char *in,
                        size_t inlen, unsigned char *out, size_t outcap,
                        int enc)
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

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated); padding disabled. */
static size_t row_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* The data-driven sweep for one row: KAT one-shot, roundtrip, and one
 * streaming equality with the {5, 3, 8, 2} grid, per direction. */
void legacy_block_test_row(const struct legacy_block_row *row)
{
    static const size_t chunks[] = { 5, 3, 8, 2 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(32);
    unsigned char *ct_want = ALLOCATE_OUTPUT(32);
    unsigned char *ct = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(32 + 32);
    EVP_CIPHER *cipher = row_fetch(row);
    size_t keylen, ptlen, ct_wantlen, ctlen, backlen;
    char msg[128];

    keylen = from_hex(row->keyhex, key, 32);
    snprintf(msg, sizeof(msg),
             "fetched %s cipher has an unexpected key length",
             row->ciphername);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen, msg);
    from_hex(row->ivhex, iv, 16);
    ptlen = from_hex(row->pthex, pt, 32);
    ct_wantlen = from_hex(row->cthex, ct_want, 32);

    /* KAT one-shot (ECB rows take no IV). */
    ctlen = row_crypt(cipher, key, row->ivhex[0] ? iv : NULL, pt, ptlen,
                      ct, 32 + 16, 1);
    snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
             row->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    /* Roundtrip. */
    backlen = row_crypt(cipher, key, row->ivhex[0] ? iv : NULL, ct_want,
                        ct_wantlen, pt_back, 32 + 32, 0);
    snprintf(msg, sizeof(msg), "%s roundtrip did not recover the plaintext",
             row->ciphername);
    expect_equal(msg, pt_back, backlen, pt, ptlen);

    /* Streaming equality, both directions. */
    ctlen = row_chunked(cipher, key, row->ivhex[0] ? iv : NULL, pt, ptlen,
                        ct, 32 + 16, 1, chunks, 4);
    snprintf(msg, sizeof(msg),
             "%s streaming ciphertext does not match the one-shot "
             "ciphertext", row->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);
    backlen = row_chunked(cipher, key, row->ivhex[0] ? iv : NULL, ct_want,
                          ct_wantlen, pt_back, 32 + 32, 0, chunks, 4);
    snprintf(msg, sizeof(msg),
             "%s streaming decryption did not recover the plaintext",
             row->ciphername);
    expect_equal(msg, pt_back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
