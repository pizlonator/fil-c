## Process this file with automake to produce Makefile.in
# Copyright (C) 2000-2012 Free Software Foundation, Inc.
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
# You should have received a copy of the GNU General Public License
# along with this file.  If not, see <https://www.gnu.org/licenses/>.

OPTIONS_BUILT = \
	srptool-options.c srptool-options.h \
	psktool-options.c psktool-options.h \
	ocsptool-options.h ocsptool-options.c \
	gnutls-serv-options.c gnutls-serv-options.h \
	gnutls-cli-options.c gnutls-cli-options.h \
	gnutls-cli-debug-options.c gnutls-cli-debug-options.h \
	certtool-options.c certtool-options.h \
	danetool-options.c danetool-options.h \
	p11tool-options.c p11tool-options.h \
	tpmtool-options.c tpmtool-options.h \
	systemkey-tool-options.c systemkey-tool-options.h

OPTIONS_JSON = \
	certtool-options.json gnutls-cli-debug-options.json gnutls-cli-options.json tpmtool-options.json \
	systemkey-tool-options.json srptool-options.json ocsptool-options.json p11tool-options.json \
	danetool-options.json gnutls-serv-options.json psktool-options.json

OPTIONS_STAMP = \
	certtool-options.stamp gnutls-cli-debug-options.stamp gnutls-cli-options.stamp tpmtool-options.stamp \
	systemkey-tool-options.stamp srptool-options.stamp ocsptool-options.stamp p11tool-options.stamp \
	danetool-options.stamp gnutls-serv-options.stamp psktool-options.stamp

BUILT_SOURCES = $(OPTIONS_BUILT)

EXTRA_DIST = gen-mech-list.sh $(OPTIONS_JSON) $(OPTIONS_BUILT) $(OPTIONS_STAMP)

MAINTAINERCLEANFILES = $(OPTIONS_BUILT) $(OPTIONS_STAMP)

AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)
AM_CPPFLAGS = \
	-I$(srcdir)/gl				\
	-I$(builddir)/gl			\
	-I$(builddir)/../lib/includes		\
	-I$(srcdir)/../lib/includes		\
	-I$(srcdir)/../libdane/includes		\
	-I$(srcdir)/../extra/includes

bin_PROGRAMS = psktool gnutls-cli-debug certtool gnutls-serv gnutls-cli
if ENABLE_SRP
bin_PROGRAMS += srptool
endif

if ENABLE_OCSP
bin_PROGRAMS += ocsptool
endif

if ENABLE_DANE
bin_PROGRAMS += danetool
endif

if ENABLE_PKCS11

# TPM makes use of PKCS #11 callbacks
if ENABLE_TROUSERS
bin_PROGRAMS += tpmtool
endif

bin_PROGRAMS += p11tool
endif

noinst_LTLIBRARIES =
noinst_PROGRAMS =

if ENABLE_SRP
srptool_SOURCES = srptool.c
srptool_LDADD = ../lib/libgnutls.la libcmd-srp.la ../gl/libgnu.la 
srptool_LDADD += $(LTLIBINTL) gl/libgnu_gpl.la
noinst_LTLIBRARIES += libcmd-srp.la
libcmd_srp_la_SOURCES = srptool-options.c srptool-options.h
endif

psktool_SOURCES = psk.c
psktool_LDADD = ../lib/libgnutls.la libcmd-psk.la ../gl/libgnu.la 
psktool_LDADD += $(LTLIBINTL) gl/libgnu_gpl.la
noinst_LTLIBRARIES += libcmd-psk.la
libcmd_psk_la_SOURCES = psktool-options.c psktool-options.h


if ENABLE_OCSP

ocsptool_SOURCES = ocsptool.c ocsptool-common.h ocsptool-common.c certtool-common.c \
	socket.c common.c common.h socket.h
ocsptool_LDADD = ../lib/libgnutls.la libcmd-ocsp.la ../gl/libgnu.la 
ocsptool_LDADD += $(LTLIBINTL) $(GETADDRINFO_LIB) gl/libgnu_gpl.la $(LIBIDN_LIBS)
noinst_LTLIBRARIES += libcmd-ocsp.la
libcmd_ocsp_la_SOURCES = ocsptool-options.h ocsptool-options.c

endif

gnutls_serv_SOURCES =		\
  serv.c			\
  udp-serv.c udp-serv.h		\
  common.h common.c		\
  certtool-common.h 
gnutls_serv_LDADD = ../lib/libgnutls.la
gnutls_serv_LDADD += libcmd-serv.la ../gl/libgnu.la
gnutls_serv_LDADD += $(LTLIBINTL) gl/libgnu_gpl.la
gnutls_serv_LDADD += $(LIBSOCKET) $(GETADDRINFO_LIB) $(LIBIDN_LIBS)
noinst_LTLIBRARIES += libcmd-serv.la
libcmd_serv_la_SOURCES = gnutls-serv-options.c gnutls-serv-options.h

BENCHMARK_SRCS = benchmark-cipher.c benchmark.c benchmark.h benchmark-tls.c

gnutls_cli_SOURCES = cli.c common.h common.c \
	socket.c socket.h inline_cmds.h \
	$(BENCHMARK_SRCS)
if ENABLE_OCSP
gnutls_cli_SOURCES += ocsptool-common.c
endif
gnutls_cli_LDADD = ../lib/libgnutls.la -lm
if ENABLE_DANE
gnutls_cli_LDADD += ../libdane/libgnutls-dane.la
endif
gnutls_cli_LDADD += libcmd-cli.la ../gl/libgnu.la $(LTLIBINTL) $(LIBIDN_LIBS)
gnutls_cli_LDADD += $(LIBSOCKET) $(GETADDRINFO_LIB) $(LIB_CLOCK_GETTIME) \
	 $(SERVENT_LIB) gl/libgnu_gpl.la
noinst_LTLIBRARIES += libcmd-cli.la
libcmd_cli_la_SOURCES = gnutls-cli-options.c gnutls-cli-options.h

gnutls_cli_debug_SOURCES = cli-debug.c tests.h tests.c \
		socket.c socket.h common.h common.c
gnutls_cli_debug_LDADD = ../lib/libgnutls.la libcmd-cli-debug.la 
gnutls_cli_debug_LDADD += ../gl/libgnu.la gl/libgnu_gpl.la
gnutls_cli_debug_LDADD += $(LTLIBINTL) $(LIBIDN_LIBS)
gnutls_cli_debug_LDADD += $(LIBSOCKET) $(GETADDRINFO_LIB)
noinst_LTLIBRARIES += libcmd-cli-debug.la
libcmd_cli_debug_la_SOURCES = gnutls-cli-debug-options.c gnutls-cli-debug-options.h

#certtool
COMMON_LIBS = $(LTLIBINTL)
if ENABLE_MINITASN1
COMMON_LIBS += ../lib/minitasn1/libminitasn1.la ../gl/libgnu.la 
AM_CPPFLAGS += -I$(top_srcdir)/lib/minitasn1
else
COMMON_LIBS += $(LIBTASN1_LIBS)
AM_CPPFLAGS += $(LIBTASN1_CFLAGS)
endif


certtool_SOURCES = certtool.c certtool-common.c certtool-extras.c common.c
certtool_LDADD = ../lib/libgnutls.la 
certtool_LDADD += libcmd-certtool.la ../gl/libgnu.la gl/libgnu_gpl.la

noinst_LTLIBRARIES += libcmd-certtool.la
libcmd_certtool_la_SOURCES = certtool-options.c certtool-options.h
libcmd_certtool_la_LIBADD = libcerttool-cfg.la ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_certtool_la_LIBADD += $(COMMON_LIBS)
libcmd_certtool_la_LIBADD += $(LTLIBREADLINE) gl/libgnu_gpl.la
libcmd_certtool_la_LIBADD += $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)


danetool_SOURCES = danetool.c certtool-common.c certtool-extras.c common.c socket.c
danetool_LDADD = ../lib/libgnutls.la  $(LIBIDN_LIBS)
danetool_LDADD += libcmd-danetool.la ../gl/libgnu.la gl/libgnu_gpl.la
if ENABLE_DANE
danetool_LDADD += ../libdane/libgnutls-dane.la
endif

noinst_LTLIBRARIES += libcmd-danetool.la
libcmd_danetool_la_SOURCES = danetool-options.c danetool-options.h
libcmd_danetool_la_LIBADD = libcerttool-cfg.la ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_danetool_la_LIBADD += $(COMMON_LIBS)
libcmd_danetool_la_LIBADD += $(LTLIBREADLINE)
libcmd_danetool_la_LIBADD += $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)

# p11 tool
if ENABLE_PKCS11

BUILT_SOURCES += mech-list.h
p11tool_SOURCES = p11tool-options.json p11tool.c pkcs11.c certtool-common.c \
	certtool-extras.c p11tool.h common.c mech-list.h
p11tool_CPPFLAGS = $(AM_CPPFLAGS) $(P11_KIT_CFLAGS)
p11tool_LDADD = ../lib/libgnutls.la
p11tool_LDADD += libcmd-p11tool.la ../gl/libgnu.la gl/libgnu_gpl.la
p11tool_LDADD += $(COMMON_LIBS)

noinst_LTLIBRARIES += libcmd-p11tool.la
libcmd_p11tool_la_SOURCES = p11tool-options.c p11tool-options.h
libcmd_p11tool_la_LIBADD = libcerttool-cfg.la ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_p11tool_la_LIBADD += $(LTLIBREADLINE) $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)

endif # ENABLE_PKCS11

if ENABLE_TROUSERS

tpmtool_SOURCES = tpmtool-options.json tpmtool.c certtool-common.c certtool-extras.c common.c
tpmtool_LDADD = ../lib/libgnutls.la
tpmtool_LDADD += libcmd-tpmtool.la ../gl/libgnu.la gl/libgnu_gpl.la
tpmtool_LDADD += $(COMMON_LIBS)

noinst_LTLIBRARIES += libcmd-tpmtool.la
libcmd_tpmtool_la_SOURCES = tpmtool-options.c tpmtool-options.h
libcmd_tpmtool_la_LIBADD = libcerttool-cfg.la ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_tpmtool_la_LIBADD += $(LTLIBREADLINE) $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)

endif # ENABLE_TROUSERS

noinst_PROGRAMS += systemkey
systemkey_SOURCES = systemkey.c certtool-common.c common.c
systemkey_LDADD = ../lib/libgnutls.la
systemkey_LDADD += libcmd-systemkey.la ../gl/libgnu.la gl/libgnu_gpl.la
systemkey_LDADD += $(COMMON_LIBS)

noinst_LTLIBRARIES += libcmd-systemkey.la
libcmd_systemkey_la_SOURCES = systemkey-tool-options.c systemkey-tool-options.h
libcmd_systemkey_la_LIBADD = libcerttool-cfg.la ../lib/libgnutls.la gl/libgnu_gpl.la ../gl/libgnu.la 
libcmd_systemkey_la_LIBADD += $(LTLIBREADLINE) $(INET_PTON_LIB) $(LIB_CLOCK_GETTIME)

noinst_LTLIBRARIES += libcerttool-cfg.la
libcerttool_cfg_la_SOURCES = certtool-cfg.h certtool-cfg.c cfg.c cfg.h
libcerttool_cfg_la_LIBADD = ../gl/libgnu.la gl/libgnu_gpl.la

noinst_PROGRAMS += dumpcfg
dumpcfg_SOURCES = cfg.c cfg.h
dumpcfg_CFLAGS = -DTEST=1
dumpcfg_LDADD = ../gl/libgnu.la gl/libgnu_gpl.la

SUFFIXES = .stamp .json

OPTIONS_STAMP: $(top_srcdir)/cligen/cli-codegen.py

CLIGEN_ARGS = \
	--bug-email bugs@gnutls.org \
	--copyright-year 2000-2023 \
	--copyright-holder 'Free Software Foundation, and others' \
	--license gpl3+ \
	--package '$(PACKAGE)' \
	--version '$(VERSION)'

.json.stamp:
	$(AM_V_GEN) { \
		b=`echo $@ | sed 's/.stamp$$//'`; \
		PYTHONPATH='$(top_srcdir)/cligen' \
		$(PYTHON) $(top_srcdir)/cligen/cli-codegen.py $(CLIGEN_ARGS) \
			$< $${b}.c $${b}.h; \
		echo "#define optionProcess(a,b,c) process_options(b,c)" >> $${b}.h; \
		echo "#define STACKCT_OPT OPTS_COUNT" >> $${b}.h; \
		echo "#define STACKLST_OPT OPTS_ARRAY" >> $${b}.h; \
	} && \
	touch $@

danetool-options.stamp: danetool-options.json
ocsptool-options.stamp: ocsptool-options.json
tpmtool-options.stamp: tpmtool-options.json
p11tool-options.stamp: p11tool-options.json
psktool-options.stamp: psktool-options.json
gnutls-cli-debug-options.stamp: gnutls-cli-debug-options.json
gnutls-cli-options.stamp: gnutls-cli-options.json
gnutls-serv-options.stamp: gnutls-serv-options.json
srptool-options.stamp: srptool-options.json
certtool-options.stamp: certtool-options.json
systemkey-tool-options.stamp: systemkey-tool-options.json

danetool-options.c danetool-options.h: danetool-options.stamp
ocsptool-options.c ocsptool-options.h: ocsptool-options.stamp
tpmtool-options.c tpmtool-options.h: tpmtool-options.stamp
p11tool-options.c p11tool-options.h: p11tool-options.stamp
psktool-options.c psktool-options.h: psktool-options.stamp
gnutls-cli-debug-options.c gnutls-cli-debug-options.h: gnutls-cli-debug-options.stamp
gnutls-cli-options.c gnutls-cli-options.h: gnutls-cli-options.stamp
gnutls-serv-options.c gnutls-serv-options.h: gnutls-serv-options.stamp
srptool-options.c srptool-options.h: srptool-options.stamp
certtool-options.c certtool-options.h: certtool-options.stamp
systemkey-tool-options.c systemkey-tool-options.h: systemkey-tool-options.stamp

mech-list.h: gen-mech-list.sh
	$(AM_V_GEN) $(srcdir)/gen-mech-list.sh > $@.tmp && mv $@.tmp $@
