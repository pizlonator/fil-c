#!/bin/sh
# adduser script for use with shadow passwords and useradd command.
# by Hrvoje Dogan <hdogan@student.math.hr>, Dec 1995.

echo -n "Login name for new user []:"
read LOGIN
if [ -z $LOGIN ]
then echo "Come on, man, you can't leave the login field empty...";exit
fi
echo
echo -n "User id for $LOGIN [ defaults to next available]:"
read ID
GUID="-u $ID"
if [ -z $ID ] 
then GUID=""
fi

echo
echo -n "Initial group for $LOGIN [users]:"
read GID
GGID="-g $GID"
if [ -z $GID ]
then GGID=""
fi

echo
echo -n "Additional groups for $LOGIN []:"
read AGID
GAGID="-G $AGID"
if [ -z $AGID ]
then GAGID=""
fi

echo
echo -n "$LOGIN's home directory [/home/$LOGIN]:"
read HME
GHME="-d $HME"
if [ -z $HME ]
then GHME=""
fi

echo
echo -n "$LOGIN's shell [/bin/bash]:"
read SHL
GSHL="-s $SHL"
if [ -z $SHL ]
then GSHL=""
fi

echo
echo -n "$LOGIN's account expiry date (MM/DD/YY) []:"
read EXP
GEXP="-e $EXP"
if [ -z $EXP ]
then GEXP=""
fi
echo
echo OK, I'm about to make a new account. Here's what you entered so far:
echo New login name: $LOGIN
if [ -z $GUID ] 
then echo New UID: [Next available]
else echo New UID: $UID
fi
if [ -z $GGID ]
then echo Initial group: users
else echo Initial group: $GID
fi
if [ -z $GAGID ]
then echo Additional groups: [none]
else echo Additional groups: $AGID
fi
if [ -z $GHME ]
then echo Home directory: /home/$LOGIN
else echo Home directory: $HME
fi
if [ -z $GSHL ]
then echo Shell: /bin/bash
else echo Shell: $SHL
fi
if [ -z $GEXP ]
then echo Expiry date: [no expiration]
else echo Expiry date: $EXP
fi
echo "This is it... if you want to bail out, you'd better do it now."
read FOO
echo Making new account...
/usr/sbin/useradd $GHME -m $GEXP $GGID $GAGID $GSHL $GUID $LOGIN
/usr/bin/chfn $LOGIN
/usr/bin/passwd $LOGIN
echo "Done..."
