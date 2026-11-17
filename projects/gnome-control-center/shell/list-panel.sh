#!/bin/sh

LIST=""
for i in $1/panels/*/gnome-*panel.desktop.in.in $1/panels/*/data/gnome-*panel.desktop.in.in; do
	basename=`basename $i`
	LIST="$LIST `echo $basename | sed 's/gnome-//' | sed 's/-panel.desktop.in.in/ /'`"
done
echo -n $LIST | tr " " "\n" | sort | tr "\n" " "
