#!/bin/bash

set -e
set -x

./build_postlc10.sh

# The postlc10 stage flows into the
# postlc11 stage (kernel HWSIM + firmware) so the one-shot chain and the
# recovery chain both produce a WiFi-capable Pizlix.
./build_postlc11_continuation.sh
