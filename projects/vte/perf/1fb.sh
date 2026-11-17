#!/bin/bash

echo '  0 1 2 3 4 5 6 7 8 9 A B C D E F'
echo

for y in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do
  echo -n "$y "
  for x in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do
    echo -ne "\e[43m\U1fb$x$y\e[49m "
  done
  echo
  echo
done
