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
# run_variants.sh -- build and run the simple-openssl-tests suite under the
# zegarmistrz emulator, sweeping every supported poisoning variant, and
# produce a TSV of raw results plus a human-readable failure report.
#
# Usage:
#   ./run_variants.sh [--flavor=zunsafe|sarcastic|both] [--jobs=N]
#                     [--timeout=S] [--poison-args='--poison-all']
#                     [--allow-no-poison] [--emulator=PATH]
#                     [--output=PREFIX]
#
#   --flavor        which Fil-C OpenSSL to build/run against: zunsafe
#                   (pizfix OpenSSL; default OPENSSL_HOME), sarcastic
#                   (projects/sarcastic-openssl), or both (sequentially,
#                   rebuilding and cleaning in between).
#   --jobs          parallelism for the per-variant test sweep (default:
#                   nproc/2, capped at 16).
#   --timeout       per-test wall-clock timeout in seconds (default 120).
#   --poison-args   extra --poison-* args handed to every test (default:
#                   --poison-all, which is what the committed report
#                   zegarmistrz-variants-report.{txt,tsv} was produced
#                   with).  Which zegarmistrz variants get tainted is the
#                   sweep's job.
#   --allow-no-poison  permit an empty --poison-args= (see below).
#   --emulator      zegarmistrz binary (default <repo>/filc/zegarmistrz).
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
cd "$(dirname "$0")"
REPO_ROOT="$(cd ../.. && pwd)"

FLAVOR="both"
JOBS="$(($(nproc) / 2))"
[ "$JOBS" -gt 16 ] && JOBS=16
[ "$JOBS" -lt 1 ] && JOBS=1
TIMEOUT=120
POISON_ARGS="--poison-all"
ALLOW_NO_POISON=0
EMU="$REPO_ROOT/filc/zegarmistrz"
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
    --flavor=*) FLAVOR="${1#*=}" ;;
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

case "$FLAVOR" in
zunsafe | sarcastic | both) ;;
*) echo "bad --flavor: $FLAVOR" >&2; exit 2 ;;
esac

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

build_flavor() {
    local flavor="$1" cc="$2" openssl_home="$3" log="$4"
    local args=(make -B -j"$JOBS" "CC=$cc")
    [ -n "$openssl_home" ] && args+=("OPENSSL_HOME=$openssl_home")
    if ! "${args[@]}" >"$log" 2>&1; then
        echo "ERROR: build failed for flavor $flavor (log: $log):" >&2
        tail -25 "$log" >&2
        return 1
    fi
    return 0
}

# Records which libcrypto the freshly built tests actually bind to.
crypto_probe() {
    ldd ./test_aes_cbc 2>/dev/null | grep crypto | head -2 | tr '\n' ' ' | sed 's/  */ /g'
}

# One test under one (flavor, variant).  Runs via xargs -P; writes a result
# file into the scratch dir and leaves the TSV assembly to the parent.
run_one() {
    local flavor="$1" variant="$2" test="$3" emu="$4" timeout_s="$5" \
        poison_args="$6" scratch="$7"
    local slug exit_code class note first last out
    slug="$(printf '%s' "$variant" | tr ',+' '__')"
    out="$scratch/result.$flavor.$slug.$test.tsv"
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
        "$flavor" "$variant" "$test" "$exit_code" "$class" "$note" >"$out"
}

# Assembles the per-flavor report section and the (test, variant) failure
# pairs into $4 (a global pairs file).  Args: flavor, results-tsv, libcrypto
# probe line, global pairs file, variant list, non-empty variant list,
# short-name list.
report_flavor() {
    local flavor="$1" tsv="$2" crypto="$3" pairs="$4"
    awk -F'\t' \
        -v flavor="$flavor" -v crypto="$crypto" \
        -v vlist="${VARIANTS[*]}" -v nlist="${NONEMPTY[*]}" \
        -v slist="${VSHORT[*]}" -v pairsfile="$pairs" '
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
        if (v == "none" && class != "PASS")
            control_bad[v "," class]++
    }
    END {
        printf "=== flavor: %s\n", flavor
        printf "libcrypto in use: %s\n", (crypto == "" ? "(probe failed)" : crypto)
        # Control verdict.
        ctotal = total["none"] + 0
        cbad = 0
        for (k in control_bad) cbad += control_bad[k]
        if (ctotal == 0)
            printf "CONTROL-FAILED: no results recorded for the control (variant none)!\n\n"
        else if (cbad == 0)
            printf "control (variant none): %d/%d PASS -- CONTROL OK\n\n", ctotal, ctotal
        else
            printf "CONTROL-FAILED: variant none had %d non-PASS of %d -- results below are meaningless!\n\n", cbad, ctotal
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
        # report_flavor invocation of the previous flavor already wrote its
        # pairs (a fresh awk process opening with ">" would truncate them).
        for (key in pout) {
            split(key, kk, "\t")
            print key "\t" pout[key] >> pairsfile
        }
    }' "$tsv"
}

# ---- sweep ----

NONEMPTY=("${VARIANTS[@]:1}")

: >"$OUT_TSV"
: >"$SCRATCH/pairs.tsv"

FLAVOR_LIST=""
case "$FLAVOR" in
both) FLAVOR_LIST="zunsafe sarcastic" ;;
*) FLAVOR_LIST="$FLAVOR" ;;
esac

first=1
for flavor in $FLAVOR_LIST; do
    if [ "$first" != 1 ]; then
        echo "Cleaning built test executables (exact names only)..."
        clean_bins
    fi
    first=0

    cc="$REPO_ROOT/build/bin/filcc"
    openssl_home=""
    if [ "$flavor" = "sarcastic" ]; then
        openssl_home="$REPO_ROOT/projects/sarcastic-openssl"
    fi

    echo "[$flavor] building all tests with CC=$cc${openssl_home:+ OPENSSL_HOME=$openssl_home} ..."
    build_log="/tmp/simple-openssl-variants-build-$flavor.log"
    if ! build_flavor "$flavor" "$cc" "$openssl_home" "$build_log"; then
        echo "aborting: build failure" >&2
        rm -rf "$SCRATCH"
        exit 1
    fi

    crypto_line="$(crypto_probe)"
    echo "[$flavor] libcrypto probe: $crypto_line"
    if [ "$flavor" = "sarcastic" ] && \
       ! printf '%s' "$crypto_line" | grep -q "sarcastic-openssl/libcrypto.so.3"; then
        echo "WARNING: sarcastic flavor did not bind sarcastic-openssl/libcrypto.so.3!" >&2
        crypto_line="$crypto_line [WARNING: NOT sarcastic-openssl!]"
    fi

    test_list="$(for f in test_*.c; do echo "${f%.c}"; done)"
    ntests="$(printf '%s\n' "$test_list" | grep -c .)"
    echo "[$flavor] sweeping $ntests tests x ${#VARIANTS[@]} variants (jobs=$JOBS timeout=${TIMEOUT}s poison-args='$POISON_ARGS')"
    echo "[$flavor] scratch: $SCRATCH (progress: ls $SCRATCH/result.$flavor.*.tsv | wc -l)"

    export -f run_one

    flavor_tsv="$SCRATCH/results.$flavor.tsv"
    : >"$flavor_tsv"
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
        run_one "$2" "${job%% *}" "${job#* }" "$3" "$4" "$5" "$6"
    ' job JOB "$flavor" "$EMU" "$TIMEOUT" "$POISON_ARGS" "$SCRATCH"
    # Deterministic per-(flavor,variant) chunks, ordered by variant then test.
    for variant in "${VARIANTS[@]}"; do
        slug="$(printf '%s' "$variant" | tr ',+' '__')"
        n=0
        for t in $test_list; do
            if [ -f "$SCRATCH/result.$flavor.$slug.$t.tsv" ]; then
                cat "$SCRATCH/result.$flavor.$slug.$t.tsv"
                n=$((n + 1))
            fi
        done >>"$flavor_tsv"
        echo "[$flavor] variant '$variant' done: $n/$ntests results"
    done
    cat "$flavor_tsv" >>"$OUT_TSV"
    echo "$crypto_line" >"$SCRATCH/crypto.$flavor"
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
    echo "variants: ${VARIANTS[*]}"
    echo "raw results: $OUT_TSV"
    echo
    for flavor in $FLAVOR_LIST; do
        report_flavor "$flavor" "$SCRATCH/results.$flavor.tsv" \
            "$(cat "$SCRATCH/crypto.$flavor" 2>/dev/null)" "$SCRATCH/pairs.tsv"
    done

    echo "=== VERDICT"
    total_pairs="$(grep -c . "$SCRATCH/pairs.tsv" 2>/dev/null || true)"
    if [ "${total_pairs:-0}" -eq 0 ]; then
        echo "No poisoning variant caused any failure: every (test, non-empty variant) pair passed everywhere."
    else
        echo "Poisoning variants caused failures.  (flavor, variant, test, class) pairs:"
        sort "$SCRATCH/pairs.tsv" | sed 's/^/  /'
    fi
} >"$OUT_TXT"

echo
echo "==================== REPORT ($OUT_TXT) ===================="
cat "$OUT_TXT"
echo "============================================================"
echo "TSV: $OUT_TSV"

rm -rf "$SCRATCH"
exit 0
