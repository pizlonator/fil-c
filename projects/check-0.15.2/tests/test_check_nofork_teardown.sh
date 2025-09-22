#!/usr/bin/env sh

. ./test_vars

expected="Running suite(s): bug-99
0%: Checks: 1, Failures: 1, Errors: 0
${SRCDIR}check_nofork_teardown.c:56:F:tc:will_fail:0: Assertion '0' failed"

actual=`./check_nofork_teardown${EXEEXT} | tr -d "\r"`
if [ x"${expected}" = x"${actual}" ]; then
  exit 0
else
  echo "Problem with check_nofork_teardown${EXEEXT}"
  echo "Expected: "
  echo "${expected}"
  echo "Got: "
  echo "${actual}"
  exit 1
fi
