#include "test_common.h"
#include "test_digest_shake_common.h"

#include <openssl/evp.h>

/*
 * test_digest_shake128 -- SHAKE128 XOF tests, one binary per digest (the
 * shared XOF machinery, the vector provenance, and the version notes live
 * in test_digest_shake_common.c).
 *
 * Known-answer provenance (SHAKE128): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -shake128 -xoflen 32
 *   printf '' | pizfix/bin/openssl dgst -shake128 -xoflen 16
 */

/* This digest's rows: "abc" squeezed to 32 bytes and the empty message
 * squeezed to 16 (NULL msg means the empty message). */
static const struct shake_kat kats[] = {
    { "SHAKE128", "abc", 32,
      "5881092dd818bf5cf8a3ddb793fbcba74097d5c526a6d35f97b83351940f2cc8" },
    { "SHAKE128", NULL, 16,
      "7f9c2ba4e88f827d616045507605853e" },
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    shake_test_kat(&kats[0]);
    shake_test_kat(&kats[1]);
    shake_test_prefix_property("SHAKE128", 32);

#if OPENSSL_VERSION_NUMBER >= 0x30500000L
    /* (3) EVP_DigestSqueeze (3.5+): squeezing 10 bytes and then 100 bytes
     * must equal a single 110-byte squeeze. */
    shake_test_squeeze("SHAKE128");

    test_report_pass();
#else
    /* The squeeze section is compiled out on pre-3.5 headers: the
     * documented mid-run SKIP line ends the run (exit 0, no pass line),
     * exactly like the monolith's gate. */
    printf("SKIP %s: EVP_DigestSqueeze is unavailable\n", test_prog_name());
#endif
    return 0;
}
