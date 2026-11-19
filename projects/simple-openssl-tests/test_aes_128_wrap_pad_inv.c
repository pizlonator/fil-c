#include "test_common.h"
#include "test_aes_wrap_pad_common.h"

#include <openssl/evp.h>

/*

 * test_aes_128_wrap_pad_inv -- AES-128-WRAP-PAD-INV tests, one binary per
 * cipher (the shared -INV rules -- the inverse AES block transform per SP
 * 800-38F section 5.1 with the RFC 5649 block structure, so the output
 * sizes are identical -- and the whole-program SKIP gate live in
 * test_aes_wrap_pad_common.c).  RFC 5649 has no example for the 128-bit
 * -INV variant, so this binary carries no KAT constants.

 */

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    wrap_pad_test_inv("AES-128-WRAP-PAD-INV", NULL, NULL);

    test_report_pass();
    return 0;
}
