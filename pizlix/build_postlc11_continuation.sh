#!/bin/bash

set -e
set -x

./build_postlc11.sh

# The postlc11 stage (WiFi/kernel/firmware)
# flows into the postlc12 stage (the login manager: libtool's libltdl fix,
# libogg/libvorbis/libcanberra, gdm 46.2, and the boot-to-GNOME wiring) so
# the one-shot chain and the recovery chain both produce a Pizlix that boots
# into the GNOME login window.
./build_postlc12_continuation.sh
