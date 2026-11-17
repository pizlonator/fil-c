#!/usr/bin/env bash

# Test deco color support
# Copyright © 2014 Egmont Koblinger
# Copyright © 2018 Christian Persch
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

sep=":"
sepsep="::"
if [ "$1" = "-colon" -o "$1" = "-official" -o "$1" = "-dejure" ]; then
  shift
elif [ "$1" = "-semicolon" -o "$1" = "-common" -o "$1" = "-defacto" ]; then
    sep=";"
    sepsep=";" # no empty param for legacy format
  shift
fi

if [ $# != 0 ]; then
  echo 'Usage: deco.sh [FORMAT]' >&2
  echo >&2
  echo '  -colon|-official|-dejure:     Official format (default)  CSI 58:2::R:G:Bm' >&2
  echo '  -semicolon|-common|-defacto:  Commonly used format       CSI 58;2;R;G;Bm' >&2
  exit 1
fi

row() {
    local format="$1"
    local n="$2"
    local v;
    for m in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
	v=$(($n * 16 + $m))
	printf "\e[${format};4m%02X\e[0m%.0s%.0s%.0s" 38 $v $v $v $v $v $v
    done
    printf "\t"
    for m in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
	v=$(($n * 16 + $m))
	printf "\e[${format};4m%02X\e[0m%.0s%.0s%.0s" 48 $v $v $v $v $v $v
    done
    printf "\n"
}

cubes() {
    local format1="$1"
    local format2="$2"
    local format="%d${sep}2${sepsep}${format1};58${sep}2${sepsep}${format2}"
    for n in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
	row "$format" $n
    done
}

printf "\e[0m"
cubes "%d${sep}0${sep}0" "%d${sep}0${sep}0"
printf "\n"
cubes "0${sep}%d${sep}0" "0${sep}%d${sep}0"
printf "\n"
cubes "0${sep}0${sep}%d" "0${sep}0${sep}%d"
printf "\n"
cubes "%d${sep}0${sep}0" "0${sep}%d${sep}0"
printf "\n"
cubes "%d${sep}0${sep}0" "0${sep}0${sep}%d"
printf "\n"
cubes "0${sep}%d${sep}0" "%d${sep}0${sep}0"
printf "\n"
cubes "0${sep}%d${sep}0" "0${sep}0${sep}%d"
printf "\n"
cubes "0${sep}0${sep}%d" "%d${sep}0${sep}0"
printf "\n"
cubes "0${sep}0${sep}%d" "0${sep}%d${sep}0"
printf "\n"
printf "\e[0m"

exit 0
