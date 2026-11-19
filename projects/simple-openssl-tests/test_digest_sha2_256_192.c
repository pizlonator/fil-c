#include "test_common.h"
#include "test_digest_sha2_common.h"

#include <openssl/evp.h>

/*
 * test_digest_sha2_256_192 -- the SHA2-256/192 truncation, one binary per
 * digest (the shared SHA-2 machinery lives in test_digest_sha2_common.c).
 *
 * SHA2-256/192 is genuinely version-gated: no current OpenSSL 3.x release
 * ships it (the monolith gated it last in main()), so this binary takes the
 * whole-program SKIP path right after test_parse_args().  If a build does
 * offer it, there is no verified KAT here; the binary then checks what we
 * can, exactly what the original checked: the digest size and the
 * chunked/one-shot self-consistency on the "abc" message.
 */

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* Whole-program version gate: SHA2-256/192 is unavailable from every
     * current OpenSSL 3.x release. */
    sha2_probe_or_skip("SHA2-256/192");

    /* No verified KAT exists for SHA2-256/192 here: verify what we can
     * without one -- the digest size (24 bytes) and the chunked/one-shot
     * self-consistency on the "abc" message. */
    sha2_test_256_192_selfcheck("SHA2-256/192", 24);

    test_report_pass();
    return 0;
}
