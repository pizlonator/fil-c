/* test_digest_md5_common.h -- shared machinery for the MD5 / MD4 / MD5-SHA1

 * digest family tests.

 *

 * One binary per digest (test_digest_md5.c, test_digest_md4.c and

 * test_digest_md5_sha1.c); each binary defines its own KAT rows

 * (struct md5_kat) and calls the per-digest test routines below.  The big

 * explanatory comment (the vector provenance and the legacy-provider

 * situation) lives in test_digest_md5_common.c.

 */

#ifndef TEST_DIGEST_MD5_COMMON_H

#define TEST_DIGEST_MD5_COMMON_H

#include "test_common.h"

#include <stddef.h>

#include <openssl/evp.h>

/* One KAT row: the digest name (the EVP_MD_fetch name), the message (NULL

 * means the empty message), and the expected digest. */

struct md5_kat {

    const char *mdname;

    const char *msg;        /* NULL means the empty message */

    const char *mdhex;

};

/* The shared "abc" message (the KAT message of MD4, MD5 and MD5-SHA1).

 * Declared as an array, not a pointer, so that it is an address constant

 * usable in the static struct md5_kat initializers of the per-digest

 * binaries. */

extern const char md5_msg_abc[];

/* The legacy provider is required for MD4 (and hosts MDC2/WHIRLPOOL in the

 * sibling families); MD5 and MD5-SHA1 come from the default provider.

 * Loading a provider explicitly disables default-provider auto-load, so

 * BOTH are loaded here.  Every family binary calls this right after

 * test_parse_args(), before its first fetch. */

void md5_load_providers(void);

/* (1) One-shot EVP_Digest with EVP_MD_fetch == KAT for every row; also

 * checks EVP_MD_get_size against the KAT digest length.  MD4's fetch

 * requires the legacy provider loaded above. */

void md5_test_kats(const struct md5_kat *kats, size_t nkats);

/* (2) The "abc" KAT (abchex) through the static route (`static_md`, spelled

 * static_name in the CHECK messages: EVP_md5() / EVP_md4() /

 * EVP_md5_sha1()) and through EVP_get_digestbyname(byname); the static

 * route digests one-shot, the byname route chunked. */

void md5_test_routes(const char *abchex, const EVP_MD *static_md,

                     const char *static_name, const char *byname);

/* (3) Chunked-vs-one-shot equality for the digest named mdname around the

 * 64-byte block boundary (deterministic byte pattern i*7+3). */

void md5_test_chunked_equality(const char *mdname);

/* One-shot digest via EVP_Digest; used by test_digest_md5_sha1.c's

 * composition cross-check against the two component digests.  Returns the

 * digest length. */

size_t md5_digest_one_shot(const EVP_MD *md, const unsigned char *in,

                           size_t inlen, unsigned char *out, size_t outcap);

/* Digest via init/update/final with explicit chunk lengths {a, b, rest}

 * (the MD5-SHA1 streaming split across the two halves).  Returns the

 * digest length. */

size_t md5_digest_chunked_ab(const EVP_MD *md, const unsigned char *in,

                             size_t inlen, size_t a, size_t b,

                             unsigned char *out, size_t outcap);

#endif /* TEST_DIGEST_MD5_COMMON_H */

