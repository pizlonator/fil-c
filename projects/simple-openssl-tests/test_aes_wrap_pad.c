#include "poisoning.h"

#include <openssl/evp.h>

/*
 * test_aes_wrap_pad -- AES key wrap with padding (RFC 5649) tests.
 *
 * RFC 5649 pads the key data with zeros to the next 64-bit boundary and
 * prepends a 4-byte fixed field (A65959A6) plus the 32-bit plaintext length
 * as the "alternative initial value"; a plaintext shorter than 8 bytes takes
 * the single-block AIV path (crypto/modes/wrap128.c: "If the padded
 * plaintext contains exactly eight octets, then prepend the AIV and encrypt
 * the resulting 128-bit block using AES in ECB mode"), longer ones go
 * through the RFC 3394 wrap.  The wrapped length is always roundup8(n) + 8,
 * which this program asserts exactly for every size (and for the -INV
 * variants too: they apply the inverse AES block transform per SP 800-38F
 * section 5.1 while keeping the same block structure, so the output sizes
 * are identical).
 *
 * Known-answer vectors: RFC 5649 section 6 publishes a single worked example
 * (a 192-bit KEK wrapping 20 and 7 octets of key data).  The AES-256-WRAP-PAD
 * and AES-192/256-WRAP-PAD-INV constants below have no published counterpart
 * and were derived as follows: with a scratch probe against the
 * authoritative pizfix OpenSSL 3.6.5 and cross-checked byte for byte against
 * the system OpenSSL 3.0.13; the AES-256-WRAP-PAD and AES-192-WRAP-PAD-INV
 * constants were additionally reproduced with the pizfix `openssl enc` CLI
 * (which does accept the wrap-pad cipher names).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_INPUT(size_t n)  { return poison_alloc(POISON_INPUT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_aes_wrap_pad: %s\n", msg);
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

/* RFC 5649 section 6 examples (192-bit KEK). */
static const char *const KEK192_HEX =
    "5840df6e29b02af1ab493b705bf16ea1ae8338f4dcc176a8";
static const char *const K20_HEX =
    "c37b7e6492584340bed12207808941155068f738";
static const char *const W20_HEX =
    "138bdeaa9b8fa7fc61f97742e72248ee"
    "5ae6ae5360d1ae6a5f54f373fa543b6a";
static const char *const K7_HEX =
    "466f7250617369";               /* "ForPasi" */
static const char *const W7_HEX =
    "afbeb0f07dfbf5419200f2ccb50bb24f";

/* Derived constants (no published RFC 5649 counterpart): the same 20 octets
 * of key data with a 256-bit KEK, and the -INV variants' wrap direction of
 * the same 20 octets.  Provenance: scratch probe against pizfix OpenSSL
 * 3.6.5, cross-checked byte for byte against system OpenSSL 3.0.13 (and, for
 * the first and third value, against the pizfix `openssl enc` CLI). */
static const char *const KEK256_HEX =
    "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";
static const char *const W20_KEK256_HEX =    /* AES-256-WRAP-PAD(K20) */
    "29b7fa191c2165684374eee9f74595e2a42bace75c425b3053efa26ffe1bb32f";
static const char *const WINV192_K20_HEX =   /* AES-192-WRAP-PAD-INV(K20) */
    "8cc17eb3bd1ec6618deeced26327b43c11724a755ce717694d99bb1829ba53f8";
static const char *const WINV256_K20_HEX =   /* AES-256-WRAP-PAD-INV(K20) */
    "d39d42cc4f1fd43d58c4583b2e2bfc5ddea02af2d0f6b6ea67700bc1ae6d8ea7";

/* Wrap-pad `inlen` bytes in one update; returns the wrapped length.  `out`
 * must have room for roundup8(inlen) + 8 + 8 bytes. */
static size_t wrap_pad(const EVP_CIPHER *cipher, const unsigned char *kek,
                       const unsigned char *in, size_t inlen,
                       unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    size_t expect = ((inlen + 7) / 8) * 8 + 8;

    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(expect + 8 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "wrap-pad EVP_EncryptUpdate failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "wrap-pad EVP_EncryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* Unwrap-pad `inlen` bytes in one update; returns the key data length. */
static size_t unwrap_pad(const EVP_CIPHER *cipher, const unsigned char *kek,
                         const unsigned char *in, size_t inlen,
                         unsigned char *out, size_t outcap)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, flen = 0;
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(inlen + 8 <= outcap,
          "output buffer smaller than the EVP update contract requires");
    CHECK(EVP_DecryptUpdate(ctx, out, &len, in, (int)inlen) == 1,
          "unwrap-pad EVP_DecryptUpdate failed");
    CHECK(EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1,
          "unwrap-pad EVP_DecryptFinal_ex failed");
    len += flen;
    EVP_CIPHER_CTX_free(ctx);
    return (size_t)len;
}

/* (1) The RFC 5649 section 6 KATs, wrap and unwrap directions, plus the
 * derived AES-256-WRAP-PAD and -INV KATs. */
static void test_kats(void)
{
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-192-WRAP-PAD", NULL);
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *k20 = ALLOCATE_INPUT(20);
    unsigned char *w20_want = ALLOCATE_OUTPUT(32);
    unsigned char *k7 = ALLOCATE_INPUT(8);
    unsigned char *w7_want = ALLOCATE_OUTPUT(16);
    unsigned char *wrap = ALLOCATE_OUTPUT(32 + 8);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 8);
    size_t w20len, w7len, wlen, backlen;

    /* AES key wrap with padding is core OpenSSL 3 functionality: never
     * SKIP, only FAIL. */
    CHECK(cipher != NULL,
          "EVP_CIPHER_fetch failed for a core AES-WRAP-PAD cipher");
    from_hex(KEK192_HEX, kek, 24);
    from_hex(K20_HEX, k20, 20);
    w20len = from_hex(W20_HEX, w20_want, 32);
    from_hex(K7_HEX, k7, 8);
    w7len = from_hex(W7_HEX, w7_want, 16);

    wlen = wrap_pad(cipher, kek, k20, 20, wrap, 32 + 8);
    CHECK(wlen == 32, "wrapping 20 octets did not give 32 bytes");
    expect_equal("20-octet wrapped output does not match the RFC 5649 KAT",
                 wrap, wlen, w20_want, w20len);
    backlen = unwrap_pad(cipher, kek, w20_want, w20len, back, 32 + 8);
    expect_equal("unwrapping the 20-octet KAT did not give the key data",
                 back, backlen, k20, 20);

    wlen = wrap_pad(cipher, kek, k7, 7, wrap, 16 + 8);
    CHECK(wlen == 16, "wrapping 7 octets did not give 16 bytes");
    expect_equal("7-octet wrapped output does not match the RFC 5649 KAT",
                 wrap, wlen, w7_want, w7len);
    backlen = unwrap_pad(cipher, kek, w7_want, w7len, back, 16 + 8);
    expect_equal("unwrapping the 7-octet KAT did not give the key data",
                 back, backlen, k7, 7);
    EVP_CIPHER_free(cipher);

    /* Derived AES-256-WRAP-PAD KAT: same 20 octets, 256-bit KEK. */
    cipher = EVP_CIPHER_fetch(NULL, "AES-256-WRAP-PAD", NULL);
    CHECK(cipher != NULL,
          "EVP_CIPHER_fetch(\"AES-256-WRAP-PAD\") failed");
    from_hex(KEK256_HEX, kek, 32);
    wlen = wrap_pad(cipher, kek, k20, 20, wrap, 32 + 8);
    CHECK(wlen == 32, "AES-256-WRAP-PAD of 20 octets did not give 32 bytes");
    w20len = from_hex(W20_KEK256_HEX, w20_want, 32);
    expect_equal("20-octet AES-256 wrapped output does not match the "
                 "derived KAT", wrap, wlen, w20_want, w20len);
    backlen = unwrap_pad(cipher, kek, w20_want, w20len, back, 32 + 8);
    expect_equal("unwrapping the derived AES-256 KAT did not give the key data",
                 back, backlen, k20, 20);
    EVP_CIPHER_free(cipher);

    /* Derived -INV KATs: the inverse ciphers wrap with the inverse AES
     * block transform, so ENC(K20) is a fresh constant per key size and
     * DEC recovers K20. */
    {
        static const struct {
            const char *ciphername;
            const char *kekhex;
            size_t keklen;
            const char *kathex;
        } invkats[] = {
            { "AES-192-WRAP-PAD-INV", KEK192_HEX, 24, WINV192_K20_HEX },
            { "AES-256-WRAP-PAD-INV", KEK256_HEX, 32, WINV256_K20_HEX },
        };
        size_t v;

        for (v = 0; v < sizeof(invkats) / sizeof(invkats[0]); v++) {
            EVP_CIPHER *cinv = EVP_CIPHER_fetch(NULL, invkats[v].ciphername,
                                                NULL);

            CHECK(cinv != NULL, "EVP_CIPHER_fetch failed for a core "
                  "AES-WRAP-PAD-INV cipher");
            from_hex(invkats[v].kekhex, kek, 32);
            wlen = wrap_pad(cinv, kek, k20, 20, wrap, 32 + 8);
            CHECK(wlen == 32, "inverse wrap-pad of 20 octets did not give "
                  "32 bytes");
            w20len = from_hex(invkats[v].kathex, w20_want, 32);
            expect_equal("20-octet inverse wrapped output does not match the "
                         "derived KAT", wrap, wlen, w20_want, w20len);
            backlen = unwrap_pad(cinv, kek, w20_want, w20len, back, 32 + 8);
            expect_equal("unwrapping the derived -INV KAT did not give the "
                         "key data", back, backlen, k20, 20);
            EVP_CIPHER_free(cinv);
        }
    }
}

/* (2) Plaintext sizes {1,4,7,8,13,20,255} round-trip and the wrapped
 * length is exactly roundup8(n) + 8 (asserted, not just observed) -- for
 * AES-128/192/256-WRAP-PAD. */
static void test_size_roundtrips(void)
{
    static const char *const names[] = {
        "AES-128-WRAP-PAD", "AES-192-WRAP-PAD", "AES-256-WRAP-PAD"
    };
    static const size_t sizes[] = { 1, 4, 7, 8, 13, 20, 255 };
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(255);
    unsigned char *out = ALLOCATE_OUTPUT(264 + 8);
    unsigned char *back = ALLOCATE_OUTPUT(264 + 8);
    size_t s, m, i, keklen, wlen, backlen;
    char msg[128];

    for (i = 0; i < 255; i++)
        in[i] = (unsigned char)(i * 17 + 3);

    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-WRAP-PAD cipher");
        keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
        for (i = 0; i < keklen; i++)
            kek[i] = (unsigned char)(0xD0 + i);

        for (s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            size_t n = sizes[s];
            size_t expect = ((n + 7) / 8) * 8 + 8;

            wlen = wrap_pad(cipher, kek, in, n, out, 264 + 8);
            snprintf(msg, sizeof(msg),
                     "wrapping %zu octets did not give %zu bytes", n, expect);
            CHECK(wlen == expect, msg);
            backlen = unwrap_pad(cipher, kek, out, wlen, back, 264 + 8);
            snprintf(msg, sizeof(msg),
                     "%s %zu-octet wrap/unwrap roundtrip failed", names[m], n);
            expect_equal(msg, back, backlen, in, n);
        }
        EVP_CIPHER_free(cipher);
    }
}

/* (3) A corrupted wrapped output must fail to unwrap-pad, whether the
 * corruption hits the AIV block or the padding block -- for all three key
 * sizes. */
static void test_corrupted_unwrap(void)
{
    static const char *const names[] = {
        "AES-128-WRAP-PAD", "AES-192-WRAP-PAD", "AES-256-WRAP-PAD"
    };
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *in = ALLOCATE_INPUT(13);
    unsigned char *out = ALLOCATE_OUTPUT(16 + 16 + 8);
    unsigned char *back = ALLOCATE_OUTPUT(16 + 16 + 8);
    EVP_CIPHER_CTX *e, *ctx;
    int len = 0, flen = 0;
    size_t m, i, keklen, off;

    for (i = 0; i < 13; i++)
        in[i] = (unsigned char)(i * 3 + 9);

    /* 13 octets wrap to 24 bytes; corrupt the AIV block, the middle and
     * the last (padding) byte in turn. */
    for (m = 0; m < sizeof(names) / sizeof(names[0]); m++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, names[m], NULL);

        CHECK(cipher != NULL, "EVP_CIPHER_fetch failed for a core AES-WRAP-PAD cipher");
        keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
        for (i = 0; i < keklen; i++)
            kek[i] = (unsigned char)(0xD0 + i);

        for (i = 0; i < 3; i++) {
            off = (i == 0) ? 0 : (i == 1) ? 12 : 23;

            e = EVP_CIPHER_CTX_new();
            CHECK(e != NULL, "EVP_CIPHER_CTX_new failed");
            CHECK(EVP_EncryptInit_ex(e, cipher, NULL, kek, NULL) == 1,
                  "EVP_EncryptInit_ex failed");
            CHECK(EVP_EncryptUpdate(e, out, &len, in, 13) == 1,
                  "wrap-pad update failed");
            CHECK(EVP_EncryptFinal_ex(e, out + len, &flen) == 1,
                  "wrap-pad final failed");
            EVP_CIPHER_CTX_free(e);
            len = 0;
            flen = 0;

            out[off] ^= 0x01;
            ctx = EVP_CIPHER_CTX_new();
            CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
            CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
                  "EVP_DecryptInit_ex failed");
            CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 24) != 1,
                  "unwrap-pad accepted corrupted key material");
            EVP_CIPHER_CTX_free(ctx);
            out[off] ^= 0x01;
            len = 0;
            flen = 0;
        }
        EVP_CIPHER_free(cipher);
    }
}

/* (4) Feeding a plain RFC 3394 wrapped key (no AIV) to unwrap-pad must
 * fail: the recovered initial value will not be A65959A6 || len. */
static void test_plain_wrap_rejected(void)
{
    EVP_CIPHER *wrap = EVP_CIPHER_fetch(NULL, "AES-128-WRAP", NULL);
    EVP_CIPHER *wpad = EVP_CIPHER_fetch(NULL, "AES-128-WRAP-PAD", NULL);
    unsigned char *kek = ALLOCATE_KEY(16);
    unsigned char *in = ALLOCATE_INPUT(16);
    unsigned char *out = ALLOCATE_OUTPUT(24 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(24 + 16);
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;
    size_t i;

    CHECK(wrap != NULL && wpad != NULL,
          "EVP_CIPHER_fetch failed for a core AES-WRAP cipher");
    for (i = 0; i < 16; i++)
        kek[i] = (unsigned char)(0xD0 + i);
    for (i = 0; i < 16; i++)
        in[i] = (unsigned char)(i * 7 + 1);

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_EncryptInit_ex(ctx, wrap, NULL, kek, NULL) == 1,
          "EVP_EncryptInit_ex failed");
    CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 16) == 1,
          "plain wrap update failed");
    CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
          "plain wrap final failed");
    EVP_CIPHER_CTX_free(ctx);
    CHECK(len == 24, "plain wrap of 16 bytes did not give 24 bytes");

    ctx = EVP_CIPHER_CTX_new();
    CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
    CHECK(EVP_DecryptInit_ex(ctx, wpad, NULL, kek, NULL) == 1,
          "EVP_DecryptInit_ex failed");
    CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 24) != 1,
          "unwrap-pad accepted a plain wrapped key (no AIV)");
    EVP_CIPHER_CTX_free(ctx);
    EVP_CIPHER_free(wrap);
    EVP_CIPHER_free(wpad);
}

/* (5) The inverse variants round-trip where the build offers them, obey the
 * same roundup8(n) + 8 output-size rule, and reject corrupted wrapping.
 * The 192/256 variants additionally match their derived KATs (see the
 * provenance note at the top); RFC 5649 has no example for them. */
static void test_wrap_pad_inv(void)
{
    static const struct {
        const char *ciphername;
        const char *kekhex;   /* NULL: KEK filled with a pattern */
        const char *kathex;   /* derived ENC(K20) constant, or NULL */
    } cases[] = {
        { "AES-128-WRAP-PAD-INV", NULL, NULL },
        { "AES-192-WRAP-PAD-INV", KEK192_HEX, WINV192_K20_HEX },
        { "AES-256-WRAP-PAD-INV", KEK256_HEX, WINV256_K20_HEX },
    };
    unsigned char *kek = ALLOCATE_KEY(32);
    unsigned char *k20 = ALLOCATE_INPUT(20);
    unsigned char *in = ALLOCATE_INPUT(13);
    unsigned char *out = ALLOCATE_OUTPUT(32 + 16);
    unsigned char *back = ALLOCATE_OUTPUT(32 + 16);
    size_t c, i, keklen, wlen, backlen, k20_wantlen;
    EVP_CIPHER_CTX *ctx;
    int len = 0, flen = 0;

    from_hex(K20_HEX, k20, 20);
    for (i = 0; i < 13; i++)
        in[i] = (unsigned char)(i * 5 + 1);

    for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, cases[c].ciphername, NULL);

        if (cipher == NULL) {
            printf("SKIP test_aes_wrap_pad: %s is unavailable\n",
                   cases[c].ciphername);
            return;
        }
        if (cases[c].kekhex != NULL)
            from_hex(cases[c].kekhex, kek, 32);
        else {
            keklen = (size_t)EVP_CIPHER_get_key_length(cipher);
            for (i = 0; i < keklen; i++)
                kek[i] = (unsigned char)(0xD0 + i);
        }

        /* KAT: wrapping the 20 RFC 5649 key-data octets gives the derived
         * constant and unwraps back to K20. */
        if (cases[c].kathex != NULL) {
            k20_wantlen = from_hex(cases[c].kathex, out, 32);
            wlen = wrap_pad(cipher, kek, k20, 20, back, 32 + 16);
            CHECK(wlen == 32, "inverse wrap-pad of 20 octets did not give "
                  "32 bytes");
            expect_equal("inverse wrap-pad output does not match the "
                         "derived KAT", back, wlen, out, k20_wantlen);
            backlen = unwrap_pad(cipher, kek, out, k20_wantlen, back,
                                 32 + 16);
            expect_equal("unwrapping the derived -INV KAT did not give the "
                         "key data", back, backlen, k20, 20);
        }

        /* 13 octets wrap to exactly 24 bytes and round-trip. */
        wlen = wrap_pad(cipher, kek, in, 13, out, 32 + 16);
        CHECK(wlen == 24, "inverse wrap-pad of 13 octets did not give 24 bytes");
        backlen = unwrap_pad(cipher, kek, out, wlen, back, 32 + 16);
        expect_equal("inverse wrap-pad roundtrip failed", back, backlen,
                     in, 13);

        /* Corrupting the last (padding/length) byte must fail the unwrap. */
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_EncryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
              "EVP_EncryptInit_ex failed");
        CHECK(EVP_EncryptUpdate(ctx, out, &len, in, 13) == 1,
              "wrap-pad update failed");
        CHECK(EVP_EncryptFinal_ex(ctx, out + len, &flen) == 1,
              "wrap-pad final failed");
        EVP_CIPHER_CTX_free(ctx);
        len = 0;
        flen = 0;
        out[23] ^= 0x01;
        ctx = EVP_CIPHER_CTX_new();
        CHECK(ctx != NULL, "EVP_CIPHER_CTX_new failed");
        CHECK(EVP_DecryptInit_ex(ctx, cipher, NULL, kek, NULL) == 1,
              "EVP_DecryptInit_ex failed");
        CHECK(EVP_DecryptUpdate(ctx, back, &len, out, 24) != 1,
              "inverse unwrap-pad accepted corrupted key material");
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
    }
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_size_roundtrips();
    test_corrupted_unwrap();
    test_plain_wrap_rejected();
    test_wrap_pad_inv();

    printf("test_aes_wrap_pad: all %u checks passed\n", g_checks);
    return 0;
}
