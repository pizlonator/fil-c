#!/bin/bash

set -e
set -x

./build_recover.sh postlc12
./build_postlc12_continuation.sh
