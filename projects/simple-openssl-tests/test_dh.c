#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>

/*

 * test_dh -- finite-field DH over the RFC 7919 ffdhe2048 well-known group,

 * with both provider fetch names ("DH" and "DHX", the latter carrying the

 * subgroup order q), fixed private keys imported through EVP_PKEY_fromdata,

 * and a CLI-cross-checked shared-secret KAT.

 *

 * Fixed key material: private exponent x = 1A2B3C4D...FF01 for the

 * initiator and x' = FEDCBA98...3210 for the responder, both fixed 256-bit

 * patterns; the public keys y = g^x mod p and y' = g^x' mod p were computed

 * from the RFC prime and are embedded (EVP_PKEY_fromdata with only the

 * private key does NOT derive the public key for FFC keys, so both are

 * imported).

 *

 * Known-answer vector: the shared secret Z = (y')^x mod p = g^(x*x') mod p.

 * It was verified with `pizfix/bin/openssl pkeyutl -derive` over hand-built

 * PKCS#8 PEM files of the same fixed keys (development-time scratch in

 * /tmp; the suite's programs do no file I/O) and matches the embedded Z.

 *

 * Behavior verified empirically on OpenSSL 3.0.13 and 3.6.5 and asserted

 * below:

 *

 *   - Both "DH" and "DHX" import the fixed keys and derive the same KAT Z;

 *     the derived secret is exactly 256 bytes (the group size).

 *   - A peer key from a different group (ffdhe3072, i.e. the wrong size) is

 *     rejected by EVP_PKEY_derive_set_peer.

 *   - A peer with a zero public value is rejected as well.

 */


/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */

static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }


static unsigned g_checks = 0;


static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_dh: %s\n", msg);
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


/* RFC 7919 A.1, ffdhe2048. */
static const char *const FFDHE2048_P_HEX =
    "ffffffffffffffffadf85458a2bb4a9aafdc5620273d3cf1d8b9c583ce2d"
    "3695a9e13641146433fbcc939dce249b3ef97d2fe363630c75d8f681b202"
    "aec4617ad3df1ed5d5fd65612433f51f5f066ed0856365553ded1af3b557"
    "135e7f57c935984f0c70e0e68b77e2a689daf3efe8721df158a136ade735"
    "30acca4f483a797abc0ab182b324fb61d108a94bb2c8e3fbb96adab760d7"
    "f4681d4f42a3de394df4ae56ede76372bb190b07a7c8ee0a6d709e02fce1"
    "cdf7e2ecc03404cd28342f619172fe9ce98583ff8e4f1232eef28183c3fe"
    "3b1b4c6fad733bb5fcbc2ec22005c58ef1837d1683b2c6f34a26c1b2effa"
    "886b423861285c97ffffffffffffffff";
/* q = (p-1)/2. */
static const char *const FFDHE2048_Q_HEX =
    "7fffffffffffffffd6fc2a2c515da54d57ee2b10139e9e78ec5ce2c1e716"
    "9b4ad4f09b208a3219fde649cee7124d9f7cbe97f1b1b1863aec7b40d901"
    "576230bd69ef8f6aeafeb2b09219fa8faf83376842b1b2aa9ef68d79daab"
    "89af3fabe49acc278638707345bbf15344ed79f7f4390ef8ac509b56f39a"
    "98566527a41d3cbd5e0558c159927db0e88454a5d96471fddcb56d5bb06b"
    "fa340ea7a151ef1ca6fa572b76f3b1b95d8c8583d3e4770536b84f017e70"
    "e6fbf176601a0266941a17b0c8b97f4e74c2c1ffc7278919777940c1e1ff"
    "1d8da637d6b99ddafe5e17611002e2c778c1be8b41d96379a51360d977fd"
    "4435a11c30942e4bffffffffffffffff";

/* Fixed initiator key. */
static const char *const DH_X_HEX =
    "1a2b3c4d5e6f778899aabbccddeeff00112233445566778899aabbccddee"
    "ff01";
static const char *const DH_Y_HEX =
    "6949b9b49fa965bec0df3e48af25065375f138c69b4a0ca9082783d7e507"
    "4a85f5f2fdca098c14c5125b39374ee8694ed68cedb339a7d75b0881143c"
    "0c70d21bfbb82f0f5b36bb05b7454d83f753faebd1bed86ab46d8017c4f6"
    "d1a9fa31cf447de7c36ec69133f88e01473b94b9447022a59cb847d07949"
    "4e57f26abe52916fa2677310cae89ae8ccfb93b779f7ea98e659e0be42f9"
    "3b2987dc6e6de06fe264115ea70b0a7b92d53eb40e2b4718a121d93bfa75"
    "fd1bcda9797ecb24f1822d163c447d8ce4138415492ac264ccfedcdee844"
    "03da1e1c65d4bed958a27ad67c250aeac422d06937dd4aca5ce565288452"
    "1779ede3474e85ad46e711aa5bf16b53";

/* Fixed responder key. */
static const char *const DH_X2_HEX =
    "fedcba9876543210fedcba9876543210fedcba9876543210fedcba987654"
    "3210";
static const char *const DH_Y2_HEX =
    "91a7d0938c593391c7ed47f8dc421d1895768d481a6b3cc96c6b2c989d78"
    "17be77921559945777020e69c87322c6508e836ba2a488c7e444cb0a902f"
    "0a400461f86389b92ff8f3a935da7726e7c56ebf2644781930228a519a5a"
    "25fc81b722fb224e49d1d6d40d548314d8bd967e1a1c56c2862804f0e7a7"
    "e54f585371999323263cfef16ee8e8ac2a9812f832d153dbb98c05ec410e"
    "6a46ca2b6793d0bbdb5890360cfc434a10d62d5a27e5106d1de0e455bf6b"
    "e9450a1a509d33171b76c95542679668f53cf0ac4c2178ce4b5b417b9fff"
    "aafb0fa3aeeeb77d619e5adffaf4d878c4b1ab27a673f9fa956eff93e458"
    "39edc2de3fdc6105caaba1dce6be7a23";

/* The CLI-cross-checked shared secret Z = g^(x*x') mod p. */
static const char *const DH_Z_HEX =
    "b8b1e6335420976bdc24fd1b982a78dfea23973cfba9be69c79e3967606c"
    "2b1dbe6454f2404bb74148ce2fb8d25b6c4b49ce82eba0ec9aa7e4bfb3a7"
    "189c2374aeb956c5bf6325e81f709455e44ecccefd9a78e3a1ea5aa58e5a"
    "4fd15ed95f927e02ae1f6c2e083885a8274b713b8c8d0243daa37d87b7ea"
    "839422ebf057986c8841009add4e936f7b92c90d51c77154055c8b9492c1"
    "2b6c74719a6e6c5d0c0a8a24c7b57e2b8537a7da603d9e0c21c113145ec7"
    "6130f480df93ecebc90eb9849210b5b3bf76bfe72b5aa30cb400a3dd4439"
    "84237765c27c8386aeb5c01ffd42870a0f0b0dfb5fc5b15b7fcef7a33b89"
    "2e9b43f158ef4a82c35d1b2944e26aaa";

/* RFC 7919 A.2, ffdhe3072 -- used only to build a wrong-size peer. */
static const char *const FFDHE3072_P_HEX =
    "ffffffffffffffffadf85458a2bb4a9aafdc5620273d3cf1d8b9c583ce2d"
    "3695a9e13641146433fbcc939dce249b3ef97d2fe363630c75d8f681b202"
    "aec4617ad3df1ed5d5fd65612433f51f5f066ed0856365553ded1af3b557"
    "135e7f57c935984f0c70e0e68b77e2a689daf3efe8721df158a136ade735"
    "30acca4f483a797abc0ab182b324fb61d108a94bb2c8e3fbb96adab760d7"
    "f4681d4f42a3de394df4ae56ede76372bb190b07a7c8ee0a6d709e02fce1"
    "cdf7e2ecc03404cd28342f619172fe9ce98583ff8e4f1232eef28183c3fe"
    "3b1b4c6fad733bb5fcbc2ec22005c58ef1837d1683b2c6f34a26c1b2effa"
    "886b4238611fcfdcde355b3b6519035bbc34f4def99c023861b46fc9d6e6"
    "c9077ad91d2691f7f7ee598cb0fac186d91caefe130985139270b4130c93"
    "bc437944f4fd4452e2d74dd364f2e21e71f54bff5cae82ab9c9df69ee86d"
    "2bc522363a0dabc521979b0deada1dbf9a42d5c4484e0abcd06bfa53ddef"
    "3c1b20ee3fd59d7c25e41d2b66c62e37ffffffffffffffff";
/* A public value under the 3072-bit prime (g^x'' mod p3072). */
static const char *const DH_BIG_PEER_Y_HEX =
    "f1835acd644427b3d92607ceda7cf0ed950cf8882724abce661428812b9f"
    "212ba6dbdd2bc70ee13b8cba56c2f657f92e2cf81bc9ffdf5259ce6a2580"
    "ed2de21a45dd5c087c0670447e22d3a9e1d389a6a99580c85f15d8cad601"
    "15313337e3e1d665d5d757512659f919224b5dcb81a656428b77fad0df9e"
    "1ef2d8d0698cb5bbcfb3bbe0e3951d20d9e9606a21793c44c7d51d409223"
    "cb7aa1b411770b49c24b51c9eabda35f72fb3f468beb53e09fda3e303e24"
    "606eda24c1ba0cfce6df909cb3c1f3d81a13cceda690aa14d6f6f47369d7"
    "45d5346a99927404fbc9f83fd4603a42d32fb2943d767d93c59fdf6c372a"
    "c393e955392e88f62748c371392af4c81bdd8973378dbcc1240947fd84bf"
    "42df376fe84cc3bcc94b4643669d5f83592b0ce2a96a626e6bfa9a9bac03"
    "9d5c1401559b04fcf826b4943838a4ddc545ade73846264dce6caef57407"
    "301fbb56cad3e1a331d30ddf1446c00c0aefa76b0e670400370a938c8533"
    "6540626adfd44f75d27083998b7cee3cdaa55525c6de5f97";


/* Imports a DH/DHX key pair from p, g [, q], priv and pub.  Returns the
 * EVP_PKEY (which references the static buffers). */
static EVP_PKEY *make_key(const char *alg, const char *phex,
                          const char *qhex_or_null,
                          const char *xhex, const char *yhex, size_t plen)
{
    static unsigned char *p, *q, *x, *y, *gtwo;
    OSSL_PARAM params[6];
    EVP_PKEY_CTX *ctx;
    EVP_PKEY *key = NULL;
    size_t i = 0;

    p = ALLOCATE_KEY(plen);
    x = ALLOCATE_KEY(32);
    y = ALLOCATE_KEY(plen);
    from_hex_le(phex, p, plen);
    from_hex_le(xhex, x, 32);
    from_hex_le(yhex, y, plen);

    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_FFC_P, p, plen);
    gtwo = ALLOCATE_KEY(1);
    gtwo[0] = 0x02; /* g = 2 for every ffdhe group */
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_FFC_G, gtwo, 1);
    if (qhex_or_null != NULL) {
        q = ALLOCATE_KEY(plen);
        from_hex_le(qhex_or_null, q, plen);
        params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_FFC_Q, q, plen);
    }
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PRIV_KEY, x, 32);
    params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PUB_KEY, y, plen);
    params[i] = OSSL_PARAM_construct_end();

    ctx = EVP_PKEY_CTX_new_from_name(NULL, alg, NULL);
    CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed");
    CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
    CHECK(EVP_PKEY_fromdata(ctx, &key, EVP_PKEY_KEYPAIR, params) == 1,
          "EVP_PKEY_fromdata failed for the fixed DH key");
    EVP_PKEY_CTX_free(ctx);
    return key;
}


/* derive(our_key, peer) into `secret`; returns 1 and stores the length on
 * success, 0 if the exchange was rejected. */
static int derive(EVP_PKEY *key, EVP_PKEY *peer, unsigned char *secret,
                  size_t cap, size_t *secretlen)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL);
    size_t len = cap;
    int ok;

    CHECK(ctx != NULL, "EVP_PKEY_CTX_new failed");
    CHECK(EVP_PKEY_derive_init(ctx) == 1, "EVP_PKEY_derive_init failed");
    if (EVP_PKEY_derive_set_peer(ctx, peer) != 1) {
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }
    ok = EVP_PKEY_derive(ctx, secret, &len) == 1;
    *secretlen = ok ? len : 0;
    EVP_PKEY_CTX_free(ctx);
    return ok;
}


/* (1) The KAT with the plain "DH" provider name. */
static void test_dh_kat(void)
{
    unsigned char *z = ALLOCATE_OUTPUT(256);
    unsigned char *want = ALLOCATE_OUTPUT(256);
    EVP_PKEY *A = make_key("DH", FFDHE2048_P_HEX, NULL,
                           DH_X_HEX, DH_Y_HEX, 256);
    EVP_PKEY *B = make_key("DH", FFDHE2048_P_HEX, NULL,
                           DH_X2_HEX, DH_Y2_HEX, 256);
    size_t zlen = 0, wantlen;

    wantlen = from_hex(DH_Z_HEX, want, 256);
    CHECK(wantlen == 256, "KAT Z constant has the wrong length");
    CHECK(derive(A, B, z, 256, &zlen) == 1,
          "DH derive with the ffdhe2048 peer failed");
    CHECK(zlen == 256, "the DH shared secret is not 256 bytes");
    CHECK(memcmp(z, want, 256) == 0,
          "DH shared secret != the CLI-cross-checked KAT");
    EVP_PKEY_free(A);
    EVP_PKEY_free(B);
}


/* (2) The same KAT with the "DHX" provider name (subgroup q imported). */
static void test_dhx_kat(void)
{
    unsigned char *z = ALLOCATE_OUTPUT(256);
    unsigned char *want = ALLOCATE_OUTPUT(256);
    EVP_PKEY *A = make_key("DHX", FFDHE2048_P_HEX, FFDHE2048_Q_HEX,
                           DH_X_HEX, DH_Y_HEX, 256);
    EVP_PKEY *B = make_key("DHX", FFDHE2048_P_HEX, FFDHE2048_Q_HEX,
                           DH_X2_HEX, DH_Y2_HEX, 256);
    size_t zlen = 0, wantlen;

    wantlen = from_hex(DH_Z_HEX, want, 256);
    CHECK(derive(A, B, z, 256, &zlen) == 1,
          "DHX derive with the ffdhe2048 peer failed");
    CHECK(zlen == 256, "the DHX shared secret is not 256 bytes");
    CHECK(memcmp(z, want, wantlen) == 0,
          "DHX shared secret != the DH shared secret (KAT)");
    EVP_PKEY_free(A);
    EVP_PKEY_free(B);
}


/* (3) Peers that must be rejected: a wrong-size (ffdhe3072) peer and a

 * peer whose public value is zero. */
static void test_bad_peers(void)
{
    EVP_PKEY *A = make_key("DH", FFDHE2048_P_HEX, NULL,
                           DH_X_HEX, DH_Y_HEX, 256);
    EVP_PKEY *BIG = make_key("DH", FFDHE3072_P_HEX, NULL,
                             DH_X_HEX, DH_BIG_PEER_Y_HEX, 384);
    unsigned char *z = ALLOCATE_OUTPUT(384);
    size_t zlen = 0;

    CHECK(derive(A, BIG, z, 384, &zlen) == 0,
          "derive accepted a peer from a different (larger) group");
    EVP_PKEY_free(BIG);

    /* Zero public value. */
    {
        unsigned char *p = ALLOCATE_KEY(256);
        unsigned char *x = ALLOCATE_KEY(32);
        unsigned char *y0 = ALLOCATE_KEY(256);
        unsigned char *gtwo = ALLOCATE_KEY(1);
        OSSL_PARAM params[5];
        EVP_PKEY_CTX *ctx;
        EVP_PKEY *Z = NULL;
        size_t i = 0;

        from_hex_le(FFDHE2048_P_HEX, p, 256);
        from_hex_le(DH_X2_HEX, x, 32);
        memset(y0, 0, 256);
        gtwo[0] = 0x02;
        params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_FFC_P, p, 256);
        params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_FFC_G, gtwo, 1);
        params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PRIV_KEY, x, 32);
        params[i++] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PUB_KEY, y0, 256);
        params[i] = OSSL_PARAM_construct_end();
        ctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
        CHECK(ctx != NULL, "EVP_PKEY_CTX_new_from_name failed");
        CHECK(EVP_PKEY_fromdata_init(ctx) == 1, "EVP_PKEY_fromdata_init failed");
        CHECK(EVP_PKEY_fromdata(ctx, &Z, EVP_PKEY_KEYPAIR, params) == 1,
              "EVP_PKEY_fromdata failed for the zero-y peer");
        EVP_PKEY_CTX_free(ctx);
        CHECK(derive(A, Z, z, 384, &zlen) == 0,
              "derive accepted a peer with a zero public value");
        EVP_PKEY_free(Z);
    }
    EVP_PKEY_free(A);
}


int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_dh_kat();
    test_dhx_kat();
    test_bad_peers();

    printf("test_dh: all %u checks passed\n", g_checks);
    return 0;
}
