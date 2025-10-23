#!/bin/sh

/usr/sbin/useradd -p '!!' tstpamunix
./tst-pam_unix1
RET=$?
/usr/sbin/userdel -r tstpamunix 2> /dev/null
exit $RET
