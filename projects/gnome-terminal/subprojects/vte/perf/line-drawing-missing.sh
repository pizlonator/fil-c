#!/bin/bash

for i in 25{0..9}{{0..9},{a..f}} 25e{2..5} 1fb{{0..9},a}{{0..9},{a..f}}; do
  char=$(echo -ne "\U$i")
  fgrep -q "$char" boxes.txt || echo $i
done
