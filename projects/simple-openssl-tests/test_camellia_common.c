/*
 * test_camellia_common.c -- shared implementation of the Camellia family
 * tests (API in test_camellia_common.h; one binary per cipher, one struct
 * camellia_kat row per binary: CAMELLIA-{128,192,256} in ECB/CBC/CTR/OFB/
 * CFB/CFB8/CFB1).
 *
 * Known-answer vectors:
 *
 *   - CAMELLIA-128-ECB is anchored by the RFC 3713 sample vector: key =
 *     plaintext = 0123456789abcdeffedcba9876543210, ciphertext
 *     67673138549669730857065648eabe43.  The RFC 3713 key and plaintext, and
 *     their 192/256-bit extensions, are family-wide and live in the constants
 *     below (each per-cipher binary picks its key size's row).
 *   - Every other constant was verified against the authoritative pizfix
 *     OpenSSL 3.6.5 CLI,
 *
 *       printf '<pt hex>' | xxd -r -p | \
 *           pizfix/bin/openssl enc -camellia-128-ecb -nopad -K <keyhex>
 *
 *     and likewise for every (key size, mode) combination of the family
 *     (128/192/256 x ECB/CBC/CTR/OFB/CFB/CFB8/CFB1, over the 64-byte CBC/CTR
 *     plaintext or the 32-byte stream-mode plaintext); every ciphertext was
 *     decrypted back to its plaintext with the same CLI, and every derivation
 *     was cross-checked to be byte-identical under the system OpenSSL
 *     3.0.13.  The 128/192/256-bit keys extend the RFC 3713 key with
 *     0011223344556677 (and 8899aabbccddeeff); the stream-mode IV is
 *     fedcba98765432100123456789abcdef and the CBC IV is the same value, so
 *     for every key size the CFB and OFB KATs agree on their first block
 *     (both are plaintext XOR E(IV) there).  The CTR KATs cover 64 bytes
 *     under the initial counter 000102...0f.
 *
 * Version note: Camellia is core OpenSSL 3 functionality, so none of the
 * family binaries ever skips -- a missing cipher is a FAIL, not a SKIP.
 *
 * CFB1: the cfb1 binaries cover byte mode (the default) and bit-length mode
 * (EVP_CIPH_FLAG_LENGTH_BITS, the same two-phase-init route test_aes_cfb1
 * documents: a bare init first, then set_flags, then the init that passes
 * key+IV, on a freshly created context every time -- see
 * camellia_bitmode_ctx_new below for why the context must be fresh).  In bit
 * mode `inl` is a BIT count: the update writes ceil(inl/8) bytes and reports
 * outl = inl, and bit-mode over 8N bits must agree with byte mode over N
 * bytes.  Every bit-mode buffer is also sized for the byte-count
 * interpretation so a silent fallback stays inside the allocation (see
 * test_aes_cfb1).
 */

#include "test_camellia_common.h"

#include <openssl/evp.h>

/* Shared KAT material.  The constants are arrays (not pointers) so that they
 * are address constants usable in the static struct camellia_kat initializers
 * of the per-cipher binaries. */

/* The RFC 3713 128-bit key, extended to 192/256 bits with 0011223344556677
 * (and 8899aabbccddeeff). */
const char camellia_key_128_hex[] =
    "0123456789abcdeffedcba9876543210";
const char camellia_key_192_hex[] =
    "0123456789abcdeffedcba9876543210"
    "0011223344556677";
const char camellia_key_256_hex[] =
    "0123456789abcdeffedcba9876543210"
    "00112233445566778899aabbccddeeff";

/* The RFC 3713 16-byte plaintext (the same bytes as the 128-bit key). */
const char camellia_pt16_hex[] =
    "0123456789abcdeffedcba9876543210";

/* Stream-mode and CBC IV (128-bit). */
const char camellia_iv_hex[] =
    "fedcba98765432100123456789abcdef";

/* The CTR initial counter (the CTR rows are the only ones that do not use
 * camellia_iv_hex). */
const char camellia_ctr_iv_hex[] =
    "000102030405060708090a0b0c0d0e0f";

/* The 32-byte stream-mode plaintext (CTR uses the 64-byte one below). */
const char camellia_pt32_hex[] =
    "0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef";

/* The 64-byte CBC/CTR plaintext (the SP 800-38A shared vector). */
const char camellia_pt64_hex[] =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";

/* Converts a row's IV constant into a labeled working buffer.  NULL rows
 * stay NULL: ECB takes no IV (the original test passed NULL there). */
static unsigned char *row_iv(const struct camellia_kat *kat)
{
    unsigned char *iv;

    if (kat->ivhex == NULL)
        return NULL;
    iv = ALLOCATE_IV(16);
    from_hex(kat->ivhex, iv, 16);
    return iv;
}

/* Fetches the cipher named by a KAT row, CHECK()ing success (Camellia is
 * core OpenSSL 3 functionality: never SKIP, only FAIL). */
static EVP_CIPHER *camellia_fetch_cipher(const struct camellia_kat *kat)
{
    char msg[128];
    EVP_CIPHER *cipher;

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed",
             kat->ciphername);
    cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* One-shot encrypt/decrypt with padding disabled; used for the ECB and
 * stream-mode KATs and roundtrips.  Returns the total output length written
 * to `out` (room for inlen + block_size per the EVP contract). */
size_t camellia_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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

/* CBC encrypt/decrypt with an explicit padding flag. */
size_t camellia_cbc_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
size_t camellia_stream(const EVP_CIPHER *cipher, const unsigned char *key,
                       const unsigned char *iv, const unsigned char *in,
                       size_t inlen, unsigned char *out, size_t outcap, int enc)
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
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(inlen + 2 * (size_t)EVP_CIPHER_CTX_get_block_size(ctx) <= outcap,
          "output buffer smaller than the EVP update contract requires");
    while (off < inlen) {
        size_t n = chunks[ci++ % (sizeof(chunks) / sizeof(chunks[0]))];

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

/* Creates a fresh context with bit-length mode enabled through the public
 * two-phase route, then passes key+IV.  A fresh context is used for every
 * bit-mode operation because EVP forwards only flag *changes* to the
 * provider: on a context whose EVP_CIPH_FLAG_LENGTH_BITS bit is already set,
 * a later set_flags() is a no-op, so a full re-init would silently drop the
 * context back to byte mode.  Returns the context; caller frees it. */
EVP_CIPHER_CTX *camellia_bitmode_ctx_new(const EVP_CIPHER *cipher,
                                         const unsigned char *key,
                                         const unsigned char *iv, int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc) {
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "bare EVP_EncryptInit_ex failed");
        EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
        g_checks++; /* set_flags() reports no status; the equality checks
                     * below prove the mode actually took effect. */
        CHECK(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
              "bit-mode key EVP_EncryptInit_ex failed");
    } else {
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
              "bare EVP_DecryptInit_ex failed");
        EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPH_FLAG_LENGTH_BITS);
        g_checks++; /* set_flags() reports no status; the behavioral checks
                     * below prove the mode actually took effect. */
        CHECK(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) == 1,
              "bit-mode key EVP_DecryptInit_ex failed");
    }
    return ctx;
}

/* (1) EVP_CIPHER_fetch(ciphername): the one-shot KAT for the row, plus the
 * decrypt direction (decrypting the KAT ciphertext gives back the
 * plaintext). */
void camellia_test_kat(const struct camellia_kat *kat)
{
    EVP_CIPHER *cipher = camellia_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = row_iv(kat);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    char msg[192];
    size_t keylen, ptlen, ct_wantlen, ctlen, backlen;

    keylen = from_hex(kat->keyhex, key, 32);
    snprintf(msg, sizeof(msg), "fetched %s cipher has an unexpected key length",
             kat->ciphername);
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen, msg);

    ptlen = from_hex(kat->pthex, pt, 64);
    snprintf(msg, sizeof(msg), "%s plaintext constant has an unexpected length",
             kat->ciphername);
    CHECK(ptlen == kat->ptlen, msg);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);

    ctlen = camellia_crypt(cipher, key, iv, pt, ptlen, ct, 64 + 32, 1);
    snprintf(msg, sizeof(msg), "%s one-shot ciphertext does not match the KAT",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    backlen = camellia_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                             64 + 32, 0);
    snprintf(msg, sizeof(msg),
             "%s decrypting the KAT ciphertext did not give the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, kat->ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (2) The KAT must also hold through the legacy static route and through
 * EVP_get_cipherbyname(); both routes encrypt the row's plaintext and must
 * produce the row's KAT ciphertext. */
void camellia_test_alternate_routes(const struct camellia_kat *kat,
                                    const EVP_CIPHER *legacy,
                                    const char *legacy_name, const char *byname)
{
    const EVP_CIPHER *byname_cipher = EVP_get_cipherbyname(byname);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = row_iv(kat);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    char msg[192];
    size_t ptlen, ct_wantlen, ctlen;

    snprintf(msg, sizeof(msg), "%s returned NULL", legacy_name);
    CHECK(legacy != NULL, msg);
    snprintf(msg, sizeof(msg), "EVP_get_cipherbyname(\"%s\") returned NULL",
             byname);
    CHECK(byname_cipher != NULL, msg);

    from_hex(kat->keyhex, key, 32);
    ptlen = from_hex(kat->pthex, pt, 64);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);

    ctlen = camellia_crypt(legacy, key, iv, pt, ptlen, ct, 64 + 32, 1);
    snprintf(msg, sizeof(msg), "%s ciphertext does not match the KAT",
             legacy_name);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    memset(ct, 0, 64 + 32);
    ctlen = camellia_crypt(byname_cipher, key, iv, pt, ptlen, ct, 64 + 32, 1);
    snprintf(msg, sizeof(msg),
             "EVP_get_cipherbyname(\"%s\") ciphertext does not match the KAT",
             byname);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    free_poison_allocations();
}

/* (3) Streaming: updates of {1,15,16,17,31} bytes (last one truncated) must
 * give exactly the one-shot ciphertext, and the same split on decrypt must
 * give exactly the plaintext. */
void camellia_test_streaming(const struct camellia_kat *kat)
{
    EVP_CIPHER *cipher = camellia_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = row_iv(kat);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *ct_want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 32);
    unsigned char *pt_back = ALLOCATE_OUTPUT(64 + 32);
    char msg[192];
    size_t ct_wantlen, ctlen, backlen;

    from_hex(kat->keyhex, key, 32);
    from_hex(kat->pthex, pt, 64);
    ct_wantlen = from_hex(kat->cthex, ct_want, 64);

    ctlen = camellia_stream(cipher, key, iv, pt, kat->ptlen, ct, 64 + 32, 1);
    snprintf(msg, sizeof(msg),
             "%s streaming ciphertext does not match the one-shot ciphertext",
             kat->ciphername);
    expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

    backlen = camellia_stream(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                              64 + 32, 0);
    snprintf(msg, sizeof(msg),
             "%s streaming decryption did not recover the plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, pt, kat->ptlen);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) CBC block/padding rules: a padded roundtrip of an odd length
 * (33 -> 48 -> 33) and the padding-disabled Final failure. */
void camellia_test_cbc_padding(const struct camellia_kat *kat)
{
    EVP_CIPHER *cipher = camellia_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = row_iv(kat);
    unsigned char *odd = ALLOCATE_INPUT(33);
    unsigned char *odd_ct = ALLOCATE_OUTPUT(48 + 16);
    unsigned char *pt_back = ALLOCATE_OUTPUT(48 + 16);
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t i, oddlen, backlen;
    char msg[192];

    from_hex(kat->keyhex, key, 32);
    for (i = 0; i < 33; i++)
        odd[i] = (unsigned char)(i * 3 + 7);

    /* Padded roundtrip of 33 bytes: 33 -> 48 -> 33. */
    oddlen = camellia_cbc_crypt(cipher, key, iv, odd, 33, odd_ct, 48 + 16, 1, 1);
    snprintf(msg, sizeof(msg),
             "%s padded CBC encryption of 33 bytes did not give 48",
             kat->ciphername);
    CHECK(oddlen == 48, msg);
    backlen = camellia_cbc_crypt(cipher, key, iv, odd_ct, oddlen, pt_back,
                                 48 + 16, 0, 1);
    snprintf(msg, sizeof(msg),
             "%s padded CBC roundtrip did not recover the 33-byte plaintext",
             kat->ciphername);
    expect_equal(msg, pt_back, backlen, odd, 33);

    /* Padding disabled: a 33-byte input cannot finish. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, odd_ct, &len, odd, 33) == 1,
          "padding-disabled CBC update failed outright");
    CHECK(len == 32, "padding-disabled update did not emit 32 full blocks");
    CHECK(EVP_EncryptFinal_ex(ctx, odd_ct + len, &len) != 1,
          "padding-disabled encryption of 33 bytes did not fail at Final");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) CFB1 bit-length mode: the same route test_aes_cfb1 documents (bare
 * init, set_flags(EVP_CIPH_FLAG_LENGTH_BITS), key+IV init on a fresh
 * context; see camellia_bitmode_ctx_new). */
void camellia_test_cfb1_bitmode(const struct camellia_kat *kat)
{
    EVP_CIPHER *cipher = camellia_fetch_cipher(kat);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = row_iv(kat);
    unsigned char *pt = ALLOCATE_INPUT(8);
    unsigned char *byte_mode = ALLOCATE_OUTPUT(8 + 16);
    unsigned char *bit_mode = ALLOCATE_OUTPUT(8 + 16);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    from_hex(kat->keyhex, key, 32);
    for (i = 0; i < 8; i++)
        pt[i] = (unsigned char)(0x70 + i);

    /* Byte mode: 8 bytes = 64 one-bit CFB steps. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    CHECK(EVP_EncryptUpdate(ctx, byte_mode, &len, pt, 8) == 1,
          "byte-mode CFB1 update failed");
    CHECK(len == 8, "byte-mode CFB1 update did not emit 8 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, byte_mode + len, &len) == 1,
          "byte-mode CFB1 final failed");
    EVP_CIPHER_CTX_free(ctx);

    /* Bit mode: 64 bits must give the same 8 bytes. */
    ctx = camellia_bitmode_ctx_new(cipher, key, iv, 1);
    CHECK(EVP_EncryptUpdate(ctx, bit_mode, &len, pt, 64) == 1,
          "bit-mode CFB1 update failed");
    CHECK(len == 64, "bit-mode CFB1 update did not report 64 bits");
    CHECK(EVP_EncryptFinal_ex(ctx, bit_mode + 8, &len) == 1,
          "bit-mode CFB1 final failed");
    CHECK(len == 0, "bit-mode CFB1 final emitted bytes");
    expect_equal("bit-mode CFB1 over 64 bits differs from byte mode over 8 bytes",
                 bit_mode, 8, byte_mode, 8);
    EVP_CIPHER_CTX_free(ctx);

    /* A partial final byte, on a FRESH encrypt context: 13 bits write
     * ceil(13/8) = 2 bytes (the trailing bits of the last destination byte
     * are left untouched -- the buffer starts zeroed), and decrypting the
     * same 13 bits on a fresh context recovers them. */
    ctx = camellia_bitmode_ctx_new(cipher, key, iv, 1);
    memset(bit_mode, 0, 8 + 16);
    CHECK(EVP_EncryptUpdate(ctx, bit_mode, &len, pt, 13) == 1,
          "bit-mode CFB1 13-bit update failed");
    CHECK(len == 13, "bit-mode CFB1 13-bit update did not report 13 bits");
    CHECK(EVP_EncryptFinal_ex(ctx, bit_mode + 2, &len) == 1,
          "bit-mode CFB1 13-bit final failed");
    EVP_CIPHER_CTX_free(ctx);

    /* Decrypt the 13-bit ciphertext under bit mode: only the first 13 bits
     * of the destination are defined (the buffer starts zeroed, so the
     * untouched trailing bits stay zero, as in test_aes_cfb1). */
    ctx = camellia_bitmode_ctx_new(cipher, key, iv, 0);
    memset(byte_mode, 0, 8 + 16);
    CHECK(EVP_DecryptUpdate(ctx, byte_mode, &len, bit_mode, 13) == 1,
          "bit-mode CFB1 13-bit decrypt failed");
    CHECK(len == 13, "bit-mode CFB1 13-bit decrypt did not report 13 bits");
    CHECK(EVP_DecryptFinal_ex(ctx, byte_mode + 2, &len) == 1,
          "bit-mode CFB1 13-bit decrypt final failed");
    CHECK(byte_mode[0] == pt[0] &&
              (unsigned char)(byte_mode[1] >> 5) == (unsigned char)(pt[1] >> 5),
          "bit-mode CFB1 roundtrip of the first 13 bits failed");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
