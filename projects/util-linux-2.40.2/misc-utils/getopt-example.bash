#!/bin/bash

# A small example script for using the getopt(1) program.
# This script will only work with bash(1).
# A similar script using the tcsh(1) language can be found
# as getopt-example.tcsh.

# Example input and output (from the bash prompt):
#
# ./getopt-example.bash -a par1 'another arg' --c-long 'wow!*\?' -cmore -b " very long "
# Option a
# Option c, no argument
# Option c, argument 'more'
# Option b, argument ' very long '
# Remaining arguments:
# --> 'par1'
# --> 'another arg'
# --> 'wow!*\?'

# Note that we use "$@" to let each command-line parameter expand to a
# separate word. The quotes around "$@" are essential!
# We need TEMP as the 'eval set --' would nuke the return value of getopt.
TEMP=$(getopt -o 'ab:c::' --long 'a-long,b-long:,c-long::' -n 'example.bash' -- "$@")

if [ $? -ne 0 ]; then
	echo 'Terminating...' >&2
	exit 1
fi

# Note the quotes around "$TEMP": they are essential!
eval set -- "$TEMP"
unset TEMP

while true; do
	case "$1" in
		'-a'|'--a-long')
			echo 'Option a'
			shift
			continue
		;;
		'-b'|'--b-long')
			echo "Option b, argument '$2'"
			shift 2
			continue
		;;
		'-c'|'--c-long')
			# c has an optional argument. As we are in quoted mode,
			# an empty parameter will be generated if its optional
			# argument is not found.
			case "$2" in
				'')
					echo 'Option c, no argument'
				;;
				*)
					echo "Option c, argument '$2'"
				;;
			esac
			shift 2
			continue
		;;
		'--')
			shift
			break
		;;
		*)
			echo 'Internal error!' >&2
			exit 1
		;;
	esac
done

echo 'Remaining arguments:'
for arg; do
	echo "--> '$arg'"
done
