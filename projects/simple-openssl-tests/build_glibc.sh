#!/bin/sh
# Builds the suite with a normal (unpizlonated) glibc compiler against the
# system OpenSSL 3.  Override with CC=... and/or OPENSSL_HOME=... on the
# command line.
set -e
exec make CC="${CC:-gcc}" OPENSSL_HOME="${OPENSSL_HOME:-}" "$@"
