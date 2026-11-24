#!/bin/bash

set -e
set -x

./build_recover_postlc9.sh

# Re-seed what the recovery wipes (the postlc9 snapshot predates the
# live-check seeding): the CA trust bundle (+ /etc/ssl/certs split +
# /etc/pki/anchors).  The stage snapshot
# written at the end of this run carries the pre-seed forward.
./build_preseed_pizlix.sh

./build_postlc10_continuation.sh

echo OK
