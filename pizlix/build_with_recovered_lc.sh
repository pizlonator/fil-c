#!/bin/bash

set -e
set -x

./build_recover_lc.sh
./build_postlc_continuation.sh

echo OK
