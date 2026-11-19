#include "test_common.h"

#include "test_rc4_common.h"

#include <openssl/evp.h>

/*
 * test_rc4_large -- RC4 (legacy provider) at >= 64 KiB through odd update
 * grids, one binary per cipher (the shared RC4 machinery, the provider
 * situation, and the family-wide commentary live in test_rc4_common.c;
 * the large-data regime sections below are its own).
 *
 * The legacy RC4 core processes 8 bytes at a time through its bulk path
 * (rc4_enc.c) with the i/j stream state carried across updates, so a long
 * run of uneven updates must reproduce the one-shot keystream exactly --
 * that equality is the oracle here (RC4 has no per-size KATs beyond the
 * classic small vectors in the sibling program test_rc4.c).
 */

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    rc4_load_providers();

    rc4_large_test_64k();
    rc4_large_test_128k_and_bytewise();

    test_report_pass();
    return 0;
}
