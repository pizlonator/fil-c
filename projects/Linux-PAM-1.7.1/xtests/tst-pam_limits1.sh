#!/bin/sh

/usr/sbin/useradd -p '!!' tstpamlimits
./tst-pam_limits1
RET=$?
/usr/sbin/userdel -r tstpamlimits 2> /dev/null
exit $RET
