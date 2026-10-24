#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_pkcs12kdf -- the PKCS#12 KDF (RFC 7292 appendix B) through the
 * EVP_KDF public API: the three material types (id 1 = key, 2 = IV, 3 = MAC
 * secret), both SHA-256 and SHA-1 digests, iteration-count sensitivity and
 * the mandatory pass/salt error paths.
 *
 * There are no published RFC vectors for this KDF, so the constants below
 * were derived with the authoritative pizfix OpenSSL 3.6.5 (and cross-checked
 * against the system OpenSSL 3.0.13; the two agree on every value):
 *
 *   pizfix/bin/openssl kdf -keylen 32 -kdfopt digest:sha256 \
 *       -kdfopt hexpass:70617373776f7264 -kdfopt hexsalt:73616c74 \
 *       -kdfopt iter:2048 -kdfopt id:1 PKCS12KDF
 *
 * (pass = "password", salt = "salt"; the hex* prefixes make the CLI decode
 * the octet-string parameter values from hex).
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_pkcs12kdf: %s\n", msg);
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

/* One PKCS12KDF derive with a fresh context.  pass or salt may be NULL to
 * exercise the "missing pass/salt" error paths.  Returns 1 on success, 0 on
 * failure.  PKCS12KDF is core OpenSSL 3 functionality: a fetch failure fails
 * the program (never SKIP). */
static int pkcs12_derive(const char *digest, const unsigned char *pass,
                         size_t passlen, const unsigned char *salt,
                         size_t saltlen, unsigned long iter, int id,
                         unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "PKCS12KDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[6];
    int ok;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core PKCS12KDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                 (char *)digest, 0);
    if (pass != NULL)
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                      (void *)pass, passlen);
    else
        params[1] = OSSL_PARAM_construct_end();
    if (salt != NULL) {
        params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                      (void *)salt, saltlen);
        params[3] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_ITER, &iter);
        params[4] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_PKCS12_ID, &id);
        params[5] = OSSL_PARAM_construct_end();
    } else {
        params[2] = OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_ITER, &iter);
        params[3] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_PKCS12_ID, &id);
        params[4] = OSSL_PARAM_construct_end();
    }
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* pass = "password" (70617373776f7264), salt = "salt" (73616c74). */
static const char *const PASS_HEX = "70617373776f7264";
static const char *const SALT_HEX = "73616c74";

static const struct pkcs12_kat {
    const char *digest;
    int id;
    unsigned long iter;
    const char *dkhex;
} pkcs12_kats[] = {
    { "sha256", 1, 2048,
      "b3e8c22aac46cada13e88c60a32bbcd34309531cc1a97b96dfa8d50f3f9c647d" },
    { "sha1",   1, 2048,
      "ba00e106a9ca89beb0f09a91c50ee7f8d501f2ebe5dde6547caaef03e0567dca" },
    { "sha256", 2, 2048,
      "8ebc518cebfd4460f0373e719e5e55639e487c9d341dfa3a7b7a47789002accd" },
    { "sha1",   2, 2048,
      "b46ddec0d601f8d6bb86992e8f5a1073da62a4878455562a69d4bd6be3d336c5" },
    { "sha256", 3, 2048,
      "4dffb4aff824f6cf5c4f21203b43cbb8482750e9ce0c9bf26bbaa39ff02147d3" },
    { "sha1",   3, 2048,
      "2bf931c3e4a127c5b48f1c9b9624d0aac9a5032982dde93ea47c5b167111ee43" },
    { "sha256", 1, 1,
      "ee3299fd9f1f99c72b4557890a25ba04ee65797e8cd70a7d1fadb950266f9652" },
};

#define PASS_CAP 8
#define SALT_CAP 4
#define DK_CAP 32

/* (1) The KAT matrix: two digests x three material ids, plus an iteration
 * count of 1.  All 32 bytes of output. */
static void test_kats(void)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(DK_CAP);
    size_t passlen, saltlen, wantlen, k;
    char msg[128];

    passlen = from_hex(PASS_HEX, pass, PASS_CAP);
    saltlen = from_hex(SALT_HEX, salt, SALT_CAP);

    for (k = 0; k < sizeof(pkcs12_kats) / sizeof(pkcs12_kats[0]); k++) {
        const struct pkcs12_kat *kat = &pkcs12_kats[k];
        wantlen = from_hex(kat->dkhex, want, DK_CAP);
        CHECK(pkcs12_derive(kat->digest, pass, passlen, salt, saltlen,
                            kat->iter, kat->id, dk, DK_CAP),
              "PKCS12KDF derive failed");
        snprintf(msg, sizeof(msg), "%s id=%d iter=%lu does not match the KAT",
                 kat->digest, kat->id, kat->iter);
        expect_equal(msg, dk, DK_CAP, want, wantlen);
    }
}

/* (2) The three material ids (key / IV / MAC secret) must all be derived
 * from the same pass and salt yet produce different output. */
static void test_ids_differ(void)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk[3];
    size_t passlen, saltlen, i, j;

    passlen = from_hex(PASS_HEX, pass, PASS_CAP);
    saltlen = from_hex(SALT_HEX, salt, SALT_CAP);
    for (i = 0; i < 3; i++) {
        dk[i] = ALLOCATE_OUTPUT(DK_CAP);
        CHECK(pkcs12_derive("sha256", pass, passlen, salt, saltlen, 2048,
                            (int)(i + 1), dk[i], DK_CAP),
              "PKCS12KDF derive failed for one of the ids");
    }
    for (i = 0; i < 3; i++)
        for (j = i + 1; j < 3; j++)
            CHECK(memcmp(dk[i], dk[j], DK_CAP) != 0,
                  "two PKCS12KDF material ids produced identical output");
}

/* (3) The pass and the salt are both mandatory: omitting either must fail. */
static void test_missing_inputs(void)
{
    unsigned char *pass = ALLOCATE_KEY(PASS_CAP);
    unsigned char *salt = ALLOCATE_SALT(SALT_CAP);
    unsigned char *dk = ALLOCATE_OUTPUT(DK_CAP);
    size_t passlen, saltlen;

    passlen = from_hex(PASS_HEX, pass, PASS_CAP);
    saltlen = from_hex(SALT_HEX, salt, SALT_CAP);

    CHECK(!pkcs12_derive("sha256", NULL, 0, salt, saltlen, 2048, 1,
                         dk, DK_CAP),
          "PKCS12KDF accepted a missing pass");
    CHECK(!pkcs12_derive("sha256", pass, passlen, NULL, 0, 2048, 1,
                         dk, DK_CAP),
          "PKCS12KDF accepted a missing salt");
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_kats();
    test_ids_differ();
    test_missing_inputs();

    printf("test_kdf_pkcs12kdf: all %u checks passed\n", g_checks);
    return 0;
}
