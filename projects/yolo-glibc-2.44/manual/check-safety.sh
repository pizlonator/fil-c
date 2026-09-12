#!/bin/sh

# Copyright 2014-2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.


# Check that the @safety notes are self-consistent, i.e., that they're
# in proper order (mt then as then ac), that remarks appear within
# corresponding sections (mt within mt, etc), that unsafety always has
# an explicit reason and when there's a reason for unsafety it's not
# safe.


success=:

# If no arguments are given, take all *.texi files in the current directory.
test $# != 0 || set *.texi

# FIXME: check that each @deftypefu?n is followed by a @safety note,
# with nothing but @deftypefu?nx and comment lines in between.  (There
# might be more stuff too).


# Check that all safety remarks have entries for all of MT, AS and AC,
# in this order, with an optional prelim note before them.
ret=true
grep -n '^@safety' "$@" |
grep -v ':@safety{\(@prelim{}\)\?@mt\(un\)\?safe{.*}'\
'@as\(un\)\?safe{.*}@ac\(un\)\?safe{.*}}' &&
ret=false
if ! $ret; then
    echo "FAIL: #1 - Missing context in @safety macro."
    success=false
fi

# Check that @mt-started notes appear within @mtsafe or @mtunsafe,
# that @as-started notes appear within @assafe or @asunsafe, and that
# @ac-started notes appear within @acsafe or @acunsafe.  Also check
# that @mt, @as and @ac are followed by an s (for safe) or u (for
# unsafe), but let @mt have as, ac or asc before [su], and let @as
# have a c (for cancel) before [su].  Also make sure blanks separate
# each of the annotations.
ret=true
grep -n '^@safety' "$@" |
grep -v ':@safety{\(@prelim{}\)\?'\
'@mt\(un\)\?safe{\(@mt\(asc\?\|ac\)\?[su][^ ]*}\)\?'\
'\( @mt\(asc\?\|ac\)\?[su][^ ]*}\)*}'\
'@as\(un\)\?safe{\(@asc\?[su][^ ]*}\)\?'\
'\( @asc\?[su][^ ]*}\)*}'\
'@ac\(un\)\?safe{\(@ac[su][^ ]*}\)\?'\
'\( @ac[su][^ ]*}\)*}}' &&
ret=false
if ! $ret; then
    echo "FAIL: #2 - Incorrect @safety macro formatting."
    success=false
fi

# Make sure safety lines marked as @mtsafe do not contain any
# MT-Unsafe remark; that would be @mtu, but there could be as, ac or
# asc between mt and u.
ret=true
grep -n '^@safety.*@mtsafe' "$@" |
grep '@mt\(asc\?\|ac\)\?u' &&
ret=false
if ! $ret; then
    echo "FAIL: #3 - MT context contains remarks when it should not."
    success=false
fi

# Make sure @mtunsafe lines contain at least one @mtu remark (with
# optional as, ac or asc between mt and u).
ret=true
grep -n '^@safety.*@mtunsafe' "$@" |
grep -v '@mtunsafe{.*@mt\(asc\?\|ac\)\?u' &&
ret=false
if ! $ret; then
    echo "FAIL: #4 - MT context lacks at least one remark."
    success=false
fi

# Make sure safety lines marked as @assafe do not contain any AS-Unsafe
# remark, which could be @asu or @mtasu note (with an optional c
# between as and u in both cases).
ret=true
grep -n '^@safety.*@assafe' "$@" |
grep '@\(mt\)\?asc\?u' &&
ret=false
if ! $ret; then
    echo "FAIL: #5 - AS context contains remarks when it should not."
    success=false
fi

# Make sure @asunsafe lines contain at least one @asu remark (which
# could be @ascu, or @mtasu or even @mtascu).  We allow the AS safety
# remark from an earlier MT-unsafe to count towards such a remark
# e.g. @mtunsafe{@mtasurace{:LogMask}}@asunsafe{}@acsafe{} is allowed.
ret=true
grep -n '^@safety.*@asunsafe' "$@" |
grep -v '@mtasc\?u.*@asunsafe\|@asunsafe{.*@asc\?u' &&
ret=false
if ! $ret; then
    echo "FAIL: #6 - AS unsafe context lacks at least one remark."
    success=false
fi

# Make sure safety lines marked as @acsafe do not contain any
# AC-Unsafe remark, which could be @acu, @ascu or even @mtacu or
# @mtascu.
ret=true
grep -n '^@safety.*@acsafe' "$@" |
grep '@\(mt\)\?as\?cu' &&
ret=false
if ! $ret; then
    echo "FAIL: #7 - AC context contains remarks when it should not."
    success=false
fi

# Make sure @acunsafe lines contain at least one @acu remark (possibly
# implied by @ascu, @mtacu or @mtascu).
ret=true
grep -n '^@safety.*@acunsafe' "$@" |
grep -v '@\(mtas\?\|as\)cu.*@acunsafe\|@acunsafe{.*@acu' &&
ret=false
if ! $ret; then
    echo "FAIL: #8 - AC unsafe context lacks at least one remark."
    success=false
fi

# Check that comments containing safety remarks do not contain {}s,
# that all @mt remarks appear before @as remarks, that in turn appear
# before @ac remarks, all properly blank-separated, and that an
# optional comment about exclusions is between []s at the end of the
# line.
ret=true
grep -n '^@c \+[^@ ]\+\( dup\)\?'\
'\( @\(mt\|a[sc]\)[^ ]*\)*\( \[.*]\)\?$' "$@" |
grep -v ':@c *[^@{}]*\( @mt[^ {}]*\)*'\
'\( @as[^ {}]*\)*\( @ac[^ {}]*\)*\( \[.*]\)\?$' &&
ret=false
if ! $ret; then
    echo "FAIL: #9 - Comment safety note ordering, brackets, or"\
	 "exclusions are incorrect."
    success=false
fi

$success
