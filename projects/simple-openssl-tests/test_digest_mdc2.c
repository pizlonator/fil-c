#include "test_common.h"

#include "test_digest_mdc2_whirlpool_common.h"

#include <openssl/evp.h>

/*

 * test_digest_mdc2 -- MDC2 (legacy provider) known-answer and behavior

 * tests, one binary per digest (the shared MDC2/WHIRLPOOL machinery, the

 * vector provenance, the legacy-provider situation, and the version gate

 * live in test_digest_mdc2_whirlpool_common.c).

 *

 * Known-answer provenance (MDC2): the constants below were verified with

 * pizfix/bin/openssl, e.g.

 *

 *   printf 'abc' | pizfix/bin/openssl dgst -provider legacy \

 *       -provider default -mdc2

 *

 *   MDC2("abc")            = 3ff42120...

 *   MDC2("message digest") = fa13444f...

 *

 * (the ISO/IEC 10118-2 style KATs, default padding option).  MDC2 lives in

 * the legacy provider, which mdc2_whirlpool_load_providers() loads

 * explicitly.

 *

 * Version/build gate: MDC2 is absent from some distributions' OpenSSL 3.0

 * builds (build config, not a core 3.x property), so this binary takes the

 * whole-program SKIP path (mdc2_whirlpool_probe_or_skip) after the

 * provider load and exits 0 on those builds.

 */

/* This digest's KAT rows: "abc" (one-shot and chunked) and "message

 * digest" (one-shot). */

static const struct mdc2_whirlpool_kat kats[] = {

    { mdc2_whirlpool_msg_abc,

      "3ff42120ee863f5d910cf2ee5064f82f", 1 },

    { mdc2_whirlpool_msg_message_digest,

      "fa13444f04050d012f209890b1c235ab", 0 },

};

int main(int argc, char **argv)

{

    test_parse_args(argc, argv);

    mdc2_whirlpool_load_providers();

    /* Whole-program version gate: MDC2 is absent from some distributions'

     * OpenSSL 3.0 builds; it must run after the provider load, because on

     * builds that do offer MDC2 the fetch needs the legacy provider. */

    mdc2_whirlpool_probe_or_skip("MDC2");

    mdc2_whirlpool_test_kats("MDC2", 16, kats, sizeof(kats) / sizeof(kats[0]));

    test_report_pass();

    return 0;

}

