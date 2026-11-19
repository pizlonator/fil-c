#include "test_common.h"

#include "test_mac_blake2_common.h"

#include <openssl/evp.h>

/*
 * test_mac_blake2b -- EVP_MAC "BLAKE2BMAC" (keyed BLAKE2b) known-answer
 * and behavior tests, one binary per MAC (the shared keyed-BLAKE2
 * machinery and the vector provenance live in test_mac_blake2_common.c).
 *
 * Known-answer provenance (BLAKE2BMAC): this MAC's rows of the shared
 * table -- the 64-byte-key output-size sweep {64,32,20,1} and the
 * 16-byte key-length variant at the default size -- every one produced
 * and verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl mac -macopt hexkey:000102...3f \
 *       -macopt size:20 BLAKE2BMAC
 *
 * with the message "abc".
 *
 * Coverage for this MAC: the output-size sweep {64,32,20,1} via the
 * "size" parameter with an EVP_MAC_CTX_get_mac_size cross-check at size
 * 20; the key lengths {16,64} accepted (the sweep's 16-byte-key row plus
 * the chunked section's 64-byte key); a zero-length key, a 65-byte key
 * and an output size of 65 rejected; and chunked updates (one-shot ==
 * chunked over a 200-byte deterministic message at the default size).
 */

/* The MAC under test: name, maximum key (the shared 000102...3f pattern),
 * the over-long key length, the default output size, and the unsupported
 * output size. */
static const struct b2mac_family fam = {
    "BLAKE2BMAC", b2mac_key64_hex, 64, 65, 64, 65,
};

/* This MAC's KAT rows: the 64-byte-key output-size sweep and the
 * 16-byte key-length variant (size 0: the provider default). */
static const struct b2mac_kat kats[] = {
    { b2mac_key64_hex, 64, 64,
      "06bbc3dedf13a31139498655251b7588ccd3bb5aaa071b2d44d8e0a04095579ed590fbfdcf941f4370ce5ce623624e7a76d33e7a8109dcda9b57d72f8f8efa51" },
    { b2mac_key64_hex, 32, 64,
      "dff38c978666dff5631db35ca15535520d134f5c8060ea569c6a178ad393719f" },
    { b2mac_key64_hex, 20, 64,
      "f3464811aec9776024bd78c73dbaad63a62c509b" },
    { b2mac_key64_hex, 1, 64,
      "46" },
    { b2mac_key16_hex, 0, 64,
      "4c76bc7ad0fc52e4bde231b38727c331172cfe3eeaf10cd2fa5c65abbdb8faeaad2da338531b382ac103f10ccaf41f74d8870d016c48b269c0d9a5cf5752c8c3" },
};

/* The chunked-update split cycle over the 200-byte message. */
static const size_t CHUNKS[] = { 1, 63, 64, 72 };

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    b2mac_test_kats(&fam, kats, sizeof(kats) / sizeof(kats[0]), 20);
    b2mac_test_chunked(&fam, CHUNKS, sizeof(CHUNKS) / sizeof(CHUNKS[0]));
    b2mac_test_negative(&fam);

    test_report_pass();
    return 0;
}
