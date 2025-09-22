#! /bin/sh
# Copyright (C) 2005-2015, 2017 Red Hat, Inc.
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


# This file is sourced by ". $srcdir/test-subr.sh" at the start of
# each test script.  It defines some functions they use and sets up
# canonical sh state for test runs.

set -e

# Each test runs in its own directory to make sure they can run in parallel.
test_dir="test-$$"
mkdir -p "$test_dir"
cd "$test_dir"

#LC_ALL=C
#export LC_ALL

remove_files=

# Tests that trap EXIT (0) themselves should call this explicitly.
exit_cleanup()
{
  rm -f $remove_files; cd ..; rmdir $test_dir
}
trap exit_cleanup 0

tempfiles()
{
  remove_files="$remove_files $*"
}

testfiles()
{
  for file; do
    bunzip2 -c ${abs_srcdir}/${file}.bz2 > ${file} || exit 77
    remove_files="$remove_files $file"
  done
}

testrun_out()
{
  outfile="$1"
  shift
  remove_files="$remove_files $outfile"
  testrun "$@" > $outfile 2>&1 || :
}

testrun_compare()
{
  outfile="${1##*/}.out"
  testrun_out $outfile "$@"
  diff -u $outfile -
  # diff's exit status will kill the script.
}

test_cleanup()
{
  rm -f $remove_files
  remove_files=
}

# See test-wrapper.sh, which sets the environment for this.
testrun()
{
  ${elfutils_testrun}_testrun "$@"
}

built_testrun()
{
  LD_LIBRARY_PATH="${built_library_path}${LD_LIBRARY_PATH:+:}$LD_LIBRARY_PATH"\
  $VALGRIND_CMD "$@"
}

installed_testrun()
{
  program="$1"
  shift
  case "$program" in
  ${abs_builddir}/*)
    if [ "x$elfutils_tests_rpath" != xno ]; then
      echo >&2 installcheck not possible with --enable-tests-rpath
      exit 77
    fi
    ;;
  ${abs_top_builddir}/src/*)
    program=${bindir}/`program_transform ${program##*/}`
    ;;
  esac
  if [ "${libdir}" != /usr/lib ] && [ "${libdir}" != /usr/lib64 ]; then
    LD_LIBRARY_PATH="${libdir}:${libdir}/elfutils\
${LD_LIBRARY_PATH:+:}$LD_LIBRARY_PATH" \
    $VALGRIND_CMD $program ${1+"$@"}
  else
    $VALGRIND_CMD $program ${1+"$@"}
  fi
}

program_transform()
{
  echo "$*" | sed "${program_transform_name}"
}

self_test_files_exe=`echo ${abs_top_builddir}/src/addr2line \
${abs_top_builddir}/src/elfclassify \
${abs_top_builddir}/src/stack \
${abs_top_builddir}/src/unstrip`

self_test_files_lib=`echo ${abs_top_builddir}/libelf/libelf.so \
${abs_top_builddir}/libasm/libasm.so`

self_test_files_obj=`echo ${abs_top_builddir}/src/size.o \
${abs_top_builddir}/src/strip.o`

self_test_files="$self_test_files_exe $self_test_files_lib $self_test_files_obj"

# Provide a command to run on all self-test files with testrun.
testrun_on_self()
{
  exit_status=0

  for file in $self_test_files; do
      testrun $* $file \
	  || { echo "*** failure in $* $file"; exit_status=1; }
  done

  # Only exit if something failed
  if test $exit_status != 0; then exit $exit_status; fi
}

testrun_on_self_exe()
{
  exit_status=0

  for file in $self_test_files_exe; do
      testrun $* $file \
	  || { echo "*** failure in $* $file"; exit_status=1; }
  done

  # Only exit if something failed
  if test $exit_status != 0; then exit $exit_status; fi
}

testrun_on_self_lib()
{
  exit_status=0

  for file in $self_test_files_lib; do
      testrun $* $file \
	  || { echo "*** failure in $* $file"; exit_status=1; }
  done

  # Only exit if something failed
  if test $exit_status != 0; then exit $exit_status; fi
}

testrun_on_self_obj()
{
  exit_status=0

  for file in $self_test_files_obj; do
      testrun $* $file \
	  || { echo "*** failure in $* $file"; exit_status=1; }
  done

  # Only exit if something failed
  if test $exit_status != 0; then exit $exit_status; fi
}

# Compress the files first. Compress both debug sections and symtab.
testrun_on_self_compressed()
{
  exit_status=0

  for file in $self_test_files; do
      tempfiles ${file}z
      testrun ${abs_top_builddir}/src/elfcompress -f -q -o ${file}z ${file}
      testrun ${abs_top_builddir}/src/elfcompress -f -q --name='.s??tab' ${file}z

      testrun $* ${file}z \
	  || { echo "*** failure in $* ${file}z"; exit_status=1; }
  done

  # Only exit if something failed
  if test $exit_status != 0; then exit $exit_status; fi
}

# Same as above, but redirects stdout to /dev/null
testrun_on_self_quiet()
{
  exit_status=0

  for file in $self_test_files; do
      testrun $* $file > /dev/null \
	  || { echo "*** failure in $* $file"; exit_status=1; }
  done

  # Only exit if something failed
  if test $exit_status != 0; then exit $exit_status; fi
}
