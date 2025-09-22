#!/bin/sh

# Copyright (C) 2004-2022 Free Software Foundation, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

FILES="$@"
FILES=${FILES:-$THREADSAFETY_FILES}
EGREP=${EGREP:-egrep}

if test -z "$FILES"; then
    echo "Usage: $0 [FILE...]"
    exit 1
fi

UNSAFE=""

# Thread unsafe functions according to latest POSIX, see:
# http://www.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_09_01
UNSAFE="$UNSAFE asctime"
UNSAFE="$UNSAFE basename"
UNSAFE="$UNSAFE catgets"
UNSAFE="$UNSAFE crypt"
UNSAFE="$UNSAFE ctime"
UNSAFE="$UNSAFE dbm_clearerr"
UNSAFE="$UNSAFE dbm_close"
UNSAFE="$UNSAFE dbm_delete"
UNSAFE="$UNSAFE dbm_error"
UNSAFE="$UNSAFE dbm_fetch"
UNSAFE="$UNSAFE dbm_firstkey"
UNSAFE="$UNSAFE dbm_nextkey"
UNSAFE="$UNSAFE dbm_open"
UNSAFE="$UNSAFE dbm_store"
UNSAFE="$UNSAFE dirname"
UNSAFE="$UNSAFE dlerror"
UNSAFE="$UNSAFE drand48"
UNSAFE="$UNSAFE encrypt"
UNSAFE="$UNSAFE endgrent"
UNSAFE="$UNSAFE endpwent"
UNSAFE="$UNSAFE endutxent"
UNSAFE="$UNSAFE ftw"
UNSAFE="$UNSAFE getc_unlocked"
UNSAFE="$UNSAFE getchar_unlocked"
UNSAFE="$UNSAFE getdate"
UNSAFE="$UNSAFE getenv"
UNSAFE="$UNSAFE getgrent"
UNSAFE="$UNSAFE getgrgid"
UNSAFE="$UNSAFE getgrnam"
UNSAFE="$UNSAFE gethostent"
UNSAFE="$UNSAFE getlogin"
UNSAFE="$UNSAFE getnetbyaddr"
UNSAFE="$UNSAFE getnetbyname"
UNSAFE="$UNSAFE getnetent"
UNSAFE="$UNSAFE getopt"
UNSAFE="$UNSAFE getprotobyname"
UNSAFE="$UNSAFE getprotobynumber"
UNSAFE="$UNSAFE getprotoent"
UNSAFE="$UNSAFE getpwent"
UNSAFE="$UNSAFE getpwnam"
UNSAFE="$UNSAFE getpwuid"
UNSAFE="$UNSAFE getservbyname"
UNSAFE="$UNSAFE getservbyport"
UNSAFE="$UNSAFE getservent"
UNSAFE="$UNSAFE getutxent"
UNSAFE="$UNSAFE getutxid"
UNSAFE="$UNSAFE getutxline"
UNSAFE="$UNSAFE gmtime"
UNSAFE="$UNSAFE hcreate"
UNSAFE="$UNSAFE hdestroy"
UNSAFE="$UNSAFE hsearch"
UNSAFE="$UNSAFE inet_ntoa"
UNSAFE="$UNSAFE l64a"
UNSAFE="$UNSAFE lgamma"
UNSAFE="$UNSAFE lgammaf"
UNSAFE="$UNSAFE lgammal"
UNSAFE="$UNSAFE localeconv"
UNSAFE="$UNSAFE localtime"
UNSAFE="$UNSAFE lrand48"
UNSAFE="$UNSAFE mrand48"
UNSAFE="$UNSAFE nftw"
UNSAFE="$UNSAFE nl_langinfo"
UNSAFE="$UNSAFE ptsname"
UNSAFE="$UNSAFE putc_unlocked"
UNSAFE="$UNSAFE putchar_unlocked"
UNSAFE="$UNSAFE putenv"
UNSAFE="$UNSAFE pututxline"
UNSAFE="$UNSAFE rand"
UNSAFE="$UNSAFE readdir"
UNSAFE="$UNSAFE setenv"
UNSAFE="$UNSAFE setgrent"
UNSAFE="$UNSAFE setkey"
UNSAFE="$UNSAFE setpwent"
UNSAFE="$UNSAFE setutxent"
UNSAFE="$UNSAFE strerror"
UNSAFE="$UNSAFE strsignal"
UNSAFE="$UNSAFE strtok"
UNSAFE="$UNSAFE system"
UNSAFE="$UNSAFE ttyname"
UNSAFE="$UNSAFE unsetenv"
UNSAFE="$UNSAFE wcstombs"
UNSAFE="$UNSAFE wctomb"

# Additional functions that aren't thread-safe if passed a NULL argument
UNSAFE="$UNSAFE ctermid"
UNSAFE="$UNSAFE tmpname"
UNSAFE="$UNSAFE wcrtomb"
UNSAFE="$UNSAFE wcsrtombs"

# Legacy functions removed between issue 6 and 7, see:
# http://www.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_09.html#tag_02_09_01
UNSAFE="$UNSAFE ecvt"
UNSAFE="$UNSAFE fcvt"
UNSAFE="$UNSAFE gcvt"
UNSAFE="$UNSAFE gethostbyaddr"
UNSAFE="$UNSAFE gethostbyname"

# Other (potentially) thread unsafe functions, see:
# http://blog.josefsson.org/2009/06/23/thread-safe-functions/
UNSAFE="$UNSAFE erand48"
UNSAFE="$UNSAFE ether_aton"
UNSAFE="$UNSAFE ether_ntoa"
UNSAFE="$UNSAFE fgetgrent"
UNSAFE="$UNSAFE fgetpwent"
UNSAFE="$UNSAFE fgetspent"
UNSAFE="$UNSAFE getaliasbyname"
UNSAFE="$UNSAFE getaliasent"
UNSAFE="$UNSAFE gethostbyname2"
UNSAFE="$UNSAFE getmntent"
UNSAFE="$UNSAFE getnetgrent"
UNSAFE="$UNSAFE getrpcbyname"
UNSAFE="$UNSAFE getrpcbynumber"
UNSAFE="$UNSAFE getrpcent"
UNSAFE="$UNSAFE getspent"
UNSAFE="$UNSAFE getspnam"
UNSAFE="$UNSAFE getutent"
UNSAFE="$UNSAFE getutid"
UNSAFE="$UNSAFE getutline"
UNSAFE="$UNSAFE initstate"
UNSAFE="$UNSAFE jrand48"
UNSAFE="$UNSAFE lcong48"
UNSAFE="$UNSAFE nrand48"
UNSAFE="$UNSAFE qecvt"
UNSAFE="$UNSAFE qfcvt"
UNSAFE="$UNSAFE random"
UNSAFE="$UNSAFE seed48"
UNSAFE="$UNSAFE setstate"
UNSAFE="$UNSAFE sgetspent"
UNSAFE="$UNSAFE srand48"
UNSAFE="$UNSAFE srandom"

set -- $UNSAFE
cmd="-e [^.>=_0-9a-z]($1"
shift
while test $# != 0; do
    cmd="${cmd}|$1"
    shift
done
cmd="${cmd})[^.>=_0-9a-z]*\("

if $EGREP $cmd $FILES; then
    exit 1
fi

exit 0
