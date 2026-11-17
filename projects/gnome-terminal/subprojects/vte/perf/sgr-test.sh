#!/usr/bin/env bash

# Tester for various SGR attributes.
# Copyright (C) 2017  Egmont Koblinger
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

words=(           Efg          Ijk                  Pqr                    Xyz)
words_comment=(normal "green text" "magenta background" "bright red underline")
words_start=(      ""           32                   45                 58:5:9)
words_end=(        ""           39                   49                     59)

attr_name=(bold dim italic underline underline "double underline" "double underline" "curly underline" "dotted underline" "dashed underline" blink reverse hidden strikethrough overline)
attr_start=(  1   2      3         4       4:1                 21                4:2               4:3                4:4                4:5     5       7      8             9       53)
attr_end=(   22  22     23        24       4:0                 24                4:0               4:0                4:0                4:0    25      27     28            29       55)

col_name=("normal")
col_start=("")
col_end=("")

row_name=("normal")
row_start=("")
row_end=("")

# Construct the cell's contents
cell=""
cell_width=0
for (( i = 0; i < ${#words[@]}; i++ )); do
  if [ $i != 0 ]; then
    cell="$cell "
    cell_width=$((cell_width+1))
  fi
  if [ -n "${words_start[i]}" ]; then
    cell="$cell"$'\e['"${words_start[i]}m"
  fi
  cell="$cell${words[i]}"
  cell_width=$((cell_width+${#words[i]}))
  if [ -n "${words_end[i]}" ]; then
    cell="$cell"$'\e['"${words_end[i]}m"
  fi
done

# Fill up the row_* arrays based on attr_*.
col1_width=6
for (( i = 0; i < ${#attr_name[@]}; i++ )); do
  rn="${attr_name[i]} (${attr_start[i]}/${attr_end[i]})"
  if [ ${#rn} -gt $col1_width ]; then
    col1_width=${#rn}
  fi
  row_name+=("$rn")
  row_start+=($'\e['"${attr_start[i]}m")
  row_end+=($'\e['"${attr_end[i]}m")
done

# Fill up the col_* arrays based on the cmdline parameters
for arg; do
  name=""
  start=""
  end=""
  IFS="+" read -r -a codes <<< "$arg"
  for code in "${codes[@]}"; do
    for (( i = 0; i < ${#attr_name[@]}; i++ )); do
      if [ "$code" = "${attr_name[i]}" -o "$code" = "${attr_start[i]}" ]; then
        if [ -n "$name" ]; then
          name="$name + "
        fi
        name="$name${attr_name[i]} (${attr_start[i]}/${attr_end[i]})"
        start="$start"$'\e['"${attr_start[i]}m"
        end=$'\e['"${attr_end[i]}m$end"
        break
      fi
    done
  done
  if [ -n "$name" ]; then
    col_name+=("$name")
    col_start+=("$start")
    col_end+=("$end")
  fi
done


echo "Hint:"
echo "  Specify command line arguments (attribute names or starting numbers)"
echo "  to create columns for them. Use the + sign to enable more attributes at once."
echo "  E.g.: ${0##*/} 1 7+8, or ${0##*/} bold reverse+hidden."
echo

echo "Sample text is:"
for (( i = 0; i < ${#words[@]}; i++ )); do
  echo -n "  ${words[i]}: ${words_comment[i]}"
  if [ -n "${words_start[i]}" -o -n "${words_end[i]}" ]; then
    echo -n " (${words_start[i]}/${words_end[i]})"
  fi
  echo
done
echo

# Print the top header, using multiple lines if necessary
again=1
while [ $again = 1 ]; do
  again=0
  printf "%${col1_width}s"
  for (( col = 0; col < ${#col_end[@]}; col++ )); do
    echo -n "  "
    printf "%-${cell_width}.${cell_width}s" "${col_name[col]}"
    col_name[col]="${col_name[col]:cell_width}"
    if [ -n "${col_name[col]}" ]; then
      again=1
    fi
  done
  echo
done

# Print the body of the table
for (( row = 0; row < ${#row_end[@]}; row++ )); do
  printf "%-${col1_width}.${col1_width}s" "${row_name[row]}"
  for (( col = 0; col < ${#col_end[@]}; col++ )); do
    echo -n "  "
    echo -n "${row_start[row]}"
    echo -n "${col_start[col]}"
    echo -n "$cell"
    echo -n "${col_end[col]}"
    echo -n "${row_end[row]}"
  done
  echo
done
