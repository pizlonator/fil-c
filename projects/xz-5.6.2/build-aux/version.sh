#!/bin/sh
# SPDX-License-Identifier: 0BSD

#############################################################################
#
# Get the version string from version.h and print it out without
# trailing newline. This makes it suitable for use in configure.ac.
#
#############################################################################
#
# Author: Lasse Collin
#
#############################################################################

sed -n 's/LZMA_VERSION_STABILITY_ALPHA/alpha/
	s/LZMA_VERSION_STABILITY_BETA/beta/
	s/LZMA_VERSION_STABILITY_STABLE//
	s/^#define LZMA_VERSION_[MPS][AIT][AJNT][A-Z]* //p' \
	src/liblzma/api/lzma/version.h \
	| tr '\n' '|' \
	| sed 's/|/./; s/|/./; s/|//g' \
	| tr -d '\r\n'
