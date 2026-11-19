#include "test_common.h"
#include "test_digest_sha3_common.h"

#include <openssl/evp.h>

/*
 * test_digest_keccak_512 -- KECCAK-512 known-answer and behavior tests, one
 * binary per digest (the shared SHA-3 machinery, the vector provenance, and
 * the KECCAK version note live in test_digest_sha3_common.c).
 *
 * Known-answer provenance (KECCAK-512): the constants below were verified
 * with pizfix/bin/openssl, e.g.
 *
 *   printf 'abc' | pizfix/bin/openssl dgst -keccak-512
 *   printf '' | pizfix/bin/openssl dgst -keccak-512
 *
 * KECCAK-* differs from SHA3-* only in the domain-separation padding byte
 * (0x01 vs 0x06), so every vector is distinct from the SHA3-512 row.
 */

/* This digest's row: the "abc" and empty-message KATs. */
static const struct sha3_kat kat = {
    "KECCAK-512", 64,
    "18587dc2ea106b9a1563e32b3312421ca164c7f1f07bc922a9c83d77cea3a1e5d0c69910739025372dc14ac9642629379540c17e2a65b19d77aa511a9d00bb96",
    "0eab42de4c3ceb9235fc91acffe746b29c29a8c366b7c60e4e67c466f36a4304c00fa9caf9d87976ba469bcbe06713b435f091ef2769fb160cdab33d3670680e",
};

int main(int argc, char **argv)
{
    test_parse_args(argc, argv);

    /* Whole-program version gate: KECCAK-* is absent from OpenSSL 3.0.x's
     * default provider (EVP_MD_fetch fails there) and present in newer
     * releases, so the binary skips when the digest does not fetch. */
    sha3_probe_or_skip("KECCAK-512");

    sha3_test_kat(&kat);
    /* The rates are 144/136/104/72 bytes for 224/256/384/512; the KECCAK
     * rates equal the SHA3 rates at each output size. */
    sha3_test_rate_boundaries("KECCAK-512", 72);

    test_report_pass();
    return 0;
}
