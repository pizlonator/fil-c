#!/bin/bash

actual=$(wget -o/dev/null -O/dev/stdout https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/include/uapi/linux/capability.h | grep "#define.CAP_LAST_CAP"|awk '{print $3}')
working=$(grep "#define.CAP_LAST_CAP" libcap/include/uapi/linux/capability.h|awk '{print $3}')

if [[ ${actual} = ${working} ]]; then
    echo "up to date with officially named caps"
    exit 0
fi

echo "want: ${actual}"
echo "have: ${working}"
exit 1
