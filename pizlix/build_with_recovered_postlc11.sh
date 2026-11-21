#!/bin/bash

set -e
set -x

./build_recover_postlc11.sh
./build_postlc11_continuation.sh

echo OK