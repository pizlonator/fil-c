#!/bin/bash

set -e
set -x

./build_recover_postlc10.sh
./build_postlc10_continuation.sh

echo OK
