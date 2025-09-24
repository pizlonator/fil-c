#!/bin/bash

set -e
set -x

echo Sub1: lfs user trampoline

test $EUID -ne 0

exec env -i HOME=/home/lfs TERM=$TERM PS1='\u:\w$ ' ./build_prelc_sub1_lfsuser.sh
