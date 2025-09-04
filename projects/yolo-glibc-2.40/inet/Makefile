# Copyright (C) 1991-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

#
#	Sub-makefile for inet portion of the library.
#
subdir	:= inet

include ../Makeconfig

headers := \
  $(wildcard arpa/*.h protocols/*.h) \
  bits/in.h \
  ifaddrs.h \
  netinet/ether.h \
  netinet/icmp6.h \
  netinet/if_ether.h \
  netinet/igmp.h \
  netinet/in.h \
  netinet/in_systm.h \
  netinet/ip.h \
  netinet/ip6.h \
  netinet/tcp.h \
  rpc/netdb.h \
  # headers

routines := \
  bindresvport \
  deadline \
  ether_aton \
  ether_aton_r \
  ether_line \
  ether_ntoa \
  ether_ntoa_r \
  getipv4sourcefilter \
  getsourcefilter \
  herrno \
  herrno-loc \
  htonl \
  htons \
  idna \
  idna_name_classify \
  if_index \
  ifaddrs \
  in6_addr \
  inet6_opt \
  inet6_option \
  inet6_rth \
  inet6_scopeid_pton \
  inet_lnaof \
  inet_mkadr \
  inet_net \
  inet_netof \
  inet_ntoa \
  rcmd \
  rexec \
  ruserpass \
  setipv4sourcefilter \
  setsourcefilter \
  # routines

install-others = $(inst_sysconfdir)/rpc

aux := check_pf check_native ifreq

tests := \
  bug-if1 \
  htontest \
  test-hnto-types \
  test-ifaddrs \
  test-inet6_opt \
  test_ifindex \
  tst-checks \
  tst-checks-posix \
  tst-ether_aton \
  tst-ether_line \
  tst-getni1 \
  tst-getni2 \
  tst-if_index-long \
  tst-inet6_rth \
  tst-network \
  tst-ntoa \
  tst-sockaddr \
  # tests

# tst-deadline must be linked statically so that we can access
# internal functions.
tests-static += tst-deadline
tests-internal += tst-deadline

# tst-idna_name_classify must be linked statically because it tests
# internal functionality.
tests-static += tst-idna_name_classify
tests-internal += tst-idna_name_classify

# tst-inet6_scopeid_pton also needs internal functions but does not
# need to be linked statically.
tests-internal += tst-inet6_scopeid_pton

include ../Rules

LOCALES := en_US.UTF-8 en_US.ISO-8859-1
include ../gen-locales.mk

ifeq ($(have-thread-library),yes)

CFLAGS-rcmd.c += -fexceptions
CFLAGS-either_ntoh.c += -fexceptions
CFLAGS-either_hton.c += -fexceptions
CFLAGS-in6_addr.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-if_index.c += $(config-cflags-wno-ignored-attributes)
CFLAGS-ifaddrs.c += $(config-cflags-wno-ignored-attributes)

CFLAGS-tst-checks-posix.c += -std=c99
CFLAGS-tst-sockaddr.c += -fno-strict-aliasing

endif

# Install the rpc database file.
$(inst_sysconfdir)/rpc: etc.rpc $(+force)
	$(do-install)

ifeq ($(build-static-nss),yes)
CFLAGS += -DSTATIC_NSS
endif

# The test uses dlopen indirectly and would otherwise load system
# objects.
tst-idna_name_classify-ENV = \
  LD_LIBRARY_PATH=$(ld-library-path):$(common-objpfx):$(common-objpfx)elf
$(objpfx)tst-idna_name_classify.out: $(gen-locales)
