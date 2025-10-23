#!/bin/sh

/usr/sbin/useradd -p '!!' tstpamaccess3
./tst-pam_access3
RET=$?
/usr/sbin/userdel -r tstpamaccess3 2> /dev/null
exit $RET
