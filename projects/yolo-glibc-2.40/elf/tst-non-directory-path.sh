#!/bin/sh
# Test if library search path does not terminates with non-directory
# components.
# Copyright (C) 2023-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

set -e

rtld=$1
test_program=$2
test_wrapper_env=$3
run_program_env=$4
# Remove the last space to allow concatenate extra paths.
library_path=$(echo $5)
objpfx=$6

test_binary=$(basename ${test_program})
test_libpath=${test_binary}-x

mkdir -p ${objpfx}/${test_libpath}
mv ${objpfx}/tst-non-directory-mod.so ${objpfx}/${test_libpath}

# Check with absolute paths.
${test_wrapper_env} \
${run_program_env} \
$rtld --inhibit-cache \
  --library-path \
  "$library_path":"${objpfx}"tst-non-directory-invalid:"${objpfx}"${test_libpath} \
  $test_program 2>&1 && rc=0 || rc=$?

${test_wrapper_env} \
${run_program_env} \
$rtld --inhibit-cache \
  --library-path \
  "$library_path":"${objpfx}"${test_binary}:"${objpfx}"${test_libpath} \
  $test_program 2>&1 && rc=0 || rc=$?

# Relative paths along with non-existent path in search list.
cd "$objpfx"
${test_wrapper_env} \
${run_program_env} \
$rtld --inhibit-cache \
  --library-path \
  "$library_path":../elf/tst-non-directory-invalid::../elf/${test_libpath} \
  $test_program 2>&1 && rc=0 || rc=$?

# Relative paths along with component in the path is not a directory in search list.
${test_wrapper_env} \
${run_program_env} \
$rtld --inhibit-cache \
  --library-path \
  "$library_path":../elf/${test_binary}:../elf/${test_libpath} \
  $test_program 2>&1 && rc=0 || rc=$?

# Relative paths along with non-existent path and a component in the path that is not a directory.
${test_wrapper_env} \
${run_program_env} \
$rtld --inhibit-cache \
  --library-path \
  "$library_path":../elf/tst-non-directory-invalid:../elf/${test_binary}:../elf/${test_libpath} \
  $test_program 2>&1 && rc=0 || rc=$?

exit $rc
