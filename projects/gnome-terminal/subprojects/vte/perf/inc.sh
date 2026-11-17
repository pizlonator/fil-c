#!/usr/bin/env bash

cnt=$1
[ -n "$cnt" ] || cnt=500000

x=0
while [ $x -lt $cnt ]; do
	echo $x
	x=$(($x + 1))
done
