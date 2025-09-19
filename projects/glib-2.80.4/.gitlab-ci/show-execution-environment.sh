#!/bin/bash

set -eux -o pipefail

id || :
capsh --print || :
env -0 | sort -z | perl -pe 's/\0/\n/g' || :
setpriv --dump || :
ulimit -a || :
cat /proc/self/status || :
cat /proc/self/mountinfo || :
stat /etc/machine-id || :
stat /var/lib/dbus/machine-id || :
