#!/bin/bash

if [ $# -ne 1 ]; then
    echo "$0: usage: <BOLT-SOURCE>"
    exit 1
fi

boltsrc="$1"

function die() {
  echo $*
  exit 1
}

function copyone() {
    dst=$1
    src="$boltsrc/$dst"

    search=(common cli)
    for base in ${search[*]}
    do
	path="$boltsrc/$base/$dst"
	if [ -f $path ]; then
	    src=$path
	    break;
	fi
    done

    if [ ! -f $src ]; then
	echo -e "$dst \t[  skipped  ] $src (ENOENT)"
    elif cmp -s $src $dst; then
	echo -e "$dst \t[ unchanged ]"
    else
	cp $src $dst || die "$dst [failed] source: $src"
	echo -e "$dst \t[  updated  ] $src"
	git add $dst
    fi
}

names=(client device enums error names proxy str time)

for fn in ${names[*]}
do
    header="bolt-$fn.h"
    source="bolt-$fn.c"

    copyone $header
    copyone $source
done

