#!/bin/sh
#
# Copyright (c) 2025-2026 Epic Games, Inc. All Rights Reserved.
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e

VERSION="0.679"

usage() {
    echo "Usage: ./setup.sh [OPTIONS]"
    echo
    echo "Install the Fil-C /opt/fil distribution version $VERSION."
    echo
    echo "Fil-C is a memory-safe implementation of C and C++ that prevents all memory"
    echo "safety errors (out-of-bounds access, use-after-free, type confusion, etc.)"
    echo "while maintaining full C/C++ compatibility."
    echo
    echo "/opt/fil is a self-contained directory containing the Fil-C compiler"
    echo "(filcc/fil++), its runtime, and a collection of common programs and libraries"
    echo "that have been compiled with Fil-C for memory safety. After installation, add"
    echo "/opt/fil/bin to your PATH to use the memory-safe tools."
    echo
    echo "This installer extracts fil.tar.xz to /opt/fil and optionally configures SSH"
    echo "(host keys, sshd user/group, config files) for the memory-safe OpenSSH server"
    echo "included in the distribution."
    echo
    echo "Options:"
    echo "  -h, --help          Show this help message and exit"
    echo "  -u, --unattended    Non-interactive install: extract to /opt/fil without any"
    echo "                      prompts, declining all optional system configuration"
    echo "                      (SSH setup, etc.)"
    echo "      --full-setup    Used with -u/--unattended: also accept all optional system"
    echo "                      configuration without prompting. Requires --unattended."
    echo "                      WARNING: This may modify your system outside of /opt/fil,"
    echo "                      including creating users/groups, writing to /etc/ssh,"
    echo "                      changing file permissions, and editing systemd"
    echo "                      configuration. Use with care."
    echo "      --ssh-setup     Re-run only the SSH-related parts of setup against an"
    echo "                      existing /opt/fil installation. Does not extract the"
    echo "                      tarball. Useful for retrying SSH/SELinux/systemd setup"
    echo "                      after fixing a prerequisite. Idempotent: skips steps"
    echo "                      that are already done."
    exit 0
}

UNATTENDED=false
FULL_SETUP=false
SSH_SETUP_ONLY=false

OPTS=$(getopt -o hu -l help,unattended,full-setup,ssh-setup -n './setup.sh' -- "$@") || {
    echo "Try './setup.sh --help' for more information."
    exit 1
}
eval set -- "$OPTS"

while true; do
    case "$1" in
        -h|--help)
            usage
            ;;
        -u|--unattended)
            UNATTENDED=true
            shift
            ;;
        --full-setup)
            FULL_SETUP=true
            shift
            ;;
        --ssh-setup)
            SSH_SETUP_ONLY=true
            shift
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "ERROR: Unexpected option: $1"
            echo "Try './setup.sh --help' for more information."
            exit 1
            ;;
    esac
done

if [ "$FULL_SETUP" = true ] && [ "$UNATTENDED" = false ]; then
    echo "ERROR: --full-setup requires -u/--unattended."
    echo "Try './setup.sh --help' for more information."
    exit 1
fi

echo "================================================================================"
if [ "$SSH_SETUP_ONLY" = true ]; then
    heading="Fil-C $VERSION SSH Setup (Re-run)"
else
    heading="Fil-C $VERSION /opt/fil Distribution"
fi
printf "%*s%s\n" $(((80 - ${#heading}) / 2)) "" "$heading"
echo "================================================================================"
echo
if [ "$SSH_SETUP_ONLY" = true ]; then
    echo "This is the --ssh-setup mode of the Fil-C installer. It will re-run only the"
    echo "SSH-related parts of setup against the existing /opt/fil installation. It will"
    echo "not extract any files. Each step is idempotent and will skip work that has"
    echo "already been done."
    echo
    echo "THIS SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND."
    echo "********************************************************************************"
    echo
else
    echo "This distribution includes the Fil-C compiler (filcc/fil++) and runtime and"
    echo "these memory safe programs and libraries compiled with Fil-C:"
    echo "    acl         attr        bash        binutils    bzip2       coreutils"
    echo "    curl        diff        find        flex        gawk        git"
    echo "    glibc       grep        gzip        icu4c       keyutils    kerberos5"
    echo "    less        libaudit    libc++      libevent    libidn2     libpsl"
    echo "    libselinux  libtasn1    libuv       lz4         m4          make"
    echo "    mg          nghttp2     openssl     openssh     p11-kit     patch"
    echo "    pam         pcre2       pkgconf     procps-ng   psmisc      rsync"
    echo "    sed         sudo        tar         tmux        unistring   wget"
    echo "    xxhash      xz          zlib        zstd"
    echo
    echo "THIS SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND."
    echo "********************************************************************************"
    echo
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: This installer must be run as root (use sudo or run as root user)."
    echo "Try './setup.sh --help' for more information."
    exit 1
fi

if [ "$SSH_SETUP_ONLY" = true ]; then
    if [ ! -d /opt/fil ]; then
        echo "ERROR: /opt/fil does not exist - run ./setup.sh without --ssh-setup first."
        echo "Try './setup.sh --help' for more information."
        exit 1
    fi
    if [ ! -x /opt/fil/sbin/sshd ]; then
        echo "ERROR: /opt/fil/sbin/sshd does not exist or is not executable."
        echo "Your /opt/fil installation may be incomplete or corrupted."
        echo "Try './setup.sh --help' for more information."
        exit 1
    fi
else
    if [ -e /opt/fil ]; then
        echo "ERROR: /opt/fil already exists!"
        echo "Please remove or move it before running this installer:"
        echo "  rm -rf /opt/fil"
        echo "    or"
        echo "  mv /opt/fil /opt/fil.backup"
        echo
        echo "If you only want to re-run SSH/SELinux/systemd setup against an existing"
        echo "/opt/fil installation, use './setup.sh --ssh-setup' instead."
        echo "Try './setup.sh --help' for more information."
        exit 1
    fi

    echo "This installer will:"
    if [ ! -d /opt ]; then
        echo "  - Create /opt directory (doesn't currently exist)"
    fi
    echo "  - Extract fil.tar.xz to /opt, creating /opt/fil (requires about 1.3 GB)"
    echo

    if [ "$UNATTENDED" = false ]; then
        echo "Type YES (in all caps) to proceed with installation, or anything else to abort:"
        read -r response

        if [ "$response" != "YES" ]; then
            echo "Installation aborted."
            exit 1
        fi
    fi

    echo
    echo "Extracting fil.tar.xz to /opt..."

    if [ ! -f fil.tar.xz ]; then
        echo "ERROR: fil.tar.xz not found in current directory."
        echo "Try './setup.sh --help' for more information."
        exit 1
    fi

    if [ ! -d /opt ]; then
        echo "Creating /opt directory..."
        mkdir -p /opt
    fi

    INSTALLER_DIR=$(pwd)
    cd /opt
    tar -xf "$INSTALLER_DIR/fil.tar.xz"
fi

echo
echo "Checking SSH configuration..."

# SELinux labeling for /opt/fil binaries and libraries
# ====================================================
# We use semanage to register persistent file context entries and
# restorecon to apply them. Labels survive future restorecon runs and full
# SELinux relabels because they live in the policy database.
#
# Tracks whether the Installation Complete section should suggest using
# /opt/fil/sbin/sshd under systemd, and whether the systemd setup phase
# below should even attempt to run. Default false; the SELinux block sets
# it to true when (a) SELinux is not active at all, or (b) every label
# rule succeeded.
SSHD_SELINUX_OK=false
SSHD_SSH_OK=true

set +e

# --- SELinux helpers ---

# Return the SELinux context of $1 (following symlinks). Prints "?" or
# empty if the file has no security context.
selinux_label_of() {
    stat -L -c '%C' "$1" 2>/dev/null
}

# Extract the type field from a label like 'user:role:type:level' or
# 'user:role:type'. Prints the type on stdout. Returns 1 if the label is
# empty / a sentinel like "?" / "(null)" / "(unknown)".
#
# Variables in this function do not leak to callers because it is always
# invoked via command substitution, which runs in a subshell.
selinux_type_of_label() {
    case "$1" in
        ""|"?"|"(null)"|"(unknown)") return 1 ;;
    esac
    type_label_remainder="${1#*:}"
    type_label_remainder="${type_label_remainder#*:}"
    printf '%s\n' "${type_label_remainder%%:*}"
}

# Register a persistent SELinux file context with semanage. Treats
# "already defined" as success since semanage -a is otherwise not
# idempotent.
# Args: type, semanage path/regex, description (for messages).
#
# Note on variable names: shell has no per-function scope, so locals
# leak to callers. Each helper that may be called by another helper uses
# names prefixed by the helper's role (e.g. register_*) so that callers
# can freely use their own names without collision.
selinux_register_context() {
    register_type=$1
    register_path=$2
    register_description=$3
    register_error=$(semanage fcontext -a -t "$register_type" "$register_path" 2>&1)
    register_return_code=$?
    if [ "$register_return_code" != 0 ]; then
        case "$register_error" in
            *already*defined*|*already*exists*|*duplicate*) register_return_code=0 ;;
        esac
    fi
    if [ "$register_return_code" != 0 ]; then
        echo "WARNING: semanage failed for $register_description:"
        echo "    $register_error"
        return 1
    fi
    return 0
}

# Validate that $1 (system reference) has SELinux type $2 (expected). If
# not, print a warning naming $3 (target description) and return 1.
# Returns 0 only if the reference is present, labeled, and has the
# expected type.
selinux_validate_ref_type() {
    validate_reference=$1
    validate_expected_type=$2
    validate_description=$3
    if [ -z "$validate_reference" ]; then
        echo "WARNING: No system reference found for $validate_description - cannot"
        echo "determine the expected SELinux type. Skipping persistent label."
        return 1
    fi
    if [ ! -e "$validate_reference" ] && [ ! -h "$validate_reference" ]; then
        echo "WARNING: System reference $validate_reference does not exist - cannot"
        echo "determine the expected SELinux type for $validate_description. Skipping"
        echo "persistent label."
        return 1
    fi
    validate_reference_label=$(selinux_label_of "$validate_reference")
    if ! validate_actual_type=$(selinux_type_of_label "$validate_reference_label"); then
        echo "WARNING: $validate_reference has no SELinux label ('$validate_reference_label')"
        echo "- cannot determine the expected type for $validate_description. Skipping"
        echo "persistent label."
        return 1
    fi
    if [ "$validate_actual_type" != "$validate_expected_type" ]; then
        echo "WARNING: $validate_reference has SELinux type '$validate_actual_type'"
        echo "(this installer expects '$validate_expected_type'). The SELinux policy on"
        echo "this system does not appear to be one this installer recognizes. Skipping"
        echo "persistent label for $validate_description. You can apply the label"
        echo "manually if you know what type to use:"
        echo "    semanage fcontext -a -t '<your_type>' '$validate_reference'"
        echo "    restorecon -v <path-or-dir-here>"
        return 1
    fi
    return 0
}

# Apply persistent SELinux label for a single file target.
# Args:
#   $1 description (for messages)
#   $2 system reference path (used to read the expected label)
#   $3 expected SELinux type (sanity-checked against the reference)
#   $4 semanage path/regex (where the rule is registered)
#   $5 target file (where restorecon is run)
selinux_label_file() {
    file_description=$1
    file_reference=$2
    file_expected_type=$3
    file_semanage_path=$4
    file_target=$5
    if [ ! -e "$file_target" ]; then
        echo "WARNING: $file_target does not exist - skipping $file_description."
        return 1
    fi
    selinux_validate_ref_type "$file_reference" "$file_expected_type" "$file_description" \
        || return 1
    file_reference_label=$(selinux_label_of "$file_reference")
    file_current_label=$(selinux_label_of "$file_target")
    if [ "$file_current_label" = "$file_reference_label" ]; then
        # Already correctly labeled; still register persistence so the
        # label survives a future restorecon or relabel.
        selinux_register_context "$file_expected_type" "$file_semanage_path" "$file_description" \
            || return 1
        echo "$file_description already has SELinux label '$file_reference_label'" \
             "(no change needed)."
        return 0
    fi
    selinux_register_context "$file_expected_type" "$file_semanage_path" "$file_description" \
        || return 1
    file_restorecon_error=$(restorecon -v "$file_target" 2>&1)
    if [ $? != 0 ]; then
        echo "WARNING: restorecon failed for $file_description:"
        echo "    $file_restorecon_error"
        return 1
    fi
    echo "Applied persistent SELinux label '$file_reference_label' to $file_description."
    return 0
}

# Apply persistent SELinux label recursively for a directory. Same args
# as selinux_label_file but the final arg is the directory; restorecon
# is run with -R. No per-file idempotency fast-path; semanage and
# restorecon are themselves idempotent.
selinux_label_recursive() {
    dir_description=$1
    dir_reference=$2
    dir_expected_type=$3
    dir_semanage_path=$4
    dir_target=$5
    if [ ! -d "$dir_target" ]; then
        echo "WARNING: $dir_target does not exist - skipping $dir_description."
        return 1
    fi
    selinux_validate_ref_type "$dir_reference" "$dir_expected_type" "$dir_description" \
        || return 1
    selinux_register_context "$dir_expected_type" "$dir_semanage_path" "$dir_description" \
        || return 1
    dir_restorecon_error=$(restorecon -R -v "$dir_target" 2>&1)
    if [ $? != 0 ]; then
        echo "WARNING: restorecon -R failed for $dir_description:"
        echo "    $dir_restorecon_error"
        return 1
    fi
    if [ -z "$dir_restorecon_error" ]; then
        echo "$dir_description already has correct SELinux labels (no change needed)."
    else
        echo "Applied persistent SELinux label '$dir_expected_type' for $dir_description."
    fi
    return 0
}

# --- SELinux main block ---

# Detect whether SELinux is actually loaded by the kernel. This is more
# reliable than selinuxenabled alone, which may not be installed even when
# SELinux is active (e.g. policycoreutils not installed).
SELINUX_KERNEL_ACTIVE=false
SELINUX_ENFORCING=unknown
if [ -d /sys/fs/selinux ] && [ -e /sys/fs/selinux/enforce ]; then
    SELINUX_KERNEL_ACTIVE=true
    enforce_state=$(cat /sys/fs/selinux/enforce 2>/dev/null)
    case "$enforce_state" in
        1) SELINUX_ENFORCING=true ;;
        0) SELINUX_ENFORCING=false ;;
    esac
fi

if [ "$SELINUX_KERNEL_ACTIVE" = false ]; then
    echo "SELinux is not active on this system - no SELinux labeling needed for"
    echo "/opt/fil."
    SSHD_SELINUX_OK=true
elif ! command -v semanage >/dev/null 2>&1 || ! command -v restorecon >/dev/null 2>&1; then
    echo "WARNING: SELinux is active on this system, but the persistent labeling"
    echo "tools (semanage and/or restorecon) are not available. The installer"
    echo "cannot label /opt/fil/sbin/sshd or the Fil-C runtime libraries"
    echo "automatically."
    echo
    echo "Install your distribution's SELinux user-space tools and re-run this"
    echo "step. On Rocky/RHEL/Fedora:"
    echo
    echo "    dnf install policycoreutils policycoreutils-python-utils"
    echo "    ./setup.sh --ssh-setup"
else
    echo "Applying persistent SELinux labels for /opt/fil binaries and libraries..."

    # Find the canonical system path for each label reference. Different
    # distros put the loader and libc in different places, so we probe a
    # short list.
    SYS_SSHD=/usr/sbin/sshd

    SYS_CHKPWD=
    for _p in /usr/sbin/unix_chkpwd /sbin/unix_chkpwd; do
        if [ -e "$_p" ]; then SYS_CHKPWD=$_p; break; fi
    done

    SYS_LOADER=
    for _p in /lib64/ld-linux-x86-64.so.2 \
              /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
              /usr/lib64/ld-linux-x86-64.so.2 \
              /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
              /lib/ld-linux-x86-64.so.2; do
        if [ -e "$_p" ]; then SYS_LOADER=$_p; break; fi
    done

    SYS_LIBC=
    for _p in /lib64/libc.so.6 \
              /lib/x86_64-linux-gnu/libc.so.6 \
              /usr/lib64/libc.so.6 \
              /usr/lib/x86_64-linux-gnu/libc.so.6 \
              /lib/libc.so.6; do
        if [ -e "$_p" ]; then SYS_LIBC=$_p; break; fi
    done

    selinux_targets_failed=0
    selinux_targets_total=0

    # /opt/fil/sbin/sshd -> sshd_exec_t (matching /usr/sbin/sshd).
    selinux_targets_total=$((selinux_targets_total + 1))
    selinux_label_file \
        "/opt/fil/sbin/sshd" \
        "$SYS_SSHD" \
        sshd_exec_t \
        "/opt/fil/sbin/sshd" \
        "/opt/fil/sbin/sshd" \
        || selinux_targets_failed=$((selinux_targets_failed + 1))

    # /opt/fil/sbin/unix_chkpwd -> chkpwd_exec_t. Needed for PAM password
    # checking when sshd runs in its sandboxed domain.
    if [ -e /opt/fil/sbin/unix_chkpwd ]; then
        selinux_targets_total=$((selinux_targets_total + 1))
        selinux_label_file \
            "/opt/fil/sbin/unix_chkpwd" \
            "$SYS_CHKPWD" \
            chkpwd_exec_t \
            "/opt/fil/sbin/unix_chkpwd" \
            "/opt/fil/sbin/unix_chkpwd" \
            || selinux_targets_failed=$((selinux_targets_failed + 1))
    fi

    # /opt/fil/lib/ld-yolo-x86_64.so -> ld_so_t. This is the Fil-C dynamic
    # loader; it needs the loader type rather than the generic library
    # type so that exec-time domain transitions work. We register this
    # rule with a literal-path regex so SELinux's specificity matching
    # picks it over the broader library rule below.
    selinux_targets_total=$((selinux_targets_total + 1))
    selinux_label_file \
        "/opt/fil/lib/ld-yolo-x86_64.so (loader)" \
        "$SYS_LOADER" \
        ld_so_t \
        '/opt/fil/lib/ld-yolo-x86_64\.so' \
        "/opt/fil/lib/ld-yolo-x86_64.so" \
        || selinux_targets_failed=$((selinux_targets_failed + 1))

    # /opt/fil/lib/*.so* -> lib_t (recursive). The loader rule above is
    # more specific and overrides this for the loader itself.
    selinux_targets_total=$((selinux_targets_total + 1))
    selinux_label_recursive \
        "/opt/fil/lib shared libraries" \
        "$SYS_LIBC" \
        lib_t \
        '/opt/fil/lib/.+\.so(\..+)?' \
        "/opt/fil/lib" \
        || selinux_targets_failed=$((selinux_targets_failed + 1))

    if [ "$selinux_targets_failed" = 0 ]; then
        SSHD_SELINUX_OK=true
        echo "SELinux labeling complete ($selinux_targets_total rules applied or already in place)."
    else
        echo
        echo "WARNING: $selinux_targets_failed of $selinux_targets_total SELinux label"
        echo "rules could not be applied (see warnings above)."
        if [ "$SELINUX_ENFORCING" = false ]; then
            echo "SELinux is in permissive mode on this system, so /opt/fil/sbin/sshd"
            echo "may still run but will produce denials in the audit log."
        else
            echo "/opt/fil/sbin/sshd is unlikely to work correctly under systemd until"
            echo "the failing rules are resolved. After fixing the underlying issue"
            echo "(often: installing missing user-space tools, or matching your"
            echo "distribution's policy types), retry with:"
            echo "    ./setup.sh --ssh-setup"
        fi
    fi
fi

set -e
echo

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
    echo "After fixing the issue you can retry with: ./setup.sh --ssh-setup"
    echo
    SSHD_SSH_OK=false
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

        if [ "$UNATTENDED" = false ]; then
            echo "Type YES (in all caps) to proceed with SSH setup, or anything else to skip."
            echo "(You can always re-run SSH setup later with: ./setup.sh --ssh-setup)"
            read -r ssh_response
        elif [ "$FULL_SETUP" = true ]; then
            ssh_response=YES
        else
            ssh_response=NO
            echo "Skipping SSH setup (--unattended without --full-setup)."
            echo "You can run SSH setup later with: ./setup.sh --ssh-setup"
        fi

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
                    cp_err=$(cp -v "/opt/fil/share/examples/ssh/$file" "/etc/ssh/$file" 2>&1)
                    if [ $? = 0 ]; then
                        echo "  Copied /etc/ssh/$file"
                    else
                        echo "  WARNING: Failed to copy $file: $cp_err"
                        SSHD_SSH_OK=false
                    fi
                done
            fi

            # Ensure the sshd privilege separation home directory exists. sshd
            # may fail at runtime if /opt/fil/var/lib/sshd is missing.
            if [ ! -d /opt/fil/var/lib/sshd ]; then
                mkdir_err=$(mkdir -p /opt/fil/var/lib/sshd 2>&1)
                if [ $? = 0 ]; then
                    chmod 0755 /opt/fil/var/lib/sshd 2>/dev/null
                    echo "Created /opt/fil/var/lib/sshd (sshd privilege separation home)"
                else
                    echo "WARNING: Failed to create /opt/fil/var/lib/sshd: $mkdir_err"
                    SSHD_SSH_OK=false
                fi
            fi

            # Generate missing keys
            if [ "$MISSING_KEYS" = true ]; then
                echo "Generating SSH host keys..."
                keygen_err=$(/opt/fil/bin/ssh-keygen -A 2>&1)
                if [ $? = 0 ]; then
                    echo "  Host keys generated successfully"
                else
                    echo "  WARNING: Failed to generate some host keys: $keygen_err"
                    SSHD_SSH_OK=false
                fi
            fi

            # Create sshd group if needed. Track success so we do not try to
            # create the sshd user with a -g sshd that does not exist.
            SSHD_GROUP_OK=true
            if [ "$MISSING_SSHD_GROUP" = true ]; then
                groupadd_err=$(groupadd sshd 2>&1)
                if [ $? = 0 ]; then
                    echo "Created sshd group"
                else
                    echo "WARNING: Failed to create sshd group: $groupadd_err"
                    SSHD_GROUP_OK=false
                    SSHD_SSH_OK=false
                fi
            fi

            # Create sshd user if needed
            if [ "$MISSING_SSHD_USER" = true ]; then
                if [ "$SSHD_GROUP_OK" = false ]; then
                    echo "WARNING: Skipping sshd user creation - sshd group is missing."
                    SSHD_SSH_OK=false
                else
                    useradd_err=$(useradd -c 'sshd PrivSep' \
                            -d /opt/fil/var/lib/sshd \
                            -g sshd \
                            -s /bin/false \
                            sshd 2>&1)
                    if [ $? = 0 ]; then
                        echo "Created sshd privilege separation user"
                    else
                        echo "WARNING: Failed to create sshd user: $useradd_err"
                        SSHD_SSH_OK=false
                    fi
                fi
            fi

            # Fix host key permissions
            if [ -n "$KEYS_TO_FIX" ]; then
                echo "Fixing host key permissions..."
                for keyfile in $KEYS_TO_FIX; do
                    chmod_err=$(chmod 0600 "$keyfile" 2>&1)
                    if [ $? = 0 ]; then
                        echo "  Changed $keyfile to mode 0600"
                    else
                        echo "  WARNING: Failed to change permissions on $keyfile: $chmod_err"
                        SSHD_SSH_OK=false
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
            echo "You can re-run SSH setup later with: ./setup.sh --ssh-setup"
            SSHD_SSH_OK=false
        fi
    fi
fi

echo
echo "Checking systemd integration..."

# Tracks the systemd setup outcome for the final summary.
#   na      = systemd is not running, or no action was applicable
#   ok      = either already configured, or we configured it successfully
#   skipped = configuration was needed and offered, but declined / not done
#   failed  = we attempted but something went wrong
SSHD_SYSTEMD_STATUS=na

set +e

if [ "$SSHD_SELINUX_OK" != true ] || [ "$SSHD_SSH_OK" != true ]; then
    echo "Skipping systemd setup because an earlier step did not complete cleanly:"
    if [ "$SSHD_SELINUX_OK" != true ]; then
        echo "  - SELinux labeling for /opt/fil/sbin/sshd"
    fi
    if [ "$SSHD_SSH_OK" != true ]; then
        echo "  - SSH configuration / key / privilege-separation setup"
    fi
    echo "(see the warnings above). Wiring sshd into systemd before these are sorted"
    echo "out could leave you with a broken sshd service. After fixing the underlying"
    echo "issue you can retry with: ./setup.sh --ssh-setup"
    SSHD_SYSTEMD_STATUS=skipped
elif [ ! -d /run/systemd/system ]; then
    echo "systemd is not running on this system - /opt/fil/sbin/sshd will not be"
    echo "wired into any init system. You can run it directly or set up your own"
    echo "service definition by hand."
else
    # systemd is running. Look for existing sshd unit and socket.
    EXISTING_UNIT=""
    for unit in sshd.service ssh.service; do
        if systemctl cat "$unit" >/dev/null 2>&1; then
            EXISTING_UNIT="$unit"
            break
        fi
    done

    EXISTING_SOCKET=""
    for sock in sshd.socket ssh.socket; do
        if systemctl cat "$sock" >/dev/null 2>&1; then
            EXISTING_SOCKET="$sock"
            break
        fi
    done

    if [ -n "$EXISTING_UNIT" ]; then
        # Case E: existing service unit. Idempotency: is the effective
        # ExecStart already pointing at /opt/fil/sbin/sshd?
        CURRENT_EXEC=$(systemctl show -p ExecStart "$EXISTING_UNIT" 2>/dev/null | head -1)
        case "$CURRENT_EXEC" in
            *"/opt/fil/sbin/sshd"*)
                echo "$EXISTING_UNIT is already configured to use /opt/fil/sbin/sshd -"
                echo "no systemd override needed."
                SSHD_SYSTEMD_STATUS=ok
                # If the service is currently active, restart it so any other
                # changes from this run (e.g. a fresh SELinux label) take effect.
                if systemctl is-active --quiet "$EXISTING_UNIT" 2>/dev/null; then
                    echo "$EXISTING_UNIT is currently active - restarting to pick up any changes."
                    restart_err=$(systemctl restart "$EXISTING_UNIT" 2>&1)
                    if [ $? = 0 ]; then
                        echo "Restarted $EXISTING_UNIT."
                    else
                        echo "WARNING: Failed to restart $EXISTING_UNIT: $restart_err"
                        echo "Check 'journalctl -u $EXISTING_UNIT' for details."
                    fi
                fi
                ;;
            *)
                echo "Found existing $EXISTING_UNIT pointing at the distribution sshd."
                echo "Will create a systemd drop-in at:"
                echo "    /etc/systemd/system/$EXISTING_UNIT.d/fil-c.conf"
                echo "that redirects ExecStart/ExecStartPre/ExecReload lines that mention"
                echo "/usr/sbin/sshd to /opt/fil/sbin/sshd. Other directives in the"
                echo "distribution unit (EnvironmentFile, KillMode, hardening flags, etc.)"
                echo "are left unchanged."
                echo
                echo "WARNING: After writing the drop-in, this will run 'systemctl"
                echo "daemon-reload' and 'systemctl restart $EXISTING_UNIT', which will"
                echo "RESTART THE SSH SERVICE. Under normal conditions (the distribution"
                echo "unit uses KillMode=process) restarting sshd does not interrupt"
                echo "already-connected SSH sessions, only the main listener. Make sure"
                echo "you have a way back in if something goes wrong before proceeding."
                echo

                if [ "$UNATTENDED" = false ]; then
                    echo "Type YES (in all caps) to proceed with systemd setup, or anything"
                    echo "else to skip. (You can re-run this later with: ./setup.sh --ssh-setup)"
                    read -r systemd_response
                elif [ "$FULL_SETUP" = true ]; then
                    systemd_response=YES
                else
                    systemd_response=NO
                    echo "Skipping systemd setup (--unattended without --full-setup)."
                    echo "You can run systemd setup later with: ./setup.sh --ssh-setup"
                fi

                if [ "$systemd_response" = "YES" ]; then
                    echo
                    DROP_IN_DIR="/etc/systemd/system/$EXISTING_UNIT.d"
                    DROP_IN="$DROP_IN_DIR/fil-c.conf"

                    mkdir_err=$(mkdir -p "$DROP_IN_DIR" 2>&1)
                    if [ $? != 0 ]; then
                        echo "WARNING: Failed to create $DROP_IN_DIR: $mkdir_err"
                        SSHD_SYSTEMD_STATUS=failed
                    else
                        {
                            echo "# Generated by Fil-C setup.sh"
                            echo "# Remove with: systemctl revert $EXISTING_UNIT"
                            echo "[Service]"
                            systemctl cat "$EXISTING_UNIT" 2>/dev/null | while IFS= read -r line; do
                                trimmed=$(printf '%s' "$line" | sed 's/^[[:space:]]*//')
                                case "$trimmed" in
                                    Exec*=*"/usr/sbin/sshd"*)
                                        key="${trimmed%%=*}"
                                        replaced=$(printf '%s' "$trimmed" | sed 's|/usr/sbin/sshd|/opt/fil/sbin/sshd|g')
                                        printf '%s=\n%s\n' "$key" "$replaced"
                                        ;;
                                esac
                            done
                        } > "$DROP_IN"
                        echo "Wrote $DROP_IN with contents:"
                        sed 's/^/    /' "$DROP_IN"
                        echo

                        reload_err=$(systemctl daemon-reload 2>&1)
                        if [ $? != 0 ]; then
                            echo "WARNING: 'systemctl daemon-reload' failed: $reload_err"
                            SSHD_SYSTEMD_STATUS=failed
                        else
                            echo "Reloaded systemd."
                            restart_err=$(systemctl restart "$EXISTING_UNIT" 2>&1)
                            if [ $? != 0 ]; then
                                echo "WARNING: Failed to restart $EXISTING_UNIT: $restart_err"
                                echo "Check 'journalctl -u $EXISTING_UNIT' for details."
                                SSHD_SYSTEMD_STATUS=failed
                            else
                                echo "Restarted $EXISTING_UNIT."
                                NEW_EXEC=$(systemctl show -p ExecStart "$EXISTING_UNIT" 2>/dev/null | head -1)
                                case "$NEW_EXEC" in
                                    *"/opt/fil/sbin/sshd"*)
                                        echo "Verified: $EXISTING_UNIT is now configured to run /opt/fil/sbin/sshd."
                                        SSHD_SYSTEMD_STATUS=ok
                                        ;;
                                    *)
                                        echo "WARNING: After restart, $EXISTING_UNIT does not appear to be"
                                        echo "running /opt/fil/sbin/sshd. ExecStart line is:"
                                        echo "    $NEW_EXEC"
                                        echo "Check 'journalctl -u $EXISTING_UNIT' for details."
                                        SSHD_SYSTEMD_STATUS=failed
                                        ;;
                                esac
                            fi
                        fi
                    fi
                else
                    SSHD_SYSTEMD_STATUS=skipped
                fi
                ;;
        esac
    elif [ -n "$EXISTING_SOCKET" ]; then
        # Case D: socket but no service. Weird, refuse to touch it.
        echo "WARNING: Found $EXISTING_SOCKET but no sshd.service or ssh.service."
        echo "This is an unusual configuration that the installer does not know how"
        echo "to handle automatically. /opt/fil/sbin/sshd was NOT set up to start"
        echo "automatically. You will need to wire it in by hand."
        SSHD_SYSTEMD_STATUS=skipped
    else
        # Case C: no service unit at all. Offer to create a minimal one.
        echo "No existing sshd.service or ssh.service found on this systemd."
        echo "Will create a minimal /etc/systemd/system/sshd.service that runs"
        echo "/opt/fil/sbin/sshd, enable it to start on boot, and start it now."
        echo
        echo "WARNING: This will create and START a new sshd. Make sure you have a"
        echo "way back in if something goes wrong."
        echo

        if [ "$UNATTENDED" = false ]; then
            echo "Type YES (in all caps) to proceed with systemd setup, or anything"
            echo "else to skip. (You can re-run this later with: ./setup.sh --ssh-setup)"
            read -r systemd_response
        elif [ "$FULL_SETUP" = true ]; then
            systemd_response=YES
        else
            systemd_response=NO
            echo "Skipping systemd setup (--unattended without --full-setup)."
            echo "You can run systemd setup later with: ./setup.sh --ssh-setup"
        fi

        if [ "$systemd_response" = "YES" ]; then
            echo
            cat > /etc/systemd/system/sshd.service <<'EOF'
# Generated by Fil-C setup.sh
[Unit]
Description=Fil-C OpenSSH server daemon
Documentation=man:sshd(8) man:sshd_config(5)
After=network.target

[Service]
ExecStart=/opt/fil/sbin/sshd -D
KillMode=process
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF
            echo "Wrote /etc/systemd/system/sshd.service"

            reload_err=$(systemctl daemon-reload 2>&1)
            if [ $? != 0 ]; then
                echo "WARNING: 'systemctl daemon-reload' failed: $reload_err"
                SSHD_SYSTEMD_STATUS=failed
            else
                enable_err=$(systemctl enable sshd.service 2>&1)
                if [ $? != 0 ]; then
                    echo "WARNING: Failed to enable sshd.service: $enable_err"
                fi
                start_err=$(systemctl start sshd.service 2>&1)
                if [ $? != 0 ]; then
                    echo "WARNING: Failed to start sshd.service: $start_err"
                    echo "Check 'journalctl -u sshd.service' for details."
                    SSHD_SYSTEMD_STATUS=failed
                else
                    echo "Started and enabled sshd.service."
                    SSHD_SYSTEMD_STATUS=ok
                fi
            fi
        else
            SSHD_SYSTEMD_STATUS=skipped
        fi
    fi
fi

set -e

echo
echo "================================================================================"
if [ "$SSH_SETUP_ONLY" = true ]; then
    echo "                         SSH Setup Complete!"
else
    echo "                         Installation Complete!"
fi
echo "================================================================================"
echo
if [ "$SSH_SETUP_ONLY" = false ]; then
    echo "To use Fil-C, add /opt/fil/bin to your PATH:"
    echo "  export PATH=/opt/fil/bin:\$PATH"
    echo
fi

case "$SSHD_SYSTEMD_STATUS" in
    ok)
        echo "The Fil-C OpenSSH server (/opt/fil/sbin/sshd) is now wired into systemd."
        echo "See sshd_setup.md in this directory for what was configured and how to"
        echo "roll back."
        ;;
    skipped)
        if [ "$SSHD_SELINUX_OK" != true ] || [ "$SSHD_SSH_OK" != true ]; then
            echo "The Fil-C OpenSSH server (/opt/fil/sbin/sshd) was NOT wired into systemd"
            echo "because an earlier setup step did not complete cleanly (see above)."
            echo "After fixing the underlying issue you can retry with:"
            echo "    ./setup.sh --ssh-setup"
        else
            echo "The Fil-C OpenSSH server (/opt/fil/sbin/sshd) was NOT wired into systemd"
            echo "(setup was skipped). You can re-run it later with:"
            echo "    ./setup.sh --ssh-setup"
            echo "or set it up by hand following sshd_setup.md."
        fi
        ;;
    failed)
        echo "WARNING: systemd setup for /opt/fil/sbin/sshd did not complete cleanly"
        echo "(see messages above). After fixing the underlying issue you can retry with:"
        echo "    ./setup.sh --ssh-setup"
        echo "or set it up by hand following sshd_setup.md."
        ;;
    na|*)
        if [ "$SSHD_SELINUX_OK" = true ]; then
            echo "You can run the memory-safe OpenSSH server at /opt/fil/sbin/sshd."
            echo "See sshd_setup.md for guidance on running it under systemd."
        else
            echo "You can use the memory-safe OpenSSH server at:"
            echo "  /opt/fil/sbin/sshd"
            echo
            echo "WARNING: SELinux labeling for /opt/fil/sbin/sshd was not completed"
            echo "(see above). Running it under systemd is unlikely to work until you"
            echo "apply a SELinux label. After fixing this you can retry the SSH"
            echo "setup with:"
            echo "    ./setup.sh --ssh-setup"
        fi
        ;;
esac
echo

if [ "$SSH_SETUP_ONLY" = false ]; then
    echo "To compile C programs with Fil-C:"
    echo "  filcc -o program program.c -g -O"
    echo
    echo "To compile C++ programs with Fil-C++:"
    echo "  fil++ -o program program.cpp -g -O -std=c++20"
    echo
    echo "For more information, see README.md in the distribution directory."
    echo
fi
