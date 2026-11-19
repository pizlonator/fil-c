#include "test_common.h"

#include "test_null_common.h"

#include <openssl/evp.h>

/*

 * test_null_digest -- the NULL identity digest: the smallest possible EVP
 * digest, useful for probing the API contract itself (the shared
 * commentary lives in test_null_common.c).

 * The NULL digest ("NULL", also available statically as EVP_md_null())
 * produces a 0-byte digest of any input.  It is present in both of the
 * OpenSSL 3.x builds exercised here (3.0.13 and 3.6.5); if some build
 * lacks it, the fetch fails and the whole program prints SKIP and exits 0
 * (a genuinely optional digest, not a core one) -- the digest is this
 * binary's whole subject, so the probe happens right after
 * test_parse_args().  Everything is verified behaviorally (zero lengths
 * and input-independence) -- there are no KAT hex constants to embed for
 * an object whose entire output is empty.
 */

/* (1) The NULL digest through EVP_MD_fetch: size 0, empty one-shot
 * output, empty streaming output, and input-independence. */
static void test_null_digest(void)
{
    EVP_MD *md = EVP_MD_fetch(NULL, "NULL", NULL);
    unsigned char *in = ALLOCATE_INPUT(NULL_PATTERN_LEN);
    unsigned char *out = ALLOCATE_OUTPUT(0);
    unsigned char *out2 = ALLOCATE_OUTPUT(0);
    EVP_MD_CTX *ctx = NULL;
    unsigned int olen = 9999;

    CHECK(md != NULL, "EVP_MD_fetch(\"NULL\") failed");
    CHECK(EVP_MD_get_size(md) == 0, "the NULL digest size is not 0");
    null_fill_pattern(in, NULL_PATTERN_LEN);

    CHECK(EVP_Digest(in, NULL_PATTERN_LEN, out, &olen, md, NULL) == 1,
          "EVP_Digest with the NULL digest failed");
    CHECK(olen == 0, "the NULL digest one-shot output is not empty");

    ctx = EVP_MD_CTX_new();
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    CHECK(EVP_DigestUpdate(ctx, in, NULL_PATTERN_LEN) == 1,
          "EVP_DigestUpdate failed");
    olen = 9999;
    CHECK(EVP_DigestFinal_ex(ctx, out, &olen) == 1,
          "EVP_DigestFinal_ex failed");
    CHECK(olen == 0, "the NULL digest streaming output is not empty");
    EVP_MD_CTX_free(ctx);

    /* Any input produces the same (empty) output. */
    olen = 0;
    CHECK(EVP_Digest(in, 1, out2, &olen, md, NULL) == 1,
          "EVP_Digest with a 1-byte input failed");
    CHECK(olen == 0, "the NULL digest output for a 1-byte input is not empty");
    EVP_MD_free(md);

    free_poison_allocations();
}

/* (2) The static route EVP_md_null(): identical behavior. */
static void test_md_null_static(void)
{
    const EVP_MD *md = EVP_md_null();
    unsigned char *in = ALLOCATE_INPUT(NULL_PATTERN_LEN);
    unsigned char *out = ALLOCATE_OUTPUT(0);
    EVP_MD_CTX *ctx = NULL;
    unsigned int olen = 9999;

    CHECK(md != NULL, "EVP_md_null() returned NULL");
    CHECK(EVP_MD_get_size(md) == 0, "EVP_md_null() size is not 0");
    null_fill_pattern(in, NULL_PATTERN_LEN);
    ctx = EVP_MD_CTX_new();
    CHECK(ctx != NULL, "EVP_MD_CTX_new failed");
    CHECK(EVP_DigestInit_ex(ctx, md, NULL) == 1, "EVP_DigestInit_ex failed");
    CHECK(EVP_DigestUpdate(ctx, in, NULL_PATTERN_LEN) == 1,
          "EVP_DigestUpdate failed");
    CHECK(EVP_DigestFinal_ex(ctx, out, &olen) == 1,
          "EVP_DigestFinal_ex failed");
    CHECK(olen == 0, "EVP_md_null() output is not empty");
    EVP_MD_CTX_free(ctx);

    free_poison_allocations();
}

int main(int argc, char **argv)
{
    EVP_MD *probe;

    test_parse_args(argc, argv);

    /* The NULL digest is genuinely optional: if some build lacks it, the
     * fetch fails and the whole program skips (the digest is this
     * binary's whole subject). */
    probe = EVP_MD_fetch(NULL, "NULL", NULL);
    if (probe == NULL)
        test_skip("the NULL digest is unavailable");
    EVP_MD_free(probe);

    test_null_digest();
    test_md_null_static();

    test_report_pass();
    return 0;
}
