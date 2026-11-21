# Simple OpenSSL test programs.  See README.md.
#
# Layout: one binary per test_*.c file, one cipher (or one narrow feature)
# per binary.  test_*_common.c files are shared family modules -- they carry
# no main(), are compiled like everything else, and are linked only into the
# binaries of their family (the prerequisite-only family rules below).  The
# generic link rule wires poisoning.o (the labeled-allocation/poisoning
# machinery, see poisoning.h) and test_common.o (the shared boilerplate:
# ALLOCATE_*/CHECK/expect_equal, failure/skip reporting, main() plumbing,
# see test_common.h) into every binary.  Header dependencies are tracked
# per object: each compile writes a .d file (-MMD -MP, included at the
# bottom of this Makefile), so touching a header rebuilds exactly the
# objects that include it.
#
# Choose the compiler (and, if needed, the OpenSSL prefix) on the make command
# line:
#
#   make CC=gcc                                  # system gcc + system OpenSSL
#   make CC=clang                                # system clang + system OpenSSL
#   make CC=<repo>/build/bin/filcc               # Fil-C + pizfix OpenSSL 3
#   make CC=gcc OPENSSL_HOME=/some/openssl/prefix
#
# Convenience wrappers: ./build_yolo.sh (normal compiler), ./build_filc.sh.

ifeq ($(origin CC),default)
CC := cc
endif

REPO_ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST))))/../..)
PIZFIX := $(REPO_ROOT)/pizfix

# Default OpenSSL prefix: pizfix for Fil-C compilers (their binaries must link
# the pizlonated libcrypto), nothing (i.e. the system OpenSSL) for others.
ifneq ($(findstring fil,$(notdir $(CC))),)
DEFAULT_OPENSSL_HOME := $(PIZFIX)
endif
OPENSSL_HOME ?= $(DEFAULT_OPENSSL_HOME)

ifneq ($(OPENSSL_HOME),)
# OpenSSL's layout varies: an installed prefix keeps libraries in lib/, but
# an in-tree build (e.g. projects/sarcastic-openssl3, which is deliberately
# never installed; see build_sarcastic_openssl3.sh) puts libcrypto.so.3 at the
# top of the tree.  Prefer lib/ when it exists, else the prefix itself.
OPENSSL_LIBDIR := $(if $(wildcard $(OPENSSL_HOME)/lib/.),$(OPENSSL_HOME)/lib,$(OPENSSL_HOME))
CPPFLAGS += -I$(OPENSSL_HOME)/include
LDFLAGS += -L$(OPENSSL_LIBDIR) -Wl,-rpath,$(OPENSSL_LIBDIR)
ifneq ($(PIZFIX),)
# A pizlonated libcrypto binds zlib's deflate/inflate etc. to the pizlonated
# zlib in pizfix (it references pizlonated_deflate and friends), so the
# pizfix library directory must be on the search path after the OpenSSL
# libdir for the in-tree flavor.
LDFLAGS += -L$(PIZFIX)/lib -Wl,-rpath,$(PIZFIX)/lib
endif
endif

# The zegarmistrz poisoning header (used by poisoning.h).  Under filcc the
# same canonical file also comes from pizfix/stdfil-include; the explicit -I
# keeps gcc/clang/yolo builds self-contained.
CPPFLAGS += -I$(REPO_ROOT)/projects/zegarmistrz/include

# Per-object automatic header dependencies: each compile writes a .d file
# recording the exact headers its object includes (-MP also emits phony
# targets for the headers, so deleting one doesn't break the build).  The
# .d files are included at the bottom of this Makefile, so editing a family
# header rebuilds exactly the objects that include it -- and nothing else.
# The compile rule also names poisoning.h as an explicit prerequisite, belt
# and braces: the .d files record it too, but the explicit entry keeps
# rebuilds correct even if the .d files are ever deleted.
CPPFLAGS += -MMD -MP

# An in-tree OpenSSL flavor (e.g. projects/sarcastic-openssl3) ships
# libcrypto.so.3 without the libcrypto.so development symlink, so -lcrypto
# links (and filcc's default RUNPATH finds) the pizfix libcrypto at runtime.
# After linking, reorder the RUNPATH so the flavor's directory is searched
# before pizfix.  (filcc always prepends its own pizfix RUNPATH entry, which
# would otherwise shadow the flavor's libcrypto.)
ifneq ($(OPENSSL_HOME),)
ifneq ($(PIZFIX),)
ifneq ($(abspath $(OPENSSL_HOME)),$(abspath $(PIZFIX)))
OPENSSL_FLAVOR_RPATH := $(OPENSSL_LIBDIR):$(PIZFIX)/lib
# The flavor reordering below needs patchelf; fail early and clearly rather
# than per-test in the middle of the build.
ifeq ($(shell command -v patchelf >/dev/null 2>&1 && echo yes),)
$(error patchelf is required for OPENSSL_HOME trees without lib/ (e.g. the sarcastic flavor): it reorders the test binaries' RUNPATH so they bind the flavor's libcrypto, not pizfix's)
endif
endif
endif
endif

CFLAGS ?= -O2 -g -Wall -Wextra
LDLIBS += -lcrypto

# One binary per test_*.c, except the *_common.c family modules: they are
# link-only objects (no main()), pulled in through the family rules below.
PROGRAMS := $(patsubst %.c,%,$(filter-out %_common.c,$(wildcard test_*.c)))

all: $(PROGRAMS)

# Compile every translation unit -- including the shared poisoning.c,
# test_common.c, and the *_common.c family modules -- to an object file.
# Header prerequisites come from the auto-generated .d files (see the
# -MMD -MP CPPFLAGS above and the -include at the bottom); poisoning.h is
# additionally named explicitly, belt and braces (see the comment above).
%.o: %.c poisoning.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Generic link rule: every program links its own object plus the shared
# poisoning.o and test_common.o.  The family rules below add each family's
# common objects to the prerequisites, which $^ then picks up.
$(PROGRAMS): %: %.o poisoning.o test_common.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
ifneq ($(OPENSSL_FLAVOR_RPATH),)
	patchelf --set-rpath '$(OPENSSL_FLAVOR_RPATH)' $@
endif

# Family common modules: prerequisite-only rules; the generic link rule
# above supplies the recipe, these lines add each family's common objects.
# Every line below is live: its binaries and its *_common.c source all
# exist (e.g. test_rc4_common.c backs test_rc4, test_rc4_40, and
# test_rc4_large; the last lines, the mac and kdf families, split the
# former monoliths into per-instantiation binaries backed by new modules).
# There are no dormant lines and no empty placeholder modules left over
# from the monolith era.

test_aes_128_cbc test_aes_192_cbc test_aes_256_cbc: test_aes_cbc_common.o
test_aes_128_ecb test_aes_192_ecb test_aes_256_ecb: test_aes_ecb_common.o
test_aes_128_ctr test_aes_192_ctr test_aes_256_ctr: test_aes_ctr_common.o
test_aes_128_ofb test_aes_192_ofb test_aes_256_ofb: test_aes_ofb_common.o
test_aes_128_cfb test_aes_192_cfb test_aes_256_cfb: test_aes_cfb_common.o
test_aes_128_cfb8 test_aes_192_cfb8 test_aes_256_cfb8: test_aes_cfb8_common.o
test_aes_128_cfb1 test_aes_192_cfb1 test_aes_256_cfb1: test_aes_cfb1_common.o
test_aes_128_gcm_small test_aes_192_gcm_small test_aes_256_gcm_small test_aes_128_gcm_large test_aes_192_gcm_large test_aes_256_gcm_large: test_aes_gcm_common.o
test_aes_128_gcm_siv test_aes_192_gcm_siv test_aes_256_gcm_siv: test_aes_gcm_siv_common.o
test_aes_128_ccm_small test_aes_192_ccm_small test_aes_256_ccm_small test_aes_128_ccm_large test_aes_192_ccm_large: test_aes_ccm_common.o
test_aes_128_ocb_small test_aes_192_ocb_small test_aes_256_ocb_small test_aes_192_ocb_large test_aes_256_ocb_large: test_aes_ocb_common.o
test_aes_128_siv test_aes_192_siv test_aes_256_siv: test_aes_siv_common.o
test_aes_128_wrap test_aes_192_wrap test_aes_256_wrap test_aes_128_wrap_inv test_aes_192_wrap_inv test_aes_256_wrap_inv: test_aes_wrap_common.o
test_aes_128_wrap_pad test_aes_192_wrap_pad test_aes_256_wrap_pad test_aes_128_wrap_pad_inv test_aes_192_wrap_pad_inv test_aes_256_wrap_pad_inv: test_aes_wrap_pad_common.o
test_aes_128_cbc_cts test_aes_192_cbc_cts test_aes_256_cbc_cts test_camellia_128_cbc_cts test_camellia_192_cbc_cts test_camellia_256_cbc_cts: test_cbc_cts_common.o
test_aes_128_cbc_hmac_sha1 test_aes_256_cbc_hmac_sha1 test_aes_128_cbc_hmac_sha256 test_aes_256_cbc_hmac_sha256 test_aes_128_cbc_hmac_sha256_etm test_aes_192_cbc_hmac_sha256_etm test_aes_256_cbc_hmac_sha256_etm: test_cbc_hmac_common.o
test_aes_128_xts test_aes_256_xts test_aes_256_xts_large: test_aes_xts_common.o
test_camellia_128_ecb test_camellia_192_ecb test_camellia_256_ecb test_camellia_128_cbc test_camellia_192_cbc test_camellia_256_cbc test_camellia_128_ctr test_camellia_192_ctr test_camellia_256_ctr test_camellia_128_ofb test_camellia_192_ofb test_camellia_256_ofb test_camellia_128_cfb test_camellia_192_cfb test_camellia_256_cfb test_camellia_128_cfb8 test_camellia_192_cfb8 test_camellia_256_cfb8 test_camellia_128_cfb1 test_camellia_192_cfb1 test_camellia_256_cfb1: test_camellia_common.o
test_aria_128_ecb test_aria_192_ecb test_aria_256_ecb test_aria_128_cbc test_aria_192_cbc test_aria_256_cbc test_aria_128_ctr test_aria_192_ctr test_aria_256_ctr test_aria_128_ofb test_aria_192_ofb test_aria_256_ofb test_aria_128_cfb test_aria_192_cfb test_aria_256_cfb test_aria_128_cfb8 test_aria_192_cfb8 test_aria_256_cfb8 test_aria_128_cfb1 test_aria_192_cfb1 test_aria_256_cfb1 test_aria_128_gcm test_aria_192_gcm test_aria_256_gcm test_aria_128_ccm test_aria_192_ccm test_aria_256_ccm: test_aria_common.o
test_sm4_ecb test_sm4_cbc test_sm4_ctr test_sm4_ofb test_sm4_cfb test_sm4_gcm test_sm4_ccm test_sm4_xts: test_sm4_common.o
test_des_ecb test_des_cbc test_des_ofb test_des_cfb test_des_cfb8 test_des_cfb1 test_desx_cbc test_des_ede test_des_ede_cbc test_des_ede_cfb test_des_ede_ofb: test_des_common.o
test_des_ede3 test_des_ede3_cbc test_des_ede3_ofb test_des_ede3_cfb test_des_ede3_cfb8 test_des_ede3_cfb1: test_des3_common.o
test_rc4 test_rc4_40 test_rc4_large: test_rc4_common.o
test_cast5_ecb test_cast5_cbc test_cast5_ofb test_cast5_cfb test_bf_ecb test_bf_cbc test_bf_ofb test_bf_cfb test_idea_ecb test_idea_cbc test_idea_ofb test_idea_cfb test_seed_ecb test_seed_cbc test_seed_ofb test_seed_cfb test_rc2_ecb test_rc2_cbc test_rc2_40_cbc test_rc2_64_cbc test_rc2_cfb test_rc2_ofb: test_legacy_block_common.o
test_null_cipher test_null_digest: test_null_common.o
test_digest_sha2_224 test_digest_sha2_256 test_digest_sha2_384 test_digest_sha2_512 test_digest_sha2_512_224 test_digest_sha2_512_256 test_digest_sha2_256_192: test_digest_sha2_common.o
test_digest_sha3_224 test_digest_sha3_256 test_digest_sha3_384 test_digest_sha3_512 test_digest_keccak_224 test_digest_keccak_256 test_digest_keccak_384 test_digest_keccak_512: test_digest_sha3_common.o
test_digest_shake128 test_digest_shake256 test_digest_keccak_kmac_128 test_digest_keccak_kmac_256: test_digest_shake_common.o
test_digest_md5 test_digest_md4 test_digest_md5_sha1: test_digest_md5_common.o
test_digest_ripemd160 test_digest_sm3: test_digest_ripemd_sm3_common.o
test_digest_mdc2 test_digest_whirlpool: test_digest_mdc2_whirlpool_common.o
test_digest_blake2b test_digest_blake2s: test_digest_blake2_common.o
test_mac_kmac128 test_mac_kmac256: test_mac_kmac_common.o
test_mac_blake2b test_mac_blake2s: test_mac_blake2_common.o
test_kdf_argon2d test_kdf_argon2i test_kdf_argon2id: test_kdf_argon2_common.o
test_dh test_dhx: test_dh_common.o
test_x25519 test_x448: test_x25519_x448_common.o
test_ed25519 test_ed448: test_ed25519_ed448_common.o
test_ml_kem_512 test_ml_kem_768 test_ml_kem_1024: test_ml_kem_common.o
test_ml_dsa_44 test_ml_dsa_65 test_ml_dsa_87: test_ml_dsa_common.o
test_slh_dsa_sha2_128f test_slh_dsa_shake_128f test_slh_dsa_sha2_192f: test_slh_dsa_common.o
test_rand_ctr_drbg test_rand_hash_drbg test_rand_hmac_drbg test_rand_test_rand: test_rand_drbg_common.o

test_mac_cmac_aes_128_cbc test_mac_cmac_aes_192_cbc test_mac_cmac_aes_256_cbc test_mac_cmac_des_ede3_cbc: test_mac_cmac_common.o
test_mac_hmac_md5 test_mac_hmac_sha1 test_mac_hmac_sha256 test_mac_hmac_sha384 test_mac_hmac_sha512: test_mac_hmac_common.o
test_kdf_hkdf_sha1 test_kdf_hkdf_sha256 test_kdf_hkdf_sha384 test_kdf_hkdf_sha512: test_kdf_hkdf_common.o
test_kdf_kbkdf_sha1 test_kdf_kbkdf_sha256 test_kdf_kbkdf_sha512: test_kdf_kbkdf_common.o
test_kdf_sskdf_sha1 test_kdf_sskdf_sha256: test_kdf_sskdf_common.o
test_kdf_sshkdf_sha1 test_kdf_sshkdf_sha256: test_kdf_sshkdf_common.o
test_kdf_pbkdf2_sha1 test_kdf_pbkdf2_sha256: test_kdf_pbkdf2_common.o
test_kdf_pkcs12kdf_sha1 test_kdf_pkcs12kdf_sha256: test_kdf_pkcs12kdf_common.o
test_kdf_tls1prf_md5_sha1 test_kdf_tls1prf_sha256: test_kdf_tls1prf_common.o
test_kdf_krb5kdf_aes_128_cbc test_kdf_krb5kdf_aes_256_cbc: test_kdf_krb5kdf_common.o

# Auto-generated header dependencies: each compile above (-MMD -MP) writes
# a .d file next to its object; include the ones that exist so make knows
# which objects a header edit must rebuild.
-include $(wildcard *.d)

check test: all
	./run_all.sh

clean:
	rm -f $(PROGRAMS) *.o *.d

.PHONY: all test check clean
