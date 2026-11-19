/* test_digest_shake_common.h -- shared machinery for the SHAKE128/256 XOF
 * and KECCAK-KMAC-128/256 digest tests.
 *
 * One binary per digest (test_digest_shake128.c, test_digest_shake256.c,
 * test_digest_keccak_kmac_128.c and test_digest_keccak_kmac_256.c); each
 * binary defines its own KAT row (struct shake_kat) and then calls the
 * per-digest test routines below.  The big explanatory comment (the two
 * output-length routes, the prefix property, the squeeze state machine, and
 * the version gates) lives in test_digest_shake_common.c.
 */

#ifndef TEST_DIGEST_SHAKE_COMMON_H
#define TEST_DIGEST_SHAKE_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One KAT row: the digest name (the EVP_MD_fetch name), the message (NULL
 * means the empty message), the requested output length, and the expected
 * output. */
struct shake_kat {
    const char *mdname;
    const char *msg;        /* NULL means the empty message */
    size_t xoflen;
    const char *outhex;
};

/* Whole-program version gate for the KECCAK-KMAC-* binaries: the original
 * monolith printed the documented SKIP line ("%s is unavailable") when a
 * KMAC digest did not fetch; each split binary's subject IS its KMAC
 * digest, so that gate is whole-program here.  SKIP with the documented
 * reason when the digest does not fetch, release the probe handle
 * otherwise. */
void shake_probe_or_skip(const char *mdname);

/* (1) One KAT row through both output-length routes: OSSL_DIGEST_PARAM_XOFLEN
 * ("xoflen") set through EVP_MD_CTX_set_params BEFORE the final, and
 * EVP_DigestFinalXOF; the two routes must agree byte for byte. */
void shake_test_kat(const struct shake_kat *kat);

/* (4) One KECCAK-KMAC-128/256 row: like shake_test_kat, plus the
 * EVP_MD_get_size == xoflen check (EVP_MD_get_size is meaningful for the
 * KMAC digests, where it is unused for SHAKE). */
void shake_test_kmac_kat(const struct shake_kat *kat);

/* (2) The prefix property for the digest named mdname: squeezing more bytes
 * extends, never changes, a shorter squeeze of the same deterministic
 * 137-byte input -- the first short_len bytes of a 200-byte squeeze must be
 * the short_len-byte squeeze. */
void shake_test_prefix_property(const char *mdname, size_t short_len);

/* (3) EVP_DigestSqueeze (OpenSSL 3.5+): squeezing 10 bytes and then 100
 * bytes must equal a single 110-byte squeeze of the same input.  Compiled
 * in only on 3.5+ headers; on older builds the binary prints the documented
 * mid-run SKIP line instead of calling this (and instead of the pass
 * line). */
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
void shake_test_squeeze(const char *mdname);
#endif

#endif /* TEST_DIGEST_SHAKE_COMMON_H */
