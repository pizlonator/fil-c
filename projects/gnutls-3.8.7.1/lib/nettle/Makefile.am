## Process this file with automake to produce Makefile.in
# Copyright (C) 2004-2012 Free Software Foundation, Inc.
#
# Author: Nikos Mavroyanopoulos
#
# This file is part of GNUTLS.
#
# The GNUTLS library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 3 of
# the License, or (at your option) any later version.
#
# The GNUTLS library is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>

include $(top_srcdir)/lib/common.mk

AM_CFLAGS += $(HOGWEED_CFLAGS) $(GMP_CFLAGS)

AM_CPPFLAGS = \
	-I$(srcdir)/int		\
	-I$(srcdir)/backport		\
	-I$(srcdir)/../../gl		\
	-I$(builddir)/../../gl		\
	-I$(srcdir)/../includes		\
	-I$(builddir)/../includes	\
	-I$(builddir)/../../gl          \
	-I$(srcdir)/..

if ENABLE_MINITASN1
AM_CPPFLAGS += -I$(srcdir)/../minitasn1
endif

if ENABLE_DLOPEN
AM_CPPFLAGS += $(LIBOQS_CFLAGS) -DGNUTLS_OQS_ENABLE_DLOPEN=1
endif

noinst_LTLIBRARIES = libcrypto.la

libcrypto_la_SOURCES = pk.c mpi.c mac.c cipher.c init.c \
	gnettle.h rnd-common.h prf.c \
	rnd.c int/rsa-fips.h int/rsa-keygen-fips186.c int/provable-prime.c \
	int/dsa-fips.h int/dsa-keygen-fips186.c int/dsa-validate.c \
	int/tls1-prf.c int/tls1-prf.h \
	int/dsa-compute-k.c int/dsa-compute-k.h \
	int/ecdsa-compute-k.c int/ecdsa-compute-k.h \
	int/mpn-base256.c int/mpn-base256.h \
	int/block8.h backport/block-internal.h \
	int/rsa-oaep.h int/rsa-pad.c int/nettle-internal.h int/sha3-shake.h

if WINDOWS
if HAVE_BCRYPT
libcrypto_la_SOURCES += sysrng-bcrypt.c
else
libcrypto_la_SOURCES += sysrng-windows.c
endif
else
if HAVE_GETENTROPY
libcrypto_la_SOURCES += sysrng-getentropy.c
else
if HAVE_KERN_ARND
libcrypto_la_SOURCES += sysrng-netbsd.c
else
libcrypto_la_SOURCES += sysrng-linux.c
endif
endif
endif

libcrypto_la_SOURCES += rnd-fuzzer.c

if ENABLE_FIPS140
libcrypto_la_SOURCES += rnd-fips.c int/drbg-aes-self-test.c \
	int/drbg-aes.c int/drbg-aes.h
endif

if ENABLE_GOST
libcrypto_la_SOURCES += \
	gost/nettle-write.h gost/write-le32.c \
	gost/gost28147.c gost/gost28147.h gost/gost-wrap.c \
	gost/streebog.c gost/streebog.h gost/streebog-meta.c \
	gost/hmac-streebog.c gost/hmac-gost.h

libcrypto_la_SOURCES += \
	gost/bignum-le.c gost/bignum-le.h

libcrypto_la_SOURCES += \
	gost/gostdsa-mask.c gost/gostdsa2.h

libcrypto_la_SOURCES += gost_keywrap.c

libcrypto_la_SOURCES += \
	gost/magma.c gost/magma.h \
	gost/kuznyechik.c gost/kuznyechik.h gost/kuztable.h \
	gost/acpkm.c gost/acpkm.h \
	gost/cmac.h gost/cmac-magma.c gost/cmac-kuznyechik.c
endif

if NEED_SIV_GCM
libcrypto_la_SOURCES += \
	backport/bswap-internal.h \
	backport/ctr-internal.h \
	backport/ctr.h \
	backport/ctr16.c \
	backport/ghash-internal.h \
	backport/ghash-set-key.c \
	backport/ghash-update.c \
	backport/siv-gcm-aes128.c \
	backport/siv-gcm-aes256.c \
	backport/siv-gcm.c \
	backport/siv-gcm.h \
	backport/siv-ghash-set-key.c \
	backport/siv-ghash-update.c \
	$(NULL)
endif

if NEED_RSA_OAEP
libcrypto_la_SOURCES += \
	backport/gmp-glue.c \
	backport/gmp-glue.h \
	backport/oaep.c \
	backport/oaep.h \
	backport/pss-mgf1.c \
	backport/pss-mgf1.h \
	backport/rsa-internal.h \
	backport/rsa-oaep-encrypt.c \
	backport/rsa-oaep-decrypt.c \
	backport/rsa-sec-compute-root.c \
	backport/rsa-sign-tr.c \
	$(NULL)
endif

if NEED_SHAKE_OUTPUT
libcrypto_la_SOURCES += \
	backport/md-internal.h \
	backport/nettle-write.h \
	backport/sha3.c \
	backport/sha3-internal.h \
	backport/sha3-shake.c \
	backport/shake128.c \
	backport/shake256.c \
	backport/write-le64.c \
	$(NULL)
endif
