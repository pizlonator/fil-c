/*

 * test_kdf_sshkdf_common.c -- shared implementation of the SSHKDF family
 * tests (API in test_kdf_sshkdf_common.h; one binary per digest, one struct
 * sshkdf_family row and one KAT row set per binary: test_kdf_sshkdf_sha1.c
 * and test_kdf_sshkdf_sha256.c).
 *
 * The family covers the SSH RFC 4253 exchange-hash KDF through the EVP_KDF
 * public API: all six derivation types (A..F), the distinctness of the six
 * outputs, and the error paths.
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
 *
 */

#include "test_kdf_sshkdf_common.h"

#include <openssl/core_names.h>
#include <openssl/kdf.h>

/* CAVS set 1: a 129-byte K, and H == SID (the initial-IV case).  The H1
 * value doubles as the session id in every binary (exactly as the
 * monolith's SHA-256 sections did), and the 15-byte prefix of the H
 * constants is the wrong-length xcghhash the negative section feeds.  The
 * constants are arrays (not pointers) so that they are address constants
 * usable in the per-digest binaries' static struct initializers. */
const char sshkdf_k1_hex[] =
    "0000008055bae931c07fd824bf10add1902b6fbc7c665347383498a686929ff5"
    "a25f8e40cb6645ea814fb1a5e0a11f852f86255641e5ed986e83a78bc8269480"
    "eac0b0dfd770cab92e7a28dd87ff452466d6ae867cead63b366b1c286e6c4811"
    "a9f14c27aea14c5171d49b78c06e3735d36e6a3be321dd5fc82308f34ee1cb17"
    "fba94a59";
const char sshkdf_h1_hex[] = "a4ebd45934f56792b5112dcd75a1075fdc889245";
const char sshkdf_sid1_hex[] = "a4ebd45934f56792b5112dcd75a1075fdc889245";
const char sshkdf_hshort_hex[] = "a4ebd45934f56792b5112dcd75a1075f";

/* Working-buffer sizes (the original monolith's K_CAP/HASH_CAP/SID_CAP/
 * OUT_CAP: the 129-byte CAVS set 1 K, the 32-byte SHA-256 H and the 32-byte
 * SHA-256 type A/C outputs are the family's longest material). */
#define K_CAP 132
#define HASH_CAP 32
#define SID_CAP 32
#define OUT_CAP 32

/* One SSHKDF derive with a fresh context (always the "SSHKDF" KDF).
 * Returns 1 on success, 0 on failure.  SSHKDF is core OpenSSL 3
 * functionality: a fetch failure fails the program (never SKIP). */
int sshkdf_derive(const char *digest, const unsigned char *key,
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

/* (1) The KAT rows of the calling binary's digest: the CAVS length is
 * derived and compared, and longer derives extend it (the CAVS value is
 * always a prefix). */
void sshkdf_test_kats(const struct sshkdf_family *fam,
                      const struct ssh_kat *kats, size_t nkats)
{
    unsigned char *key = ALLOCATE_KEY(K_CAP);
    unsigned char *h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *sid = ALLOCATE_SALT(SID_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(sshkdf_k1_hex, key, K_CAP);
    size_t xlen = from_hex(fam->h_hex, h, HASH_CAP);
    size_t sidlen = from_hex(sshkdf_sid1_hex, sid, SID_CAP);
    size_t k;
    char msg[128];
    char typebuf[2];

    for (k = 0; k < nkats; k++) {
        typebuf[0] = kats[k].type;
        typebuf[1] = '\0';
        snprintf(msg, sizeof(msg), "SSHKDF %s type %c derive failed",
                 fam->digest, kats[k].type);
        CHECK(sshkdf_derive(fam->digest, key, keylen, h, xlen, sid, sidlen,
                            typebuf, out, kats[k].derivelen), msg);
        from_hex(kats[k].outhex, want, OUT_CAP);
        snprintf(msg, sizeof(msg),
                 "%s type %c output does not match the CAVS KAT",
                 fam->digest, kats[k].type);
        unpoison_all();
        CHECK(memcmp(out, want, kats[k].outlen) == 0, msg);
        repoison_all();
    }

    free_poison_allocations();
}

/* (2) The six types produce six pairwise-distinct outputs, over the
 * binary's own digest and digest-sized H. */
void sshkdf_test_types_differ(const struct sshkdf_family *fam)
{
    unsigned char *key = ALLOCATE_KEY(K_CAP);
    unsigned char *h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *sid = ALLOCATE_SALT(SID_CAP);
    unsigned char *out[6];
    size_t keylen = from_hex(sshkdf_k1_hex, key, K_CAP);
    size_t xlen = from_hex(fam->h_hex, h, HASH_CAP);
    size_t sidlen = from_hex(sshkdf_sid1_hex, sid, SID_CAP);
    size_t i, j;
    char typebuf[2];
    char msg[128];

    for (i = 0; i < 6; i++)
        out[i] = ALLOCATE_OUTPUT(OUT_CAP);
    for (i = 0; i < 6; i++) {
        typebuf[0] = (char)('A' + (int)i);
        typebuf[1] = '\0';
        snprintf(msg, sizeof(msg),
                 "SSHKDF derive failed in the distinctness test (%s)",
                 fam->digest);
        CHECK(sshkdf_derive(fam->digest, key, keylen, h, xlen, sid, sidlen,
                            typebuf, out[i], 24), msg);
    }
    for (i = 0; i < 6; i++) {
        for (j = i + 1; j < 6; j++) {
            snprintf(msg, sizeof(msg),
                     "types %c and %c produced identical output (%s)",
                     (int)('A' + (int)i), (int)('A' + (int)j), fam->digest);
            unpoison_all();
            CHECK(memcmp(out[i], out[j], 24) != 0, msg);
            repoison_all();
        }
    }

    free_poison_allocations();
}

/* (3) Error paths and the wrong-length xcghhash behavior.  Every binary
 * runs it for its own digest, comparing the wrong-length output against
 * its verified constant where one exists and against a second derive where
 * none does. */
void sshkdf_test_negative(const struct sshkdf_family *fam,
                          const char *short_h_out_hex)
{
    unsigned char *key = ALLOCATE_KEY(K_CAP);
    unsigned char *h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *sid = ALLOCATE_SALT(SID_CAP);
    unsigned char *short_h = ALLOCATE_SALT(HASH_CAP);
    unsigned char *out = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *out2 = ALLOCATE_OUTPUT(OUT_CAP);
    unsigned char *want = ALLOCATE_OUTPUT(OUT_CAP);
    size_t keylen = from_hex(sshkdf_k1_hex, key, K_CAP);
    size_t xlen = from_hex(fam->h_hex, h, HASH_CAP);
    size_t sidlen = from_hex(sshkdf_sid1_hex, sid, SID_CAP);
    size_t wantlen;
    char msg[128];

    /* A type outside A..F must be rejected. */
    snprintf(msg, sizeof(msg), "SSHKDF accepted type G (%s)", fam->digest);
    CHECK(!sshkdf_derive(fam->digest, key, keylen, h, xlen, sid, sidlen, "G",
                         out, 8), msg);
    /* A missing xcghhash or session id must be rejected. */
    snprintf(msg, sizeof(msg), "SSHKDF accepted a missing xcghhash (%s)",
             fam->digest);
    CHECK(!sshkdf_derive(fam->digest, key, keylen, NULL, 0, sid, sidlen, "A",
                         out, 8), msg);
    snprintf(msg, sizeof(msg), "SSHKDF accepted a missing session id (%s)",
             fam->digest);
    CHECK(!sshkdf_derive(fam->digest, key, keylen, h, xlen, NULL, 0, "A",
                         out, 8), msg);

    /* A wrong-length xcghhash is accepted by the default provider (RFC
     * 4253 conformance is not enforced); the output is deterministic and
     * identical on 3.0.13 and 3.6.5. */
    xlen = from_hex(sshkdf_hshort_hex, short_h, HASH_CAP);
    snprintf(msg, sizeof(msg), "SSHKDF rejected a 15-byte xcghhash (%s)",
             fam->digest);
    CHECK(sshkdf_derive(fam->digest, key, keylen, short_h, xlen, sid, sidlen,
                        "A", out, 8), msg);
    if (short_h_out_hex != NULL) {
        wantlen = from_hex(short_h_out_hex, want, OUT_CAP);
        snprintf(msg, sizeof(msg),
                 "wrong-length xcghhash output is not the observed value "
                 "(%s)", fam->digest);
        expect_equal(msg, out, 8, want, wantlen);
    } else {
        /* No verified wrong-length constant for this digest: the same
         * acceptance is checked relatively, by deriving twice and requiring
         * identical output (the wrong-length H derivation is
         * deterministic). */
        CHECK(sshkdf_derive(fam->digest, key, keylen, short_h, xlen, sid,
                            sidlen, "A", out2, 8),
              "second wrong-length xcghhash derive failed");
        unpoison_all();
        CHECK(memcmp(out, out2, 8) == 0,
              "wrong-length xcghhash output is not deterministic");
        repoison_all();
    }

    free_poison_allocations();
}