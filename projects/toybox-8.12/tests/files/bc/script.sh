#! /bin/sh

if [ "$#" -lt 4 ]; then
	echo "usage: script.sh <bc> <test_output1> <test_output2> <script>"
	exit 1
fi

set -e

bc="$1"
shift

out1="$1"
shift

out2="$1"
shift

script="$1"

echo "quit" | bc -lq "$script" > "$out1"
echo "quit" | "$bc" -lq "$script" > "$out2"

diff "$out1" "$out2"
