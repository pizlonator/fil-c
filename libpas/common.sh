# Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS'' AND ANY
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

case `uname -s` in
    Linux)
        MAKE=make
        CCPREFIX=""
        OS=linux
        ;;
    *)
        echo "Unsupported OS"
        exit 1
        ;;
esac

case `uname -m` in
    amd64|x86_64)
        ARCH=x86_64
        CROSSARCH=aarch64
        ;;
    arm64|aarch64)
        ARCH=aarch64
        CROSSARCH=x86_64
        ;;
    *)
        echo "Unsupported arch"
        exit 1
        ;;
esac

case $OS in
    linux)
        NCPU=`nproc`
        ;;
    *)
        NCPU=`sysctl -n hw.ncpu`
        ;;
esac

extract_source()
{
    rm -rf extracted-source
    git archive --format=tar HEAD --prefix=extracted-source/ | tar -xf -
    git diff --relative HEAD . | (cd extracted-source && patch -p1)
    cd extracted-source
}

# Runs 'make check' with default signal dispositions.
#
# When the build runs in a batch environment (setsid/nohup, or an unprivileged
# container without a controlling tty), SIGINT and SIGQUIT are ignored on entry
# and a POSIX shell cannot reset them. Some test suites (e.g. gnulib's
# test-execute, used by m4 among others) require the default dispositions -
# they raise SIGINT in a child and expect it to die. python3 can reset the
# dispositions (the cannot-reset-ignored-signals rule is shell-specific), so
# use it as an exec launcher when SIGINT is ignored on entry. Note that python
# itself ignores SIGPIPE (and SIGXFZ/SIGXFSZ) at startup, so the launcher must
# explicitly reset those too or they'd propagate to the tests (gnulib's
# test-execute case 3 expects a child to die from SIGPIPE). Falls back to a
# plain 'make check' when python3 is unavailable or signals are fine.
make_check()
{
    if python3 -c 'import signal, sys; sys.exit(0 if signal.getsignal(signal.SIGINT) == signal.SIG_IGN else 1)' 2>/dev/null
    then
        python3 -c '
import os, signal, sys
names = ["SIGINT", "SIGQUIT", "SIGPIPE", "SIGXFZ", "SIGXFSZ"]
for name in names:
    sig = getattr(signal, name, None)
    if sig is not None and signal.getsignal(sig) == signal.SIG_IGN:
        signal.signal(sig, signal.SIG_DFL)
os.execvp(sys.argv[1], sys.argv[1:])' make check "$@"
    else
        make check "$@"
    fi
}
