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
CPPFLAGS += -I$(OPENSSL_HOME)/include
LDFLAGS += -L$(OPENSSL_HOME)/lib -Wl,-rpath,$(OPENSSL_HOME)/lib
endif

CFLAGS ?= -O2 -g -Wall -Wextra
LDLIBS += -lcrypto

PROGRAMS := $(patsubst %.c,%,$(wildcard test_*.c))

all: $(PROGRAMS)

%: %.c poisoning.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

check test: all
	./run_all.sh

clean:
	rm -f $(PROGRAMS)

.PHONY: all test check clean
