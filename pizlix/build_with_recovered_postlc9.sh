#!/bin/bash

set -e
set -x

# House pattern (cf. build_with_recovered_postlc8.sh and the recovered-
# postlc8-run.log flow from the S8 sprint): recover the postlc9 snapshot
# (wiping /mnt/lfs back to the pre-WebKit state -- lfsbuildstate=postlc9,
# which is exactly what build_postlc10.sh's driver asserts) and then run
# the NEXT stage, the postlc10 stage (the WebKitGTK + desktop-file-utils +
# gnutls-trust + libportal + epiphany chain), end-to-end over the
# recovered base.  This is the honest chain-reproduction test for the
# W2+W3 postlc10 stage.
./build_recover_postlc9.sh

# Re-seed what the recovery wipes (the postlc9 snapshot predates the
# live-check seeding): the CA trust bundle (+ /etc/ssl/certs split +
# /etc/pki/anchors) and the t800 ssh authorized_keys.  The stage snapshot
# written at the end of this run carries the pre-seed forward.
./build_preseed_pizlix.sh

./build_postlc10_continuation.sh

echo OK
