#!/bin/bash

set -e
set -x

./build_recover_postlc.sh
./build_postlc2_continuation.sh

echo OK
