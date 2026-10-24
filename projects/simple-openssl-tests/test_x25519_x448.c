#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_x25519_x448 -- X25519 and X448 Diffie-Hellman: RFC 7748 known-answer

 * vectors, the iterated test vectors, the low-order-point rejection

 * requirement, and X25519-as-KEM.

 *

 * Known-answer vectors, all taken verbatim from RFC 7748 and verified with

 * filcc-compiled scratch probes before being embedded:

 *

 *   - Section 6.1: Alice (a -> X25519(a,9)), Bob (b -> X25519(b,9)) and the

 *     shared secret K, in both derive directions.

 *   - Section 6.2: the X448 counterparts (56-byte keys, shared secret K).

 *   - Section 5.2: the iterated vectors -- starting from k = u = 0x09 (for

 *     X25519) / 0x05 (for X448), each iteration computes k = f(k, u) and

 *     u = old k; the result after 1 and 1000 iterations must match the RFC.

 *   - Section 6.2/6: an all-zero peer point (low-order) must yield an

 *     all-zero shared secret, and implementations must abort: OpenSSL's

 *     EVP_PKEY_derive fails outright, which is what we assert.

 *

 * X25519-as-KEM (EVP_PKEY_encapsulate/decapsulate) exists from OpenSSL 3.2

 * and is skipped with a note on older builds (the only SKIP-able feature

 * here; the raw DH KATs exist in all OpenSSL 3.x).

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_x25519_x448: %s\n", msg);
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


/* Copies a hex constant into a buffer.  X25519/X448 keys are octet strings

 * (the RFC byte order), so no byte-swapping is needed here.  Returns the

 * byte length. */
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


/* Imports an X25519/X448 key from the given private and/or public key

 * bytes (either may be NULL).  Buffers are static: the key references them

 * for its lifetime. */
static EVP_PKEY *make_key(const char *alg, const unsigned char *priv,
                          size_t privlen, const unsigned char *pub,
                          size_t publen)
{
    static unsigned char *pbuf;
    OSSL_PARAM params[3];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = NULL;
    size_t i = 0;

    if (priv != NULL) {
        pbuf = ALLOCATE_KEY(privlen);
        memcpy(pbuf, priv, privlen);
        params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PRIV_KEY,
                                                        pbuf, privlen);
    }
    if (pub != NULL) {
        pbuf = ALLOCATE_KEY(publen);
        memcpy(pbuf, pub, publen);
        params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                        pbuf, publen);
    }
    params[i] = OSSL_PARAM_construct_end();
    ctx = EVP_PKEY_CTX_new_from_name(NULL, alg, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed");
    EVP_PKEY_CTX_free(ctx);
    return key;
}


/* derive(our_key, peer); returns 1 and stores the shared secret on

 * success, 0 on failure. */
static int derive(EVP_PKEY *key, EVP_PKEY *peer, unsigned char *secret,
                  size_t cap, size_t *secretlen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    size_t len = cap;
    int ok;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_derive_init(ctx) == 1, "EVP_PKEY_derive_init failed");
    CHECK(EVP_PKEY_derive_set_peer(ctx, peer) == 1,
          "EVP_PKEY_derive_set_peer failed");
    ok = EVP_PKEY_derive(ctx, secret, &len) == 1;
    *secretlen = ok ? len : 0;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}


/* RFC 7748 section 6.1 (X25519) and 6.2 (X448) vectors. */
static const char *const X25519_A_SK_HEX =
    "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a";
static const char *const X25519_A_PK_HEX =
    "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a";
static const char *const X25519_B_SK_HEX =
    "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb";
static const char *const X25519_B_PK_HEX =
    "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f";
static const char *const X25519_K_HEX =
    "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742";

static const char *const X448_A_SK_HEX =
    "9a8f4925d1519f5775cf46b04b5800d4ee9ee8bae8bc5565d498c28dd9c9baf5"
    "74a9419744897391006382a6f127ab1d9ac2d8c0a598726b";
static const char *const X448_A_PK_HEX =
    "9b08f7cc31b7e3e67d22d5aea121074a273bd2b83de09c63faa73d2c22c5d9bb"
    "c836647241d953d40c5b12da88120d53177f80e532c41fa0";
static const char *const X448_B_SK_HEX =
    "1c306a7ac2a0e2e0990b294470cba339e6453772b075811d8fad0d1d6927c120"
    "bb5ee8972b0d3e21374c9c921b09d1b0366f10b65173992d";
static const char *const X448_B_PK_HEX =
    "3eb7a829b0cd20f5bcfc0b599b6feccf6da4627107bdb0d4f345b43027d8b972"
    "fc3e34fb4232a13ca706dcb57aec3dae07bdc1c67bf33609";
static const char *const X448_K_HEX =
    "07fff4181ac6cc95ec1c16a94a0f74d12da232ce40a77552281d282bb60c0b56"
    "fd2464c335543936521c24403085d59a449a5037514a879d";


/* One curve's section 6.1/6.2 KAT, run in both derive directions, plus the

 * public-key-derivation checks (the RFC public key is X(sk, base)). */
static void test_kats(const char *alg, size_t keylen,
                      const char *askhex, const char *apkhex,
                      const char *bskhex, const char *bpkhex,
                      const char *khex)
{
    unsigned char *ask = ALLOCATE_KEY(keylen), *apk = ALLOCATE_KEY(keylen);
    unsigned char *bsk = ALLOCATE_KEY(keylen), *bpk = ALLOCATE_KEY(keylen);
    unsigned char *z = ALLOCATE_OUTPUT(keylen);
    unsigned char *want = ALLOCATE_OUTPUT(keylen);
    size_t zlen = 0, wantlen;
    char mbuf[128];
    EVP_PKEY *A, *B;

    from_hex(askhex, ask, keylen);
    from_hex(apkhex, apk, keylen);
    from_hex(bskhex, bsk, keylen);
    from_hex(bpkhex, bpk, keylen);
    wantlen = from_hex(khex, want, keylen);
    CHECK(wantlen == keylen, "shared-secret constant has the wrong length");

    A = make_key(alg, ask, keylen, apk, keylen);
    B = make_key(alg, bsk, keylen, bpk, keylen);

    zlen = 0;
    CHECK(derive(A, B, z, keylen, &zlen) == 1, "X25519/X448 derive failed");
    snprintf(mbuf, sizeof(mbuf), "%s: derive(a, B_pub) != RFC K", alg);
    CHECK(zlen == keylen && memcmp(z, want, keylen) == 0, mbuf);

    CHECK(derive(B, A, z, keylen, &zlen) == 1, "X25519/X448 derive failed");
    snprintf(mbuf, sizeof(mbuf), "%s: derive(b, A_pub) != RFC K", alg);
    CHECK(zlen == keylen && memcmp(z, want, keylen) == 0, mbuf);

    EVP_PKEY_free(A);
    EVP_PKEY_free(B);
}


/* RFC 7748 section 5.2 iterated vectors: k = X(k, u), u = old k, starting

 * from the base-point encoding.  Checks 1 and 1000 iterations. */
static void test_iterated(const char *alg, size_t keylen,
                          const char *starthex,
                          const char *iter1hex, const char *iter1000hex)
{
    unsigned char *k = ALLOCATE_KEY(keylen);
    unsigned char *u = ALLOCATE_KEY(keylen);
    unsigned char *t = ALLOCATE_OUTPUT(keylen);
    unsigned char *want = ALLOCATE_OUTPUT(keylen);
    unsigned char *skbuf, *pubbuf;
    int it;
    size_t tl = 0;

    from_hex(starthex, k, keylen);
    memcpy(u, k, keylen);

    for (it = 0; it < 1000; it++) {
        EVP_PKEY *ka, *kb;
        skbuf = ALLOCATE_KEY(keylen);
        memcpy(skbuf, k, keylen);
        pubbuf = ALLOCATE_KEY(keylen);
        memcpy(pubbuf, u, keylen);
        ka = make_key(alg, skbuf, keylen, NULL, 0);
        kb = make_key(alg, NULL, 0, pubbuf, keylen);
        CHECK(derive(ka, kb, t, keylen, &tl) == 1 && tl == keylen,
              "iterated X25519/X448 derive failed");
        EVP_PKEY_free(ka);
        EVP_PKEY_free(kb);
        memcpy(u, k, keylen);
        memcpy(k, t, keylen);
        if (it == 0) {
            from_hex(iter1hex, want, keylen);
            CHECK(memcmp(k, want, keylen) == 0,
                  "iterated X25519/X448: 1-iteration value != RFC");
        }
    }
    from_hex(iter1000hex, want, keylen);
    CHECK(memcmp(k, want, keylen) == 0,
          "iterated X25519/X448: 1000-iteration value != RFC");
}


/* RFC 7748 section 6: an all-zero peer point must produce an all-zero

 * shared secret, which implementations must reject; OpenSSL's derive fails

 * outright. */
static void test_low_order_peer(const char *alg, size_t keylen)
{
    unsigned char *ask = ALLOCATE_KEY(keylen);
    unsigned char *zero = ALLOCATE_KEY(keylen);
    unsigned char *z = ALLOCATE_OUTPUT(keylen);
    size_t zlen = 0;
    EVP_PKEY *A, *Z;

    if (strcmp(alg, "X25519") == 0)
        from_hex(X25519_A_SK_HEX, ask, keylen);
    else
        from_hex(X448_A_SK_HEX, ask, keylen);
    memset(zero, 0, keylen);
    A = make_key(alg, ask, keylen, NULL, 0);
    Z = make_key(alg, NULL, 0, zero, keylen);
    CHECK(derive(A, Z, z, keylen, &zlen) == 0,
          "low-order (all-zero) peer point was accepted");
    EVP_PKEY_free(A);
    EVP_PKEY_free(Z);
}


/* X25519 as a KEM (OpenSSL 3.2+): encapsulate/decapsulate round-trip. */
static void test_x25519_kem(void)
{
    EVP_PKEY *key = NULL;
    EVP_PKEY_CTX *kctx, *dctx;
    unsigned char *ct, *ss, *ss2;
    size_t ctlen = 0, sslen = 0, ss2len = 0;

    kctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);
    CHECK(kctx != NULL, "EVP_PKEY_CTX_new_from_name(X25519) failed");
    CHECK(EVP_PKEY_keygen_init(kctx) == 1, "X25519 keygen_init failed");
    CHECK(EVP_PKEY_generate(kctx, &key) == 1, "X25519 key generation failed");
    EVP_PKEY_CTX_free(kctx);

    kctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    CHECK(kctx != NULL, "EVP_PKEY_CTX_new_from_pkey failed");
    if (kctx == NULL || EVP_PKEY_encapsulate_init(kctx, NULL) != 1 ||
        EVP_PKEY_encapsulate(kctx, NULL, &ctlen, NULL, &sslen) != 1) {
        printf("SKIP test_x25519_x448: X25519 KEM is unavailable\n");
        EVP_PKEY_CTX_free(kctx);
        EVP_PKEY_free(key);
        return;
    }
    ct = ALLOCATE_OUTPUT(ctlen);
    ss = ALLOCATE_OUTPUT(sslen);
    CHECK(EVP_PKEY_encapsulate(kctx, ct, &ctlen, ss, &sslen) == 1,
          "X25519 KEM encapsulation failed");
    EVP_PKEY_CTX_free(kctx);

    dctx = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    CHECK(dctx != NULL && EVP_PKEY_decapsulate_init(dctx, NULL) == 1,
          "X25519 KEM decapsulate_init failed");
    CHECK(EVP_PKEY_decapsulate(dctx, NULL, &ss2len, ct, ctlen) == 1,
          "X25519 KEM decapsulate size query failed");
    ss2 = ALLOCATE_OUTPUT(ss2len);
    CHECK(EVP_PKEY_decapsulate(dctx, ss2, &ss2len, ct, ctlen) == 1,
          "X25519 KEM decapsulation failed");
    CHECK(ss2len == sslen && memcmp(ss2, ss, sslen) == 0,
          "X25519 KEM decapsulation did not recover the shared secret");
    EVP_PKEY_CTX_free(dctx);
    EVP_PKEY_free(key);
}


int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats("X25519", 32,
              X25519_A_SK_HEX, X25519_A_PK_HEX,
              X25519_B_SK_HEX, X25519_B_PK_HEX, X25519_K_HEX);
    test_kats("X448", 56,
              X448_A_SK_HEX, X448_A_PK_HEX,
              X448_B_SK_HEX, X448_B_PK_HEX, X448_K_HEX);

    /* Section 5.2 starts: k = u = 0x09 (X25519) / 0x05 (X448), i.e. the
     * base-point encoding (little-endian). */
    test_iterated("X25519", 32,
                  "0900000000000000000000000000000000000000000000000000000000000000",
                  "422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079",
                  "684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51");
    test_iterated("X448", 56,
                  "0500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                  "3f482c8a9f19b01e6c46ee9711d9dc14fd4bf67af30765c2ae2b846a4d23a8cd0db897086239492caf350b51f833868b9bc2b3bca9cf4113",
                  "aa3b4749d55b9daf1e5b00288826c467274ce3ebbdd5c17b975e09d4af6c67cf10d087202db88286e2b79fceea3ec353ef54faa26e219f38");

    test_low_order_peer("X25519", 32);
    test_low_order_peer("X448", 56);
    test_x25519_kem();

    printf("test_x25519_x448: all %u checks passed\n", g_checks);
    return 0;
}
