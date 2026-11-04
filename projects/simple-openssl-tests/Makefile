# Simple OpenSSL test programs.  See README.md.
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
# an in-tree build (e.g. projects/sarcastic-openssl, which is deliberately
# never installed; see build_sarcastic_openssl.sh) puts libcrypto.so.3 at the
# top of the tree.  Prefer lib/ when it exists, else the prefix itself.
OPENSSL_LIBDIR := $(if $(wildcard $(OPENSSL_HOME)/lib/.),$(OPENSSL_HOME)/lib,$(OPENSSL_HOME))
CPPFLAGS += -I$(OPENSSL_HOME)/include
LDFLAGS += -L$(OPENSSL_LIBDIR) -Wl,-rpath,$(OPENSSL_LIBDIR)
# A pizlonated libcrypto binds zlib's deflate/inflate etc. to the pizlonated
# zlib in pizfix (it references pizlonated_deflate and friends), so the
# pizfix library directory must be on the search path after the OpenSSL
# libdir for the in-tree flavor.
LDFLAGS += -L$(PIZFIX)/lib -Wl,-rpath,$(PIZFIX)/lib
endif

# The zegarmistrz poisoning header (used by poisoning.h).  Under filcc the
# same canonical file also comes from pizfix/stdfil-include; the explicit -I
# keeps gcc/clang/yolo builds self-contained.
CPPFLAGS += -I$(REPO_ROOT)/projects/zegarmistrz/include

# An in-tree OpenSSL flavor (e.g. projects/sarcastic-openssl) ships
# libcrypto.so.3 without the libcrypto.so development symlink, so -lcrypto
# links (and filcc's default RUNPATH finds) the pizfix libcrypto at runtime.
# After linking, reorder the RUNPATH so the flavor's directory is searched
# before pizfix.  (filcc always prepends its own pizfix RUNPATH entry, which
# would otherwise shadow the flavor's libcrypto.)
ifneq ($(OPENSSL_HOME),)
ifneq ($(abspath $(OPENSSL_HOME)),$(abspath $(PIZFIX)))
OPENSSL_FLAVOR_RPATH := $(OPENSSL_LIBDIR):$(PIZFIX)/lib
# The flavor reordering below needs patchelf; fail early and clearly rather
# than per-test in the middle of the build.
ifeq ($(shell command -v patchelf >/dev/null 2>&1 && echo yes),)
$(error patchelf is required for OPENSSL_HOME trees without lib/ (e.g. the sarcastic flavor): it reorders the test binaries' RUNPATH so they bind the flavor's libcrypto, not pizfix's)
endif
endif
endif

CFLAGS ?= -O2 -g -Wall -Wextra
LDLIBS += -lcrypto

PROGRAMS := $(patsubst %.c,%,$(wildcard test_*.c))

all: $(PROGRAMS)

%: %.c poisoning.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)
ifneq ($(OPENSSL_FLAVOR_RPATH),)
	patchelf --set-rpath '$(OPENSSL_FLAVOR_RPATH)' $@
endif

check test: all
	./run_all.sh

clean:
	rm -f $(PROGRAMS)

.PHONY: all test check clean
