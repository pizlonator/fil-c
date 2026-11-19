/* test_dh_common.h -- shared machinery for the finite-field DH family
 * tests (the "DH" and "DHX" provider flavors over the RFC 7919 ffdhe2048
 * well-known group).
 *
 * One binary per provider fetch name (test_dh.c for the plain "DH" flavor
 * and test_dhx.c for "DHX", the flavor that carries the subgroup order q);
 * each binary defines one struct dh_family row and calls the shared
 * fixed-key routines below with the shared KAT material from
 * test_dh_common.c.  The big explanatory comment (the fixed key material,
 * the CLI cross-check, and the observed import/derive behavior) lives in
 * test_dh_common.c.
 */

#ifndef TEST_DH_COMMON_H
#define TEST_DH_COMMON_H

#include "test_common.h"
#include <stddef.h>
#include <openssl/evp.h>

/* One row per provider flavor: the EVP_PKEY fetch name ("DH" or "DHX"),
 * whether the fixed-key import includes the subgroup order q (the DHX
 * flavor does; the plain DH flavor does not), and the tail of the KAT
 * CHECK text (what the derived secret is compared against, kept verbatim
 * from the monolith: "DHX" compared its Z against the DH KAT when both
 * flavors ran in one program). */
struct dh_family {
    const char *alg;   /* EVP_PKEY fetch name: "DH" or "DHX" */
    int use_q;         /* import OSSL_PKEY_PARAM_FFC_Q with the fixed keys */
    const char *zcmp;  /* tail of the KAT comparison CHECK text */
};

/* Shared fixed-key KAT material (the RFC 7919 A.1 ffdhe2048 prime and its
 * subgroup order, the fixed initiator/responder key pairs, the
 * CLI-cross-checked shared secret, and the ffdhe3072 wrong-size peer;
 * provenance in test_dh_common.c).  Declared as arrays, not pointers, so
 * that they are address constants usable in static initializers and can be
 * handed to from_hex()/from_hex_le() directly. */

extern const char dh_ffdhe2048_p_hex[];  /* RFC 7919 A.1, ffdhe2048 (256 bytes) */
extern const char dh_ffdhe2048_q_hex[];  /* q = (p-1)/2 */
extern const char dh_x_hex[];            /* fixed initiator private key (32 bytes) */
extern const char dh_y_hex[];            /* fixed initiator public key y = g^x mod p */
extern const char dh_x2_hex[];           /* fixed responder private key (32 bytes) */
extern const char dh_y2_hex[];           /* fixed responder public key y' = g^x' mod p */
extern const char dh_z_hex[];            /* Z = g^(x*x') mod p, CLI-cross-checked */
extern const char dh_ffdhe3072_p_hex[];  /* RFC 7919 A.2 (wrong-size peer only) */
extern const char dh_big_peer_y_hex[];   /* a public value under the 3072-bit prime */

/* Imports a DH/DHX key pair from p, g = 2 [, q if qhex_or_null != NULL],
 * priv and pub through EVP_PKEY_fromdata.  Returns the EVP_PKEY (which
 * references the labeled working buffers). */
EVP_PKEY *dh_make_key(const struct dh_family *fam, const char *phex,
                      const char *qhex_or_null, const char *xhex,
                      const char *yhex, size_t plen);

/* derive(our_key, peer) into `secret`; returns 1 and stores the length on
 * success, 0 if the exchange was rejected (EVP_PKEY_derive_set_peer or
 * EVP_PKEY_derive failed). */
int dh_derive(EVP_PKEY *key, EVP_PKEY *peer, unsigned char *secret,
              size_t cap, size_t *secretlen);

/* (1) The fixed-key KAT for fam: both fixed keys are imported under
 * fam->alg (with q iff fam->use_q) and the derived 256-byte secret must
 * equal the shared CLI-cross-checked Z. */
void dh_test_kat(const struct dh_family *fam);

/* (2) Peers that must be rejected, for fam's own flavor: a wrong-size
 * (ffdhe3072) peer built under fam->alg, and a same-group peer whose
 * public value is zero. */
void dh_test_bad_peers(const struct dh_family *fam);

#endif /* TEST_DH_COMMON_H */
