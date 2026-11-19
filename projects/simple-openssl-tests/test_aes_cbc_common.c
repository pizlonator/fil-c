/*
 * test_aes_cbc_common.c -- shared implementation of the AES-CBC family tests
 * (API in test_aes_cbc_common.h; one binary per cipher, one struct cbc_kat
 * row per binary).
 *
 * Known-answer vectors: NIST SP 800-38A, sections F.2.1 (AES-128), F.2.5
 * (AES-192), and F.2.9 (AES-256).  All three use the same 4-block plaintext
 * and the same IV (000102..0f; the shared constants below); each binary
 * carries its own key and ciphertext.  The hex was verified against the
 * authoritative build with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-cbc -nopad -K <key> -iv <iv>
 *
 * (and likewise for -aes-192-cbc and -aes-256-cbc; the per-binary provenance
 * notes say which row is which).  The vectors are block-aligned, so every
 * KAT comparison runs with padding disabled; padding itself gets its own
 * section (cbc_test_padding).
 */

#include "test_aes_cbc_common.h"

#include <openssl/evp.h>

/* Shared SP 800-38A F.2 material: the same 4-block plaintext and IV for all
 * three key sizes.  The constants are arrays (not pointers) so that they are
 * address constants usable in static initializers of the per-cipher
 * binaries. */
const char cbc_kat_pt_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
const char cbc_kat_iv_hex[] =
    "000102030405060708090a0b0c0d0e0f";

/* Fetches the cipher named by a KAT row, CHECK()ing success (AES-CBC is core
 * OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *cbc_fetch_cipher(const struct cbc_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot CBC encrypt.  Encrypts `inlen` bytes in a single
 * EVP_EncryptUpdate call and returns the total ciphertext length written to
 * `out` (which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract; the helper CHECKs that). */
size_t cbc_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *iv, const unsigned char *in,
                   size_t inlen, unsigned char *out, size_t outcap,
                   int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, padding) == 1,
          "EVP_CIPHER_CTX_set_padding failed");
    CHECK(inlen + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_EncryptUpdate failed");
    total = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_EncryptFinal_ex failed");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* One-shot CBC decrypt; mirror image of cbc_encrypt(). */
size_t cbc_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *iv, const unsigned char *in,
                   size_t inlen, unsigned char *out, size_t outcap,
                   int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, padding) == 1,
          "EVP_CIPHER_CTX_set_padding failed");
    CHECK(inlen + (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "EVP_DecryptUpdate failed");
    total = len;
    CHECK(EVP_DecryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_DecryptFinal_ex failed");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (the last chunk is truncated to what remains).  Returns the total output
 * length.  Splits must reproduce the one-shot result exactly, which is only
 * possible if every intermediate buffer size honors the EVP contract. */
size_t cbc_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *iv, const unsigned char *in,
                  size_t inlen, unsigned char *out, size_t outcap,
                  int enc, int padding)
{
    static const size_t chunks[] = { 1, 15, 16, 17, 31 };
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
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, padding) == 1,
          "EVP_CIPHER_CTX_set_padding failed");
    CHECK(inlen + 2 * (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % (sizeof(chunks) / sizeof(chunks[0]))];
        if (n > inlen - off)
            n = inlen - off;
        if (enc)
            CHECK(EVP_EncryptUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
                  "EVP_EncryptUpdate failed in streaming test");
        else
            CHECK(EVP_DecryptUpdate(ctx, out + total, &len, in + off, (int)n) == 1,
                  "EVP_DecryptUpdate failed in streaming test");
        total += len;
        off += n;
    }
    if (enc)
        CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
              "EVP_EncryptFinal_ex failed in streaming test");
    else
        CHECK(EVP_DecryptFinal_ex(ctx, out + total, &len) == 1,
              "EVP_DecryptFinal_ex failed in streaming test");
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)total;
}

/* Builds "EVP_aes_128_cbc()" out of the by-name spelling "aes-128-cbc" for
 * the CHECK message texts of the legacy static route (any uppercase letters
 * in the by-name spelling are lowercased, so "AES-192-ECB"-style spellings
 * map to the real function name too). */
static void legacy_route_name(char *dst, size_t dstcap, const char *byname)
{
    size_t i, n = strlen(byname);

    CHECK(n + 6 <= dstcap, "route-name buffer too small");
    memcpy(dst, "EVP_", 4);
    for (i = 0; i < n; i++) {
        char c = byname[i];

        if (c == '-')
            c = '_';
        else if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        dst[4 + i] = c;
    }
    memcpy(dst + 4 + n, "()", 3);
}

/* (1) EVP_CIPHER_fetch(name) one-shot encrypt == KAT, and decrypting the KAT
 * ciphertext gives back the plaintext. */
void cbc_test_fetched_kat(const struct cbc_kat *kat)
{
    EVP_CIPHER *cipher = cbc_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CBC_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CBC_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CBC_KAT_PTLEN + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(CBC_KAT_PTLEN + 16);
    size_t keylen, ct_wantlen, ctlen, ptlen;

    keylen = from_hex(kat->keyhex, key, 32);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
          "fetched cipher has unexpected key length");

    from_hex(cbc_kat_iv_hex, iv, 16);
    from_hex(cbc_kat_pt_hex, pt, CBC_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CBC_KAT_PTLEN);

    ctlen = cbc_encrypt(cipher, key, iv, pt, CBC_KAT_PTLEN, ct,
                        CBC_KAT_PTLEN + 16, 0);
    expect_equal("one-shot ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cbc_decrypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                        CBC_KAT_PTLEN + 16, 0);
    expect_equal("decrypting the KAT ciphertext did not give the plaintext",
                 pt_back, ptlen, pt, CBC_KAT_PTLEN);

    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) + (3) The same KAT must also hold through the legacy static
 * EVP_aes_<n>_cbc() route and through EVP_get_cipherbyname(). */
void cbc_test_alternate_routes(const struct cbc_kat *kat,
                               const EVP_CIPHER *legacy, const char *byname)
{
    const EVP_CIPHER *byname_cipher = EVP_get_cipherbyname(byname);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CBC_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CBC_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CBC_KAT_PTLEN + 16);
    char msg[160], route[64];
    size_t ct_wantlen, ctlen;

    legacy_route_name(route, sizeof(route), byname);
    snprintf(msg, sizeof(msg), "%s returned NULL", route);
    CHECK(legacy != NULL, msg);
    snprintf(msg, sizeof(msg),
             "EVP_get_cipherbyname(\"%s\") returned NULL", byname);
    CHECK(byname_cipher != NULL, msg);

    from_hex(kat->keyhex, key, 32);
    from_hex(cbc_kat_iv_hex, iv, 16);
    from_hex(cbc_kat_pt_hex, pt, CBC_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CBC_KAT_PTLEN);

    ctlen = cbc_encrypt(legacy, key, iv, pt, CBC_KAT_PTLEN, ct,
                        CBC_KAT_PTLEN + 16, 0);
    snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT", route);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    memset(ct, 0, CBC_KAT_PTLEN + 16);
    ctlen = cbc_encrypt(byname_cipher, key, iv, pt, CBC_KAT_PTLEN, ct,
                        CBC_KAT_PTLEN + 16, 0);
    snprintf(msg, sizeof(msg),
             "EVP_get_cipherbyname(\"%s\") ciphertext does not match the KAT",
             byname);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    free_poison_allocations();
}

/* (5) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot ciphertext, and the same split on decrypt must
 * give exactly the plaintext. */
void cbc_test_streaming(const struct cbc_kat *kat)
{
    EVP_CIPHER *cipher = cbc_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(CBC_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(CBC_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(CBC_KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(CBC_KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(cbc_kat_iv_hex, iv, 16);
    from_hex(cbc_kat_pt_hex, pt, CBC_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, CBC_KAT_PTLEN);

    ctlen = cbc_stream(cipher, key, iv, pt, CBC_KAT_PTLEN, ct,
                       CBC_KAT_PTLEN + 32, 1, 0);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = cbc_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                       CBC_KAT_PTLEN + 32, 0, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, CBC_KAT_PTLEN);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (6) Padding: a 33-byte plaintext becomes a 48-byte ciphertext with
 * PKCS#5/#7 padding, round-trips, and is rejected with padding disabled. */
void cbc_test_padding(const struct cbc_kat *kat)
{
    EVP_CIPHER *cipher = cbc_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *pt_back = ALLOCATE_OUTPUT(48 + 16);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(cbc_kat_iv_hex, iv, 16);
    for (i = 0; i < 33; i++)
        pt[i] = (unsigned char)(i * 3 + 7);

    ctlen = cbc_encrypt(cipher, key, iv, pt, 33, ct, 33 + 16, 1);
    CHECK(ctlen == 48, "padded encryption of 33 bytes did not give 48 bytes");
    ptlen = cbc_decrypt(cipher, key, iv, ct, ctlen, pt_back, 48 + 16, 1);
    expect_equal("padded roundtrip did not recover the 33-byte plaintext",
                 pt_back, ptlen, pt, 33);

    /* With padding disabled, a non-multiple-of-16 input cannot be encrypted:
     * the update buffers what it can (emitting the full blocks), and the
     * finalization fails because one byte remains. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 33) == 1,
          "EVP_EncryptUpdate with padding disabled failed outright");
    CHECK(len == 32, "update with padding disabled did not emit 32 full blocks");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + len, &len) != 1,
          "padding-disabled encryption of 33 bytes did not fail at Final");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (7) Output-buffer contract: a single 16-byte update into a buffer of
 * exactly inl + block_size bytes succeeds and produces exactly that many
 * bytes (update 16 + final 16, the latter being the full pad block). */
void cbc_test_output_buffer_contract(const struct cbc_kat *kat)
{
    EVP_CIPHER *cipher = cbc_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *out = ALLOCATE_OUTPUT(32); /* exactly inl + block_size */
    EVP_CIPHER_CTX *ctx;
    int len, total;
    size_t i;

    from_hex(kat->keyhex, key, 32);
    from_hex(cbc_kat_iv_hex, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0xC0 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 16) == 1,
          "EVP_EncryptUpdate into an exact-size buffer failed");
    CHECK(len == 16, "16-byte update did not emit 16 bytes");
    total = len;
    CHECK(EVP_EncryptFinal_ex(ctx, out + total, &len) == 1,
          "EVP_EncryptFinal_ex failed at the end of the exact-size buffer");
    CHECK(len == 16, "padding Final did not emit the 16-byte pad block");
    total += len;
    CHECK(total == 32, "update+final did not fill the exact-size buffer");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
