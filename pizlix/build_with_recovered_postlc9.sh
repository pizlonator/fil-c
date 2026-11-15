#!/bin/bash

set -e
set -x

./build_recover_postlc9.sh
./build_postlc9_continuation.sh

echo OK
