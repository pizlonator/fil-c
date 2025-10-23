#!/bin/sh

/usr/sbin/groupadd tstpamaccess
/usr/sbin/useradd -p '!!' tstpamaccess2
./tst-pam_access2
RET=$?
/usr/sbin/userdel -r tstpamaccess2 2> /dev/null
/usr/sbin/groupdel tstpamaccess 2> /dev/null
exit $RET
