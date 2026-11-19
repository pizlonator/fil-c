#include "test_common.h"

#include "test_digest_md5_common.h"

#include <openssl/evp.h>

/*

 * test_digest_md5_sha1 -- the MD5-SHA1 two-digest composition, one binary

 * per digest (the shared MD5/MD4/MD5-SHA1 machinery, the vector provenance,

 * and the legacy-provider situation live in test_digest_md5_common.c).

 *

 * Known-answer provenance (MD5-SHA1): the constant below was verified with

 * pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -md5-sha1

 *

 *   MD5-SHA1("abc") = MD5("abc") || SHA1("abc")   (36 bytes)

 *

 * Coverage for this digest: the 36-byte "abc" KAT through EVP_MD_fetch

 * (one-shot and the {2,20,rest} streaming split across the two halves),

 * the composition cross-check that re-derives the digest from its MD5 and

 * SHA1 components, the static EVP_md5_sha1() route and the

 * EVP_get_digestbyname("md5-sha1") route, and chunked-vs-one-shot equality

 * around the 64-byte block boundary.

 */

/* The MD5-SHA1("abc") KAT: MD5("abc") || SHA1("abc"), 36 bytes. */

static const char MD5_SHA1_ABC_HEX[] =

    "900150983cd24fb0d6963f7d28e17f72"

    "a9993e364706816aba3e25717850c26c9cd0d89d";

/* The 36-byte concatenation of MD5("abc") and SHA1("abc"): verified as a

 * KAT, split across the two halves with streaming updates ({2, 20, rest}

 * bytes), and by re-deriving it from the two component digests. */

static void test_md5_sha1(void)

{

    EVP_MD *md = EVP_MD_fetch(NULL, "MD5-SHA1", NULL);

    EVP_MD *md5 = EVP_MD_fetch(NULL, "MD5", NULL);

    EVP_MD *sha1 = EVP_MD_fetch(NULL, "SHA1", NULL);

    unsigned char *in = ALLOCATE_INPUT(3);

    unsigned char *want = ALLOCATE_OTHER(36);

    unsigned char *got = ALLOCATE_OUTPUT(36);

    unsigned char *halves = ALLOCATE_OTHER(36);

    unsigned char *md5_out = ALLOCATE_OUTPUT(16);

    unsigned char *sha1_out = ALLOCATE_OUTPUT(20);

    size_t wantlen, gotlen, md5len, sha1len;

    CHECK(md != NULL, "EVP_MD_fetch(\"MD5-SHA1\") failed");

    CHECK(md5 != NULL && sha1 != NULL, "EVP_MD_fetch failed for MD5 or SHA1");

    CHECK(EVP_MD_get_size(md) == 36, "EVP_MD_get_size(MD5-SHA1) is not 36");

    memcpy(in, md5_msg_abc, strlen(md5_msg_abc));

    wantlen = from_hex(MD5_SHA1_ABC_HEX, want, 36);

    gotlen = md5_digest_one_shot(md, in, 3, got, 36);

    unpoison_all();

    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,

          "MD5-SHA1(\"abc\") one-shot digest does not match the KAT");

    repoison_all();

    memset(got, 0, 36);

    gotlen = md5_digest_chunked_ab(md, in, 3, 2, 20, got, 36);

    unpoison_all();

    CHECK(gotlen == wantlen && memcmp(got, want, wantlen) == 0,

          "MD5-SHA1 streaming updates ({2,20,rest}) do not match the KAT");

    repoison_all();

    md5len = md5_digest_one_shot(md5, in, 3, md5_out, 16);

    sha1len = md5_digest_one_shot(sha1, in, 3, sha1_out, 20);

    CHECK(md5len == 16 && sha1len == 20, "component digest lengths are wrong");

    unpoison_all();

    memcpy(halves, md5_out, 16);

    memcpy(halves + 16, sha1_out, 20);

    CHECK(memcmp(halves, want, 36) == 0,

          "MD5-SHA1 is not the concatenation MD5(\"abc\") || SHA1(\"abc\")");

    repoison_all();

    EVP_MD_free(md);

    EVP_MD_free(md5);

    EVP_MD_free(sha1);

    free_poison_allocations();

}

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    md5_load_providers();

    test_md5_sha1();

    md5_test_routes(MD5_SHA1_ABC_HEX, EVP_md5_sha1(), "EVP_md5_sha1()",

                    "md5-sha1");

    md5_test_chunked_equality("MD5-SHA1");

    test_report_pass();

    return 0;

}

