#!/bin/bash
#
# Builds the following .syso files to the directory containing this script:
#
#   fib_linux_arm.syso
#   fib_linux_arm64.syso

cd ${0%/*}
GCC=arm-linux-gnueabi-gcc ./gcc.sh -O3 fib.c -c -o fib_linux_arm.syso
GCC=aarch64-linux-gnu-gcc ./gcc.sh -O3 fib.c -c -o fib_linux_arm64.syso
