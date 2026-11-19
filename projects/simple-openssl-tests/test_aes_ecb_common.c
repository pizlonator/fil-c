/*
 * test_aes_ecb_common.c -- shared implementation of the AES-ECB family tests
 * (API in test_aes_ecb_common.h; one binary per cipher, one struct ecb_kat
 * row per binary).
 *
 * Known-answer vectors: NIST SP 800-38A, section F.1 (ECB example vectors;
 * the AES-128, AES-192, and AES-256 sets use the shared 4-block plaintext,
 * the constant below).  The hex was verified against the authoritative build
 * with:
 *
 *   printf '<pt hex>' | xxd -r -p | \
 *       pizfix/bin/openssl enc -aes-128-ecb -nopad -K <keyhex>
 *
 * (and likewise for -aes-192-ecb and -aes-256-ecb; the per-binary provenance
 * notes say which row is which).  ECB has no IV.  The vectors are
 * block-aligned, so every KAT comparison runs with padding disabled; padding
 * itself gets its own section (ecb_test_padding).
 */

#include "test_aes_ecb_common.h"

#include <openssl/evp.h>

/* Shared SP 800-38A F.1 material: the same 4-block plaintext for all three
 * key sizes.  The constant is an array (not a pointer) so that it is an
 * address constant usable in static initializers of the per-cipher
 * binaries. */
const char ecb_kat_pt_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

/* Fetches the cipher named by a KAT row, CHECK()ing success (AES-ECB is core
 * OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *ecb_fetch_cipher(const struct ecb_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot ECB encrypt.  Encrypts `inlen` bytes in a single
 * EVP_EncryptUpdate call and returns the total ciphertext length written to
 * `out` (which must have room for inlen + block_size bytes per the EVP
 * output-buffer contract; the helper CHECKs that). */
size_t ecb_encrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *in, size_t inlen,
                   unsigned char *out, size_t outcap, int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
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

/* One-shot ECB decrypt; mirror image of ecb_encrypt(). */
size_t ecb_decrypt(const EVP_CIPHER *cipher, const unsigned char *key,
                   const unsigned char *in, size_t inlen,
                   unsigned char *out, size_t outcap, int padding)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
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
 * (the last chunk is truncated to what remains).  Splits must reproduce the
 * one-shot result exactly, which is only possible if every intermediate
 * buffer size honors the EVP contract. */
size_t ecb_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                  const unsigned char *in, size_t inlen,
                  unsigned char *out, size_t outcap, int enc, int padding)
{
    static const size_t chunks[] = { 1, 15, 16, 17, 31 };
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    size_t off = 0, ci = 0;
    int len = 0, total = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
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

/* Builds "EVP_aes_128_ecb()" out of the by-name spelling "aes-128-ecb" for
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
void ecb_test_fetched_kat(const struct ecb_kat *kat)
{
    EVP_CIPHER *cipher = ecb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *pt = ALLOCATE_INPUT(ECB_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(ECB_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ECB_KAT_PTLEN + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(ECB_KAT_PTLEN + 16);
    size_t keylen, ct_wantlen, ctlen, ptlen;

    keylen = from_hex(kat->keyhex, key, 32);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
          "fetched cipher has unexpected key length");
    CHECK(EVP_CIPHER_get_block_size(cipher) == 16,
          "AES-ECB block size is not 16");

    from_hex(ecb_kat_pt_hex, pt, ECB_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, ECB_KAT_PTLEN);

    ctlen = ecb_encrypt(cipher, key, pt, ECB_KAT_PTLEN, ct, ECB_KAT_PTLEN + 16,
                        0);
    expect_equal("one-shot ciphertext does not match the KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ecb_decrypt(cipher, key, ct_want, ct_wantlen, pt_back,
                        ECB_KAT_PTLEN + 16, 0);
    expect_equal("decrypting the KAT ciphertext did not give the plaintext",
                 pt_back, ptlen, pt, ECB_KAT_PTLEN);

    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) + (3) The KAT must also hold through the legacy static
 * EVP_aes_<n>_ecb() route and through EVP_get_cipherbyname(). */
void ecb_test_alternate_routes(const struct ecb_kat *kat,
                               const EVP_CIPHER *legacy, const char *byname)
{
    const EVP_CIPHER *byname_cipher = EVP_get_cipherbyname(byname);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *pt = ALLOCATE_INPUT(ECB_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(ECB_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ECB_KAT_PTLEN + 16);
    char msg[160], route[64];
    size_t ct_wantlen, ctlen;

    legacy_route_name(route, sizeof(route), byname);
    snprintf(msg, sizeof(msg), "%s returned NULL", route);
    CHECK(legacy != NULL, msg);
    snprintf(msg, sizeof(msg),
             "EVP_get_cipherbyname(\"%s\") returned NULL", byname);
    CHECK(byname_cipher != NULL, msg);

    from_hex(kat->keyhex, key, 32);
    from_hex(ecb_kat_pt_hex, pt, ECB_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, ECB_KAT_PTLEN);

    ctlen = ecb_encrypt(legacy, key, pt, ECB_KAT_PTLEN, ct, ECB_KAT_PTLEN + 16,
                        0);
    snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT", route);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    memset(ct, 0, ECB_KAT_PTLEN + 16);
    ctlen = ecb_encrypt(byname_cipher, key, pt, ECB_KAT_PTLEN, ct,
                        ECB_KAT_PTLEN + 16, 0);
    snprintf(msg, sizeof(msg),
             "EVP_get_cipherbyname(\"%s\") ciphertext does not match the KAT",
             byname);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    free_poison_allocations();
}

/* (4) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot ciphertext, and the same split on decrypt must
 * give exactly the plaintext. */
void ecb_test_streaming(const struct ecb_kat *kat)
{
    EVP_CIPHER *cipher = ecb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *pt = ALLOCATE_INPUT(ECB_KAT_PTLEN);
    unsigned char *ct_want = ALLOCATE_OUTPUT(ECB_KAT_PTLEN);
    unsigned char *ct = ALLOCATE_OUTPUT(ECB_KAT_PTLEN + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(ECB_KAT_PTLEN + 32);
    size_t ct_wantlen, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(ecb_kat_pt_hex, pt, ECB_KAT_PTLEN);
    ct_wantlen = from_hex(kat->cthex, ct_want, ECB_KAT_PTLEN);

    ctlen = ecb_stream(cipher, key, pt, ECB_KAT_PTLEN, ct, ECB_KAT_PTLEN + 32,
                       1, 0);
    expect_equal("streaming ciphertext does not match the one-shot ciphertext",
                 ct, ctlen, ct_want, ct_wantlen);

    ptlen = ecb_stream(cipher, key, ct_want, ct_wantlen, pt_back,
                       ECB_KAT_PTLEN + 32, 0, 0);
    expect_equal("streaming decryption did not recover the plaintext",
                 pt_back, ptlen, pt, ECB_KAT_PTLEN);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) Padding: a 33-byte plaintext becomes a 48-byte ciphertext with PKCS#7
 * padding, round-trips, and is rejected with padding disabled. */
void ecb_test_padding(const struct ecb_kat *kat)
{
    EVP_CIPHER *cipher = ecb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *pt = ALLOCATE_INPUT(33);
    unsigned char *pt_back = ALLOCATE_OUTPUT(48 + 16);
    unsigned char *ct = ALLOCATE_OUTPUT(33 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, ctlen, ptlen;

    from_hex(kat->keyhex, key, 32);
    for (i = 0; i < 33; i++)
        pt[i] = (unsigned char)(i * 3 + 7);

    ctlen = ecb_encrypt(cipher, key, pt, 33, ct, 33 + 16, 1);
    CHECK(ctlen == 48, "padded encryption of 33 bytes did not give 48 bytes");
    ptlen = ecb_decrypt(cipher, key, ct, ctlen, pt_back, 48 + 16, 1);
    expect_equal("padded roundtrip did not recover the 33-byte plaintext",
                 pt_back, ptlen, pt, 33);

    /* With padding disabled, a non-multiple-of-16 input cannot be encrypted:
     * the update buffers what it can (emitting the 32 whole bytes) and the
     * finalization fails because one byte remains. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, pt, 33) == 1,
          "EVP_EncryptUpdate with padding disabled failed outright");
    CHECK(len == 32, "update with padding disabled did not emit 32 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, ct + len, &len) != 1,
          "padding-disabled encryption of 33 bytes did not fail at Final");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (6) A zero-length no-padding ECB roundtrip is legal: the update is a no-op
 * and Final emits nothing. */
void ecb_test_zero_length(const struct ecb_kat *kat)
{
    EVP_CIPHER *cipher = ecb_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(1);
    unsigned char *ct = ALLOCATE_OUTPUT(16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(16);
    EVP_CIPHER_CTX *ctx;
    int len;

    from_hex(kat->keyhex, key, 32);
    in[0] = 0;

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, ct, &len, in, 0) == 1,
          "zero-length encrypt update failed");
    CHECK(len == 0, "zero-length encrypt update emitted bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, ct, &len) == 1,
          "zero-length no-padding encrypt Final failed");
    CHECK(len == 0, "zero-length no-padding encrypt Final emitted bytes");

    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, NULL, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, pt_back, &len, ct, 0) == 1,
          "zero-length decrypt update failed");
    CHECK(len == 0, "zero-length decrypt update emitted bytes");
    CHECK(EVP_DecryptFinal_ex(ctx, pt_back, &len) == 1,
          "zero-length no-padding decrypt Final failed");
    CHECK(len == 0, "zero-length no-padding decrypt Final emitted bytes");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (7) A key of the wrong length must not be usable: the only public way to
 * present a badkeylen-byte key to a fixed-length cipher is
 * EVP_CIPHER_CTX_set_key_length() + init, and OpenSSL must reject the
 * operation at one of those two points.  (EVP reads exactly
 * EVP_CIPHER_CTX_get_key_length() bytes from the caller's buffer, so the
 * init is only attempted when set_key_length() actually resized the
 * context.) */
void ecb_test_wrong_key_length(const struct ecb_kat *kat, int badkeylen)
{
    EVP_CIPHER *cipher = ecb_fetch_cipher(kat);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char *key = ALLOCATE_KEY((size_t)badkeylen);
    unsigned char *in = ALLOCATE_INPUT(16);
    char msg[160];
    int rejected, init_ok = 1, i;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    for (i = 0; i < badkeylen; i++)
        key[i] = (unsigned char)(i + 1);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(0xC0 + i);

    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    rejected = (EVP_CIPHER_CTX_set_key_length(ctx, badkeylen) != 1);
    if (!rejected)
        init_ok = (EVP_EncryptInit_ex(ctx, NULL, NULL, key, NULL) == 1);
    snprintf(msg, sizeof(msg), "a %d-byte key was accepted for %s",
             badkeylen, kat->ciphername);
    CHECK(rejected || !init_ok, msg);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
