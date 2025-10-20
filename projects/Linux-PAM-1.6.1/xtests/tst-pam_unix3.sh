#!/bin/sh

# pamunix01 = 0aXKZztA.d1KYIuFXArmd2jU
/usr/sbin/useradd -p 0aXKZztA.d1KYIuFXArmd2jU tstpamunix
./tst-pam_unix3
RET=$?
/usr/sbin/userdel -r tstpamunix 2> /dev/null
exit $RET
