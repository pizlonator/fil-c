#include "test_common.h"
#include "test_cbc_hmac_common.h"

#include <openssl/evp.h>

/*
 * test_aes_128_cbc_hmac_sha256_etm -- the encrypt-then-MAC variant
 * AES-128-CBC-HMAC-SHA256-ETM, one binary per cipher (the shared note on the
 * ETM record format -- record = IV || ciphertext || MAC over AAD ||
 * ciphertext -- lives in test_cbc_hmac_common.c).
 *
 * The -ETM variants are only registered on ARM64 builds of OpenSSL 3.5+; on
 * x86 (both 3.0.13 and 3.6.5) the fetch fails, so the whole program takes
 * the documented SKIP path here.  If a future build offers the cipher, the
 * ETM composition itself is documented in test_cbc_hmac_common.c and left to
 * the build that has the algorithm; this binary then demonstrates the fetch
 * is live with a streaming-mode init (no AAD).
 */

/* The ETM cipher under test. */
static const char *const CIPHERNAME = "AES-128-CBC-HMAC-SHA256-ETM";

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* The ETM variants are ARM64-only in OpenSSL 3.5+; when the fetch
     * fails, the whole program takes the documented SKIP path. */
    if (EVP_CIPHER_fetch(NULL, CIPHERNAME, NULL) == NULL)
        test_skip("the CBC-HMAC-SHA256-ETM variants are unavailable "
                  "(ARM64-only in OpenSSL 3.5+)");

    cbhmac_test_etm_streaming_init(CIPHERNAME);

    test_report_pass();
    return 0;
}
