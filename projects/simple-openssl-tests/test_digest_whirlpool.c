#include "test_common.h"

#include "test_digest_mdc2_whirlpool_common.h"

#include <openssl/evp.h>

/*

 * test_digest_whirlpool -- WHIRLPOOL (legacy provider) known-answer and

 * behavior tests, one binary per digest (the shared MDC2/WHIRLPOOL

 * machinery, the vector provenance, and the legacy-provider situation live

 * in test_digest_mdc2_whirlpool_common.c).

 *

 * Known-answer provenance (WHIRLPOOL): the constants below were verified

 * with pizfix/bin/openssl, e.g.

 *

 *   printf 'message digest' | pizfix/bin/openssl dgst -provider legacy \

 *       -provider default -whirlpool

 *

 * (plus "abc" and the empty message).  WHIRLPOOL lives in the legacy

 * provider, which mdc2_whirlpool_load_providers() loads explicitly.

 *

 * Coverage for this digest: the "abc" (one-shot and chunked), "message

 * digest" and empty-message KATs through EVP_MD_fetch with the 64-byte

 * size check, and the EVP_get_digestbyname("whirlpool") route (which the

 * monolith ran for WHIRLPOOL only).

 */

/* This digest's KAT rows: "abc" (one-shot and chunked), "message digest"

 * (one-shot), and the empty message (one-shot). */

static const struct mdc2_whirlpool_kat kats[] = {

    { mdc2_whirlpool_msg_abc,

      "4e2448a4c6f486bb16b6562c73b4020bf3043e3a731bce721ae1b303d97e6d4c"

      "7181eebdb6c57e277d0e34957114cbd6c797fc9d95d8b582d225292076d4eef5",

      1 },

    { mdc2_whirlpool_msg_message_digest,

      "378c84a4126e2dc6e56dcc7458377aac838d00032230f53ce1f5700c0ffb4d3b"

      "8421557659ef55c106b4b52ac5a4aaa692ed920052838f3362e86dbd37a8903e",

      0 },

    { NULL, /* the empty message */

      "19fa61d75522a4669b44e39c1d2e1726c530232130d407f89afee0964997f7a7"

      "3e83be698b288febcf88e3e03c4f0757ea8964e59b63d93708b138cc42a66eb3",

      0 },

};

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    mdc2_whirlpool_load_providers();

    mdc2_whirlpool_test_kats("WHIRLPOOL", 64, kats,

                             sizeof(kats) / sizeof(kats[0]));

    /* kats[0] is the "abc" vector used by the byname route. */

    mdc2_whirlpool_test_byname_route("whirlpool", mdc2_whirlpool_msg_abc,

                                     MDC2_WHIRLPOOL_MSG_ABC_LEN,

                                     kats[0].mdhex);

    test_report_pass();

    return 0;

}

