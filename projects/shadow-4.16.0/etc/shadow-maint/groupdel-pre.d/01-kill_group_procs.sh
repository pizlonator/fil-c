#!/bin/sh

PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
GROUPID=`awk -F: '$1 == "'"${SUBJECT}"'" { print $3 }' /etc/group`

if [ "${GROUPID}" = "" ]; then
    exit 0
fi

for status in /proc/*/status; do
    # either this isn't a process or its already dead since expanding the list
    [ -f "$status" ] || continue

    tbuf=${status%/status}
    pid=${tbuf#/proc/}
    case "$pid" in
        "$$") continue;;
        [0-9]*) :;;
        *) continue
    esac
    
    grep -q '^Groups:.*\b'"${GROUPID}"'\b.*' "/proc/$pid/status" || continue

    kill -9 "$pid" || echo "cannot kill $pid" 1>&2
done

