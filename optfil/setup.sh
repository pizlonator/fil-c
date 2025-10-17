#!/bin/sh
#
# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
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

VERSION="0.673"

echo "================================================================================"
echo "                         Fil-C /opt/fil Distribution"
echo "                              Version $VERSION"
echo "================================================================================"
echo
echo "Fil-C is a memory-safe implementation of C and C++ that prevents all memory"
echo "safety errors through concurrent garbage collection and invisible capabilities."
echo
echo "This distribution includes:"
echo "  - Fil-C compiler version $VERSION (filcc, fil++) and its runtime (libpizlo.so)"
echo "  - Basic libraries compiled with Fil-C (glibc 2.40, LLVM libc++ 20.1.8)"
echo "  - Memory safe programs compiled with Fil-C:"
echo "    - GNU Bash"
echo "    - GNU Coreutils"
echo "    - Compression utilities (bzip2, xz, lz4, zstd)"
echo "    - OpenSSL cryptographic library"
echo "    - OpenSSH client and server"
echo
echo "********************************************************************************"
echo "                                  WARNING!"
echo "********************************************************************************"
echo "THIS IS THE FIRST RELEASE of the /opt/fil binary distribution! Take appropriate"
echo "precautions before using! This distribution is still under active development!"
echo
echo "THIS SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND."
echo "********************************************************************************"
echo

if [ `id -u` -ne 0 ]; then
    echo "ERROR: This installer must be run as root (use sudo or run as root user)."
    exit 1
fi

if [ -e /opt/fil ]; then
    echo "ERROR: /opt/fil already exists!"
    echo "Please remove or move it before running this installer:"
    echo "  rm -rf /opt/fil"
    echo "    or"
    echo "  mv /opt/fil /opt/fil.backup"
    exit 1
fi

echo "This installer will:"
if [ ! -d /opt ]; then
    echo "  - Create /opt directory (doesn't currently exist)"
fi
echo "  - Extract fil.tar to /opt, creating /opt/fil"
echo
echo "Type YES (in all caps) to proceed with installation, or anything else to abort:"
read response

if [ "$response" != "YES" ]; then
    echo "Installation aborted."
    exit 1
fi

echo
echo "Extracting fil.tar to /opt..."

if [ ! -f fil.tar ]; then
    echo "Error: fil.tar not found in current directory"
    exit 1
fi

if [ ! -d /opt ]; then
    echo "Creating /opt directory..."
    mkdir -p /opt
fi

cd /opt
tar -xf "$OLDPWD/fil.tar"

echo
echo "Setting up SSH host keys..."
# Check if SSH host keys exist in /etc/ssh
if [ -f /etc/ssh/ssh_host_rsa_key ] \
       && [ -f /etc/ssh/ssh_host_ecdsa_key ] \
       && [ -f /etc/ssh/ssh_host_ed25519_key ] \
       && [ -f /etc/ssh/ssh_host_rsa_key.pub ] \
       && [ -f /etc/ssh/ssh_host_ecdsa_key.pub ] \
       && [ -f /etc/ssh/ssh_host_ed25519_key.pub ]; then
    echo "Found existing SSH host keys in /etc/ssh"
    echo "Copying them to /opt/fil/etc/ssh..."
    cp -v /etc/ssh/ssh_host_rsa_key /opt/fil/etc/ssh/
    cp -v /etc/ssh/ssh_host_ecdsa_key /opt/fil/etc/ssh/
    cp -v /etc/ssh/ssh_host_ed25519_key /opt/fil/etc/ssh/
    cp -v /etc/ssh/ssh_host_rsa_key.pub /opt/fil/etc/ssh/
    cp -v /etc/ssh/ssh_host_ecdsa_key.pub /opt/fil/etc/ssh/
    cp -v /etc/ssh/ssh_host_ed25519_key.pub /opt/fil/etc/ssh/
    chmod -v 600 /etc/ssh/ssh_host_rsa_key
    chmod -v 600 /etc/ssh/ssh_host_ecdsa_key
    chmod -v 600 /etc/ssh/ssh_host_ed25519_key
    chmod -v 644 /etc/ssh/ssh_host_rsa_key.pub
    chmod -v 644 /etc/ssh/ssh_host_ecdsa_key.pub
    chmod -v 644 /etc/ssh/ssh_host_ed25519_key.pub
    echo "SSH host keys copied successfully."
else
    echo "No existing SSH host keys found in /etc/ssh"
    echo "Generating new SSH host keys for /opt/fil..."
    /opt/fil/bin/ssh-keygen -A
    echo "New SSH host keys generated."
fi

if id sshd > /dev/null 2>&1; then
    echo "SSH privsep user sshd already exists."
else
    echo
    if getent group sshd > /dev/null 2>&1; then
	echo "SSH privsep user sshd does not exist!"
	echo
	echo "To create sshd privsep user, type YES (in all caps) or anything else to skip."
    else
	echo "SSH privsep user sshd and group sshd do not exist!"
	echo
	echo "To create sshd privsep user and sshd group, type YES (in all caps) or anything"
	echo "else to skip."
    fi
    read sshd_response

    if [ "$sshd_response" = "YES" ]; then
	if ! getent group sshd > /dev/null 2>&1; then
	    groupadd sshd
	    echo "Created sshd group."
	fi
	useradd -c 'sshd PrivSep' \
		-d /opt/fil/var/lib/sshd \
		-g sshd \
		-s /bin/false \
		sshd
	echo "Created sshd privsep user."
    else
	echo "Not creating sshd privsep user."
    fi
fi
echo

echo
echo "================================================================================"
echo "                         Installation Complete!"
echo "================================================================================"
echo
echo "To use Fil-C, add /opt/fil/bin to your PATH:"
echo "  export PATH=/opt/fil/bin:\$PATH"
echo
echo "You can use the memory-safe OpenSSH server at:"
echo "  /opt/fil/sbin/sshd"
echo
echo "To compile C programs with Fil-C:"
echo "  filcc -o program program.c -g -O"
echo
echo "To compile C++ programs with Fil-C++:"
echo "  fil++ -o program program.cpp -g -O -std=c++20"
echo
echo "For more information, see README.md in the distribution directory."
echo
