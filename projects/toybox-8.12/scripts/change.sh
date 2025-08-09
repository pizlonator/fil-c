#!/bin/bash

# build each command as a standalone executable

source scripts/portability.sh

NOBUILD=1 scripts/make.sh > /dev/null &&
${HOSTCC:-cc} -I . scripts/install.c -o "$UNSTRIPPED"/instlist &&
export PREFIX=${PREFIX:-change/} &&
mkdir -p "$PREFIX" || exit 1

# Build all the commands standalone
for i in $("$UNSTRIPPED"/instlist)
do
  echo -n " $i" &&
  scripts/single.sh $i &>$PREFIX/${i}.bad &&
    rm $PREFIX/${i}.bad || echo -n '*'
done
echo
