#!/usr/bin/env bash

# Image viewer for terminals that support true colors.
# Copyright (C) 2014  Egmont Koblinger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

sep1=':'
sep2='::'
if [ "$1" = "-colon4" -o "$1" = "-official" -o "$1" = "-dejure" ]; then
  shift
elif [ "$1" = "-colon3" -o "$1" = "-wrong" ]; then
  sep2=':'
  shift
elif [ "$1" = "-semicolon" -o "$1" = "-common" -o "$1" = "-defacto" ]; then
  sep1=';'
  sep2=';'
  shift
fi

if [ $# != 1 -o "${1:0:1}" = "-" ]; then
  echo 'Usage: img.sh [-format] imagefile' >&2
  echo >&2
  echo '  -colon4|-official|-dejure:    Official format (default)  \e[38:2::R:G:Bm' >&2
  echo '  -colon3|-wrong:               Misinterpreted format      \e[38:2:R:G:Bm' >&2
  echo '  -semicolon|-common|-defacto:  Commonly used format       \e[38;2;R;G;Bm' >&2
  exit 1
elif [ -z $(type -p convert) ]; then
  echo 'Please install ImageMagick to run this script.' >&2
  exit 1
fi

# This is so that "upper" is still visible after exiting the while loop.
shopt -s lastpipe

COLUMNS=$(tput cols)

declare -a upper lower
upper=()
lower=()

convert -thumbnail ${COLUMNS}x -define txt:compliance=SVG "$1" txt:- |
while IFS=',:() ' read col row dummy red green blue rest; do
  if [ "$col" = "#" ]; then
    continue
  fi

  if [ $((row%2)) = 0 ]; then
    upper[$col]="$red$sep1$green$sep1$blue"
  else
    lower[$col]="$red$sep1$green$sep1$blue"
  fi

  # After reading every second image row, print them out.
  if [ $((row%2)) = 1 -a $col = $((COLUMNS-1)) ]; then
    i=0
    while [ $i -lt $COLUMNS ]; do
      echo -ne "\\e[38${sep1}2${sep2}${upper[$i]};48${sep1}2${sep2}${lower[$i]}m▀"
      i=$((i+1))
    done
    # \e[K is useful when you resize the terminal while this script is still running.
    echo -e "\\e[0m\e[K"
    upper=()
    d=()
  fi
done

# Print the last half line, if required.
if [ "${upper[0]}" != "" ]; then
  i=0
  while [ $i -lt $COLUMNS ]; do
    echo -ne "\\e[38${sep1}2${sep2}${upper[$i]}m▀"
    i=$((i+1))
  done
  echo -e "\\e[0m\e[K"
fi
