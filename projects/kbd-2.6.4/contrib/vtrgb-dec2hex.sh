#!/usr/bin/bash
# convert setvtrgb decimal file to hexadecimal

if [[ -z "$1" ]]; then
	echo "usage: vtrgb-dec2hex.sh <decfile_path>"
	exit
fi

declare -i x=0
declare -i y=0
declare -A d

mapfile decimals < $1

for line in ${decimals[@]}; do
	x=0
	IFS=,
	for dec in $line; do
		d[$y,$x]=$dec
		x+=1
	done
	y+=1
done

for x in {0..15}; do
	printf "#%02x%02x%02x\n" ${d[0,$x]} ${d[1,$x]} ${d[2,$x]}
done
