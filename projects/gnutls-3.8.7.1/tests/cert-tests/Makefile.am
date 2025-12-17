## Process this file with automake to produce Makefile.in
# Copyright (C) 2007-2008, 2010, 2012 Free Software Foundation, Inc.
#
# Author: Simon Josefsson
#
# This file is part of GnuTLS.
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>

EXTRA_DIST = data/ca-no-pathlen.pem data/no-ca-or-pathlen.pem data/aki-cert.pem \
	data/template-test.key data/template-test.pem templates/template-test.tmpl \
	data/funny-spacing.pem data/ca-certs.pem data/dane-test.rr data/cert-ecc256.pem \
	data/bmpstring.pem data/template-utf8.pem templates/template-utf8.tmpl \
	templates/template-dn.tmpl data/template-dn.pem data/complex-cert.pem \
	data/template-overflow.pem templates/template-overflow.tmpl data/template-overflow2.pem \
	templates/template-overflow2.tmpl data/template-crq.pem data/cert-ecc256-full.pem \
	templates/template-date.tmpl data/template-date.pem templates/template-dn-err.tmpl \
	templates/template-nc.tmpl data/template-nc.pem data/xmpp-othername.pem \
	suppressions.valgrind data/csr-invalid.der data/invalid-sig2.pem data/invalid-sig3.pem \
	data/invalid-sig.pem email-certs/chain.exclude.test.example.com email-certs/chain.test.example.com \
	email-certs/chain.invalid.example.com email-certs/chain.test.example.com-2 \
	data/single-ca.p7b data/single-ca.p7b.out data/full.p7b data/full.p7b.out data/detached.p7b \
	data/pkcs7-detached.txt data/p7-combined.out data/template-generalized.pem \
	templates/template-generalized.tmpl data/privkey1.pem data/privkey2.pem data/privkey3.pem \
	data/name-constraints-ip.pem data/cert-invalid-utf8.der data/very-long-dn.pem \
	data/provable3072.pem data/provable2048.pem data/provable-dsa2048.pem \
	data/provable-dsa2048-fips.pem templates/template-crq.tmpl data/invalid-sig5.pem \
	templates/template-unique.tmpl data/template-unique.pem data/invalid-sig4.pem \
	templates/template-othername.tmpl data/template-othername.pem \
	templates/template-othername-xmpp.tmpl data/template-othername-xmpp.pem \
	templates/template-krb5name.tmpl data/crl-demo1.pem data/crl-demo2.pem data/crl-demo3.pem \
	data/template-krb5name.pem data/template-krb5name-full.pem data/template-test-ecc.key \
	data/template-rsa-sha3-256.pem data/template-rsa-sha3-512.pem data/template-rsa-sha3-224.pem \
	data/template-rsa-sha3-384.pem data/long-oids.pem \
	data/name-constraints-ip2.pem data/chain-md5.pem data/pubkey-ecc256.pem \
	templates/template-dates-after2038.tmpl data/template-dates-after2038.pem \
	data/gost-cert.pem data/gost-cert-nogost.pem data/gost94-cert.pem \
	templates/template-tlsfeature.tmpl data/cert-with-crl.p12 \
	data/template-tlsfeature.pem data/template-tlsfeature.csr \
	templates/template-tlsfeature-crq.tmpl templates/arb-extensions.tmpl data/arb-extensions.pem \
	data/arb-extensions.csr data/pkcs1-pad-ok.pem data/pkcs1-pad-broken.pem \
	data/pkcs1-pad-ok2.pem data/pkcs1-pad-broken2.pem data/pkcs1-pad-broken3.pem \
	data/client.p12 data/noclient.p12 data/unclient.p12 data/pkcs12_2certs.p12 \
	data/pkcs12_5certs.p12 data/test-null.p12 data/cert-ca.p12 data/sha256.p12 \
	data/key-ca.pem data/key-subca.pem data/key-subsubca.pem data/key-user.pem \
	data/key-dsa.pem data/key-ca-dsa.pem data/key-subca-dsa.pem \
	data/ca-public.gpg data/srv-public-all-signed.gpg data/srv-secret.gpg \
	data/ca-secret.gpg data/srv-public.gpg data/srv-public-127.0.0.1-signed.gpg	\
	data/srv-public-localhost-signed.gpg data/selfsigs/alice-mallory-badsig18.pub \
	data/selfsigs/alice-mallory-irrelevantsig.pub data/selfsigs/alice-mallory-nosig18.pub \
	data/selfsigs/alice.pub data/key-utf8-1.p12 data/key-utf8-2.p12 \
	data/code-signing-ca.pem data/code-signing-cert.pem data/multi-value-dn.pem \
	data/pkcs7-cat-ca.pem data/pkcs7-cat.p7 data/openssl.p7b data/openssl.p7b.out \
	data/openssl-keyid.p7b data/openssl-keyid.p7b.out data/openssl.p12 \
	data/x509-v1-with-sid.pem data/x509-v1-with-iid.pem data/x509-v3-with-fractional-time.pem \
	templates/template-long-dns.tmpl templates/template-long-serial.tmpl \
	data/key-rsa-pss-raw.pem data/key-rsa-pss.pem data/cve-2019-3829.pem \
	data/long-dns.pem data/template-long-dns-crq.pem data/chain-with-critical-on-root.pem \
	data/chain-with-critical-on-intermediate.pem data/chain-with-critical-on-endcert.pem \
	templates/crit-extensions.tmpl data/crit-extensions.pem data/x509-with-zero-version.pem \
	data/key-corpus-rc2-1.p12 data/key-corpus-rc2-2.p12 data/key-corpus-rc2-3.p12 \
	data/key-corpus-rc2-1.p12.out data/no-salt.p12 data/mac-sha512.p12 data/pbes1-no-salt.p12 \
	templates/inhibit-anypolicy.tmpl data/inhibit-anypolicy.pem data/aes-128.p12 \
	data/pkcs7.smime data/invalid-date-hour.der data/invalid-date-mins.der \
	data/invalid-date-secs.der data/invalid-date-month.der data/invalid-date-day.der \
	data/mem-leak.p12 data/alt-chain-new-ca.pem data/alt-chain-old-ca.pem \
	data/alt-chain.pem data/pkcs7-chain.pem data/pkcs7-chain-root.pem data/chain-eddsa.pem \
	data/pkcs7-chain-endcert-key.pem data/cert-rsa-pss.pem data/openssl-invalid-time-format.pem \
	data/cert-eddsa.pem data/pubkey-eddsa.pem data/pkcs7-eddsa-sig.p7s \
	data/key-ca.pem data/key-user.pem data/template-sgenerate.pem \
	data/ca-gnutls-keyid.pem data/ca-no-keyid.pem data/ca-weird-keyid.pem \
	data/key-ca-1234.p8 data/key-ca-empty.p8 data/key-ca-null.p8 \
	data/openssl-key-ecc.p8 data/key-ecc.p8 data/key-ecc.pem suppressions.valgrind \
	data/encpkcs8.pem data/unencpkcs8.pem data/enc2pkcs8.pem data/dup-exts.pem \
	data/openssl-3des.p8 data/openssl-3des.p8.txt data/openssl-aes128.p8 \
	data/openssl-aes128.p8.txt data/openssl-aes256.p8 data/openssl-aes256.p8.txt \
	data/cert.dsa.1024.pem data/cert.dsa.2048.pem data/cert.dsa.3072.pem \
	data/dsa.1024.pem data/dsa.2048.pem data/dsa.3072.pem data/dsa-pubkey-1018.pem \
	data/bad-key.pem data/p8key-illegal.pem data/key-illegal.pem data/pkcs8-pbes2-sha256.pem \
	data/pkcs8-pbes1-des-md5.pem data/pkcs8-invalid8.der data/key-invalid1.der \
	data/key-invalid4.der data/key-invalid5.der data/key-invalid6.der \
	data data/pkcs8-invalid9.der data/key-invalid2.der data/pkcs8-invalid10.der \
	data/key-invalid3.der data/pkcs8-eddsa.pem data/pkcs8-eddsa.pem.txt \
	data/rfc4490.p7b data/rfc4490.p7b.out data/gost01.p12 data/gost12.p12 data/gost12-2.p12 \
	data/ca-crl-invalid.crl data/ca-crl-invalid.pem data/ca-crl-valid.pem data/ca-crl-valid.crl \
	data/rfc4134-ca-rsa.pem data/rfc4134-4.5.p7b templates/template-no-ca.tmpl \
	data/key-gost01.p8 data/key-gost01-2.p8 data/key-gost01-2-enc.p8 data/crq-cert-no-ca.pem \
	data/key-gost12-256.p8 data/key-gost12-256-2.p8 data/key-gost12-256-2-enc.p8 \
	data/key-gost12-512.p8 data/grfc.crt data/gost-cert-ca.pem data/gost-cert-new.pem \
	data/cert-with-non-digits-time-ca.pem data/cert-with-non-digits-time.pem \
	data/chain-512-leaf.pem data/chain-512-subca.pem data/chain-512-ca.pem \
	templates/template-no-ca-honor.tmpl templates/template-no-ca-explicit.tmpl \
	data/crq-cert-no-ca-explicit.pem data/crq-cert-no-ca-honor.pem data/commonName.cer \
	data/pbmac1_256_256.bad-iter.p12 data/pbmac1_256_256.bad-salt.p12 \
	data/pbmac1_256_256.good.p12 data/pbmac1_256_256.no-len.p12 \
	data/pbmac1_256_256.short-len.p12 \
	data/pbmac1_256_256.extended-mac.p12 \
	data/pbmac1_256_256.truncated-mac.p12 \
	data/pbmac1_512_256.good.p12 data/pbmac1_512_512.good.p12 \
	data/pbmac1-simple.p12 \
	templates/simple-policy.tmpl data/simple-policy.pem templates/template-negative-serial.tmpl \
	templates/template-encryption-only.tmpl

dist_check_SCRIPTS = pathlen.sh aki.sh invalid-sig.sh email.sh \
	pkcs7.sh pkcs7-broken-sigs.sh privkey-import.sh name-constraints.sh certtool-long-cn.sh crl.sh provable-privkey.sh \
	provable-dh.sh sha2-test.sh sha2-dsa-test.sh provable-privkey-dsa2048.sh \
	provable-privkey-rsa2048.sh provable-privkey-gen-default.sh pkcs7-constraints.sh \
	pkcs7-constraints2.sh certtool-long-oids.sh pkcs7-cat.sh cert-sanity.sh cert-critical.sh \
	pkcs12.sh certtool-crl-decoding.sh pkcs12-encode.sh pkcs12-corner-cases.sh pkcs12-pbmac1.sh inhibit-anypolicy.sh \
	smime.sh cert-time.sh alt-chain.sh pkcs7-list-sign.sh pkcs7-eddsa.sh certtool-ecdsa.sh \
	key-id.sh pkcs8.sh pkcs8-decode.sh ecdsa.sh illegal-rsa.sh pkcs8-invalid.sh key-invalid.sh \
	pkcs8-eddsa.sh certtool-subca.sh certtool-verify-profiles.sh x509-duplicate-ext.sh x25519-and-x448.sh \
	reject-negative-serial.sh

dist_check_SCRIPTS += key-id.sh ecdsa.sh pkcs8-invalid.sh key-invalid.sh pkcs8-decode.sh pkcs8.sh pkcs8-eddsa.sh \
	certtool-utf8.sh crq.sh

if STRICT_DER_TIME
dist_check_SCRIPTS += cert-non-digits-time.sh reject-invalid-time.sh
else
dist_check_SCRIPTS += tolerate-invalid-time.sh
endif

if WANT_TEST_SUITE
dist_check_SCRIPTS += provable-dh-default.sh
endif

if !WINDOWS
dist_check_SCRIPTS += template-test.sh pem-decoding.sh othername-test.sh krb5-test.sh sha3-test.sh md5-test.sh \
	tlsfeature-test.sh template-exts-test.sh pkcs1-pad.sh pkcs12-utf8.sh rsa-pss-pad.sh dsa.sh certtool.sh \
	template-policy-test.sh
endif

if ENABLE_DANE
dist_check_SCRIPTS += dane.sh
endif

if ENABLE_GOST
dist_check_SCRIPTS += gost.sh
if !WINDOWS
dist_check_SCRIPTS += pkcs12-gost.sh pkcs8-gost.sh
endif
endif

dist_check_SCRIPTS += certtool-rsa-pss.sh certtool-eddsa.sh certtool-rsa-oaep.sh

TESTS = $(dist_check_SCRIPTS)

# Set detect_leaks=0 to ASAN. It seems it is detecting many leaks in tools
# which are not trivial, and makes no point to address.
TESTS_ENVIRONMENT = EXEEXT=$(EXEEXT) \
	LC_ALL="C"				\
	VALGRIND='$(LOG_VALGRIND)'		\
	LIBTOOL="$(LIBTOOL)"			\
	top_builddir="$(top_builddir)"		\
	abs_top_builddir="$(abs_top_builddir)"	\
	ac_cv_sizeof_time_t="$(ac_cv_sizeof_time_t)" \
	ASAN_OPTIONS="detect_leaks=0:exitcode=6" \
	GNUTLS_TEST_SUITE_RUN=1			\
	GNUTLS_SYSTEM_PRIORITY_FILE=$(abs_top_srcdir)/tests/system.prio \
	PKCS12_ITER_COUNT="$(PKCS12_ITER_COUNT)" \
	srcdir="$(srcdir)"

if ENABLE_FIPS140
TESTS_ENVIRONMENT += FIPS140=1
endif

if WINDOWS
TESTS_ENVIRONMENT += WINDOWS=1
endif

if ENABLE_GOST
TESTS_ENVIRONMENT += ENABLE_GOST=1
else
TESTS_ENVIRONMENT += ENABLE_GOST=0
endif

if DISABLE_BASH_TESTS
TESTS_ENVIRONMENT += DISABLE_BASH_TESTS=1
endif

TESTS_ENVIRONMENT +=					\
	FAKETIME="$(FAKETIME)"				\
	FAKETIME_F_OPT="$(FAKETIME_F_OPT)"		\
	ac_cv_faketime_works="$(ac_cv_faketime_works)"

TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = $(SHELL)

AM_VALGRINDFLAGS = --suppressions=$(srcdir)/suppressions.valgrind
LOG_COMPILER = $(LOG_VALGRIND)

distclean-local:
	rm -rf tmp-* *.tmp
