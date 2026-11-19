/*

 * test_null_common.c -- shared implementation of the NULL identity-object
 * tests (API in test_null_common.h; two binaries, one object each:
 * test_null_cipher.c tests the NULL cipher, test_null_digest.c the NULL
 * digest).

 * The NULL identity objects are the smallest possible EVP objects, useful
 * for probing the API contract itself:

 *   - The NULL cipher ("NULL", also available statically as
 *     EVP_enc_null()) has a 0-byte key, a 0-byte IV and a block size of
 *     1: every Encrypt/DecryptUpdate passes its input through unchanged
 *     and Final emits nothing.  EVP_get_cipherbyname("NULL") returns NULL
 *     on the OpenSSL 3.x builds exercised here (3.0.13 and 3.6.5) -- the
 *     identity cipher has no OBJ name -- so only the fetch and static
 *     routes are checked (test_null_cipher.c).

 *   - The NULL digest ("NULL", also available statically as
 *     EVP_md_null()) produces a 0-byte digest of any input.  It is present
 *     in both of the OpenSSL 3.x builds exercised here; if some build
 *     lacks it, the fetch fails and test_null_digest.c prints SKIP and
 *     exits 0 (a genuinely optional digest, not a core one).

 * Everything is verified behaviorally (identity roundtrips and zero
 * lengths) -- there are no KAT hex constants to embed for objects whose
 * entire output is empty.  The one piece both halves share is the
 * deterministic 64-byte input pattern below.
 */

#include "test_null_common.h"

/* Fills buf[0..n) with the deterministic pattern i*3+1. */
void null_fill_pattern(unsigned char *buf, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)(i * 3 + 1);
}
