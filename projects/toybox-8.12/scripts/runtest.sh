# Simple test harness infrastructure
#
# Copyright 2005 by Rob Landley

# This file defines three main functions: "testing", "testcmd", and "txpect".

# The following environment variables enable optional behavior in "testing":
#    DEBUG - Show every command run by test script.
#    VERBOSE - "all"    continue after failed test
#              "fail"   show diff and stop at first failed test
#              "nopass" don't show successful tests
#              "quiet"  don't show diff -u for failures
#              "spam"   show passing test command lines
#
# The "testcmd" function takes five arguments:
#	$1) Description to display when running command
#	$2) Command line arguments to command
#	$3) Expected result (on stdout)
#	$4) Data written to file "input"
#	$5) Data written to stdin
#
# The "testing" function is like testcmd but takes a complete command line
# (I.E. you have to include the command name.) The variable $C is an absolute
# path to the command being tested, which can bypass shell builtins.
#
# The exit value of testcmd is the exit value of the command it ran.
#
# The environment variable "FAILCOUNT" contains a cumulative total of the
# number of failed tests.
#
# The environment variable "SKIP" says how many upcoming tests to skip,
# defaulting to 0 and counting down when set to a higher number.
#
# Function "optional" enables/disables tests based on configuration options.

export FAILCOUNT=0 SKIP=0
: ${SHOWPASS:=PASS} ${SHOWFAIL:=FAIL} ${SHOWSKIP:=SKIP}
if tty -s <&1
then
  SHOWPASS="$(echo -e "\033[1;32m${SHOWPASS}\033[0m")"
  SHOWFAIL="$(echo -e "\033[1;31m${SHOWFAIL}\033[0m")"
  SHOWSKIP="$(echo -e "\033[1;33m${SHOWSKIP}\033[0m")"
fi

# Helper functions

# Check if VERBOSE= contains a given string. (This allows combining.)
verbose_has()
{
  [ "${VERBOSE/$1/}" != "$VERBOSE" ]
}

wrong_args()
{
  if [ $# -ne 5 ]
  then
    printf "%s\n" "Test $NAME has the wrong number of arguments ($# $*)" >&2
    exit
  fi
}

# Announce success
do_pass()
{
  verbose_has nopass || printf "%s\n" "$SHOWPASS: $NAME"
}

# Announce failure and handle fallout for txpect
do_fail()
{
  FAILCOUNT=$(($FAILCOUNT+1))
  printf "%s\n" "$SHOWFAIL: $NAME"
  if [ ! -z "$CASE" ]
  then
    echo "Expected '$CASE'"
    echo "Got '$A'"
  fi
  ! verbose_has all && exit 1
}

# Functions test files call directly

# Set SKIP high if option not enabled in $OPTIONFLAGS (unless OPTIONFLAGS blank)
optional()
{
  [ -n "$OPTIONFLAGS" ] && [ "$OPTIONFLAGS" == "${OPTIONFLAGS/:$1:/}" ] &&
    SKIP=99999 || SKIP=0
}

# Evalute command line and skip next test when false
skipnot()
{
  if verbose_has quiet
  then
    eval "$@" >/dev/null 2>&1
  else
    eval "$@"
  fi
  [ $? -eq 0 ] || { ((++SKIP)); return 1; }
}

# Skip this test (rest of command line) when not running toybox.
toyonly()
{
  [ -z "$TEST_HOST" ] && IS_TOYBOX=toybox
  : ${IS_TOYBOX:=$("$C" --version 2>/dev/null | grep -o toybox)} \
    ${IS_TOYBOX:="$(basename $(readlink -f "$C"))"}
  # Ideally we'd just check for "toybox", but toybox sed lies to make autoconf
  # happy, so we have at least two things to check for.
  case "$IS_TOYBOX" in
    toybox*) ;;
    This\ is\ not\ GNU*) ;;
    *) [ $SKIP -eq 0 ] && ((++SKIP)) ;;
  esac

  "$@"
}

# Takes five arguments: "name" "command" "result" "infile" "stdin"
testing()
{
  wrong_args "$@"

  [ -z "$1" ] && NAME="$2" || NAME="$1"
  [ "${NAME#$CMDNAME }" == "$NAME" ] && NAME="$CMDNAME $1"

  [ -n "$DEBUG" ] && set -x

  if [ "$SKIP" -gt 0 ]
  then
    verbose_has quiet || printf "%s\n" "$SHOWSKIP: $NAME"
    ((--SKIP))

    return 0
  fi

  echo -ne "$3" > "$TESTDIR"/expected
  [ ! -z "$4" ] && echo -ne "$4" > input || rm -f input
  echo -ne "$5" | ${EVAL:-eval --} "$2" > "$TESTDIR"/actual
  RETVAL=$?

  # Catch segfaults
  [ $RETVAL -gt 128 ] &&
    echo "exited with signal (or returned $RETVAL)" >> actual
  DIFF="$(cd "$TESTDIR"; diff -au${NOSPACE:+w} expected actual 2>&1)"
  [ -z "$DIFF" ] && do_pass || VERBOSE=all do_fail
  if ! verbose_has quiet && { [ -n "$DIFF" ] || verbose_has spam; }
  then
    [ ! -z "$4" ] && printf "%s\n" "echo -ne \"$4\" > input"
    printf "%s\n" "echo -ne '$5' |$EVAL $2"
    [ -n "$DIFF" ] && printf "%s\n" "$DIFF"
  fi

  [ -n "$DIFF" ] && ! verbose_has all && exit 1
  rm -f input ../expected ../actual

  [ -n "$DEBUG" ] && set +x

  return 0
}

# Wrapper for "testing", adds command name being tested to start of command line
testcmd()
{
  wrong_args "$@"

  testing "${1:-$CMDNAME $2}" "\"$C\" $2" "$3" "$4" "$5"
}

utf8locale()
{
  local i

  for i in $LC_ALL C.UTF-8 en_US.UTF-8
  do
    [ "$(LC_ALL=$i locale charmap 2>/dev/null)" == UTF-8 ] && LC_ALL=$i && break
  done
}

# Simple implementation of "expect" written in shell.

# txpect NAME COMMAND [I/O/E/X/R[OE]string]...
# Run COMMAND and interact with it:
# I send string to input
# OE read exactly this string from stdout or stderr (bare = read+discard line)
#    note: non-bare does not read \n unless you include it with O$'blah\n'
# R prefix means O or E is regex match (read line, must contain substring)
# X close stdin/stdout/stderr and match return code (blank means nonzero)
txpect()
{
  local NAME CASE VERBOSITY IN OUT ERR LEN PID A B X O

  # Run command with redirection through fifos
  NAME="$CMDNAME $1"
  CASE=
  VERBOSITY=

  if [ $# -lt 2 ] || ! mkfifo in-$$ out-$$ err-$$
  then
    do_fail
    return
  fi
  eval "$2" <in-$$ >out-$$ 2>err-$$ &
  PID=$!
  shift 2
  : {IN}>in-$$ {OUT}<out-$$ {ERR}<err-$$ && rm in-$$ out-$$ err-$$

  [ $? -ne 0 ] && { do_fail;return;}

  # Loop through challenge/response pairs, with 2 second timeout
  while [ $# -gt 0 -a -n "$PID" ]
  do
    VERBOSITY="$VERBOSITY"$'\n'"$1"  LEN=$((${#1}-1))  CASE="$1"  A=  B=

    verbose_has spam && echo "txpect $CASE"
    case ${1::1} in

      # send input to child
      I) printf %s "${1:1}" >&$IN || { do_fail;break;} ;;

      R) LEN=0; B=1; ;&
      # check output from child
      [OE])
        [ $LEN == 0 ] && LARG="" || LARG="-rN $LEN"
        O=$OUT  A=
        [ "${1:$B:1}" == 'E' ] && O=$ERR
        read -t2 $LARG A <&$O
        X=$?
        verbose_has spam && echo "txgot $X '$A'"
        VERBOSITY="$VERBOSITY"$'\n'"$A"
        if [ $LEN -eq 0 ]
        then
          [ -z "$A" -o "$X" -ne 0 ] && { do_fail;break;}
        else
          if [ ${1::1} == 'R' ] && grep -q "${1:2}" <<< "$A"; then true
          elif [ ${1::1} != 'R' ] && [ "$A" == "${1:1}" ]; then true
          else
            # Append the rest of the output if there is any.
            read -t.1 B <&$O
            A="$A$B"
            read -t.1 -rN 9999 B<&$ERR
            do_fail
            break
          fi
        fi
        ;;

      # close I/O and wait for exit
      X)
        exec {IN}<&-
        wait $PID
        A=$?
        exec {OUT}<&- {ERR}<&-
        if [ "$LEN" -eq 0 ]
        then
          [ $A -eq 0 ] && { do_fail;break;}        # any error
        else
          [ $A != "${1:1}" ] && { do_fail;break;}  # specific value
        fi
        do_pass
        return
        ;;
      *) do_fail; break ;;
    esac
    shift
  done
  # In case we already closed it
  exec {IN}<&- {OUT}<&- {ERR}<&-

  if [ $# -eq 0 ]
  then
    do_pass
  else
    ! verbose_has quiet && echo "$VERBOSITY" >&2
    do_fail
  fi
}
