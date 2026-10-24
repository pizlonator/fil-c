#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_ecdh -- ECDH over the RFC 5903 256-bit and 384-bit Random ECP groups

 * (P-256/P-384), using the fixed RFC 5114 A.6/A.7 key pairs and shared

 * secrets.

 *

 * Known-answer vectors: RFC 5114 A.6 (256-bit Random ECP Group) and A.7

 * (384-bit Random ECP Group) -- the same groups RFC 5903 defines

 * for IKEv2.  The private keys (dA/dB), public keys (qA = dA*G, qB = dB*G)

 * and the shared secret Z are taken verbatim from the RFC text (extracted

 * programmatically to avoid transcription errors) and were verified with a

 * filcc-compiled scratch probe before being embedded: the probe derived the

 * secret through EVP_PKEY_derive and compared it with x_Z for both groups.

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5 and asserted

 * below:

 *

 *   - derive(dA, qB) == derive(dB, qA) == x_Z (the x-coordinate of the

 *     shared point), for both groups.

 *   - The derived secret length equals the field size (32/48 bytes).

 *   - A peer on a different curve is rejected (EVP_PKEY_derive_set_peer

 *     fails).

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_ecdh: %s\n", msg);
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


/* OSSL_PARAM integers are native-endian (OSSL_PARAM(3)); see
 * test_rsa_pkcs1.c. */
static size_t from_hex_le(const char *hex, unsigned char *dst, size_t dstcap)
{
    size_t n = from_hex(hex, dst, dstcap);
    size_t i;
    for (i = 0; i < n / 2; i++) {
        unsigned char t = dst[i];
        dst[i] = dst[n - 1 - i];
        dst[n - 1 - i] = t;
    }
    return n;
}


/* RFC 5114 A.6 -- 256-bit Random ECP Group (P-256). */
static const char *const P256_DA_HEX =
    "814264145f2f56f2e96a8e337a1284993faf432a5abce59e867b7291d507a3af";
static const char *const P256_QAX_HEX =
    "2af502f3be8952f2c9b5a8d4160d09e97165be50bc42ae4a5e8d3b4ba83aeb15";
static const char *const P256_QAY_HEX =
    "eb0faf4ca986c4d38681a0f9872d79d56795bd4bff6e6de3c0f5015ece5efd85";
static const char *const P256_DB_HEX =
    "2ce1788ec197e096db95a200cc0ab26a19ce6bccad562b8eee1b593761cf7f41";
static const char *const P256_QBX_HEX =
    "b120de4aa36492795346e8de6c2c8646ae06aaea279fa775b3ab0715f6ce51b0";
static const char *const P256_QBY_HEX =
    "9f1b7eece20d7b5ed8ec685fa3f071d83727027092a8411385c34dde5708b2b6";
static const char *const P256_Z_HEX =
    "dd0f5396219d1ea393310412d19a08f1f5811e9dc8ec8eea7f80d21c820c2788";


/* RFC 5114 A.7 -- 384-bit Random ECP Group (P-384). */
static const char *const P384_DA_HEX =
    "d27335ea71664af244dd14e9fd1260715dfd8a7965571c48d709ee7a7962a156"
    "d706a90cbcb5df2986f05feadb9376f1";
static const char *const P384_QAX_HEX =
    "793148f1787634d5da4c6d9074417d05e057ab62f82054d10ee6b0403d627954"
    "7e6a8ea9d1fd77427d016fe27a8b8c66";
static const char *const P384_QAY_HEX =
    "c6c41294331d23e6f480f4fb4cd40504c947392e94f4c3f06b8f398bb29e4236"
    "8f7a685923de3b67baced214a1a1d128";
static const char *const P384_DB_HEX =
    "52d1791fdb4b70f89c0f00d456c2f7023b6125262c36a7df1f80231121cce3d3"
    "9be52e00c194a4132c4a6c768bcd94d2";
static const char *const P384_QBX_HEX =
    "5cd42ab9c41b5347f74b8d4efb708b3d5b36db65915359b44abc17647b6b9999"
    "789d72a84865ae2f223f12b5a1abc120";
static const char *const P384_QBY_HEX =
    "e171458feaa939aaa3a8bfac46b404bd8f6d5b348c0fa4d80ceca16356ca9332"
    "40bde8723415a8ece035b0edf36755de";
static const char *const P384_Z_HEX =
    "5ea1fc4af7256d2055981b110575e0a8cae53160137d904c59d926eb1b8456e4"
    "27aa8a4540884c37de159a58028abc0e";


/* Imports a fixed EC key pair (group name, private scalar, uncompressed

 * public point) through EVP_PKEY_fromdata.  Buffers are static: the key

 * references them for its lifetime. */
static EVP_PKEY *make_key(const char *group, const char *dhex,
                          const char *qxhex, const char *qyhex, size_t fieldlen)
{
    static unsigned char *d, *pub;
    OSSL_PARAM params[4];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = NULL;
    size_t i = 0;

    d = ALLOCATE_KEY(fieldlen);
    pub = ALLOCATE_KEY(1 + 2 * fieldlen);
    from_hex_le(dhex, d, fieldlen);
    pub[0] = 0x04; /* uncompressed point */
    from_hex(qxhex, pub + 1, fieldlen);
    from_hex(qyhex, pub + 1 + fieldlen, fieldlen);

    params[i++] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                   (char *)group, 0);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PRIV_KEY, d, fieldlen);
    params[i++] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                    pub, 1 + 2 * fieldlen);
    params[i] = OSSL_PARAM_construct_end();
    ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name(EC) failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed for the fixed ECDH key");
    EVP_PKEY_CTX_free(ctx);
    return key;
}


/* derive(our_key, peer) -> secret buffer; returns the secret length (0 on

 * failure, which the caller turns into the appropriate CHECK). */
static size_t derive(EVP_PKEY *key, EVP_PKEY *peer,
                     unsigned char *secret, size_t cap)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    size_t len = cap;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_derive_init(ctx) == 1, "EVP_PKEY_derive_init failed");
    CHECK(EVP_PKEY_derive_set_peer(ctx, peer) == 1,
          "EVP_PKEY_derive_set_peer failed");
    CHECK(EVP_PKEY_derive(ctx, secret, &len) == 1, "EVP_PKEY_derive failed");
    EVP_PKEY_CTX_free(ctx);
    return len;
}


/* One group's worth of KAT checks. */
static void test_group(const char *group, const char *dahex, const char *qaxhex,
                       const char *qayhex, const char *dbhex,
                       const char *qbxhex, const char *qbyhex,
                       const char *zhex, size_t fieldlen)
{
    unsigned char *z = ALLOCATE_OUTPUT(fieldlen);
    unsigned char *want = ALLOCATE_OUTPUT(fieldlen);
    size_t zlen, wantlen;
    char mbuf[128];
    EVP_PKEY *A = make_key(group, dahex, qaxhex, qayhex, fieldlen);
    EVP_PKEY *B = make_key(group, dbhex, qbxhex, qbyhex, fieldlen);

    wantlen = from_hex(zhex, want, fieldlen);
    CHECK(wantlen == fieldlen, "KAT Z constant has the wrong length");

    zlen = derive(A, B, z, fieldlen);
    snprintf(mbuf, sizeof(mbuf), "%s: derive(dA, qB) length is %zu, want %zu",
             group, zlen, fieldlen);
    CHECK(zlen == fieldlen, mbuf);
    snprintf(mbuf, sizeof(mbuf), "%s: derive(dA, qB) != RFC Z", group);
    CHECK(memcmp(z, want, fieldlen) == 0, mbuf);

    zlen = derive(B, A, z, fieldlen);
    snprintf(mbuf, sizeof(mbuf), "%s: derive(dB, qA) != RFC Z", group);
    CHECK(zlen == fieldlen && memcmp(z, want, fieldlen) == 0, mbuf);

    EVP_PKEY_free(A);
    EVP_PKEY_free(B);
}


/* A P-256 initiator against a P-384 peer must be rejected. */
static void test_cross_curve(void)
{
    EVP_PKEY *A = make_key("prime256v1", P256_DA_HEX, P256_QAX_HEX,
                           P256_QAY_HEX, 32);
    EVP_PKEY *B = make_key("secp384r1", P384_DB_HEX, P384_QBX_HEX,
                           P384_QBY_HEX, 48);
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(A, NULL);
    unsigned char *secret = ALLOCATE_OUTPUT(48);
    size_t len = 48;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_derive_init(ctx) == 1, "EVP_PKEY_derive_init failed");
    CHECK(EVP_PKEY_derive_set_peer(ctx, B) != 1,
          "EVP_PKEY_derive_set_peer accepted a P-384 peer for a P-256 key");
    CHECK(EVP_PKEY_derive(ctx, secret, &len) != 1,
          "ECDH derive succeeded across different curves");
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(A);
    EVP_PKEY_free(B);
}


int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_group("prime256v1", P256_DA_HEX, P256_QAX_HEX, P256_QAY_HEX,
               P256_DB_HEX, P256_QBX_HEX, P256_QBY_HEX, P256_Z_HEX, 32);
    test_group("secp384r1", P384_DA_HEX, P384_QAX_HEX, P384_QAY_HEX,
               P384_DB_HEX, P384_QBX_HEX, P384_QBY_HEX, P384_Z_HEX, 48);
    test_cross_curve();

    printf("test_ecdh: all %u checks passed\n", g_checks);
    return 0;
}
