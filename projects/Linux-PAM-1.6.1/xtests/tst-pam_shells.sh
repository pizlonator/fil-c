#!/bin/sh

/usr/sbin/groupadd tstpamshells1
/usr/sbin/useradd -s /bin/testbash -G tstpamshells1 -p '!!' tstpamshells
/usr/sbin/useradd -s /bin/testnoshell -G tstpamshells1 -p '!!' tstnoshell
./tst-pam_shells
RET=$?
/usr/sbin/userdel -r tstpamshells 2> /dev/null
/usr/sbin/userdel -r tstnoshell 2> /dev/null
/usr/sbin/groupdel tstpamshells1 2> /dev/null
exit $RET
