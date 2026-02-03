#!/bin/sh

set -e

make -C test clean
make -k -j2 -C test sanity

