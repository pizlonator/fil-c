#!/bin/sh

# pamunix01 = 0aXKZztA.d1KYIuFXArmd2jU
/usr/sbin/useradd -p 0aXKZztA.d1KYIuFXArmd2jU tstpamunix
# this run must successfully change the password
./tst-pam_unix4 pass
RET=$?
/usr/sbin/usermod -p 0aXKZztA.d1KYIuFXArmd2jU tstpamunix
/usr/bin/chage -m 10000 tstpamunix
# this run must fail to change the password
./tst-pam_unix4 fail || RET=$?

/usr/sbin/userdel -r tstpamunix 2> /dev/null
exit $RET
