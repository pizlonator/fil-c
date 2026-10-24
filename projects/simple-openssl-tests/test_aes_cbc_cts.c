#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*
 * test_aes_cbc_cts -- CBC ciphertext stealing (CS1/CS2/CS3) tests for
 * AES-128/192/256 and CAMELLIA-128/192/256 (the Camellia rows cross-check
 * that the shared CTS helpers are cipher-independent).
 *
 * The CTS variant is selected with the OSSL_CIPHER_PARAM_CTS_MODE ("cts_mode")
 * UTF8 string context parameter at init time ("CS1" the NIST variant, "CS2"
 * and "CS3" the Kerberos5 variant; see providers/implementations/ciphers/
 * cipher_cts.c).  Observed rules, all asserted here:
 *
 *   - a data update shorter than one block (16 bytes) fails, and only ONE
 *     data update is allowed (ossl_cipher_cbc_cts_block_update);
 *   - CS1 and CS3 round-trip; for non-multiple-of-16 lengths CS1 != CS3 and
 *     CS2 == CS3; for multiple-of-16 lengths CS1 == CS2 == plain CBC, while
 *     CS3 swaps the last two blocks (so CS3 != CS1 once there are at least
 *     two blocks);
 *   - a single-block (16-byte) message: all three variants equal plain CBC.
 *
 * Version note: despite CBC-CTS being documented as new in 3.2, the
 * default provider of OpenSSL 3.0.x already provides AES-*-CBC-CTS and
 * CAMELLIA-*-CBC-CTS and honors cts_mode, so this program never skips.
 *
 * Known-answer constants: no published SP 800-38A-addendum vectors are
 * embedded; instead the constants below were derived and cross-checked in
 * one of two ways, per key size:
 *
 *   - AES-128 (and Camellia-128): derived with a scratch probe against the
 *     authoritative pizfix OpenSSL 3.6.5 and cross-checked byte for byte
 *     against the system OpenSSL 3.0.13 (CS1/CS2/CS3 differences included);
 *   - AES-192 / AES-256 / CAMELLIA-192 / CAMELLIA-256: `openssl enc` accepts
 *     CTS cipher names but has no -cts_mode option, so it always runs in the
 *     default CS1 mode and cannot produce CS2/CS3 vectors; these constants
 *     were therefore derived with the scratch EVP probe against pizfix 3.6.5
 *     and cross-checked byte for byte against system OpenSSL 3.0.13, with
 *     the CS1 values additionally re-confirmed through `openssl enc
 *     -<cipher>` on both builds.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_IV(size_t n)     { return poison_alloc(POISON_IV, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_aes_cbc_cts: %s\n", msg);
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

/* Shared KAT material (AES-128 and Camellia-128, key = IV = 00..0F). */
static const char *const KAT_KEY_HEX =
    "000102030405060708090A0B0C0D0E0F";
static const char *const KAT_IV_HEX =
    "000102030405060708090A0B0C0D0E0F";
/* 54-byte pattern used for the CS1/CS2/CS3 54-byte KATs. */
static const char *const KAT_PT54_HEX =
    "000102030405060708090A0B0C0D0E0F1011121314151617"
    "181920212223242526272829303132333435363738393a3b"
    "3c3d3e3f4041";
static const char *const KAT_CS1_54_HEX =
    "c6a13b37878f5b826f4f8162a1c8d879"
    "7b76b773f24d5b06a31ff04c02b96951"
    "715eab1c8033ce27882efe832ea04504"
    "0665e9ed2977";
static const char *const KAT_CS2_54_HEX =
    "c6a13b37878f5b826f4f8162a1c8d879"
    "7b76b773f24d5b06a31ff04c02b96951"
    "ce27882efe832ea045040665e9ed2977"
    "715eab1c8033";
static const char *const KAT_CS3_54_HEX =
    "c6a13b37878f5b826f4f8162a1c8d879"
    "7b76b773f24d5b06a31ff04c02b96951"
    "ce27882efe832ea045040665e9ed2977"
    "715eab1c8033";
/* 32-byte (block-aligned) pattern: CS1 = plain CBC, CS3 swaps the last two
 * blocks. */
static const char *const KAT_PT32_HEX =
    "000102030405060708090A0B0C0D0E0F"
    "101112131415161718191A1B1C1D1E1F";
static const char *const KAT_CS1_32_HEX =
    "c6a13b37878f5b826f4f8162a1c8d879"
    "35d9dcdb829fec3352e7bf10b84be4a5";
static const char *const KAT_CS3_32_HEX =
    "35d9dcdb829fec3352e7bf10b84be4a5"
    "c6a13b37878f5b826f4f8162a1c8d879";
/* Single block: all variants are plain CBC. */
static const char *const KAT_PT16_HEX =
    "000102030405060708090A0B0C0D0E0F";
static const char *const KAT_CS3_16_HEX =
    "c6a13b37878f5b826f4f8162a1c8d879";
/* Camellia-128, 38-byte pattern, CS3. */
static const char *const KAT_CAMELLIA_PT38_HEX =
    "000102030405060708090A0B0C0D0E0F1011121314151617"
    "1819202122232425262728293031";
static const char *const KAT_CAMELLIA_CS3_38_HEX =
    "477650012aa6284033e1b85321eef770"
    "49f2ab18de42f141e0bf054e47d52438"
    "70a35c28b52f";

/* Camellia-128/192/256: 54-byte pattern KATs, same key/IV convention as the
 * AES rows (key = the 00..0F pattern continued to the key size, so
 * CAMELLIA-128 shares KAT_KEY_HEX with AES-128; IV stays 00..0F), and the
 * same CS1 != CS2 == CS3 rule for a non-block-aligned length. */
static const char *const KAT_CAMELLIA_CS1_54_HEX =
    "477650012aa6284033e1b85321eef770"
    "70a35c28b52fb3c93853113529d46dbc"
    "831bb66b8d54ebe8919e8656f21eeec5"
    "8f7672d9aa6a";
static const char *const KAT_CAMELLIA_CS2_54_HEX =
    "477650012aa6284033e1b85321eef770"
    "70a35c28b52fb3c93853113529d46dbc"
    "ebe8919e8656f21eeec58f7672d9aa6a"
    "831bb66b8d54";
static const char *const KAT_CAMELLIA_CS3_54_HEX =
    "477650012aa6284033e1b85321eef770"
    "70a35c28b52fb3c93853113529d46dbc"
    "ebe8919e8656f21eeec58f7672d9aa6a"
    "831bb66b8d54";
static const char *const KAT_CAMELLIA192_CS1_54_HEX =
    "c3cc4fc3a5816d9b466daddc9bda7bcf"
    "cf6b2627f7199f257ecdc21a765cac0f"
    "0794cadb8de352948303e331fd4b9f2a"
    "1898d04b9970";
static const char *const KAT_CAMELLIA192_CS2_54_HEX =
    "c3cc4fc3a5816d9b466daddc9bda7bcf"
    "cf6b2627f7199f257ecdc21a765cac0f"
    "52948303e331fd4b9f2a1898d04b9970"
    "0794cadb8de3";
static const char *const KAT_CAMELLIA192_CS3_54_HEX =
    "c3cc4fc3a5816d9b466daddc9bda7bcf"
    "cf6b2627f7199f257ecdc21a765cac0f"
    "52948303e331fd4b9f2a1898d04b9970"
    "0794cadb8de3";
static const char *const KAT_CAMELLIA256_CS1_54_HEX =
    "cecf4ffa14e51c625e58a8efc3ec3814"
    "97e385d962ea20e36674f0e1961a5da1"
    "ab24931b3aada424affaffc182fc54c4"
    "08efff4dd9ae";
static const char *const KAT_CAMELLIA256_CS2_54_HEX =
    "cecf4ffa14e51c625e58a8efc3ec3814"
    "97e385d962ea20e36674f0e1961a5da1"
    "a424affaffc182fc54c408efff4dd9ae"
    "ab24931b3aad";
static const char *const KAT_CAMELLIA256_CS3_54_HEX =
    "cecf4ffa14e51c625e58a8efc3ec3814"
    "97e385d962ea20e36674f0e1961a5da1"
    "a424affaffc182fc54c408efff4dd9ae"
    "ab24931b3aad";

/* AES-192/256: 54-byte pattern KATs (same key=IV-extension convention as
 * AES-128: the 00..0F pattern continued to the key size; IV stays 00..0F). */
static const char *const KAT_KEY192_HEX =
    "000102030405060708090A0B0C0D0E0F1011121314151617";
static const char *const KAT_KEY256_HEX =
    "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";
static const char *const KAT_CS1_192_54_HEX =
    "916251821c73a522c396d627380196071f443ad4cf0cfa11"
    "3d73bab6304ff9496fdd97aafa8ebd4142d53fb4de764066"
    "be1aa340ec2c";
static const char *const KAT_CS2_192_54_HEX =
    "916251821c73a522c396d627380196071f443ad4cf0cfa11"
    "3d73bab6304ff949bd4142d53fb4de764066be1aa340ec2c"
    "6fdd97aafa8e";
static const char *const KAT_CS3_192_54_HEX =
    "916251821c73a522c396d627380196071f443ad4cf0cfa11"
    "3d73bab6304ff949bd4142d53fb4de764066be1aa340ec2c"
    "6fdd97aafa8e";
static const char *const KAT_CS1_256_54_HEX =
    "f29000b62a499fd0a9f39a6add2e77804df1a80a1ea488dd"
    "a0096bfba58fa9bc6f641e6b90df449564258400a3080a97"
    "b728e4624ce3";
static const char *const KAT_CS2_256_54_HEX =
    "f29000b62a499fd0a9f39a6add2e77804df1a80a1ea488dd"
    "a0096bfba58fa9bc449564258400a3080a97b728e4624ce3"
    "6f641e6b90df";
static const char *const KAT_CS3_256_54_HEX =
    "f29000b62a499fd0a9f39a6add2e77804df1a80a1ea488dd"
    "a0096bfba58fa9bc449564258400a3080a97b728e4624ce3"
    "6f641e6b90df";

/* The six CBC-CTS ciphers under test and their keys (key = 000102... pattern,
 * continued to the key size; CAMELLIA-128 shares its key with AES-128).  The
 * cs[123]_54hex columns are the 54-byte KAT ciphertexts for KAT_PT54_HEX in
 * the given cts_mode; every helper below that loops over this table
 * (length sweep, single block, update rules) thereby covers all six. */
static const struct cts_cipher {
    const char *ctsname;
    const char *cbcname;
    const char *keyhex;
    const char *cs1_54hex;
    const char *cs2_54hex;
    const char *cs3_54hex;
} cts_ciphers[] = {
    { "AES-128-CBC-CTS", "AES-128-CBC", KAT_KEY_HEX,
      KAT_CS1_54_HEX, KAT_CS2_54_HEX, KAT_CS3_54_HEX },
    { "AES-192-CBC-CTS", "AES-192-CBC", KAT_KEY192_HEX,
      KAT_CS1_192_54_HEX, KAT_CS2_192_54_HEX, KAT_CS3_192_54_HEX },
    { "AES-256-CBC-CTS", "AES-256-CBC", KAT_KEY256_HEX,
      KAT_CS1_256_54_HEX, KAT_CS2_256_54_HEX, KAT_CS3_256_54_HEX },
    { "CAMELLIA-128-CBC-CTS", "CAMELLIA-128-CBC", KAT_KEY_HEX,
      KAT_CAMELLIA_CS1_54_HEX, KAT_CAMELLIA_CS2_54_HEX,
      KAT_CAMELLIA_CS3_54_HEX },
    { "CAMELLIA-192-CBC-CTS", "CAMELLIA-192-CBC", KAT_KEY192_HEX,
      KAT_CAMELLIA192_CS1_54_HEX, KAT_CAMELLIA192_CS2_54_HEX,
      KAT_CAMELLIA192_CS3_54_HEX },
    { "CAMELLIA-256-CBC-CTS", "CAMELLIA-256-CBC", KAT_KEY256_HEX,
      KAT_CAMELLIA256_CS1_54_HEX, KAT_CAMELLIA256_CS2_54_HEX,
      KAT_CAMELLIA256_CS3_54_HEX },
};

/* One-shot CTS encrypt/decrypt with the given cts_mode.  Returns the output
 * length (== inlen; CTS preserves length). */
static size_t cts_crypt(const EVP_CIPHER *cipher, int enc,
                        const char *cts_mode, const unsigned char *key,
                        const unsigned char *iv, const unsigned char *in,
                        size_t inlen, unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    OSSL_PARAM params[2];
    char mode[4];
    int len = 0, flen = 0;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(inlen + 16 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    strncpy(mode, cts_mode, sizeof(mode) - 1);
    mode[sizeof(mode) - 1] = '\0';
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_CIPHER_PARAM_CTS_MODE,
                                                 mode, 0);
    params[1] = OSSL_PARAM_construct_end();
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, params) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, params) == 1,
              "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
          "EVP_CIPHER_CTX_set_padding(0) failed");
    if (enc)
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "CTS encrypt update failed");
    else
        CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "CTS decrypt update failed");
    CHECK((size_t)len == inlen, "CTS update length is not inlen");
    if (enc)
        CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
              "CTS encrypt final failed");
    else
        CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
              "CTS decrypt final failed");
    CHECK(flen == 0, "CTS Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* (1) KAT checks, including the mode parameter being readable back. */
static void test_kats(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *want = ALLOCATE_OUTPUT(64);
    unsigned char *ct = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC-CTS", NULL);
    EVP_CIPHER_CTX *ctx;
    OSSL_PARAM getp[2];
    char mode[8];
    size_t mode_len = sizeof(mode);
    size_t ptlen, wantlen, ctlen, backlen, k;

    /* CBC-CTS ships in the default provider of every OpenSSL 3.x this
     * suite targets: never SKIP, only FAIL. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"AES-128-CBC-CTS\") failed");
    from_hex(KAT_KEY_HEX, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);

    /* The default mode is CS1 and is readable through the same param. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, NULL, NULL, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    getp[0] = OSSL_PARAM_construct_utf8_string(OSSL_CIPHER_PARAM_CTS_MODE,
                                               mode, mode_len);
    getp[1] = OSSL_PARAM_construct_end();
    CHECK(EVP_CIPHER_CTX_get_params(ctx, getp) == 1,
          "reading back cts_mode failed");
    CHECK(strcmp(mode, "CS1") == 0, "the default CTS mode is not CS1");
    EVP_CIPHER_CTX_free(ctx);

    /* 54-byte KAT: CS1 differs from CS2/CS3 (which agree). */
    ptlen = from_hex(KAT_PT54_HEX, pt, 64);
    ctlen = cts_crypt(cipher, 1, "CS1", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(KAT_CS1_54_HEX, want, 64);
    expect_equal("CS1 ciphertext does not match the derived KAT",
                 ct, ctlen, want, wantlen);
    ctlen = cts_crypt(cipher, 1, "CS2", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(KAT_CS2_54_HEX, want, 64);
    expect_equal("CS2 ciphertext does not match the derived KAT",
                 ct, ctlen, want, wantlen);
    ctlen = cts_crypt(cipher, 1, "CS3", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(KAT_CS3_54_HEX, want, 64);
    expect_equal("CS3 ciphertext does not match the derived KAT",
                 ct, ctlen, want, wantlen);

    /* 32-byte KAT: CS1 = plain CBC, CS3 swaps the last two blocks. */
    ptlen = from_hex(KAT_PT32_HEX, pt, 64);
    ctlen = cts_crypt(cipher, 1, "CS1", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(KAT_CS1_32_HEX, want, 64);
    expect_equal("CS1 block-aligned ciphertext does not match the KAT",
                 ct, ctlen, want, wantlen);
    ctlen = cts_crypt(cipher, 1, "CS3", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(KAT_CS3_32_HEX, want, 64);
    expect_equal("CS3 block-aligned ciphertext does not match the KAT",
                 ct, ctlen, want, wantlen);

    /* 16-byte KAT: CS3 is plain CBC. */
    ptlen = from_hex(KAT_PT16_HEX, pt, 64);
    ctlen = cts_crypt(cipher, 1, "CS3", key, iv, pt, ptlen, ct, 64 + 16);
    wantlen = from_hex(KAT_CS3_16_HEX, want, 64);
    expect_equal("CS3 single-block ciphertext does not match the KAT",
                 ct, ctlen, want, wantlen);

    /* Camellia-128 CS3 KAT (the CTS helpers are shared, cipher_cts.c). */
    {
        EVP_CIPHER *cam = EVP_CIPHER_fetch(NULL, "CAMELLIA-128-CBC-CTS", NULL);

        CHECK(cam != NULL, "EVP_CIPHER_fetch(\"CAMELLIA-128-CBC-CTS\") failed");
        ptlen = from_hex(KAT_CAMELLIA_PT38_HEX, pt, 64);
        ctlen = cts_crypt(cam, 1, "CS3", key, iv, pt, ptlen, ct, 64 + 16);
        wantlen = from_hex(KAT_CAMELLIA_CS3_38_HEX, want, 64);
        expect_equal("Camellia CS3 ciphertext does not match the derived KAT",
                     ct, ctlen, want, wantlen);
        EVP_CIPHER_free(cam);
    }

    /* The other five ciphers' 54-byte KATs (AES-192/256 and the three
     * Camellia sizes): same CS1 != CS2 == CS3 rule as AES-128, and each CS3
     * KAT decrypts back to the plaintext.  The CAMELLIA rows double as a
     * cross-check that the CTS helpers are cipher-independent. */
    for (k = 1; k < sizeof(cts_ciphers) / sizeof(cts_ciphers[0]); k++) {
        const struct cts_cipher *kc = &cts_ciphers[k];
        EVP_CIPHER *cts = EVP_CIPHER_fetch(NULL, kc->ctsname, NULL);
        char msg[128];

        CHECK(cts != NULL,
              "EVP_CIPHER_fetch failed for a core AES/Camellia-CBC-CTS cipher");
        from_hex(kc->keyhex, key, 32);
        from_hex(KAT_IV_HEX, iv, 16);
        ptlen = from_hex(KAT_PT54_HEX, pt, 64);

        ctlen = cts_crypt(cts, 1, "CS1", key, iv, pt, ptlen, ct, 64 + 16);
        wantlen = from_hex(kc->cs1_54hex, want, 64);
        snprintf(msg, sizeof(msg),
                 "%s CS1 ciphertext does not match the derived KAT", kc->ctsname);
        expect_equal(msg, ct, ctlen, want, wantlen);

        ctlen = cts_crypt(cts, 1, "CS2", key, iv, pt, ptlen, ct, 64 + 16);
        wantlen = from_hex(kc->cs2_54hex, want, 64);
        snprintf(msg, sizeof(msg),
                 "%s CS2 ciphertext does not match the derived KAT", kc->ctsname);
        expect_equal(msg, ct, ctlen, want, wantlen);

        ctlen = cts_crypt(cts, 1, "CS3", key, iv, pt, ptlen, ct, 64 + 16);
        wantlen = from_hex(kc->cs3_54hex, want, 64);
        snprintf(msg, sizeof(msg),
                 "%s CS3 ciphertext does not match the derived KAT", kc->ctsname);
        expect_equal(msg, ct, ctlen, want, wantlen);

        backlen = cts_crypt(cts, 0, "CS3", key, iv, want, wantlen,
                            back, 64 + 16);
        snprintf(msg, sizeof(msg),
                 "%s CS3 decryption of the KAT did not give the plaintext",
                 kc->ctsname);
        expect_equal(msg, back, backlen, pt, ptlen);
        EVP_CIPHER_free(cts);
    }
    from_hex(KAT_KEY_HEX, key, 16);
    from_hex(KAT_IV_HEX, iv, 16);

    /* And the 54-byte CS3 KAT round-trips back to the plaintext. */
    ptlen = from_hex(KAT_PT54_HEX, pt, 64);
    wantlen = from_hex(KAT_CS3_54_HEX, want, 64);
    backlen = cts_crypt(cipher, 0, "CS3", key, iv, want, wantlen,
                        back, 64 + 16);
    expect_equal("CS3 decryption of the KAT did not give the plaintext",
                 back, backlen, pt, ptlen);
    EVP_CIPHER_free(cipher);
}

/* (2) Length sweep {17,31,32,33,48} for all six ciphers: CS1/CS3 round-trip
 * everywhere; the documented CS1/CS2/CS3 equalities and differences. */
static void test_length_sweep(void)
{
    static const size_t lengths[] = { 17, 31, 32, 33, 48 };
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *cs1 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *cs2 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *cs3 = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(64 + 16);
    unsigned char *cbcref = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t l, i, n, cs1len, cs2len, cs3len, backlen, k;
    char msg[160];

    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i * 5 + 1);

    for (k = 0; k < sizeof(cts_ciphers) / sizeof(cts_ciphers[0]); k++) {
        const struct cts_cipher *kc = &cts_ciphers[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kc->ctsname, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core AES/Camellia-CBC-CTS cipher");
        from_hex(kc->keyhex, key, 32);

        for (l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++) {
            n = lengths[l];

            cs1len = cts_crypt(cipher, 1, "CS1", key, iv, pt, n, cs1, 64 + 16);
            cs2len = cts_crypt(cipher, 1, "CS2", key, iv, pt, n, cs2, 64 + 16);
            cs3len = cts_crypt(cipher, 1, "CS3", key, iv, pt, n, cs3, 64 + 16);

            /* CS1 and CS3 round-trip. */
            backlen = cts_crypt(cipher, 0, "CS1", key, iv, cs1, cs1len,
                                back, 64 + 16);
            snprintf(msg, sizeof(msg), "%s CS1 roundtrip of %zu bytes failed",
                     kc->ctsname, n);
            expect_equal(msg, back, backlen, pt, n);
            backlen = cts_crypt(cipher, 0, "CS3", key, iv, cs3, cs3len,
                                back, 64 + 16);
            snprintf(msg, sizeof(msg), "%s CS3 roundtrip of %zu bytes failed",
                     kc->ctsname, n);
            expect_equal(msg, back, backlen, pt, n);

            if (n % 16 != 0) {
                /* Non-aligned: CS1 differs from CS3; CS2 == CS3. */
                CHECK(cs1len == cs3len && memcmp(cs1, cs3, cs1len) != 0,
                      "CS1 equals CS3 for a non-aligned length");
                expect_equal("CS2 differs from CS3 for a non-aligned length",
                             cs2, cs2len, cs3, cs3len);
            } else {
                /* Aligned: CS1 == CS2 == plain CBC; CS3 swaps the last two
                 * blocks, so it differs once there are at least two blocks.
                 * The plain-CBC reference is computed with the CBC cipher of
                 * the same key size. */
                EVP_CIPHER *cbcsz = EVP_CIPHER_fetch(NULL, kc->cbcname, NULL);

                CHECK(cbcsz != NULL,
                      "EVP_CIPHER_fetch failed for a core AES/Camellia-CBC cipher");
                expect_equal("CS2 differs from CS1 for an aligned length",
                             cs2, cs2len, cs1, cs1len);
                ctx = EVP_CIPHER_CTX_new();
                CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
                CHECK(EVP_EncryptInit_ex2(ctx, cbcsz, key, iv, NULL) == 1,
                      "EVP_EncryptInit_ex2 failed");
                CHECK(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1,
                      "EVP_CIPHER_CTX_set_padding(0) failed");
                CHECK(EVP_EncryptUpdate(ctx, cbcref, &len, pt, (int)n) == 1,
                      "plain CBC update failed");
                CHECK(EVP_EncryptFinal_ex(ctx, cbcref + len, &flen) == 1,
                      "plain CBC final failed");
                EVP_CIPHER_CTX_free(ctx);
                len = 0;
                flen = 0;
                expect_equal("CS1 differs from plain CBC for an aligned length",
                             cs1, cs1len, cbcref, n);
                if (n >= 32) {
                    CHECK(memcmp(cs3, cs1, cs1len) != 0,
                          "CS3 equals CS1 for an aligned length >= 32");
                } else {
                    expect_equal("CS3 differs from plain CBC for one block",
                                 cs3, cs3len, cbcref, n);
                }
                EVP_CIPHER_free(cbcsz);
            }
        }
        EVP_CIPHER_free(cipher);
    }
}

/* (3) A single 16-byte message: CS1 == CS2 == CS3 == plain CBC, for all six
 * ciphers. */
static void test_single_block(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(16);
    unsigned char *cs1 = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *cs2 = ALLOCATE_OUTPUT(16 + 16);
    unsigned char *cs3 = ALLOCATE_OUTPUT(16 + 16);
    size_t k, cs1len, cs2len, cs3len;

    from_hex(KAT_IV_HEX, iv, 16);
    from_hex(KAT_PT16_HEX, pt, 16);

    for (k = 0; k < sizeof(cts_ciphers) / sizeof(cts_ciphers[0]); k++) {
        const struct cts_cipher *kc = &cts_ciphers[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kc->ctsname, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core AES/Camellia-CBC-CTS cipher");
        from_hex(kc->keyhex, key, 32);

        cs1len = cts_crypt(cipher, 1, "CS1", key, iv, pt, 16, cs1, 16 + 16);
        cs2len = cts_crypt(cipher, 1, "CS2", key, iv, pt, 16, cs2, 16 + 16);
        cs3len = cts_crypt(cipher, 1, "CS3", key, iv, pt, 16, cs3, 16 + 16);
        CHECK(cs1len == 16 && cs2len == 16 && cs3len == 16,
              "single-block CTS lengths are not 16");
        CHECK(memcmp(cs1, cs2, 16) == 0 && memcmp(cs1, cs3, 16) == 0,
              "the CTS variants disagree on a single-block message");
        EVP_CIPHER_free(cipher);
    }
}

/* (4) Update rules: fewer than 16 bytes fails; a second update fails
 * (one-shot only, cipher_cts.c ctx->updated).  Runs for all six ciphers. */
static void test_update_rules(void)
{
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(64);
    unsigned char *out = ALLOCATE_OUTPUT(64 + 16);
    EVP_CIPHER_CTX *ctx;
    int len = 0;
    size_t k, i;

    from_hex(KAT_IV_HEX, iv, 16);
    for (i = 0; i < 64; i++)
        pt[i] = (unsigned char)(i + 2);

    for (k = 0; k < sizeof(cts_ciphers) / sizeof(cts_ciphers[0]); k++) {
        const struct cts_cipher *kc = &cts_ciphers[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kc->ctsname, NULL);

        CHECK(cipher != NULL,
              "EVP_CIPHER_fetch failed for a core AES/Camellia-CBC-CTS cipher");
        from_hex(kc->keyhex, key, 32);

        /* A sub-block update fails. */
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
        CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, 15) != 1,
              "a 15-byte CTS update was accepted");
        EVP_CIPHER_CTX_free(ctx);

        /* Exactly one block is fine, but a second update fails. */
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
        CHECK(EVP_EncryptUpdate(ctx, out, &len, pt, 16) == 1,
              "a one-block CTS update failed");
        CHECK(EVP_EncryptUpdate(ctx, out + len, &len, pt + 16, 16) != 1,
              "a second CTS update was accepted");
        EVP_CIPHER_CTX_free(ctx);

        /* Same on the decrypt side. */
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
        CHECK(EVP_DecryptUpdate(ctx, out, &len, pt, 16) == 1,
              "a one-block CTS decrypt update failed");
        CHECK(EVP_DecryptUpdate(ctx, out + len, &len, pt + 16, 16) != 1,
              "a second CTS decrypt update was accepted");
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_length_sweep();
    test_single_block();
    test_update_rules();

    printf("test_aes_cbc_cts: all %u checks passed\n", g_checks);
    return 0;
}
