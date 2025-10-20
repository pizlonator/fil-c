#!/bin/sh

/usr/sbin/groupadd tstpamaccess
/usr/sbin/useradd -G tstpamaccess -p '!!' tstpamaccess1
./tst-pam_access1
RET=$?
/usr/sbin/userdel -r tstpamaccess1 2> /dev/null
/usr/sbin/groupdel tstpamaccess 2> /dev/null
exit $RET
