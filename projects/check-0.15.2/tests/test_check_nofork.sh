#!/usr/bin/env sh

. ./test_vars

if [ $HAVE_FORK -eq 1 ]; then
expected="Running suite(s): NoFork
0%: Checks: 1, Failures: 1, Errors: 0"
else
expected="Running suite(s): NoFork
0%: Checks: 1, Failures: 1, Errors: 0
Running suite(s): NoForkSupport
100%: Checks: 1, Failures: 0, Errors: 0"
fi

actual=`./check_nofork${EXEEXT} | tr -d "\r"`
if [ x"${expected}" = x"${actual}" ]; then
  exit 0
else
  echo "Problem with check_nofork${EXEEXT}"
  echo "Expected: "
  echo "${expected}"
  echo "Got: "
  echo "${actual}"
  exit 1
fi
