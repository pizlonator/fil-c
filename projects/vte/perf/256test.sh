#!/usr/bin/env bash

# Test 256 color support along with bold and dim attributes.
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

sep=':'
if [ "$1" = "-colon" -o "$1" = "-official" -o "$1" = "-dejure" ]; then
  shift
elif [ "$1" = "-semicolon" -o "$1" = "-common" -o "$1" = "-defacto" ]; then
  sep=';'
  shift
fi

if [ $# != 0 ]; then
  echo 'Usage: 256test.sh [-format]' >&2
  echo >&2
  echo '  -colon|-official|-dejure:     Official format (default)  \e[38:5:INDEXm' >&2
  echo '  -semicolon|-common|-defacto:  Commonly used format       \e[38;5;INDEXm' >&2
  exit 1
fi

format_number() {
  local c=$'\u254F'
  if [ $1 -lt 10 ]; then
    printf "$c %d" $1
  else
    printf "$c%02d" $(($1%100))
  fi
}

somecolors() {
  local from="$1"
  local to="$2"
  local prefix="$3"
  local line

  for line in \
      "\e[2mdim      " \
      "normal   " \
      "\e[1mbold     " \
      "\e[1;2mbold+dim "; do
    echo -ne "$line"
    i=$from
    while [ $i -le $to ]; do
      echo -ne "\e[$prefix${i}m"
      format_number $i
      i=$((i+1))
    done
    echo $'\e[0m\e[K'
  done
}

allcolors() {
  echo "-- 8 standard colors: SGR ${1}0..${1}7 --"
  somecolors 0 7 "$1"
  echo
  echo "-- 8 bright colors: SGR ${2}0..${2}7 --"
  somecolors 0 7 "$2"
  echo
  echo "-- 256 colors: SGR ${1}8${sep}5${sep}0..255 --"
  somecolors 0 15 "${1}8${sep}5${sep}"
  echo
  somecolors  16  51 "${1}8${sep}5${sep}"
  somecolors  52  87 "${1}8${sep}5${sep}"
  somecolors  88 123 "${1}8${sep}5${sep}"
  somecolors 124 159 "${1}8${sep}5${sep}"
  somecolors 160 195 "${1}8${sep}5${sep}"
  somecolors 196 231 "${1}8${sep}5${sep}"
  echo
  somecolors 232 255 "${1}8${sep}5${sep}"
}

allcolors 3 9
echo
allcolors 4 10
