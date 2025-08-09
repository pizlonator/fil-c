#!/bin/bash

source scripts/runtest.sh
source scripts/portability.sh

# Kill child processes when we exit
trap 'kill $(jobs -p) 2>/dev/null; exit 1' INT

# Create working directory
TOPDIR="$PWD"
export FILES="$PWD"/tests/files PREFIX=generated/testdir
rm -rf "$PREFIX"
mkdir -p "$PREFIX"/testdir

# Populate working directory
if [ -z "$TEST_HOST" ]
then
  if [ $# -ne 0 ]
  then
    scripts/single.sh "$@" || exit 1
  else
    scripts/install.sh --symlink --force || exit 1
  fi
fi

# Add prefix to $PATH
export -n PREFIX
cd "$PREFIX"
PATH="$PWD:$PATH" TESTDIR="$PWD"
export LC_COLLATE=C

# Collection OPTIONFLAGS for optional()
[ -f "$TOPDIR/generated/config.h" ] &&
  export OPTIONFLAGS=:$($SED -nr 's/^#define CFG_(.*) 1$/\1/p' "$TOPDIR/generated/config.h" | tr '\n' :)

# Run a test file in $TESTDIR/testdir with $CMDNAME and $C set, parse $FAILCOUNT
do_test()
{
  # reset testdir
  cd "$TESTDIR" && rm -rf testdir continue && mkdir testdir && cd testdir ||
    exit 1

  # set CMDNAME to base name of test file, and C to full path to command
  CMDNAME="${1##*/}" CMDNAME="${CMDNAME%.test}"
  if [ -z "$TEST_HOST" ]
  then
    C="$TESTDIR/$CMDNAME"
    [ ! -e "$C" ] && echo "$SHOWSKIP: $CMDNAME disabled" && return
    C="$(dirname $(realpath "$C"))/$CMDNAME"
  else
    C="$(which $CMDNAME 2>/dev/null)"
    [ -z "$C" ] && printf '%s\n' "$SHOWSKIP: no $CMDNAME" && return
  fi

  # Run command.test in a subshell
  (. "$1"; cd "$TESTDIR"; echo "$FAILCOUNT" > continue)
  cd "$TESTDIR"
  [ -e continue ] && FAILCOUNT=$(($(cat continue)+$FAILCOUNT)) || exit 1
}

# Run each test listed on command line or else all tests with executable bit set
if [ $# -ne 0 ]
then
  for i in "$@"
  do
    do_test "$TOPDIR"/tests/$i.test
  done
else
  for i in "$TOPDIR"/tests/*.test
  do
    [ -z "$TEST_ALL" ] && [ ! -x "$i" ] && continue
    do_test "$i"
  done
fi

[ $FAILCOUNT -eq 0 ]
