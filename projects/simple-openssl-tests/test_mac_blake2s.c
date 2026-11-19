#include "test_common.h"

#include "test_mac_blake2_common.h"

#include <openssl/evp.h>

/*
 * test_mac_blake2s -- EVP_MAC "BLAKE2SMAC" (keyed BLAKE2s) known-answer
 * and behavior tests, one binary per MAC (the shared keyed-BLAKE2
 * machinery and the vector provenance live in test_mac_blake2_common.c).
 *
 * Known-answer provenance (BLAKE2SMAC): this MAC's rows of the shared
 * table -- the 32-byte-key output-size sweep {32,16,1} and the 16-byte
 * key-length variant at the default size -- every one produced and
 * verified with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl mac -macopt hexkey:000102...1f \
 *       -macopt size:20 BLAKE2SMAC
 *
 * with the message "abc".
 *
 * Coverage for this MAC: the output-size sweep {32,16,1} via the "size"
 * parameter with an EVP_MAC_CTX_get_mac_size cross-check at size 20; the
 * key lengths {16,32} accepted (the sweep's 16-byte-key row plus the
 * chunked section's 32-byte key); a zero-length key, a 33-byte key and an
 * output size of 33 rejected; and chunked updates (one-shot == chunked
 * over a 200-byte deterministic message at the default size).
 */

/* The MAC under test: name, maximum key (the shared 000102...1f pattern),
 * the over-long key length, the default output size, and the unsupported
 * output size. */
static const struct b2mac_family fam = {
    "BLAKE2SMAC", b2mac_key32_hex, 32, 33, 32, 33,
};

/* This MAC's KAT rows: the 32-byte-key output-size sweep and the
 * 16-byte key-length variant (size 0: the provider default). */
static const struct b2mac_kat kats[] = {
    { b2mac_key32_hex, 32, 32,
      "a281f725754969a702f6fe36fc591b7def866e4b70173ece402fc01c064d6b65" },
    { b2mac_key32_hex, 16, 32,
      "61ba5f165c194692e09d12520cc4c74a" },
    { b2mac_key32_hex, 1, 32,
      "d1" },
    { b2mac_key16_hex, 0, 32,
      "033a40b64c6296fc2e3584d2a06bcf1a8003ebb9b8007940e2f820b645917a4e" },
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
