#!/bin/sh

for t in *
do
	if [ ! -d $t/data ]; then continue; fi
	for i in passwd group shadow gshadow
	do
		if [ -f $t/data/$i ]
		then
			if cmp -s $t/config/etc/$i $t/data/$i
			then
				echo "# $t/data/$i identical to config"
				svn rm "$t/data/$i"
			fi
		fi
	done
done

for t in *
do
	cd $t
	if [ ! -d data ]; then cd ..; continue; fi
	for i in data/*
	do
		if [ ! -f $i ]; then continue; fi
		if ! grep -q $i *.test
		then
			echo "# $t/$i not used"
			svn rm "$i"
		fi
	done
	cd ..
done
