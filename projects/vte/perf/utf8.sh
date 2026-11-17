#!/bin/sh

cnt=$1
[ -n "$cnt" ] || cnt=6000

i=0 
while [ $i -lt $cnt ]
do
 	cat UTF-8-demo.txt
 	i=$(( $i + 1 ))
done
