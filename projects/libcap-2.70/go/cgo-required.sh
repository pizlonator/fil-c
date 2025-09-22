#!/bin/bash
#
# Runtime check for whether or not syscall.AllThreadsSyscall is
# available to the working go runtime or not. If it isn't we always
# have to use libcap/psx to get POSIX semantics for syscalls that
# change security state.
if [ -n "$1" ]; then
    export GO="${1}"
else
    export GO=go
fi

if [ -z "$(${GO} doc syscall 2>/dev/null|grep AllThreadsSyscall)" ]; then
    echo "1"
else
    echo "0"
fi
