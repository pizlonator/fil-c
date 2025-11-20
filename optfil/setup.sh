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

VERSION="0.675"

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
echo "      - bash        - zlib        - gzip        - bzip2       - xz"
echo "      - lz4         - zstd        - coreutils   - openssl     - openssh"
echo "      - libaudit    - keyutils    - binutils    - pam         - kerberos5"
echo "      - libselinux  - mg          - pkgconf     - pcre2       - sudo"
echo "      - sed         - psmisc      - flex        - grep        - less"
echo "      - diff        - gawk        - find        - make        - patch"
echo "      - tar         - icu4c       - procps-ng   - libevent    - libuv"
echo "      - tmux        - unistring   - libidn2     - libpsl      - libtasn1"
echo "      - p11-kit     - nghttp2     - curl        - wget        - git"
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
echo "  - Extract fil.tar.xz to /opt, creating /opt/fil"
echo
echo "Type YES (in all caps) to proceed with installation, or anything else to abort:"
read response

if [ "$response" != "YES" ]; then
    echo "Installation aborted."
    exit 1
fi

echo
echo "Extracting fil.tar.xz to /opt..."

if [ ! -f fil.tar.xz ]; then
    echo "Error: fil.tar.xz not found in current directory"
    exit 1
fi

if [ ! -d /opt ]; then
    echo "Creating /opt directory..."
    mkdir -p /opt
fi

cd /opt
tar -xf "$OLDPWD/fil.tar.xz"

echo
echo "Checking SSH configuration..."

# Phase 1: Detection
# ==================

# Temporarily disable exit-on-error for detection phase
set +e

# Check if /etc/ssh exists and is writable
SSH_DIR_OK=false
if [ -d /etc/ssh ]; then
    if [ -w /etc/ssh ]; then
        SSH_DIR_OK=true
    fi
elif mkdir -p /etc/ssh 2>/dev/null; then
    SSH_DIR_OK=true
fi

# Re-enable exit-on-error
set -e

if [ "$SSH_DIR_OK" = false ]; then
    echo "WARNING: Cannot create or write to /etc/ssh directory."
    echo "Skipping SSH configuration setup."
    echo "You may need to configure SSH manually."
    echo
else
    # Check for required files
    MISSING_FILES=""

    # Check config files
    if [ ! -f /etc/ssh/ssh_config ]; then
        MISSING_FILES="$MISSING_FILES ssh_config"
    fi
    if [ ! -f /etc/ssh/sshd_config ]; then
        MISSING_FILES="$MISSING_FILES sshd_config"
    fi
    if [ ! -f /etc/ssh/moduli ]; then
        MISSING_FILES="$MISSING_FILES moduli"
    fi

    # Check key files
    MISSING_KEYS=false
    if [ ! -f /etc/ssh/ssh_host_rsa_key ] \
           || [ ! -f /etc/ssh/ssh_host_rsa_key.pub ] \
           || [ ! -f /etc/ssh/ssh_host_ecdsa_key ] \
           || [ ! -f /etc/ssh/ssh_host_ecdsa_key.pub ] \
           || [ ! -f /etc/ssh/ssh_host_ed25519_key ] \
           || [ ! -f /etc/ssh/ssh_host_ed25519_key.pub ]; then
        MISSING_KEYS=true
    fi

    # Check sshd user and group
    MISSING_SSHD_USER=false
    MISSING_SSHD_GROUP=false
    if ! id sshd > /dev/null 2>&1; then
        MISSING_SSHD_USER=true

        # We only care about there being a sshd group if we're going to be creating a sshd user.
        if ! getent group sshd > /dev/null 2>&1; then
            MISSING_SSHD_GROUP=true
        fi
    fi

    # Check host key permissions
    KEYS_TO_FIX=""
    KEYS_WITH_BAD_PERMS=""
    for keyfile in /etc/ssh/ssh_host_*_key; do
        # Skip if file doesn't exist or ends in .pub
        [ -f "$keyfile" ] || continue
        case "$keyfile" in
            *.pub) continue ;;
        esac

        # Get file permissions and group
        if [ -f "$keyfile" ]; then
            mode=$(stat -c '%a' "$keyfile" 2>/dev/null)
            group=$(stat -c '%G' "$keyfile" 2>/dev/null)

            if [ "$mode" != "600" ]; then
                if [ "$mode" = "640" ] && [ "$group" = "ssh_keys" ]; then
                    # Red Hat quirk - safe to fix
                    KEYS_TO_FIX="$KEYS_TO_FIX $keyfile"
                else
                    # Unknown permission issue - warn but don't fix
                    KEYS_WITH_BAD_PERMS="$KEYS_WITH_BAD_PERMS $keyfile"
                fi
            fi
        fi
    done

    # Phase 2: Reporting and Prompting
    # =================================

    if [ -z "$MISSING_FILES" ] && [ "$MISSING_KEYS" = false ] \
           && [ "$MISSING_SSHD_USER" = false ] && [ "$MISSING_SSHD_GROUP" = false ] \
           && [ -z "$KEYS_TO_FIX" ] && [ -z "$KEYS_WITH_BAD_PERMS" ]; then
        # Everything is ready
        echo "Found complete SSH configuration in /etc/ssh:"
        echo "  - Configuration files (ssh_config, sshd_config, moduli)"
        echo "  - Host keys (RSA, ECDSA, ED25519)"
        echo "  - sshd privilege separation user"
        echo "  - Host key permissions are correct (mode 0600)"
        echo "No SSH setup needed."
    else
        # Something is missing - report what we'll do
        echo "SSH configuration needs setup. The following will be created:"
        echo

        if [ -n "$MISSING_FILES" ]; then
            echo "  Configuration files (will copy from /opt/fil/share/examples/ssh/):"
            for file in $MISSING_FILES; do
                echo "    - /etc/ssh/$file"
            done
        fi

        if [ "$MISSING_KEYS" = true ]; then
            echo "  Host keys (will generate with /opt/fil/bin/ssh-keygen -A):"
            echo "    - /etc/ssh/ssh_host_rsa_key{,.pub}"
            echo "    - /etc/ssh/ssh_host_ecdsa_key{,.pub}"
            echo "    - /etc/ssh/ssh_host_ed25519_key{,.pub}"
        fi

        if [ "$MISSING_SSHD_GROUP" = true ]; then
            echo "  - sshd privilege separation group"
        fi

        if [ "$MISSING_SSHD_USER" = true ]; then
            echo "  - sshd privilege separation user"
        fi

        if [ -n "$KEYS_TO_FIX" ]; then
            echo
            echo "  Host key permissions (will fix Red Hat quirk - changing mode to 0600):"
            echo "    The following keys are currently mode 0640 with group ssh_keys."
            echo "    This is an old Red Hat configuration that has been tested and works"
            echo "    fine with mode 0600. Changing to 0600 is more secure and compatible."
            for keyfile in $KEYS_TO_FIX; do
                echo "    - $keyfile"
            done
        fi

        if [ -n "$KEYS_WITH_BAD_PERMS" ]; then
            echo
            echo "  WARNING: Host keys with unexpected permissions (will NOT modify):"
            echo "    The following keys do not have mode 0600 and do not match the known"
            echo "    Red Hat pattern (0640 with group ssh_keys). You should manually change"
            echo "    these to mode 0600 if you want sshd to accept them:"
            for keyfile in $KEYS_WITH_BAD_PERMS; do
                mode=$(stat -c '%a' "$keyfile" 2>/dev/null)
                group=$(stat -c '%G' "$keyfile" 2>/dev/null)
                echo "    - $keyfile (mode $mode, group $group)"
            done
        fi

        echo
        echo "Type YES (in all caps) to proceed with SSH setup, or anything else to skip:"
        read ssh_response

        # Phase 3: Action
        # ================

        if [ "$ssh_response" = "YES" ]; then
            echo
            echo "Setting up SSH configuration..."

            # Disable exit-on-error for this section - we want to report errors but continue
            set +e

            # Copy missing config files
            if [ -n "$MISSING_FILES" ]; then
                echo "Copying configuration files..."
                for file in $MISSING_FILES; do
                    if cp -v /opt/fil/share/examples/ssh/$file /etc/ssh/$file 2>/dev/null; then
                        echo "  Copied /etc/ssh/$file"
                    else
                        echo "  WARNING: Failed to copy $file"
                    fi
                done
            fi

            # Generate missing keys
            if [ "$MISSING_KEYS" = true ]; then
                echo "Generating SSH host keys..."
                if /opt/fil/bin/ssh-keygen -A 2>/dev/null; then
                    echo "  Host keys generated successfully"
                else
                    echo "  WARNING: Failed to generate some host keys"
                fi
            fi

            # Create sshd group if needed
            if [ "$MISSING_SSHD_GROUP" = true ]; then
                if groupadd sshd 2>/dev/null; then
                    echo "Created sshd group"
                else
                    echo "WARNING: Failed to create sshd group"
                fi
            fi

            # Create sshd user if needed
            if [ "$MISSING_SSHD_USER" = true ]; then
                if useradd -c 'sshd PrivSep' \
                        -d /opt/fil/var/lib/sshd \
                        -g sshd \
                        -s /bin/false \
                        sshd 2>/dev/null; then
                    echo "Created sshd privilege separation user"
                else
                    echo "WARNING: Failed to create sshd user"
                fi
            fi

            # Fix host key permissions
            if [ -n "$KEYS_TO_FIX" ]; then
                echo "Fixing host key permissions..."
                for keyfile in $KEYS_TO_FIX; do
                    if chmod 0600 "$keyfile" 2>/dev/null; then
                        echo "  Changed $keyfile to mode 0600"
                    else
                        echo "  WARNING: Failed to change permissions on $keyfile"
                    fi
                done
            fi

            # Display warning about unfixable permissions
            if [ -n "$KEYS_WITH_BAD_PERMS" ]; then
                echo
                echo "WARNING: Some host keys have unexpected permissions that were not modified."
                echo "Please manually fix the following keys by running:"
                for keyfile in $KEYS_WITH_BAD_PERMS; do
                    echo "  chmod 0600 $keyfile"
                done
            fi

            # Re-enable exit-on-error
            set -e

            echo "SSH setup complete."
        else
            echo "Skipping SSH setup."
        fi
    fi
fi

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
