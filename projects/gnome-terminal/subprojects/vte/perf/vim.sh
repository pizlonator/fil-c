#!/bin/sh

cd "`dirname "$0"`"
# rudimentary Vim performance benchmark

# scrolling (just the cursor)
\time vim -u scroll.vim -c ':quit' UTF-8-demo.txt
\time vim -u scroll.vim -c ':call AutoScroll(100)' UTF-8-demo.txt
\time vim -u scroll.vim -c ':call AutoWindowScroll(10)' UTF-8-demo.txt

echo press enter to close
read
