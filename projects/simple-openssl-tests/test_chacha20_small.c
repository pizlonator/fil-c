#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_chacha20_small -- ChaCha20 (default provider) known-answer and
 * behavior tests for small inputs.  (A sibling program covers large
 * payloads and the 32-bit counter-wrap path.)
 *
 * In the EVP the ChaCha20 "IV" is 16 bytes: the 4-byte LITTLE-ENDIAN
 * block counter followed by the 12-byte nonce (cipher_chacha20_hw.c
 * chacha20_initiv: "ctx->counter[i/4] = CHACHA_U8TOU32(bctx->oiv + i)").
 *
 * Known-answer vectors: RFC 8439 section 2.4.2 -- key 000102...1f,
 * nonce 000000090000004a00000000, initial counter 1.  The 64-byte
 * keystream below is the RFC's block output (verified by encrypting 64
 * zero bytes and by the first block of the message ciphertext), and the
 * 110-byte ciphertext is the RFC's test message under the same
 * parameters.  Every constant was verified against the authoritative
 * pizfix OpenSSL 3.6.5 CLI,
 *
 *   printf '<pt hex>' | xxd -r -p | pizfix/bin/openssl enc -chacha20 \
 *       -K <keyhex> -iv 01000000000000090000004a00000000
 *
 * and cross-checked against the system OpenSSL 3.0.13 (identical).
 *
 * Behavior: ChaCha20 keeps a PARTIAL-KEYSTREAM buffer between updates --
 * an update that ends mid-64-byte-block stashes the leftover keystream
 * in ctx->buf/partial_len and consumes it on the next update
 * (cipher_chacha20_hw.c:45-61 and 98-110), so any chunk split must
 * reproduce the one-shot output exactly.
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
    fprintf(stderr, "FAIL test_chacha20_small: %s\n", msg);
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
    "000102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f";
/* counter = 1 (little-endian) || nonce 000000090000004a00000000 */
static const char *const IV_HEX =
    "01000000000000090000004a00000000";

/* RFC 8439 2.4.2 keystream block (first 64 bytes of encrypting zeros). */
static const char *const KEYSTREAM64_HEX =
    "10f1e7e4d13b5915500fdd1fa32071c4"
    "c7d1f4c733c068030422aa9ac3d46c4e"
    "d2826446079faa0914c2d705d98b02a2"
    "b5129cd1de164eb9cbd083e8a2503c4e";

/* The RFC 8439 2.4.2 test message (110 bytes) and its ciphertext. */
static const char *const MSG_HEX =
    "4c616469657320616e642047656e746c656d656e206f662074686520726f756e64"
    "207461626c657320746f6f2070726f7465637420746865206578636c7573697665"
    "2073746174656d656e7473206c696b652074686573652061726520777261707065"
    "6420696e20302066656172";
static const char *const MSG_CT_HEX =
    "5c90838db44879743e6bfd58c64e05a8"
    "a2bc91a913af0e23704acfbab1bb1920"
    "b6a2102765f3cf7a34b6b86af9fb70cd"
    "c177ffa5fe6226dcebb5fb8bce254f27"
    "7ceda3044db6cb2b95a9c2c4990bd5bf"
    "f633e3e0c2506e9885de37fa2a59ed89"
    "97519df67590aaa68f0101f9094f";

/* One-shot encrypt/decrypt; ChaCha20 is a stream cipher, so Final emits
 * nothing and the update can produce at most inl bytes.  Returns the
 * output length. */
static size_t chacha20_crypt(const EVP_CIPHER *cipher, const unsigned char *key,
                             const unsigned char *iv, const unsigned char *in,
                             size_t inlen, unsigned char *out, size_t outcap,
                             int enc)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    if (enc)
        CHECK(EVP_EncryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_EncryptInit_ex2 failed");
    else
        CHECK(EVP_DecryptInit_ex2(ctx, cipher, key, iv, NULL) == 1,
              "EVP_DecryptInit_ex2 failed");
    CHECK(inlen <= outcap,
          "output buffer smaller than the EVP update contract requires");
    if (inlen)
        CHECK(EVP_CipherUpdate(ctx, out, &len, in, (int)inlen) == 1,
              "EVP_CipherUpdate failed");
    CHECK(EVP_CipherFinal_ex(ctx, out + len, &flen) == 1,
          "EVP_CipherFinal_ex failed");
    CHECK(flen == 0, "ChaCha20 Final emitted unexpected bytes");
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Feed `inlen` bytes through updates whose lengths cycle through `chunks`
 * (last one truncated).  Must reproduce the one-shot output exactly
 * (partial keystream buffer state). */
static size_t chacha20_chunked(const EVP_CIPHER *cipher, const unsigned char *key,
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
    CHECK(inlen + 64 <= outcap,
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

/* (1) The RFC 8439 2.4.2 KATs: the 64-byte keystream (encrypting zeros)
 * and the 110-byte message ciphertext; decrypting the ciphertext
 * recovers the message. */
static void test_kats(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *zeros = ALLOCATE_INPUT(64);
    unsigned char *ks_want = ALLOCATE_OUTPUT(64);
    unsigned char *ks = ALLOCATE_OUTPUT(64);
    unsigned char *msg = ALLOCATE_INPUT(110);
    unsigned char *ct_want = ALLOCATE_OUTPUT(110);
    unsigned char *ct = ALLOCATE_OUTPUT(110);
    unsigned char *back = ALLOCATE_OUTPUT(110);
    size_t ct_wantlen, ctlen;

    /* ChaCha20 is core OpenSSL 3 functionality: never SKIP, only FAIL. */
    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20\") failed");
    CHECK((size_t)EVP_CIPHER_get_key_length(cipher) == 32,
          "ChaCha20 key length is not 32");
    CHECK((size_t)EVP_CIPHER_get_iv_length(cipher) == 16,
          "ChaCha20 IV length is not 16 (counter || nonce)");
    from_hex(KEY_HEX, key, 32);
    from_hex(IV_HEX, iv, 16);
    memset(zeros, 0, 64);
    from_hex(KEYSTREAM64_HEX, ks_want, 64);
    from_hex(MSG_HEX, msg, 110);
    ct_wantlen = from_hex(MSG_CT_HEX, ct_want, 110);

    ctlen = chacha20_crypt(cipher, key, iv, zeros, 64, ks, 64, 1);
    expect_equal("the 64-byte keystream does not match the RFC 8439 KAT",
                 ks, ctlen, ks_want, 64);

    ctlen = chacha20_crypt(cipher, key, iv, msg, 110, ct, 110, 1);
    expect_equal("the message ciphertext does not match the RFC 8439 KAT",
                 ct, ctlen, ct_want, ct_wantlen);

    ctlen = chacha20_crypt(cipher, key, iv, ct_want, ct_wantlen, back, 110, 0);
    expect_equal("decrypting the KAT ciphertext did not give the message",
                 back, ctlen, msg, 110);
    EVP_CIPHER_free(cipher);
}

/* (2) One-shot vs chunked equality for sizes around the 64-byte keystream
 * block boundary {0, 1, 63, 64, 65, 127, 130} (the partial keystream
 * buffer bridges uneven updates), for both directions. */
static void test_sizes(void)
{
    static const size_t sizes[] = { 0, 1, 63, 64, 65, 127, 130 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(130);
    unsigned char *oneshot = ALLOCATE_OUTPUT(130);
    unsigned char *chunked = ALLOCATE_OUTPUT(130 + 64);
    static const size_t chunks[] = { 5, 64, 17, 1 };
    size_t s, i, olen, clen;
    char msg[128];

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(IV_HEX, iv, 16);
    for (i = 0; i < 130; i++)
        in[i] = (unsigned char)(i * 7 + 3);

    for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        size_t n = sizes[s];

        olen = chacha20_crypt(cipher, key, iv, in, n, oneshot, 130, 1);
        clen = chacha20_chunked(cipher, key, iv, in, n, chunked, 130 + 64, 1,
                                chunks, 4);
        snprintf(msg, sizeof(msg),
                 "chunked ChaCha20 differs from one-shot at %zu bytes (encrypt)",
                 n);
        expect_equal(msg, chunked, clen, oneshot, olen);

        clen = chacha20_chunked(cipher, key, iv, oneshot, olen, chunked,
                                130 + 64, 0, chunks, 4);
        snprintf(msg, sizeof(msg),
                 "chunked ChaCha20 decrypt did not recover the plaintext at "
                 "%zu bytes", n);
        expect_equal(msg, chunked, clen, in, n);
    }
    EVP_CIPHER_free(cipher);
}

/* (3) The explicit multi-update grid {3, 61, 64, 1} over the 110-byte KAT
 * message: 3 + 61 = 64 (exactly one block), then 64 more, then the final
 * byte.  Must equal the one-shot KAT ciphertext. */
static void test_multi_update(void)
{
    static const size_t chunks[] = { 3, 61, 64, 1 };
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *msg = ALLOCATE_INPUT(110);
    unsigned char *ct_want = ALLOCATE_OUTPUT(110);
    unsigned char *ct = ALLOCATE_OUTPUT(110 + 64);
    size_t ct_wantlen, ctlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(IV_HEX, iv, 16);
    from_hex(MSG_HEX, msg, 110);
    ct_wantlen = from_hex(MSG_CT_HEX, ct_want, 110);

    ctlen = chacha20_chunked(cipher, key, iv, msg, 110, ct, 110 + 64, 1,
                             chunks, 4);
    expect_equal("the {3,61,64,1} multi-update ciphertext does not match the "
                 "KAT", ct, ctlen, ct_want, ct_wantlen);
    EVP_CIPHER_free(cipher);
}

/* (4) Encrypt/decrypt roundtrips (stream cipher: decrypt is encrypt). */
static void test_roundtrip(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "ChaCha20", NULL);
    unsigned char *key = ALLOCATE_KEY(32);
    unsigned char *iv = ALLOCATE_IV(16);
    unsigned char *in = ALLOCATE_INPUT(100);
    unsigned char *ct = ALLOCATE_OUTPUT(100);
    unsigned char *back = ALLOCATE_OUTPUT(100);
    size_t i, ctlen, backlen;

    CHECK(cipher != NULL, "EVP_CIPHER_fetch(\"ChaCha20\") failed");
    from_hex(KEY_HEX, key, 32);
    from_hex(IV_HEX, iv, 16);
    for (i = 0; i < 100; i++)
        in[i] = (unsigned char)(0xC0 - i);

    ctlen = chacha20_crypt(cipher, key, iv, in, 100, ct, 100, 1);
    CHECK(ctlen == 100, "ChaCha20 encryption of 100 bytes != 100 bytes");
    backlen = chacha20_crypt(cipher, key, iv, ct, ctlen, back, 100, 0);
    expect_equal("ChaCha20 decrypt roundtrip failed", back, backlen, in, 100);
    EVP_CIPHER_free(cipher);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_sizes();
    test_multi_update();
    test_roundtrip();

    printf("test_chacha20_small: all %u checks passed\n", g_checks);
    return 0;
}
