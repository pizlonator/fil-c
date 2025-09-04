#!/bin/sh

# Test some UTF-8 locales.
${CHECKER} ./test-mbrlen-w32${EXEEXT} French_France Japanese_Japan Chinese_Taiwan Chinese_China 65001
