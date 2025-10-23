#!/bin/sh

/usr/sbin/useradd -p '!!' tstpamaccess4
./tst-pam_access4
RET=$?
/usr/sbin/userdel -r tstpamaccess4 2> /dev/null
exit $RET
