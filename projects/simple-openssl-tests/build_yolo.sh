#!/bin/sh
# Builds this suite with a normal (unpizlonated) yolo compiler against the
# system OpenSSL 3.  Override with CC=... and/or OPENSSL_HOME=... on the
# command line.
# OPENSSL_HOME may name a separately-built unpizlonated OpenSSL; args
# go to make.
set -e
exec make CC="${CC:-gcc}" OPENSSL_HOME="${OPENSSL_HOME:-}" "$@"
