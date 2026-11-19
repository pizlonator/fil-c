/*
 * test_aes_xts_common.c -- shared implementation of the AES-XTS family tests
 * (API in test_aes_xts_common.h; one binary per cipher, one struct xts_kat
 * table per small-suite binary).
 *
 * An XTS key is the concatenation of two same-length AES keys; the 128-bit
 * IV is the data-unit number.
 *
 * Known-answer vectors: the IEEE Std 1619-2007 XTS-AES test vectors as
 * distributed in OpenSSL's own test data
 * (test/recipes/30-test_evp_data/evpciph_aes_common.txt, "AES XTS test
 * vectors from IEEE Std 1619-2007" and the "Exercise different lengths
 * covering even ciphertext stealing cases" set).  Every hex constant was
 * re-verified against the authoritative pizfix OpenSSL 3.6.5 with a
 * scratch program compiled by build/bin/filcc (`openssl enc` refuses XTS
 * ciphers, so the CLI oracle is not available here).
 *
 * Behavior established by scratch experiments (identical on 3.0.13 and
 * 3.6.5, and matching crypto/modes/xts128.c and
 * providers/implementations/ciphers/cipher_aes_xts.c):
 *
 *   - a data unit shorter than 16 bytes fails (xts128.c: "if (len < 16)
 *     return -1"), and so does a zero-length update;
 *   - ciphertext stealing works in BOTH directions: CRYPTO_xts128_encrypt()
 *     handles a non-multiple-of-16 tail when encrypting (its final
 *     "if (enc)" branch) as well as when decrypting, and OpenSSL's own test
 *     data contains 17/18/19-byte ENCRYPT vectors;
 *   - every update is an independent data unit: the tweak is never advanced
 *     between updates, so the same 16-byte input under the same IV encrypts
 *     to the same block every time;
 *   - a key whose two halves are equal is rejected when INITIALIZING for
 *     encryption (aes_xts_check_keys_differ); for decryption the check only
 *     applies inside the FIPS provider (OpenSSL's test data: "Using the same
 *     key twice for decryption is allowed outside of FIPS mode"), so the
 *     decrypt route is covered here with a roundtrip instead of a
 *     must-fail;
 *   - a data unit larger than 2^20 * 16 bytes fails with
 *     XTS_DATA_UNIT_IS_TOO_LARGE.
 *
 * Data-unit size boundary (the test_aes_256_xts_large binary): IEEE Std
 * 1619-2018 (and NIST SP 800-38E) limit an XTS data unit to 2^20 AES blocks
 * = 16 MiB.  OpenSSL enforces this in the provider
 * (providers/implementations/ciphers/cipher_aes_xts.c:
 *
 *     if (inl > XTS_MAX_BLOCKS_PER_DATA_UNIT * AES_BLOCK_SIZE) ...
 *         PROV_R_XTS_DATA_UNIT_IS_TOO_LARGE
 *
 * with XTS_MAX_BLOCKS_PER_DATA_UNIT = 1 << 20 in include/crypto/modes.h), so
 * a single update of exactly 16777216 bytes is the largest permitted data
 * unit and 16777217 bytes must fail.  Both bounds were verified by scratch
 * experiments on OpenSSL 3.0.13 and 3.6.5 (identical behavior).  Every
 * update is an independent data unit (the tweak is never advanced between
 * updates), which the 4 x 1 MiB stream below exercises at scale.
 *
 * There is no published KAT at the large sizes; the vectors are the IEEE
 * Std 1619-2007 AES-256 key/plaintext patterns and all comparisons are
 * encrypt/decrypt roundtrips plus stream-vs-one-shot consistency, all
 * deterministic.
 */

#include "test_aes_xts_common.h"

#include <openssl/evp.h>

/* Shared KAT material: the IEEE Std 1619-2007 XTS-AES keys (two concatenated
 * AES keys).  The constants are arrays (not pointers) so that the
 * per-cipher binaries can use them in static struct xts_kat initializers. */
const char xts_key_128_ieee_hex[] =
    "fffefdfcfbfaf9f8f7f6f5f4f3f2f1f0bfbebdbcbbbab9b8b7b6b5b4b3b2b1b0";
const char xts_key_256_ieee_hex[] =
    "27182818284590452353602874713526"
    "62497757247093699959574966967627"
    "31415926535897932384626433832795"
    "02884197169399375105820974944592";

/* One-shot XTS encrypt/decrypt of a single data unit.  Returns the output
 * length; `out` must have room for inlen bytes (XTS emits exactly inlen
 * bytes; the EVP layer passes outsize = inl for this block-size-1 cipher). */
size_t xts_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
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
    CHECK(inlen <= outcap,
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

/* Fetches an XTS cipher by name, CHECK()ing success. */
static EVP_CIPHER *xts_fetch_cipher(const char *ciphername)
{
    EVP_CIPHER *cipher;
    char msg[160];

    snprintf(msg, sizeof(msg), "EVP_CIPHER_fetch(\"%s\") failed", ciphername);
    cipher = EVP_CIPHER_fetch(NULL, ciphername, NULL);
    CHECK(cipher != NULL, msg);
    return cipher;
}

/* (1) One-shot KATs: encrypt the plaintext and compare against the IEEE
 * vector; decrypt the vector ciphertext and compare against the plaintext. */
void xts_test_kats(const struct xts_kat *kats, size_t nkats)
{
    size_t k;

    for (k = 0; k < nkats; k++) {
        const struct xts_kat *kat = &kats[k];
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, kat->ciphername, NULL);
        unsigned char *key = ALLOCATE_KEY(64);
        unsigned char *iv = ALLOCATE_IV(16);
        unsigned char *pt = ALLOCATE_INPUT(128);
        unsigned char *ct_want = ALLOCATE_OUTPUT(128);
        unsigned char *ct = ALLOCATE_OUTPUT(128);
        unsigned char *pt_back = ALLOCATE_OUTPUT(128);
        size_t keylen, ptlen, ct_wantlen, ctlen, ptlen_back;
        char msg[128];

        /* AES-XTS is core OpenSSL 3 functionality: never SKIP, only FAIL. */
        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-XTS cipher");
        CHECK(EVP_CIPHER_get_block_size(cipher) == 1,
              "XTS reports a block size other than 1");
        CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
              "XTS IV length is not 16");
        keylen = from_hex(kat->keyhex, key, 64);
        CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == keylen,
              "fetched cipher has unexpected key length");
        from_hex(kat->ivhex, iv, 16);
        ptlen = from_hex(kat->pthex, pt, 128);
        ct_wantlen = from_hex(kat->cthex, ct_want, 128);

        ctlen = xts_crypt(cipher, key, iv, pt, ptlen, ct, 128, 1);
        snprintf(msg, sizeof(msg), "%s: ciphertext does not match the KAT",
                 kat->label);
        expect_equal(msg, ct, ctlen, ct_want, ct_wantlen);

        ptlen_back = xts_crypt(cipher, key, iv, ct_want, ct_wantlen, pt_back,
                               128, 0);
        snprintf(msg, sizeof(msg), "%s: decrypting the KAT did not give the plaintext",
                 kat->label);
        expect_equal(msg, pt_back, ptlen_back, pt, ptlen);

        EVP_CIPHER_free(cipher);

        free_poison_allocations();
    }
}

/* (2) Data units shorter than 16 bytes must fail, and a zero-length update
 * must fail too (aes_xts_cipher rejects inl < AES_BLOCK_SIZE and in ==
 * NULL).  Each negative test uses a fresh context. */
void xts_test_short_data_units_fail(const char *ciphername, const char *keyhex,
                                    const char *ivhex)
{
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *out = ALLOCATE_OUTPUT(16);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    from_hex(keyhex, key, 64);
    from_hex(ivhex, iv, 16);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)i;

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 15) != 1,
          "a 15-byte data unit was accepted for encryption");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, 15) != 1,
          "a 15-byte data unit was accepted for decryption");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 0) != 1,
          "a zero-length data unit was accepted for encryption");
    EVP_CIPHER_CTX_free(ctx);

    /* A 16-byte unit still works after all those failures (fresh context). */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 16) == 1,
          "a 16-byte data unit was rejected");
    CHECK(len == 16, "16-byte XTS update did not emit 16 bytes");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (3) Ciphertext stealing round-trips for non-multiple-of-16 lengths on the
 * decrypt route (and the matching encryption, per the KATs above).  A
 * 21-byte data unit is encrypted and decrypted back in one shot and in
 * place, and a 53-byte message round-trips through two split updates (a
 * 32-byte and a 21-byte data unit; every XTS update is a data unit of at
 * least 16 bytes). */
void xts_test_stealing_roundtrip(const char *ciphername, const char *keyhex,
                                 const char *ivhex)
{
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *pt = ALLOCATE_INPUT(53);
    unsigned char *ct = ALLOCATE_OUTPUT(53);
    unsigned char *buf = ALLOCATE_OUTPUT(53);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    from_hex(keyhex, key, 64);
    from_hex(ivhex, iv, 16);
    for (i = 0; i < 53; i++)
        pt[i] = (unsigned char)(i * 3 + 1);

    /* One-shot 21-byte (stolen tail) roundtrip. */
    xts_crypt(cipher, key, iv, pt, 21, ct, 53, 1);
    xts_crypt(cipher, key, iv, ct, 21, buf, 53, 0);
    unpoison_all();
    CHECK(memcmp(buf, pt, 21) == 0,
          "21-byte (stolen tail) roundtrip failed");
    repoison_all();

    /* In-place 21-byte roundtrip. */
    memcpy(buf, pt, 21);
    xts_crypt(cipher, key, iv, buf, 21, buf, 53, 1);
    unpoison_all();
    CHECK(memcmp(buf, ct, 21) == 0,
          "in-place XTS encryption differs from out-of-place");
    repoison_all();
    xts_crypt(cipher, key, iv, buf, 21, buf, 53, 0);
    unpoison_all();
    CHECK(memcmp(buf, pt, 21) == 0, "in-place XTS roundtrip failed");
    repoison_all();

    /* Split updates: 32 bytes + 21 bytes.  Each update is an independent
     * data unit (so both are at least 16 bytes), and decrypting the two
     * ciphertext pieces with the same IV in the same order reproduces the
     * plaintext pieces. */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, buf, &len, pt, 32) == 1,
          "32-byte XTS encrypt update failed");
    CHECK(len == 32, "32-byte XTS encrypt update did not emit 32 bytes");
    CHECK(EVP_EncryptUpdate(ctx, buf + 32, &len, pt + 32, 21) == 1,
          "21-byte XTS encrypt update failed");
    CHECK(len == 21, "21-byte XTS encrypt update did not emit 21 bytes");
    CHECK(EVP_EncryptFinal_ex(ctx, buf + 53, &len) == 1,
          "XTS Final failed");
    CHECK(len == 0, "XTS Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, ct, &len, buf, 32) == 1,
          "32-byte XTS decrypt update failed");
    CHECK(len == 32, "32-byte XTS decrypt update did not emit 32 bytes");
    CHECK(EVP_DecryptUpdate(ctx, ct + 32, &len, buf + 32, 21) == 1,
          "21-byte XTS decrypt update failed");
    CHECK(len == 21, "21-byte XTS decrypt update did not emit 21 bytes");
    CHECK(EVP_DecryptFinal_ex(ctx, ct + 53, &len) == 1,
          "XTS Final failed");
    EVP_CIPHER_CTX_free(ctx);
    unpoison_all();
    CHECK(memcmp(ct, pt, 53) == 0,
          "32+21 split data units did not round-trip");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (4) Every update is an independent data unit: the tweak is never advanced
 * between updates, so encrypting the same 16-byte block twice through one
 * context yields identical ciphertext, while the second block of a single
 * 32-byte one-shot data unit differs (its tweak has advanced). */
void xts_test_each_update_is_a_data_unit(const char *ciphername,
                                         const char *keyhex,
                                         const char *ivhex)
{
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(32);
    unsigned char *twice = ALLOCATE_OUTPUT(32);
    unsigned char *oneshot = ALLOCATE_OUTPUT(32);
    EVP_CIPHER_CTX *ctx;
    int len, i;

    from_hex(keyhex, key, 64);
    from_hex(ivhex, iv, 16);
    for (i = 0; i < 32; i++)
        in[i] = (unsigned char)(0x40 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, twice, &len, in, 16) == 1,
          "first 16-byte XTS update failed");
    CHECK(EVP_EncryptUpdate(ctx, twice + 16, &len, in, 16) == 1,
          "second 16-byte XTS update failed");
    EVP_CIPHER_CTX_free(ctx);
    unpoison_all();
    CHECK(memcmp(twice, twice + 16, 16) == 0,
          "two equal 16-byte updates encrypted differently: the tweak "
          "advanced between updates");
    repoison_all();

    xts_crypt(cipher, key, iv, in, 32, oneshot, 32, 1);
    unpoison_all();
    CHECK(memcmp(oneshot, twice, 16) == 0,
          "the first block of a 32-byte one-shot data unit differs from a "
          "16-byte data unit with the same IV");
    repoison_all();
    unpoison_all();
    CHECK(memcmp(oneshot + 16, twice + 16, 16) != 0,
          "the second block of a 32-byte data unit did not use an advanced "
          "tweak");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (5) Weak keys: a key whose two halves are equal must be rejected when
 * initializing for encryption.  (For decryption the default provider
 * permits it -- see the file comment -- and the route is still a
 * well-behaved, deterministic permutation on aligned data units.) */
void xts_test_weak_key(const char *ciphername, const char *keyhex,
                       const char *ivhex)
{
    size_t keylen = strlen(keyhex) / 2;   /* two concatenated AES keys */
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(32);
    unsigned char *out = ALLOCATE_OUTPUT(32);
    unsigned char *back = ALLOCATE_OUTPUT(32);
    EVP_CIPHER_CTX *ctx;
    size_t i;
    int len;

    from_hex(ivhex, iv, 16);
    for (i = 0; i < keylen; i++)
        key[i] = (unsigned char)0x77;   /* two identical halves */
    for (i = 0; i < 32; i++)
        in[i] = (unsigned char)(0x80 + i);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) != 1,
          "encryption with equal key halves was accepted");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "decryption with equal key halves was rejected (expected: only "
          "the FIPS provider checks this)");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, 21) == 1,
          "weak-key decryption of a 21-byte unit failed");
    CHECK(len == 21, "weak-key decryption did not emit 21 bytes");
    EVP_CIPHER_CTX_free(ctx);

    /* Deterministic: a second context decrypts the same data unit to the
     * same value.  (XTS decryption is not self-inverse -- like any cipher
     * built on AES it is only the inverse of *encryption* -- so no
     * decrypt-twice property is asserted here.) */
    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "second weak-key decrypt init failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, in, 21) == 1,
          "second weak-key decryption update failed");
    unpoison_all();
    CHECK(memcmp(back, out, 21) == 0,
          "weak-key XTS decryption is not deterministic");
    repoison_all();
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (6) A 16-byte key cannot be used for AES-128-XTS (nor a 32-byte key for
 * AES-256-XTS): the only public way to present it is
 * EVP_CIPHER_CTX_set_key_length() + init, and OpenSSL must reject the
 * operation at one of those two points.  (As with ECB, the init is only
 * attempted when set_key_length() actually resized the context, because
 * EVP reads EVP_CIPHER_CTX_get_key_length() bytes from the key buffer.) */
void xts_test_wrong_key_length(const char *ciphername, const char *keyhex,
                               const char *ivhex)
{
    /* The half-length key: half of the two concatenated AES keys. */
    size_t halflen = strlen(keyhex) / 4;
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    EVP_CIPHER_CTX *ctx;
    char msg[160];
    int rejected, init_ok = 1;
    size_t i;

    from_hex(ivhex, iv, 16);
    for (i = 0; i < halflen; i++)
        key[i] = (unsigned char)i;

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) == 1,
          "bare EVP_EncryptInit_ex failed");
    rejected = (EVP_CIPHER_CTX_set_key_length(ctx, (int)halflen) != 1);
    if (!rejected)
        init_ok = (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) == 1);
    snprintf(msg, sizeof(msg), "a %zu-byte key was accepted for %s",
             halflen, ciphername);
    CHECK(rejected || !init_ok, msg);
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (L1) The maximum data unit: a single 16777216-byte update encrypts and
 * decrypts back. */
void xts_test_max_data_unit_roundtrip(const char *ciphername,
                                      const char *keyhex, const char *ivhex)
{
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(XTS_MAX_DATA_UNIT);
    unsigned char *ct = ALLOCATE_OUTPUT(XTS_MAX_DATA_UNIT);
    unsigned char *back = ALLOCATE_OUTPUT(XTS_MAX_DATA_UNIT);
    size_t i, ctlen, backlen;

    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == from_hex(keyhex, key, 64),
          "fetched cipher has unexpected key length");
    CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
          "XTS IV length is not 16");
    from_hex(ivhex, iv, 16);
    for (i = 0; i < XTS_MAX_DATA_UNIT; i++)
        in[i] = (unsigned char)(i * 11 + (i >> 9));

    ctlen = xts_crypt(cipher, key, iv, in, XTS_MAX_DATA_UNIT, ct,
                      XTS_MAX_DATA_UNIT, 1);
    CHECK(ctlen == XTS_MAX_DATA_UNIT,
          "the 16 MiB data unit did not encrypt to 16 MiB");

    backlen = xts_crypt(cipher, key, iv, ct, ctlen, back, XTS_MAX_DATA_UNIT, 0);
    CHECK(backlen == XTS_MAX_DATA_UNIT,
          "the 16 MiB data unit did not decrypt to 16 MiB");
    unpoison_all();
    CHECK(memcmp(back, in, XTS_MAX_DATA_UNIT) == 0,
          "the 16 MiB data unit roundtrip failed");
    repoison_all();

    /* Spot-check that the ciphertext is not trivially the plaintext. */
    unpoison_all();
    CHECK(memcmp(ct, in, 4096) != 0,
          "the 16 MiB ciphertext begins with the plaintext");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (L2) One byte beyond the maximum data unit must fail with
 * XTS_DATA_UNIT_IS_TOO_LARGE -- for encryption and decryption. */
void xts_test_data_unit_too_large(const char *ciphername, const char *keyhex,
                                  const char *ivhex)
{
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(XTS_MAX_DATA_UNIT + 1);
    unsigned char *out = ALLOCATE_OUTPUT(XTS_MAX_DATA_UNIT + 1);
    EVP_CIPHER_CTX *ctx;
    int len;

    from_hex(keyhex, key, 64);
    from_hex(ivhex, iv, 16);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in,
                            (int)(XTS_MAX_DATA_UNIT + 1)) != 1,
          "a 16 MiB + 1 byte data unit was accepted for encryption");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in,
                            (int)(XTS_MAX_DATA_UNIT + 1)) != 1,
          "a 16 MiB + 1 byte data unit was accepted for decryption");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (L3) A 1 MiB data unit round-trips in one shot and in place. */
void xts_test_one_mebibyte_unit(const char *ciphername, const char *keyhex,
                                const char *ivhex)
{
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(XTS_MEBIBYTE);
    unsigned char *ct = ALLOCATE_OUTPUT(XTS_MEBIBYTE);
    unsigned char *back = ALLOCATE_OUTPUT(XTS_MEBIBYTE);
    size_t i, ctlen, backlen;

    from_hex(keyhex, key, 64);
    from_hex(ivhex, iv, 16);
    for (i = 0; i < XTS_MEBIBYTE; i++)
        in[i] = (unsigned char)(i * 11 + (i >> 9));

    ctlen = xts_crypt(cipher, key, iv, in, XTS_MEBIBYTE, ct, XTS_MEBIBYTE, 1);
    CHECK(ctlen == XTS_MEBIBYTE, "the 1 MiB data unit did not encrypt to 1 MiB");
    backlen = xts_crypt(cipher, key, iv, ct, ctlen, back, XTS_MEBIBYTE, 0);
    CHECK(backlen == XTS_MEBIBYTE, "the 1 MiB data unit did not decrypt to 1 MiB");
    unpoison_all();
    CHECK(memcmp(back, in, XTS_MEBIBYTE) == 0, "the 1 MiB data unit roundtrip failed");
    repoison_all();
    unpoison_all();
    CHECK(memcmp(ct, in, 4096) != 0,
          "the 1 MiB ciphertext begins with the plaintext");
    repoison_all();

    /* In-place roundtrip of the same data unit. */
    memcpy(back, in, XTS_MEBIBYTE);
    xts_crypt(cipher, key, iv, back, XTS_MEBIBYTE, back, XTS_MEBIBYTE, 1);
    unpoison_all();
    CHECK(memcmp(back, ct, XTS_MEBIBYTE) == 0,
          "in-place 1 MiB encryption differs from out-of-place");
    repoison_all();
    xts_crypt(cipher, key, iv, back, XTS_MEBIBYTE, back, XTS_MEBIBYTE, 0);
    unpoison_all();
    CHECK(memcmp(back, in, XTS_MEBIBYTE) == 0, "in-place 1 MiB roundtrip failed");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}

/* (L4) A 4 MiB stream through four 1 MiB updates round-trips, and each
 * update is an independent data unit: the first 1 MiB of the streamed
 * ciphertext equals the one-shot 1 MiB data-unit ciphertext with the same
 * data-unit number. */
void xts_test_four_mebibyte_stream(const char *ciphername, const char *keyhex,
                                   const char *ivhex)
{
    EVP_CIPHER *cipher = xts_fetch_cipher(ciphername);
    unsigned char *key = ALLOCATE_KEY(64);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(4 * XTS_MEBIBYTE);
    unsigned char *ct = ALLOCATE_OUTPUT(4 * XTS_MEBIBYTE);
    unsigned char *back = ALLOCATE_OUTPUT(4 * XTS_MEBIBYTE);
    unsigned char *unit = ALLOCATE_OUTPUT(XTS_MEBIBYTE);
    EVP_CIPHER_CTX *ctx;
    int len, u;
    size_t i;

    from_hex(keyhex, key, 64);
    from_hex(ivhex, iv, 16);
    for (i = 0; i < 4 * XTS_MEBIBYTE; i++)
        in[i] = (unsigned char)(i * 11 + (i >> 9));

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_EncryptInit_ex2 failed");
    for (u = 0; u < 4; u++) {
        CHECK(EVP_EncryptUpdate(ctx, ct + u * XTS_MEBIBYTE, &len,
                                in + u * XTS_MEBIBYTE, (int)XTS_MEBIBYTE) == 1,
              "1 MiB stream encryption update failed");
        CHECK(len == (int)XTS_MEBIBYTE,
              "1 MiB stream encryption update did not emit 1 MiB");
    }
    CHECK(EVP_EncryptFinal_ex(ctx, ct + 4 * XTS_MEBIBYTE, &len) == 1,
          "XTS Final failed");
    CHECK(len == 0, "XTS Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
          "EVP_DecryptInit_ex2 failed");
    for (u = 0; u < 4; u++) {
        CHECK(EVP_DecryptUpdate(ctx, back + u * XTS_MEBIBYTE, &len,
                                ct + u * XTS_MEBIBYTE, (int)XTS_MEBIBYTE) == 1,
              "1 MiB stream decryption update failed");
        CHECK(len == (int)XTS_MEBIBYTE,
              "1 MiB stream decryption update did not emit 1 MiB");
    }
    CHECK(EVP_DecryptFinal_ex(ctx, back + 4 * XTS_MEBIBYTE, &len) == 1,
          "XTS Final failed");
    EVP_CIPHER_CTX_free(ctx);
    unpoison_all();
    CHECK(memcmp(back, in, 4 * XTS_MEBIBYTE) == 0,
          "the 4 x 1 MiB stream roundtrip failed");
    repoison_all();

    /* Each stream update is its own data unit: encrypting the first MiB
     * through a fresh context reproduces the first streamed MiB. */
    xts_crypt(cipher, key, iv, in, XTS_MEBIBYTE, unit, XTS_MEBIBYTE, 1);
    unpoison_all();
    CHECK(memcmp(unit, ct, XTS_MEBIBYTE) == 0,
          "the first 1 MiB stream update is not an independent data unit "
          "with the given data-unit number");
    repoison_all();
    EVP_CIPHER_free(cipher);

    free_poison_allocations();
}
