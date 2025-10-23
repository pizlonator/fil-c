#!/bin/sh

# pamunix0 = 0aXKZztA.d1KY
/usr/sbin/useradd -p 0aXKZztA.d1KY  tstpamunix
./tst-pam_unix2
RET=$?
/usr/sbin/userdel -r tstpamunix 2> /dev/null
exit $RET
