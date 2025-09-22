# Copyright (C) 2021 Red Hat, Inc.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# sourced from run-debuginfod-*.sh tests (must be bash scripts)

# We trap ERR and like commands that fail in function to also trap
set -o functrace
set -o errtrace

. $srcdir/test-subr.sh  # includes set -e

type curl 2>/dev/null || (echo "need curl"; exit 77)
type rpm2cpio 2>/dev/null || (echo "need rpm2cpio"; exit 77)
type cpio 2>/dev/null || (echo "need cpio"; exit 77)
type bzcat 2>/dev/null || (echo "need bzcat"; exit 77)
bsdtar --version | grep -q zstd && zstd=true || zstd=false
echo "zstd=$zstd bsdtar=`bsdtar --version`"

cleanup()
{
  # No more cleanups after this cleanup
  trap - 0

  if [ $PID1 -ne 0 ]; then kill $PID1 || : ; wait $PID1 || :; fi
  if [ $PID2 -ne 0 ]; then kill $PID2 || : ; wait $PID2 || :; fi
  rm -rf F R D L Z ${PWD}/foobar ${PWD}/mocktree ${PWD}/.client_cache* ${PWD}/tmp*
  exit_cleanup
}

# clean up trash if we exit
trap cleanup 0

errfiles_list=
err() {
    # Don't trap any new errors from now on
    trap - ERR

    echo ERROR REPORTS
    for port in $PORT1 $PORT2
    do
        echo ERROR REPORT $port metrics
        curl -s http://127.0.0.1:$port/metrics || :
        echo
    done
    for x in $errfiles_list
    do
        echo ERROR REPORT "$x"
        cat $x
        echo
    done
    cleanup
    false # trigger set -e
}
trap err ERR

errfiles() {
    errfiles_list="$errfiles_list $*"
}

# We want to run debuginfod in the background.  We also want to start
# it with the same check/installcheck-sensitive LD_LIBRARY_PATH stuff
# that the testrun alias sets.  But: we if we just use
#    testrun .../debuginfod
# it runs in a subshell, with different pid, so not helpful.
#
# So we gather the LD_LIBRARY_PATH with this cunning trick:
ldpath=`testrun sh -c 'echo $LD_LIBRARY_PATH'`

wait_ready4()
{
  port=$1;
  what=$2;
  value=$3;
  timeout=$4;

  echo "Wait $timeout seconds on $port for metric $what to change to $value"
  while [ $timeout -gt 0 ]; do
    mvalue="$(curl -s http://127.0.0.1:$port/metrics \
              | grep "$what" | awk '{print $NF}')"
    if [ -z "$mvalue" ]; then mvalue=0; fi
      echo "metric $what: $mvalue"
      if [ "$mvalue" -eq "$value" ]; then
        break;
    fi
    sleep 0.5;
    ((timeout--));
  done;

  if [ $timeout -eq 0 ]; then
    echo "metric $what never changed to $value on port $port"
    err
  fi
}

wait_ready()
{
  port=$1;
  what=$2;
  value=$3;
  timeout=20;
  wait_ready4 "$port" "$what" "$value" "$timeout"
}


archive_test() {
    __BUILDID=$1
    __SOURCEPATH=$2
    __SOURCESHA1=$3

    filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find executable $__BUILDID`
    buildid=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
             -a $filename | grep 'Build ID' | cut -d ' ' -f 7`
    test $__BUILDID = $buildid
    # check that timestamps are plausible - older than the near-present (tmpdir mtime)
    test $filename -ot `pwd`

    # run again to assure that fdcache is being enjoyed
    filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find executable $__BUILDID`
    buildid=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
             -a $filename | grep 'Build ID' | cut -d ' ' -f 7`
    test $__BUILDID = $buildid
    test $filename -ot `pwd`

    filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $__BUILDID`
    buildid=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
             -a $filename | grep 'Build ID' | cut -d ' ' -f 7`
    test $__BUILDID = $buildid
    test $filename -ot `pwd`

    if test "x$__SOURCEPATH" != "x"; then
        filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find source $__BUILDID $__SOURCEPATH`
        hash=`cat $filename | sha1sum | awk '{print $1}'`
        test $__SOURCESHA1 = $hash
        test $filename -ot `pwd`
    fi
}

get_ports() {
  while true; do
    PORT1=`expr '(' $RANDOM % 50 ')' + $base`
    ss -atn | grep -F ":$PORT1" || break
  done
# Some tests will use two servers, so assign the second var
  while true; do
    PORT2=`expr '(' $RANDOM % 50 ')' + $base + 50`
    ss -atn | grep -F ":$PORT2" || break
  done

}

VERBOSE=-vvv
# We gather the LD_LIBRARY_PATH with this cunning trick:
ldpath=`testrun sh -c 'echo $LD_LIBRARY_PATH'`
PORT1=0
PORT2=0
PID1=0
PID2=0


# run $1 as a sh -c command, invert result code
xfail() {
    if sh -c "$1"; then
        false
    else
        true
    fi
}
