#!/bin/bash
set -e
set -x
rm -rf pizlonated-unzip
tar -xf pizlonated-unzip.tar.gz
cd pizlonated-unzip
make -f unix/Makefile generic CC=cc
make -f unix/Makefile install CC=cc prefix=/usr
cd ..
rm -rf pizlonated-unzip
