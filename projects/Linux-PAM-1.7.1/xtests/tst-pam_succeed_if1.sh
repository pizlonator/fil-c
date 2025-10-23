#!/bin/sh

/usr/sbin/useradd -p '!!' tstpamtest
/usr/sbin/useradd -p '!!' pamtest
./tst-pam_succeed_if1
RET=$?
/usr/sbin/userdel -r tstpamtest 2> /dev/null
/usr/sbin/userdel -r pamtest 2> /dev/null
exit $RET
