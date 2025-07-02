#!/bin/sh
#
# Copyright (c) 2023-2025 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

set -e
set -x

GITBASE=`git remote -v | awk '{print $2}' | head -1 | sed 's/\([^\/]*\)$//'`

handle_git_with_branch()
{
    remote_path=$1
    local_path=$2
    branch=$3
    if test -d $local_path
    then
        (cd $local_path && git pull --rebase)
    else
        git clone $GITBASE$remote_path $local_path
        (cd $local_path &&
             git checkout $branch &&
             git branch --set-upstream-to origin/$branch)
    fi
}

handle_git()
{
    remote_path=$1
    local_path=$2
    if test -d $local_path
    then
        (cd $local_path && git pull --rebase)
    else
        git clone $GITBASE$remote_path $local_path
    fi
}

handle_git_with_branch deluded-musl yolomusl yolomusl
handle_git_with_branch deluded-musl usermusl usermusl

handle_git deluded-zlib-1.3.git zlib-1.3
handle_git deluded-openssl-3.2.0.git openssl-3.2.0
handle_git deluded-curl-8.5.0.git curl-8.5.0
handle_git deluded-openssh-portable.git deluded-openssh-portable
handle_git pizlonated-pcre-8.39.git pcre-8.39
handle_git pizlonated-jpeg-6b.git pizlonated-jpeg-6b
handle_git pizlonated-bzip2.git pizlonated-bzip2
handle_git pizlonated-xz.git pizlonated-xz
handle_git pizlonated-mg.git pizlonated-mg
handle_git pizlonated-sqlite.git pizlonated-sqlite
handle_git pizlonated-cpython.git pizlonated-cpython
handle_git pizlonated-ncurses.git ncurses-6.5-20240720
handle_git pizlonated-pcre2.git pizlonated-pcre2
handle_git pizlonated-zsh.git pizlonated-zsh
handle_git pizlonated-nghttp2.git pizlonated-nghttp2
handle_git pizlonated-lua.git pizlonated-lua
handle_git pizlonated-simdutf pizlonated-simdutf
handle_git pizlonated-tcl pizlonated-tcl
handle_git pizlonated-icu pizlonated-icu
handle_git pizlonated-libedit pizlonated-libedit
handle_git pizlonated-postgres pizlonated-postgres
handle_git pizlonated-quickjs pizlonated-quickjs
handle_git wg14_signals wg14_signals
handle_git pizlonated-libuev pizlonated-libuev
handle_git pizlonated-bzip3 pizlonated-bzip3
handle_git pizlonated-simdjson pizlonated-simdjson
handle_git pizlonated-ada pizlonated-ada
handle_git pizlonated-zstd-1.5.6 pizlonated-zstd
handle_git pizlonated-libffi libffi-3.4.6
handle_git pizlonated-libexpat expat-2.7.1
handle_git pizlonated-libxml2 libxml2-2.14.4
handle_git pizlonated-dash dash-0.5.12

