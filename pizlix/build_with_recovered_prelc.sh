#!/bin/bash

set -e
set -x

./build_recover_prelc.sh
./build_lc_continuation.sh

echo OK
