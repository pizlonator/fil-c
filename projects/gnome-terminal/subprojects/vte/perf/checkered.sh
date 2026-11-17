#!/bin/bash

color1='255:224:255'
color2='248:255:255'

y=1
while IFS=$'\n' read -r line; do
  x=0
  while [ -n "$line" -a "x${line:0:1}" != $'x\e' ]; do
    char="${line:0:1}"
    line="${line:1}"
    x=$((x+1))
    if [ $(( (x+y) % 2 )) = 1 ]; then
      echo -ne "\e[48:2::${color1}m"
    else
      echo -ne "\e[48:2::${color2}m"
    fi
    printf %s "$char"
  done
  # If an escape character is found, stop painting the pattern for the rest of the line, and dump the remaining part.
  echo -ne '\e[m'
  echo "$line"
  y=$((y+1))
done < "$1"
