#!/bin/bash
#
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

# ===================== BEGIN USAGE =====================
# run_variants.sh -- run the simple-openssl-tests suite under the zegarmistrz
# emulator, sweeping every supported poisoning variant, and produce a TSV of
# raw results plus a human-readable failure report.  By default it runs the
# test binaries that are already built, without building anything; --build
# opts into building first.
#
# Usage:
#   ./run_variants.sh [--build=zunsafe|sarcastic|yolo|both|all] [--jobs=N]
#                     [--timeout=S] [--poison-args='--poison-all']
#                     [--allow-no-poison] [--emulator=PATH]
#                     [--yolo-openssl-prefix=PREFIX] [--output=PREFIX]
#
#   (no --build)    run whatever top-level test_* executables already
#                   exist, without building anything.  Each binary is
#                   classified by the libcrypto ldd resolves it to:
#                   a pizfix libcrypto -> zunsafe, a libcrypto under
#                   projects/sarcastic-openssl -> sarcastic, a system
#                   libcrypto -> yolo, and anything else -> an "other"
#                   group keyed by that libcrypto's directory (those
#                   binaries are run too, and the directory is called out
#                   in the report).  Every non-empty group is swept; groups
#                   with no binaries are absent from the report, whose
#                   header records the per-group classification counts.
#                   If no test binaries exist at all, the script aborts:
#                   build some first with --build=yolo, --build=zunsafe,
#                   or --build=sarcastic.
#   --build         build before sweeping: zunsafe (Fil-C's filcc + the
#                   pizfix OpenSSL), sarcastic (filcc + the in-tree
#                   projects/sarcastic-openssl), yolo (the system compiler,
#                   gcc, + the system OpenSSL), both (= zunsafe sarcastic),
#                   or all (= zunsafe sarcastic yolo).  Multiple builds
#                   rebuild and clean (exact binary names only) in between.
#   --yolo-openssl-prefix=PREFIX
#                   with --build=yolo or --build=all: build the yolo flavor
#                   against the OpenSSL installed at PREFIX instead of the
#                   system one, as `make CC=gcc OPENSSL_HOME=PREFIX
#                   PIZFIX=`.  PREFIX must contain an include/ directory
#                   and a libcrypto under lib/ or lib64/.  PREFIX is
#                   normalized once up front (made absolute, trailing
#                   slashes stripped; a relative PREFIX resolves against
#                   the directory the script was invoked from), so the
#                   build, the report, and the ldd provenance check all
#                   see the same canonical path.  It is an error
#                   without a --build that includes yolo, since it only
#                   affects building.  The trailing PIZFIX= neutralizes
#                   every pizfix-derived path in the Makefile, so no
#                   pizfix -L/-rpath leaks into the yolo binaries.
#   --jobs          parallelism for the per-variant test sweep (default:
#                   nproc/2, capped at 16).
#   --timeout       per-test wall-clock timeout in seconds (default 120).
#   --poison-args   extra --poison-* args handed to every test (default:
#                   --poison-all, which is what the committed report
#                   zegarmistrz-variants-report.{txt,tsv} was produced
#                   with).  Which zegarmistrz variants get tainted is the
#                   sweep's job.
#   --allow-no-poison  permit an empty --poison-args= (see below).
#   --emulator      zegarmistrz binary (default <repo>/projects/zegarmistrz/zegarmistrz).
#   --output        output prefix; writes <PREFIX>.txt and <PREFIX>.tsv
#                   (default /tmp/simple-openssl-variants-<timestamp>).
#
# With an explicitly empty --poison-args='' no buffer is ever poisoned, so
# no variant can ever fault and the sweep degenerates to a slow PASS-only
# no-op.  That is almost certainly a mistake (e.g. a quoting accident), so
# the script prints a loud warning and aborts unless --allow-no-poison was
# also passed.
#
# Exit statuses are DATA here: a test faulting under a poisoning variant is
# a result, not a script failure.  Only build errors and usage errors abort.
# ===================== END USAGE =====================
set -u

# The cwd this script was invoked from, captured BEFORE the cd below: a
# RELATIVE --yolo-openssl-prefix must resolve against the user's invocation
# cwd, not against this script's directory (which the cd makes the cwd).
INVOCATION_CWD="$(pwd -P)"

cd "$(dirname "$0")"
REPO_ROOT="$(cd ../.. && pwd)"

BUILD=""                    # empty = prebuilt mode: run what's already built
YOLO_OPENSSL_PREFIX=""
JOBS="$(($(nproc) / 2))"
[ "$JOBS" -gt 16 ] && JOBS=16
[ "$JOBS" -lt 1 ] && JOBS=1
TIMEOUT=120
POISON_ARGS="--poison-all"
ALLOW_NO_POISON=0
EMU="$REPO_ROOT/projects/zegarmistrz/zegarmistrz"
OUTPUT_PREFIX=""

# Provenance for the report header: how this script was invoked (path +
# full argv), so a report can always be traced back to its run.
SCRIPT_PATH="$(pwd -P)/$(basename "$0")"
ARGV_STR="$(basename "$0")"
for _arg in "$@"; do
    ARGV_STR="$ARGV_STR $(printf '%q' "$_arg")"
done

usage() {
    # Extract the usage comment block, delimited by the BEGIN/END USAGE
    # markers above, from the script file.  Must use $SCRIPT_PATH (computed
    # after the cd at the top of this script), not "$0": "$0" may be a path
    # relative to a cwd this script has already left (e.g. invoking
    # projects/simple-openssl-tests/run_variants.sh --help from the repo
    # root).  Marker-based extraction means editing the copyright header can
    # never again silently truncate --help.
    sed -n '/^# ===================== BEGIN USAGE =====================$/,/^# ===================== END USAGE =====================$/p' "$SCRIPT_PATH" \
        | sed -e '/^# ===================== BEGIN USAGE =====================$/d' \
              -e '/^# ===================== END USAGE =====================$/d' \
              -e 's/^# \{0,1\}//'
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
    --build=*) BUILD="${1#*=}" ;;
    --flavor=*) echo "unknown option: $1 (it was renamed: use --build)" >&2; usage ;;
    --yolo-openssl-prefix=*)
        YOLO_OPENSSL_PREFIX="${1#*=}"
        # Normalize ONCE, here, before the existence checks below and before
        # any other use (the make invocation, the report's build: line, and
        # the ldd provenance grep): strip trailing slashes and make the
        # prefix absolute.  Otherwise a trailing slash makes the ldd grep
        # anchor on "...//lib" while ldd may resolve ".../lib" (or vice
        # versa: a correctly-bound build then prints a false "NOT <prefix>!"
        # warning), and a relative prefix can never match at all, since ldd
        # prints absolute paths.  Relative prefixes resolve against the
        # invocation cwd captured above, not against this script's dir.
        if [ -n "$YOLO_OPENSSL_PREFIX" ]; then
            case "$YOLO_OPENSSL_PREFIX" in
            /*) _yolo_norm="$(realpath -m -- "$YOLO_OPENSSL_PREFIX")" ;;
            *)  _yolo_norm="$(realpath -m -- "$INVOCATION_CWD/$YOLO_OPENSSL_PREFIX")" ;;
            esac
            # If realpath somehow fails, keep the raw value: the validation
            # below then reports it verbatim.
            [ -n "${_yolo_norm:-}" ] && YOLO_OPENSSL_PREFIX="$_yolo_norm"
            unset _yolo_norm
        fi
        ;;
    --jobs=*) JOBS="${1#*=}" ;;
    --timeout=*) TIMEOUT="${1#*=}" ;;
    --poison-args=*) POISON_ARGS="${1#*=}" ;;
    --allow-no-poison) ALLOW_NO_POISON=1 ;;
    --emulator=*) EMU="${1#*=}" ;;
    --output=*) OUTPUT_PREFIX="${1#*=}" ;;
    -h | --help) usage ;;
    *) echo "unknown option: $1" >&2; usage ;;
    esac
    shift
done

case "$BUILD" in
"" | zunsafe | sarcastic | yolo | both | all) ;;
*) echo "bad --build: $BUILD" >&2; usage ;;
esac

# Which flavors does --build ask us to build, in build order?
case "$BUILD" in
both) BUILD_LIST="zunsafe sarcastic" ;;
all) BUILD_LIST="zunsafe sarcastic yolo" ;;
"") BUILD_LIST="" ;;
*) BUILD_LIST="$BUILD" ;;
esac

# --yolo-openssl-prefix only affects building, and only the yolo build.
# Validate it up front: it must point at an installed OpenSSL prefix
# (include/ plus a libcrypto under lib/ or lib64/).
if [ -n "$YOLO_OPENSSL_PREFIX" ]; then
    case " $BUILD_LIST " in
    *" yolo "*) ;;
    *)
        echo "ERROR: --yolo-openssl-prefix only affects building, but this run will not build the yolo flavor (build mode: '${BUILD:-none}').  Pass --build=yolo or --build=all." >&2
        exit 2
        ;;
    esac
    yolo_prefix_err=""
    [ -d "$YOLO_OPENSSL_PREFIX/include" ] || \
        yolo_prefix_err="no include/ directory at $YOLO_OPENSSL_PREFIX"
    yolo_prefix_libdir=""
    if [ -z "$yolo_prefix_err" ]; then
        for d in lib lib64; do
            if [ -e "$YOLO_OPENSSL_PREFIX/$d/libcrypto.so.3" ] || \
               [ -e "$YOLO_OPENSSL_PREFIX/$d/libcrypto.so" ]; then
                yolo_prefix_libdir="$YOLO_OPENSSL_PREFIX/$d"
                break
            fi
        done
        # The Makefile also handles an in-tree OpenSSL whose libcrypto sits
        # at the top of the prefix (that is how projects/sarcastic-openssl
        # is laid out), so accept that too.
        if [ -z "$yolo_prefix_libdir" ] && \
           [ -e "$YOLO_OPENSSL_PREFIX/libcrypto.so.3" ]; then
            yolo_prefix_libdir="$YOLO_OPENSSL_PREFIX"
        fi
        [ -n "$yolo_prefix_libdir" ] || \
            yolo_prefix_err="no libcrypto in $YOLO_OPENSSL_PREFIX/lib or $YOLO_OPENSSL_PREFIX/lib64"
    fi
    if [ -n "$yolo_prefix_err" ]; then
        echo "ERROR: bad --yolo-openssl-prefix=$YOLO_OPENSSL_PREFIX: $yolo_prefix_err" >&2
        exit 2
    fi
    if [ "$yolo_prefix_libdir" = "$YOLO_OPENSSL_PREFIX/lib64" ] && \
       [ ! -e "$YOLO_OPENSSL_PREFIX/lib/libcrypto.so.3" ]; then
        echo "WARNING: $YOLO_OPENSSL_PREFIX has libcrypto only under lib64/: the Makefile links via $YOLO_OPENSSL_PREFIX/lib (or the prefix itself), so the built tests may bind the system libcrypto instead.  A lib/ directory (or a lib -> lib64 symlink) is recommended." >&2
    fi
fi

# An empty poison-args means no buffer is ever poisoned, so no variant can
# ever fault: the sweep could only ever report PASS (or TIMEOUT).  Refuse
# that loudly unless the user insisted with --allow-no-poison.
if [ -z "$POISON_ARGS" ] && [ "$ALLOW_NO_POISON" != 1 ]; then
    cat >&2 <<'EOM'
**********************************************************************
***  WARNING: --poison-args is EMPTY.                              ***
***                                                                ***
***  No buffer will be poisoned, so no zegarmistrz variant can     ***
***  ever fault: this sweep can only ever produce PASS rows, and   ***
***  hours of emulation would prove nothing.                       ***
***                                                                ***
***  Pass --allow-no-poison if an unpoisoned sweep is really what  ***
***  you want; otherwise use the default (--poison-all) or e.g.    ***
***  --poison-args='--poison=key,input'.                           ***
**********************************************************************
EOM
    exit 2
fi

if [ ! -x "$EMU" ]; then
    echo "ERROR: zegarmistrz emulator not found at $EMU" >&2
    echo "Build it first:  cd $REPO_ROOT && ./build_zegarmistrz.sh" >&2
    exit 1
fi

if [ -z "$OUTPUT_PREFIX" ]; then
    OUTPUT_PREFIX="/tmp/simple-openssl-variants-$(date +%Y%m%d-%H%M%S)"
fi
OUT_TXT="$OUTPUT_PREFIX.txt"
OUT_TSV="$OUTPUT_PREFIX.tsv"
SCRATCH="$(mktemp -d /tmp/simple-openssl-variants.XXXXXX)"

# The swept variants, in report order.  Variant 0 ("none") is the control:
# every test must PASS there, since with no variant selected poisoning is a
# no-op and poison options never change what a program computes.
VARIANTS=(
    "none"
    "cannot-branch"
    "cannot-index"
    "cannot-var-time-op"
    "cannot-branch,cannot-index"
    "cannot-branch,cannot-var-time-op"
    "cannot-index,cannot-var-time-op"
    "cannot-branch,cannot-index,cannot-var-time-op"
)
# Short names for the matrix columns (the 7 non-empty variants).
VSHORT=("br" "ix" "vt" "br+ix" "br+vt" "ix+vt" "br+ix+vt")

# Exact-name cleanup of the built test executables.  NEVER "rm -f test_*":
# that would delete the test_*.c sources.
clean_bins() {
    local f
    for f in test_*.c; do
        rm -f -- "${f%.c}"
    done
}

# The top-level test executables that exist right now (never the test_*.c
# sources, which are not executable, and never anything below the top level).
list_bins() {
    local f
    for f in test_*; do
        [ -f "$f" ] || continue
        [ -x "$f" ] || continue
        printf '%s\n' "$f"
    done
}

# The libcrypto a binary binds to, as resolved by ldd (empty if ldd resolves
# no libcrypto at all).
bin_libcrypto() {
    ldd "./$1" 2>/dev/null | awk '$2 == "=>" && $3 ~ /libcrypto/ {print $3; exit}'
}

# Classify one binary by the libcrypto ldd resolves it to.  Prints
# "<group><TAB><libcrypto directory>": zunsafe for a pizfix libcrypto,
# sarcastic for projects/sarcastic-openssl, yolo for a system libcrypto,
# and "other" for anything else (the directory then keys the group; such
# binaries are still swept and reported, with the directory called out).
classify_bin() {
    local res dir group
    res="$(bin_libcrypto "$1")"
    if [ -z "$res" ]; then
        printf 'other\t(unresolved)\n'
        return
    fi
    dir="$(dirname "$res")"
    case "$res" in
    */pizfix/lib/*) group=zunsafe ;;
    */pizfix/lib64/*) group=zunsafe ;;
    *projects/sarcastic-openssl*) group=sarcastic ;;
    /lib/* | /usr/lib/* | /lib64/* | /usr/lib64/* | \
    /usr/local/lib/* | /usr/local/lib64/*) group=yolo ;;
    *) group=other ;;
    esac
    printf '%s\t%s\n' "$group" "$dir"
}

# Which libcrypto the tests currently bind to (the resolved path).
crypto_probe() {
    ldd ./test_aes_cbc 2>/dev/null | awk '$2 == "=>" && $3 ~ /libcrypto/ {print $3; exit}'
}

# The file-safe slug for a group display name: every character outside
# [A-Za-z0-9._-] mapped to '_'.  Different names can map to the same slug
# (other:/x/y vs other:/x_y, other:/a:b vs other:/a_b); since the slug keys
# the group's bins file, result files, and report identity, such a collision
# would silently merge two distinct "other:<dir>" groups and misattribute
# the second group's binaries to the first group's libcrypto directory.  So
# on a collision -- an existing group whose display file holds a DIFFERENT
# name (the g.<slug>.display files double as the slug->name map) -- a short
# hash of the name is folded into the slug, giving each group its own bins/
# report identity.  Names that map to unique slugs (the clean flavors, and
# non-colliding other:<dir>s) get exactly the old, un-hashed slug.
slug_for() {
    local name="$1" base slug salt hsrc
    base="$(printf '%s' "$name" | tr -c 'A-Za-z0-9._-' '_')"
    slug="$base"
    salt=0
    while [ -f "$SCRATCH/g.$slug.display" ] && \
          [ "$(cat "$SCRATCH/g.$slug.display")" != "$name" ]; do
        if [ "$salt" -eq 0 ]; then hsrc="$name"; else hsrc="$name#$salt"; fi
        slug="$base-$(printf '%s' "$hsrc" | cksum | awk '{printf "%08x", $1}')"
        salt=$((salt + 1))
    done
    printf '%s\n' "$slug"
}

# Does the dynamic-linker resolution mention any pizfix path?  Expected (and
# fine) for the Fil-C flavors; a pizfix leak in a yolo build.
ldd_mentions_pizfix() {
    ldd ./test_aes_cbc 2>/dev/null | grep -q '/pizfix/'
}

# One test under one (group, variant).  Runs via xargs -P; writes a result
# file into the scratch dir and leaves the TSV assembly to the parent.
# <gslug> is the file-safe group id used for scratch/result file names;
# <display> is the label written into the TSV row.
run_one() {
    local gslug="$1" display="$2" variant="$3" test="$4" emu="$5" timeout_s="$6" \
        poison_args="$7" scratch="$8"
    local vslug exit_code class note first last out
    vslug="$(printf '%s' "$variant" | tr ',+' '__')"
    out="$scratch/result.$gslug.$vslug.$test.tsv"
    exit_code=0
    # --no-core: classification is by exit code, and the sweep must not litter
    # hundreds of zegarmistrz.core.<name>.<pid> files into the tree.  (For a
    # post-mortem of a specific surprising failure, rerun that one test under
    # the emulator by hand without --no-core.)
    timeout "$timeout_s" "$emu" --no-core "./$test" $poison_args \
        "--poison-variants=$variant" >"$out.raw" 2>&1 || exit_code=$?
    # Belt and braces: drop any core files the emulator may still have written
    # next to the binary (exact emulator core-name prefix only; never test_*).
    rm -f -- "zegarmistrz.core.$test".* 2>/dev/null
    first="$(head -n 1 "$out.raw")"
    last="$(tail -n 1 "$out.raw" | tr '\t\r' '  ' | cut -c1-140)"
    case "$exit_code" in
    0)
        # A real self-skip prints its SKIP line last (and exits 0).  Tests
        # that merely NOTE an unavailable sub-feature in a leading SKIP line
        # still run and pass their remaining checks, so the decision uses
        # the LAST line, not the first.
        if [ "${last#SKIP}" != "$last" ]; then
            class="SKIP"
        else
            class="PASS"
        fi
        ;;
    124) class="TIMEOUT" ;;
    *)
        if [ "$exit_code" -ge 128 ] 2>/dev/null; then
            class="FAULT"
        else
            class="FAIL"
        fi
        ;;
    esac
    note="$last"
    [ -z "$note" ] && note="(no output)"
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$display" "$variant" "$test" "$exit_code" "$class" "$note" >"$out"
}

# Assembles the per-group report section and the (test, variant) failure
# pairs into $3, plus the version-gated-skip entries into $4 (both global
# files).  Args: group slug, results-tsv, global pairs file, global skips
# file.  Group metadata (display label, build provenance, libcrypto
# directory, libcrypto probe) is read from the scratch files the group
# registration wrote.
report_group() {
    local gslug="$1" tsv="$2" pairs="$3" skips="$4"
    awk -F'\t' \
        -v flavor="$(cat "$SCRATCH/g.$gslug.display")" \
        -v buildline="$(cat "$SCRATCH/g.$gslug.build")" \
        -v grpdir="$(cat "$SCRATCH/g.$gslug.dir")" \
        -v crypto="$(cat "$SCRATCH/g.$gslug.crypto")" \
        -v vlist="${VARIANTS[*]}" -v nlist="${NONEMPTY[*]}" \
        -v slist="${VSHORT[*]}" -v pairsfile="$pairs" -v skipsfile="$skips" '
    BEGIN {
        nv = split(vlist, VORD, " ")
        nn = split(nlist, NORD, " ")
        ns = split(slist, SORD, " ")
    }
    {
        v = $2; t = $3; class = $5
        total[v]++
        cnt[v "," class]++
        if (v != "none" && class != "PASS") {
            key = flavor "\t" v "\t" t
            # pcls is read by the fault matrix below (reads create awk array
            # entries, so the output pairs go in a separate array).
            pcls[key] = class
            pout[key] = class
            if (class == "FAULT")
                faulted[t] = 1
        }
        # Remember each CONTROL row class: a variant-row SKIP only means
        # "poisoning caused a skip" if the control did not SKIP too (see the
        # pairs loop in END).
        if (v == "none")
            ctrl[t] = class
        if (v == "none" && class != "PASS")
            control_bad[v "," class]++
    }
    END {
        printf "=== flavor: %s\n", flavor
        printf "build: %s\n", buildline
        if (grpdir != "")
            printf "libcrypto directory: %s\n", grpdir
        printf "libcrypto in use: %s\n", (crypto == "" ? "(probe failed)" : crypto)
        # Control verdict.  A version-gated SKIP is not a poisoning failure
        # (it is decided by which OpenSSL the binary binds, before any
        # poisoned buffer is used), so only FAULT/TIMEOUT/FAIL rows count
        # against the control; SKIPs are reported alongside the PASS count.
        ctotal = total["none"] + 0
        cbad = 0
        cskip = 0
        for (k in control_bad) {
            if (k ~ /,SKIP$/) cskip += control_bad[k]
            else cbad += control_bad[k]
        }
        if (ctotal == 0)
            printf "CONTROL-FAILED: no results recorded for the control (variant none)!\n\n"
        else if (cbad == 0 && cskip > 0)
            printf "control (variant none): %d/%d PASS, %d/%d SKIP (version-gated) -- CONTROL OK\n\n", ctotal - cskip, ctotal, cskip, ctotal
        else if (cbad == 0)
            printf "control (variant none): %d/%d PASS -- CONTROL OK\n\n", ctotal, ctotal
        else
            printf "CONTROL-FAILED: variant none had %d non-PASS of %d -- results below are meaningless!\n\n", cbad + cskip, ctotal
        # Per-variant summaries.
        for (i = 1; i <= nv; i++) {
            v = VORD[i]
            printf "variant %-36s:", v
            printf " PASS %d", cnt[v ",PASS"] + 0
            printf "  FAULT %d", cnt[v ",FAULT"] + 0
            printf "  TIMEOUT %d", cnt[v ",TIMEOUT"] + 0
            printf "  FAIL %d", cnt[v ",FAIL"] + 0
            printf "  SKIP %d\n", cnt[v ",SKIP"] + 0
        }
        # Faulting tests per non-empty variant.
        for (i = 1; i <= nn; i++) {
            v = NORD[i]
            if ((cnt[v ",FAULT"] + 0) == 0 && (cnt[v ",TIMEOUT"] + 0) == 0 &&
                (cnt[v ",FAIL"] + 0) == 0)
                continue
            printf "\n  %s: non-PASS tests:\n", v
            n = 0
            for (key in pout) {
                split(key, kk, "\t")
                if (kk[1] == flavor && kk[2] == v) {
                    printf "    %s (%s)\n", kk[3], pcls[key]
                    n++
                }
            }
            if (n == 0) printf "    (none)\n"
        }
        # Matrix: rows = tests that FAULTed in >= 1 non-empty variant,
        # columns = the non-empty variants; P/F/T/X/S.
        nft = 0
        for (t in faulted) ftlist[++nft] = t
        if (nft > 0) {
            # Insertion sort (portable awk; no gawk asort dependency).
            for (i = 2; i <= nft; i++) {
                key = ftlist[i]
                for (j = i - 1; j > 0 && ftlist[j] > key; j--)
                    ftlist[j + 1] = ftlist[j]
                ftlist[j + 1] = key
            }
            printf "\n  fault matrix (rows: tests that FAULTed anywhere; columns: %s):\n", nlist
            printf "    %-22s", "test"
            for (i = 1; i <= ns; i++) printf " %-9s", SORD[i]
            printf "\n"
            for (j = 1; j <= nft; j++) {
                t = ftlist[j]
                printf "    %-22s", t
                for (i = 1; i <= nn; i++) {
                    v = NORD[i]
                    c = "P"
                    if (pcls[flavor "\t" v "\t" t] == "FAULT") c = "F"
                    else if (pcls[flavor "\t" v "\t" t] == "TIMEOUT") c = "T"
                    else if (pcls[flavor "\t" v "\t" t] == "FAIL") c = "X"
                    else if (pcls[flavor "\t" v "\t" t] == "SKIP") c = "S"
                    printf " %-9s", c
                }
                printf "\n"
            }
        }
        printf "\n"
        # Emit the global (flavor, variant, test, class) pairs.  Append: the
        # report_group invocation of the previous group already wrote its
        # pairs (a fresh awk process opening with ">" would truncate them).
        # A variant-row SKIP whose CONTROL row was also SKIP is a version-
        # gated skip, not a poisoning failure: the gate is decided by which
        # OpenSSL the binary binds, before any poisoned buffer is used, so
        # every variant just replicates that control SKIP.  Those go to the
        # separate skips file (one line per test), not to the pairs file.
        # A variant-row SKIP over a PASSing (or missing) control row stays a
        # failure pair: poisoning must not be able to cause a skip.  FAULT,
        # TIMEOUT and FAIL pairs are unaffected.
        for (key in pout) {
            split(key, kk, "\t")
            cls = pout[key]
            if (cls == "SKIP" && ctrl[kk[3]] == "SKIP") {
                t = kk[3]
                if (!(t in seen)) {
                    seen[t] = 1
                    skipt[++nvskip] = t
                }
                skipset[t, kk[2]] = 1
            } else {
                print key "\t" cls >> pairsfile
            }
        }
        for (i = 1; i <= nvskip; i++) {
            t = skipt[i]
            vstr = ""
            vcount = 0
            for (j = 1; j <= nn; j++) {
                v = NORD[j]
                if ((t SUBSEP v) in skipset) {
                    vstr = vstr (vstr == "" ? "" : " ") v
                    vcount++
                }
            }
            printf "%s\t%s\t%d/%d\t%s\n", flavor, t, vcount, nn, vstr \
                >> skipsfile
        }
    }' "$tsv"
}

# ---- sweep ----

NONEMPTY=("${VARIANTS[@]:1}")

: >"$OUT_TSV"
: >"$SCRATCH/pairs.tsv"
: >"$SCRATCH/vskips.tsv"

# A "group" is a set of test binaries swept together under one flavor label:
# in --build mode, the binaries of one build; in prebuilt mode, the binaries
# whose ldd libcrypto resolution classifies them the same.  Group state
# lives in $SCRATCH (g.<slug>.*), keyed by a file-safe slug; the display
# label is what the report and the TSV rows show.
#   g.<slug>.display  report/TSV label (e.g. yolo, other:/some/dir)
#   g.<slug>.dir      the group's libcrypto directory ("" if uninteresting)
#   g.<slug>.build    build provenance line for the report
#   g.<slug>.crypto   libcrypto probe line for the report
#   g.<slug>.bins     binary names, one per line
GSLUGS=()

new_group() {
    # new_group <slug> <display> <dir> <buildline> <crypto>
    # Writes the group's metadata files.  Does NOT register the slug in
    # $GSLUGS: callers control ordering (build order vs. canonical order).
    local slug="$1"
    printf '%s\n' "$2" >"$SCRATCH/g.$slug.display"
    printf '%s\n' "$3" >"$SCRATCH/g.$slug.dir"
    printf '%s\n' "$4" >"$SCRATCH/g.$slug.build"
    printf '%s\n' "$5" >"$SCRATCH/g.$slug.crypto"
    : >"$SCRATCH/g.$slug.bins"
}

add_bin() {
    # add_bin <slug> <binary>
    printf '%s\n' "$2" >>"$SCRATCH/g.$1.bins"
}

if [ -z "$BUILD" ]; then
    # Prebuilt mode: no build step.  Classify whatever test binaries exist
    # by their ldd libcrypto resolution and sweep each non-empty group.
    all_bins="$(list_bins)"
    if [ -z "$all_bins" ]; then
        echo "ERROR: no test binaries found; build with --build=yolo|zunsafe|sarcastic (or --build=both|all) first." >&2
        rm -rf "$SCRATCH"
        exit 1
    fi
    echo "Prebuilt mode: not building; classifying existing test binaries via ldd..."
    # Canonical group order: zunsafe, sarcastic, yolo, then the "other"
    # groups (keyed by libcrypto directory) sorted by name.
    known_order() {
        case "$1" in
        zunsafe) echo 0 ;;
        sarcastic) echo 1 ;;
        yolo) echo 2 ;;
        *) echo 3 ;;
        esac
    }
    : >"$SCRATCH/groups.unsorted"
    for b in $all_bins; do
        cls="$(classify_bin "$b")"
        g="${cls%%$'\t'*}"
        dir="${cls#*$'\t'}"
        if [ "$g" = "other" ]; then name="other:$dir"; else name="$g"; fi
        slug="$(slug_for "$name")"
        if [ ! -f "$SCRATCH/g.$slug.display" ]; then
            new_group "$slug" "$name" "$dir" \
                "none (prebuilt binaries; classification via ldd)" \
                "$(bin_libcrypto "$b")"
            echo "$(known_order "$g") $slug" >>"$SCRATCH/groups.unsorted"
        fi
        add_bin "$slug" "$b"
        echo "  $b -> $name ($(bin_libcrypto "$b"))"
    done
    # Slugs are file-safe (no whitespace), so unquoted word splitting is safe.
    for slug in $(sort -n "$SCRATCH/groups.unsorted" | awk '{print $2}'); do
        GSLUGS+=("$slug")
    done
    rm -f "$SCRATCH/groups.unsorted"
else
    # Build mode: build each requested flavor, then sweep what it produced.
    first=1
    for flavor in $BUILD_LIST; do
        if [ "$first" != 1 ]; then
            echo "Cleaning built test executables (exact names only)..."
            clean_bins
        fi
        first=0

        cc=""
        openssl_home=""
        pizfix_empty=0
        case "$flavor" in
        zunsafe) cc="$REPO_ROOT/build/bin/filcc" ;;
        sarcastic)
            cc="$REPO_ROOT/build/bin/filcc"
            openssl_home="$REPO_ROOT/projects/sarcastic-openssl"
            ;;
        yolo)
            cc="gcc"
            if [ -n "$YOLO_OPENSSL_PREFIX" ]; then
                openssl_home="$YOLO_OPENSSL_PREFIX"
                # PIZFIX= (a command-line variable) neutralizes every
                # pizfix-derived path in the Makefile, so no pizfix
                # -L/-rpath leaks into the yolo binaries.
                pizfix_empty=1
            fi
            ;;
        esac

        make_args=(make -B -j"$JOBS" "CC=$cc")
        [ -n "$openssl_home" ] && make_args+=("OPENSSL_HOME=$openssl_home")
        [ "$pizfix_empty" = 1 ] && make_args+=("PIZFIX=")
        build_line="${make_args[*]}"

        echo "[$flavor] building all tests: $build_line"
        build_log="$SCRATCH/build-$flavor.log"
        if ! "${make_args[@]}" >"$build_log" 2>&1; then
            echo "ERROR: build failed for flavor $flavor (log: $build_log):" >&2
            tail -25 "$build_log" >&2
            rm -rf "$SCRATCH"
            exit 1
        fi

        crypto_line="$(crypto_probe)"
        echo "[$flavor] libcrypto probe: $crypto_line"
        case "$flavor" in
        zunsafe)
            if ! printf '%s' "$crypto_line" | grep -q '/pizfix/'; then
                echo "WARNING: zunsafe flavor did not bind a pizfix libcrypto!" >&2
                crypto_line="$crypto_line [WARNING: NOT pizfix!]"
            fi
            ;;
        sarcastic)
            if ! printf '%s' "$crypto_line" | grep -q "sarcastic-openssl/libcrypto.so.3"; then
                echo "WARNING: sarcastic flavor did not bind sarcastic-openssl/libcrypto.so.3!" >&2
                crypto_line="$crypto_line [WARNING: NOT sarcastic-openssl!]"
            fi
            ;;
        yolo)
            if ldd_mentions_pizfix; then
                echo "WARNING: yolo build binds pizfix paths!" >&2
                crypto_line="$crypto_line [WARNING: pizfix leak!]"
            elif [ -n "$openssl_home" ] && \
                ! printf '%s' "$crypto_line" | grep -q "^$openssl_home/lib"; then
                echo "WARNING: yolo build did not bind $openssl_home's libcrypto!" >&2
                crypto_line="$crypto_line [WARNING: NOT $openssl_home!]"
            elif [ -n "$openssl_home" ]; then
                crypto_line="$crypto_line [OK: bound $openssl_home, no pizfix paths]"
            fi
            ;;
        esac

        new_group "$flavor" "$flavor" "" "$build_line" "$crypto_line"
        GSLUGS+=("$flavor")
        for b in $(list_bins); do
            add_bin "$flavor" "$b"
        done
    done
fi

export -f run_one

for gslug in "${GSLUGS[@]}"; do
    display="$(cat "$SCRATCH/g.$gslug.display")"
    test_list="$(cat "$SCRATCH/g.$gslug.bins")"
    ntests="$(printf '%s\n' "$test_list" | grep -c .)"
    echo "[$display] sweeping $ntests tests x ${#VARIANTS[@]} variants (jobs=$JOBS timeout=${TIMEOUT}s poison-args='$POISON_ARGS')"
    echo "[$display] scratch: $SCRATCH (progress: ls $SCRATCH/result.$gslug.*.tsv | wc -l)"

    group_tsv="$SCRATCH/results.$gslug.tsv"
    : >"$group_tsv"
    # One flat job pool over every (variant, test) pair: the suite contains a
    # few tests that take many minutes PER RUN under the emulator, so
    # scheduling all variants through a single -P pool (instead of per-variant
    # batches) keeps every worker busy and cuts hours of wall time.
    { for variant in "${VARIANTS[@]}"; do
          for t in $test_list; do
              printf '%s %s\n' "$variant" "$t"
          done
      done
    } | xargs -P "$JOBS" -I JOB bash -c '
        job="$1"
        run_one "$2" "$3" "${job%% *}" "${job#* }" "$4" "$5" "$6" "$7"
    ' job JOB "$gslug" "$display" "$EMU" "$TIMEOUT" "$POISON_ARGS" "$SCRATCH"
    # Deterministic per-(group,variant) chunks, ordered by variant then test.
    for variant in "${VARIANTS[@]}"; do
        vslug="$(printf '%s' "$variant" | tr ',+' '__')"
        n=0
        for t in $test_list; do
            if [ -f "$SCRATCH/result.$gslug.$vslug.$t.tsv" ]; then
                cat "$SCRATCH/result.$gslug.$vslug.$t.tsv"
                n=$((n + 1))
            fi
        done >>"$group_tsv"
        echo "[$display] variant '$variant' done: $n/$ntests results"
    done
    cat "$group_tsv" >>"$OUT_TSV"
done

# ---- report ----

{
    echo "simple-openssl-tests zegarmistrz poisoning variant sweep"
    echo "script: $SCRIPT_PATH"
    echo "argv: $ARGV_STR"
    echo "date: $(date '+%Y-%m-%d %H:%M:%S')"
    head_sha="$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || true)"
    if [ -n "$head_sha" ]; then
        echo "git head: $head_sha"
    fi
    echo "emulator: $EMU (mtime $(stat -c %y "$EMU" 2>/dev/null || echo unknown))"
    echo "timeout per test: ${TIMEOUT}s   jobs: $JOBS   poison-args: '$POISON_ARGS'"
    if [ -z "$BUILD" ]; then
        echo "build: none (prebuilt binaries; flavor classification via ldd)"
        echo "classification (binaries per group):"
        for gslug in "${GSLUGS[@]}"; do
            printf '  %s: %d\n' \
                "$(cat "$SCRATCH/g.$gslug.display")" \
                "$(grep -c . "$SCRATCH/g.$gslug.bins")"
        done
    else
        echo "build: $BUILD (per-group \"build:\" lines below record the exact make invocations)"
    fi
    echo "variants: ${VARIANTS[*]}"
    echo "raw results: $OUT_TSV"
    echo
    for gslug in "${GSLUGS[@]}"; do
        report_group "$gslug" "$SCRATCH/results.$gslug.tsv" "$SCRATCH/pairs.tsv" \
            "$SCRATCH/vskips.tsv"
    done

    echo "=== VERDICT"
    total_pairs="$(grep -c . "$SCRATCH/pairs.tsv" 2>/dev/null || true)"
    total_vskips="$(grep -c . "$SCRATCH/vskips.tsv" 2>/dev/null || true)"
    if [ "${total_pairs:-0}" -eq 0 ] && [ "${total_vskips:-0}" -eq 0 ]; then
        echo "No poisoning variant caused any failure: every (test, non-empty variant) pair passed everywhere."
    else
        if [ "${total_pairs:-0}" -gt 0 ]; then
            echo "Poisoning variants caused failures.  (flavor, variant, test, class) pairs:"
            sort "$SCRATCH/pairs.tsv" | sed 's/^/  /'
        fi
        if [ "${total_vskips:-0}" -gt 0 ]; then
            echo "Version-gated skips (not poisoning failures): the control variant SKIPped"
            echo "these tests too, so the per-variant SKIPs replicate the version gate"
            echo "rather than a poisoning effect.  (flavor, test, variants that SKIPped):"
            sort "$SCRATCH/vskips.tsv" | sed 's/^/  /'
        fi
    fi
} >"$OUT_TXT"

echo
echo "==================== REPORT ($OUT_TXT) ===================="
cat "$OUT_TXT"
echo "============================================================"
echo "TSV: $OUT_TSV"

rm -rf "$SCRATCH"
exit 0
