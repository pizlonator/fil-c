#
# This is a sed script to patch the generated libtool,
# which works well against both of libtool 2.4.2 and 2.4.7.
#
# You may use this work under the terms of a Creative Commons CC0 1.0
# License/Waiver.
#
# CC0 Public Domain Dedication
# https://creativecommons.org/publicdomain/zero/1.0/

#
# This sed script applys two hunks of the patch:
#
#     Part1: after the comment "# bleh windows"
#     Part2: after the comment "#extension on DOS 8.3..."
#
# Only when those two parts are patched correctly, it exits with 0 or
# else, it exits with 1
#

# Find the part 1, by the comment
/^[ \t]*# bleh windows$/b part1_start
# Not found the part1, raise an error
$ q1
b

:part1_start
n
# The first line in the part 1 must be the begining of the case statement.
/^[ \t]*case \$host in$/! q1
n
# Insert the entry for x86_64-*mingw32*, for modified versuffix.
i\
	      x86_64-*mingw32*)
i\
		func_arith $current - $age
i\
		major=$func_arith_result
i\
		versuffix="6-$major"
i\
		;;
:part1_0
# Find the end of the case statement
/^[ \t]*esac$/b find_part2
# Not found the end of the case statement, raise an error
$ q1
n
b part1_0

:find_part2
/^[ \t]*# extension on DOS 8.3 file.*systems.$/b part2_process
# Not found the part2, raise an error
$ q1
n
b find_part2

:part2_process
$ q1
s/^[ \t]*\(versuffix=\)\(.*\)\(-$major\)\(.*\)$/\t  case \$host in\n\t  x86_64-*mingw32*)\n\t    \1\26\3\4\n\t    ;;\n\t  *)\n\t    \1\2\3\4\n\t    ;;\n\t  esac/
t part2_done
n
b part2_process

:part2_done
$ q0
n
b part2_done
