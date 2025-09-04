#!/bin/sh
# Test of the 'verror' module.

. "${srcdir=.}/init.sh"; path_prepend_ .

${CHECKER} test-verror${EXEEXT} > out 2> err
# Verify the exit code.
case $? in
  4) ;;
  *) Exit 1;;
esac

# Normalize the stderr output on Windows platforms.
tr -d '\015' < err | sed 's,.*test-verror[.ex]*:,test-verror:,' > err2 || Exit 1

# Verify the stderr output.
compare - err2 <<\EOF || Exit 1
test-verror: bummer
test-verror: Zonk 123 is too large
test-verror: Pokémon started
test-verror:d1/foo.c:10: invalid blub
test-verror:d1/foo.c:10: invalid blarn
test-verror:d1/foo.c:10: unsupported glink
test-verror:d1/foo.c:13: invalid brump
test-verror:d2/foo.c:13: unsupported flinge
hammer
boing 123 is too large
d2/bar.c:11: bark too loud
test-verror: can't steal: Permission denied
test-verror: fatal error
EOF

# Verify the stdout output.
test -s out && Exit 1

Exit 0
