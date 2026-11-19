/* test_digest_mdc2_whirlpool_common.h -- shared machinery for the MDC2 and

 * WHIRLPOOL (legacy provider) digest tests.

 *

 * One binary per digest (test_digest_mdc2.c and test_digest_whirlpool.c);

 * each binary defines its own KAT rows (struct mdc2_whirlpool_kat) and

 * calls the per-digest test routines below.  The big explanatory comment

 * (the vector provenance, the legacy-provider situation, and the MDC2

 * version gate) lives in test_digest_mdc2_whirlpool_common.c.

 */

#ifndef TEST_DIGEST_MDC2_WHIRLPOOL_COMMON_H

#define TEST_DIGEST_MDC2_WHIRLPOOL_COMMON_H

#include "test_common.h"

#include <stddef.h>

#include <openssl/evp.h>

/* One KAT row: the message (NULL means the empty message) and the expected

 * digest; chunked != 0 also digests the message with the awkward

 * {1,63,rest} update split. */

struct mdc2_whirlpool_kat {

    const char *msg;        /* NULL means the empty message */

    const char *mdhex;

    int chunked;

};

/* The two KAT messages of this family ("abc" and "message digest").

 * Declared as arrays, not pointers, so that they are address constants

 * usable in the static struct mdc2_whirlpool_kat initializers and the

 * byname-route calls of the per-digest binaries. */

extern const char mdc2_whirlpool_msg_abc[];

extern const char mdc2_whirlpool_msg_message_digest[];

/* Byte lengths of the shared message constants. */

#define MDC2_WHIRLPOOL_MSG_ABC_LEN 3

#define MDC2_WHIRLPOOL_MSG_MESSAGE_DIGEST_LEN 14

/* The legacy provider hosts MDC2 and WHIRLPOOL in OpenSSL 3.x.  Loading a

 * provider explicitly disables default-provider auto-load, so BOTH are

 * loaded here (legacy hosts MD4/MDC2/WHIRLPOOL; everything else comes from

 * default).  Every family binary calls this right after test_parse_args(),

 * before its first fetch -- including the version gate below, which needs

 * the legacy provider on builds that do offer these digests. */

void mdc2_whirlpool_load_providers(void);

/* Whole-program version gate: MDC2 is absent from some distributions'

 * OpenSSL 3.0 builds (build config, not a core 3.x property).  SKIP with

 * the documented reason ("%s is unavailable") when the digest does not

 * fetch, release the probe handle otherwise. */

void mdc2_whirlpool_probe_or_skip(const char *mdname);

/* (1) The rows' KATs for the digest named mdname through EVP_MD_fetch:

 * one-shot for every row, plus the awkward {1,63,rest} chunked digest for

 * the rows with chunked != 0.  Also checks EVP_MD_get_size against `size`. */

void mdc2_whirlpool_test_kats(const char *mdname, int size,

                              const struct mdc2_whirlpool_kat *kats,

                              size_t nkats);

/* (2) The KAT (kathex) for the msglen-byte message `msg` via the

 * EVP_get_digestbyname(byname) route, chunked. */

void mdc2_whirlpool_test_byname_route(const char *byname, const char *msg,

                                      size_t msglen, const char *kathex);

#endif /* TEST_DIGEST_MDC2_WHIRLPOOL_COMMON_H */

