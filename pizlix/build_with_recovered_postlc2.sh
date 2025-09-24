#!/bin/bash

set -e
set -x

./build_recover_postlc2.sh
./build_postlc3_continuation.sh

echo OK
