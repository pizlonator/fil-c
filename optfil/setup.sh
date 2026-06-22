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

VERSION="0.680"

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
    echo
    echo "    acl       attr      bash     binutils bzip2  coreutils curl     diff    "
    echo "    find      flex      gawk     git      glibc  grep      gzip     icu4c   "
    echo "    keyutils  krb5      less     libaudit libc++ libedit   libevent libidn2 "
    echo "    libpsl    libtasn1  libuv    lz4      m4     make      mg       nghttp2 "
    echo "    openssh   openssl   p11-kit  pam      patch  patchelf  pcre2    pkgconf "
    echo "    procps-ng psmisc    readline rsync    sed    selinux   sudo     tar     "
    echo "    tmux      unistring wget     xxhash   xz     zlib      zstd     "
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
echo "Checking if SELinux labels are needed..."
echo

# Tracks whether the Installation Complete section should suggest using
# /opt/fil/sbin/sshd under systemd, and whether the systemd setup phase
# below should even attempt to run. Default false; the SELinux block sets
# it to true when (a) SELinux is not active at all, or (b) every label
# rule succeeded.
SSHD_SELINUX_OK=false
SSHD_SSH_OK=true

set +e

# SELinux labeling for /opt/fil binaries and libraries
# ====================================================
# We use semanage to register persistent file context entries and
# restorecon to apply them. Labels survive future restorecon runs and full
# SELinux relabels because they live in the policy database.

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
# The function body is wrapped in (...) so it runs in its own subshell,
# meaning intermediate assignments never leak even if a future caller
# forgets to use command substitution.
selinux_type_of_label() (
    case "$1" in
        ""|"?"|"(null)"|"(unknown)") return 1 ;;
    esac
    remainder="${1#*:}"
    remainder="${remainder#*:}"
    printf '%s\n' "${remainder%%:*}"
)

# Register a persistent SELinux file context with semanage. Treats
# "already defined" / "already exists" / "conflicts with existing" as
# success since semanage -a is otherwise not idempotent.
# Args: type, semanage path/regex, description (for messages).
#
# The semanage call is run under LC_ALL=C so that the message-matching
# below works on systems with localized output. The match patterns are
# the literal phrases semanage prints (with quoted spaces) rather than
# broad "*already*" substrings, so unrelated errors aren't swallowed.
#
# Note on variable names: shell has no per-function scope, so locals
# leak to callers. Each helper that may be called by another helper uses
# names prefixed by the helper's role (e.g. register_*) so that callers
# can freely use their own names without collision.
selinux_register_context() {
    register_type=$1
    register_path=$2
    register_description=$3
    register_error=$(LC_ALL=C semanage fcontext -a -t "$register_type" "$register_path" 2>&1)
    register_return_code=$?
    if [ "$register_return_code" != 0 ]; then
        case "$register_error" in
            *"already defined"*|*"already exists"*|*"conflicts with existing"*)
                register_return_code=0
                ;;
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
    file_restorecon_rc=$?
    if [ "$file_restorecon_rc" != 0 ]; then
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
    dir_restorecon_rc=$?
    if [ "$dir_restorecon_rc" != 0 ]; then
        echo "WARNING: restorecon -R failed for $dir_description:"
        echo "    $dir_restorecon_error"
        return 1
    fi
    # restorecon -v prints a line per relabel; we don't try to infer
    # "nothing changed" from empty output because other policy rules in
    # the recursive walk can also produce output unrelated to ours.
    echo "Applied or verified persistent SELinux label '$dir_expected_type' for $dir_description."
    return 0
}

# --- SELinux main block ---

# Detect whether SELinux is loaded and active on this kernel. Prefer
# selinuxenabled when available because it is the canonical "is SELinux
# in effect" check; fall back to reading /sys/fs/selinux/enforce
# directly so we still work on systems where libselinux-utils is not
# installed but the kernel does have SELinux mounted.
SELINUX_KERNEL_ACTIVE=false
SELINUX_ENFORCING=unknown
if command -v selinuxenabled >/dev/null 2>&1; then
    if selinuxenabled 2>/dev/null; then
        SELINUX_KERNEL_ACTIVE=true
    fi
elif [ -d /sys/fs/selinux ] && [ -r /sys/fs/selinux/enforce ]; then
    SELINUX_KERNEL_ACTIVE=true
fi
if [ "$SELINUX_KERNEL_ACTIVE" = true ] && [ -r /sys/fs/selinux/enforce ]; then
    # Trim trailing whitespace/newline; some kernels emit a single byte,
    # some append a newline.
    enforce_state=$(tr -d '[:space:]' < /sys/fs/selinux/enforce 2>/dev/null)
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
    # Probe for the canonical system path for each label reference. Use
    # both -e (file exists, follows symlinks) and -h (is a symlink, even
    # if dangling) so that broken-symlink references are surfaced by the
    # validator rather than silently skipped here.
    selinux_first_existing() {
        for candidate; do
            if [ -e "$candidate" ] || [ -h "$candidate" ]; then
                printf '%s\n' "$candidate"
                return 0
            fi
        done
        return 1
    }

    # All of these references are essential to SELinux labeling. The
    # systemd phase below does NOT use SYS_SSHD - it reads the actual
    # path the existing unit uses straight from systemctl. SYS_SSHD is
    # purely the reference for determining the sshd_exec_t label type.
    SYS_SSHD=$(selinux_first_existing \
        /usr/sbin/sshd \
        /usr/bin/sshd \
        /usr/libexec/openssh/sshd \
        /usr/lib/openssh/sshd \
        /sbin/sshd)

    SYS_CHKPWD=$(selinux_first_existing \
        /usr/sbin/unix_chkpwd \
        /sbin/unix_chkpwd)

    SYS_LOADER=$(selinux_first_existing \
        /lib64/ld-linux-x86-64.so.2 \
        /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
        /usr/lib64/ld-linux-x86-64.so.2 \
        /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
        /lib/ld-linux-x86-64.so.2)

    # For lib_t we probe a list of widely-shipped libraries rather than
    # libc, because some distributions' SELinux policies use a more
    # specific type for libc itself (e.g. libc_so_t or ld_so_t) even
    # though ordinary shared libraries still get lib_t.
    SYS_LIB=$(selinux_first_existing \
        /lib64/libz.so.1 \
        /lib/x86_64-linux-gnu/libz.so.1 \
        /usr/lib64/libz.so.1 \
        /usr/lib/x86_64-linux-gnu/libz.so.1 \
        /lib64/libm.so.6 \
        /lib/x86_64-linux-gnu/libm.so.6 \
        /usr/lib64/libcrypt.so.1 \
        /usr/lib/x86_64-linux-gnu/libcrypt.so.1)

    # Tell the user what we found before we try to do anything, so a
    # missing reference is reported up-front instead of as one of N
    # buried per-rule warnings inside the labeling loop.
    echo "System reference paths for SELinux labels:"
    echo "  sshd binary:        ${SYS_SSHD:-NOT FOUND}"
    echo "  unix_chkpwd binary: ${SYS_CHKPWD:-NOT FOUND}"
    echo "  dynamic loader:     ${SYS_LOADER:-NOT FOUND}"
    echo "  shared library:     ${SYS_LIB:-NOT FOUND}"

    # All four references are essential. We cannot label the Fil-C
    # equivalents without knowing what type the system uses for the
    # corresponding role, and shipping the wrong type (or no type) is
    # not safer than refusing - sshd will simply fail to start under
    # systemd in confusing ways. SYS_CHKPWD looks "optional" because
    # it's only invoked from PAM password auth, but the Fil-C
    # /opt/fil/sbin/unix_chkpwd needs the right SELinux type in order
    # for the sshd_t domain to actually exec it; without that, PAM
    # auth from sshd fails on SELinux systems.
    selinux_refs_essential_missing=false
    if [ -z "$SYS_SSHD" ]; then
        selinux_refs_essential_missing=true
    fi
    if [ -z "$SYS_CHKPWD" ]; then
        selinux_refs_essential_missing=true
    fi
    if [ -z "$SYS_LOADER" ]; then
        selinux_refs_essential_missing=true
    fi
    if [ -z "$SYS_LIB" ]; then
        selinux_refs_essential_missing=true
    fi

    if [ "$selinux_refs_essential_missing" = true ]; then
        echo
        echo "WARNING: Essential SELinux reference paths could not be found"
        echo "(see NOT FOUND lines above). SELinux labeling for /opt/fil"
        echo "cannot proceed without them, so this step is being marked as"
        echo "failed. /opt/fil/sbin/sshd is unlikely to start under systemd"
        echo "until the labels are applied. After installing the missing"
        echo "packages (typically openssh for sshd, pam for unix_chkpwd,"
        echo "glibc for the loader, zlib/libz for the lib reference)"
        echo "retry with:"
        echo "    ./setup.sh --ssh-setup"
        # SSHD_SELINUX_OK stays false; the systemd block will see this
        # and skip itself.
    else
        echo
        echo "Applying persistent SELinux labels for /opt/fil binaries and libraries..."

        selinux_attempts_succeeded=0
        selinux_attempts_failed=0

        # Run one rule. Updates the counters above and returns 0 on success.
        selinux_run_rule() {
            if "$@"; then
                selinux_attempts_succeeded=$((selinux_attempts_succeeded + 1))
                return 0
            else
                selinux_attempts_failed=$((selinux_attempts_failed + 1))
                return 1
            fi
        }

        # /opt/fil/sbin/sshd -> sshd_exec_t.
        selinux_run_rule selinux_label_file \
            "/opt/fil/sbin/sshd" \
            "$SYS_SSHD" \
            sshd_exec_t \
            "/opt/fil/sbin/sshd" \
            "/opt/fil/sbin/sshd"

        # /opt/fil/sbin/unix_chkpwd -> chkpwd_exec_t. Needed for PAM password
        # checking when sshd runs in its sandboxed domain. SYS_CHKPWD is
        # guaranteed non-empty here because it was validated as essential
        # above; if the Fil-C target itself is somehow missing from the
        # extracted tarball the rule is skipped (a clean install will
        # never hit this branch).
        if [ -e /opt/fil/sbin/unix_chkpwd ]; then
            selinux_run_rule selinux_label_file \
                "/opt/fil/sbin/unix_chkpwd" \
                "$SYS_CHKPWD" \
                chkpwd_exec_t \
                "/opt/fil/sbin/unix_chkpwd" \
                "/opt/fil/sbin/unix_chkpwd"
        fi

        # Library rule MUST be registered BEFORE the loader rule. libselinux's
        # selabel_lookup_common walks file_contexts.local from end to
        # beginning, returning the first matching entry. Whichever rule is
        # registered LAST is checked FIRST. We want the loader-specific rule
        # to win for the loader file, so we register the broad library rule
        # first and the literal-path loader rule second.

        # /opt/fil/lib/*.so* -> lib_t (recursive).
        selinux_run_rule selinux_label_recursive \
            "/opt/fil/lib shared libraries" \
            "$SYS_LIB" \
            lib_t \
            '/opt/fil/lib/.+\.so(\..+)?' \
            "/opt/fil/lib"

        # /opt/fil/lib/ld-yolo-x86_64.so -> ld_so_t. Registered after the
        # library rule (see comment above) so that semanage's most-recent
        # entry wins for the loader file at restorecon time.
        selinux_run_rule selinux_label_file \
            "/opt/fil/lib/ld-yolo-x86_64.so (loader)" \
            "$SYS_LOADER" \
            ld_so_t \
            '/opt/fil/lib/ld-yolo-x86_64\.so' \
            "/opt/fil/lib/ld-yolo-x86_64.so"

        selinux_attempts_total=$((selinux_attempts_succeeded + selinux_attempts_failed))

        if [ "$selinux_attempts_failed" = 0 ]; then
            SSHD_SELINUX_OK=true
            echo "SELinux labeling complete ($selinux_attempts_succeeded rules applied" \
                 "or already in place)."
        else
            echo
            echo "WARNING: $selinux_attempts_failed of $selinux_attempts_total SELinux label"
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
fi

set -e
echo
echo "Checking SSH configuration..."
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
            # may fail at runtime if /opt/fil/var/lib/sshd is missing. The
            # mode is 0711 to match the convention OpenSSH expects for its
            # privsep chroot (some OpenSSH versions refuse to start if the
            # privsep home is unexpectedly group/world writable).
            if [ ! -d /opt/fil/var/lib/sshd ]; then
                mkdir_err=$(mkdir -p /opt/fil/var/lib/sshd 2>&1)
                if [ $? = 0 ]; then
                    chmod 0711 /opt/fil/var/lib/sshd 2>/dev/null
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

    if [ "$SSHD_SSH_OK" = true ]; then
        echo
        echo "Testing sshd configuration (/opt/fil/sbin/sshd -t)..."
        echo
        if /opt/fil/sbin/sshd -t; then
            echo "sshd configuration confirmed!"
        else
            echo
            echo "Cannot run sshd! If you want to use sshd, fix the errors above and run"
            echo "./setup.sh --ssh-setup"
            SSHD_SSH_OK=false
        fi
    fi
fi

echo
echo "Checking systemd sshd service integration..."
echo

# Tracks the systemd setup outcome for the final summary.
#   na      = systemd is not running, or no action was applicable
#   ok      = either already configured, or we configured it successfully
#   skipped = configuration was needed and offered, but declined / not done
#   failed  = we attempted but something went wrong
SSHD_SYSTEMD_STATUS=na

set +e

# --- systemd helpers ---

# Is a unit's effective ExecStart already configured to run
# /opt/fil/sbin/sshd? systemctl show -p ExecStart prints the property as
# a structured value like 'ExecStart={ path=/usr/sbin/sshd ; argv[]=...';
# we match the specific path= and argv[]= fields rather than the whole
# line so that a unit which only *mentions* the path elsewhere does not
# false-positive.
systemd_unit_uses_fil_sshd() {
    show=$(systemctl show -p ExecStart "$1" 2>/dev/null)
    case "$show" in
        *"path=/opt/fil/sbin/sshd"*|*"argv[]=/opt/fil/sbin/sshd"*) return 0 ;;
    esac
    return 1
}

# Is a unit currently active? Avoid --quiet because that flag is newer
# than the systemd version on some long-lived enterprise distros; check
# the exit status with stdout redirected to /dev/null instead.
systemd_unit_active() {
    systemctl is-active "$1" >/dev/null 2>&1
}

# Extract the executable path that a unit's first ExecStart entry uses.
# 'systemctl show -p ExecStart' prints a structured value containing a
# 'path=...' field which is the source-of-truth path the unit will
# actually run; that may differ from any path we guessed by probing
# the filesystem (Arch/Omarchy puts sshd in /usr/bin/sshd even when
# /usr/sbin/sshd resolves to the same binary by symlink, and openSUSE
# uses /usr/lib/openssh/sshd, etc.). Prints the path on stdout, or
# nothing if the unit has no ExecStart with a 'path=' field.
systemd_unit_sshd_path() {
    systemctl show -p ExecStart "$1" 2>/dev/null \
        | sed -n 's/.*path=\([^ ;]*\).*/\1/p' \
        | head -1
}

# Join systemd unit-file line continuations. Lines that end with `\`
# (optionally followed by whitespace) are concatenated with the next
# line. The output stream has one logical directive per line, which is
# what the drop-in generator wants to scan.
systemd_join_continuations() {
    awk '
        /\\[[:space:]]*$/ {
            sub(/\\[[:space:]]*$/, "")
            accumulator = accumulator $0
            next
        }
        { print accumulator $0; accumulator = "" }
        END { if (accumulator != "") print accumulator }
    '
}

# Generate the drop-in body for one existing unit. Stdin is the
# systemctl-cat output (with continuations already joined); $1 is the
# sshd path to look for (read from systemd_unit_sshd_path - never
# guessed). For each Exec{Start,StartPre,StartPost,Reload,Stop,StopPost}
# line whose value mentions that path, emit a `Key=` clear followed by
# a `Key=...replaced` line. The sed substitution is anchored at
# start-of-line, '=', or whitespace and ended at whitespace or
# end-of-line so that a path embedded in a config argument (e.g.
# -f /tmp/usr/sbin/sshd.conf) is not corrupted.
systemd_emit_dropin_lines() {
    sshd_path=$1
    # Escape ERE metacharacters that could appear in unusual sshd paths
    # before splicing into the substitution pattern. We use sed -E (ERE)
    # below; the '|' delimiter conflicts with ERE alternation, so we use
    # '#' instead and escape any literal '#' in the path.
    sshd_path_for_sed=$(printf '%s\n' "$sshd_path" \
        | sed 's#[][\\/.&^$*?+(){}|#]#\\&#g')
    while IFS= read -r line; do
        trimmed=$(printf '%s' "$line" | sed 's/^[[:space:]]*//')
        case "$trimmed" in
            ExecStart=*|ExecStartPre=*|ExecStartPost=*|ExecReload=*|ExecStop=*|ExecStopPost=*)
                case "$trimmed" in
                    *"$sshd_path"*)
                        key="${trimmed%%=*}"
                        # Match the path only when preceded by start-of-line,
                        # '=', or whitespace, and followed by whitespace or
                        # end-of-line; this leaves paths embedded inside an
                        # argument (e.g. -f /tmp/usr/sbin/sshd.conf) alone.
                        replaced=$(printf '%s' "$trimmed" \
                            | sed -E "s#(^|[=[:space:]])${sshd_path_for_sed}([[:space:]]|\$)#\\1/opt/fil/sbin/sshd\\2#g")
                        printf '%s=\n%s\n' "$key" "$replaced"
                        ;;
                esac
                ;;
        esac
    done
}

# --- systemd main block ---

if [ ! -d /run/systemd/system ]; then
    echo "systemd is not running on this system - /opt/fil/sbin/sshd will not be"
    echo "wired into any init system. You can run it directly or set up your own"
    echo "service definition by hand."
else
    # Detect existing unit and socket.
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

    if [ "$SSHD_SELINUX_OK" != true ] || [ "$SSHD_SSH_OK" != true ]; then
        # If the SELinux or SSH config steps did not complete cleanly, the new sshd is
        # unlikely to start, so bail.
        echo "Skipping systemd sshd service integration because an earlier step did not"
        echo "complete cleanly:"
        if [ "$SSHD_SELINUX_OK" != true ]; then
            echo "  - SELinux labeling for /opt/fil"
        fi
        if [ "$SSHD_SSH_OK" != true ]; then
            echo "  - SSH configuration / key / privilege-separation setup"
        fi
        echo "(see the warnings above). Wiring sshd into systemd before these are sorted"
        echo "out could leave you with a broken sshd service. After fixing the underlying"
        echo "issue you can retry with: ./setup.sh --ssh-setup"
        SSHD_SYSTEMD_STATUS=skipped
    elif [ -n "$EXISTING_UNIT" ] && systemd_unit_uses_fil_sshd "$EXISTING_UNIT"; then
        # Idempotent fast-path: if an existing unit is already configured to
        # run /opt/fil/sbin/sshd, just report that and optionally restart.
        # This check runs *before* the SELinux/SSH gating so that a re-run of
        # --ssh-setup which had a transient SELinux failure still reports the
        # systemd state correctly instead of pretending it does not know.
        echo "$EXISTING_UNIT is already configured to use /opt/fil/sbin/sshd -"
        echo "no systemd override needed."
        SSHD_SYSTEMD_STATUS=ok
        if systemd_unit_active "$EXISTING_UNIT"; then
            echo "$EXISTING_UNIT is currently active - restarting to pick up any changes."
            restart_err=$(systemctl restart "$EXISTING_UNIT" 2>&1)
            restart_rc=$?
            if [ "$restart_rc" = 0 ] && systemd_unit_active "$EXISTING_UNIT"; then
                echo "Restarted $EXISTING_UNIT."
            else
                echo "WARNING: $EXISTING_UNIT did not come back up cleanly after restart."
                if [ -n "$restart_err" ]; then
                    echo "    $restart_err"
                fi
                echo "Check 'journalctl -u $EXISTING_UNIT' for details."
                SSHD_SYSTEMD_STATUS=failed
            fi
        fi
    elif [ -n "$EXISTING_UNIT" ]; then
        # Case E: existing service unit, points at distribution sshd.
        # Read the actual path the unit's ExecStart uses straight from
        # systemctl; this is the only path we can safely redirect from,
        # because it's exactly the literal string we need to find in
        # 'systemctl cat' output. Do NOT fall back to SYS_SSHD or any
        # hardcoded path - on systems where the two disagree (Arch /
        # Omarchy: unit uses /usr/bin/sshd, /usr/sbin/sshd is a
        # different symlink to the same binary; or openSUSE where the
        # unit uses /usr/lib/openssh/sshd) the wrong path produces a
        # silent no-op drop-in.
        EXISTING_UNIT_SSHD_PATH=$(systemd_unit_sshd_path "$EXISTING_UNIT")
        if [ -z "$EXISTING_UNIT_SSHD_PATH" ]; then
            echo "WARNING: Could not extract the sshd path from $EXISTING_UNIT"
            echo "(systemctl show -p ExecStart returned no path= field). Cannot"
            echo "generate a drop-in without knowing what string to replace."
            echo "Inspect with:"
            echo "    systemctl cat $EXISTING_UNIT"
            echo "    systemctl show -p ExecStart $EXISTING_UNIT"
            echo "and apply the redirect by hand following sshd_setup.md."
            SSHD_SYSTEMD_STATUS=failed
        else
            echo "Found existing $EXISTING_UNIT pointing at the distribution sshd"
            echo "($EXISTING_UNIT_SSHD_PATH). Will create a systemd drop-in at:"
            echo "    /etc/systemd/system/$EXISTING_UNIT.d/fil-c.conf"
            echo "that redirects ExecStart/ExecStartPre/ExecReload lines that mention"
            echo "$EXISTING_UNIT_SSHD_PATH to /opt/fil/sbin/sshd. Other directives in the"
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
                echo "Type YES (in all caps) to proceed with systemd sshd service integration, or"
                echo "anything else to skip. (You can re-run this later with: ./setup.sh --ssh-setup)"
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

                # Generate the drop-in body in a variable first so we can
                # sanity-check that we actually emitted Exec* override lines.
                # If we didn't, writing an empty body and restarting would
                # silently leave the distro sshd in place (no redirect).
                DROP_IN_BODY=$(systemctl cat "$EXISTING_UNIT" 2>/dev/null \
                                   | systemd_join_continuations \
                                   | systemd_emit_dropin_lines "$EXISTING_UNIT_SSHD_PATH")

                case "$DROP_IN_BODY" in
                    *Exec*=*)
                        drop_in_ok=true
                        ;;
                    *)
                        drop_in_ok=false
                        ;;
                esac

                if [ "$drop_in_ok" = false ]; then
                    echo "WARNING: Scanned $EXISTING_UNIT for Exec* lines mentioning"
                    echo "'$EXISTING_UNIT_SSHD_PATH' but found none. The drop-in would"
                    echo "have no effect (the distribution sshd would keep running)."
                    echo
                    echo "This usually means the unit refers to sshd by a different path"
                    echo "than the installer expected. Inspect the unit with:"
                    echo "    systemctl cat $EXISTING_UNIT"
                    echo "and apply the redirect by hand following sshd_setup.md."
                    SSHD_SYSTEMD_STATUS=failed
                elif ! mkdir_err=$(mkdir -p "$DROP_IN_DIR" 2>&1); then
                    echo "WARNING: Failed to create $DROP_IN_DIR: $mkdir_err"
                    SSHD_SYSTEMD_STATUS=failed
                else
                    {
                        echo "# Generated by Fil-C setup.sh"
                        echo "# Remove with: systemctl revert $EXISTING_UNIT"
                        echo "[Service]"
                        printf '%s\n' "$DROP_IN_BODY"
                    } > "$DROP_IN"
                    echo "Wrote $DROP_IN with contents:"
                    sed 's/^/    /' "$DROP_IN"
                    echo

                    reload_err=$(systemctl daemon-reload 2>&1)
                    reload_rc=$?
                    if [ "$reload_rc" != 0 ]; then
                        echo "WARNING: 'systemctl daemon-reload' failed: $reload_err"
                        SSHD_SYSTEMD_STATUS=failed
                    else
                        echo "Reloaded systemd."
                        restart_err=$(systemctl restart "$EXISTING_UNIT" 2>&1)
                        restart_rc=$?
                        if [ "$restart_rc" != 0 ]; then
                            echo "WARNING: Failed to restart $EXISTING_UNIT: $restart_err"
                            echo "Check 'journalctl -u $EXISTING_UNIT' for details."
                            SSHD_SYSTEMD_STATUS=failed
                        elif ! systemd_unit_active "$EXISTING_UNIT"; then
                            echo "WARNING: After restart, $EXISTING_UNIT is not active."
                            echo "Check 'journalctl -u $EXISTING_UNIT' for details."
                            SSHD_SYSTEMD_STATUS=failed
                        elif ! systemd_unit_uses_fil_sshd "$EXISTING_UNIT"; then
                            echo "WARNING: After restart, $EXISTING_UNIT does not appear to be"
                            echo "running /opt/fil/sbin/sshd. Inspect:"
                            echo "    systemctl show -p ExecStart $EXISTING_UNIT"
                            echo "Check 'journalctl -u $EXISTING_UNIT' for details."
                            SSHD_SYSTEMD_STATUS=failed
                        else
                            echo "Verified: $EXISTING_UNIT is now active and running" \
                                 "/opt/fil/sbin/sshd."
                            SSHD_SYSTEMD_STATUS=ok
                        fi
                    fi
                fi
            else
                SSHD_SYSTEMD_STATUS=skipped
            fi
        fi
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
            echo "Type YES (in all caps) to proceed with systemd sshd service integration, or"
            echo "anything else to skip. (You can re-run this later with: ./setup.sh --ssh-setup)"
            read -r systemd_response
        elif [ "$FULL_SETUP" = true ]; then
            systemd_response=YES
        else
            systemd_response=NO
            echo "Skipping systemd sshd service integration (--unattended without --full-setup)."
            echo "You can run systemd setup later with: ./setup.sh --ssh-setup"
        fi

        if [ "$systemd_response" = "YES" ]; then
            echo
            # Type=notify is supported by the Fil-C OpenSSH build (it
            # includes the Debian systemd-notify patches), and means
            # systemctl start only returns once sshd has bound its
            # listening sockets, so a crash-on-bind is reported as a
            # start failure rather than silently succeeding.
            cat > /etc/systemd/system/sshd.service <<'EOF'
# Generated by Fil-C setup.sh
[Unit]
Description=Fil-C OpenSSH server daemon
Documentation=man:sshd(8) man:sshd_config(5)
After=network.target

[Service]
Type=notify
ExecStart=/opt/fil/sbin/sshd -D
KillMode=process
Restart=on-failure
RestartSec=10s

[Install]
WantedBy=multi-user.target
EOF
            echo "Wrote /etc/systemd/system/sshd.service"

            reload_err=$(systemctl daemon-reload 2>&1)
            reload_rc=$?
            if [ "$reload_rc" != 0 ]; then
                echo "WARNING: 'systemctl daemon-reload' failed: $reload_err"
                SSHD_SYSTEMD_STATUS=failed
            else
                enable_err=$(systemctl enable sshd.service 2>&1)
                enable_rc=$?
                start_err=$(systemctl start sshd.service 2>&1)
                start_rc=$?
                if [ "$start_rc" != 0 ] || ! systemd_unit_active sshd.service; then
                    echo "WARNING: sshd.service did not come up cleanly."
                    if [ -n "$start_err" ]; then
                        echo "    $start_err"
                    fi
                    echo "Check 'journalctl -u sshd.service' for details."
                    SSHD_SYSTEMD_STATUS=failed
                elif [ "$enable_rc" != 0 ]; then
                    echo "WARNING: sshd.service is running but enable failed:"
                    echo "    $enable_err"
                    echo "It will not start automatically on next boot. Resolve the"
                    echo "underlying problem and retry with: ./setup.sh --ssh-setup"
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
