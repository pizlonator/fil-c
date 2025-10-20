#!/bin/sh

/usr/sbin/useradd tstpampwhistory
./tst-pam_pwhistory1
RET=$?
/usr/sbin/userdel -r tstpampwhistory 2> /dev/null
exit $RET
