#!/bin/bash

set -e
set -x

./build_lc.sh
./build_postlc_continuation.sh
