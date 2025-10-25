# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

# Makefile for stub pam_ecryptfs.so
#
# This builds a minimal PAM module that provides pam_ecryptfs.so compatibility
# without requiring Mozilla NSS or actual ecryptfs support.
#
# Usage:
#   make                          # Build pam_ecryptfs.so
#   make install                  # Install to PREFIX/lib/security
#   make clean                    # Remove build artifacts
#   make CC=/opt/fil/bin/filcc    # Build with Fil-C compiler
#
# Variables:
#   CC       - C compiler (default: cc)
#   PREFIX   - Installation prefix (default: /usr/local)
#   DESTDIR  - Staging directory for packaging (default: empty)

CC       ?= cc
PREFIX   ?= /usr/local
DESTDIR  ?=

CFLAGS   := -fPIC -Wall -Wextra -O2 -Wno-unused-parameter
LDFLAGS  := -shared
LIBS     := -lpam

TARGET   := pam_ecryptfs.so
SOURCE   := stub_pam_ecryptfs.c
INSTALL_DIR := $(DESTDIR)$(PREFIX)/lib/security

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

install: $(TARGET)
	install -d $(INSTALL_DIR)
	install -m 0755 $(TARGET) $(INSTALL_DIR)/$(TARGET)

clean:
	rm -f $(TARGET)

# For development: show what would be built
info:
	@echo "CC         = $(CC)"
	@echo "PREFIX     = $(PREFIX)"
	@echo "DESTDIR    = $(DESTDIR)"
	@echo "INSTALL_DIR= $(INSTALL_DIR)"
	@echo "TARGET     = $(TARGET)"
