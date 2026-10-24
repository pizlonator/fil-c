#include "poisoning.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

/*
 * test_kdf_sshkdf -- the SSH RFC 4253 exchange-hash KDF through the EVP_KDF
 * public API: all six derivation types (A..F) with SHA-1, a SHA-256 pair,
 * the distinctness of the six outputs, and the error paths.
 *
 * Known-answer vectors: NIST CAVS 14.1 values as distributed in OpenSSL's
 * own test data (test/recipes/30-test_evp_data/evpkdf_ssh.txt); every value
 * re-verified with
 *
 *   pizfix/bin/openssl kdf -keylen <L> -kdfopt digest:SHA1 \
 *       -kdfopt hexkey:<K> -kdfopt hexxcghash:<H> \
 *       -kdfopt hexsession_id:<SID> -kdfopt type:<A..F> SSHKDF
 *
 * and the same command under the system OpenSSL 3.0.13.  Note the parameter
 * spelling: "xcghhash" in the man page but "xcghash" (OSSL_KDF_PARAM_SSHKDF_XCGHASH)
 * in core_names.h, which is what the CLI accepts too.
 *
 * Behavior note: RFC 4253 requires H (xcghhash) to be the digest size, but
 * OpenSSL's default provider does not enforce it -- a wrong-length H is
 * accepted deterministically (verified identically on 3.0.13 and 3.6.5);
 * only a missing H, a missing session id or a bad type are rejected.
 */

/* Labeled allocation wrappers (house rule: every buffer handed to OpenSSL
 * comes from one of these; see poisoning.h for why). */
static void *ALLOCATE_KEY(size_t n)    { return poison_alloc(POISON_KEY, n); }
static void *ALLOCATE_SALT(size_t n)   { return poison_alloc(POISON_SALT, n); }
static void *ALLOCATE_OUTPUT(size_t n) { return poison_alloc(POISON_OUTPUT, n); }

static unsigned g_checks = 0;

static void test_failed(const char *msg)
{
    fprintf(stderr, "FAIL test_kdf_sshkdf: %s\n", msg);
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

/* One SSHKDF derive with a fresh context.  Returns 1 on success, 0 on
 * failure.  SSHKDF is core OpenSSL 3 functionality: a fetch failure fails
 * the program (never SKIP). */
static int sshkdf_derive(const char *digest, const unsigned char *key,
                         size_t keylen, const unsigned char *xcghash,
                         size_t xlen, const unsigned char *session_id,
                         size_t sidlen, const char *type,
                         unsigned char *out, size_t outlen)
{
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "SSHKDF", NULL);
    EVP_KDF_CTX *ctx;
    OSSL_PARAM params[6];
    int ok, n = 0;

    CHECK(kdf != NULL, "EVP_KDF_fetch failed for the core SSHKDF KDF");
    ctx = EVP_KDF_CTX_new(kdf);
    CHECK(ctx != NULL, "EVP_KDF_CTX_new failed");
    params[n++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                                   (char *)digest, 0);
    params[n++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                                    (void *)key, keylen);
    if (xcghash != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_SSHKDF_XCGHASH, (void *)xcghash, xlen);
    if (session_id != NULL)
        params[n++] = OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_SSHKDF_SESSION_ID, (void *)session_id, sidlen);
    if (type != NULL)
        params[n++] = OSSL_PARAM_construct_utf8_string(
            OSSL_KDF_PARAM_SSHKDF_TYPE, (char *)type, 0);
    params[n] = OSSL_PARAM_construct_end();
    ok = EVP_KDF_derive(ctx, out, outlen, params) > 0;
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
    return ok;
}

/* CAVS set 1: a 129-byte K, and H == SID (the initial-IV case). */
static const char *const K1_HEX =
    "0000008055bae931c07fd824bf10add1902b6fbc7c665347383498a686929ff5"
    "a25f8e40cb6645ea814fb1a5e0a11f852f86255641e5ed986e83a78bc8269480"
    "eac0b0dfd770cab92e7a28dd87ff452466d6ae867cead63b366b1c286e6c4811"
    "a9f14c27aea14c5171d49b78c06e3735d36e6a3be321dd5fc82308f34ee1cb17"
    "fba94a59";
static const char *const H1_HEX = "a4ebd45934f56792b5112dcd75a1075fdc889245";
static const char *const SID1_HEX = "a4ebd45934f56792b5112dcd75a1075fdc889245";

static const struct ssh_kat {
    char type;
    size_t outlen;
    const char *outhex;
} ssh_kats[] = {
    { 'A', 8, "e2f627c0b43f1ac1" },
    { 'B', 8, "58471445f342b181" },
    { 'C', 24, "1ca9d310f86d51f6cb8e7007cb2b220d55c5281ce680b533" },
    { 'D', 24, "2c60df8603d34cc1dbb03c11f725a44b44008851c73d6844" },
    { 'E', 20, "472eb8a26166ae6aa8e06868e45c3b26e6eeed06" },
    { 'F', 20, "e3e2fdb9d7bc21165a3dbe47e1eceb7764390bab" },
};

#define K_CAP 132
#define HASH_CAP 32
#define SID_CAP 32
#define OUT_CAP 32

/* (1) All six types with SHA-1: the CAVS length is derived and compared,
 * and longer derives extend it (the CAVS value is always a prefix). */
static void test_types_sha1(void)
{
    unsigned char *key = ALLOCATE_KEY(K_CAP);
    unsigned char *h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *sid = ALLOCATE_SALT(SID_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(K1_HEX, key, K_CAP);
    size_t xlen = from_hex(H1_HEX, h, HASH_CAP);
    size_t sidlen = from_hex(SID1_HEX, sid, SID_CAP);
    size_t k;
    char msg[128];
    char typebuf[2];

    for (k = 0; k < sizeof(ssh_kats) / sizeof(ssh_kats[0]); k++) {
        typebuf[0] = ssh_kats[k].type;
        typebuf[1] = '\0';
        CHECK(sshkdf_derive("SHA1", key, keylen, h, xlen, sid, sidlen,
                            typebuf, out, 24),
              "SSHKDF derive failed");
        from_hex(ssh_kats[k].outhex, want, OUT_CAP);
        snprintf(msg, sizeof(msg), "type %c output does not match the CAVS KAT",
                 ssh_kats[k].type);
        CHECK(memcmp(out, want, ssh_kats[k].outlen) == 0, msg);
    }
}

/* (2) SHA-256 types A and C (CLI-verified on both builds; the SHA-256 H
 * must be the 32-byte digest-sized hash). */
static void test_types_sha256(void)
{
    unsigned char *key = ALLOCATE_KEY(K_CAP);
    unsigned char *h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *sid = ALLOCATE_SALT(SID_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(K1_HEX, key, K_CAP);
    size_t xlen = from_hex("a4ebd45934f56792b5112dcd75a1075fdc889245"
                           "e5acaf549cd25c22d964c0d9", h, HASH_CAP);
    size_t sidlen = from_hex(SID1_HEX, sid, SID_CAP);
    size_t wantlen;

    wantlen = from_hex("40f858eb8c8b5119a07d5254a8e2bc14591ee1cea7dcc88f"
                       "cde8d2aa165026f5", want, OUT_CAP);
    CHECK(sshkdf_derive("SHA256", key, keylen, h, xlen, sid, sidlen, "A",
                        out, 32),
          "SSHKDF SHA256 type A derive failed");
    expect_equal("SHA256 type A output does not match the KAT",
                 out, 32, want, wantlen);

    wantlen = from_hex("3f1dabb5b973cc21c5e349c31136eca5ec002ad6502080bd"
                       "877abcf77a6b59ae", want, OUT_CAP);
    CHECK(sshkdf_derive("SHA256", key, keylen, h, xlen, sid, sidlen, "C",
                        out, 32),
          "SSHKDF SHA256 type C derive failed");
    expect_equal("SHA256 type C output does not match the KAT",
                 out, 32, want, wantlen);
}

/* (3) The six types produce six pairwise-distinct outputs. */
static void test_types_differ(void)
{
    unsigned char *key = ALLOCATE_KEY(K_CAP);
    unsigned char *h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *sid = ALLOCATE_SALT(SID_CAP);
    unsigned char out[6][OUT_CAP];
    size_t keylen = from_hex(K1_HEX, key, K_CAP);
    size_t xlen = from_hex(H1_HEX, h, HASH_CAP);
    size_t sidlen = from_hex(SID1_HEX, sid, SID_CAP);
    size_t i, j;
    char typebuf[2];
    char msg[128];

    for (i = 0; i < 6; i++) {
        typebuf[0] = (char)('A' + (int)i);
        typebuf[1] = '\0';
        CHECK(sshkdf_derive("SHA1", key, keylen, h, xlen, sid, sidlen,
                            typebuf, out[i], 24),
              "SSHKDF derive failed in the distinctness test");
    }
    for (i = 0; i < 6; i++) {
        for (j = i + 1; j < 6; j++) {
            snprintf(msg, sizeof(msg),
                     "types %c and %c produced identical output",
                     (int)('A' + (int)i), (int)('A' + (int)j));
            CHECK(memcmp(out[i], out[j], 24) != 0, msg);
        }
    }
}

/* (4) Error paths and the wrong-length xcghhash behavior. */
static void test_negative(void)
{
    unsigned char *key = ALLOCATE_KEY(K_CAP);
    unsigned char *h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *sid = ALLOCATE_SALT(SID_CAP);
    unsigned char *short_h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(K1_HEX, key, K_CAP);
    size_t xlen = from_hex(H1_HEX, h, HASH_CAP);
    size_t sidlen = from_hex(SID1_HEX, sid, SID_CAP);
    size_t wantlen;

    /* A type outside A..F must be rejected. */
    CHECK(!sshkdf_derive("SHA1", key, keylen, h, xlen, sid, sidlen, "G",
                         out, 8),
          "SSHKDF accepted type G");
    /* A missing xcghhash or session id must be rejected. */
    CHECK(!sshkdf_derive("SHA1", key, keylen, NULL, 0, sid, sidlen, "A",
                         out, 8),
          "SSHKDF accepted a missing xcghhash");
    CHECK(!sshkdf_derive("SHA1", key, keylen, h, xlen, NULL, 0, "A",
                         out, 8),
          "SSHKDF accepted a missing session id");

    /* A wrong-length xcghhash is accepted by the default provider (RFC
     * 4253 conformance is not enforced); the output is deterministic and
     * identical on 3.0.13 and 3.6.5. */
    xlen = from_hex("a4ebd45934f56792b5112dcd75a1075f", short_h, HASH_CAP);
    CHECK(sshkdf_derive("SHA1", key, keylen, short_h, xlen, sid, sidlen,
                        "A", out, 8),
          "SSHKDF rejected a 15-byte xcghhash");
    wantlen = from_hex("dd8c1159d741e4f6", want, OUT_CAP);
    expect_equal("wrong-length xcghhash output is not the observed value",
                 out, 8, want, wantlen);
}

int main(int argc, char **argv)
{
    poison_parse_args(argc, argv);

    test_types_sha1();
    test_types_sha256();
    test_types_differ();
    test_negative();

    printf("test_kdf_sshkdf: all %u checks passed\n", g_checks);
    return 0;
}
