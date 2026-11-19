#include "test_common.h"
#include "test_digest_shake_common.h"

#include <openssl/evp.h>

/*
 * test_digest_shake256 -- SHAKE256 XOF tests, one binary per digest (the
 * shared XOF machinery, the vector provenance, and the version notes live
 * in test_digest_shake_common.c).
 *
 * Known-answer provenance (SHAKE256): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -shake256 -xoflen 64
 *   printf '' | pizfix/bin/openssl dgst -shake256 -xoflen 32
 */

/* This digest's rows: "abc" squeezed to 64 bytes and the empty message
 * squeezed to 32 (NULL msg means the empty message). */
static const struct shake_kat kats[] = {
    { "SHAKE256", "abc", 64,
      "483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739d5a15bef186a5386c75744c0527e1faa9f8726e462a12a4feb06bd8801e751e4" },
    { "SHAKE256", NULL, 32,
      "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    shake_test_kat(&kats[0]);
    shake_test_kat(&kats[1]);
    shake_test_prefix_property("SHAKE256", 64);

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
    /* (3) EVP_DigestSqueeze (3.5+): squeezing 10 bytes and then 100 bytes
     * must equal a single 110-byte squeeze. */
    shake_test_squeeze("SHAKE256");

    test_report_pass();
#else
    /* The squeeze section is compiled out on pre-3.5 headers: the
     * documented mid-run SKIP line ends the run (exit 0, no pass line),
     * exactly like the monolith's gate. */
    printf("SKIP %s: EVP_DigestSqueeze is unavailable\n", test_prog_name());
#endif
    return 0;
}
