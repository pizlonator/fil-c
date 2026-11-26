#!/bin/bash
set -e
set -x
rm -rf pizlonated-zip
tar -xf pizlonated-zip.tar.gz
cd pizlonated-zip
make -f unix/Makefile generic CC=gcc
make -f unix/Makefile install CC=gcc prefix=/usr
cd ..
rm -rf pizlonated-zip
