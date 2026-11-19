#include "test_common.h"
#include "test_aes_wrap_common.h"

#include <openssl/evp.h>

/*

 * test_aes_128_wrap_inv -- AES-128-WRAP-INV tests, one binary per cipher
 * (the shared -INV rules -- the wrap and unwrap primitives exchanged per
 * SP 800-38F section 5.1, so only self-consistency and the same length
 * rules are asserted -- and the whole-program SKIP gate live in
 * test_aes_wrap_common.c).  The RFC 3394 KATs do not cover the -INV
 * variants, so this binary carries no KAT constants.

 */

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_test_inv_roundtrips("AES-128-WRAP-INV");

    test_report_pass();
    return 0;
}
