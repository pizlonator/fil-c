#!/bin/bash
# projeny test suite: fixture-based shell tests over tiny fake-project
# tarballs v1/v2. Covers fresh setup, edit+commit roundtrips, setup-again
# noops, divergent merges (clean + conflicting, the latter exiting nonzero),
# add/rm/mv + commit, rebase (clean + conflict), package/extract (tracked-only
# payloads, compression autodetect incl. the short .tbz/.tbz2/.txz/.tzst
# forms, conflict refusal), and the hard-error paths.
# Later sections cover the dot-prefixed bookkeeping names: legacy undotted
# .projeny.status / .snapshot migration on first use, stale-state
# reconciliation (.stale, .stale2, ...) when the workdir is gone (both
# naming forms, plus the previous archive's snapshot after a rebase), the
# setup-journal crash window (nothing is staled while recovery state is
# needed), the workdir-without-status hard error, and snapshot
# copy-on-fallback from the tarball (setup and status).
# The frozen-mtime sections cover freeze/unfreeze/list/get-attributes
# (ordering, dual attributes, scope filtering), pin survival across
# commit/rebase/package, pin movement on rename, pin pruning on rm, the
# stamp pass running even on setups that end in conflicts, the loud pin
# drop when a frozen file becomes a symlink, and malformed-header
# rejection; the path-resolution sections cover the
# physical (symlink-aware) resolver and '.'/'..' arguments on every
# workdir-mutating command.
# The URL sections cover URL:-based .projeny headers (the checked-in-tarball
# alternative): `projeny hash`, fresh setup from file:// URLs (fully
# hermetic — no network, no server), the no-network re-setup while the
# snapshot matches a URL hash, corrupted-snapshot self-healing, multi-URL
# fallback (unreachable and hash-mismatched first URLs) and the
# all-URLs-fail hard errors, malformed-header rejection, uppercase hashes,
# query-string basename derivation, and the rest of the command surface on
# URL projects (commit, URL-edit merge, rebase refusal, tolerant status,
# package/extract/get-attributes, freeze-mtime).
# The parallel sections cover the multi-project forms of
# setup/package/extract and the download command: a repeat-everything
# flakiness loop over fresh state (default -j, -j1, -j2, -j100), shared-
# archive dedupe (one download per archive basename, common URLs first),
# in-batch mirror fallback, the announced single retry pass, the loud
# shared-package warnings, duplicate-argument collapsing (including the
# paired package/extract forms and path-alias spellings), the parallel
# failure isolation (labeled guard error + summary line, good projects
# still set up), the URL HASH download pairs (byte-exact, already-have
# skip, hash normalization, exact hard-error wordings), the -j/-c
# option surface (accepted by setup/package/extract/download only), the
# parallel conflicted-file fallback (two conflicted .projeny files
# sharing one archive download exactly once outside the empty-plan batch
# phase, deterministically across repeated rounds), and the batch's
# combined download progress line (one single-token entry per announced
# package — a whole-percent or a compact byte count, never "?" — in a
# roster that only ever grows, closed by one deterministic final line
# listing every package of the round) alongside the multi-mode silence of
# the per-project snapshot phase ("using existing snapshot" notes stay
# single-project-only). The erase-setup sections cover the checkout + both
# status-name deletion, --erase-snapshots (exact snapshot only), the
# parallel batch form (already-erased checkouts, a deletion that fails its
# project but not the rest, unparseable and nonexistent .projeny files,
# the dir argument form, the option surface, the legacy undotted status
# file), and the no-force check phase: without --force erase-setup first
# checks every project in parallel (subject to -j) and refuses the whole
# invocation — erasing nothing — when any project's status reports
# anything a commit would fold in (modified, disappeared, pending
# added/removed/renamed, conflicts; untracked files alone erase happily),
# or when a project's state cannot be assessed at all; --force restores
# the unconditional erasure.
#
# Bash is required (process substitution in the status-copy comparisons
# below); /bin/sh (dash) cannot run this suite.
#
# Usage: ./tests/run_tests.sh ./projeny
# Exits nonzero on failure; prints ok/FAIL lines plus summary counts.
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
set -u

PROJENY="${1:-./projeny}"
case "$PROJENY" in
/*) ;;
*) PROJENY="$(pwd)/$PROJENY" ;;
esac

PASS=0
FAIL=0

ok() {
    PASS=$((PASS + 1))
    echo "ok $PASS - $1"
}

fail() {
    FAIL=$((FAIL + 1))
    echo "FAIL - $1"
    if [ $# -ge 2 ]; then
        echo "  detail: $2"
    fi
}

# expect_ok <name> <command...>: command must exit 0.
expect_ok() {
    name="$1"
    shift
    out="$("$@" 2>&1)"
    rc=$?
    if [ $rc -eq 0 ]; then
        ok "$name"
    else
        fail "$name" "exit=$rc out: $out"
    fi
}

# expect_fail <name> <command...>: command must exit nonzero.
expect_fail() {
    name="$1"
    shift
    out="$("$@" 2>&1)"
    rc=$?
    if [ $rc -ne 0 ]; then
        ok "$name"
    else
        fail "$name" "expected failure but exited 0 (out: $out)"
    fi
}

# expect_out <name> <fixed-substring> <command...>: command must exit 0 AND
# its combined output must contain the fixed substring (case-match, like
# the `case ... in *"$want"*)` blocks used throughout this suite).
expect_out() {
    name="$1"
    want="$2"
    shift 2
    out="$("$@" 2>&1)"
    rc=$?
    case "$out" in
    *"$want"*)
        if [ $rc -eq 0 ]; then
            ok "$name"
        else
            fail "$name" "output matched but exit=$rc out: $out"
        fi
        ;;
    *)
        fail "$name" "exit=$rc output lacks '$want' out: $out"
        ;;
    esac
}

# expect_no_out <name> <command...>: command must exit 0 AND print nothing
# on either stream (the documented empty-output policy, e.g. get-attributes
# with nothing to report).
expect_no_out() {
    name="$1"
    shift
    out="$("$@" 2>&1)"
    rc=$?
    if [ $rc -eq 0 ] && [ -z "$out" ]; then
        ok "$name"
    else
        fail "$name" "exit=$rc expected silence, got: $out"
    fi
}

# make_project <dir> <name> <version>: create the tiny fake project fixture
# (a <name>-<version>.tar.gz holding README + src/a.c + src/b.c, and a
# <name>.projeny pointing at it) in <dir>, which is created fresh. The same
# dance the earlier sections perform by hand, factored out for the later
# fixtures.
make_project() {
    _dir="$1"
    _name="$2"
    _ver="$3"
    mkdir -p "$_dir"
    rm -rf "$_dir/$_name-$_ver"
    mkdir -p "$_dir/$_name-$_ver/src"
    printf 'int alpha = %s;\n' "$_ver" > "$_dir/$_name-$_ver/src/a.c"
    printf 'line one v%s\n' "$_ver" > "$_dir/$_name-$_ver/src/b.c"
    printf 'hello v%s\n' "$_ver" > "$_dir/$_name-$_ver/README"
    (cd "$_dir" && tar -czf "$_name-$_ver.tar.gz" "$_name-$_ver" && rm -rf "$_name-$_ver")
    printf 'Archive: %s-%s.tar.gz\nOrigname: %s-%s\nName: %s\n\n    Fake project %s for projeny tests.\n\n' \
        "$_name" "$_ver" "$_name" "$_ver" "$_name" "$_name" > "$_dir/$_name.projeny"
}

# expect_file_contains <name> <file> <fixed-string>
expect_file_contains() {
    if grep -qF -- "$3" "$2"; then
        ok "$1"
    else
        fail "$1" "'$2' lacks '$3'"
    fi
}

# expect_file_not_contains <name> <file> <fixed-string>
expect_file_not_contains() {
    if grep -qF -- "$3" "$2"; then
        fail "$1" "'$2' unexpectedly contains '$3'"
    else
        ok "$1"
    fi
}

# expect_file_eq <name> <file> <expected-content> (exact bytes via stdin heredoc file)
expect_file_eq() {
    if [ ! -f "$2" ]; then
        fail "$1" "'$2' does not exist"
        return
    fi
    if cmp -s "$2" "$3"; then
        ok "$1"
    else
        fail "$1" "'$2' differs from expected; got: $(cat "$2")"
    fi
}

# Run a command in a directory without forking the assertion counters:
# (cd X && expect_...) would run expect_* in a subshell, losing PASS/FAIL.
# Instead cd there, run, and cd back — all in the current shell.
run_in() {
    _dir="$1"
    shift
    _prev="$(pwd)"
    cd "$_dir" || { fail "cannot cd to $_dir"; return 1; }
    "$@"
    _rc=$?
    cd "$_prev" || exit 1
    return $_rc
}

ROOT="$(pwd)/.projeny-test-tmp"
rm -rf "$ROOT"
mkdir -p "$ROOT"

# ---------------------------------------------------------------- fixtures
# fake v1/v2: multi-line files with GAPS between change regions so that
# simultaneous edits to different regions merge cleanly with git merge-file.
make_tarballs() {
    dir="$1"       # scratch dir (created fresh)
    stem="$2"      # e.g. fake
    mkdir -p "$dir"
    rm -rf "$dir/$stem-1.0" "$dir/$stem-2.0"
    mkdir -p "$dir/$stem-1.0/src"
    cat > "$dir/$stem-1.0/src/a.c" <<'EOF'
int alpha = 1;

int beta = 1;

int gamma = 1;

int delta = 1;
EOF
    printf 'line one v1\n' > "$dir/$stem-1.0/src/b.c"
    printf 'hello v1\n' > "$dir/$stem-1.0/README"
    (cd "$dir" && tar -czf "$stem-1.0.tar.gz" "$stem-1.0")
    mkdir -p "$dir/$stem-2.0/src"
    cat > "$dir/$stem-2.0/src/a.c" <<'EOF'
int alpha = 2;

int beta = 1;

int gamma = 1;

int delta = 1;
EOF
    printf 'line one v2\n' > "$dir/$stem-2.0/src/b.c"
    printf 'hello v2\n' > "$dir/$stem-2.0/README"
    (cd "$dir" && tar -czf "$stem-2.0.tar.gz" "$stem-2.0")
    rm -rf "$dir/$stem-1.0" "$dir/$stem-2.0"
}

write_projeny() {
    # $1 = dir, $2 = stem, $3 = version (1.0/2.0), $4 = workdir name
    cat > "$1/$2.projeny" <<EOF
Archive: $2-$3.tar.gz
Origname: $2-$3
Name: $4

    Fake project $2 for projeny tests.

EOF
}

check_deps() {
    if [ ! -x "$PROJENY" ]; then
        echo "FAIL - projeny binary '$PROJENY' missing/not executable" >&2
        exit 1
    fi
    # projeny itself needs only tar at runtime (diff/patch/merge are
    # internal); git/patch below are used solely for optional compatibility
    # spot-checks, which skip themselves when the helper is absent.
    for t in tar; do
        if ! command -v $t >/dev/null 2>&1; then
            echo "FAIL - required helper '$t' missing" >&2
            exit 1
        fi
    done
}

check_deps

# ---------------------------------------------------------- 1. fresh setup
T1="$ROOT/t1"
make_tarballs "$T1" fake
write_projeny "$T1" fake 1.0 fake
run_in "$T1" expect_ok "fresh setup exits 0" "$PROJENY" setup fake.projeny
if [ -d "$T1/fake/src" ] && [ -f "$T1/fake/src/a.c" ]; then
    ok "fresh setup creates workdir with files"
else
    fail "fresh setup creates workdir with files" "ls: $(ls -R "$T1" 2>&1)"
fi
expect_file_contains "fresh setup workdir has v1 content" "$T1/fake/README" "hello v1"
if [ -f "$T1/.fake.projeny.status" ]; then
    ok "fresh setup writes status file"
else
    fail "fresh setup writes status file"
fi
expect_file_contains "status reports setup state" "$T1/.fake.projeny.status" "Status: setup"
expect_file_contains "status embeds projeny verbatim" "$T1/.fake.projeny.status" "Archive: fake-1.0.tar.gz"

# ----------------------------------------------- 2. edit + commit roundtrip
T2="$ROOT/t2"
make_tarballs "$T2" fake
write_projeny "$T2" fake 1.0 fake
(cd "$T2" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T2/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int alpha = 1;", "int alpha = 10;")
open(p, "w").write(s)
EOF
run_in "$T2" expect_ok "commit edited file exits 0" "$PROJENY" commit fake.projeny
expect_file_contains "commit stores diff in projeny" "$T2/fake.projeny" "alpha = 10"
expect_file_contains "commit refreshes status copy" "$T2/.fake.projeny.status" "alpha = 10"
run_in "$T2" expect_ok "setup-again noop exits 0" "$PROJENY" setup fake.projeny
expect_file_contains "setup-again keeps committed edit" "$T2/fake/src/a.c" "alpha = 10"

# --------------------------------------- 3. divergent setup merge, no conflict
T3="$ROOT/t3"
make_tarballs "$T3" fake
write_projeny "$T3" fake 1.0 fake
(cd "$T3" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
# local committed change: delta region.
python3 - "$T3/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
(cd "$T3" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T3/fake.projeny" "$ROOT/t3-local.projeny"
# upstream: same committed base, rebased onto v2 + gamma-region change.
(cd "$T3" && "$PROJENY" rebase fake.projeny fake-2.0.tar.gz >/dev/null 2>&1)
python3 - "$T3/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int gamma = 1;", "int gamma = 200;")
open(p, "w").write(s)
EOF
(cd "$T3" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T3/fake.projeny" "$ROOT/t3-upstream.projeny"
# local clone: local base + uncommitted beta-region edit, then upstream merge.
rm -rf "$T3/fake" "$T3/.fake.projeny.status"
cp "$ROOT/t3-local.projeny" "$T3/fake.projeny"
(cd "$T3" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T3/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 300;")
open(p, "w").write(s)
EOF
cp "$ROOT/t3-upstream.projeny" "$T3/fake.projeny"
run_in "$T3" expect_ok "divergent setup merge exits 0" "$PROJENY" setup fake.projeny
expect_file_contains "divergent merge keeps local beta edit" "$T3/fake/src/a.c" "beta = 300"
expect_file_contains "divergent merge keeps committed delta edit" "$T3/fake/src/a.c" "delta = 100"
expect_file_contains "divergent merge takes upstream gamma edit" "$T3/fake/src/a.c" "gamma = 200"
expect_file_contains "divergent merge takes upstream v2 alpha" "$T3/fake/src/a.c" "alpha = 2"
expect_file_not_contains "divergent merge has no conflict markers" "$T3/fake/src/a.c" "<<<<<<<"
if grep -q "^Conflict:" "$T3/.fake.projeny.status"; then
    fail "divergent merge records no conflicts" "$(cat "$T3/.fake.projeny.status")"
else
    ok "divergent merge records no conflicts"
fi

# --------------------------- 4. conflicting setup merge + resolve + commit
T4="$ROOT/t4"
make_tarballs "$T4" fake
write_projeny "$T4" fake 1.0 fake
(cd "$T4" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T4/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 10;")
open(p, "w").write(s)
EOF
(cd "$T4" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T4/fake.projeny" "$ROOT/t4-local.projeny"
rm -rf "$T4/fake" "$T4/.fake.projeny.status"
cp "$ROOT/t4-local.projeny" "$T4/fake.projeny"
(cd "$T4" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T4/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 999;")
open(p, "w").write(s)
EOF
# upstream changes the same beta line differently (same base region).
U4="$ROOT/t4up"
mkdir -p "$U4"
cp "$T4/fake-1.0.tar.gz" "$U4/"
cp "$ROOT/t4-local.projeny" "$U4/fake.projeny"
(cd "$U4" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$U4/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 555;")
open(p, "w").write(s)
EOF
(cd "$U4" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$U4/fake.projeny" "$T4/fake.projeny"
run_in "$T4" expect_fail "conflicting setup merge exits nonzero (with markers)" "$PROJENY" setup fake.projeny
expect_file_contains "conflicting merge leaves markers" "$T4/fake/src/a.c" "<<<<<<<"
expect_file_contains "conflicting merge lists conflict in status" "$T4/.fake.projeny.status" "Conflict: src/a.c"
run_in "$T4" expect_fail "commit fails while conflicted" "$PROJENY" commit fake.projeny
printf 'int alpha = 1;\n\nint beta = 777;\n\nint gamma = 1;\n\nint delta = 1;\n' > "$T4/fake/src/a.c"
run_in "$T4" expect_ok "resolve clears conflict" "$PROJENY" resolve fake.projeny fake/src/a.c
if grep -q "^Conflict:" "$T4/.fake.projeny.status"; then
    fail "resolve removes conflict entry" "$(cat "$T4/.fake.projeny.status")"
else
    ok "resolve removes conflict entry"
fi
run_in "$T4" expect_ok "commit succeeds after resolve" "$PROJENY" commit fake.projeny
expect_file_contains "post-resolve commit stores resolution" "$T4/fake.projeny" "beta = 777"

# ------------------------------------------------- 5. add + commit (new file)
T5="$ROOT/t5"
make_tarballs "$T5" fake
write_projeny "$T5" fake 1.0 fake
(cd "$T5" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'brand new file\n' > "$T5/fake/src/added.c"
run_in "$T5" expect_ok "add marks new file" "$PROJENY" add fake.projeny fake/src/added.c
expect_file_contains "add records pending op in status" "$T5/.fake.projeny.status" "Added: src/added.c"
run_in "$T5" expect_ok "commit folds add into patch" "$PROJENY" commit fake.projeny
expect_file_contains "add commit stores new-file diff" "$T5/fake.projeny" "new file"
expect_file_contains "add commit keeps new content" "$T5/fake.projeny" "brand new file"
run_in "$T5" expect_ok "setup-again after add-commit" "$PROJENY" setup fake.projeny
expect_file_contains "added file survives re-setup" "$T5/fake/src/added.c" "brand new file"

# ----------------------------------------------- 6. rm + commit (deletion)
T6="$ROOT/t6"
make_tarballs "$T6" fake
write_projeny "$T6" fake 1.0 fake
(cd "$T6" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T6" expect_ok "rm marks deletion" "$PROJENY" rm fake.projeny fake/src/b.c
if [ ! -e "$T6/fake/src/b.c" ]; then
    ok "rm removes file from workdir"
else
    fail "rm removes file from workdir"
fi
expect_file_contains "rm records pending op in status" "$T6/.fake.projeny.status" "Removed: src/b.c"
run_in "$T6" expect_ok "commit folds rm into patch" "$PROJENY" commit fake.projeny
expect_file_contains "rm commit stores deletion diff" "$T6/fake.projeny" "deleted file"
run_in "$T6" expect_ok "setup-again after rm-commit" "$PROJENY" setup fake.projeny
if [ ! -e "$T6/fake/src/b.c" ]; then
    ok "deleted file stays deleted after re-setup"
else
    fail "deleted file stays deleted after re-setup"
fi

# ------------------------------------------------------ 7. mv + commit
T7="$ROOT/t7"
make_tarballs "$T7" fake
write_projeny "$T7" fake 1.0 fake
(cd "$T7" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T7" expect_ok "mv renames file" "$PROJENY" mv fake.projeny fake/src/b.c fake/src/renamed.c
expect_file_contains "mv records pending op in status" "$T7/.fake.projeny.status" "Renamed: src/b.c -> src/renamed.c"
run_in "$T7" expect_ok "commit folds mv into patch" "$PROJENY" commit fake.projeny
expect_file_contains "mv commit stores rename" "$T7/fake.projeny" "rename from"
run_in "$T7" expect_ok "setup-again after mv-commit" "$PROJENY" setup fake.projeny
if [ -f "$T7/fake/src/renamed.c" ] && [ ! -e "$T7/fake/src/b.c" ]; then
    ok "rename survives re-setup"
else
    fail "rename survives re-setup" "ls: $(ls "$T7/fake/src" 2>&1)"
fi

# ------------------------------------------------------- 8. rebase clean
T8="$ROOT/t8"
make_tarballs "$T8" fake
write_projeny "$T8" fake 1.0 fake
(cd "$T8" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T8/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 42;")
open(p, "w").write(s)
EOF
(cd "$T8" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
run_in "$T8" expect_ok "clean rebase exits 0" "$PROJENY" rebase fake.projeny fake-2.0.tar.gz
expect_file_contains "rebase updates Archive header" "$T8/fake.projeny" "Archive: fake-2.0.tar.gz"
expect_file_contains "rebase updates Origname header" "$T8/fake.projeny" "Origname: fake-2.0"
expect_file_contains "rebase keeps local delta change" "$T8/fake/src/a.c" "delta = 42"
expect_file_contains "rebase takes new alpha" "$T8/fake/src/a.c" "alpha = 2"

# ------------------------------------------------------- 9. rebase conflict
T9="$ROOT/t9"
make_tarballs "$T9" fake
write_projeny "$T9" fake 1.0 fake
(cd "$T9" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T9/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int alpha = 1;", "int alpha = 100;")
open(p, "w").write(s)
EOF
(cd "$T9" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
run_in "$T9" expect_ok "conflicting rebase exits 0 (with markers)" "$PROJENY" rebase fake.projeny fake-2.0.tar.gz
expect_file_contains "conflicting rebase leaves markers" "$T9/fake/src/a.c" "<<<<<<<"
expect_file_contains "conflicting rebase records conflict" "$T9/.fake.projeny.status" "Conflict: src/a.c"
expect_file_contains "conflicting rebase still updates Archive" "$T9/fake.projeny" "Archive: fake-2.0.tar.gz"

# ----------------------------------------- 10. missing-status setup error
T10="$ROOT/t10"
make_tarballs "$T10" fake
write_projeny "$T10" fake 1.0 fake
(cd "$T10" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm "$T10/.fake.projeny.status"
run_in "$T10" expect_fail "setup without status hard-errors" "$PROJENY" setup fake.projeny

# ----------------------------------------- 11. bad tarball (multi top-level)
T11="$ROOT/t11"
mkdir -p "$T11/top1" "$T11/top2"
printf 'a\n' > "$T11/top1/f.c"
printf 'b\n' > "$T11/top2/g.c"
(cd "$T11" && tar -czf bad.tar.gz top1 top2)
cat > "$T11/b.projeny" <<'EOF'
Archive: bad.tar.gz
Origname: top1
Name: b

    Bad tarball fixture.

EOF
run_in "$T11" expect_fail "multi-top-level tarball hard-errors" "$PROJENY" setup b.projeny

# ----------------------------------------- 12. commit-after-projeny-edit error
T12="$ROOT/t12"
make_tarballs "$T12" fake
write_projeny "$T12" fake 1.0 fake
(cd "$T12" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf '# sneaky hand edit\n' >> "$T12/fake.projeny"
run_in "$T12" expect_fail "commit after projeny edit hard-errors" "$PROJENY" commit fake.projeny

# ----------------------------------------- 13. rebase dirty-tree refusal
T13="$ROOT/t13"
make_tarballs "$T13" fake
write_projeny "$T13" fake 1.0 fake
(cd "$T13" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'uncommitted change\n' >> "$T13/fake/src/a.c"
run_in "$T13" expect_fail "rebase with dirty tree fails" "$PROJENY" rebase fake.projeny fake-2.0.tar.gz

# ----------------------------------------- 14. rebase setup-first fallback
T14="$ROOT/t14"
make_tarballs "$T14" fake
write_projeny "$T14" fake 1.0 fake
run_in "$T14" expect_ok "rebase without setup runs setup first" "$PROJENY" rebase fake.projeny fake-2.0.tar.gz
expect_file_contains "setup-first rebase updates Archive" "$T14/fake.projeny" "Archive: fake-2.0.tar.gz"
expect_file_contains "setup-first rebase unpacks new tree" "$T14/fake/README" "hello v2"

# ----------------------------------------- 15. trailing-whitespace roundtrip
# A file whose content lines end in spaces/tabs must survive commit followed
# by a fresh setup byte-for-byte (normalize_patch_text must not rtrim them).
T15="$ROOT/t15"
make_tarballs "$T15" fake
write_projeny "$T15" fake 1.0 fake
(cd "$T15" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'keep   \nline2\t\n  indented  \n' > "$T15/fake/src/spacey.c"
run_in "$T15" expect_ok "add trailing-whitespace file" "$PROJENY" add fake.projeny fake/src/spacey.c
run_in "$T15" expect_ok "commit trailing-whitespace file" "$PROJENY" commit fake.projeny
cp "$T15/fake/src/spacey.c" "$ROOT/t15-expect-spacey.c"
rm -rf "$T15/fake" "$T15/.fake.projeny.status"
run_in "$T15" expect_ok "fresh setup after whitespace commit" "$PROJENY" setup fake.projeny
if cmp -s "$T15/fake/src/spacey.c" "$ROOT/t15-expect-spacey.c"; then
    ok "trailing-whitespace file identical after fresh setup"
else
    fail "trailing-whitespace file identical after fresh setup" "got: $(cat -A "$T15/fake/src/spacey.c" 2>&1)"
fi
# Appending another trailing-space line + committing must also roundtrip.
printf 'extra   \n' >> "$T15/fake/src/spacey.c"
cp "$T15/fake/src/spacey.c" "$ROOT/t15-expect-spacey.c"
run_in "$T15" expect_ok "commit appended trailing-whitespace line" "$PROJENY" commit fake.projeny
rm -rf "$T15/fake" "$T15/.fake.projeny.status"
run_in "$T15" expect_ok "fresh setup after append commit" "$PROJENY" setup fake.projeny
if cmp -s "$T15/fake/src/spacey.c" "$ROOT/t15-expect-spacey.c"; then
    ok "appended trailing-whitespace line identical after fresh setup"
else
    fail "appended trailing-whitespace line identical after fresh setup" "got: $(cat -A "$T15/fake/src/spacey.c" 2>&1)"
fi

# ----------------------------------------- 16. pax-header tarball setup
# Tarballs carrying pax_global_header metadata entries must still enforce
# exactly-one-top-dir on the REAL content and set up cleanly.
T16="$ROOT/t16"
make_tarballs "$T16" fake
if command -v python3 >/dev/null 2>&1; then
    (cd "$T16" && python3 - fake-1.0.tar.gz <<'PYEOF'
import sys, tarfile, io, os
src, dst = sys.argv[1], "pax-fake-1.0.tar.gz"
os.makedirs("_px/fake-1.0", exist_ok=True)
with tarfile.open(src) as t:
    t.extractall("_px")
with tarfile.open(dst, "w") as t:
    meta = b"pax metadata, not content\n"
    ti = tarfile.TarInfo("pax_global_header")
    ti.size = len(meta)
    ti.mtime = 0
    t.addfile(ti, io.BytesIO(meta))
    t.add("_px/fake-1.0", arcname="fake-1.0")
PYEOF
    )
    rm -rf "$T16/_px"
    cat > "$T16/pax.projeny" <<'EOF'
Archive: pax-fake-1.0.tar.gz
Origname: fake-1.0
Name: paxfake

    Pax-header fixture.

EOF
    run_in "$T16" expect_ok "pax-header tarball setup exits 0" "$PROJENY" setup pax.projeny
    expect_file_contains "pax-header tarball unpacks real content" "$T16/paxfake/README" "hello v1"
    if [ -e "$T16/paxfake/pax_global_header" ]; then
        fail "pax-header metadata not left in workdir" "$(ls "$T16/paxfake")"
    else
        ok "pax-header metadata not left in workdir"
    fi
else
    ok "pax-header tarball setup exits 0 (skipped: no python3)"
    ok "pax-header tarball unpacks real content (skipped: no python3)"
    ok "pax-header metadata not left in workdir (skipped: no python3)"
fi

# ----------------------------------------- 17. no-trailing-newline projeny
# A .projeny file without a final newline must set up AND commit cleanly
# (bytes preserved exactly; commit compares raw bytes). The committed patch
# must start on its own line: the prose keeps its missing trailing newline
# and the patch never glues onto the last prose line (that glue made the
# next parse treat the whole patch as prose and silently lose it).
T17="$ROOT/t17"
make_tarballs "$T17" fake
printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    No trailing newline.' > "$T17/fake.projeny"
run_in "$T17" expect_ok "no-newline projeny setup exits 0" "$PROJENY" setup fake.projeny
expect_file_contains "no-newline projeny unpacks tree" "$T17/fake/README" "hello v1"
python3 - "$T17/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int alpha = 1;", "int alpha = 11;")
open(p, "w").write(s)
EOF
run_in "$T17" expect_ok "no-newline projeny commit exits 0" "$PROJENY" commit fake.projeny
expect_file_contains "no-newline commit stores diff" "$T17/fake.projeny" "alpha = 11"
expect_file_not_contains "no-newline commit never glues the patch onto the prose" \
    "$T17/fake.projeny" "No trailing newline.diff --git "
if grep -q '^diff --git ' "$T17/fake.projeny"; then
    ok "no-newline commit starts the patch at column 0"
else
    fail "no-newline commit starts the patch at column 0" "$(cat "$T17/fake.projeny")"
fi
run_in "$T17" expect_ok "setup-again after no-newline commit" "$PROJENY" setup fake.projeny
expect_file_contains "no-newline roundtrip keeps edit" "$T17/fake/src/a.c" "alpha = 11"

# ----------------------------------------- 18. tricky filenames in status
# Filenames containing " -> " and spaces must survive add/mv/commit/setup via
# the backslash-escaped status file (split on the unescaped separator only).
T18="$ROOT/t18"
mkdir -p "$T18/w-1.0/src"
printf 'int x = 1;\n' > "$T18/w-1.0/src/a.c"
printf 'old\n' > "$T18/w-1.0/src/old -> file.c"
(cd "$T18" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Tricky names.\n' > "$T18/w.projeny"
(cd "$T18" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
run_in "$T18" expect_ok "mv arrow-name file" "$PROJENY" mv w.projeny "w/src/old -> file.c" "w/src/new -> file.c"
expect_file_contains "status stores escaped rename" "$T18/.w.projeny.status" 'Renamed: src/old -\> file.c -> src/new -\> file.c'
run_in "$T18" expect_ok "add alongside tricky rename" "$PROJENY" add w.projeny w/src/a.c
run_in "$T18" expect_ok "commit tricky rename" "$PROJENY" commit w.projeny
expect_file_contains "tricky commit stores rename" "$T18/w.projeny" "rename from"
rm -rf "$T18/w" "$T18/.w.projeny.status"
run_in "$T18" expect_ok "fresh setup after tricky commit" "$PROJENY" setup w.projeny
if [ -f "$T18/w/src/new -> file.c" ] && [ ! -e "$T18/w/src/old -> file.c" ]; then
    ok "tricky rename survives re-setup"
else
    fail "tricky rename survives re-setup" "ls: $(ls "$T18/w/src" 2>&1)"
fi

# ----------------------------------------- 19. resolve warns on markers
# resolve must warn to stderr when markers remain, but still resolve.
T19="$ROOT/t19"
make_tarballs "$T19" fake
write_projeny "$T19" fake 1.0 fake
(cd "$T19" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'int alpha = 1;\n\nint beta = 1;\n\nint gamma = 1;\n\nint delta = 1;\n' > "$T19/fake/src/a.c"
(cd "$T19" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T19/fake.projeny" "$ROOT/t19-local.projeny"
rm -rf "$T19/fake" "$T19/.fake.projeny.status"
cp "$ROOT/t19-local.projeny" "$T19/fake.projeny"
(cd "$T19" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T19/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 999;")
open(p, "w").write(s)
EOF
U19="$ROOT/t19up"
mkdir -p "$U19"
cp "$T19/fake-1.0.tar.gz" "$U19/"
cp "$ROOT/t19-local.projeny" "$U19/fake.projeny"
(cd "$U19" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$U19/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 555;")
open(p, "w").write(s)
EOF
(cd "$U19" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$U19/fake.projeny" "$T19/fake.projeny"
run_in "$T19" expect_fail "conflicting setup leaves markers (t19, nonzero)" "$PROJENY" setup fake.projeny
expect_file_contains "t19 merge leaves markers" "$T19/fake/src/a.c" "<<<<<<<"
out="$(cd "$T19" && "$PROJENY" resolve fake.projeny fake/src/a.c 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "resolve with markers still exits 0"
else
    fail "resolve with markers still exits 0" "exit=$rc out: $out"
fi
case "$out" in
*conflict*marker*|*marker*|*warning*)
    ok "resolve with markers warns on stderr"
    ;;
*)
    if [ -f "$T19/fake/src/a.c" ] && grep -q "<<<<<<<" "$T19/fake/src/a.c"; then
        fail "resolve with markers warns on stderr" "no warning; out: $out"
    else
        ok "resolve with markers warns on stderr (no markers left)"
    fi
    ;;
esac

# ----------------------------------------- 20. tar escape rejection
# Symlink members pointing outside the tree must hard-error on setup.
T20="$ROOT/t20"
mkdir -p "$T20/evil-1.0"
printf 'hi\n' > "$T20/evil-1.0/f.c"
ln -s /etc/passwd "$T20/evil-1.0/evil-link"
(cd "$T20" && tar -czf evil-1.0.tar.gz evil-1.0)
cat > "$T20/e.projeny" <<'EOF'
Archive: evil-1.0.tar.gz
Origname: evil-1.0
Name: e

    Evil tarball fixture.

EOF
run_in "$T20" expect_fail "absolute symlink target hard-errors" "$PROJENY" setup e.projeny

# ----------------------------------------- 21. rebase keeps pending ops
# Pending add/rm/mv operations must survive a clean rebase.
T21="$ROOT/t21"
make_tarballs "$T21" fake
write_projeny "$T21" fake 1.0 fake
(cd "$T21" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'pending new\n' > "$T21/fake/src/pending.c"
run_in "$T21" expect_ok "stage pending add before rebase" "$PROJENY" add fake.projeny fake/src/pending.c
run_in "$T21" expect_ok "clean rebase with pending add" "$PROJENY" rebase fake.projeny fake-2.0.tar.gz
expect_file_contains "rebase preserves pending add" "$T21/.fake.projeny.status" "Added: src/pending.c"
expect_file_contains "rebase keeps pending file" "$T21/fake/src/pending.c" "pending new"

# ----------------------------------------- 22. bare resolve path UX
# resolve must accept the stored wid-relative form without the <Name>/ prefix.
T22="$ROOT/t22"
make_tarballs "$T22" fake
write_projeny "$T22" fake 1.0 fake
(cd "$T22" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T22/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 10;")
open(p, "w").write(s)
EOF
(cd "$T22" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T22/fake.projeny" "$ROOT/t22-local.projeny"
rm -rf "$T22/fake" "$T22/.fake.projeny.status"
cp "$ROOT/t22-local.projeny" "$T22/fake.projeny"
(cd "$T22" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T22/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 999;")
open(p, "w").write(s)
EOF
U22="$ROOT/t22up"
mkdir -p "$U22"
cp "$T22/fake-1.0.tar.gz" "$U22/"
cp "$ROOT/t22-local.projeny" "$U22/fake.projeny"
(cd "$U22" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$U22/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 555;")
open(p, "w").write(s)
EOF
(cd "$U22" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$U22/fake.projeny" "$T22/fake.projeny"
(cd "$T22" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'int alpha = 1;\n\nint beta = 777;\n\nint gamma = 1;\n\nint delta = 1;\n' > "$T22/fake/src/a.c"
run_in "$T22" expect_ok "resolve accepts bare wid-relative path" "$PROJENY" resolve fake.projeny src/a.c
if grep -q "^Conflict:" "$T22/.fake.projeny.status"; then
    fail "bare resolve removes conflict entry" "$(cat "$T22/.fake.projeny.status")"
else
    ok "bare resolve removes conflict entry"
fi

# ----------------------------------------- 23. space/arrow filenames roundtrip
# Filenames with spaces and "->" used to produce un-applyable patches: git
# leaves them unquoted, the "diff --git" split misparsed, and the stored patch
# leaked absolute tmp labels with ---/+++ disagreeing. Commit two such files,
# wipe, and fresh-setup must reproduce them byte-identical.
T23="$ROOT/t23"
mkdir -p "$T23/w-1.0"
printf 'base\n' > "$T23/w-1.0/base.c"
(cd "$T23" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Space/arrow names.\n' > "$T23/w.projeny"
(cd "$T23" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'new\n' > "$T23/w/new -> file.c"
printf 'new2\n' > "$T23/w/my file.c"
cp "$T23/w/new -> file.c" "$ROOT/t23-expect-arrow.c"
cp "$T23/w/my file.c" "$ROOT/t23-expect-space.c"
run_in "$T23" expect_ok "add arrow-name file" "$PROJENY" add w.projeny "w/new -> file.c"
run_in "$T23" expect_ok "add space-name file" "$PROJENY" add w.projeny "w/my file.c"
run_in "$T23" expect_ok "commit space/arrow files" "$PROJENY" commit w.projeny
if grep -q "tmp" "$T23/w.projeny"; then
    fail "space/arrow commit stores no absolute tmp labels" "$(grep '^diff --git' "$T23/w.projeny")"
else
    ok "space/arrow commit stores no absolute tmp labels"
fi
# The stored patch must pass git's own consistency check (diff --git agrees
# with ---/+++). Extract the patch, relabel wid prefixes (as projeny does for
# -p1 application with cwd=tree), and check against the pristine base.
# Skipped when git or python3 is unavailable (projeny itself needs neither).
if command -v python3 >/dev/null 2>&1 && command -v git >/dev/null 2>&1; then
    (cd "$T23" && sed -n '/^diff --git /,$p' w.projeny > patch-extract.diff && mkdir -p check-base && tar -xzf w-1.0.tar.gz -C check-base && python3 - patch-extract.diff check-relabeled.diff <<'PYEOF'
import sys
s = open(sys.argv[1]).read()
s = s.replace("a/w/", "a/").replace("b/w/", "b/")
open(sys.argv[2], "w").write(s)
PYEOF
    )
    if (cd "$T23/check-base/w-1.0" && GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null GIT_CEILING_DIRECTORIES=/ GIT_DIR=/dev/null/no-such-projeny-repo GIT_WORK_TREE= git apply -p1 --whitespace=nowarn --check ../../check-relabeled.diff 2>/dev/null); then
        ok "space/arrow stored patch passes git apply --check"
    else
        fail "space/arrow stored patch passes git apply --check"
    fi
    rm -rf "$T23/check-base" "$T23/patch-extract.diff" "$T23/check-relabeled.diff"
elif command -v python3 >/dev/null 2>&1; then
    ok "space/arrow stored patch passes git apply --check (skipped: no git)"
fi
rm -rf "$T23/w" "$T23/.w.projeny.status"
run_in "$T23" expect_ok "fresh setup after space/arrow commit" "$PROJENY" setup w.projeny
if cmp -s "$T23/w/new -> file.c" "$ROOT/t23-expect-arrow.c"; then
    ok "arrow-name file identical after fresh setup"
else
    fail "arrow-name file identical after fresh setup" "ls: $(ls "$T23/w" 2>&1)"
fi
if cmp -s "$T23/w/my file.c" "$ROOT/t23-expect-space.c"; then
    ok "space-name file identical after fresh setup"
else
    fail "space-name file identical after fresh setup" "ls: $(ls "$T23/w" 2>&1)"
fi

# ----------------------------------------- 24. empty files roundtrip
T24="$ROOT/t24"
mkdir -p "$T24/w-1.0"
printf 'normal\n' > "$T24/w-1.0/n.c"
: > "$T24/w-1.0/empty.c"
(cd "$T24" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Empty files.\n' > "$T24/w.projeny"
run_in "$T24" expect_ok "setup with empty base file" "$PROJENY" setup w.projeny
if [ ! -f "$T24/w/empty.c" ]; then
    fail "empty base file unpacked"
else
    if [ -s "$T24/w/empty.c" ]; then
        fail "empty base file is really empty"
    else
        ok "empty base file is really empty"
    fi
fi
: > "$T24/w/newempty.c"
run_in "$T24" expect_ok "add empty file" "$PROJENY" add w.projeny w/newempty.c
run_in "$T24" expect_ok "commit empty file" "$PROJENY" commit w.projeny
expect_file_contains "empty commit stores new-file entry" "$T24/w.projeny" "new file mode"
rm -rf "$T24/w" "$T24/.w.projeny.status"
run_in "$T24" expect_ok "setup after empty commit" "$PROJENY" setup w.projeny
if [ -f "$T24/w/newempty.c" ] && [ ! -s "$T24/w/newempty.c" ]; then
    ok "empty added file survives re-setup"
else
    fail "empty added file survives re-setup" "ls: $(ls -l "$T24/w" 2>&1)"
fi
run_in "$T24" expect_ok "rm empty file" "$PROJENY" rm w.projeny w/empty.c
run_in "$T24" expect_ok "commit empty rm" "$PROJENY" commit w.projeny
rm -rf "$T24/w" "$T24/.w.projeny.status"
run_in "$T24" expect_ok "setup after empty rm" "$PROJENY" setup w.projeny
if [ ! -e "$T24/w/empty.c" ]; then
    ok "deleted empty file stays deleted"
else
    fail "deleted empty file stays deleted"
fi

# ----------------------------------------- 25. missing trailing newline
T25="$ROOT/t25"
mkdir -p "$T25/w-1.0"
printf 'aaa\nbbb' > "$T25/w-1.0/nonl.c"
printf 'x\n' > "$T25/w-1.0/nl.c"
(cd "$T25" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    No-newline files.\n' > "$T25/w.projeny"
run_in "$T25" expect_ok "no-newline setup" "$PROJENY" setup w.projeny
printf 'aaa\nBBB' > "$T25/w/nonl.c"
run_in "$T25" expect_ok "no-newline commit" "$PROJENY" commit w.projeny
expect_file_contains "no-newline commit stores marker" "$T25/w.projeny" "No newline at end of file"
cp "$T25/w/nonl.c" "$ROOT/t25-expect-nonl.c"
rm -rf "$T25/w" "$T25/.w.projeny.status"
run_in "$T25" expect_ok "setup after no-newline commit" "$PROJENY" setup w.projeny
if cmp -s "$T25/w/nonl.c" "$ROOT/t25-expect-nonl.c"; then
    ok "no-newline file byte-identical after re-setup"
else
    fail "no-newline file byte-identical after re-setup" "got: $(xxd "$T25/w/nonl.c" 2>&1)"
fi
printf 'brand new no newline' > "$T25/w/added-no-nl.c"
run_in "$T25" expect_ok "add no-newline file" "$PROJENY" add w.projeny w/added-no-nl.c
run_in "$T25" expect_ok "commit no-newline add" "$PROJENY" commit w.projeny
cp "$T25/w/added-no-nl.c" "$ROOT/t25-expect-added.c"
rm -rf "$T25/w" "$T25/.w.projeny.status"
run_in "$T25" expect_ok "setup after no-newline add" "$PROJENY" setup w.projeny
if cmp -s "$T25/w/added-no-nl.c" "$ROOT/t25-expect-added.c"; then
    ok "added no-newline file byte-identical after re-setup"
else
    fail "added no-newline file byte-identical after re-setup"
fi

# ----------------------------------------- 26. CRLF line endings roundtrip
T26="$ROOT/t26"
mkdir -p "$T26/w-1.0"
printf 'line1\r\nline2\r\nline3\r\n' > "$T26/w-1.0/crlf.c"
printf 'plain\n' > "$T26/w-1.0/n.c"
(cd "$T26" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    CRLF files.\n' > "$T26/w.projeny"
run_in "$T26" expect_ok "crlf setup" "$PROJENY" setup w.projeny
printf 'line1\r\nLINE2\r\nline3\r\n' > "$T26/w/crlf.c"
run_in "$T26" expect_ok "crlf commit" "$PROJENY" commit w.projeny
if grep -q "$(printf '\r')" "$T26/w.projeny"; then
    ok "crlf commit preserves CR bytes in patch"
else
    fail "crlf commit preserves CR bytes in patch"
fi
cp "$T26/w/crlf.c" "$ROOT/t26-expect-crlf.c"
rm -rf "$T26/w" "$T26/.w.projeny.status"
run_in "$T26" expect_ok "setup after crlf commit" "$PROJENY" setup w.projeny
if cmp -s "$T26/w/crlf.c" "$ROOT/t26-expect-crlf.c"; then
    ok "crlf file byte-identical after re-setup"
else
    fail "crlf file byte-identical after re-setup" "got: $(xxd "$T26/w/crlf.c" 2>&1)"
fi

# ----------------------------------------- 27. large file, distant edits
T27="$ROOT/t27"
mkdir -p "$T27/w-1.0"
seq 1 5000 > "$T27/w-1.0/big.txt"
(cd "$T27" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Large file.\n' > "$T27/w.projeny"
run_in "$T27" expect_ok "large setup" "$PROJENY" setup w.projeny
python3 - "$T27/w/big.txt" <<'EOF'
import sys
p = sys.argv[1]
ls = open(p).read().split("\n")
ls[99] = "CHANGED-100"
ls[4899] = "CHANGED-4900"
open(p, "w").write("\n".join(ls))
EOF
run_in "$T27" expect_ok "large commit" "$PROJENY" commit w.projeny
n_hunks="$(awk '/^diff --git /{f=1} f&&/^@@ /{c++} END{print c+0}' "$T27/w.projeny")"
if [ "$n_hunks" -ge 2 ]; then
    ok "large distant edits produce separate hunks ($n_hunks)"
else
    fail "large distant edits produce separate hunks" "found $n_hunks"
fi
cp "$T27/w/big.txt" "$ROOT/t27-expect-big.txt"
rm -rf "$T27/w" "$T27/.w.projeny.status"
run_in "$T27" expect_ok "setup after large commit" "$PROJENY" setup w.projeny
if cmp -s "$T27/w/big.txt" "$ROOT/t27-expect-big.txt"; then
    ok "large file byte-identical after re-setup"
else
    fail "large file byte-identical after re-setup"
fi

# ----------------------------------------- 28. close edits share one hunk
T28="$ROOT/t28"
mkdir -p "$T28/w-1.0"
seq 1 30 > "$T28/w-1.0/n.txt"
(cd "$T28" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Hunk merging.\n' > "$T28/w.projeny"
(cd "$T28" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$T28/w/n.txt" <<'EOF'
import sys
p = sys.argv[1]
ls = open(p).read().split("\n")
ls[9] = "C10"
ls[13] = "C14"
open(p, "w").write("\n".join(ls))
EOF
run_in "$T28" expect_ok "close-edit commit" "$PROJENY" commit w.projeny
n_hunks="$(awk '/^diff --git /{f=1} f&&/^@@ /{c++} END{print c+0}' "$T28/w.projeny")"
if [ "$n_hunks" -eq 1 ]; then
    ok "close edits merge into one hunk"
else
    fail "close edits merge into one hunk" "found $n_hunks"
fi
cp "$T28/w/n.txt" "$ROOT/t28-expect-n.txt"
rm -rf "$T28/w" "$T28/.w.projeny.status"
run_in "$T28" expect_ok "setup after close-edit commit" "$PROJENY" setup w.projeny
if cmp -s "$T28/w/n.txt" "$ROOT/t28-expect-n.txt"; then
    ok "close-edit file identical after re-setup"
else
    fail "close-edit file identical after re-setup"
fi

# ----------------------------------------- 29. binary files are tracked

# Untracked binaries stay out of patches until explicitly added (like git
# leaves untracked files out); once added they commit as base64 binary
# blocks and roundtrip byte-identical through a fresh setup.
T29="$ROOT/t29"
make_tarballs "$T29" fake
write_projeny "$T29" fake 1.0 fake
(cd "$T29" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 -c "open('$T29/fake/src/bin.dat','wb').write(b'ab\x00cd\n')"
run_in "$T29" expect_ok "commit ignores untracked binary" "$PROJENY" commit fake.projeny
if grep -q "bin.dat" "$T29/fake.projeny"; then
    fail "untracked binary stays out of the patch" "$(grep "bin.dat" "$T29/fake.projeny")"
else
    ok "untracked binary stays out of the patch"
fi
run_in "$T29" expect_ok "add of binary succeeds" "$PROJENY" add fake.projeny fake/src/bin.dat
run_in "$T29" expect_ok "commit of added binary succeeds" "$PROJENY" commit fake.projeny
expect_file_contains "added binary is stored as a binary block" "$T29/fake.projeny" "GIT binary patch"
expect_file_contains "added binary names the file" "$T29/fake.projeny" "bin.dat"
python3 -c "open('$ROOT/t29-expect.bin','wb').write(open('$T29/fake/src/bin.dat','rb').read())"
rm -rf "$T29/fake" "$T29/.fake.projeny.status"
run_in "$T29" expect_ok "setup after binary commit" "$PROJENY" setup fake.projeny
if cmp -s "$T29/fake/src/bin.dat" "$ROOT/t29-expect.bin"; then
    ok "binary add byte-identical after re-setup"
else
    fail "binary add byte-identical after re-setup"
fi
run_in "$T29" expect_ok "commit works with binary present" "$PROJENY" commit fake.projeny

# ----------------------------------------- 30. executable bit roundtrips
T30="$ROOT/t30"
mkdir -p "$T30/w-1.0"
printf 'echo hi\n' > "$T30/w-1.0/run.sh"
printf 'code\n' > "$T30/w-1.0/f.c"
(cd "$T30" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Modes.\n' > "$T30/w.projeny"
run_in "$T30" expect_ok "mode setup" "$PROJENY" setup w.projeny
chmod 755 "$T30/w/run.sh"
printf 'echo HI\n' > "$T30/w/run.sh"
chmod 755 "$T30/w/run.sh"
run_in "$T30" expect_ok "mode+content commit" "$PROJENY" commit w.projeny
expect_file_contains "mode commit stores new mode" "$T30/w.projeny" "new mode 100755"
rm -rf "$T30/w" "$T30/.w.projeny.status"
run_in "$T30" expect_ok "setup after mode commit" "$PROJENY" setup w.projeny
if [ -x "$T30/w/run.sh" ]; then
    ok "executable bit survives re-setup"
else
    fail "executable bit survives re-setup" "$(ls -l "$T30/w/run.sh" 2>&1)"
fi
expect_file_contains "mode commit keeps content" "$T30/w/run.sh" "echo HI"
# Content-only change on an executable file must not drop +x.
chmod 755 "$T30/w/run.sh"
printf 'echo YO\n' > "$T30/w/run.sh"
chmod 755 "$T30/w/run.sh"
run_in "$T30" expect_ok "second content commit" "$PROJENY" commit w.projeny
rm -rf "$T30/w" "$T30/.w.projeny.status"
run_in "$T30" expect_ok "setup after second commit" "$PROJENY" setup w.projeny
if [ -x "$T30/w/run.sh" ]; then
    ok "content-only change keeps +x after re-setup"
else
    fail "content-only change keeps +x after re-setup" "$(ls -l "$T30/w/run.sh" 2>&1)"
fi

# ----------------------------------------- 31. patch compatibility
# (a) wid-stripped (plain a/b-label) patches still set up via projeny
# (no git needed); (b) stored patches pass git apply / patch -p1 checks
# when those helpers exist (projeny itself needs neither).
T31="$ROOT/t31"
make_tarballs "$T31" fake
write_projeny "$T31" fake 1.0 fake
(cd "$T31" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T31/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 4242;")
open(p, "w").write(s)
EOF
(cd "$T31" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
mkdir -p "$T31/s-top-tmp"
(cd "$T31" && tar -xzf fake-1.0.tar.gz && mv fake-1.0 s-top && tar -czf s.tar.gz s-top && rm -rf s-top)
cat > "$T31/s.projeny" <<'EOF'
Archive: s.tar.gz
Origname: s-top
Name: s

    Stripped-label fixture.

EOF
(sed -n '/^diff --git /,$p' "$T31/fake.projeny" | sed 's|a/fake/|a/|g; s|b/fake/|b/|g' >> "$T31/s.projeny")
run_in "$T31" expect_ok "setup with plain a/b-label patch" "$PROJENY" setup s.projeny
expect_file_contains "plain-label setup applies content" "$T31/s/src/a.c" "beta = 4242"
# Hand-written p0-style patch (no a/ b/ prefixes at all).
cat > "$T31/h.projeny" <<'EOF'
Archive: s.tar.gz
Origname: s-top
Name: h

    Hand-written fixture.

diff --git a/h/README b/h/README
--- README
+++ README
@@ -1,1 +1,1 @@
-hello v1
+hello HAND
EOF
run_in "$T31" expect_ok "setup with hand-written p0 patch" "$PROJENY" setup h.projeny
expect_file_contains "p0 patch applies content" "$T31/h/README" "hello HAND"
if command -v git >/dev/null 2>&1; then
    (cd "$T31" && sed -n '/^diff --git /,$p' fake.projeny > compat-git.diff && sed -i 's|a/fake/|a/|g; s|b/fake/|b/|g' compat-git.diff && rm -rf compat-base && mkdir compat-base && tar -xzf fake-1.0.tar.gz -C compat-base)
    if (cd "$T31/compat-base/fake-1.0" && git apply -p1 --whitespace=nowarn --check ../../compat-git.diff 2>/dev/null); then
        ok "stored patch passes git apply --check"
    else
        fail "stored patch passes git apply --check"
    fi
    rm -rf "$T31/compat-base" "$T31/compat-git.diff"
else
    ok "stored patch passes git apply --check (skipped: no git)"
fi
if command -v patch >/dev/null 2>&1; then
    (cd "$T31" && sed -n '/^diff --git /,$p' fake.projeny > compat-patch.diff && sed -i 's|a/fake/|a/|g; s|b/fake/|b/|g' compat-patch.diff && rm -rf compat-pbase && mkdir compat-pbase && tar -xzf fake-1.0.tar.gz -C compat-pbase)
    if (cd "$T31/compat-pbase/fake-1.0" && patch -p1 --dry-run < ../../compat-patch.diff >/dev/null 2>&1); then
        ok "stored patch passes patch -p1 --dry-run"
    else
        fail "stored patch passes patch -p1 --dry-run"
    fi
    rm -rf "$T31/compat-pbase" "$T31/compat-patch.diff"
else
    ok "stored patch passes patch -p1 --dry-run (skipped: no patch)"
fi

# ----------------------------------------- 32. applier tolerates bad hunk offsets
T32="$ROOT/t32"
make_tarballs "$T32" fake
write_projeny "$T32" fake 1.0 fake
(cd "$T32" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T32/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int gamma = 1;", "int gamma = 7777;")
open(p, "w").write(s)
EOF
(cd "$T32" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
python3 - "$T32/fake.projeny" <<'PYEOF'
import re, sys
p = sys.argv[1]
s = open(p).read()
def bump(m):
    return "@@ -%d,%s +%s,%s @@" % (int(m.group(1)) + 6, m.group(2), m.group(3), m.group(4))
s2 = re.sub(r"@@ -(\d+),(\d+) \+(\d+),(\d+) @@", bump, s, count=1)
assert s2 != s
open(p, "w").write(s2)
PYEOF
cp "$T32/fake/src/a.c" "$ROOT/t32-expect-a.c" 2>/dev/null || true
rm -rf "$T32/fake" "$T32/.fake.projeny.status"
run_in "$T32" expect_ok "setup applies patch with shifted hunk offsets" "$PROJENY" setup fake.projeny
expect_file_contains "shifted-offset setup yields right content" "$T32/fake/src/a.c" "gamma = 7777"

# ----------------------------------------- 33. rename with modification
T33="$ROOT/t33"
mkdir -p "$T33/w-1.0"
seq 1 10 > "$T33/w-1.0/nums.txt"
(cd "$T33" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Rename+edit.\n' > "$T33/w.projeny"
(cd "$T33" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
run_in "$T33" expect_ok "mv for rename+edit" "$PROJENY" mv w.projeny w/nums.txt w/digits.txt
python3 - "$T33/w/digits.txt" <<'EOF'
import sys
p = sys.argv[1]
ls = open(p).read().split("\n")
ls[4] = "FIVE"
open(p, "w").write("\n".join(ls))
EOF
run_in "$T33" expect_ok "commit rename+edit" "$PROJENY" commit w.projeny
expect_file_contains "rename+edit stores rename" "$T33/w.projeny" "rename from"
expect_file_contains "rename+edit stores content hunk" "$T33/w.projeny" "FIVE"
rm -rf "$T33/w" "$T33/.w.projeny.status"
run_in "$T33" expect_ok "setup after rename+edit" "$PROJENY" setup w.projeny
if [ -f "$T33/w/digits.txt" ] && [ ! -e "$T33/w/nums.txt" ]; then
    ok "rename+edit paths correct after re-setup"
else
    fail "rename+edit paths correct after re-setup" "ls: $(ls "$T33/w" 2>&1)"
fi
expect_file_contains "rename+edit content correct" "$T33/w/digits.txt" "FIVE"

# ----------------------------------------- 34. manual delete+add of same content
T34="$ROOT/t34"
mkdir -p "$T34/w-1.0"
printf 'identical bytes\n' > "$T34/w-1.0/oldname.txt"
(cd "$T34" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Manual rename.\n' > "$T34/w.projeny"
(cd "$T34" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
run_in "$T34" expect_ok "rm old for manual rename" "$PROJENY" rm w.projeny w/oldname.txt
printf 'identical bytes\n' > "$T34/w/newname.txt"
run_in "$T34" expect_ok "add new for manual rename" "$PROJENY" add w.projeny w/newname.txt
run_in "$T34" expect_ok "commit manual rename" "$PROJENY" commit w.projeny
expect_file_contains "manual rename detected as rename" "$T34/w.projeny" "rename from"
rm -rf "$T34/w" "$T34/.w.projeny.status"
run_in "$T34" expect_ok "setup after manual rename" "$PROJENY" setup w.projeny
if [ -f "$T34/w/newname.txt" ] && [ ! -e "$T34/w/oldname.txt" ]; then
    ok "manual rename survives re-setup"
else
    fail "manual rename survives re-setup" "ls: $(ls "$T34/w" 2>&1)"
fi

# ----------------------------------------- 35. CLI error paths and help
T35="$ROOT/t35"
make_tarballs "$T35" fake
write_projeny "$T35" fake 1.0 fake
(cd "$T35" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T35" expect_fail "add missing file fails" "$PROJENY" add fake.projeny fake/src/nope.c
run_in "$T35" expect_fail "mv onto existing fails" "$PROJENY" mv fake.projeny fake/src/a.c fake/src/b.c
run_in "$T35" expect_fail "mv same src/dst fails" "$PROJENY" mv fake.projeny fake/src/a.c fake/src/a.c
run_in "$T35" expect_fail "resolve non-conflict fails" "$PROJENY" resolve fake.projeny fake/src/a.c
run_in "$T35" expect_fail "status of missing setup fails" "$PROJENY" status "$T35/never.projeny"
run_in "$T35" expect_ok "help exits 0" "$PROJENY" help
run_in "$T35" expect_fail "unknown command fails" "$PROJENY" frobnicate fake.projeny
run_in "$T35" expect_fail "setup with missing tarball fails" "$PROJENY" setup "$T35/never.projeny"
run_in "$T35" expect_ok "commit with no changes succeeds" "$PROJENY" commit fake.projeny
if grep -q "^diff --git " "$T35/fake.projeny"; then
    fail "no-change commit stores empty patch"
else
    ok "no-change commit stores empty patch"
fi
run_in "$T35" expect_ok "status shows setup" "$PROJENY" status fake.projeny

# ----------------------------------------- 36. symlinks roundtrip
T36="$ROOT/t36"
mkdir -p "$T36/w-1.0"
printf 'target content\n' > "$T36/w-1.0/real.txt"
ln -s real.txt "$T36/w-1.0/link"
(cd "$T36" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Symlinks.\n' > "$T36/w.projeny"
run_in "$T36" expect_ok "symlink setup" "$PROJENY" setup w.projeny
if [ -L "$T36/w/link" ] && [ "$(readlink "$T36/w/link")" = "real.txt" ]; then
    ok "symlink unpacked with target intact"
else
    fail "symlink unpacked with target intact" "ls: $(ls -l "$T36/w" 2>&1)"
fi
printf 'other\n' > "$T36/w/other.txt"
run_in "$T36" expect_ok "add symlink neighbor" "$PROJENY" add w.projeny w/other.txt
ln -sf other.txt "$T36/w/link"
run_in "$T36" expect_ok "commit retargeted symlink" "$PROJENY" commit w.projeny
expect_file_contains "symlink retarget stored" "$T36/w.projeny" "other.txt"
rm -rf "$T36/w" "$T36/.w.projeny.status"
run_in "$T36" expect_ok "setup after symlink commit" "$PROJENY" setup w.projeny
if [ -L "$T36/w/link" ] && [ "$(readlink "$T36/w/link")" = "other.txt" ]; then
    ok "retargeted symlink survives re-setup"
else
    fail "retargeted symlink survives re-setup" "ls: $(ls -l "$T36/w" 2>&1)"
fi
# In-tree ".." link targets are kept: the check resolves the target against
# the tree instead of string-matching "..", so "sub/../f.c" (which resolves
# back inside e-1.0) unpacks fine.
T36B="$ROOT/t36b"
mkdir -p "$T36B/e-1.0/sub"
printf 'x\n' > "$T36B/e-1.0/f.c"
ln -s sub/../f.c "$T36B/e-1.0/esc"
(cd "$T36B" && tar -czf e-1.0.tar.gz e-1.0)
printf 'Archive: e-1.0.tar.gz\nOrigname: e-1.0\nName: e\n\n    Escape.\n' > "$T36B/e.projeny"
run_in "$T36B" expect_ok "in-tree dotdot symlink target unpacks" "$PROJENY" setup e.projeny
if [ -L "$T36B/e/esc" ] && [ "$(readlink "$T36B/e/esc")" = "sub/../f.c" ] && \
   [ "$(cat "$T36B/e/esc")" = "x" ]; then
    ok "in-tree dotdot link unpacked with target intact"
else
    fail "in-tree dotdot link unpacked with target intact" "$(ls -l "$T36B/e" 2>&1)"
fi
# A true escape (resolving above the tree root) is still rejected at unpack.
T36D="$ROOT/t36d"
mkdir -p "$T36D/f-1.0"
printf 'x\n' > "$T36D/f-1.0/f.c"
ln -s ../../escape "$T36D/f-1.0/esc"
(cd "$T36D" && tar -czf f-1.0.tar.gz f-1.0)
printf 'Archive: f-1.0.tar.gz\nOrigname: f-1.0\nName: f\n\n    Escape.\n' > "$T36D/f.projeny"
run_in "$T36D" expect_fail "escaping dotdot symlink target hard-errors" "$PROJENY" setup f.projeny

# An archive symlink whose ".." target resolves inside the tree survives the
# whole setup -> commit -> package -> extract roundtrip with the link intact.
T36C="$ROOT/t36c"
mkdir -p "$T36C/s-1.0/sub"
printf 'root\n' > "$T36C/s-1.0/rootfile"
ln -s ../rootfile "$T36C/s-1.0/sub/link"
(cd "$T36C" && tar -czf s-1.0.tar.gz s-1.0)
printf 'Archive: s-1.0.tar.gz\nOrigname: s-1.0\nName: s\n\n    Dotdot roundtrip.\n' > "$T36C/s.projeny"
run_in "$T36C" expect_ok "dotdot link archive setup" "$PROJENY" setup s.projeny
if [ -L "$T36C/s/sub/link" ] && [ "$(readlink "$T36C/s/sub/link")" = "../rootfile" ]; then
    ok "dotdot link unpacked with target intact"
else
    fail "dotdot link unpacked with target intact" "$(ls -l "$T36C/s/sub" 2>&1)"
fi
run_in "$T36C" expect_ok "commit collects dotdot link" "$PROJENY" commit s.projeny
run_in "$T36C" expect_ok "package dotdot link tree" "$PROJENY" package s.projeny s-out.tar.gz
mkdir -p "$T36C/unpack" && (cd "$T36C/unpack" && tar -xzf ../s-out.tar.gz)
if [ -L "$T36C/unpack/s-out/sub/link" ] && \
   [ "$(readlink "$T36C/unpack/s-out/sub/link")" = "../rootfile" ] && \
   [ "$(cat "$T36C/unpack/s-out/sub/link")" = "root" ]; then
    ok "dotdot link survives package roundtrip"
else
    fail "dotdot link survives package roundtrip" "$(ls -l "$T36C/unpack/s-out/sub" 2>&1)"
fi
run_in "$T36C" expect_ok "extract dotdot link tree" "$PROJENY" extract s.projeny extracted
if [ -L "$T36C/extracted/sub/link" ] && \
   [ "$(readlink "$T36C/extracted/sub/link")" = "../rootfile" ] && \
   [ "$(cat "$T36C/extracted/sub/link")" = "root" ]; then
    ok "dotdot link survives extract"
else
    fail "dotdot link survives extract" "$(ls -l "$T36C/extracted/sub" 2>&1)"
fi

# ----------------------------------------- 37. adjacent hunks stay exact
T37="$ROOT/t37"
mkdir -p "$T37/w-1.0"
seq 1 40 > "$T37/w-1.0/n.txt"
(cd "$T37" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Adjacent.\n' > "$T37/w.projeny"
(cd "$T37" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$T37/w/n.txt" <<'EOF'
import sys
p = sys.argv[1]
ls = open(p).read().split("\n")
ls[9] = "C10"
ls[19] = "C20"
open(p, "w").write("\n".join(ls))
EOF
run_in "$T37" expect_ok "distant-edit commit" "$PROJENY" commit w.projeny
n_hunks="$(awk '/^diff --git /{f=1} f&&/^@@ /{c++} END{print c+0}' "$T37/w.projeny")"
if [ "$n_hunks" -eq 2 ]; then
    ok "edits 10 lines apart give two hunks"
else
    fail "edits 10 lines apart give two hunks" "found $n_hunks"
fi
cp "$T37/w/n.txt" "$ROOT/t37-expect-n.txt"
rm -rf "$T37/w" "$T37/.w.projeny.status"
run_in "$T37" expect_ok "setup after distant edits" "$PROJENY" setup w.projeny
if cmp -s "$T37/w/n.txt" "$ROOT/t37-expect-n.txt"; then
    ok "distant-edit file identical after re-setup"
else
    fail "distant-edit file identical after re-setup"
fi

# ----------------------------------------- 38. new executable file keeps +x
T38="$ROOT/t38"
mkdir -p "$T38/w-1.0"
printf 'base\n' > "$T38/w-1.0/b.c"
(cd "$T38" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    New exec.\n' > "$T38/w.projeny"
(cd "$T38" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf '#!/bin/sh\necho new\n' > "$T38/w/tool.sh"
chmod 755 "$T38/w/tool.sh"
run_in "$T38" expect_ok "add executable file" "$PROJENY" add w.projeny w/tool.sh
run_in "$T38" expect_ok "commit executable file" "$PROJENY" commit w.projeny
expect_file_contains "new exec stores mode" "$T38/w.projeny" "new file mode 100755"
rm -rf "$T38/w" "$T38/.w.projeny.status"
run_in "$T38" expect_ok "setup after new-exec commit" "$PROJENY" setup w.projeny
if [ -x "$T38/w/tool.sh" ]; then
    ok "new executable file keeps +x after re-setup"
else
    fail "new executable file keeps +x after re-setup" "$(ls -l "$T38/w/tool.sh" 2>&1)"
fi
expect_file_contains "new exec content correct" "$T38/w/tool.sh" "echo new"

# ----------------------------------------- 39. extra headers survive commit
T39="$ROOT/t39"
make_tarballs "$T39" fake
write_projeny "$T39" fake 1.0 fake
python3 - "$T39/fake.projeny" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("Name: fake\n", "Name: fake\nX-Custom: yes\n")
open(p, "w").write(s)
EOF
(cd "$T39" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T39/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int alpha = 1;", "int alpha = 31337;")
open(p, "w").write(s)
EOF
run_in "$T39" expect_ok "commit with extra header" "$PROJENY" commit fake.projeny
expect_file_contains "extra header preserved" "$T39/fake.projeny" "X-Custom: yes"
expect_file_contains "extra-header commit stores diff" "$T39/fake.projeny" "31337"

# ----------------------------------------- 40. quoting in filenames roundtrip
T40="$ROOT/t40"
mkdir -p "$T40/w-1.0"
printf 'base\n' > "$T40/w-1.0/base.c"
(cd "$T40" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Quoting.\n' > "$T40/w.projeny"
(cd "$T40" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'quoted\n' > "$T40/w/say \"hi\".c"
printf 'tabbed\n' > "$T40/w/with	tab.c"
cp "$T40/w/say \"hi\".c" "$ROOT/t40-expect-quote.c"
cp "$T40/w/with	tab.c" "$ROOT/t40-expect-tab.c"
run_in "$T40" expect_ok "add quoted-name file" "$PROJENY" add w.projeny 'w/say "hi".c'
run_in "$T40" expect_ok "add tab-name file" "$PROJENY" add w.projeny 'w/with	tab.c'
run_in "$T40" expect_ok "commit quoted names" "$PROJENY" commit w.projeny
rm -rf "$T40/w" "$T40/.w.projeny.status"
run_in "$T40" expect_ok "setup after quoted commit" "$PROJENY" setup w.projeny
if cmp -s "$T40/w/say \"hi\".c" "$ROOT/t40-expect-quote.c"; then
    ok "quoted-name file identical after re-setup"
else
    fail "quoted-name file identical after re-setup" "ls: $(ls "$T40/w" 2>&1)"
fi
if cmp -s "$T40/w/with	tab.c" "$ROOT/t40-expect-tab.c"; then
    ok "tab-name file identical after re-setup"
else
    fail "tab-name file identical after re-setup" "ls: $(ls "$T40/w" 2>&1)"
fi

# ----------------------------------------- 41. long single-line file
T41="$ROOT/t41"
mkdir -p "$T41/w-1.0"
python3 -c "open('$T41/w-1.0/long.txt','w').write('A'*100000 + '\n')"
(cd "$T41" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Long line.\n' > "$T41/w.projeny"
(cd "$T41" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$T41/w/long.txt','w').write('B'*100000 + '\n')"
run_in "$T41" expect_ok "long-line commit" "$PROJENY" commit w.projeny
cp "$T41/w/long.txt" "$ROOT/t41-expect-long.txt"
rm -rf "$T41/w" "$T41/.w.projeny.status"
run_in "$T41" expect_ok "setup after long-line commit" "$PROJENY" setup w.projeny
if cmp -s "$T41/w/long.txt" "$ROOT/t41-expect-long.txt"; then
    ok "long single-line file identical after re-setup"
else
    fail "long single-line file identical after re-setup"
fi

# ----------------------------------------- 42. tabs and trailing tabs roundtrip
T42="$ROOT/t42"
mkdir -p "$T42/w-1.0"
printf 'a\n' > "$T42/w-1.0/t.c"
(cd "$T42" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Tabs.\n' > "$T42/w.projeny"
(cd "$T42" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf '\tindented\nmid\tdle\ntrailing\t\n' > "$T42/w/tabs.c"
run_in "$T42" expect_ok "add tab-content file" "$PROJENY" add w.projeny w/tabs.c
run_in "$T42" expect_ok "commit tab-content file" "$PROJENY" commit w.projeny
cp "$T42/w/tabs.c" "$ROOT/t42-expect-tabs.c"
rm -rf "$T42/w" "$T42/.w.projeny.status"
run_in "$T42" expect_ok "setup after tab commit" "$PROJENY" setup w.projeny
if cmp -s "$T42/w/tabs.c" "$ROOT/t42-expect-tabs.c"; then
    ok "tab-content file byte-identical after re-setup"
else
    fail "tab-content file byte-identical after re-setup" "got: $(cat -A "$T42/w/tabs.c" 2>&1)"
fi

# ----------------------------------------- 43. dual-parser unification

# Combined, p0 and mode-only patches must not diverge into OOB/wrong names:

# a combined diff is a precise per-file failure (not silent success).

T43="$ROOT/t43"

mkdir -p "$T43/w-1.0"

printf 'hello\n' > "$T43/w-1.0/f.c"

(cd "$T43" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Dual parser.\n' > "$T43/w.projeny"

cat >> "$T43/w.projeny" <<'EOF'
diff --git a/w/f.c b/w/f.c
--- a/w/f.c
+++ b/w/f.c
@@ -1,1 +1,1 @@
-hello
+hello patched
diff --cc f.c
index 1111111,2222222..0000000
--- a/f.c
+++ b/f.c
@@@ -1,1 -1,1 +1,1 @@@
-hello
 -hello2
++hello patched
EOF

out="$(cd "$T43" && "$PROJENY" setup w.projeny 2>&1)"

rc=$?

if [ $rc -ne 0 ]; then

    ok "combined block fails cleanly (no OOB/silent success)"

else

    fail "combined block fails cleanly (no OOB/silent success)" "exited 0"

fi

case "$out" in

*f.c*)

    ok "combined failure names the file precisely"

    ;;

*)

    fail "combined failure names the file precisely" "out: $out"

    ;;

esac

# p0 hand-written form (no a/b prefixes on ---/+++) still applies.

T43B="$ROOT/t43b"

mkdir -p "$T43B/w-1.0"

printf 'hello v1\n' > "$T43B/w-1.0/README"

(cd "$T43B" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

cat > "$T43B/h.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: h

    Hand-written p0.

diff --git a/h/README b/h/README
--- README
+++ README
@@ -1,1 +1,1 @@
-hello v1
+hello HAND
EOF

run_in "$T43B" expect_ok "p0 patch setup succeeds" "$PROJENY" setup h.projeny

expect_file_contains "p0 patch applies content" "$T43B/h/README" "hello HAND"

# Mode-only patch (no ---/+++/hunks) applies and sets the bit.

T43C="$ROOT/t43c"

mkdir -p "$T43C/w-1.0"

printf 'code\n' > "$T43C/w-1.0/f.c"

(cd "$T43C" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Mode only.\n' > "$T43C/w.projeny"

cat >> "$T43C/w.projeny" <<'EOF'

diff --git a/w/f.c b/w/f.c

old mode 100644

new mode 100755

EOF

run_in "$T43C" expect_ok "mode-only patch setup succeeds" "$PROJENY" setup w.projeny

if [ -x "$T43C/w/f.c" ]; then

    ok "mode-only patch sets +x"

else

    fail "mode-only patch sets +x" "$(ls -l "$T43C/w/f.c" 2>&1)"

fi

# ----------------------------------------- 44. pure-rename lineage

# Same-content destination is idempotent success; different content fails

# without overwriting.

T44="$ROOT/t44"

mkdir -p "$T44/w-1.0"

printf 'same\n' > "$T44/w-1.0/old.txt"

printf 'same\n' > "$T44/w-1.0/new.txt"

(cd "$T44" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Rename.\n' > "$T44/w.projeny"

cat >> "$T44/w.projeny" <<'EOF'

diff --git a/w/old.txt b/w/new.txt

similarity index 100%

rename from old.txt

rename to new.txt

EOF

run_in "$T44" expect_ok "pure-rename same-content dest succeeds" "$PROJENY" setup w.projeny

if [ -f "$T44/w/new.txt" ]; then

    ok "pure-rename same-content keeps dest"

else

    fail "pure-rename same-content keeps dest" "ls: $(ls "$T44/w" 2>&1)"

fi

if grep -q "same" "$T44/w/new.txt"; then

    ok "pure-rename same-content dest bytes intact"

else

    fail "pure-rename same-content dest bytes intact"

fi

T44B="$ROOT/t44b"

mkdir -p "$T44B/w-1.0"

printf 'same\n' > "$T44B/w-1.0/old.txt"

printf 'DIFFERENT\n' > "$T44B/w-1.0/new.txt"

(cd "$T44B" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Rename.\n' > "$T44B/w.projeny"

cat >> "$T44B/w.projeny" <<'EOF'

diff --git a/w/old.txt b/w/new.txt

similarity index 100%

rename from old.txt

rename to new.txt

EOF

run_in "$T44B" expect_fail "pure-rename different-content dest fails" "$PROJENY" setup w.projeny

# ----------------------------------------- 45. Already enforces mode/newline

# New-file Already with correct content but wrong +x must repair the bit on

# rebase onto a base that already contains the file.

T45="$ROOT/t45"

mkdir -p "$T45/w-1.0"

printf 'hello\n' > "$T45/w-1.0/f.c"

(cd "$T45" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Already mode.\n' > "$T45/w.projeny"

(cd "$T45" && "$PROJENY" setup w.projeny >/dev/null 2>&1)

printf 'hello new\n' > "$T45/w/newfile.c"

chmod 755 "$T45/w/newfile.c"

(cd "$T45" && "$PROJENY" add w.projeny w/newfile.c >/dev/null 2>&1)

(cd "$T45" && "$PROJENY" commit w.projeny >/dev/null 2>&1)

mkdir -p "$T45/w-2.0"

printf 'hello\n' > "$T45/w-2.0/f.c"

printf 'hello new\n' > "$T45/w-2.0/newfile.c"

chmod 644 "$T45/w-2.0/newfile.c"

(cd "$T45" && tar -czf w-2.0.tar.gz w-2.0 && rm -rf w-2.0)

run_in "$T45" expect_ok "rebase onto drifted new-file mode" "$PROJENY" rebase w.projeny w-2.0.tar.gz

if [ -x "$T45/w/newfile.c" ]; then

    ok "Already new-file repairs +x"

else

    fail "Already new-file repairs +x" "$(ls -l "$T45/w/newfile.c" 2>&1)"

fi

# Modify Already with correct lines but wrong +x and missing newline must

# repair both on rebase.

T45B="$ROOT/t45b"

mkdir -p "$T45B/w-1.0"

printf 'line1\nline2\nline3\n' > "$T45B/w-1.0/f.c"

(cd "$T45B" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Already mod.\n' > "$T45B/w.projeny"

(cd "$T45B" && "$PROJENY" setup w.projeny >/dev/null 2>&1)

printf 'line1\nLINE2\nline3\n' > "$T45B/w/f.c"

chmod 755 "$T45B/w/f.c"

(cd "$T45B" && "$PROJENY" commit w.projeny >/dev/null 2>&1)

mkdir -p "$T45B/w-2.0"

printf 'line1\nLINE2\nline3' > "$T45B/w-2.0/f.c"

chmod 755 "$T45B/w-2.0/f.c"

(cd "$T45B" && tar -czf w-2.0.tar.gz w-2.0 && rm -rf w-2.0)

run_in "$T45B" expect_ok "rebase onto drifted newline" "$PROJENY" rebase w.projeny w-2.0.tar.gz

printf 'line1\nLINE2\nline3\n' > "$ROOT/t45b-expect.c"

if cmp -s "$T45B/w/f.c" "$ROOT/t45b-expect.c"; then

    ok "Already modify repairs trailing newline"

else

    fail "Already modify repairs trailing newline" "got: $(xxd "$T45B/w/f.c" 2>&1)"

fi

# Independent fixture for the +x drift (same patch shape, fresh base).

T45C="$ROOT/t45c"

mkdir -p "$T45C/w-1.0"

printf 'line1\nline2\nline3\n' > "$T45C/w-1.0/f.c"

(cd "$T45C" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Already mod.\n' > "$T45C/w.projeny"

(cd "$T45C" && "$PROJENY" setup w.projeny >/dev/null 2>&1)

printf 'line1\nLINE2\nline3\n' > "$T45C/w/f.c"

chmod 755 "$T45C/w/f.c"

(cd "$T45C" && "$PROJENY" commit w.projeny >/dev/null 2>&1)

mkdir -p "$T45C/w-2.0"

printf 'line1\nLINE2\nline3\n' > "$T45C/w-2.0/f.c"

chmod 644 "$T45C/w-2.0/f.c"

(cd "$T45C" && tar -czf w-2.0.tar.gz w-2.0 && rm -rf w-2.0)

run_in "$T45C" expect_ok "rebase onto drifted mode bit" "$PROJENY" rebase w.projeny w-2.0.tar.gz

if [ -x "$T45C/w/f.c" ]; then

    ok "Already modify repairs +x"

else

    fail "Already modify repairs +x" "$(ls -l "$T45C/w/f.c" 2>&1)"

fi

# ----------------------------------------- 46. patch symlink escape

# Patch symlinks with absolute or tree-escaping targets must hard-error
# like tar.

T46="$ROOT/t46"

mkdir -p "$T46/w-1.0"

printf 'hello\n' > "$T46/w-1.0/f.c"

(cd "$T46" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Link escape.\n' > "$T46/w.projeny"

cat >> "$T46/w.projeny" <<'EOF'

diff --git a/w/evil b/w/evil

new file mode 120000

--- /dev/null

+++ b/w/evil

@@ -0,0 +1,1 @@

+/etc/passwd

\ No newline at end of file

EOF

run_in "$T46" expect_fail "patch absolute symlink target hard-errors" "$PROJENY" setup w.projeny

T46B="$ROOT/t46b"

mkdir -p "$T46B/w-1.0"

printf 'hello\n' > "$T46B/w-1.0/f.c"

(cd "$T46B" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)

printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Link escape.\n' > "$T46B/w.projeny"

cat >> "$T46B/w.projeny" <<'EOF'

diff --git a/w/evil b/w/evil

new file mode 120000

--- /dev/null

+++ b/w/evil

@@ -0,0 +1,1 @@

+../escape

\ No newline at end of file

EOF

run_in "$T46B" expect_fail "patch dotdot symlink target hard-errors" "$PROJENY" setup w.projeny

# ----------------------------------------- 47. opaque combined merge/rebase

# Opaque blocks (diff --cc with no parseable per-file path would silently
# drop the failed block: touched stays empty, no conflict, success). A
# failure must never be a no-op: merge and rebase with a combined diff must
# fail visibly, not succeed silently.

T47="$ROOT/t47"
mkdir -p "$T47/w-1.0"
printf 'hello\n' > "$T47/w-1.0/f.c"
(cd "$T47" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Base.\n' > "$T47/w.projeny"
(cd "$T47" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'hello local\n' > "$T47/w/f.c"
cat > "$T47/up.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    Upstream with combined.
diff --cc f.c
index 1111111,2222222..0000000
--- a/f.c
+++ b/f.c
@@@ -1,1 -1,1 +1,1 @@@
-hello
 -hello2
++hello upstream
EOF
cp "$T47/up.projeny" "$T47/w.projeny"
run_in "$T47" expect_fail "merge with combined diff fails (not silent)" "$PROJENY" setup w.projeny
out="$(cd "$T47" && "$PROJENY" setup w.projeny 2>&1 || true)"
case "$out" in
*f.c*|*combined*|*unsupported*|*cannot*|*does*apply*)
    ok "merge opaque failure is visible (names block)"
    ;;
*)
    fail "merge opaque failure is visible (names block)" "out: $out"
    ;;
esac

T48="$ROOT/t48"
mkdir -p "$T48/w-1.0"
printf 'hello\n' > "$T48/w-1.0/f.c"
(cd "$T48" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Base.\n' > "$T48/w.projeny"
cat >> "$T48/w.projeny" <<'EOF'
diff --cc f.c
index 1111111,2222222..0000000
--- a/f.c
+++ b/f.c
@@@ -1,1 -1,1 +1,1 @@@
-hello
 -hello2
++hello upstream
EOF
mkdir -p "$T48/w-2.0"
printf 'hello v2\n' > "$T48/w-2.0/f.c"
(cd "$T48" && tar -czf w-2.0.tar.gz w-2.0 && rm -rf w-2.0)
run_in "$T48" expect_fail "rebase with combined diff fails (not silent)" "$PROJENY" rebase w.projeny w-2.0.tar.gz

# ----------------------------------------- 48. symlink merge: link vs link

# Links must be compared by target string, never by dereferenced bytes.
# Same bytes via different targets is a mismatch and must conflict.

T49="$ROOT/t49"
mkdir -p "$T49/w-1.0"
printf 'same content\n' > "$T49/w-1.0/a.txt"
printf 'same content\n' > "$T49/w-1.0/b.txt"
printf 'same content\n' > "$T49/w-1.0/c.txt"
printf 'base\n' > "$T49/w-1.0/f.c"
ln -s a.txt "$T49/w-1.0/lnk"
(cd "$T49" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Base.\n' > "$T49/w.projeny"
(cd "$T49" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
ln -sf c.txt "$T49/w/lnk"
(cd "$T49" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T49/w.projeny" "$ROOT/t49-upstream.projeny"
cat > "$T49/w.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    Base.
EOF
rm -rf "$T49/w" "$T49/.w.projeny.status"
(cd "$T49" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
ln -sf b.txt "$T49/w/lnk"
cp "$ROOT/t49-upstream.projeny" "$T49/w.projeny"
run_in "$T49" expect_fail "symlink-vs-symlink divergent merge exits nonzero (with conflict)" "$PROJENY" setup w.projeny
expect_file_contains "symlink target mismatch conflicts" "$T49/.w.projeny.status" "Conflict: lnk"
expect_file_contains "symlink conflict leaves markers" "$T49/w/lnk" "<<<<<<<"
if [ -L "$T49/w/lnk" ]; then
    fail "symlink conflict is regular markers file, not link" "$(ls -l "$T49/w/lnk" 2>&1)"
else
    ok "symlink conflict is regular markers file, not link"
fi

# ----------------------------------------- 49. symlink merge: link vs file

# Link vs regular file is always a mismatch, even when dereferenced bytes
# are identical.

T50="$ROOT/t50"
mkdir -p "$T50/w-1.0"
printf 'hello\n' > "$T50/w-1.0/f.c"
printf 'hello\n' > "$T50/w-1.0/other.txt"
(cd "$T50" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Base.\n' > "$T50/w.projeny"
(cd "$T50" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'hello upstream\n' > "$T50/w/f.c"
(cd "$T50" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T50/w.projeny" "$ROOT/t50-upstream.projeny"
cat > "$T50/w.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    Base.
EOF
rm -rf "$T50/w" "$T50/.w.projeny.status"
(cd "$T50" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
rm "$T50/w/f.c"
ln -s other.txt "$T50/w/f.c"
cp "$ROOT/t50-upstream.projeny" "$T50/w.projeny"
run_in "$T50" expect_fail "symlink-vs-file divergent merge exits nonzero (with conflict)" "$PROJENY" setup w.projeny
expect_file_contains "symlink-vs-file records conflict" "$T50/.w.projeny.status" "Conflict: f.c"
expect_file_contains "symlink-vs-file leaves markers" "$T50/w/f.c" "<<<<<<<"

# ----------------------------------------- 50. symlink merge: escape rejected

# Any link placed via merge is validated like patch/tar links: absolute and
# tree-escaping targets are rejected, never created.

T51="$ROOT/t51"
mkdir -p "$T51/w-1.0"
printf 'hello\n' > "$T51/w-1.0/f.c"
printf 'other\n' > "$T51/w-1.0/g.c"
(cd "$T51" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Base.\n' > "$T51/w.projeny"
(cd "$T51" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'other upstream\n' > "$T51/w/g.c"
(cd "$T51" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T51/w.projeny" "$ROOT/t51-upstream.projeny"
cat > "$T51/w.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    Base.
EOF
rm -rf "$T51/w" "$T51/.w.projeny.status"
(cd "$T51" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
rm "$T51/w/f.c"
ln -s /etc/passwd "$T51/w/f.c"
cp "$ROOT/t51-upstream.projeny" "$T51/w.projeny"
out="$(cd "$T51" && "$PROJENY" setup w.projeny 2>&1)"; rc=$?
if [ $rc -ne 0 ]; then
    ok "malicious absolute link via merge rejected (dies)"
else
    if [ -L "$T51/w/f.c" ] && [ "$(readlink "$T51/w/f.c")" = "/etc/passwd" ]; then
        fail "malicious absolute link via merge rejected (not created)" "link was created"
    else
        ok "malicious absolute link via merge rejected (conflict, not created)"
    fi
fi
if [ -L "$T51/w/f.c" ] && [ "$(readlink "$T51/w/f.c" 2>/dev/null)" = "/etc/passwd" ] && [ $rc -eq 0 ]; then
    fail "absolute link not planted on success path" "$(ls -l "$T51/w/f.c" 2>&1)"
else
    ok "absolute link not planted"
fi
case "$out" in
*escape*|*refus*|*cannot*|*Conflict*|*conflict*)
    ok "malicious merge failure is visible"
    ;;
*)
    if [ $rc -ne 0 ]; then
        ok "malicious merge failure is visible"
    else
        fail "malicious merge failure is visible" "out: $out"
    fi
    ;;
esac

T52="$ROOT/t52"
mkdir -p "$T52/w-1.0"
printf 'hello\n' > "$T52/w-1.0/f.c"
printf 'other\n' > "$T52/w-1.0/g.c"
(cd "$T52" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Base.\n' > "$T52/w.projeny"
(cd "$T52" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'other upstream\n' > "$T52/w/g.c"
(cd "$T52" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T52/w.projeny" "$ROOT/t52-upstream.projeny"
cat > "$T52/w.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    Base.
EOF
rm -rf "$T52/w" "$T52/.w.projeny.status"
(cd "$T52" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
rm "$T52/w/f.c"
ln -s ../escape "$T52/w/f.c"
cp "$ROOT/t52-upstream.projeny" "$T52/w.projeny"
out="$(cd "$T52" && "$PROJENY" setup w.projeny 2>&1)"; rc=$?
if [ $rc -ne 0 ]; then
    ok "malicious dotdot link via merge rejected (dies)"
else
    if [ -L "$T52/w/f.c" ] && [ "$(readlink "$T52/w/f.c")" = "../escape" ]; then
        fail "malicious dotdot link via merge rejected (not created)" "link was created"
    else
        ok "malicious dotdot link via merge rejected (conflict, not created)"
    fi
fi
if [ -L "$T52/w/f.c" ] && [ "$(readlink "$T52/w/f.c" 2>/dev/null)" = "../escape" ] && [ $rc -eq 0 ]; then
    fail "dotdot link not planted on success path" "$(ls -l "$T52/w/f.c" 2>&1)"
else
    ok "dotdot link not planted"
fi

# ----------------------------------------- 51. nasty filenames roundtrip
# Leading dashes, glob characters, trailing spaces and backslashes must not
# confuse the differ, the status file, or the applier.
T53="$ROOT/t53"
mkdir -p "$T53/w-1.0"
printf 'base\n' > "$T53/w-1.0/base.c"
(cd "$T53" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Nasty names.\n' > "$T53/w.projeny"
(cd "$T53" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'dash\n' > "$T53/w/-dash.c"
printf 'glob\n' > "$T53/w/star*.c"
printf 'q\n' > "$T53/w/what?.c"
printf 'trail\n' > "$T53/w/trailspace .c"
printf 'back\n' > "$T53/w/back\\slash.c"
run_in "$T53" expect_ok "add leading-dash file" "$PROJENY" add w.projeny "w/-dash.c"
run_in "$T53" expect_ok "add glob-char file" "$PROJENY" add w.projeny "w/star*.c"
run_in "$T53" expect_ok "add question-mark file" "$PROJENY" add w.projeny "w/what?.c"
run_in "$T53" expect_ok "add trailing-space file" "$PROJENY" add w.projeny "w/trailspace .c"
run_in "$T53" expect_ok "add backslash file" "$PROJENY" add w.projeny 'w/back\slash.c'
run_in "$T53" expect_ok "commit nasty names" "$PROJENY" commit w.projeny
run_in "$T53" expect_ok "setup after nasty commit" "$PROJENY" setup w.projeny
for n in "-dash.c" "star*.c" "what?.c" "trailspace .c" 'back\slash.c'; do
    if [ -f "$T53/w/$n" ]; then
        ok "nasty file '$n' survives re-setup"
    else
        fail "nasty file '$n' survives re-setup" "ls: $(ls "$T53/w" 2>&1)"
    fi
done
expect_file_contains "dash content intact" "$T53/w/-dash.c" "dash"
expect_file_contains "backslash content intact" "$T53/w/back\\slash.c" "back"

# ----------------------------------------- 52. unicode names and content
# UTF-8 filenames roundtrip byte-exact; non-ASCII content (including emoji)
# is plain text to projeny, not binary.
T54="$ROOT/t54"
mkdir -p "$T54/w-1.0"
printf 'base\n' > "$T54/w-1.0/base.c"
(cd "$T54" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Unicode.\n' > "$T54/w.projeny"
(cd "$T54" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$T54/w" <<'EOF'
import sys, os
d = sys.argv[1]
open(os.path.join(d, "caf\xc3\xa9-\u20ac.c"), "w").write("plain\n")
open(os.path.join(d, "emoji.c"), "w").write("smile \U0001F600\nsnow \u2603\n")
EOF
(cd "$T54" && ls w | LC_ALL=C sort > "$ROOT/t54-expect-ls.txt")
for _n in "$T54"/w/*.c; do
    _b="$(basename "$_n")"
    if [ "$_b" != "base.c" ]; then
        run_in "$T54" expect_ok "add unicode file" "$PROJENY" add w.projeny "w/$_b"
    fi
done
run_in "$T54" expect_ok "commit unicode names" "$PROJENY" commit w.projeny
rm -rf "$T54/w" "$T54/.w.projeny.status"
run_in "$T54" expect_ok "setup after unicode commit" "$PROJENY" setup w.projeny
(cd "$T54" && ls w | LC_ALL=C sort > "$ROOT/t54-got-ls.txt")
if cmp -s "$ROOT/t54-expect-ls.txt" "$ROOT/t54-got-ls.txt"; then
    ok "unicode filenames byte-identical after re-setup"
else
    fail "unicode filenames byte-identical after re-setup" "got: $(cat "$ROOT/t54-got-ls.txt" 2>&1)"
fi
python3 - "$T54/w" <<'EOF'
import sys, os
d = sys.argv[1]
names = os.listdir(d)
assert any(n.startswith("caf") for n in names), names
data = open(os.path.join(d, "emoji.c"), encoding="utf-8").read()
assert "\U0001F600" in data and "\u2603" in data, repr(data)
EOF
if [ $? -eq 0 ]; then
    ok "unicode and emoji content intact after re-setup"
else
    fail "unicode and emoji content intact after re-setup"
fi

# ----------------------------------------- 53. newline in filename roundtrip
# The status escaper and the C-quoted diff labels must carry a raw newline.
T55="$ROOT/t55"
mkdir -p "$T55/w-1.0"
printf 'base\n' > "$T55/w-1.0/base.c"
(cd "$T55" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Newline name.\n' > "$T55/w.projeny"
(cd "$T55" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
NLFILE="w/new
line.c"
printf 'nlcontent\n' > "$T55/$NLFILE"
run_in "$T55" expect_ok "add newline-name file" "$PROJENY" add w.projeny "$NLFILE"
run_in "$T55" expect_ok "commit newline-name file" "$PROJENY" commit w.projeny
cp "$T55/$NLFILE" "$ROOT/t55-expect-nl.c"
rm -rf "$T55/w" "$T55/.w.projeny.status"
run_in "$T55" expect_ok "setup after newline-name commit" "$PROJENY" setup w.projeny
if cmp -s "$T55/$NLFILE" "$ROOT/t55-expect-nl.c"; then
    ok "newline-name file byte-identical after re-setup"
else
    fail "newline-name file byte-identical after re-setup" "ls: $(ls "$T55/w" 2>&1)"
fi

# ----------------------------------------- 54. deep nesting roundtrip
T56="$ROOT/t56"
mkdir -p "$T56/w-1.0/a/b/c/d/e/f/g/h"
printf 'deep\n' > "$T56/w-1.0/a/b/c/d/e/f/g/h/deep.c"
(cd "$T56" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Deep.\n' > "$T56/w.projeny"
run_in "$T56" expect_ok "deep setup" "$PROJENY" setup w.projeny
printf 'deeper\n' >> "$T56/w/a/b/c/d/e/f/g/h/deep.c"
printf 'brand new\n' > "$T56/w/a/b/c/d/e/f/g/h/fresh.c"
run_in "$T56" expect_ok "add deeply nested file" "$PROJENY" add w.projeny w/a/b/c/d/e/f/g/h/fresh.c
run_in "$T56" expect_ok "commit deeply nested tree" "$PROJENY" commit w.projeny
cp "$T56/w/a/b/c/d/e/f/g/h/deep.c" "$ROOT/t56-expect-deep.c"
rm -rf "$T56/w" "$T56/.w.projeny.status"
run_in "$T56" expect_ok "setup after deep commit" "$PROJENY" setup w.projeny
if cmp -s "$T56/w/a/b/c/d/e/f/g/h/deep.c" "$ROOT/t56-expect-deep.c"; then
    ok "deep file identical after re-setup"
else
    fail "deep file identical after re-setup"
fi
expect_file_contains "deep fresh file survives" "$T56/w/a/b/c/d/e/f/g/h/fresh.c" "brand new"

# ----------------------------------------- 55. many files at once
T57="$ROOT/t57"
mkdir -p "$T57/m-1.0"
for i in $(seq 1 60); do printf 'content %s\n' "$i" > "$T57/m-1.0/f$i.c"; done
(cd "$T57" && tar -czf m-1.0.tar.gz m-1.0 && rm -rf m-1.0)
printf 'Archive: m-1.0.tar.gz\nOrigname: m-1.0\nName: m\n\n    Many.\n' > "$T57/m.projeny"
run_in "$T57" expect_ok "many-file setup" "$PROJENY" setup m.projeny
for i in $(seq 1 60); do printf 'changed %s\n' "$i" > "$T57/m/f$i.c"; done
run_in "$T57" expect_ok "rm f1 for many-file" "$PROJENY" rm m.projeny m/f1.c
run_in "$T57" expect_ok "rm f2 for many-file" "$PROJENY" rm m.projeny m/f2.c
printf 'extra\n' > "$T57/m/extra.c"
run_in "$T57" expect_ok "add among many files" "$PROJENY" add m.projeny m/extra.c
run_in "$T57" expect_ok "commit 60-file change" "$PROJENY" commit m.projeny
rm -rf "$T57/m" "$T57/.m.projeny.status"
run_in "$T57" expect_ok "setup after many-file commit" "$PROJENY" setup m.projeny
expect_file_contains "many-file edit survives" "$T57/m/f60.c" "changed 60"
expect_file_contains "many-file extra survives" "$T57/m/extra.c" "extra"
if [ ! -e "$T57/m/f1.c" ] && [ ! -e "$T57/m/f2.c" ]; then
    ok "many-file deletions stay deleted"
else
    fail "many-file deletions stay deleted" "ls: $(ls "$T57/m" 2>&1)"
fi

# ----------------------------------------- 56. empty dirs are untracked
# Empty directories carry no files, so the differ ignores them: setup and
# commit must succeed and simply not track them.
T58="$ROOT/t58"
mkdir -p "$T58/w-1.0/emptydir/nested"
printf 'base\n' > "$T58/w-1.0/base.c"
(cd "$T58" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Empty dirs.\n' > "$T58/w.projeny"
run_in "$T58" expect_ok "setup with empty base dirs" "$PROJENY" setup w.projeny
mkdir -p "$T58/w/newempty/a/b"
run_in "$T58" expect_ok "commit with empty dirs present" "$PROJENY" commit w.projeny
if grep -q "^diff --git " "$T58/w.projeny"; then
    fail "empty dirs leave patch empty"
else
    ok "empty dirs leave patch empty"
fi
# ----------------------------------------- 57. mode-only changes both ways
# A chmod with no content edit stores an old/new-mode block; flipping the bit
# back empties the patch again. New files default to 0644.
T59="$ROOT/t59"
mkdir -p "$T59/w-1.0"
printf 'code\n' > "$T59/w-1.0/f.c"
printf 'plain\n' > "$T59/w-1.0/p.c"
(cd "$T59" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Modes.\n' > "$T59/w.projeny"
run_in "$T59" expect_ok "mode setup" "$PROJENY" setup w.projeny
chmod 755 "$T59/w/f.c"
run_in "$T59" expect_ok "mode-only commit" "$PROJENY" commit w.projeny
expect_file_contains "mode-only stores old mode" "$T59/w.projeny" "old mode 100644"
expect_file_contains "mode-only stores new mode" "$T59/w.projeny" "new mode 100755"
rm -rf "$T59/w" "$T59/.w.projeny.status"
run_in "$T59" expect_ok "setup after mode-only commit" "$PROJENY" setup w.projeny
if [ -x "$T59/w/f.c" ] && [ ! -x "$T59/w/p.c" ]; then
    ok "mode-only bits exact after re-setup"
else
    fail "mode-only bits exact after re-setup" "$(ls -l "$T59/w" 2>&1)"
fi
chmod 644 "$T59/w/f.c"
run_in "$T59" expect_ok "mode flip-back commit" "$PROJENY" commit w.projeny
if grep -q "^diff --git " "$T59/w.projeny"; then
    fail "mode flip-back empties patch"
else
    ok "mode flip-back empties patch"
fi
printf 'fresh\n' > "$T59/w/fresh.c"
run_in "$T59" expect_ok "add fresh regular file" "$PROJENY" add w.projeny w/fresh.c
run_in "$T59" expect_ok "commit fresh regular file" "$PROJENY" commit w.projeny
rm -rf "$T59/w" "$T59/.w.projeny.status"
run_in "$T59" expect_ok "setup after fresh commit" "$PROJENY" setup w.projeny
if [ ! -x "$T59/w/fresh.c" ]; then
    ok "new regular file defaults to non-exec"
else
    fail "new regular file defaults to non-exec" "$(ls -l "$T59/w/fresh.c" 2>&1)"
fi

# ----------------------------------------- 58. symlinks: dangling, loop, subdir
# Relative links that stay inside the tree roundtrip even when dangling or
# self-referential (including ".." targets that resolve back inside);
# targets that resolve outside the tree are refused like tar escapes.
T60="$ROOT/t60"
mkdir -p "$T60/w-1.0/sub"
printf 'base\n' > "$T60/w-1.0/base.c"
printf 'sub target\n' > "$T60/w-1.0/sub/t.txt"
(cd "$T60" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Links.\n' > "$T60/w.projeny"
run_in "$T60" expect_ok "link setup" "$PROJENY" setup w.projeny
ln -s nowhere "$T60/w/dangling"
ln -s self "$T60/w/self"
ln -s sub/t.txt "$T60/w/sublink"
run_in "$T60" expect_ok "add dangling link" "$PROJENY" add w.projeny w/dangling
run_in "$T60" expect_ok "add self link" "$PROJENY" add w.projeny w/self
run_in "$T60" expect_ok "add sublink" "$PROJENY" add w.projeny w/sublink
run_in "$T60" expect_ok "commit dangling/loop/subdir links" "$PROJENY" commit w.projeny
expect_file_contains "dangling link stored" "$T60/w.projeny" "nowhere"
rm -rf "$T60/w" "$T60/.w.projeny.status"
run_in "$T60" expect_ok "setup after link commit" "$PROJENY" setup w.projeny
if [ -L "$T60/w/dangling" ] && [ "$(readlink "$T60/w/dangling")" = "nowhere" ]; then
    ok "dangling link survives re-setup"
else
    fail "dangling link survives re-setup" "$(ls -l "$T60/w" 2>&1)"
fi
if [ -L "$T60/w/self" ] && [ "$(readlink "$T60/w/self")" = "self" ]; then
    ok "self-loop link survives re-setup"
else
    fail "self-loop link survives re-setup" "$(ls -l "$T60/w" 2>&1)"
fi
if [ -L "$T60/w/sublink" ] && [ "$(cat "$T60/w/sublink")" = "sub target" ]; then
    ok "subdir-relative link resolves after re-setup"
else
    fail "subdir-relative link resolves after re-setup" "$(ls -l "$T60/w" 2>&1)"
fi
ln -s ../escape "$T60/w/dotescape"
run_in "$T60" expect_fail "commit with dotdot link fails" "$PROJENY" commit w.projeny
rm -f "$T60/w/dotescape"
run_in "$T60" expect_ok "commit works after removing escape link" "$PROJENY" commit w.projeny

# ----------------------------------------- 59. symlink rename roundtrip
T61="$ROOT/t61"
mkdir -p "$T61/w-1.0"
printf 'target\n' > "$T61/w-1.0/r.txt"
ln -s r.txt "$T61/w-1.0/lnk"
(cd "$T61" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Link mv.\n' > "$T61/w.projeny"
run_in "$T61" expect_ok "link-mv setup" "$PROJENY" setup w.projeny
run_in "$T61" expect_ok "mv symlink" "$PROJENY" mv w.projeny w/lnk w/lnk2
run_in "$T61" expect_ok "commit symlink rename" "$PROJENY" commit w.projeny
rm -rf "$T61/w" "$T61/.w.projeny.status"
run_in "$T61" expect_ok "setup after symlink rename" "$PROJENY" setup w.projeny
if [ -L "$T61/w/lnk2" ] && [ "$(readlink "$T61/w/lnk2")" = "r.txt" ] && [ ! -e "$T61/w/lnk" ]; then
    ok "renamed symlink survives re-setup"
else
    fail "renamed symlink survives re-setup" "$(ls -l "$T61/w" 2>&1)"
fi

# ----------------------------------------- 60. hardlinks flatten to regular files
T62="$ROOT/t62"
mkdir -p "$T62/w-1.0"
printf 'base\n' > "$T62/w-1.0/base.c"
(cd "$T62" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Hardlinks.\n' > "$T62/w.projeny"
run_in "$T62" expect_ok "hardlink setup" "$PROJENY" setup w.projeny
printf 'shared bytes\n' > "$T62/w/orig.txt"
ln "$T62/w/orig.txt" "$T62/w/twin.txt"
run_in "$T62" expect_ok "add hardlink orig" "$PROJENY" add w.projeny w/orig.txt
run_in "$T62" expect_ok "add hardlink twin" "$PROJENY" add w.projeny w/twin.txt
run_in "$T62" expect_ok "commit hardlinked pair" "$PROJENY" commit w.projeny
rm -rf "$T62/w" "$T62/.w.projeny.status"
run_in "$T62" expect_ok "setup after hardlink commit" "$PROJENY" setup w.projeny
if cmp -s "$T62/w/orig.txt" "$T62/w/twin.txt"; then
    ok "hardlinked contents match after re-setup"
else
    fail "hardlinked contents match after re-setup"
fi

# ----------------------------------------- 61. FIFOs fail cleanly, never hang
T63="$ROOT/t63"
mkdir -p "$T63/w-1.0"
printf 'base\n' > "$T63/w-1.0/base.c"
(cd "$T63" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    FIFO.\n' > "$T63/w.projeny"
run_in "$T63" expect_ok "fifo setup" "$PROJENY" setup w.projeny
mkfifo "$T63/w/myfifo"
out="$(cd "$T63" && "$PROJENY" commit w.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "commit with FIFO fails (no hang)"
else
    fail "commit with FIFO fails (no hang)" "exited 0"
fi
case "$out" in
*unsupported*|*FIFO*|*fifo*|*file\ type*)
    ok "FIFO failure message is clear"
    ;;
*)
    fail "FIFO failure message is clear" "out: $out"
    ;;
esac
rm -f "$T63/w/myfifo"
run_in "$T63" expect_ok "commit works after removing FIFO" "$PROJENY" commit w.projeny

# ----------------------------------------- 62. mixed line endings roundtrip
T64="$ROOT/t64"
mkdir -p "$T64/w-1.0"
printf 'l1\nl2\nl3\n' > "$T64/w-1.0/m.txt"
(cd "$T64" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Mixed.\n' > "$T64/w.projeny"
run_in "$T64" expect_ok "mixed setup" "$PROJENY" setup w.projeny
printf 'l1\r\nl2\r\nl3\nl4\r\nmixed tail\n' > "$T64/w/m.txt"
run_in "$T64" expect_ok "mixed-endings commit" "$PROJENY" commit w.projeny
cp "$T64/w/m.txt" "$ROOT/t64-expect-m.txt"
rm -rf "$T64/w" "$T64/.w.projeny.status"
run_in "$T64" expect_ok "setup after mixed commit" "$PROJENY" setup w.projeny
if cmp -s "$T64/w/m.txt" "$ROOT/t64-expect-m.txt"; then
    ok "mixed-endings file byte-identical after re-setup"
else
    fail "mixed-endings file byte-identical after re-setup" "got: $(xxd "$T64/w/m.txt" 2>&1)"
fi
# ----------------------------------------- 63. tracked binaries commit like text
# A NUL-bearing file inside the base tarball sets up fine; commits that
# leave it alone succeed, and commits that modify or delete it store base64
# binary blocks and roundtrip byte-identical. Explicit `rm` of a binary
# works and commits as a binary delete.
T65="$ROOT/t65"
mkdir -p "$T65/b-1.0"
python3 -c "open('$T65/b-1.0/b.dat','wb').write(b'a\x00b\n')"
printf 'ok\n' > "$T65/b-1.0/f.c"
(cd "$T65" && tar -czf b-1.0.tar.gz b-1.0 && rm -rf b-1.0)
printf 'Archive: b-1.0.tar.gz\nOrigname: b-1.0\nName: b\n\n    Binary base.\n' > "$T65/b.projeny"
run_in "$T65" expect_ok "setup with binary base file" "$PROJENY" setup b.projeny
printf 'ok changed\n' > "$T65/b/f.c"
run_in "$T65" expect_ok "commit leaving binary untouched succeeds" "$PROJENY" commit b.projeny
python3 -c "open('$T65/b/b.dat','wb').write(b'a\x00B\n')"
run_in "$T65" expect_ok "commit touching binary base succeeds" "$PROJENY" commit b.projeny
expect_file_contains "binary modify stored as binary block" "$T65/b.projeny" "GIT binary patch"
python3 -c "open('$ROOT/t65-expect.bin','wb').write(open('$T65/b/b.dat','rb').read())"
rm -rf "$T65/b" "$T65/.b.projeny.status"
run_in "$T65" expect_ok "setup after binary modify" "$PROJENY" setup b.projeny
if cmp -s "$T65/b/b.dat" "$ROOT/t65-expect.bin"; then
    ok "binary modify byte-identical after re-setup"
else
    fail "binary modify byte-identical after re-setup"
fi
run_in "$T65" expect_ok "rm of binary base file succeeds" "$PROJENY" rm b.projeny b/b.dat
if [ ! -f "$T65/b/b.dat" ]; then
    ok "rm deletes the binary"
else
    fail "rm deletes the binary"
fi
run_in "$T65" expect_ok "commit of binary delete succeeds" "$PROJENY" commit b.projeny
expect_file_contains "binary delete names the file" "$T65/b.projeny" "b.dat"
rm -rf "$T65/b" "$T65/.b.projeny.status"
run_in "$T65" expect_ok "setup after binary delete" "$PROJENY" setup b.projeny
if [ ! -f "$T65/b/b.dat" ]; then
    ok "binary delete survives re-setup"
else
    fail "binary delete survives re-setup" "$(ls "$T65/b")"
fi

# ----------------------------------------- 64. delete + recreate same path
T66="$ROOT/t66"
mkdir -p "$T66/w-1.0"
printf 'v1\n' > "$T66/w-1.0/f.c"
(cd "$T66" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Recreate.\n' > "$T66/w.projeny"
run_in "$T66" expect_ok "recreate setup" "$PROJENY" setup w.projeny
run_in "$T66" expect_ok "rm for recreate" "$PROJENY" rm w.projeny w/f.c
printf 'v2 brand new\n' > "$T66/w/f.c"
run_in "$T66" expect_ok "add recreated file" "$PROJENY" add w.projeny w/f.c
run_in "$T66" expect_ok "commit delete+recreate" "$PROJENY" commit w.projeny
rm -rf "$T66/w" "$T66/.w.projeny.status"
run_in "$T66" expect_ok "setup after delete+recreate" "$PROJENY" setup w.projeny
expect_file_contains "recreated content survives" "$T66/w/f.c" "v2 brand new"

# ----------------------------------------- 65. chained mv collapses
T67="$ROOT/t67"
mkdir -p "$T67/w-1.0"
seq 1 20 > "$T67/w-1.0/n.txt"
(cd "$T67" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Chain.\n' > "$T67/w.projeny"
run_in "$T67" expect_ok "chain setup" "$PROJENY" setup w.projeny
run_in "$T67" expect_ok "mv chain step one" "$PROJENY" mv w.projeny w/n.txt w/m1.txt
run_in "$T67" expect_ok "mv chain step two" "$PROJENY" mv w.projeny w/m1.txt w/m2.txt
expect_file_contains "chained rename recorded once" "$T67/.w.projeny.status" "Renamed: n.txt -> m2.txt"
run_in "$T67" expect_ok "commit chained rename" "$PROJENY" commit w.projeny
rm -rf "$T67/w" "$T67/.w.projeny.status"
run_in "$T67" expect_ok "setup after chained rename" "$PROJENY" setup w.projeny
if [ -f "$T67/w/m2.txt" ] && [ ! -e "$T67/w/n.txt" ] && [ ! -e "$T67/w/m1.txt" ]; then
    ok "chained rename paths exact after re-setup"
else
    fail "chained rename paths exact after re-setup" "ls: $(ls "$T67/w" 2>&1)"
fi
run_in "$T67" expect_ok "mv cycle step one" "$PROJENY" mv w.projeny w/m2.txt w/tmp.txt
run_in "$T67" expect_ok "mv cycle step two (back)" "$PROJENY" mv w.projeny w/tmp.txt w/m2.txt
if grep -q "^Renamed:" "$T67/.w.projeny.status"; then
    fail "mv cycle collapses pending rename" "$(cat "$T67/.w.projeny.status")"
else
    ok "mv cycle collapses pending rename"
fi

# ----------------------------------------- 66. add/rm whole directory
T68="$ROOT/t68"
mkdir -p "$T68/w-1.0"
printf 'base\n' > "$T68/w-1.0/base.c"
(cd "$T68" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Dirs.\n' > "$T68/w.projeny"
run_in "$T68" expect_ok "dir setup" "$PROJENY" setup w.projeny
mkdir -p "$T68/w/newdir"
printf 'one\n' > "$T68/w/newdir/one.c"
printf 'two\n' > "$T68/w/newdir/two.c"
run_in "$T68" expect_ok "add file under new dir" "$PROJENY" add w.projeny w/newdir/one.c
run_in "$T68" expect_ok "add second file under new dir" "$PROJENY" add w.projeny w/newdir/two.c
run_in "$T68" expect_ok "commit new dir content" "$PROJENY" commit w.projeny
expect_file_contains "newdir file committed" "$T68/w.projeny" "two"
rm -rf "$T68/w" "$T68/.w.projeny.status"
run_in "$T68" expect_ok "setup after dir add" "$PROJENY" setup w.projeny
expect_file_contains "dir add file one survives" "$T68/w/newdir/one.c" "one"
expect_file_contains "dir add file two survives" "$T68/w/newdir/two.c" "two"
run_in "$T68" expect_ok "rm whole directory" "$PROJENY" rm w.projeny w/newdir
run_in "$T68" expect_ok "commit dir removal" "$PROJENY" commit w.projeny
rm -rf "$T68/w" "$T68/.w.projeny.status"
run_in "$T68" expect_ok "setup after dir removal" "$PROJENY" setup w.projeny
if [ ! -e "$T68/w/newdir" ]; then
    ok "removed directory stays gone"
else
    fail "removed directory stays gone" "ls: $(ls -R "$T68/w" 2>&1)"
fi

# ----------------------------------------- 67. CLI arity and missing inputs
T69="$ROOT/t69"
mkdir -p "$T69/w-1.0"
printf 'a\n' > "$T69/w-1.0/f.c"
(cd "$T69" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    CLI.\n' > "$T69/w.projeny"
run_in "$T69" expect_fail "bare projeny fails" "$PROJENY"
run_in "$T69" expect_fail "setup with extra args fails" "$PROJENY" setup a b c
run_in "$T69" expect_fail "commit with extra args fails" "$PROJENY" commit a b
run_in "$T69" expect_fail "setup of missing projeny fails" "$PROJENY" setup "$T69/never.projeny"
run_in "$T69" expect_fail "rebase with missing tarball fails" "$PROJENY" rebase w.projeny "$T69/never.tar.gz"
(cd "$T69" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
run_in "$T69" expect_fail "add with too many args fails" "$PROJENY" add w.projeny w/f.c extra
run_in "$T69" expect_fail "resolve of unknown path fails" "$PROJENY" resolve w.projeny w/f.c
run_in "$T69" expect_ok "status shows pending add" "$PROJENY" add w.projeny w/f.c
run_in "$T69" expect_ok "status prints pending state" "$PROJENY" status w.projeny
out="$(cd "$T69" && "$PROJENY" status w.projeny 2>&1)"
case "$out" in
*Added:\ f.c*)
    ok "status lists pending add"
    ;;
*)
    fail "status lists pending add" "out: $out"
    ;;
esac
# ----------------------------------------- 68. corrupt inputs fail loudly
T70="$ROOT/t70"
mkdir -p "$T70/w-1.0"
printf 'v1\n' > "$T70/w-1.0/f.c"
(cd "$T70" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    T.\n' > "$T70/w.projeny"
printf 'No headers here\njust text\n' > "$T70/bad1.projeny"
run_in "$T70" expect_fail "headerless projeny fails" "$PROJENY" setup bad1.projeny
printf 'Archive: sub/dir.tar.gz\nOrigname: w-1.0\nName: w\n\n    T.\n' > "$T70/bad2.projeny"
run_in "$T70" expect_fail "slashed Archive fails" "$PROJENY" setup bad2.projeny
printf 'Archive: w-1.0.tar.gz\nOrigname: a/b\nName: w\n\n    T.\n' > "$T70/bad3.projeny"
run_in "$T70" expect_fail "slashed Origname fails" "$PROJENY" setup bad3.projeny
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: ..\n\n    T.\n' > "$T70/bad4.projeny"
run_in "$T70" expect_fail "dotdot Name fails" "$PROJENY" setup bad4.projeny
run_in "$T70" expect_ok "good setup for status corruption" "$PROJENY" setup w.projeny
printf 'Garbage line\n' > "$T70/.w.projeny.status"
run_in "$T70" expect_fail "garbage status line fails" "$PROJENY" setup w.projeny
printf 'Status: setup\nRenamed: broken-no-arrow\n' > "$T70/.w.projeny.status"
run_in "$T70" expect_fail "malformed Renamed line fails" "$PROJENY" setup w.projeny
printf 'Conflict: f.c\n' > "$T70/.w.projeny.status"
run_in "$T70" expect_fail "status without Status line fails" "$PROJENY" setup w.projeny

# ----------------------------------------- 69. tarball edge cases
T71="$ROOT/t71"
mkdir -p "$T71/real-9.9"
printf 'x\n' > "$T71/real-9.9/f.c"
(cd "$T71" && tar -czf real-9.9.tar.gz real-9.9 && rm -rf real-9.9)
printf 'Archive: real-9.9.tar.gz\nOrigname: wrong-top\nName: w\n\n    T.\n' > "$T71/o.projeny"
run_in "$T71" expect_fail "Origname mismatch fails" "$PROJENY" setup o.projeny
printf 'Archive: missing.tar.gz\nOrigname: w-1.0\nName: w\n\n    T.\n' > "$T71/m.projeny"
run_in "$T71" expect_fail "missing tarball fails" "$PROJENY" setup m.projeny
printf 'Archive: real-9.9.tar.gz\nOrigname: real-9.9\nName: w\n\n    T.\n' > "$T71/w.projeny"
run_in "$T71" expect_ok "Name differing from Origname works" "$PROJENY" setup w.projeny
expect_file_contains "renamed top unpacks content" "$T71/w/f.c" "x"
mkdir -p "$T71/dot-1.0"
printf 'y\n' > "$T71/dot-1.0/g.c"
(cd "$T71" && tar -czf dot.tar.gz -C . ./dot-1.0)
printf 'Archive: dot.tar.gz\nOrigname: dot-1.0\nName: d\n\n    T.\n' > "$T71/d.projeny"
run_in "$T71" expect_ok "dot-prefixed members unpack" "$PROJENY" setup d.projeny
expect_file_contains "dot-prefixed content intact" "$T71/d/g.c" "y"
mkdir -p "$T71/empty-1.0"
(cd "$T71" && tar -czf empty.tar.gz empty-1.0 && rm -rf empty-1.0)
printf 'Archive: empty.tar.gz\nOrigname: empty-1.0\nName: e\n\n    T.\n' > "$T71/e.projeny"
run_in "$T71" expect_ok "empty top dir sets up" "$PROJENY" setup e.projeny
run_in "$T71" expect_ok "empty top dir commits" "$PROJENY" commit e.projeny
if command -v python3 >/dev/null 2>&1; then
    (cd "$T71" && python3 - <<'PYEOF'
import tarfile, io
with tarfile.open("abs.tar.gz", "w") as t:
    d = tarfile.TarInfo("top"); d.type = tarfile.DIRTYPE; t.addfile(d)
    f = tarfile.TarInfo("/absmember"); f.size = 2; t.addfile(f, io.BytesIO(b"x\n"))
with tarfile.open("dotdot.tar.gz", "w") as t:
    d = tarfile.TarInfo("top"); d.type = tarfile.DIRTYPE; t.addfile(d)
    f = tarfile.TarInfo("top/../../evil"); f.size = 2; t.addfile(f, io.BytesIO(b"x\n"))
PYEOF
    )
    printf 'Archive: abs.tar.gz\nOrigname: top\nName: a\n\n    T.\n' > "$T71/a.projeny"
    run_in "$T71" expect_fail "absolute tar member refused" "$PROJENY" setup a.projeny
    out="$(cd "$T71" && "$PROJENY" setup a.projeny 2>&1 || true)"
    case "$out" in
    *absolute*)
        ok "absolute-member error names the problem"
        ;;
    *)
        fail "absolute-member error names the problem" "out: $out"
        ;;
    esac
    printf 'Archive: dotdot.tar.gz\nOrigname: top\nName: b\n\n    T.\n' > "$T71/b.projeny"
    run_in "$T71" expect_fail "dotdot tar member refused" "$PROJENY" setup b.projeny
    out="$(cd "$T71" && "$PROJENY" setup b.projeny 2>&1 || true)"
    case "$out" in
    *top/../../evil*)
        ok "dotdot-member error names the member"
        ;;
    *)
        fail "dotdot-member error names the member" "out: $out"
        ;;
    esac
else
    ok "absolute tar member refused (skipped: no python3)"
    ok "absolute-member error names the problem (skipped: no python3)"
    ok "dotdot tar member refused (skipped: no python3)"
    ok "dotdot-member error names the member (skipped: no python3)"
fi

# ----------------------------------------- 70. hostile patch paths fail cleanly
# A stored patch that creates a file under a path blocked by a regular file
# (or into a directory) must fail with a precise per-file error, not die.
T72="$ROOT/t72"
mkdir -p "$T72/w-1.0"
printf 'file-a\n' > "$T72/w-1.0/a"
(cd "$T72" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    T.\n' > "$T72/w.projeny"
cat >> "$T72/w.projeny" <<'EOF'
diff --git a/w/a/b b/w/a/b
new file mode 100644
--- /dev/null
+++ b/w/a/b
@@ -0,0 +1,1 @@
+hello
EOF
run_in "$T72" expect_fail "file-under-file patch refused" "$PROJENY" setup w.projeny
out="$(cd "$T72" && "$PROJENY" setup w.projeny 2>&1 || true)"
case "$out" in
*a/b*)
    ok "file-under-file error names the path"
    ;;
*)
    fail "file-under-file error names the path" "out: $out"
    ;;
esac

# ----------------------------------------- 71. pending symlink survives rebase
T73="$ROOT/t73"
mkdir -p "$T73/w-1.0"
printf 'v1\n' > "$T73/w-1.0/f.c"
printf 'real\n' > "$T73/w-1.0/r.txt"
(cd "$T73" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    T.\n' > "$T73/w.projeny"
run_in "$T73" expect_ok "pending-link setup" "$PROJENY" setup w.projeny
ln -s r.txt "$T73/w/pendlink"
run_in "$T73" expect_ok "stage pending symlink" "$PROJENY" add w.projeny w/pendlink
mkdir -p "$T73/w-2.0"
printf 'v2\n' > "$T73/w-2.0/f.c"
printf 'real\n' > "$T73/w-2.0/r.txt"
(cd "$T73" && tar -czf w-2.0.tar.gz w-2.0 && rm -rf w-2.0)
run_in "$T73" expect_ok "rebase with pending symlink" "$PROJENY" rebase w.projeny w-2.0.tar.gz
expect_file_contains "pending symlink kept in status" "$T73/.w.projeny.status" "Added: pendlink"
if [ -L "$T73/w/pendlink" ] && [ "$(readlink "$T73/w/pendlink")" = "r.txt" ]; then
    ok "pending symlink still a link after rebase"
else
    fail "pending symlink still a link after rebase" "$(ls -l "$T73/w" 2>&1)"
fi
run_in "$T73" expect_ok "commit pending symlink after rebase" "$PROJENY" commit w.projeny
rm -rf "$T73/w" "$T73/.w.projeny.status"
run_in "$T73" expect_ok "setup after symlink commit" "$PROJENY" setup w.projeny
if [ -L "$T73/w/pendlink" ]; then
    ok "committed symlink survives re-setup"
else
    fail "committed symlink survives re-setup" "$(ls -l "$T73/w" 2>&1)"
fi

# ----------------------------------------- 72. file/dir swaps merge safely
# Replacing a tracked file with a directory (or vice versa) must merge the
# real content or conflict — never silently drop user data, die, or hang.
T74="$ROOT/t74"
mkdir -p "$T74/w-1.0"
printf 'v1\n' > "$T74/w-1.0/f.c"
printf 'keep\n' > "$T74/w-1.0/g.c"
(cd "$T74" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    T.\n' > "$T74/w.projeny"
run_in "$T74" expect_ok "swap setup" "$PROJENY" setup w.projeny
cp "$T74/w.projeny" "$ROOT/t74-base.projeny"
rm "$T74/w/f.c"
mkdir "$T74/w/f.c"
printf 'user inner\n' > "$T74/w/f.c/inner.txt"
cp "$ROOT/t74-base.projeny" "$T74/w.projeny"
run_in "$T74" expect_ok "file-to-dir merge keeps content" "$PROJENY" setup w.projeny
expect_file_contains "swapped dir content preserved" "$T74/w/f.c/inner.txt" "user inner"
T74B="$ROOT/t74b"
mkdir -p "$T74B/w-1.0"
printf 'v1\n' > "$T74B/w-1.0/f.c"
(cd "$T74B" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    T.\n' > "$T74B/w.projeny"
(cd "$T74B" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'v1 upstream\n' > "$T74B/w/f.c"
(cd "$T74B" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T74B/w.projeny" "$ROOT/t74b-up.projeny"
cat > "$T74B/w.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    T.
EOF
rm -rf "$T74B/w" "$T74B/.w.projeny.status"
(cd "$T74B" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
rm "$T74B/w/f.c"
mkdir "$T74B/w/f.c"
printf 'local inner\n' > "$T74B/w/f.c/inner.txt"
cp "$ROOT/t74b-up.projeny" "$T74B/w.projeny"
run_in "$T74B" expect_fail "divergent file-to-dir merge exits nonzero" "$PROJENY" setup w.projeny
expect_file_contains "divergent swap records conflict" "$T74B/.w.projeny.status" "Conflict: f.c"
expect_file_contains "divergent swap keeps local inner file" "$T74B/w/f.c/inner.txt" "local inner"
if [ -d "$T74B/w/f.c" ]; then
    ok "divergent swap keeps local directory"
else
    fail "divergent swap keeps local directory" "ls: $(ls -l "$T74B/w" 2>&1)"
fi

# ----------------------------------------- 73. huge line without trailing newline
T75="$ROOT/t75"
mkdir -p "$T75/w-1.0"
printf 'seed\n' > "$T75/w-1.0/s.txt"
(cd "$T75" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Huge.\n' > "$T75/w.projeny"
run_in "$T75" expect_ok "huge setup" "$PROJENY" setup w.projeny
python3 -c "open('$T75/w/huge.txt','w').write('Z'*100000)"
run_in "$T75" expect_ok "add huge no-newline file" "$PROJENY" add w.projeny w/huge.txt
run_in "$T75" expect_ok "commit huge no-newline file" "$PROJENY" commit w.projeny
expect_file_contains "huge commit stores no-newline marker" "$T75/w.projeny" "No newline at end of file"
cp "$T75/w/huge.txt" "$ROOT/t75-expect-huge.txt"
rm -rf "$T75/w" "$T75/.w.projeny.status"
run_in "$T75" expect_ok "setup after huge commit" "$PROJENY" setup w.projeny
if cmp -s "$T75/w/huge.txt" "$ROOT/t75-expect-huge.txt"; then
    ok "huge no-newline file byte-identical after re-setup"
else
    fail "huge no-newline file byte-identical after re-setup"
fi

# ----------------------------------------- 74. wid-prefixed resolve and rebase noop
T76="$ROOT/t76"
mkdir -p "$T76/w-1.0"
printf 'one\ntwo\nthree\n' > "$T76/w-1.0/f.c"
(cd "$T76" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    T.\n' > "$T76/w.projeny"
(cd "$T76" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'ONE\ntwo\nthree\n' > "$T76/w/f.c"
(cd "$T76" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T76/w.projeny" "$ROOT/t76-local.projeny"
rm -rf "$T76/w" "$T76/.w.projeny.status"
cp "$ROOT/t76-local.projeny" "$T76/w.projeny"
(cd "$T76" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'local edit\n' > "$T76/w/f.c"
U76="$ROOT/t76up"
mkdir -p "$U76"
cp "$T76/w-1.0.tar.gz" "$U76/"
cp "$ROOT/t76-local.projeny" "$U76/w.projeny"
(cd "$U76" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'upstream edit\n' > "$U76/w/f.c"
(cd "$U76" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$U76/w.projeny" "$T76/w.projeny"
(cd "$T76" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'resolved\n' > "$T76/w/f.c"
run_in "$T76" expect_ok "resolve accepts wid-prefixed path" "$PROJENY" resolve w.projeny w/f.c
if grep -q "^Conflict:" "$T76/.w.projeny.status"; then
    fail "wid-prefixed resolve clears conflict"
else
    ok "wid-prefixed resolve clears conflict"
fi
run_in "$T76" expect_ok "commit after wid resolve" "$PROJENY" commit w.projeny
run_in "$T76" expect_ok "rebase onto identical tarball" "$PROJENY" rebase w.projeny w-1.0.tar.gz
expect_file_contains "rebase noop keeps content" "$T76/w/f.c" "resolved"

# ----------------------------------------- 75. long names and odd-but-legal bytes
T77="$ROOT/t77"
mkdir -p "$T77/w-1.0"
printf 'base\n' > "$T77/w-1.0/base.c"
(cd "$T77" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Long.\n' > "$T77/w.projeny"
run_in "$T77" expect_ok "long-name setup" "$PROJENY" setup w.projeny
LONG="$(python3 -c "print('L'*200 + '.c')")"
printf 'long content\n' > "$T77/w/$LONG"
run_in "$T77" expect_ok "add 200-char name" "$PROJENY" add w.projeny "w/$LONG"
printf 'punct\n' > "$T77/w/semi;colon.c"
printf 'p2\n' > "$T77/w/eq=uals.c"
run_in "$T77" expect_ok "add semicolon name" "$PROJENY" add w.projeny "w/semi;colon.c"
run_in "$T77" expect_ok "add equals name" "$PROJENY" add w.projeny "w/eq=uals.c"
run_in "$T77" expect_ok "commit odd names" "$PROJENY" commit w.projeny
rm -rf "$T77/w" "$T77/.w.projeny.status"
run_in "$T77" expect_ok "setup after odd-name commit" "$PROJENY" setup w.projeny
if [ -f "$T77/w/$LONG" ]; then
    ok "200-char filename survives re-setup"
else
    fail "200-char filename survives re-setup" "ls: $(ls "$T77/w" 2>&1)"
fi
expect_file_contains "semicolon name survives" "$T77/w/semi;colon.c" "punct"
expect_file_contains "equals name survives" "$T77/w/eq=uals.c" "p2"

T78="$ROOT/t78"
mkdir -p "$T78/A" "$T78/B"
printf 'one\ntwo\nthree\n' > "$T78/A/f.c"
printf 'one\nTWO\nthree\n' > "$T78/B/f.c"
printf 'new file\n' > "$T78/B/g.c"
out="$(cd "$T78" && "$PROJENY" diff A B 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "diff exits 0"
else
    fail "diff exits 0" "exit=$rc out: $out"
fi
if echo "$out" | grep -q "diff --git a/B/f.c b/B/f.c"; then
    ok "diff labels use second dir basename"
else
    fail "diff labels use second dir basename" "out: $out"
fi
if echo "$out" | grep -q "new file mode"; then
    ok "diff shows added file"
else
    fail "diff shows added file" "out: $out"
fi
out2="$(cd "$T78" && "$PROJENY" diff A A 2>&1)"
if [ -z "$out2" ]; then
    ok "diff of identical dirs is empty"
else
    fail "diff of identical dirs is empty" "out: $out2"
fi
# roundtrip: diff A B, patch a copy of A, get B back byte-for-byte.
(cd "$T78" && "$PROJENY" diff A B > roundtrip.diff && rm -rf C && cp -r A C && "$PROJENY" patch C roundtrip.diff >/dev/null 2>&1)
if [ $? -eq 0 ] && [ ! -e "$T78/C/B" ]; then
    ok "diff+patch roundtrip applies"
else
    fail "diff+patch roundtrip applies" "ls: $(ls -R "$T78/C" 2>&1)"
fi
if diff -r "$T78/C" "$T78/B" >/dev/null 2>&1; then
    ok "diff+patch roundtrip is byte-identical"
else
    fail "diff+patch roundtrip is byte-identical" "$(diff -r "$T78/C" "$T78/B" 2>&1 | head -5)"
fi
# rename detection through the diff command.
(cd "$T78" && rm -rf R1 R2 && mkdir R1 R2 && printf 'same bytes\n' > R1/old.txt && printf 'same bytes\n' > R2/new.txt && "$PROJENY" diff R1 R2 > rename.diff 2>&1)
if grep -q "rename from" "$T78/rename.diff"; then
    ok "diff detects renames"
else
    fail "diff detects renames" "$(cat "$T78/rename.diff")"
fi
run_in "$T78" expect_fail "diff of missing dir fails" "$PROJENY" diff A "$T78/nope"
printf 'x\n' > "$T78/plainfile"
run_in "$T78" expect_fail "diff of non-dir fails" "$PROJENY" diff "$T78/plainfile" B
run_in "$T78" expect_fail "diff with one arg fails" "$PROJENY" diff A
run_in "$T78" expect_fail "patch with one arg fails" "$PROJENY" patch A

# ----------------------------------------- 77. minimum-diff devious cases
T79="$ROOT/t79"
mkdir -p "$T79/A" "$T79/B"
python3 -c "open('$T79/A/big.c','w').write('same line\n'*100)"
python3 -c "open('$T79/B/big.c','w').write('same line\n'*49 + 'CHANGED\n' + 'same line\n'*50)"
(cd "$T79" && "$PROJENY" diff A B > big.diff 2>&1)
if [ "$(grep -c '^@@' "$T79/big.diff")" -eq 1 ]; then
    ok "repeated-line change is a single hunk"
else
    fail "repeated-line change is a single hunk" "hunks: $(grep -c '^@@' "$T79/big.diff")"
fi
if [ "$(wc -l < "$T79/big.diff")" -lt 20 ]; then
    ok "repeated-line diff stays small"
else
    fail "repeated-line diff stays small" "lines: $(wc -l < "$T79/big.diff")"
fi
# alternating-pattern trap: one flipped line among ABA BAB alternation.
python3 -c "open('$T79/A/alt.c','w').write('aaa\nbbb\n'*30)"
python3 -c "open('$T79/B/alt.c','w').write('aaa\nbbb\n'*14 + 'aaa\nFLIP\n' + 'aaa\nbbb\n'*15)"
(cd "$T79" && "$PROJENY" diff A B > alt.diff 2>&1)
if [ "$(grep -c '^@@' "$T79/alt.diff")" -le 2 ]; then
    ok "alternating-pattern change stays minimal"
else
    fail "alternating-pattern change stays minimal" "hunks: $(grep -c '^@@' "$T79/alt.diff")"
fi
(cd "$T79" && rm -rf C && cp -r A C && "$PROJENY" patch C alt.diff >/dev/null 2>&1 && "$PROJENY" patch C big.diff >/dev/null 2>&1)
if diff -r "$T79/C" "$T79/B" >/dev/null 2>&1; then
    ok "devious diffs roundtrip byte-identical"
else
    fail "devious diffs roundtrip byte-identical" "$(diff -r "$T79/C" "$T79/B" 2>&1 | head -5)"
fi
# trailing-whitespace-only change roundtrips exactly.
mkdir -p "$T79/W1" "$T79/W2"
printf 'pad   \nend\n' > "$T79/W1/ws.c"
printf 'pad\nend\n' > "$T79/W2/ws.c"
(cd "$T79" && "$PROJENY" diff W1 W2 > ws.diff 2>&1 && rm -rf W3 && cp -r W1 W3 && "$PROJENY" patch W3 ws.diff >/dev/null 2>&1)
if cmp -s "$T79/W3/ws.c" "$T79/W2/ws.c"; then
    ok "whitespace-only change roundtrips byte-identical"
else
    fail "whitespace-only change roundtrips byte-identical"
fi

# ----------------------------------------- 78. patch devious + conflicts
T80="$ROOT/t80"
mkdir -p "$T80/A" "$T80/B"
printf 'one\ntwo\nthree\n' > "$T80/A/f.c"
printf 'one\nTWO\nthree\n' > "$T80/B/f.c"
(cd "$T80" && "$PROJENY" diff A B > f.diff 2>&1)
mkdir -p "$T80/T"
printf 'one\ntwo\nthree\n' > "$T80/T/f.c"
out="$(cd "$T80" && "$PROJENY" patch T f.diff 2>&1)"
if [ $? -eq 0 ] && echo "$out" | grep -q "patched"; then
    ok "clean patch exits 0 with message"
else
    fail "clean patch exits 0 with message" "out: $out"
fi
expect_file_contains "clean patch updates content" "$T80/T/f.c" "TWO"
out="$(cd "$T80" && "$PROJENY" patch T f.diff 2>&1)"
if [ $? -eq 0 ]; then
    ok "re-applying a patch is idempotent"
else
    fail "re-applying a patch is idempotent" "out: $out"
fi
# shifted hunk offsets (fuzz) still apply.
python3 - "$T80/f.diff" <<'EOF'
import re, sys
s = open(sys.argv[1]).read()
s2 = re.sub(r"@@ -(\d+),", lambda m: "@@ -%d," % (int(m.group(1)) + 5), s, count=1)
assert s2 != s
open(sys.argv[1] + ".shifted", "w").write(s2)
EOF
mkdir -p "$T80/T2"
printf 'one\ntwo\nthree\n' > "$T80/T2/f.c"
run_in "$T80" expect_ok "patch with shifted offsets applies" "$PROJENY" patch T2 f.diff.shifted
expect_file_contains "shifted patch updates content" "$T80/T2/f.c" "TWO"
# conflicting patch: same line changed differently.
mkdir -p "$T80/T3"
printf 'one\nCONFLICT\nthree\n' > "$T80/T3/f.c"
out="$(cd "$T80" && "$PROJENY" patch T3 f.diff 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "conflicting patch still exits 0"
else
    fail "conflicting patch still exits 0" "exit=$rc out: $out"
fi
if echo "$out" | grep -q "f.c"; then
    ok "conflicting patch lists the file on console"
else
    fail "conflicting patch lists the file on console" "out: $out"
fi
expect_file_contains "conflicting patch inserts <<<<<<<" "$T80/T3/f.c" "<<<<<<<"
expect_file_contains "conflicting patch inserts =======" "$T80/T3/f.c" "======="
expect_file_contains "conflicting patch inserts >>>>>>>" "$T80/T3/f.c" ">>>>>>>"
expect_file_contains "conflicting patch keeps current side" "$T80/T3/f.c" "CONFLICT"
expect_file_contains "conflicting patch keeps patched side" "$T80/T3/f.c" "TWO"
# mixed: one clean file + one conflicted file in a single patch.
mkdir -p "$T80/M1" "$T80/M2"
printf 'keep\nme\n' > "$T80/M1/ok.c"
printf 'same\nline\n' > "$T80/M1/bad.c"
printf 'keep\nME\n' > "$T80/M2/ok.c"
printf 'same\nLINE\n' > "$T80/M2/bad.c"
(cd "$T80" && "$PROJENY" diff M1 M2 > m.diff 2>&1)
mkdir -p "$T80/M3"
printf 'keep\nme\n' > "$T80/M3/ok.c"
printf 'same\nDIFFERENT\n' > "$T80/M3/bad.c"
out="$(cd "$T80" && "$PROJENY" patch M3 m.diff 2>&1)"
expect_file_contains "mixed patch applies the clean file" "$T80/M3/ok.c" "ME"
expect_file_contains "mixed patch marks the conflicted file" "$T80/M3/bad.c" "<<<<<<<"
if echo "$out" | grep -q "bad.c" && ! echo "$out" | grep -q "ok.c"; then
    ok "mixed patch lists only the conflicted file"
else
    fail "mixed patch lists only the conflicted file" "out: $out"
fi
# new-file conflict: patch adds a file that exists with other bytes.
mkdir -p "$T80/N1" "$T80/N2"
printf 'base\n' > "$T80/N1/base.c"
printf 'base\n' > "$T80/N2/base.c"
printf 'wanted\n' > "$T80/N2/added.c"
(cd "$T80" && "$PROJENY" diff N1 N2 > n.diff 2>&1)
mkdir -p "$T80/N3"
printf 'base\n' > "$T80/N3/base.c"
printf 'rival\n' > "$T80/N3/added.c"
out="$(cd "$T80" && "$PROJENY" patch N3 n.diff 2>&1)"
expect_file_contains "new-file conflict leaves markers" "$T80/N3/added.c" "<<<<<<<"
expect_file_contains "new-file conflict keeps current bytes" "$T80/N3/added.c" "rival"
expect_file_contains "new-file conflict shows wanted bytes" "$T80/N3/added.c" "wanted"
if echo "$out" | grep -q "added.c"; then
    ok "new-file conflict is listed"
else
    fail "new-file conflict is listed" "out: $out"
fi
# delete-vs-modify: patch deletes a file the target changed.
mkdir -p "$T80/D1" "$T80/D2"
printf 'gone one\ngone two\n' > "$T80/D1/del.c"
mkdir -p "$T80/D3"
printf 'gone one\nCHANGED\n' > "$T80/D3/del.c"
(cd "$T80" && "$PROJENY" diff D1 D2 > d.diff 2>&1)
out="$(cd "$T80" && "$PROJENY" patch D3 d.diff 2>&1)"
if [ -f "$T80/D3/del.c" ]; then
    ok "delete-vs-modify keeps the file"
else
    fail "delete-vs-modify keeps the file" "ls: $(ls "$T80/D3" 2>&1)"
fi
if echo "$out" | grep -q "del.c"; then
    ok "delete-vs-modify is listed"
else
    fail "delete-vs-modify is listed" "out: $out"
fi
# empty and garbage patch files.
printf '' > "$T80/empty.diff"
run_in "$T80" expect_ok "empty patch is a noop" "$PROJENY" patch T3 empty.diff
printf 'just some text\nno diffs here\n' > "$T80/garbage.diff"
run_in "$T80" expect_fail "garbage patch file fails" "$PROJENY" patch T3 garbage.diff
run_in "$T80" expect_fail "missing patch file fails" "$PROJENY" patch T3 "$T80/nope.diff"
run_in "$T80" expect_fail "patch of missing dir fails" "$PROJENY" patch "$T80/nope" f.diff
# ----------------------------------------- 79. conflicted .projeny (markers)
# Helper: splice ours/theirs .projeny files into one git-style conflict.
make_conflicted() {
    # $1=ours $2=theirs $3=out [$4=style: merge (default) or diff3]
    python3 - "$1" "$2" "$3" "${4:-merge}" <<'EOF'
import sys
ours, theirs, out = sys.argv[1], sys.argv[2], sys.argv[3]
style = sys.argv[4] if len(sys.argv) > 4 else "merge"
a = open(ours).read().split("\n")
b = open(theirs).read().split("\n")
n = min(len(a), len(b))
i = 0
while i < n and a[i] == b[i]:
    i += 1
j0, j1 = len(a), len(b)
while j0 > i and j1 > i and a[j0 - 1] == b[j1 - 1]:
    j0 -= 1
    j1 -= 1
blk = ["<<<<<<< HEAD"] + a[i:j0]
if style == "diff3":
    blk += ["||||||| base"] + ["# ancestral placeholder"]
blk += ["======="] + b[i:j1] + [">>>>>>> branch"]
open(out, "w").write("\n".join(a[:i] + blk + a[j0:]))
EOF
}

T81="$ROOT/t81"
mkdir -p "$T81/w-1.0"
printf 'alpha 1\nbeta 1\ngamma 1\ndelta 1\n' > "$T81/w-1.0/a.c"
(cd "$T81" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    C.\n' > "$T81/w.projeny"
(cd "$T81" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$T81/w/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("beta 1", "beta LOCAL")
open(p, "w").write(s)
EOF
(cd "$T81" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T81/w.projeny" "$ROOT/t81-local.projeny"
# upstream twin: same base, beta changed the other way.
U81="$ROOT/t81up"
mkdir -p "$U81"
cp "$T81/w-1.0.tar.gz" "$U81/"
cp "$ROOT/t81-local.projeny" "$U81/w.projeny"
(cd "$U81" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$U81/w/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("beta LOCAL", "beta UPSTREAM")
open(p, "w").write(s)
EOF
(cd "$U81" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$U81/w.projeny" "$ROOT/t81-up.projeny"
# simple case: no uncommitted changes; conflict the .projeny file.
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T81/w.projeny"
out="$(cd "$T81" && "$PROJENY" setup w.projeny 2>&1)"
if [ $? -ne 0 ]; then
    ok "conflicted setup (simple) exits nonzero"
else
    fail "conflicted setup (simple) exits nonzero" "out: $out"
fi
if cmp -s "$T81/w.projeny" "$ROOT/t81-up.projeny"; then
    ok ".projeny force-takes upstream"
else
    fail ".projeny force-takes upstream" "$(cat "$T81/w.projeny")"
fi
if cmp -s <(sed -n '/^--- projeny content ---$/,$p' "$T81/.w.projeny.status" | tail -n +2) "$ROOT/t81-up.projeny" 2>/dev/null || cmp -s "$T81/w.projeny" <(sed -n '/^--- projeny content ---$/,$p' "$T81/.w.projeny.status" | tail -n +2); then
    ok "status embeds the upstream copy"
else
    fail "status embeds the upstream copy" "$(head -8 "$T81/.w.projeny.status")"
fi
expect_file_contains "conflicted checkout has workdir markers" "$T81/w/a.c" "<<<<<<<"
expect_file_contains "conflicted checkout keeps local side" "$T81/w/a.c" "beta LOCAL"
expect_file_contains "conflicted checkout keeps upstream side" "$T81/w/a.c" "beta UPSTREAM"
expect_file_contains "conflicted checkout records conflict" "$T81/.w.projeny.status" "Conflict: a.c"
if echo "$out" | grep -q "a.c"; then
    ok "conflicted setup lists conflicted files"
else
    fail "conflicted setup lists conflicted files" "out: $out"
fi
# every other command refuses the conflicted file (re-conflict first).
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T81/w.projeny"
run_in "$T81" expect_fail "commit refuses conflicted .projeny" "$PROJENY" commit w.projeny
run_in "$T81" expect_fail "add refuses conflicted .projeny" "$PROJENY" add w.projeny w/a.c
run_in "$T81" expect_fail "rm refuses conflicted .projeny" "$PROJENY" rm w.projeny w/a.c
run_in "$T81" expect_fail "mv refuses conflicted .projeny" "$PROJENY" mv w.projeny w/a.c w/b.c
run_in "$T81" expect_fail "resolve refuses conflicted .projeny" "$PROJENY" resolve w.projeny w/a.c
run_in "$T81" expect_fail "rebase refuses conflicted .projeny" "$PROJENY" rebase w.projeny w-1.0.tar.gz
# resolve+commit works after fixing through the conflicted setup.
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T81/w.projeny"
(cd "$T81" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'alpha 1\nbeta FIXED\ngamma 1\ndelta 1\n' > "$T81/w/a.c"
run_in "$T81" expect_ok "resolve after conflicted setup" "$PROJENY" resolve w.projeny w/a.c
run_in "$T81" expect_ok "commit after conflicted setup" "$PROJENY" commit w.projeny
expect_file_contains "commit stores the fix" "$T81/w.projeny" "beta FIXED"

# ----------------------------------------- 80. harder + no-checkout + diff3
T82="$ROOT/t82"
mkdir -p "$T82"
cp "$T81/w-1.0.tar.gz" "$T82/"
cp "$ROOT/t81-local.projeny" "$T82/w.projeny"
(cd "$T82" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
# uncommitted edit in a disjoint region (harder case).
python3 - "$T82/w/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("delta 1", "delta UNCOMMITTED")
open(p, "w").write(s)
EOF
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T82/w.projeny" diff3
expect_file_contains "diff3 fixture has base section" "$T82/w.projeny" "|||||||"
run_in "$T82" expect_fail "conflicted setup (harder, diff3) exits nonzero" "$PROJENY" setup w.projeny
expect_file_contains "harder case keeps uncommitted edit" "$T82/w/a.c" "delta UNCOMMITTED"
expect_file_contains "harder case still marks the conflict" "$T82/w/a.c" "<<<<<<<"
if cmp -s "$T82/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "harder case force-takes upstream"
else
    fail "harder case force-takes upstream"
fi
# no-checkout case: status file but no workdir (the checkout was lost).
# The status copy still breaks the direction tie, so the merge resolves.
T83="$ROOT/t83"
mkdir -p "$T83"
cp "$T81/w-1.0.tar.gz" "$T83/"
cp "$ROOT/t81-local.projeny" "$T83/w.projeny"
(cd "$T83" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
rm -rf "$T83/w"
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T83/w.projeny"
run_in "$T83" expect_fail "conflicted setup (no checkout) exits nonzero" "$PROJENY" setup w.projeny
if cmp -s "$T83/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "no-checkout case force-takes upstream"
else
    fail "no-checkout case force-takes upstream"
fi
expect_file_contains "no-checkout case marks conflicts" "$T83/w/a.c" "<<<<<<<"
expect_file_contains "no-checkout case records conflict" "$T83/.w.projeny.status" "Conflict: a.c"
# header-only conflict (prose differs, patch identical) merges cleanly.
# The checkout is set up from the local-prose side first so the status copy
# breaks the direction tie (a voteless merge with no status must die).
T84="$ROOT/t84"
mkdir -p "$T84"
cp "$T81/w-1.0.tar.gz" "$T84/"
python3 - "$ROOT/t81-local.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read()
open(sys.argv[1] + ".ours", "w").write(s.replace("    C.\n", "    Local prose.\n"))
open(sys.argv[1] + ".theirs", "w").write(s.replace("    C.\n", "    Upstream prose.\n"))
PYEOF
cp "$ROOT/t81-local.projeny.ours" "$T84/w.projeny"
(cd "$T84" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
make_conflicted "$ROOT/t81-local.projeny.ours" "$ROOT/t81-local.projeny.theirs" "$T84/w.projeny"
(cd "$T84" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
if [ $? -eq 0 ]; then
    ok "prose-only conflict sets up"
else
    fail "prose-only conflict sets up"
fi
expect_file_contains "prose-only conflict takes upstream prose" "$T84/w.projeny" "Upstream prose."
if grep -q "^Conflict:" "$T84/.w.projeny.status"; then
    fail "prose-only conflict records no conflicts" "$(cat "$T84/.w.projeny.status")"
else
    ok "prose-only conflict records no conflicts"
fi
expect_file_contains "prose-only workdir keeps patch content" "$T84/w/a.c" "beta LOCAL"

# ----------------------------------------- 81. malformed/truncated/binary refuse
T85="$ROOT/t85"
mkdir -p "$T85"
cp "$T81/w-1.0.tar.gz" "$T85/"
cp "$ROOT/t81-local.projeny" "$T85/w.projeny"
(cd "$T85" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
cp "$T85/w/a.c" "$ROOT/t85-a-before.c"
# malformed: opener with no closer.
python3 -c "open('$T85/w.projeny','w').write(open('$ROOT/t81-local.projeny').read().replace('+beta LOCAL', '<<<<<<< HEAD\\n+beta LOCAL'))"
run_in "$T85" expect_fail "unclosed markers fail setup" "$PROJENY" setup w.projeny
if cmp -s "$T85/w/a.c" "$ROOT/t85-a-before.c"; then
    ok "failed setup leaves workdir untouched"
else
    fail "failed setup leaves workdir untouched"
fi
# truncated: no markers, headers incomplete.
python3 -c "open('$T85/w.projeny','w').write('Archive: w-1.0.tar.gz\nOrigname: TRUNC')"
run_in "$T85" expect_fail "truncated .projeny fails setup" "$PROJENY" setup w.projeny
if cmp -s "$T85/w/a.c" "$ROOT/t85-a-before.c"; then
    ok "truncated setup leaves workdir untouched"
else
    fail "truncated setup leaves workdir untouched"
fi
# Archive pointing at a missing tarball (half-pulled tree, LFS-style).
python3 -c "open('$T85/w.projeny','w').write(open('$ROOT/t81-local.projeny').read().replace('w-1.0.tar.gz', 'w-9.9.tar.gz'))"
run_in "$T85" expect_fail "missing tarball fails setup" "$PROJENY" setup w.projeny
if cmp -s "$T85/w/a.c" "$ROOT/t85-a-before.c"; then
    ok "missing-tarball setup leaves workdir untouched"
else
    fail "missing-tarball setup leaves workdir untouched"
fi
# binary garbage (NUL bytes, as a bad merge driver might leave).
python3 -c "open('$T85/w.projeny','wb').write(b'Archive: x\x00binary\n')"
run_in "$T85" expect_fail "binary .projeny fails setup" "$PROJENY" setup w.projeny
if cmp -s "$T85/w/a.c" "$ROOT/t85-a-before.c"; then
    ok "binary setup leaves workdir untouched"
else
    fail "binary setup leaves workdir untouched"
fi
# CRLF conflicted file: warns but resolves like LF. The conflict is built
# exactly like make_conflicted (common prefix/suffix factored out so each
# side byte-matches its fixture); only the line endings are CRLF. Note the
# trailing "" split element already terminates the text, so no extra
# newline is appended (an extra one would leave a phantom blank line in
# the split sides and break the status-copy comparison).
python3 - "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T85/w.projeny" <<'PYEOF'
import sys
a = open(sys.argv[1]).read().split("\n")
b = open(sys.argv[2]).read().split("\n")
n = min(len(a), len(b))
i = 0
while i < n and a[i] == b[i]:
    i += 1
j0, j1 = len(a), len(b)
while j0 > i and j1 > i and a[j0 - 1] == b[j1 - 1]:
    j0 -= 1
    j1 -= 1
out = a[:i] + ["<<<<<<< HEAD"] + a[i:j0] + ["======="] + b[i:j1] + [">>>>>>> branch"] + a[j0:]
open(sys.argv[3], "w").write("\r\n".join(out))
PYEOF
out="$(cd "$T85" && "$PROJENY" setup w.projeny 2>&1)"
if [ $? -ne 0 ]; then
    ok "CRLF conflicted setup exits nonzero"
else
    fail "CRLF conflicted setup exits nonzero" "out: $out"
fi
if echo "$out" | grep -qi "CRLF"; then
    ok "CRLF setup warns about line endings"
else
    fail "CRLF setup warns about line endings" "out: $out"
fi
expect_file_contains "CRLF setup marks conflicts" "$T85/w/a.c" "<<<<<<<"
# ----------------------------------------- 82. git-merge conflict via setup
if command -v git >/dev/null 2>&1; then
    export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t GIT_EDITOR=true
    G86="$ROOT/t86"
    mkdir -p "$G86/repo"
    (cd "$G86/repo" && git init -q -b master . && mkdir w-1.0 && printf 'alpha 1\nbeta 1\ngamma 1\ndelta 1\n' > w-1.0/a.c && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0 && printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    G.\n' > w.projeny && "$PROJENY" setup w.projeny >/dev/null 2>&1 && git add w-1.0.tar.gz w.projeny && git commit -qm base)
    # upstream branch: beta -> UPSTREAM, committed to git.
    (cd "$G86/repo" && git checkout -qb upstream && python3 - w/a.c <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("beta 1", "beta UPSTREAM")
open(p, "w").write(s)
PYEOF
    "$PROJENY" commit w.projeny >/dev/null 2>&1 && git commit -qm upstream w.projeny)
    cp "$G86/repo/w.projeny" "$ROOT/t86-up.projeny"
    # local branch off base: beta -> LOCAL, committed to git.
    (cd "$G86/repo" && git checkout -q master && rm -rf w .w.projeny.status && "$PROJENY" setup w.projeny >/dev/null 2>&1 && python3 - w/a.c <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("beta 1", "beta LOCAL")
open(p, "w").write(s)
PYEOF
    "$PROJENY" commit w.projeny >/dev/null 2>&1 && git checkout -qb local && git commit -qm local w.projeny)
    # merge upstream into local: git conflicts on w.projeny.
    (cd "$G86/repo" && git merge -q upstream >/dev/null 2>&1)
    if [ $? -ne 0 ] && grep -q "<<<<<<<" "$G86/repo/w.projeny"; then
        ok "git merge conflicts the .projeny file"
    else
        fail "git merge conflicts the .projeny file" "$(cat "$G86/repo/w.projeny" 2>&1 | head -20)"
    fi
    # harder case: uncommitted disjoint edit before projeny takes over.
    (cd "$G86/repo" && python3 - w/a.c <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("delta 1", "delta UNCOMMITTED")
open(p, "w").write(s)
PYEOF
)
    out="$(cd "$G86/repo" && "$PROJENY" setup w.projeny 2>&1)"
    if [ $? -ne 0 ]; then
        ok "setup resolves git conflict markers (exits nonzero)"
    else
        fail "setup resolves git conflict markers (exits nonzero)" "out: $out"
    fi
    if cmp -s "$G86/repo/w.projeny" "$ROOT/t86-up.projeny"; then
        ok "git conflict takes upstream bytes"
    else
        fail "git conflict takes upstream bytes" "$(cat "$G86/repo/w.projeny")"
    fi
    expect_file_contains "git setup keeps uncommitted edit" "$G86/repo/w/a.c" "delta UNCOMMITTED"
    expect_file_contains "git setup marks beta conflict" "$G86/repo/w/a.c" "<<<<<<<"
    expect_file_contains "git setup records conflict" "$G86/repo/.w.projeny.status" "Conflict: a.c"
    if echo "$out" | grep -q "a.c"; then
        ok "git setup lists conflicted files"
    else
        fail "git setup lists conflicted files" "out: $out"
    fi
    # finish the merge by hand: fix, resolve, commit, keep git history sane.
    (cd "$G86/repo" && printf 'alpha 1\nbeta MERGED\ngamma 1\ndelta UNCOMMITTED\n' > w/a.c && "$PROJENY" resolve w.projeny w/a.c >/dev/null 2>&1 && "$PROJENY" commit w.projeny >/dev/null 2>&1 && git add w.projeny && git -c core.editor=true commit -qm resolved)
    if [ $? -eq 0 ]; then
        ok "resolve+commit finishes the git merge"
    else
        fail "resolve+commit finishes the git merge"
    fi
    # stash-pop flavor: conflicting uncommitted edit reapplied over a move.
    (cd "$G86/repo" && git checkout -qb spop && python3 - w/a.c <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("gamma 1", "gamma STASHED")
open(p, "w").write(s)
PYEOF
    "$PROJENY" commit w.projeny >/dev/null 2>&1 && git stash push -q -- w.projeny && "$PROJENY" setup w.projeny >/dev/null 2>&1 && python3 - w/a.c <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("gamma 1", "gamma OTHER")
open(p, "w").write(s)
PYEOF
    "$PROJENY" commit w.projeny >/dev/null 2>&1 && git commit -qm other w.projeny && git stash pop >/dev/null 2>&1; true)
    if grep -q "<<<<<<<" "$G86/repo/w.projeny"; then
        ok "stash pop conflicts the .projeny file"
    else
        fail "stash pop conflicts the .projeny file" "$(head -20 "$G86/repo/w.projeny")"
    fi
    out="$(cd "$G86/repo" && "$PROJENY" setup w.projeny 2>&1)"
    if [ $? -ne 0 ] && grep -q "<<<<<<<" "$G86/repo/w/a.c"; then
        ok "setup resolves stash-pop markers into workdir conflicts (nonzero)"
    else
        fail "setup resolves stash-pop markers into workdir conflicts (nonzero)" "out: $out"
    fi
    unset GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL GIT_COMMITTER_NAME GIT_COMMITTER_EMAIL GIT_EDITOR
else
    ok "git conflict tests (skipped: no git)"
fi

# ----------------------------------------- 84. rebase-direction conflicted .projeny
# `git pull --rebase` swaps the sides: <<<<<<< HEAD holds upstream, >>>>>>>
# names the local commit. setup must still take the upstream side (the old
# code force-took the second side and kept the local file instead).
T88="$ROOT/t88"
mkdir -p "$T88"
cp "$T81/w-1.0.tar.gz" "$T88/"
cp "$ROOT/t81-local.projeny" "$T88/w.projeny"
(cd "$T88" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T88/w.projeny" <<'PYEOF'
import sys
local = open(sys.argv[1]).read().split("\n")
up = open(sys.argv[2]).read().split("\n")
n = min(len(local), len(up))
i = 0
while i < n and local[i] == up[i]:
    i += 1
j0, j1 = len(local), len(up)
while j0 > i and j1 > i and local[j0 - 1] == up[j1 - 1]:
    j0 -= 1
    j1 -= 1
# rebase order: upstream content first (HEAD side), local content second.
blk = ["<<<<<<< HEAD"] + up[i:j1] + ["======="] + local[i:j0] + [">>>>>>> pick deadbeef local beta work"]
open(sys.argv[3], "w").write("\n".join(local[:i] + blk + local[j0:]))
PYEOF
out="$(cd "$T88" && "$PROJENY" setup w.projeny 2>&1)"
if [ $? -ne 0 ]; then
    ok "conflicted setup (rebase order) exits nonzero"
else
    fail "conflicted setup (rebase order) exits nonzero" "out: $out"
fi
if cmp -s "$T88/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "rebase-order markers still take upstream"
else
    fail "rebase-order markers still take upstream" "$(cat "$T88/w.projeny")"
fi
if cmp -s <(sed -n '/^--- projeny content ---$/,$p' "$T88/.w.projeny.status" | tail -n +2) "$ROOT/t81-up.projeny" 2>/dev/null || cmp -s "$T88/w.projeny" <(sed -n '/^--- projeny content ---$/,$p' "$T88/.w.projeny.status" | tail -n +2); then
    ok "rebase-order status embeds the upstream copy"
else
    fail "rebase-order status embeds the upstream copy" "$(head -8 "$T88/.w.projeny.status")"
fi
expect_file_contains "rebase-order checkout has workdir markers" "$T88/w/a.c" "<<<<<<<"
expect_file_contains "rebase-order checkout keeps local side" "$T88/w/a.c" "beta LOCAL"
expect_file_contains "rebase-order checkout keeps upstream side" "$T88/w/a.c" "beta UPSTREAM"
expect_file_contains "rebase-order checkout records conflict" "$T88/.w.projeny.status" "Conflict: a.c"
if echo "$out" | grep -qi "rebase"; then
    ok "rebase-order setup says which side it took"
else
    fail "rebase-order setup says which side it took" "out: $out"
fi
printf 'alpha 1\nbeta FIXED\ngamma 1\ndelta 1\n' > "$T88/w/a.c"
run_in "$T88" expect_ok "resolve after rebase-order setup" "$PROJENY" resolve w.projeny w/a.c
run_in "$T88" expect_ok "commit after rebase-order setup" "$PROJENY" commit w.projeny
expect_file_contains "rebase-order commit stores the fix" "$T88/w.projeny" "beta FIXED"

# ----------------------------------------- 85. stash-direction conflicted .projeny
# `git stash pop` labels the sides "Updated upstream" (checkout) and
# "Stashed changes" (local change); setup must take the checkout side.
T89="$ROOT/t89"
mkdir -p "$T89"
cp "$T81/w-1.0.tar.gz" "$T89/"
cp "$ROOT/t81-local.projeny" "$T89/w.projeny"
(cd "$T89" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T89/w.projeny" <<'PYEOF'
import sys
local = open(sys.argv[1]).read().split("\n")
up = open(sys.argv[2]).read().split("\n")
n = min(len(local), len(up))
i = 0
while i < n and local[i] == up[i]:
    i += 1
j0, j1 = len(local), len(up)
while j0 > i and j1 > i and local[j0 - 1] == up[j1 - 1]:
    j0 -= 1
    j1 -= 1
blk = ["<<<<<<< Updated upstream"] + up[i:j1] + ["======="] + local[i:j0] + [">>>>>>> Stashed changes"]
open(sys.argv[3], "w").write("\n".join(local[:i] + blk + local[j0:]))
PYEOF
out="$(cd "$T89" && "$PROJENY" setup w.projeny 2>&1)"
if [ $? -ne 0 ]; then
    ok "conflicted setup (stash labels) exits nonzero"
else
    fail "conflicted setup (stash labels) exits nonzero" "out: $out"
fi
if cmp -s "$T89/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "stash labels take the checkout (upstream) side"
else
    fail "stash labels take the checkout (upstream) side" "$(cat "$T89/w.projeny")"
fi
expect_file_contains "stash-label checkout has workdir markers" "$T89/w/a.c" "<<<<<<<"
expect_file_contains "stash-label checkout records conflict" "$T89/.w.projeny.status" "Conflict: a.c"

# ----------------------------------------- 86. patch path traversal refused
# Patch member paths must never escape the tree (like tar members): absolute
# labels and any ".." component — including ones only visible after the
# a/b + workdir prefix strip (a/w/../../evil with wid w -> ../../evil) —
# are hard errors, refused before anything is written.
T90="$ROOT/t90"
mkdir -p "$T90/w-1.0"
printf 'hello\n' > "$T90/w-1.0/f.c"
(cd "$T90" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Traversal.\n' > "$T90/base.projeny"
cat >> "$T90/base.projeny" <<'EOF'

diff --git a/w/f.c b/w/f.c
--- a/w/f.c
+++ b/w/f.c
@@ -1 +1 @@
-hello
+hello world
EOF
make_traversal_projeny() {
    # $1=out, $2=plusplus-label: copy the benign patch, swap the +++ label.
    python3 - "$T90/base.projeny" "$1" "$2" <<'PYEOF'
import sys
s = open(sys.argv[1]).read().replace("+++ b/w/f.c", "+++ " + sys.argv[3])
open(sys.argv[2], "w").write(s)
PYEOF
}
make_traversal_projeny "$T90/widstrip.projeny" "b/w/../../escape1"
run_in "$T90" expect_fail "patch wid-strip ../ refused" "$PROJENY" setup widstrip.projeny
out="$(cd "$T90" && "$PROJENY" setup widstrip.projeny 2>&1 || true)"
case "$out" in
*traversal*)
    ok "wid-strip error names traversal"
    ;;
*)
    fail "wid-strip error names traversal" "out: $out"
    ;;
esac
make_traversal_projeny "$T90/abs.projeny" "b//etc/abs-escape"
run_in "$T90" expect_fail "patch absolute member refused" "$PROJENY" setup abs.projeny
make_traversal_projeny "$T90/plain.projeny" "../plain-escape"
run_in "$T90" expect_fail "patch plain ../ refused" "$PROJENY" setup plain.projeny
# rename traversal.
python3 - "$T90/base.projeny" "$T90/rename.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read()
s = s.replace("+++ b/w/f.c", "+++ b/w/g.c")
s = s.replace("diff --git a/w/f.c b/w/f.c", "diff --git a/w/f.c b/w/g.c\nrename from w/f.c\nrename to w/../../rename-escape")
open(sys.argv[2], "w").write(s)
PYEOF
run_in "$T90" expect_fail "patch rename ../ refused" "$PROJENY" setup rename.projeny
# direct `projeny patch` traversal (same parser, refused before applying).
mkdir -p "$T90/dir"
printf 'hello\n' > "$T90/dir/f.c"
cat > "$T90/evil.diff" <<'EOF'
diff --git a/dir/f.c b/dir/f.c
--- a/dir/f.c
+++ b/dir/../../patched-escape
@@ -1 +1 @@
-hello
+bye
EOF
run_in "$T90" expect_fail "projeny patch ../ refused" "$PROJENY" patch dir evil.diff
expect_file_contains "refused patch leaves the tree untouched" "$T90/dir/f.c" "hello"
if [ -e "$T90/escape1" ] || [ -e "$T90/../escape1" ] || [ -e "$T90/patched-escape" ] || [ -e "$ROOT/escape1" ] || [ -e "/tmp/escape1" ]; then
    fail "traversal planted no files outside the tree" "$(ls "$T90" 2>&1)"
else
    ok "traversal planted no files outside the tree"
fi
if [ -d "$T90/w" ]; then
    fail "refused setup creates no workdir" "$(ls "$T90/w" 2>&1)"
else
    ok "refused setup creates no workdir"
fi

# ----------------------------------------- 87. conflicted setup keeps state
# Previously recorded (unresolved) status conflicts are unioned into the new
# list, never silently dropped. A .projeny deleted on one side of the git
# conflict (and a missing .projeny file) gets recovery guidance, not a bare
# ENOENT/missing-header error.
T91="$ROOT/t91"
mkdir -p "$T91"
cp "$T81/w-1.0.tar.gz" "$T91/"
cp "$ROOT/t81-local.projeny" "$T91/w.projeny"
(cd "$T91" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$T91/.w.projeny.status" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("Status: setup\n", "Status: setup\nConflict: stale/old.c\n")
open(p, "w").write(s)
PYEOF
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T91/w.projeny"
run_in "$T91" expect_fail "conflicted setup with stale conflicts exits nonzero" "$PROJENY" setup w.projeny
expect_file_contains "new conflict recorded" "$T91/.w.projeny.status" "Conflict: a.c"
expect_file_contains "stale conflict kept (union)" "$T91/.w.projeny.status" "Conflict: stale/old.c"
# deletion on one side: empty first side.
T92="$ROOT/t92"
mkdir -p "$T92"
cp "$T81/w-1.0.tar.gz" "$T92/"
python3 - "$ROOT/t81-local.projeny" "$T92/w.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read()
open(sys.argv[2], "w").write("<<<<<<< HEAD\n=======\n" + s + ">>>>>>> branch\n")
PYEOF
run_in "$T92" expect_fail "deleted-side conflict fails setup" "$PROJENY" setup w.projeny
out="$(cd "$T92" && "$PROJENY" setup w.projeny 2>&1 || true)"
case "$out" in
*deleted*checkout*)
    ok "deleted-side error guides recovery"
    ;;
*)
    fail "deleted-side error guides recovery" "out: $out"
    ;;
esac
# deletion on the other side: empty second side.
python3 - "$ROOT/t81-local.projeny" "$T92/w.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read()
open(sys.argv[2], "w").write("<<<<<<< HEAD\n" + s + "=======\n>>>>>>> branch\n")
PYEOF
run_in "$T92" expect_fail "other-side deletion fails setup" "$PROJENY" setup w.projeny
# missing .projeny file: recovery guidance, not bare ENOENT.
T93="$ROOT/t93"
mkdir -p "$T93"
run_in "$T93" expect_fail "missing .projeny fails setup" "$PROJENY" setup gone.projeny
out="$(cd "$T93" && "$PROJENY" setup gone.projeny 2>&1 || true)"
case "$out" in
*checkout*)
    ok "missing-file error guides recovery"
    ;;
*)
    fail "missing-file error guides recovery" "out: $out"
    ;;
esac

# ----------------------------------------- 88. prose/marker ambiguity
# Tool-written files never contain a column-0 prose line (commit prepends a
# single space when missing), so column-0 marker-shaped lines are always
# genuine git conflicts: indented marker-like prose roundtrips, a column-0
# marker-shaped prose line is refused with an indent hint, and column-0
# non-marker prose migrates on commit.
T94="$ROOT/t94"
mkdir -p "$T94"
cp "$T81/w-1.0.tar.gz" "$T94/"
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Prose with indented markers:\n     =======\n     <<<<<<< not a conflict\n     >>>>>>> nope\n' > "$T94/w.projeny"
run_in "$T94" expect_ok "indented marker-like prose sets up" "$PROJENY" setup w.projeny
python3 - "$T94/w/a.c" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("beta 1", "beta EDIT")
open(p, "w").write(s)
PYEOF
run_in "$T94" expect_ok "indented marker-like prose commits" "$PROJENY" commit w.projeny
expect_file_contains "indented prose survives commit" "$T94/w.projeny" "======="
run_in "$T94" expect_ok "indented prose still sets up" "$PROJENY" setup w.projeny
T95="$ROOT/t95"
mkdir -p "$T95"
cp "$T81/w-1.0.tar.gz" "$T95/"
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Prose then a trap:\n=======\n' > "$T95/w.projeny"
run_in "$T95" expect_fail "column-0 marker prose fails setup" "$PROJENY" setup w.projeny
out="$(cd "$T95" && "$PROJENY" setup w.projeny 2>&1 || true)"
case "$out" in
*indent*)
    ok "column-0 prose error hints at indenting"
    ;;
*)
    fail "column-0 prose error hints at indenting" "out: $out"
    ;;
esac
T96="$ROOT/t96"
mkdir -p "$T96"
cp "$T81/w-1.0.tar.gz" "$T96/"
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\ncol0 prose line\n' > "$T96/w.projeny"
run_in "$T96" expect_ok "column-0 non-marker prose sets up" "$PROJENY" setup w.projeny
run_in "$T96" expect_ok "column-0 non-marker prose commits" "$PROJENY" commit w.projeny
if grep -q "^ col0 prose line" "$T96/w.projeny"; then
    ok "commit migrates prose to leading-space indent"
else
    fail "commit migrates prose to leading-space indent" "$(cat "$T96/w.projeny")"
fi
run_in "$T96" expect_ok "migrated prose still sets up" "$PROJENY" setup w.projeny

# ----------------------------------------- 83. help topics
T87="$ROOT/t87"
mkdir -p "$T87"
for t in setup commit add rm mv resolve rebase status diff patch help; do
    out="$(cd "$T87" && "$PROJENY" help "$t" 2>&1)"
    if [ $? -eq 0 ] && [ -n "$out" ]; then
        ok "help $t exits 0 with text"
    else
        fail "help $t exits 0 with text" "out: $out"
    fi
done
for t in "setup:setup" "commit:commit" "patch:patch" "diff:diff" "rebase:rebase" "resolve:resolve" "status:status"; do
    topic="${t%%:*}"
    want="${t##*:}"
    if "$PROJENY" help "$topic" 2>&1 | grep -qi "$want"; then
        ok "help $topic mentions $want"
    else
        fail "help $topic mentions $want"
    fi
done
# Every topic must carry a real content marker, not just its own name
# (non-empty output alone proves nothing).
for t in "setup:conflict" "commit:diff the workdir" \
         "add:added-but-not-committed" "rm:removed-but-not-committed" \
         "mv:record the rename" "resolve:conflict" "rebase:tarball" \
         "status:Status:" "diff:uncommitted" "patch:fuzz" \
         "help:command name"; do
    topic="${t%%:*}"
    want="${t##*:}"
    if "$PROJENY" help "$topic" 2>&1 | grep -qF -- "$want"; then
        ok "help $topic explains '$want'"
    else
        fail "help $topic explains '$want'"
    fi
done
if "$PROJENY" help setup 2>&1 | grep -q "conflict"; then
    ok "help setup explains conflicts"
else
    fail "help setup explains conflicts"
fi
if "$PROJENY" help patch 2>&1 | grep -q "<<<<<<<"; then
    ok "help patch documents markers"
else
    fail "help patch documents markers"
fi
run_in "$T87" expect_fail "help bogus fails" "$PROJENY" help bogus
run_in "$T87" expect_ok "bare help still works" "$PROJENY" help

# ----------------------------------------- 89. rebase order without status
# Rebase-order markers (upstream first, local second) with no status file:
# the old code fell through to the merge default and kept the local file.
# The closer below is a realistic rebase label ("<sha> (<subject>)") whose
# subject contains "original" — a substring trap for naive matching
# ("origin" inside "original" would vote the wrong side; token matching
# must not fire there, while the commit-hash shape must vote rebase).
T97="$ROOT/t97"
mkdir -p "$T97"
cp "$T81/w-1.0.tar.gz" "$T97/"
cp "$ROOT/t81-local.projeny" "$T97/w.projeny"
(cd "$T97" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
rm -f "$T97/.w.projeny.status"
python3 - "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T97/w.projeny" <<'PYEOF'
import sys
local = open(sys.argv[1]).read().split("\n")
up = open(sys.argv[2]).read().split("\n")
n = min(len(local), len(up))
i = 0
while i < n and local[i] == up[i]:
    i += 1
j0, j1 = len(local), len(up)
while j0 > i and j1 > i and local[j0 - 1] == up[j1 - 1]:
    j0 -= 1
    j1 -= 1
# rebase order: upstream content first (HEAD side), local content second,
# with a commit-hash closer whose subject hides an "origin" substring.
blk = ["<<<<<<< HEAD"] + up[i:j1] + ["======="] + local[i:j0] + [">>>>>>> 99d93bd (original work)"]
open(sys.argv[3], "w").write("\n".join(local[:i] + blk + local[j0:]))
PYEOF
out="$(cd "$T97" && "$PROJENY" setup w.projeny 2>&1)"
if [ $? -ne 0 ]; then
    ok "rebase-order setup without status exits nonzero"
else
    fail "rebase-order setup without status exits nonzero" "out: $out"
fi
if cmp -s "$T97/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "no-status rebase markers still take upstream"
else
    fail "no-status rebase markers still take upstream" "$(cat "$T97/w.projeny")"
fi
expect_file_contains "no-status rebase checkout has workdir markers" "$T97/w/a.c" "<<<<<<<"
expect_file_contains "no-status rebase checkout keeps local side" "$T97/w/a.c" "beta LOCAL"
expect_file_contains "no-status rebase checkout keeps upstream side" "$T97/w/a.c" "beta UPSTREAM"
expect_file_contains "no-status rebase checkout records conflict" "$T97/.w.projeny.status" "Conflict: a.c"
if echo "$out" | grep -qi "rebase"; then
    ok "no-status rebase setup says which side it took"
else
    fail "no-status rebase setup says which side it took" "out: $out"
fi
# Stale status (embeds a lineage matching neither side) must not hijack
# the direction either: labels still decide.
T97S="$ROOT/t97s"
mkdir -p "$T97S"
cp "$T81/w-1.0.tar.gz" "$T97S/"
cp "$T97/w.projeny" "$T97S/w.projeny"
cp "$T97/.w.projeny.status" "$T97S/.w.projeny.status"
cp -r "$T97/w" "$T97S/w"
python3 - "$T97S/.w.projeny.status" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("    G.\n", "    Stale lineage.\n")
open(p, "w").write(s)
PYEOF
run_in "$T97S" expect_fail "rebase-order setup with stale status exits nonzero" "$PROJENY" setup w.projeny
if cmp -s "$T97S/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "stale-status rebase markers still take upstream"
else
    fail "stale-status rebase markers still take upstream" "$(cat "$T97S/w.projeny")"
fi
# Merge order with a substring-trap branch name ("original-work" contains
# "origin" but its tokens are "original"/"work"): the label casts no vote,
# and the status copy breaks the tie, so the second side is still taken as
# upstream. (Without the status copy this shape must die as ambiguous.)
T97M="$ROOT/t97m"
mkdir -p "$T97M"
cp "$T81/w-1.0.tar.gz" "$T97M/"
cp "$ROOT/t81-local.projeny" "$T97M/w.projeny"
(cd "$T97M" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T97M/w.projeny" <<'PYEOF'
import sys
local = open(sys.argv[1]).read().split("\n")
up = open(sys.argv[2]).read().split("\n")
n = min(len(local), len(up))
i = 0
while i < n and local[i] == up[i]:
    i += 1
j0, j1 = len(local), len(up)
while j0 > i and j1 > i and local[j0 - 1] == up[j1 - 1]:
    j0 -= 1
    j1 -= 1
# merge order: local first, upstream second, closer is a branch name.
blk = ["<<<<<<< HEAD"] + local[i:j0] + ["======="] + up[i:j1] + [">>>>>>> original-work"]
open(sys.argv[3], "w").write("\n".join(local[:i] + blk + local[j0:]))
PYEOF
run_in "$T97M" expect_fail "substring-trap merge setup exits nonzero" "$PROJENY" setup w.projeny
if cmp -s "$T97M/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "substring-trap branch still takes upstream"
else
    fail "substring-trap branch still takes upstream" "$(cat "$T97M/w.projeny")"
fi

# ----------------------------------------- 89b. direction false-positive guards
# Voteless labels must never pick the swapped side on a normal merge: a
# 4-char hex closer ("cafe ...", "beef" — below git's 7+ short-SHA length),
# a stash-mentioning branch ("stash-cleanup" — no "stashed changes" phrase
# or stash pair), and a ref closer ("origin/master") must not vote swapped.
# With the status copy present the merge still resolves to the upstream
# side via the status tiebreak.
T97X="$ROOT/t97x"
mkdir -p "$T97X"
cp "$T81/w-1.0.tar.gz" "$T97X/"
for closer in "cafe (my feature)" "beef" "stash-cleanup" "origin/master"; do
    rm -rf "$T97X/w" "$T97X/w.projeny" "$T97X/.w.projeny.status"
    cp "$ROOT/t81-local.projeny" "$T97X/w.projeny"
    (cd "$T97X" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
    python3 - "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T97X/w.projeny" "$closer" <<'PYEOF'
import sys
local = open(sys.argv[1]).read().split("\n")
up = open(sys.argv[2]).read().split("\n")
n = min(len(local), len(up))
i = 0
while i < n and local[i] == up[i]:
    i += 1
j0, j1 = len(local), len(up)
while j0 > i and j1 > i and local[j0 - 1] == up[j1 - 1]:
    j0 -= 1
    j1 -= 1
# merge order: local first, upstream second, closer carries no real signal.
blk = ["<<<<<<< HEAD"] + local[i:j0] + ["======="] + up[i:j1] + [">>>>>>> " + sys.argv[4]]
open(sys.argv[3], "w").write("\n".join(local[:i] + blk + local[j0:]))
PYEOF
    out="$(cd "$T97X" && "$PROJENY" setup w.projeny 2>&1)"
    if [ $? -ne 0 ]; then
        ok "merge with closer '$closer' exits nonzero"
    else
        fail "merge with closer '$closer' exits nonzero" "out: $out"
    fi
    if cmp -s "$T97X/w.projeny" "$ROOT/t81-up.projeny"; then
        ok "closer '$closer' still takes upstream"
    else
        fail "closer '$closer' still takes upstream" "$(cat "$T97X/w.projeny")"
    fi
    expect_file_contains "closer '$closer' records conflict" "$T97X/.w.projeny.status" "Conflict: a.c"
done
# Truly ambiguous with no status and no deciding label: setup must die
# instead of silently defaulting to the merge side, leaving everything
# untouched.
T97Y="$ROOT/t97y"
mkdir -p "$T97Y"
cp "$T81/w-1.0.tar.gz" "$T97Y/"
cp "$ROOT/t81-local.projeny" "$T97Y/w.projeny"
(cd "$T97Y" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
cp "$T97Y/w/a.c" "$ROOT/t97y-a-before.c"
rm -f "$T97Y/.w.projeny.status"
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T97Y/w.projeny"
run_in "$T97Y" expect_fail "ambiguous no-status conflict fails setup" "$PROJENY" setup w.projeny
out="$(cd "$T97Y" && "$PROJENY" setup w.projeny 2>&1 || true)"
case "$out" in
*ambiguous*)
    ok "ambiguous-direction error says ambiguous"
    ;;
*)
    fail "ambiguous-direction error says ambiguous" "out: $out"
    ;;
esac
if cmp -s "$T97Y/w/a.c" "$ROOT/t97y-a-before.c"; then
    ok "ambiguous setup leaves workdir untouched"
else
    fail "ambiguous setup leaves workdir untouched"
fi

# ----------------------------------------- 90. real git rebase / pull --rebase
if command -v git >/dev/null 2>&1; then
    export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t GIT_EDITOR=true
    # git rebase: local commit replayed onto upstream conflicts w.projeny.
    G98="$ROOT/t98"
    mkdir -p "$G98/repo"
    (cd "$G98/repo" && git init -q -b master . && mkdir w-1.0 && printf 'alpha 1\nbeta 1\ngamma 1\ndelta 1\n' > w-1.0/a.c && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0 && printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    G.\n' > w.projeny && "$PROJENY" setup w.projeny >/dev/null 2>&1 && git add w-1.0.tar.gz w.projeny && git commit -qm base)
    (cd "$G98/repo" && git checkout -qb upstream && python3 - w/a.c <<'PYEOF'
import sys
p = 'w/a.c'
s = open(p).read().replace("beta 1", "beta UPSTREAM")
open(p, "w").write(s)
PYEOF
    "$PROJENY" commit w.projeny >/dev/null 2>&1 && git commit -qm upstream w.projeny)
    cp "$G98/repo/w.projeny" "$ROOT/t98-up.projeny"
    (cd "$G98/repo" && git checkout -q master && rm -rf w .w.projeny.status && "$PROJENY" setup w.projeny >/dev/null 2>&1 && python3 - w/a.c <<'PYEOF'
import sys
p = 'w/a.c'
s = open(p).read().replace("beta 1", "beta LOCAL")
open(p, "w").write(s)
PYEOF
    "$PROJENY" commit w.projeny >/dev/null 2>&1 && git commit -qm "local beta work" w.projeny)
    (cd "$G98/repo" && git rebase upstream >/dev/null 2>&1)
    if [ $? -ne 0 ] && grep -q "<<<<<<<" "$G98/repo/w.projeny"; then
        ok "git rebase conflicts the .projeny file"
    else
        fail "git rebase conflicts the .projeny file" "$(head -20 "$G98/repo/w.projeny" 2>&1)"
    fi
    out="$(cd "$G98/repo" && "$PROJENY" setup w.projeny 2>&1)"
    if [ $? -ne 0 ]; then
        ok "setup resolves real rebase markers (exits nonzero)"
    else
        fail "setup resolves real rebase markers (exits nonzero)" "out: $out"
    fi
    if cmp -s "$G98/repo/w.projeny" "$ROOT/t98-up.projeny"; then
        ok "real rebase takes upstream bytes"
    else
        fail "real rebase takes upstream bytes" "$(cat "$G98/repo/w.projeny")"
    fi
    expect_file_contains "real rebase marks beta conflict" "$G98/repo/w/a.c" "<<<<<<<"
    expect_file_contains "real rebase keeps local side" "$G98/repo/w/a.c" "beta LOCAL"
    expect_file_contains "real rebase keeps upstream side" "$G98/repo/w/a.c" "beta UPSTREAM"
    expect_file_contains "real rebase records conflict" "$G98/repo/.w.projeny.status" "Conflict: a.c"
    (cd "$G98/repo" && printf 'alpha 1\nbeta MERGED\ngamma 1\ndelta 1\n' > w/a.c && "$PROJENY" resolve w.projeny w/a.c >/dev/null 2>&1 && "$PROJENY" commit w.projeny >/dev/null 2>&1 && git add w.projeny && git -c core.editor=true rebase --continue >/dev/null 2>&1)
    if [ $? -eq 0 ]; then
        ok "resolve+commit finishes the real rebase"
    else
        fail "resolve+commit finishes the real rebase"
    fi
    # git pull --rebase across a file remote: same marker family.
    R98="$ROOT/t98r"
    mkdir -p "$R98"
    (cd "$R98" && git init -q --bare remote.git && git clone -q remote.git local 2>/dev/null && cd local && mkdir w-1.0 && printf 'alpha 1\nbeta 1\ngamma 1\ndelta 1\n' > w-1.0/a.c && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0 && printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    G.\n' > w.projeny && "$PROJENY" setup w.projeny >/dev/null 2>&1 && git add w-1.0.tar.gz w.projeny && git commit -qm base && git push -q origin master)
    (cd "$R98" && git clone -q remote.git up 2>/dev/null && cd up && "$PROJENY" setup w.projeny >/dev/null 2>&1 && python3 - w/a.c <<'PYEOF'
import sys
p = 'w/a.c'
s = open(p).read().replace("beta 1", "beta UPSTREAM")
open(p, "w").write(s)
PYEOF
    "$PROJENY" commit w.projeny >/dev/null 2>&1 && git commit -qam upstream && git push -q origin master)
    cp "$R98/up/w.projeny" "$ROOT/t98r-up.projeny"
    (cd "$R98/local" && python3 - w/a.c <<'PYEOF'
import sys
p = 'w/a.c'
s = open(p).read().replace("beta 1", "beta LOCAL")
open(p, "w").write(s)
PYEOF
    "$PROJENY" commit w.projeny >/dev/null 2>&1 && git commit -qam "my local work" && git pull --rebase -q origin master >/dev/null 2>&1)
    if grep -q "<<<<<<<" "$R98/local/w.projeny"; then
        ok "git pull --rebase conflicts the .projeny file"
    else
        fail "git pull --rebase conflicts the .projeny file" "$(head -20 "$R98/local/w.projeny" 2>&1)"
    fi
    out="$(cd "$R98/local" && "$PROJENY" setup w.projeny 2>&1)"
    if [ $? -ne 0 ]; then
        ok "setup resolves real pull --rebase markers (exits nonzero)"
    else
        fail "setup resolves real pull --rebase markers (exits nonzero)" "out: $out"
    fi
    if cmp -s "$R98/local/w.projeny" "$ROOT/t98r-up.projeny"; then
        ok "real pull --rebase takes upstream bytes"
    else
        fail "real pull --rebase takes upstream bytes" "$(cat "$R98/local/w.projeny")"
    fi
    expect_file_contains "real pull --rebase marks beta conflict" "$R98/local/w/a.c" "<<<<<<<"
    expect_file_contains "real pull --rebase records conflict" "$R98/local/.w.projeny.status" "Conflict: a.c"
    (cd "$R98/local" && printf 'alpha 1\nbeta MERGED\ngamma 1\ndelta 1\n' > w/a.c && "$PROJENY" resolve w.projeny w/a.c >/dev/null 2>&1 && "$PROJENY" commit w.projeny >/dev/null 2>&1 && git add w.projeny && git -c core.editor=true rebase --continue >/dev/null 2>&1)
    if [ $? -eq 0 ]; then
        ok "resolve+commit finishes the real pull --rebase"
    else
        fail "resolve+commit finishes the real pull --rebase"
    fi
    unset GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL GIT_COMMITTER_NAME GIT_COMMITTER_EMAIL GIT_EDITOR
else
    ok "real rebase tests (skipped: no git)"
fi

# ----------------------------------------- 91. patch path traversal extras
# Wid-strip to the tree root itself ("b/w" with wid w, or "b/w/.") must be
# refused as traversal (it would otherwise target the tree directory), as
# must C-quoted rename escapes. Symlink ancestors must not be followed:
# with "link -> outside" in the target, a failing block for "link/evil"
# must stay a recorded conflict and plant nothing outside.
T99="$ROOT/t99"
mkdir -p "$T99/w-1.0"
printf 'hello\n' > "$T99/w-1.0/f.c"
(cd "$T99" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Traversal.\n' > "$T99/base.projeny"
cat >> "$T99/base.projeny" <<'EOF'

diff --git a/w/f.c b/w/f.c
--- a/w/f.c
+++ b/w/f.c
@@ -1 +1 @@
-hello
+hello world
EOF
python3 - "$T99/base.projeny" "$T99/empty.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read().replace("+++ b/w/f.c", "+++ b/w")
open(sys.argv[2], "w").write(s)
PYEOF
run_in "$T99" expect_fail "patch wid-strip-to-empty refused" "$PROJENY" setup empty.projeny
out="$(cd "$T99" && "$PROJENY" setup empty.projeny 2>&1 || true)"
case "$out" in
*traversal*)
    ok "wid-strip-to-empty error names traversal"
    ;;
*)
    fail "wid-strip-to-empty error names traversal" "out: $out"
    ;;
esac
python3 - "$T99/base.projeny" "$T99/dot.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read().replace("+++ b/w/f.c", "+++ b/w/.")
open(sys.argv[2], "w").write(s)
PYEOF
run_in "$T99" expect_fail "patch wid-strip-to-dot refused" "$PROJENY" setup dot.projeny
out="$(cd "$T99" && "$PROJENY" setup dot.projeny 2>&1 || true)"
case "$out" in
*traversal*)
    ok "wid-strip-to-dot error names traversal"
    ;;
*)
    fail "wid-strip-to-dot error names traversal" "out: $out"
    ;;
esac
# C-quoted rename escape ("w/../../rename-quoted" quoted git-style).
python3 - "$T99/base.projeny" "$T99/renameq.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read()
s = s.replace("+++ b/w/f.c", "+++ b/w/g.c")
s = s.replace("diff --git a/w/f.c b/w/f.c", "diff --git a/w/f.c b/w/g.c\nrename from w/f.c\nrename to \"w/../../rename-quoted\"")
open(sys.argv[2], "w").write(s)
PYEOF
run_in "$T99" expect_fail "patch quoted rename ../ refused" "$PROJENY" setup renameq.projeny
out="$(cd "$T99" && "$PROJENY" setup renameq.projeny 2>&1 || true)"
case "$out" in
*traversal*)
    ok "quoted-rename error names traversal"
    ;;
*)
    fail "quoted-rename error names traversal" "out: $out"
    ;;
esac
# Symlink ancestor: `projeny patch` on a dir holding "link -> outside".
mkdir -p "$T99/sym" "$T99/outside"
printf 'hello\n' > "$T99/sym/f.c"
ln -s "$T99/outside" "$T99/sym/link"
cat > "$T99/symlink.diff" <<'EOF'
diff --git a/sym/link/evil b/sym/link/evil
--- a/sym/link/evil
+++ b/sym/link/evil
@@ -1 +1 @@
-hello
+bye
EOF
out="$(cd "$T99" && "$PROJENY" patch sym symlink.diff 2>&1)"
if echo "$out" | grep -qi "conflict"; then
    ok "symlink-ancestor patch reports a conflict"
else
    fail "symlink-ancestor patch reports a conflict" "out: $out"
fi
if [ -e "$T99/outside/evil" ] || [ -e "$T99/sym/link/evil" ]; then
    fail "symlink-ancestor patch plants no files" "$(ls -la "$T99/outside" 2>&1)"
else
    ok "symlink-ancestor patch plants no files"
fi
expect_file_contains "symlink-ancestor patch leaves the tree untouched" "$T99/sym/f.c" "hello"
# Same, but the through-link file exists (failing hunk splice path): the
# outside file must keep its bytes instead of gaining markers.
printf 'outside bytes\n' > "$T99/outside/base.c"
cat > "$T99/symlink2.diff" <<'EOF'
diff --git a/sym/link/base.c b/sym/link/base.c
--- a/sym/link/base.c
+++ b/sym/link/base.c
@@ -1 +1 @@
-something else entirely
+changed
EOF
out="$(cd "$T99" && "$PROJENY" patch sym symlink2.diff 2>&1)"
if echo "$out" | grep -qi "conflict"; then
    ok "through-link failing hunk reports a conflict"
else
    fail "through-link failing hunk reports a conflict" "out: $out"
fi
expect_file_contains "through-link conflict leaves outside bytes alone" "$T99/outside/base.c" "outside bytes"
if grep -q "<<<<<<<" "$T99/outside/base.c"; then
    fail "through-link conflict writes no markers outside" "$(cat "$T99/outside/base.c")"
else
    ok "through-link conflict writes no markers outside"
fi

# ----------------------------------------- 91b. through-link non-create ops
# Delete/modify/chmod/rename patches touching link/evil (link -> outside)
# must fail as conflicts without touching anything outside the tree. Each
# patch below matches the outside bytes, so without the ancestor gate the
# op would apply cleanly straight through the link.
T99B="$ROOT/t99b"
mkdir -p "$T99B/sym" "$T99B/outside"
printf 'hello\n' > "$T99B/sym/f.c"
ln -s "$T99B/outside" "$T99B/sym/link"
printf 'doomed\n' > "$T99B/outside/evil"
printf 'keep me\n' > "$T99B/outside/cm"
chmod 644 "$T99B/outside/cm"
printf 'oldbytes\n' > "$T99B/outside/mv"
# delete with matching hunks: without the gate this unlinks outside/evil.
cat > "$T99B/del.diff" <<'EOF'
diff --git a/sym/link/evil b/sym/link/evil
deleted file mode 100644
--- a/sym/link/evil
+++ /dev/null
@@ -1 +0,0 @@
-doomed
EOF
out="$(cd "$T99B" && "$PROJENY" patch sym del.diff 2>&1)"
if echo "$out" | grep -qi "conflict"; then
    ok "through-link delete reports a conflict"
else
    fail "through-link delete reports a conflict" "out: $out"
fi
expect_file_contains "through-link delete leaves outside bytes alone" "$T99B/outside/evil" "doomed"
# modify with matching hunks: without the gate this rewrites outside/evil.
cat > "$T99B/mod.diff" <<'EOF'
diff --git a/sym/link/evil b/sym/link/evil
--- a/sym/link/evil
+++ b/sym/link/evil
@@ -1 +1 @@
-doomed
+changed
EOF
out="$(cd "$T99B" && "$PROJENY" patch sym mod.diff 2>&1)"
if echo "$out" | grep -qi "conflict"; then
    ok "through-link modify reports a conflict"
else
    fail "through-link modify reports a conflict" "out: $out"
fi
expect_file_contains "through-link modify leaves outside bytes alone" "$T99B/outside/evil" "doomed"
if grep -q "<<<<<<<" "$T99B/outside/evil"; then
    fail "through-link modify writes no markers outside" "$(cat "$T99B/outside/evil")"
else
    ok "through-link modify writes no markers outside"
fi
# chmod-only: without the gate this chmods outside/cm.
cat > "$T99B/chmod.diff" <<'EOF'
diff --git a/sym/link/cm b/sym/link/cm
old mode 100644
new mode 100755
--- a/sym/link/cm
+++ b/sym/link/cm
EOF
out="$(cd "$T99B" && "$PROJENY" patch sym chmod.diff 2>&1)"
if echo "$out" | grep -qi "conflict"; then
    ok "through-link chmod reports a conflict"
else
    fail "through-link chmod reports a conflict" "out: $out"
fi
expect_file_contains "through-link chmod leaves outside bytes alone" "$T99B/outside/cm" "keep me"
if [ -x "$T99B/outside/cm" ]; then
    fail "through-link chmod leaves outside mode alone" "$(stat -c %a "$T99B/outside/cm" 2>&1)"
else
    ok "through-link chmod leaves outside mode alone"
fi
# rename with hunks: without the gate this moves outside/mv to outside/mv2.
cat > "$T99B/mv.diff" <<'EOF'
diff --git a/sym/link/mv b/sym/link/mv2
similarity index 90%
rename from sym/link/mv
rename to sym/link/mv2
--- a/sym/link/mv
+++ b/sym/link/mv2
@@ -1 +1 @@
-oldbytes
+newbytes
EOF
out="$(cd "$T99B" && "$PROJENY" patch sym mv.diff 2>&1)"
if echo "$out" | grep -qi "conflict"; then
    ok "through-link rename reports a conflict"
else
    fail "through-link rename reports a conflict" "out: $out"
fi
expect_file_contains "through-link rename leaves outside bytes alone" "$T99B/outside/mv" "oldbytes"
if [ -e "$T99B/outside/mv2" ]; then
    fail "through-link rename plants no files outside" "$(ls -la "$T99B/outside" 2>&1)"
else
    ok "through-link rename plants no files outside"
fi
# pure rename: also a conflict, nothing planted outside, link intact.
cat > "$T99B/mvpure.diff" <<'EOF'
diff --git a/sym/link/mv b/sym/link/mv2
similarity index 100%
rename from sym/link/mv
rename to sym/link/mv2
EOF
out="$(cd "$T99B" && "$PROJENY" patch sym mvpure.diff 2>&1)"
if echo "$out" | grep -qi "conflict"; then
    ok "through-link pure rename reports a conflict"
else
    fail "through-link pure rename reports a conflict" "out: $out"
fi
expect_file_contains "through-link pure rename leaves outside bytes alone" "$T99B/outside/mv" "oldbytes"
if [ -e "$T99B/outside/mv2" ]; then
    fail "through-link pure rename plants no files outside" "$(ls -la "$T99B/outside" 2>&1)"
else
    ok "through-link pure rename plants no files outside"
fi
expect_file_contains "through-link ops leave the tree untouched" "$T99B/sym/f.c" "hello"
if [ -L "$T99B/sym/link" ]; then
    ok "through-link ops leave the link itself alone"
else
    fail "through-link ops leave the link itself alone" "$(ls -la "$T99B/sym" 2>&1)"
fi

# ----------------------------------------- 92. interrupted conflicted setup
# A crash between the conflicted-setup renames leaves a split brain (here:
# new .projeny + new .status with the markers gone, workdir missing). The
# setup journal makes it recoverable: rerunning setup must rebuild the
# conflicted checkout (not take the fresh path and discard the patch).
T100="$ROOT/t100"
mkdir -p "$T100"
cp "$T81/w-1.0.tar.gz" "$T100/"
cp "$ROOT/t81-local.projeny" "$T100/w.projeny"
(cd "$T100" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T100/w.projeny"
python3 - "$T100/w.projeny" "$T100/w.projeny.setup-journal" <<'PYEOF'
import sys
raw = open(sys.argv[1]).read()
journal = "projeny setup journal v1\nupstream: theirs\n--- conflicted .projeny ---\n" + raw
open(sys.argv[2], "w").write(journal)
PYEOF
# Simulate the crash: .projeny + .status already flipped to upstream, the
# merged workdir never landed.
cp "$ROOT/t81-up.projeny" "$T100/w.projeny"
python3 - "$ROOT/t81-up.projeny" "$T100/.w.projeny.status" <<'PYEOF'
import sys
up = open(sys.argv[1]).read()
open(sys.argv[2], "w").write("Status: setup\nConflict: a.c\n--- projeny content ---\n" + up)
PYEOF
rm -rf "$T100/w"
out="$(cd "$T100" && "$PROJENY" setup w.projeny 2>&1)"
if [ $? -ne 0 ]; then
    ok "split-brain setup recovers exits nonzero"
else
    fail "split-brain setup recovers exits nonzero" "out: $out"
fi
if cmp -s "$T100/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "recovered setup keeps upstream bytes"
else
    fail "recovered setup keeps upstream bytes" "$(cat "$T100/w.projeny")"
fi
expect_file_contains "recovered checkout has workdir markers" "$T100/w/a.c" "<<<<<<<"
expect_file_contains "recovered checkout keeps local side" "$T100/w/a.c" "beta LOCAL"
expect_file_contains "recovered checkout keeps upstream side" "$T100/w/a.c" "beta UPSTREAM"
expect_file_contains "recovered checkout records conflict" "$T100/.w.projeny.status" "Conflict: a.c"
if [ -e "$T100/w.projeny.setup-journal" ]; then
    fail "recovery removes the journal" "$(ls "$T100" 2>&1)"
else
    ok "recovery removes the journal"
fi
if echo "$out" | grep -qi "recover"; then
    ok "recovery says it recovered"
else
    fail "recovery says it recovered" "out: $out"
fi
if echo "$out" | grep -qi "rebase/stash-style"; then
    fail "merge-order recovery claims no rebase swap" "out: $out"
else
    ok "merge-order recovery claims no rebase swap"
fi
# Split brain plus drift: clean .projeny matching neither recorded side
# must fail loudly, never silently pick one.
T100B="$ROOT/t100b"
mkdir -p "$T100B"
cp "$T81/w-1.0.tar.gz" "$T100B/"
cp "$ROOT/t81-local.projeny" "$T100B/w.projeny"
(cd "$T100B" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T100B/w.projeny"
python3 - "$T100B/w.projeny" "$T100B/w.projeny.setup-journal" <<'PYEOF'
import sys
raw = open(sys.argv[1]).read()
journal = "projeny setup journal v1\nupstream: theirs\n--- conflicted .projeny ---\n" + raw
open(sys.argv[2], "w").write(journal)
PYEOF
python3 - "$ROOT/t81-up.projeny" "$T100B/w.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read().replace("beta UPSTREAM", "beta DRIFTED")
open(sys.argv[2], "w").write(s)
PYEOF
run_in "$T100B" expect_fail "drifted split-brain fails setup" "$PROJENY" setup w.projeny
out="$(cd "$T100B" && "$PROJENY" setup w.projeny 2>&1 || true)"
case "$out" in
*journal*)
    ok "drifted split-brain error names the journal"
    ;;
*)
    fail "drifted split-brain error names the journal" "out: $out"
    ;;
esac

# ----------------------------------------- 93. extended conflict markers
# Nested-conflict style markers (8+ characters) must be detected like the
# plain 7-char form: an all-extended block resolves, while a genuinely
# nested block (outer 8 + inner 7) fails loudly instead of being silently
# treated as clean.
T101="$ROOT/t101"
mkdir -p "$T101"
cp "$T81/w-1.0.tar.gz" "$T101/"
cp "$ROOT/t81-local.projeny" "$T101/w.projeny"
(cd "$T101" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T101/w.projeny" <<'PYEOF'
import sys
local = open(sys.argv[1]).read().split("\n")
up = open(sys.argv[2]).read().split("\n")
n = min(len(local), len(up))
i = 0
while i < n and local[i] == up[i]:
    i += 1
j0, j1 = len(local), len(up)
while j0 > i and j1 > i and local[j0 - 1] == up[j1 - 1]:
    j0 -= 1
    j1 -= 1
# merge order with 8-char markers throughout.
blk = ["<<<<<<<< HEAD"] + local[i:j0] + ["========"] + up[i:j1] + [">>>>>>>> branch"]
open(sys.argv[3], "w").write("\n".join(local[:i] + blk + local[j0:]))
PYEOF
run_in "$T101" expect_fail "extended-marker setup exits nonzero" "$PROJENY" setup w.projeny
if cmp -s "$T101/w.projeny" "$ROOT/t81-up.projeny"; then
    ok "extended markers still take upstream"
else
    fail "extended markers still take upstream" "$(cat "$T101/w.projeny")"
fi
expect_file_contains "extended-marker checkout has workdir markers" "$T101/w/a.c" "<<<<<<<"
expect_file_contains "extended-marker checkout records conflict" "$T101/.w.projeny.status" "Conflict: a.c"
# Nested markers cannot be split: refuse with guidance, never silently.
T101N="$ROOT/t101n"
mkdir -p "$T101N"
cp "$T81/w-1.0.tar.gz" "$T101N/"
cp "$ROOT/t81-local.projeny" "$T101N/w.projeny"
(cd "$T101N" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 - "$ROOT/t81-local.projeny" "$T101N/w.projeny" <<'PYEOF'
import sys
s = open(sys.argv[1]).read()
trap = "<<<<<<<< outer\nlocal line\n<<<<<<< inner\ninner ours\n=======\ninner theirs\n>>>>>>> inner\n========\nupstream line\n>>>>>>>> outer\n"
open(sys.argv[2], "w").write(s + trap)
PYEOF
run_in "$T101N" expect_fail "nested markers fail setup" "$PROJENY" setup w.projeny
out="$(cd "$T101N" && "$PROJENY" setup w.projeny 2>&1 || true)"
case "$out" in
*nested*)
    ok "nested-marker error names nesting"
    ;;
*)
    fail "nested-marker error names nesting" "out: $out"
    ;;
esac

# ----------------------------------------- 94. package/extract basics
# package = setup + tar of exactly the tracked files (committed patch plus
# uncommitted edits plus pending adds, minus pending rms and untracked
# files, like `git archive`). extract puts the same set into a directory.
TPKG="$ROOT/tpkg"
make_tarballs "$TPKG" pkg
write_projeny "$TPKG" pkg 1.0 pkg
(cd "$TPKG" && "$PROJENY" setup pkg.projeny >/dev/null 2>&1)
python3 - "$TPKG/pkg/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 10;")
open(p, "w").write(s)
EOF
(cd "$TPKG" && "$PROJENY" commit pkg.projeny >/dev/null 2>&1)
# uncommitted edit: must be included in package/extract output.
python3 - "$TPKG/pkg/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int gamma = 1;", "int gamma = 99;")
open(p, "w").write(s)
EOF
# untracked file: must be left out.
printf 'junk\n' > "$TPKG/pkg/UNTRACKED.txt"
# pending add: tracked, must be included.
printf 'added but uncommitted\n' > "$TPKG/pkg/src/new.c"
run_in "$TPKG" expect_ok "package fixture pending add" "$PROJENY" add pkg.projeny pkg/src/new.c
# pending rm: must be excluded.
run_in "$TPKG" expect_ok "package fixture pending rm" "$PROJENY" rm pkg.projeny pkg/README
run_in "$TPKG" expect_ok "package exits 0" "$PROJENY" package pkg.projeny pkg-out.tar.gz
(cd "$TPKG" && tar -tzf pkg-out.tar.gz | sort > pkg-members.txt)
if grep -qx "pkg-out/src/a.c" "$TPKG/pkg-members.txt" && grep -qx "pkg-out/src/new.c" "$TPKG/pkg-members.txt"; then
    ok "package holds tracked files"
else
    fail "package holds tracked files" "$(cat "$TPKG/pkg-members.txt")"
fi
if grep -q "README" "$TPKG/pkg-members.txt"; then
    fail "package omits pending rm" "$(cat "$TPKG/pkg-members.txt")"
else
    ok "package omits pending rm"
fi
if grep -q "UNTRACKED" "$TPKG/pkg-members.txt"; then
    fail "package omits untracked files" "$(cat "$TPKG/pkg-members.txt")"
else
    ok "package omits untracked files"
fi
mkdir -p "$TPKG/unpack" && (cd "$TPKG/unpack" && tar -xzf ../pkg-out.tar.gz)
expect_file_contains "package keeps committed edit" "$TPKG/unpack/pkg-out/src/a.c" "beta = 10"
expect_file_contains "package keeps uncommitted edit" "$TPKG/unpack/pkg-out/src/a.c" "gamma = 99"
run_in "$TPKG" expect_ok "extract exits 0" "$PROJENY" extract pkg.projeny extracted
if diff -r "$TPKG/unpack/pkg-out" "$TPKG/extracted" >/dev/null 2>&1; then
    ok "extract equals package payload"
else
    fail "extract equals package payload" "$(diff -r "$TPKG/unpack/pkg-out" "$TPKG/extracted" 2>&1 | head -10)"
fi
expect_file_contains "extract keeps uncommitted edit" "$TPKG/extracted/src/a.c" "gamma = 99"
if [ -e "$TPKG/extracted/UNTRACKED.txt" ]; then
    fail "extract omits untracked files" "$(ls "$TPKG/extracted")"
else
    ok "extract omits untracked files"
fi

# ----------------------------------------- 95. package formats, args, conflicts
TFMT="$ROOT/tfmt"
make_tarballs "$TFMT" pkg
write_projeny "$TFMT" pkg 1.0 pkg
(cd "$TFMT" && "$PROJENY" setup pkg.projeny >/dev/null 2>&1)
for ext in tar tgz tar.gz tar.bz2 tar.xz; do
    run_in "$TFMT" expect_ok "package .$ext exits 0" "$PROJENY" package pkg.projeny "o.$ext"
done
if command -v zstd >/dev/null 2>&1 && tar --help 2>/dev/null | grep -q -- --zstd; then
    run_in "$TFMT" expect_ok "package .tar.zst exits 0" "$PROJENY" package pkg.projeny o.tar.zst
    (cd "$TFMT" && tar --zstd -tf o.tar.zst | sort > m.zst)
else
    ok "zstd absent; .tar.zst checks skipped"
fi
(cd "$TFMT" && tar -tf o.tar | sort > m.tar)
(cd "$TFMT" && tar -tzf o.tgz | sort > m.tgz)
(cd "$TFMT" && tar -tzf o.tar.gz | sort > m.targz)
(cd "$TFMT" && tar -tjf o.tar.bz2 | sort > m.tbz2)
(cd "$TFMT" && tar -tJf o.tar.xz | sort > m.txz)
if cmp -s "$TFMT/m.tar" "$TFMT/m.tgz" && cmp -s "$TFMT/m.tar" "$TFMT/m.targz" && cmp -s "$TFMT/m.tar" "$TFMT/m.tbz2" && cmp -s "$TFMT/m.tar" "$TFMT/m.txz"; then
    ok "every compression holds the same members"
else
    fail "every compression holds the same members" "$(cat "$TFMT/m.tar" 2>&1)"
fi
if [ -f "$TFMT/m.zst" ]; then
    if cmp -s "$TFMT/m.tar" "$TFMT/m.zst"; then
        ok "zstd holds the same members"
    else
        fail "zstd holds the same members"
    fi
else
    ok "zstd absent; member comparison skipped"
fi
mkdir -p "$TFMT/u1" "$TFMT/u2" && (cd "$TFMT/u1" && tar -xf ../o.tar) && (cd "$TFMT/u2" && tar -xzf ../o.tar.gz)
if diff -r "$TFMT/u1" "$TFMT/u2" >/dev/null 2>&1; then
    ok "every compression holds the same payload"
else
    fail "every compression holds the same payload"
fi
# top-level dir matches the output name (package-source.sh $name prefix).
if grep -qx "o/src/a.c" "$TFMT/m.tar"; then
    ok "package top dir matches output name"
else
    fail "package top dir matches output name" "$(cat "$TFMT/m.tar")"
fi
run_in "$TFMT" expect_fail "package unknown extension fails" "$PROJENY" package pkg.projeny o.zip
if [ -e "$TFMT/o.zip" ]; then
    fail "failed package writes no archive" "$(ls "$TFMT")"
else
    ok "failed package writes no archive"
fi
# directory argument holding exactly one .projeny file.
mkdir -p "$TFMT/proj" && cp "$TFMT/pkg-1.0.tar.gz" "$TFMT/proj/" && cp "$TFMT/pkg.projeny" "$TFMT/proj/"
run_in "$TFMT" expect_ok "package accepts a project dir" "$PROJENY" package proj proj-out.tar.gz
if [ -f "$TFMT/proj-out.tar.gz" ]; then
    ok "project-dir package writes the archive"
else
    fail "project-dir package writes the archive"
fi
# workdir argument: resolves the "<dir>.projeny" sibling.
run_in "$TFMT" expect_ok "package accepts the workdir" "$PROJENY" package pkg pkg/pkg-out.tar.gz
if grep -q "pkg-out.tar.gz" "$TFMT/pkg/pkg-out.tar.gz" 2>/dev/null || (cd "$TFMT" && tar -tzf pkg/pkg-out.tar.gz | grep -q "pkg-out.tar.gz"); then
    fail "workdir package excludes itself" "$(cd "$TFMT" && tar -tzf pkg/pkg-out.tar.gz)"
else
    ok "workdir package excludes itself"
fi
# conflicting tree: setup fails, and package/extract refuse without output.
TCONF="$ROOT/tconf"
make_tarballs "$TCONF" fake
write_projeny "$TCONF" fake 1.0 fake
(cd "$TCONF" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$TCONF/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 10;")
open(p, "w").write(s)
EOF
(cd "$TCONF" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$TCONF/fake.projeny" "$ROOT/tconf-local.projeny"
rm -rf "$TCONF/fake" "$TCONF/.fake.projeny.status"
cp "$ROOT/tconf-local.projeny" "$TCONF/fake.projeny"
(cd "$TCONF" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$TCONF/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 999;")
open(p, "w").write(s)
EOF
mkdir -p "$ROOT/tconfup"
cp "$TCONF/fake-1.0.tar.gz" "$ROOT/tconfup/"
cp "$ROOT/tconf-local.projeny" "$ROOT/tconfup/fake.projeny"
(cd "$ROOT/tconfup" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$ROOT/tconfup/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 555;")
open(p, "w").write(s)
EOF
(cd "$ROOT/tconfup" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$ROOT/tconfup/fake.projeny" "$TCONF/fake.projeny"
run_in "$TCONF" expect_fail "conflicting setup exits nonzero (package fixture)" "$PROJENY" setup fake.projeny
run_in "$TCONF" expect_fail "package refuses conflicted tree" "$PROJENY" package fake.projeny c.tar.gz
if [ -e "$TCONF/c.tar.gz" ]; then
    fail "refused package writes no archive" "$(ls "$TCONF")"
else
    ok "refused package writes no archive"
fi
run_in "$TCONF" expect_fail "extract refuses conflicted tree" "$PROJENY" extract fake.projeny cdir
if [ -e "$TCONF/cdir" ]; then
    fail "refused extract writes no directory" "$(ls "$TCONF")"
else
    ok "refused extract writes no directory"
fi
# repair and the commands succeed again.
printf 'int alpha = 1;\n\nint beta = 777;\n\nint gamma = 1;\n\nint delta = 1;\n' > "$TCONF/fake/src/a.c"
run_in "$TCONF" expect_ok "resolve clears package-fixture conflict" "$PROJENY" resolve fake.projeny fake/src/a.c
run_in "$TCONF" expect_ok "commit stores package-fixture fix" "$PROJENY" commit fake.projeny
run_in "$TCONF" expect_ok "package works after resolve+commit" "$PROJENY" package fake.projeny c.tar.gz
run_in "$TCONF" expect_ok "extract works after resolve+commit" "$PROJENY" extract fake.projeny cdir
expect_file_contains "repaired package holds the fix" "$TCONF/cdir/src/a.c" "beta = 777"

# ----------------------------------------- 96. extract/package edge cases
TEDGE="$ROOT/tedge"
make_tarballs "$TEDGE" pkg
write_projeny "$TEDGE" pkg 1.0 pkg
(cd "$TEDGE" && "$PROJENY" setup pkg.projeny >/dev/null 2>&1)
mkdir -p "$TEDGE/nonempty" && printf 'x\n' > "$TEDGE/nonempty/f"
run_in "$TEDGE" expect_fail "extract refuses non-empty dest" "$PROJENY" extract pkg.projeny nonempty
printf 'x\n' > "$TEDGE/afile"
run_in "$TEDGE" expect_fail "extract refuses non-directory dest" "$PROJENY" extract pkg.projeny afile
run_in "$TEDGE" expect_ok "extract creates nested dest" "$PROJENY" extract pkg.projeny a/b/c
expect_file_contains "nested extract lands files" "$TEDGE/a/b/c/src/a.c" "alpha = 1"
run_in "$TEDGE" expect_ok "package creates missing parents" "$PROJENY" package pkg.projeny sub/dir/o.tar.gz
if [ -f "$TEDGE/sub/dir/o.tar.gz" ]; then
    ok "package writes through missing parents"
else
    fail "package writes through missing parents"
fi
# pending rename: new name in, old name out.
run_in "$TEDGE" expect_ok "edge mv records" "$PROJENY" mv pkg.projeny pkg/src/a.c pkg/src/renamed.c
run_in "$TEDGE" expect_ok "package after mv exits 0" "$PROJENY" package pkg.projeny mv.tar.gz
(cd "$TEDGE" && tar -tzf mv.tar.gz | sort > mv-members.txt)
if grep -qx "mv/src/renamed.c" "$TEDGE/mv-members.txt"; then
    ok "package holds the renamed file"
else
    fail "package holds the renamed file" "$(cat "$TEDGE/mv-members.txt")"
fi
if grep -qx "mv/src/a.c" "$TEDGE/mv-members.txt"; then
    fail "package drops the old name" "$(cat "$TEDGE/mv-members.txt")"
else
    ok "package drops the old name"
fi
# CLI arity and missing inputs.
run_in "$TEDGE" expect_fail "package with missing args fails" "$PROJENY" package pkg.projeny
run_in "$TEDGE" expect_fail "package with extra args fails" "$PROJENY" package pkg.projeny o.tar.gz extra
run_in "$TEDGE" expect_fail "extract with missing args fails" "$PROJENY" extract pkg.projeny
run_in "$TEDGE" expect_fail "extract with extra args fails" "$PROJENY" extract pkg.projeny d extra
run_in "$TEDGE" expect_fail "package of missing project fails" "$PROJENY" package gone.projeny o.tar.gz
mkdir -p "$TEDGE/emptyproj"
run_in "$TEDGE" expect_fail "package of empty dir fails" "$PROJENY" package emptyproj o2.tar.gz
if [ -e "$TEDGE/o2.tar.gz" ]; then
    fail "failed package writes no archive" "$(ls "$TEDGE")"
else
    ok "failed package writes no archive"
fi

# ----------------------------------------- 97. trailing blank lines roundtrip
# Files ending in one or more blank lines must survive commit->setup exactly
# (new files and modifications alike); blank lines are real content.
TNL="$ROOT/tnl"
mkdir -p "$TNL/w-1.0"
printf 'seed\n' > "$TNL/w-1.0/s.txt"
(cd "$TNL" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Blanks.\n' > "$TNL/w.projeny"
(cd "$TNL" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'a\n\n\n' > "$TNL/w/blanks.txt"
printf 'b\n\n' > "$TNL/w/one.txt"
printf 'seed\n\n' > "$TNL/w/s.txt"
run_in "$TNL" expect_ok "add blanks file" "$PROJENY" add w.projeny w/blanks.txt
run_in "$TNL" expect_ok "add one-blank file" "$PROJENY" add w.projeny w/one.txt
(cd "$TNL" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
printf 'a\n\n\n' > "$TNL/expect-blanks.txt"
printf 'b\n\n' > "$TNL/expect-one.txt"
printf 'seed\n\n' > "$TNL/expect-s.txt"
rm -rf "$TNL/w" "$TNL/.w.projeny.status"
(cd "$TNL" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
if cmp -s "$TNL/w/blanks.txt" "$TNL/expect-blanks.txt"; then
    ok "new file keeps two trailing blank lines"
else
    fail "new file keeps two trailing blank lines" "$(xxd "$TNL/w/blanks.txt" | tail -2)"
fi
if cmp -s "$TNL/w/one.txt" "$TNL/expect-one.txt"; then
    ok "new file keeps one trailing blank line"
else
    fail "new file keeps one trailing blank line" "$(xxd "$TNL/w/one.txt" | tail -2)"
fi
if cmp -s "$TNL/w/s.txt" "$TNL/expect-s.txt"; then
    ok "modified file keeps appended blank line"
else
    fail "modified file keeps appended blank line" "$(xxd "$TNL/w/s.txt" | tail -2)"
fi

# ----------------------------------------- 98. binaries ride along, tracked ones commit
# Untracked binaries (like a package tarball written into the workdir) must
# survive setup/package/extract/commit: preserved on disk, left out of
# patches and archives until explicitly added. Tracked binaries and
# explicitly added ones commit as base64 binary blocks.
TBIN="$ROOT/tbin"
make_tarballs "$TBIN" pkg
write_projeny "$TBIN" pkg 1.0 pkg
(cd "$TBIN" && "$PROJENY" setup pkg.projeny >/dev/null 2>&1)
python3 -c "open('$TBIN/pkg/blob.bin','wb').write(b'\x00\x01\x02\n')"
printf 'untracked text\n' > "$TBIN/pkg/NOTE.txt"
run_in "$TBIN" expect_ok "package with binaries present exits 0" "$PROJENY" package pkg.projeny pkg/out.tar.gz
(cd "$TBIN" && tar -tzf pkg/out.tar.gz | sort > bin-members.txt)
if grep -q "blob.bin\|out.tar.gz\|NOTE" "$TBIN/bin-members.txt"; then
    fail "package leaves all untracked files out" "$(cat "$TBIN/bin-members.txt")"
else
    ok "package leaves all untracked files out"
fi
# Second packaging run: the first run's tarball (an untracked binary sitting
# in the workdir) must neither break setup nor sneak into the new archive.
run_in "$TBIN" expect_ok "second package exits 0" "$PROJENY" package pkg.projeny pkg/out2.tar.gz
(cd "$TBIN" && tar -tzf pkg/out2.tar.gz | sort > bin-members2.txt)
if grep -q "blob.bin\|out.tar.gz\|out2.tar.gz\|NOTE" "$TBIN/bin-members2.txt"; then
    fail "repackage leaves binaries out" "$(cat "$TBIN/bin-members2.txt")"
else
    ok "repackage leaves binaries out"
fi
if [ -f "$TBIN/pkg/blob.bin" ] && [ -f "$TBIN/pkg/out.tar.gz" ]; then
    ok "setups preserve untracked binaries on disk"
else
    fail "setups preserve untracked binaries on disk" "$(ls "$TBIN/pkg")"
fi
run_in "$TBIN" expect_ok "commit ignores binaries" "$PROJENY" commit pkg.projeny
if grep -q "blob.bin" "$TBIN/pkg.projeny" || grep -q "out.tar.gz" "$TBIN/pkg.projeny" || grep -q "out2.tar.gz" "$TBIN/pkg.projeny"; then
    fail "commit leaves binaries out of the patch" "$(grep "blob.bin\|tar.gz" "$TBIN/pkg.projeny")"
else
    ok "commit leaves binaries out of the patch"
fi
if grep -q "NOTE" "$TBIN/pkg.projeny"; then
    fail "commit leaves untracked text out of the patch"
else
    ok "commit leaves untracked text out of the patch"
fi
run_in "$TBIN" expect_ok "add untracked text" "$PROJENY" add pkg.projeny pkg/NOTE.txt
run_in "$TBIN" expect_ok "commit of added text succeeds" "$PROJENY" commit pkg.projeny
expect_file_contains "added text is stored" "$TBIN/pkg.projeny" "NOTE"
run_in "$TBIN" expect_ok "setup after binary commit" "$PROJENY" setup pkg.projeny
if [ -f "$TBIN/pkg/blob.bin" ] && [ -f "$TBIN/pkg/NOTE.txt" ]; then
    ok "post-commit setup preserves untracked files"
else
    fail "post-commit setup preserves untracked files" "$(ls "$TBIN/pkg")"
fi
# Explicit binary intent is tracked: adding an untracked binary commits it.
run_in "$TBIN" expect_ok "add of binary succeeds" "$PROJENY" add pkg.projeny pkg/blob.bin
run_in "$TBIN" expect_ok "commit of added binary succeeds" "$PROJENY" commit pkg.projeny
expect_file_contains "added binary stored" "$TBIN/pkg.projeny" "blob.bin"
python3 -c "open('$ROOT/tbin-expect.bin','wb').write(open('$TBIN/pkg/blob.bin','rb').read())"
rm -rf "$TBIN/pkg" "$TBIN/.pkg.projeny.status"
run_in "$TBIN" expect_ok "setup after binary add commit" "$PROJENY" setup pkg.projeny
if cmp -s "$TBIN/pkg/blob.bin" "$ROOT/tbin-expect.bin"; then
    ok "added binary byte-identical after re-setup"
else
    fail "added binary byte-identical after re-setup"
fi
# Tracked binaries: edits commit as binary blocks and merge cleanly through
# setup; explicit rm deletes and commits as a binary delete.
TBIN2="$ROOT/tbin2"
mkdir -p "$TBIN2/b-1.0"
python3 -c "open('$TBIN2/b-1.0/b.dat','wb').write(b'a\x00b\n')"
printf 'ok\n' > "$TBIN2/b-1.0/f.c"
(cd "$TBIN2" && tar -czf b-1.0.tar.gz b-1.0 && rm -rf b-1.0)
printf 'Archive: b-1.0.tar.gz\nOrigname: b-1.0\nName: b\n\n    Tracked binary.\n' > "$TBIN2/b.projeny"
(cd "$TBIN2" && "$PROJENY" setup b.projeny >/dev/null 2>&1)
python3 -c "open('$TBIN2/b/b.dat','wb').write(b'a\x00CHANGED\n')"
run_in "$TBIN2" expect_ok "commit of edited tracked binary succeeds" "$PROJENY" commit b.projeny
run_in "$TBIN2" expect_ok "setup keeps committed binary edit" "$PROJENY" setup b.projeny
python3 -c "import sys; sys.exit(0 if open('$TBIN2/b/b.dat','rb').read()==b'a\x00CHANGED\n' else 1)"
if [ $? -eq 0 ]; then
    ok "committed binary edit survives setup"
else
    fail "committed binary edit survives setup"
fi
run_in "$TBIN2" expect_ok "rm of tracked binary succeeds" "$PROJENY" rm b.projeny b/b.dat
if [ ! -f "$TBIN2/b/b.dat" ]; then
    ok "rm deletes the tracked binary"
else
    fail "rm deletes the tracked binary"
fi
run_in "$TBIN2" expect_ok "commit of tracked binary delete succeeds" "$PROJENY" commit b.projeny
rm -rf "$TBIN2/b" "$TBIN2/.b.projeny.status"
run_in "$TBIN2" expect_ok "setup after tracked binary delete" "$PROJENY" setup b.projeny
if [ ! -f "$TBIN2/b/b.dat" ]; then
    ok "tracked binary delete survives re-setup"
else
    fail "tracked binary delete survives re-setup"
fi

# ----------------------------------------- 99. package/extract keep +x
# Regression test: an executable shipped by the base tarball (never touched
# by the patch, like libffi's configure) must stay executable through
# setup, package (tar member mode), and extract (on-disk mode).
TXBIT="$ROOT/txbit"
mkdir -p "$TXBIT/x-1.0"
printf '#!/bin/sh\necho hi\n' > "$TXBIT/x-1.0/configure"
chmod 755 "$TXBIT/x-1.0/configure"
printf 'plain\n' > "$TXBIT/x-1.0/README"
(cd "$TXBIT" && tar -czf x-1.0.tar.gz x-1.0 && rm -rf x-1.0)
printf 'Archive: x-1.0.tar.gz\nOrigname: x-1.0\nName: x\n\n    Exec fixture.\n' > "$TXBIT/x.projeny"
run_in "$TXBIT" expect_ok "exec setup" "$PROJENY" setup x.projeny
if [ -x "$TXBIT/x/configure" ]; then
    ok "setup keeps base +x"
else
    fail "setup keeps base +x" "$(ls -l "$TXBIT/x/configure" 2>&1)"
fi
run_in "$TXBIT" expect_ok "exec package" "$PROJENY" package x.projeny x-out.tar.gz
if tar -tvzf "$TXBIT/x-out.tar.gz" | grep -q "^-rwx.*configure"; then
    ok "package tar keeps +x"
else
    fail "package tar keeps +x" "$(tar -tvzf "$TXBIT/x-out.tar.gz" 2>&1)"
fi
run_in "$TXBIT" expect_ok "exec extract" "$PROJENY" extract x.projeny x-dest
if [ -x "$TXBIT/x-dest/configure" ]; then
    ok "extract keeps +x"
else
    fail "extract keeps +x" "$(ls -l "$TXBIT/x-dest/configure" 2>&1)"
fi
if [ -x "$TXBIT/x-dest/configure" ] && [ ! -x "$TXBIT/x-dest/README" ]; then
    ok "extract keeps non-exec non-exec"
else
    fail "extract keeps non-exec non-exec" "$(ls -l "$TXBIT/x-dest" 2>&1)"
fi
# Content-only edit of the executable plus commit must still package +x.
printf '#!/bin/sh\necho yo\n' > "$TXBIT/x/configure"
chmod 755 "$TXBIT/x/configure"
run_in "$TXBIT" expect_ok "exec content commit" "$PROJENY" commit x.projeny
rm -rf "$TXBIT/x-dest" "$TXBIT/x-out.tar.gz"
run_in "$TXBIT" expect_ok "repackage after content edit" "$PROJENY" package x.projeny x-out.tar.gz
if tar -tvzf "$TXBIT/x-out.tar.gz" | grep -q "^-rwx.*configure"; then
    ok "repackage keeps +x after content edit"
else
    fail "repackage keeps +x after content edit" "$(tar -tvzf "$TXBIT/x-out.tar.gz" 2>&1)"
fi
run_in "$TXBIT" expect_ok "re-extract after content edit" "$PROJENY" extract x.projeny x-dest2
if [ -x "$TXBIT/x-dest2/configure" ]; then
    ok "re-extract keeps +x after content edit"
else
    fail "re-extract keeps +x after content edit" "$(ls -l "$TXBIT/x-dest2/configure" 2>&1)"
fi

# ----------------------------------------- 100. clean merge keeps +x
# Regression test: a genuine three-way line merge of an executable (adjacent
# local/upstream edits, so the local hunk cannot apply directly and the
# merger rewrites the file) must keep +x. Note ours aliases dst in every
# in-place merge, so the exec vote has to be sampled before the write.
TMERGE="$ROOT/tmerge"
mkdir -p "$TMERGE/m-1.0"
cat > "$TMERGE/m-1.0/run.sh" <<'EOF'
#!/bin/sh
echo one
echo two
echo three
echo four
echo five
EOF
chmod 755 "$TMERGE/m-1.0/run.sh"
printf 'hello v1\n' > "$TMERGE/m-1.0/README"
(cd "$TMERGE" && tar -czf m-1.0.tar.gz m-1.0 && rm -rf m-1.0)
printf 'Archive: m-1.0.tar.gz\nOrigname: m-1.0\nName: m\n\n    Merge exec fixture.\n' > "$TMERGE/m.projeny"
run_in "$TMERGE" expect_ok "merge-exec setup" "$PROJENY" setup m.projeny
sed -i 's/echo two/echo TWO/' "$TMERGE/m/run.sh"
chmod 755 "$TMERGE/m/run.sh"
mkdir -p "$TMERGE/up"
cp "$TMERGE/m-1.0.tar.gz" "$TMERGE/m.projeny" "$TMERGE/up/"
(cd "$TMERGE/up" && "$PROJENY" setup m.projeny >/dev/null 2>&1)
sed -i 's/echo three/echo THREE/' "$TMERGE/up/m/run.sh"
chmod 755 "$TMERGE/up/m/run.sh"
(cd "$TMERGE/up" && "$PROJENY" commit m.projeny >/dev/null 2>&1)
cp "$TMERGE/up/m.projeny" "$TMERGE/m.projeny"
run_in "$TMERGE" expect_ok "adjacent-edit merge exits 0" "$PROJENY" setup m.projeny
expect_file_contains "merge keeps local edit" "$TMERGE/m/run.sh" "echo TWO"
expect_file_contains "merge keeps upstream edit" "$TMERGE/m/run.sh" "echo THREE"
if [ -x "$TMERGE/m/run.sh" ]; then
    ok "clean merge keeps +x"
else
    fail "clean merge keeps +x" "$(ls -l "$TMERGE/m/run.sh" 2>&1)"
fi

# ----------------------------------------- 101. all execs + modes via package/extract
# libffi-style: many executables (configure, config.guess, ...) plus
# restrictive modes (0700 owner-only, 0640 group-read). Every +x member of
# the base tarball must stay +x through setup, package (tar member mode),
# and extract (on-disk mode); rw bits must not widen (0700 stays 700, 0640
# stays 640); setuid must never leak into package/extract payloads.
TMODE="$ROOT/tmode"
mkdir -p "$TMODE/y-1.0"
for f in configure config.guess config.sub missing compile depcomp install-sh ltmain.sh mdate-sh texinfo.tex; do
    printf '#!/bin/sh\necho %s\n' "$f" > "$TMODE/y-1.0/$f"
    chmod 755 "$TMODE/y-1.0/$f"
done
printf '#!/bin/sh\necho private\n' > "$TMODE/y-1.0/tool-700"
chmod 700 "$TMODE/y-1.0/tool-700"
printf 'group data\n' > "$TMODE/y-1.0/data-640"
chmod 640 "$TMODE/y-1.0/data-640"
printf 'plain\n' > "$TMODE/y-1.0/README"
(cd "$TMODE" && tar -czf y-1.0.tar.gz y-1.0 && rm -rf y-1.0)
printf 'Archive: y-1.0.tar.gz\nOrigname: y-1.0\nName: y\n\n    Mode fixture.\n' > "$TMODE/y.projeny"
run_in "$TMODE" expect_ok "mode setup" "$PROJENY" setup y.projeny
# Enumerate every executable member shipped by the base tarball (libffi
# ships ~21) instead of asserting a single file.
BASE_EXECS=$(cd "$TMODE" && tar -tzvf y-1.0.tar.gz | awk '$1 ~ /^-..x/ {print $NF}' | sed 's|^y-1.0/||')
NEXEC=0
for f in $BASE_EXECS; do
    [ -n "$f" ] || continue
    NEXEC=$((NEXEC + 1))
    if [ -x "$TMODE/y/$f" ]; then
        ok "setup keeps +x on $f"
    else
        fail "setup keeps +x on $f" "$(ls -l "$TMODE/y/$f" 2>&1)"
    fi
done
if [ "$NEXEC" -ge 10 ]; then
    ok "base tarball ships all executables ($NEXEC)"
else
    fail "base tarball ships all executables" "found $NEXEC: $BASE_EXECS"
fi
if [ "$(stat -c %a "$TMODE/y/tool-700")" = "700" ]; then
    ok "setup keeps 0700 exact"
else
    fail "setup keeps 0700 exact" "$(stat -c %a "$TMODE/y/tool-700" 2>&1)"
fi
if [ "$(stat -c %a "$TMODE/y/data-640")" = "640" ]; then
    ok "setup keeps 0640 exact"
else
    fail "setup keeps 0640 exact" "$(stat -c %a "$TMODE/y/data-640" 2>&1)"
fi
if [ ! -x "$TMODE/y/README" ] && [ ! -x "$TMODE/y/data-640" ]; then
    ok "setup keeps non-exec non-exec"
else
    fail "setup keeps non-exec non-exec" "$(ls -l "$TMODE/y" 2>&1)"
fi
run_in "$TMODE" expect_ok "mode package" "$PROJENY" package y.projeny y-out.tar.gz
for f in $BASE_EXECS; do
    [ -n "$f" ] || continue
    if tar -tzvf "$TMODE/y-out.tar.gz" | grep -F "/$f" | grep -q "^-..x"; then
        ok "package tar keeps +x on $f"
    else
        fail "package tar keeps +x on $f" "$(tar -tzvf "$TMODE/y-out.tar.gz" | grep -F "/$f" 2>&1)"
    fi
done
if tar -tzvf "$TMODE/y-out.tar.gz" | grep -F "/tool-700" | grep -q "^-rwx------"; then
    ok "package tar keeps 0700 exact"
else
    fail "package tar keeps 0700 exact" "$(tar -tzvf "$TMODE/y-out.tar.gz" | grep -F "/tool-700" 2>&1)"
fi
if tar -tzvf "$TMODE/y-out.tar.gz" | grep -F "/data-640" | grep -q "^-rw-r-----"; then
    ok "package tar keeps 0640 exact"
else
    fail "package tar keeps 0640 exact" "$(tar -tzvf "$TMODE/y-out.tar.gz" | grep -F "/data-640" 2>&1)"
fi
run_in "$TMODE" expect_ok "mode extract" "$PROJENY" extract y.projeny y-dest
for f in $BASE_EXECS; do
    [ -n "$f" ] || continue
    if [ -x "$TMODE/y-dest/$f" ]; then
        ok "extract keeps +x on $f"
    else
        fail "extract keeps +x on $f" "$(ls -l "$TMODE/y-dest/$f" 2>&1)"
    fi
done
if [ "$(stat -c %a "$TMODE/y-dest/tool-700")" = "700" ]; then
    ok "extract keeps 0700 exact"
else
    fail "extract keeps 0700 exact" "$(stat -c %a "$TMODE/y-dest/tool-700" 2>&1)"
fi
if [ "$(stat -c %a "$TMODE/y-dest/data-640")" = "640" ]; then
    ok "extract keeps 0640 exact"
else
    fail "extract keeps 0640 exact" "$(stat -c %a "$TMODE/y-dest/data-640" 2>&1)"
fi
if [ ! -x "$TMODE/y-dest/README" ] && [ ! -x "$TMODE/y-dest/data-640" ]; then
    ok "extract keeps non-exec non-exec"
else
    fail "extract keeps non-exec non-exec" "$(ls -l "$TMODE/y-dest" 2>&1)"
fi
# setuid/setgid/sticky must never leak into staged payloads (staged copies
# mask 0777). setup normalizes workdir modes first, so this is defense in
# depth: the payload must still be clean even for a pending-added suid file.
printf '#!/bin/sh\necho suid\n' > "$TMODE/y/suid-tool"
chmod 4755 "$TMODE/y/suid-tool"
run_in "$TMODE" expect_ok "suid add" "$PROJENY" add y.projeny y/suid-tool
run_in "$TMODE" expect_ok "suid package" "$PROJENY" package y.projeny y-suid.tar.gz
if tar -tzvf "$TMODE/y-suid.tar.gz" | grep -F "/suid-tool" | grep -Eq "^[^ ]*[sST]"; then
    fail "package strips setuid" "$(tar -tzvf "$TMODE/y-suid.tar.gz" | grep -F "/suid-tool" 2>&1)"
else
    ok "package strips setuid"
fi
run_in "$TMODE" expect_ok "suid extract" "$PROJENY" extract y.projeny y-suid-dest
if [ -u "$TMODE/y-suid-dest/suid-tool" ]; then
    fail "extract strips setuid" "$(ls -l "$TMODE/y-suid-dest/suid-tool" 2>&1)"
else
    ok "extract strips setuid"
fi

# ----------------------------------------- 102. conflict merge keeps +x
# Same-line local/upstream edits to an executable force conflict markers
# (the early and late merge-conflict write paths); the result must stay
# executable with its restrictive mode intact (0700, not widened to 0755).
TCONF="$ROOT/tconf"
mkdir -p "$TCONF/c-1.0"
cat > "$TCONF/c-1.0/run.sh" <<'EOF'
#!/bin/sh
echo one
echo two
echo three
echo four
echo five
EOF
chmod 700 "$TCONF/c-1.0/run.sh"
printf 'hello v1\n' > "$TCONF/c-1.0/README"
(cd "$TCONF" && tar -czf c-1.0.tar.gz c-1.0 && rm -rf c-1.0)
printf 'Archive: c-1.0.tar.gz\nOrigname: c-1.0\nName: c\n\n    Conflict exec fixture.\n' > "$TCONF/c.projeny"
run_in "$TCONF" expect_ok "conflict-exec setup" "$PROJENY" setup c.projeny
sed -i 's/echo two/echo LOCAL/' "$TCONF/c/run.sh"
UCONF="$ROOT/tconfup"
mkdir -p "$UCONF"
cp "$TCONF/c-1.0.tar.gz" "$TCONF/c.projeny" "$UCONF/"
(cd "$UCONF" && "$PROJENY" setup c.projeny >/dev/null 2>&1)
sed -i 's/echo two/echo UPSTREAM/' "$UCONF/c/run.sh"
(cd "$UCONF" && "$PROJENY" commit c.projeny >/dev/null 2>&1)
cp "$UCONF/c.projeny" "$TCONF/c.projeny"
run_in "$TCONF" expect_fail "conflicting merge exits nonzero" "$PROJENY" setup c.projeny
expect_file_contains "conflict leaves markers" "$TCONF/c/run.sh" "<<<<<<<"
if [ -x "$TCONF/c/run.sh" ]; then
    ok "conflict merge keeps +x"
else
    fail "conflict merge keeps +x" "$(ls -l "$TCONF/c/run.sh" 2>&1)"
fi
if [ "$(stat -c %a "$TCONF/c/run.sh")" = "700" ]; then
    ok "conflict merge keeps 0700 exact"
else
    fail "conflict merge keeps 0700 exact" "$(stat -c %a "$TCONF/c/run.sh" 2>&1)"
fi

# ----------------------------------------- 103. add/add conflict keeps +x
# File absent from base, added differently on both sides with +x: the
# new-file conflict path must also restore the executable bit.
TADD="$ROOT/tadd"
mkdir -p "$TADD/d-1.0"
printf 'hello v1\n' > "$TADD/d-1.0/README"
(cd "$TADD" && tar -czf d-1.0.tar.gz d-1.0 && rm -rf d-1.0)
printf 'Archive: d-1.0.tar.gz\nOrigname: d-1.0\nName: d\n\n    Add-add fixture.\n' > "$TADD/d.projeny"
run_in "$TADD" expect_ok "add-add setup" "$PROJENY" setup d.projeny
printf '#!/bin/sh\necho local\n' > "$TADD/d/newtool.sh"
chmod 755 "$TADD/d/newtool.sh"
run_in "$TADD" expect_ok "add local newtool" "$PROJENY" add d.projeny d/newtool.sh
UADD="$ROOT/taddup"
mkdir -p "$UADD"
cp "$TADD/d-1.0.tar.gz" "$TADD/d.projeny" "$UADD/"
(cd "$UADD" && "$PROJENY" setup d.projeny >/dev/null 2>&1)
printf '#!/bin/sh\necho upstream\n' > "$UADD/d/newtool.sh"
chmod 755 "$UADD/d/newtool.sh"
run_in "$UADD" expect_ok "add upstream newtool" "$PROJENY" add d.projeny d/newtool.sh
(cd "$UADD" && "$PROJENY" commit d.projeny >/dev/null 2>&1)
cp "$UADD/d.projeny" "$TADD/d.projeny"
run_in "$TADD" expect_fail "add-add merge exits nonzero" "$PROJENY" setup d.projeny
expect_file_contains "add-add leaves markers" "$TADD/d/newtool.sh" "<<<<<<<"
if [ -x "$TADD/d/newtool.sh" ]; then
    ok "add-add conflict keeps +x"
else
    fail "add-add conflict keeps +x" "$(ls -l "$TADD/d/newtool.sh" 2>&1)"
fi

# ----------------------------------------- 104. setup restores deleted text files
# A file deleted without `projeny rm` (stray rm, build artifact cleanup) is
# accidental loss: the next setup restores it to fresh-setup state instead
# of treating the deletion as an intended change.
T104="$ROOT/t104"
make_tarballs "$T104" fake
write_projeny "$T104" fake 1.0 fake
(cd "$T104" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
cp "$T104/fake/src/a.c" "$ROOT/t104-expect-a.c"
rm "$T104/fake/src/a.c"
run_in "$T104" expect_ok "setup restores deleted text file" "$PROJENY" setup fake.projeny
if cmp -s "$T104/fake/src/a.c" "$ROOT/t104-expect-a.c"; then
    ok "deleted text file byte-identical after setup"
else
    fail "deleted text file byte-identical after setup"
fi
run_in "$T104" expect_ok "second setup is a noop" "$PROJENY" setup fake.projeny

# ----------------------------------------- 105. setup restores deleted binaries
# The libffi case: doc/libffi.info (NUL-bearing) is deleted by a make bug;
# the next setup must restore it byte-identical without any "binary files
# are not supported" error, and a second setup must keep working.
T105="$ROOT/t105"
mkdir -p "$T105/w-1.0/doc"
python3 -c "open('$T105/w-1.0/doc/libffi.info','wb').write(b'This is libffi.info \x00 binary \x00 tail\n' * 100)"
python3 -c "open('$T105/w-1.0/doc/libffi.pdf','wb').write(bytes(range(256)) * 100)"
printf 'ok\n' > "$T105/w-1.0/f.c"
(cd "$T105" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Libffi-like.\n' > "$T105/w.projeny"
(cd "$T105" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$ROOT/t105-expect.info','wb').write(open('$T105/w/doc/libffi.info','rb').read())"
python3 -c "open('$ROOT/t105-expect.pdf','wb').write(open('$T105/w/doc/libffi.pdf','rb').read())"
rm "$T105/w/doc/libffi.info"
out="$(cd "$T105" && "$PROJENY" setup w.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup after binary delete exits 0"
else
    fail "setup after binary delete exits 0" "exit=$rc out: $out"
fi
case "$out" in
*inary*)
    fail "no binary error on restore" "out: $out"
    ;;
*)
    ok "no binary error on restore"
    ;;
esac
if cmp -s "$T105/w/doc/libffi.info" "$ROOT/t105-expect.info"; then
    ok "deleted binary byte-identical after setup"
else
    fail "deleted binary byte-identical after setup"
fi
rm "$T105/w/doc/libffi.info" "$T105/w/doc/libffi.pdf"
run_in "$T105" expect_ok "setup restores two deleted binaries" "$PROJENY" setup w.projeny
if cmp -s "$T105/w/doc/libffi.info" "$ROOT/t105-expect.info" && cmp -s "$T105/w/doc/libffi.pdf" "$ROOT/t105-expect.pdf"; then
    ok "both binaries byte-identical after second setup"
else
    fail "both binaries byte-identical after second setup"
fi

# ----------------------------------------- 106. explicit rm survives setup
# Deletions recorded with `projeny rm` are intent: setup must re-apply them
# to the new tree (not restore the file), and the pending removal stays
# listed until committed.
T106="$ROOT/t106"
make_tarballs "$T106" fake
write_projeny "$T106" fake 1.0 fake
(cd "$T106" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T106" expect_ok "rm a tracked file" "$PROJENY" rm fake.projeny fake/README
run_in "$T106" expect_ok "setup keeps explicit removal" "$PROJENY" setup fake.projeny
if [ ! -f "$T106/fake/README" ]; then
    ok "explicitly removed file stays deleted across setup"
else
    fail "explicitly removed file stays deleted across setup"
fi
expect_file_contains "pending removal kept across setup" "$T106/.fake.projeny.status" "Removed: README"
run_in "$T106" expect_ok "commit records explicit removal" "$PROJENY" commit fake.projeny
rm -rf "$T106/fake" "$T106/.fake.projeny.status"
run_in "$T106" expect_ok "setup after removal commit" "$PROJENY" setup fake.projeny
if [ ! -f "$T106/fake/README" ]; then
    ok "committed removal survives re-setup"
else
    fail "committed removal survives re-setup"
fi

# ----------------------------------------- 107. setup merges edits and restores deletes
# One setup must do both: keep an uncommitted text edit and restore an
# accidentally deleted (binary and text) file.
T107="$ROOT/t107"
mkdir -p "$T107/w-1.0"
python3 -c "open('$T107/w-1.0/b.dat','wb').write(b'v\x00one\n')"
printf 'line one\nline two\nline three\nline four\n' > "$T107/w-1.0/f.c"
printf 'keep me\n' > "$T107/w-1.0/g.c"
(cd "$T107" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Mixed.\n' > "$T107/w.projeny"
(cd "$T107" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$ROOT/t107-expect.dat','wb').write(open('$T107/w/b.dat','rb').read())"
printf 'line ONE\nline two\nline three\nline four\n' > "$T107/w/f.c"
rm "$T107/w/g.c" "$T107/w/b.dat"
run_in "$T107" expect_ok "setup merges edit and restores deletes" "$PROJENY" setup w.projeny
expect_file_contains "uncommitted edit preserved" "$T107/w/f.c" "line ONE"
if cmp -s "$T107/w/b.dat" "$ROOT/t107-expect.dat"; then
    ok "deleted binary restored alongside edit"
else
    fail "deleted binary restored alongside edit"
fi
expect_file_contains "deleted text file restored alongside edit" "$T107/w/g.c" "keep me"

# ----------------------------------------- 108. setup restores deleted patched files patched
# When the patch itself modifies a file, deleting that file must restore the
# PATCHED content (tarball + patch), not the raw tarball bytes.
T108="$ROOT/t108"
make_tarballs "$T108" fake
write_projeny "$T108" fake 1.0 fake
(cd "$T108" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'patched beta\n' >> "$T108/fake/src/a.c"
(cd "$T108" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T108/fake/src/a.c" "$ROOT/t108-expect-a.c"
rm "$T108/fake/src/a.c"
run_in "$T108" expect_ok "setup restores deleted patched file" "$PROJENY" setup fake.projeny
if cmp -s "$T108/fake/src/a.c" "$ROOT/t108-expect-a.c"; then
    ok "restored file carries the patch content"
else
    fail "restored file carries the patch content"
fi

# ----------------------------------------- 109. binary rename roundtrips
# `projeny mv` of a binary records a rename; commit stores it without a
# payload and fresh setup reproduces the bytes at the new path.
T109="$ROOT/t109"
mkdir -p "$T109/w-1.0"
python3 -c "open('$T109/w-1.0/b.dat','wb').write(b'm\x00ove me\n' * 50)"
printf 'ok\n' > "$T109/w-1.0/f.c"
(cd "$T109" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Mv.\n' > "$T109/w.projeny"
(cd "$T109" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$ROOT/t109-expect.bin','wb').write(open('$T109/w/b.dat','rb').read())"
run_in "$T109" expect_ok "mv a binary" "$PROJENY" mv w.projeny w/b.dat w/sub/moved.dat
run_in "$T109" expect_ok "commit binary rename" "$PROJENY" commit w.projeny
expect_file_contains "rename recorded in patch" "$T109/w.projeny" "rename from"
rm -rf "$T109/w" "$T109/.w.projeny.status"
run_in "$T109" expect_ok "setup after binary rename" "$PROJENY" setup w.projeny
if [ ! -f "$T109/w/b.dat" ] && cmp -s "$T109/w/sub/moved.dat" "$ROOT/t109-expect.bin"; then
    ok "binary rename byte-identical at new path"
else
    fail "binary rename byte-identical at new path" "$(ls -R "$T109/w" 2>&1)"
fi

# ----------------------------------------- 110. binary mode changes roundtrip
# Chmod +x on a binary commits as a mode-only change and survives setup.
T110="$ROOT/t110"
mkdir -p "$T110/w-1.0"
python3 -c "open('$T110/w-1.0/tool.bin','wb').write(b't\x00ool\n')"
(cd "$T110" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    X.\n' > "$T110/w.projeny"
(cd "$T110" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
chmod 755 "$T110/w/tool.bin"
run_in "$T110" expect_ok "commit binary chmod" "$PROJENY" commit w.projeny
rm -rf "$T110/w" "$T110/.w.projeny.status"
run_in "$T110" expect_ok "setup after binary chmod" "$PROJENY" setup w.projeny
if [ -x "$T110/w/tool.bin" ]; then
    ok "binary +x survives re-setup"
else
    fail "binary +x survives re-setup" "$(ls -l "$T110/w/tool.bin" 2>&1)"
fi
python3 -c "import sys; sys.exit(0 if open('$T110/w/tool.bin','rb').read()==b't\x00ool\n' else 1)"
if [ $? -eq 0 ]; then
    ok "binary bytes untouched by mode commit"
else
    fail "binary bytes untouched by mode commit"
fi

# ----------------------------------------- 111. binary edits merge cleanly with text edits
# Upstream patch changes a text file while the workdir edits a binary:
# setup must apply both with no conflict.
T111="$ROOT/t111"
mkdir -p "$T111/w-1.0"
python3 -c "open('$T111/w-1.0/b.dat','wb').write(b'b\x00ase\n')"
printf 'alpha 1\n' > "$T111/w-1.0/f.c"
(cd "$T111" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Merge.\n' > "$T111/w.projeny"
(cd "$T111" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'alpha 2 upstream\n' > "$T111/w/f.c"
(cd "$T111" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T111/w.projeny" "$ROOT/t111-up.projeny"
cat > "$T111/w.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    Merge.
EOF
rm -rf "$T111/w" "$T111/.w.projeny.status"
(cd "$T111" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$T111/w/b.dat','wb').write(b'b\x00LOCAL\n')"
cp "$ROOT/t111-up.projeny" "$T111/w.projeny"
run_in "$T111" expect_ok "disjoint binary/text merge is clean" "$PROJENY" setup w.projeny
expect_file_contains "upstream text change present" "$T111/w/f.c" "alpha 2 upstream"
python3 -c "import sys; sys.exit(0 if open('$T111/w/b.dat','rb').read()==b'b\x00LOCAL\n' else 1)"
if [ $? -eq 0 ]; then
    ok "local binary edit preserved by merge"
else
    fail "local binary edit preserved by merge"
fi

# ----------------------------------------- 112. divergent binary edits conflict, keep local
# Both sides change the same binary differently: setup exits nonzero, records
# the conflict, and keeps the local bytes (never silently discards them).
# resolve + commit then works.
T112="$ROOT/t112"
mkdir -p "$T112/w-1.0"
python3 -c "open('$T112/w-1.0/b.dat','wb').write(b'b\x00ase\n')"
printf 'f\n' > "$T112/w-1.0/f.c"
(cd "$T112" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Conflict.\n' > "$T112/w.projeny"
(cd "$T112" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$T112/w/b.dat','wb').write(b'b\x00UPSTREAM\n')"
(cd "$T112" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T112/w.projeny" "$ROOT/t112-up.projeny"
cat > "$T112/w.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    Conflict.
EOF
rm -rf "$T112/w" "$T112/.w.projeny.status"
(cd "$T112" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$T112/w/b.dat','wb').write(b'b\x00LOCAL\n')"
cp "$ROOT/t112-up.projeny" "$T112/w.projeny"
run_in "$T112" expect_fail "divergent binary merge exits nonzero" "$PROJENY" setup w.projeny
expect_file_contains "divergent binary records conflict" "$T112/.w.projeny.status" "Conflict: b.dat"
python3 -c "import sys; sys.exit(0 if open('$T112/w/b.dat','rb').read()==b'b\x00LOCAL\n' else 1)"
if [ $? -eq 0 ]; then
    ok "binary conflict keeps local bytes"
else
    fail "binary conflict keeps local bytes"
fi
python3 -c "open('$T112/w/b.dat','wb').write(b'b\x00RESOLVED\n')"
run_in "$T112" expect_ok "resolve binary conflict" "$PROJENY" resolve w.projeny w/b.dat
run_in "$T112" expect_ok "commit resolved binary" "$PROJENY" commit w.projeny
rm -rf "$T112/w" "$T112/.w.projeny.status"
run_in "$T112" expect_ok "setup after binary resolve" "$PROJENY" setup w.projeny
python3 -c "import sys; sys.exit(0 if open('$T112/w/b.dat','rb').read()==b'b\x00RESOLVED\n' else 1)"
if [ $? -eq 0 ]; then
    ok "resolved binary byte-identical after re-setup"
else
    fail "resolved binary byte-identical after re-setup"
fi

# ----------------------------------------- 113. binary delete vs modify conflicts
# Upstream deletes a binary (rm+commit) while the workdir modifies it: setup
# must conflict and keep the local bytes.
T113="$ROOT/t113"
mkdir -p "$T113/w-1.0"
python3 -c "open('$T113/w-1.0/b.dat','wb').write(b'b\x00ase\n')"
printf 'f\n' > "$T113/w-1.0/f.c"
(cd "$T113" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Delmod.\n' > "$T113/w.projeny"
(cd "$T113" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
(cd "$T113" && "$PROJENY" rm w.projeny w/b.dat >/dev/null 2>&1 && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$T113/w.projeny" "$ROOT/t113-up.projeny"
cat > "$T113/w.projeny" <<'EOF'
Archive: w-1.0.tar.gz
Origname: w-1.0
Name: w

    Delmod.
EOF
rm -rf "$T113/w" "$T113/.w.projeny.status"
(cd "$T113" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$T113/w/b.dat','wb').write(b'b\x00LOCAL\n')"
cp "$ROOT/t113-up.projeny" "$T113/w.projeny"
run_in "$T113" expect_fail "binary delete/modify exits nonzero" "$PROJENY" setup w.projeny
expect_file_contains "binary delete/modify records conflict" "$T113/.w.projeny.status" "Conflict: b.dat"
python3 -c "import sys; sys.exit(0 if open('$T113/w/b.dat','rb').read()==b'b\x00LOCAL\n' else 1)"
if [ $? -eq 0 ]; then
    ok "binary delete/modify keeps local bytes"
else
    fail "binary delete/modify keeps local bytes"
fi

# ----------------------------------------- 114. diff/patch roundtrip carries binaries
# `projeny diff` emits binary blocks and `projeny patch` reproduces the bytes.
T114="$ROOT/t114"
mkdir -p "$T114/a" "$T114/b"
python3 -c "open('$T114/a/b.dat','wb').write(b'a\x00A\n')"
python3 -c "open('$T114/b/b.dat','wb').write(b'b\x00B\n' * 100)"
printf 'same\n' > "$T114/a/f.c"
printf 'same\n' > "$T114/b/f.c"
printf 'added text\n' > "$T114/b/new.txt"
(cd "$T114" && "$PROJENY" diff a b > roundtrip.diff 2>/dev/null)
expect_file_contains "diff emits binary block" "$T114/roundtrip.diff" "GIT binary patch"
rm -rf "$T114/c" && cp -a "$T114/a" "$T114/c"
run_in "$T114" expect_ok "patch applies binary diff" "$PROJENY" patch c roundtrip.diff
if cmp -s "$T114/c/b.dat" "$T114/b/b.dat" && cmp -s "$T114/c/new.txt" "$T114/b/new.txt"; then
    ok "patched tree byte-identical"
else
    fail "patched tree byte-identical"
fi

# ----------------------------------------- 115. package/extract carry committed binaries
# Committed binaries ride in package tarballs and extract dirs byte-identical.
T115="$ROOT/t115"
mkdir -p "$T115/w-1.0"
python3 -c "open('$T115/w-1.0/b.dat','wb').write(b'p\x00kg\n' * 200)"
printf 'ok\n' > "$T115/w-1.0/f.c"
(cd "$T115" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Pkg.\n' > "$T115/w.projeny"
(cd "$T115" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$T115/w/b.dat','wb').write(bytes(range(256)) * 10)"
(cd "$T115" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
python3 -c "open('$ROOT/t115-expect.bin','wb').write(open('$T115/w/b.dat','rb').read())"
run_in "$T115" expect_ok "package with binary" "$PROJENY" package w.projeny w/out.tar.gz
(cd "$T115" && tar -tzf w/out.tar.gz | sort > members.txt)
expect_file_contains "package holds the binary" "$T115/members.txt" "b.dat"
(cd "$T115" && rm -rf pk && mkdir pk && tar -xzf w/out.tar.gz -C pk)
if cmp -s "$T115/pk/out/b.dat" "$ROOT/t115-expect.bin"; then
    ok "packaged binary byte-identical"
else
    fail "packaged binary byte-identical"
fi
run_in "$T115" expect_ok "extract with binary" "$PROJENY" extract w.projeny ext
if cmp -s "$T115/ext/b.dat" "$ROOT/t115-expect.bin"; then
    ok "extracted binary byte-identical"
else
    fail "extracted binary byte-identical"
fi

# ----------------------------------------- 116. extract/package preserve tarball mtimes
# Files the patch never touches must keep the tarball timestamps through
# setup, extract, and package (like libffi's configure/mdate-sh, whose
# staleness decides whether the doc build runs).
T116="$ROOT/t116"
mkdir -p "$T116/w-1.0"
printf 'content\n' > "$T116/w-1.0/a.c"
printf '#!/bin/sh\necho hi\n' > "$T116/w-1.0/run.sh"
chmod 755 "$T116/w-1.0/run.sh"
touch -d "2020-01-01 00:00:00" "$T116/w-1.0/a.c" "$T116/w-1.0/run.sh"
(cd "$T116" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Times.\n' > "$T116/w.projeny"
(cd "$T116" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
if [ "$(stat -c %Y "$T116/w/a.c")" = "$(stat -c %Y "$T116/w/run.sh")" ] && [ "$(stat -c %y "$T116/w/a.c" | cut -c1-10)" = "2020-01-01" ]; then
    ok "setup preserves tarball mtimes"
else
    fail "setup preserves tarball mtimes" "$(stat -c '%y %n' "$T116/w/a.c" "$T116/w/run.sh")"
fi
run_in "$T116" expect_ok "extract for mtimes" "$PROJENY" extract w.projeny ext
if [ "$(stat -c %Y "$T116/ext/a.c")" = "$(stat -c %Y "$T116/w-1.0.tar.gz")" ] || [ "$(stat -c %y "$T116/ext/a.c" | cut -c1-10)" = "2020-01-01" ]; then
    ok "extract preserves tarball mtimes"
else
    fail "extract preserves tarball mtimes" "$(stat -c '%y %n' "$T116/ext/a.c" "$T116/ext/run.sh")"
fi
if [ -x "$T116/ext/run.sh" ]; then
    ok "extract keeps +x with old mtime"
else
    fail "extract keeps +x with old mtime"
fi
run_in "$T116" expect_ok "package for mtimes" "$PROJENY" package w.projeny w/out.tar.gz
(cd "$T116" && rm -rf pk && mkdir pk && tar -xzf w/out.tar.gz -C pk)
if [ "$(stat -c %y "$T116/pk/out/a.c" | cut -c1-10)" = "2020-01-01" ]; then
    ok "package preserves tarball mtimes"
else
    fail "package preserves tarball mtimes" "$(stat -c '%y %n' "$T116/pk/out/a.c")"
fi
# A file touched by the patch gets a newer timestamp, and extract keeps it newer.
printf 'content v2\n' > "$T116/w/a.c"
(cd "$T116" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
rm -rf "$T116/w" "$T116/.w.projeny.status" "$T116/ext"
(cd "$T116" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
if [ "$T116/w/a.c" -nt "$T116/w-1.0.tar.gz" ] || [ "$(stat -c %y "$T116/w/a.c" | cut -c1-10)" != "2020-01-01" ]; then
    ok "patched file gets a newer timestamp"
else
    fail "patched file gets a newer timestamp" "$(stat -c '%y %n' "$T116/w/a.c")"
fi
if [ "$(stat -c %y "$T116/w/run.sh" | cut -c1-10)" = "2020-01-01" ]; then
    ok "unpatched file keeps tarball mtime after patch commit"
else
    fail "unpatched file keeps tarball mtime after patch commit" "$(stat -c '%y %n' "$T116/w/run.sh")"
fi
run_in "$T116" expect_ok "extract after patch" "$PROJENY" extract w.projeny ext
if [ "$(stat -c %y "$T116/ext/run.sh" | cut -c1-10)" = "2020-01-01" ] && [ "$T116/ext/a.c" -nt "$T116/ext/run.sh" ]; then
    ok "extract keeps patched-newer/unpatched-old order"
else
    fail "extract keeps patched-newer/unpatched-old order" "$(stat -c '%y %n' "$T116/ext/a.c" "$T116/ext/run.sh")"
fi

# ----------------------------------------- 117. text/binary transitions roundtrip
# A text file rewritten with NUL bytes (and a binary rewritten as text)
# commits as a binary block and comes back byte-identical.
T117="$ROOT/t117"
mkdir -p "$T117/w-1.0"
python3 -c "open('$T117/w-1.0/b.dat','wb').write(b'b\x00in\n')"
printf 'plain text\n' > "$T117/w-1.0/t.txt"
(cd "$T117" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Trans.\n' > "$T117/w.projeny"
(cd "$T117" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
python3 -c "open('$T117/w/t.txt','wb').write(b't\x00ext to bin\n')"
printf 'now plain\n' > "$T117/w/b.dat"
run_in "$T117" expect_ok "commit text/binary transitions" "$PROJENY" commit w.projeny
python3 -c "open('$ROOT/t117-t.bin','wb').write(open('$T117/w/t.txt','rb').read())"
rm -rf "$T117/w" "$T117/.w.projeny.status"
run_in "$T117" expect_ok "setup after transitions" "$PROJENY" setup w.projeny
if cmp -s "$T117/w/t.txt" "$ROOT/t117-t.bin"; then
    ok "text->binary byte-identical after re-setup"
else
    fail "text->binary byte-identical after re-setup"
fi
expect_file_contains "binary->text content survives" "$T117/w/b.dat" "now plain"

# ----------------------------------------- 118. directory rm persists across setup
# `projeny rm` on a directory records the dir itself while patch blocks name
# files under it: setup must prefix-match so it does not restore them, and
# commit+setup keeps them deleted (text and binary alike).
T118="$ROOT/t118"
mkdir -p "$T118/w-1.0/d"
printf 'text a\n' > "$T118/w-1.0/d/a.txt"
python3 -c "open('$T118/w-1.0/d/b.dat','wb').write(b'd\x00ir bin\n')"
printf 'keep me\n' > "$T118/w-1.0/keep.txt"
(cd "$T118" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    DirRm.\n' > "$T118/w.projeny"
(cd "$T118" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
run_in "$T118" expect_ok "rm directory" "$PROJENY" rm w.projeny w/d
expect_file_contains "dir rm records pending op" "$T118/.w.projeny.status" "Removed: d"
if [ ! -e "$T118/w/d/a.txt" ] && [ ! -e "$T118/w/d/b.dat" ]; then
    ok "dir rm removes files from workdir"
else
    fail "dir rm removes files from workdir" "$(ls -R "$T118/w" 2>&1)"
fi
run_in "$T118" expect_ok "setup keeps dir-rm" "$PROJENY" setup w.projeny
if [ ! -e "$T118/w/d/a.txt" ] && [ ! -e "$T118/w/d/b.dat" ] && [ -f "$T118/w/keep.txt" ]; then
    ok "dir-rm persists across setup"
else
    fail "dir-rm persists across setup" "$(ls -R "$T118/w" 2>&1)"
fi
run_in "$T118" expect_ok "commit dir-rm" "$PROJENY" commit w.projeny
expect_file_contains "dir-rm commit stores text delete" "$T118/w.projeny" "d/a.txt"
expect_file_contains "dir-rm commit stores binary delete" "$T118/w.projeny" "d/b.dat"
rm -rf "$T118/w" "$T118/.w.projeny.status"
run_in "$T118" expect_ok "setup after dir-rm commit" "$PROJENY" setup w.projeny
if [ ! -e "$T118/w/d/a.txt" ] && [ ! -e "$T118/w/d/b.dat" ] && [ -f "$T118/w/keep.txt" ]; then
    ok "committed dir-rm stays deleted after re-setup"
else
    fail "committed dir-rm stays deleted after re-setup" "$(ls -R "$T118/w" 2>&1)"
fi

# ----------------------------------------- 119. directory add with binaries commits
# `projeny add` on a directory records the dir itself while patch blocks name
# files under it: commit must prefix-match so binary adds under the dir are
# kept (text adds are never filtered), and fresh setup reproduces them.
T119="$ROOT/t119"
mkdir -p "$T119/w-1.0"
printf 'base\n' > "$T119/w-1.0/f.c"
(cd "$T119" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    DirAdd.\n' > "$T119/w.projeny"
(cd "$T119" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
mkdir -p "$T119/w/newdir"
printf 'new text\n' > "$T119/w/newdir/nt.txt"
python3 -c "open('$T119/w/newdir/nb.dat','wb').write(b'n\x00ew bin\n' * 20)"
run_in "$T119" expect_ok "add directory" "$PROJENY" add w.projeny w/newdir
expect_file_contains "dir add records pending op" "$T119/.w.projeny.status" "Added: newdir"
run_in "$T119" expect_ok "commit dir-add" "$PROJENY" commit w.projeny
expect_file_contains "dir-add text stored in patch" "$T119/w.projeny" "newdir/nt.txt"
expect_file_contains "dir-add binary stored in patch" "$T119/w.projeny" "newdir/nb.dat"
expect_file_contains "dir-add binary block stored" "$T119/w.projeny" "GIT binary patch"
python3 -c "open('$ROOT/t119-expect.bin','wb').write(open('$T119/w/newdir/nb.dat','rb').read())"
rm -rf "$T119/w" "$T119/.w.projeny.status"
run_in "$T119" expect_ok "setup after dir-add commit" "$PROJENY" setup w.projeny
expect_file_contains "dir-add text survives re-setup" "$T119/w/newdir/nt.txt" "new text"
if cmp -s "$T119/w/newdir/nb.dat" "$ROOT/t119-expect.bin"; then
    ok "dir-add binary byte-identical after re-setup"
else
    fail "dir-add binary byte-identical after re-setup"
fi

# ----------------------------------------- 120. commit refuses disappeared files
# A tracked file deleted without `projeny rm` is an error at commit time,
# not a silent deletion. Marking it with `projeny rm` recovers.
T120="$ROOT/t120"
mkdir -p "$T120/w-1.0"
printf 'one\n' > "$T120/w-1.0/a.c"
printf 'two\n' > "$T120/w-1.0/b.c"
(cd "$T120" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Disappear.\n' > "$T120/w.projeny"
run_in "$T120" expect_ok "disappear setup" "$PROJENY" setup w.projeny
rm "$T120/w/b.c"
run_in "$T120" expect_fail "commit with disappeared file fails" "$PROJENY" commit w.projeny
out="$(cd "$T120" && "$PROJENY" commit w.projeny 2>&1 || true)"
case "$out" in
*b.c*)
    ok "disappeared error names the file"
    ;;
*)
    fail "disappeared error names the file" "out: $out"
    ;;
esac
if grep -q "^diff --git " "$T120/w.projeny"; then
    fail "failed commit stores no patch"
else
    ok "failed commit stores no patch"
fi
run_in "$T120" expect_ok "rm disappeared file" "$PROJENY" rm w.projeny w/b.c
run_in "$T120" expect_ok "commit after rm succeeds" "$PROJENY" commit w.projeny
expect_file_contains "rm commit stores deletion" "$T120/w.projeny" "deleted file"
rm -rf "$T120/w" "$T120/.w.projeny.status"
run_in "$T120" expect_ok "setup after disappeared-rm commit" "$PROJENY" setup w.projeny
if [ ! -e "$T120/w/b.c" ]; then
    ok "removed file stays deleted"
else
    fail "removed file stays deleted" "ls: $(ls "$T120/w" 2>&1)"
fi

# ----------------------------------------- 121. commit ignores untracked files
# A new file that appeared without `projeny add` is left out of the patch.
# Adding it explicitly folds it in, and a later no-change commit keeps it.
T121="$ROOT/t121"
mkdir -p "$T121/w-1.0"
printf 'base\n' > "$T121/w-1.0/f.c"
(cd "$T121" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Untracked.\n' > "$T121/w.projeny"
run_in "$T121" expect_ok "untracked setup" "$PROJENY" setup w.projeny
printf 'untracked bytes\n' > "$T121/w/loose.c"
run_in "$T121" expect_ok "commit ignores untracked file" "$PROJENY" commit w.projeny
if grep -q "loose.c" "$T121/w.projeny"; then
    fail "untracked file stays out of the patch" "$(grep "loose" "$T121/w.projeny")"
else
    ok "untracked file stays out of the patch"
fi
if [ -f "$T121/w/loose.c" ]; then
    ok "untracked file survives the commit on disk"
else
    fail "untracked file survives the commit on disk"
fi
run_in "$T121" expect_ok "add untracked file" "$PROJENY" add w.projeny w/loose.c
run_in "$T121" expect_ok "commit of added file succeeds" "$PROJENY" commit w.projeny
expect_file_contains "added file is stored" "$T121/w.projeny" "loose.c"
run_in "$T121" expect_ok "recommit keeps tracked add" "$PROJENY" commit w.projeny
expect_file_contains "tracked add survives recommit" "$T121/w.projeny" "loose.c"
rm -rf "$T121/w" "$T121/.w.projeny.status"
run_in "$T121" expect_ok "setup after add commit" "$PROJENY" setup w.projeny
expect_file_contains "added file survives re-setup" "$T121/w/loose.c" "untracked bytes"

# ----------------------------------------- 122. mv pair is exempt from the filter
# The delete-side of a `projeny mv` must not error as disappeared, the
# add-side must not be ignored as untracked, and the patch must carry a
# rename.
T122="$ROOT/t122"
mkdir -p "$T122/w-1.0"
seq 1 20 > "$T122/w-1.0/n.txt"
(cd "$T122" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    MvExempt.\n' > "$T122/w.projeny"
run_in "$T122" expect_ok "mv-exempt setup" "$PROJENY" setup w.projeny
run_in "$T122" expect_ok "mv records rename" "$PROJENY" mv w.projeny w/n.txt w/m.txt
run_in "$T122" expect_ok "commit of mv succeeds" "$PROJENY" commit w.projeny
expect_file_contains "mv commit stores rename" "$T122/w.projeny" "rename from"
expect_file_contains "mv commit names source" "$T122/w.projeny" "n.txt"
expect_file_contains "mv commit names dest" "$T122/w.projeny" "m.txt"
rm -rf "$T122/w" "$T122/.w.projeny.status"
run_in "$T122" expect_ok "setup after mv commit" "$PROJENY" setup w.projeny
if [ -f "$T122/w/m.txt" ] && [ ! -e "$T122/w/n.txt" ]; then
    ok "mv paths exact after re-setup"
else
    fail "mv paths exact after re-setup" "ls: $(ls "$T122/w" 2>&1)"
fi

# ----------------------------------------- 123. disappeared binaries error
# A tracked NUL-bearing file removed without `projeny rm` fails the commit
# like text; an untracked binary stays out until added.
T123="$ROOT/t123"
mkdir -p "$T123/w-1.0"
python3 -c "open('$T123/w-1.0/b.dat','wb').write(b'a\x00b\n')"
printf 'ok\n' > "$T123/w-1.0/f.c"
(cd "$T123" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    BinDis.\n' > "$T123/w.projeny"
run_in "$T123" expect_ok "binary-disappear setup" "$PROJENY" setup w.projeny
rm "$T123/w/b.dat"
run_in "$T123" expect_fail "commit with disappeared binary fails" "$PROJENY" commit w.projeny
out="$(cd "$T123" && "$PROJENY" commit w.projeny 2>&1 || true)"
case "$out" in
*b.dat*)
    ok "disappeared-binary error names the file"
    ;;
*)
    fail "disappeared-binary error names the file" "out: $out"
    ;;
esac
run_in "$T123" expect_ok "rm disappeared binary" "$PROJENY" rm w.projeny w/b.dat
run_in "$T123" expect_ok "commit after binary rm succeeds" "$PROJENY" commit w.projeny
expect_file_contains "binary delete names the file" "$T123/w.projeny" "b.dat"
python3 -c "open('$T123/w/untracked.bin','wb').write(b'u\x00n\n')"
run_in "$T123" expect_ok "commit ignores untracked binary" "$PROJENY" commit w.projeny
if grep -q "untracked.bin" "$T123/w.projeny"; then
    fail "untracked binary stays out of the patch"
else
    ok "untracked binary stays out of the patch"
fi
run_in "$T123" expect_ok "add untracked binary" "$PROJENY" add w.projeny w/untracked.bin
run_in "$T123" expect_ok "commit of added binary succeeds" "$PROJENY" commit w.projeny
expect_file_contains "added binary is stored" "$T123/w.projeny" "untracked.bin"

# ----------------------------------------- 124. directory add covers its files
# `projeny add` on a directory marks everything under it: one add folds the
# whole subtree into the patch (prefix match, text and binary alike).
T124="$ROOT/t124"
mkdir -p "$T124/w-1.0"
printf 'base\n' > "$T124/w-1.0/base.c"
(cd "$T124" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    DirAdd.\n' > "$T124/w.projeny"
run_in "$T124" expect_ok "dir-add setup" "$PROJENY" setup w.projeny
mkdir -p "$T124/w/newdir"
printf 'new text\n' > "$T124/w/newdir/nt.txt"
python3 -c "open('$T124/w/newdir/nb.dat','wb').write(b'n\x00b\n')"
run_in "$T124" expect_ok "add whole directory" "$PROJENY" add w.projeny w/newdir
run_in "$T124" expect_ok "commit dir-add" "$PROJENY" commit w.projeny
expect_file_contains "dir-add text stored" "$T124/w.projeny" "newdir/nt.txt"
expect_file_contains "dir-add binary stored" "$T124/w.projeny" "newdir/nb.dat"
python3 -c "open('$ROOT/t124-expect.bin','wb').write(open('$T124/w/newdir/nb.dat','rb').read())"
rm -rf "$T124/w" "$T124/.w.projeny.status"
run_in "$T124" expect_ok "setup after dir-add" "$PROJENY" setup w.projeny
expect_file_contains "dir-add text survives" "$T124/w/newdir/nt.txt" "new text"
if cmp -s "$T124/w/newdir/nb.dat" "$ROOT/t124-expect.bin"; then
    ok "dir-add binary byte-identical after re-setup"
else
    fail "dir-add binary byte-identical after re-setup"
fi

# ----------------------------------------- 125. status reports all six categories
# One checkout holds a modification, a pending removal, a pending addition,
# an untracked file, a disappeared file, and a pending rename at once.
T125="$ROOT/t125"
mkdir -p "$T125/w-1.0"
printf 'v1\n' > "$T125/w-1.0/mod.c"
printf 'v1\n' > "$T125/w-1.0/gone.c"
printf 'v1\n' > "$T125/w-1.0/victim.c"
printf 'v1\n' > "$T125/w-1.0/old.c"
(cd "$T125" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Six.\n' > "$T125/w.projeny"
run_in "$T125" expect_ok "six-state setup" "$PROJENY" setup w.projeny
printf 'v2\n' > "$T125/w/mod.c"
rm "$T125/w/gone.c"
printf 'fresh\n' > "$T125/w/fresh.c"
run_in "$T125" expect_ok "six-state add" "$PROJENY" add w.projeny w/fresh.c
run_in "$T125" expect_ok "six-state rm" "$PROJENY" rm w.projeny w/victim.c
run_in "$T125" expect_ok "six-state mv" "$PROJENY" mv w.projeny w/old.c w/new.c
printf 'loose\n' > "$T125/w/loose.c"
out="$(cd "$T125" && "$PROJENY" status w.projeny 2>&1)"
case "$out" in
*Modified:\ mod.c*)
    ok "status lists modified file"
    ;;
*)
    fail "status lists modified file" "out: $out"
    ;;
esac
case "$out" in
*Removed:\ victim.c*)
    ok "status lists marked-for-deletion file"
    ;;
*)
    fail "status lists marked-for-deletion file" "out: $out"
    ;;
esac
case "$out" in
*Added:\ fresh.c*)
    ok "status lists marked-for-addition file"
    ;;
*)
    fail "status lists marked-for-addition file" "out: $out"
    ;;
esac
case "$out" in
*Untracked:\ loose.c*)
    ok "status lists untracked file"
    ;;
*)
    fail "status lists untracked file" "out: $out"
    ;;
esac
case "$out" in
*Disappeared:\ gone.c*)
    ok "status lists disappeared file"
    ;;
*)
    fail "status lists disappeared file" "out: $out"
    ;;
esac
case "$out" in
*Renamed:\ old.c\ -\>\ new.c*)
    ok "status lists renamed file"
    ;;
*)
    fail "status lists renamed file" "out: $out"
    ;;
esac
case "$out" in
*Disappeared:\ victim.c*)
    fail "pending removal is not reported as disappeared" "out: $out"
    ;;
*)
    ok "pending removal is not reported as disappeared"
    ;;
esac
case "$out" in
*Untracked:\ fresh.c*|*Untracked:\ new.c*)
    fail "pending add/rename-dest is not reported as untracked" "out: $out"
    ;;
*)
    ok "pending add/rename-dest is not reported as untracked"
    ;;
esac
run_in "$T125" expect_fail "commit with disappeared file fails" "$PROJENY" commit w.projeny

# ----------------------------------------- 126. pkgconf real-project roundtrip
# Exercises setup/extract/package against the real converted project
# (projects/pkgconf.projeny + projects/pkgconf-3.0.6.tar.xz) when present:
# the Fil-C patch must apply and extract/package must carry identical
# tracked payloads. Skips cleanly when run outside a checkout.
TPKGCONF="$ROOT/t126-pkgconf"
mkdir -p "$TPKGCONF"
_SUITE_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$_SUITE_DIR/../../pkgconf.projeny" ] && [ -f "$_SUITE_DIR/../../pkgconf-3.0.6.tar.xz" ]; then
    cp "$_SUITE_DIR/../../pkgconf.projeny" "$_SUITE_DIR/../../pkgconf-3.0.6.tar.xz" "$TPKGCONF/"
    run_in "$TPKGCONF" expect_ok "pkgconf setup exits 0" "$PROJENY" setup pkgconf.projeny
    expect_file_contains "pkgconf setup applies configure patch" "$TPKGCONF/pkgconf/configure" "pizlonated_"
    expect_file_contains "pkgconf setup applies Makefile.in patch" "$TPKGCONF/pkgconf/Makefile.in" "pizlonated_pkgconf_"
    run_in "$TPKGCONF" expect_ok "pkgconf extract exits 0" "$PROJENY" extract pkgconf.projeny extracted
    expect_file_contains "pkgconf extract carries patch" "$TPKGCONF/extracted/configure" "pizlonated_"
    run_in "$TPKGCONF" expect_ok "pkgconf package exits 0" "$PROJENY" package pkgconf.projeny pkgconf-out.tar.xz
    mkdir -p "$TPKGCONF/unpack" && tar -xf "$TPKGCONF/pkgconf-out.tar.xz" -C "$TPKGCONF/unpack"
    if diff -r "$TPKGCONF/unpack/pkgconf-out" "$TPKGCONF/extracted" >/dev/null 2>&1; then
        ok "pkgconf extract equals package payload"
    else
        fail "pkgconf extract equals package payload" "$(diff -r "$TPKGCONF/unpack/pkgconf-out" "$TPKGCONF/extracted" 2>&1 | head -10)"
    fi
    expect_file_contains "pkgconf package carries patch" "$TPKGCONF/unpack/pkgconf-out/configure" "pizlonated_"
else
    ok "pkgconf roundtrip skipped (no real project files)"
fi

# ------------------------------------- 127. setup maintains <Archive>.snapshot
# setup records a byte-exact snapshot copy of the tarball it used, so a
# later setup can still reconstruct the status-recorded tree after git
# deletes the archive (e.g. an upstream rebase removes the old tarball).
T127="$ROOT/t127"
make_tarballs "$T127" fake
write_projeny "$T127" fake 1.0 fake
run_in "$T127" expect_ok "snapshot setup exits 0" "$PROJENY" setup fake.projeny
if [ -f "$T127/.fake-1.0.tar.gz.snapshot" ]; then
    ok "setup writes snapshot next to archive"
else
    fail "setup writes snapshot next to archive" "ls: $(ls "$T127" 2>&1)"
fi
if [ -f "$T127/.fake-1.0.tar.gz.snapshot" ] && \
   cmp -s "$T127/.fake-1.0.tar.gz.snapshot" "$T127/fake-1.0.tar.gz"; then
    ok "snapshot is byte-exact copy of archive"
else
    fail "snapshot is byte-exact copy of archive" "sizes: $(wc -c < "$T127/.fake-1.0.tar.gz.snapshot" 2>&1) vs $(wc -c < "$T127/fake-1.0.tar.gz" 2>&1)"
fi
run_in "$T127" expect_ok "snapshot setup-again exits 0" "$PROJENY" setup fake.projeny
if [ -f "$T127/.fake-1.0.tar.gz.snapshot" ] && \
   cmp -s "$T127/.fake-1.0.tar.gz.snapshot" "$T127/fake-1.0.tar.gz"; then
    ok "setup-again keeps snapshot byte-exact"
else
    fail "setup-again keeps snapshot byte-exact"
fi

# ----------------------------- 128. setup after status archive deleted (clean)
# The pulled upstream rebase rewrote the .projeny file to the new tarball and
# git deleted the old one; setup must still work (snapshot-based E tree).
T128="$ROOT/t128"
make_tarballs "$T128" fake
write_projeny "$T128" fake 1.0 fake
(cd "$T128" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm "$T128/fake-1.0.tar.gz"
write_projeny "$T128" fake 2.0 fake
run_in "$T128" expect_ok "setup with deleted status archive exits 0" "$PROJENY" setup fake.projeny
expect_file_contains "deleted-archive setup checks out v2" "$T128/fake/README" "hello v2"
expect_file_contains "deleted-archive setup embeds new archive" "$T128/.fake.projeny.status" "Archive: fake-2.0.tar.gz"
if [ -f "$T128/.fake-2.0.tar.gz.snapshot" ] && \
   cmp -s "$T128/.fake-2.0.tar.gz.snapshot" "$T128/fake-2.0.tar.gz"; then
    ok "deleted-archive setup snapshots the new archive"
else
    fail "deleted-archive setup snapshots the new archive"
fi

# ------------------------------ 129. setup merge after status archive deleted
# THE bug: uncommitted local changes must still merge onto the new tarball
# even after the status-recorded archive was deleted from git.
T129="$ROOT/t129"
make_tarballs "$T129" fake
write_projeny "$T129" fake 1.0 fake
(cd "$T129" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T129/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
rm "$T129/fake-1.0.tar.gz"
write_projeny "$T129" fake 2.0 fake
run_in "$T129" expect_ok "merge setup with deleted status archive exits 0" "$PROJENY" setup fake.projeny
expect_file_contains "deleted-archive merge keeps local edit" "$T129/fake/src/a.c" "delta = 100"
expect_file_contains "deleted-archive merge takes v2 content" "$T129/fake/src/a.c" "alpha = 2"
expect_file_not_contains "deleted-archive merge has no markers" "$T129/fake/src/a.c" "<<<<<<<"
expect_file_contains "deleted-archive merge embeds new archive" "$T129/.fake.projeny.status" "Archive: fake-2.0.tar.gz"

# -------------------- 130. conflicting setup merge after archive deleted
# Same scenario, but the local edit conflicts with the new tarball: setup
# must still produce the usual markers + status conflict (not die on the
# missing archive), and resolve/commit/setup recover normally.
T130="$ROOT/t130"
make_tarballs "$T130" fake
write_projeny "$T130" fake 1.0 fake
(cd "$T130" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T130/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int alpha = 1;", "int alpha = 999;")
open(p, "w").write(s)
EOF
rm "$T130/fake-1.0.tar.gz"
write_projeny "$T130" fake 2.0 fake
run_in "$T130" expect_fail "conflicting setup with deleted archive exits nonzero" "$PROJENY" setup fake.projeny
expect_file_contains "deleted-archive conflict leaves markers" "$T130/fake/src/a.c" "<<<<<<<"
expect_file_contains "deleted-archive conflict lists conflict in status" "$T130/.fake.projeny.status" "Conflict: src/a.c"
printf 'int alpha = 777;\n\nint beta = 1;\n\nint gamma = 1;\n\nint delta = 1;\n' > "$T130/fake/src/a.c"
run_in "$T130" expect_ok "deleted-archive conflict resolves" "$PROJENY" resolve fake.projeny fake/src/a.c
run_in "$T130" expect_ok "deleted-archive conflict commits" "$PROJENY" commit fake.projeny
run_in "$T130" expect_ok "setup after deleted-archive conflict resolution exits 0" "$PROJENY" setup fake.projeny
expect_file_contains "post-resolution workdir keeps resolved content" "$T130/fake/src/a.c" "alpha = 777"
expect_file_contains "post-resolution workdir keeps v2 content" "$T130/fake/README" "hello v2"

# ------------------- 131. hard error when archive AND snapshot are both gone
# Legacy checkouts set up before snapshots existed have neither file; setup
# must still fail, but with an actionable message that mentions snapshots.
T131="$ROOT/t131"
make_tarballs "$T131" fake
write_projeny "$T131" fake 1.0 fake
(cd "$T131" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm "$T131/fake-1.0.tar.gz" "$T131/.fake-1.0.tar.gz.snapshot"
write_projeny "$T131" fake 2.0 fake
out="$(cd "$T131" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "missing archive and snapshot fails setup"
else
    fail "missing archive and snapshot fails setup" "out: $out"
fi
case "$out" in
*snapshot*)
    ok "missing archive and snapshot error mentions snapshot"
    ;;
*)
    fail "missing archive and snapshot error mentions snapshot" "out: $out"
    ;;
esac

# --------------------------------- 132. snapshot refreshed on in-place change
# A tarball rewritten in place (same name, new bytes) must refresh the
# snapshot and be picked up by the next setup.
T132="$ROOT/t132"
make_tarballs "$T132" fake
write_projeny "$T132" fake 1.0 fake
(cd "$T132" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
T132NEW="$ROOT/t132-new"
mkdir -p "$T132NEW/fake-1.0/src"
cat > "$T132NEW/fake-1.0/src/a.c" <<'EOF'
int alpha = 42;

int beta = 1;

int gamma = 1;

int delta = 1;
EOF
printf 'hello v1\n' > "$T132NEW/fake-1.0/README"
printf 'line one v1\n' > "$T132NEW/fake-1.0/src/b.c"
(cd "$T132NEW" && tar -czf "$T132/fake-1.0.tar.gz" fake-1.0)
run_in "$T132" expect_ok "in-place tarball change setup exits 0" "$PROJENY" setup fake.projeny
if [ -f "$T132/.fake-1.0.tar.gz.snapshot" ] && \
   cmp -s "$T132/.fake-1.0.tar.gz.snapshot" "$T132/fake-1.0.tar.gz"; then
    ok "in-place tarball change refreshes snapshot"
else
    fail "in-place tarball change refreshes snapshot"
fi
expect_file_contains "in-place tarball change reaches workdir" "$T132/fake/src/a.c" "alpha = 42"

# --------------------------- 133. status works after status archive deleted
# status diffs the workdir against the status-recorded tree; it must prefer
# the snapshot and still report local modifications after git deleted the
# archive.
T133="$ROOT/t133"
make_tarballs "$T133" fake
write_projeny "$T133" fake 1.0 fake
(cd "$T133" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T133/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
rm "$T133/fake-1.0.tar.gz"
out="$(cd "$T133" && "$PROJENY" status fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "status after deleted archive exits 0"
else
    fail "status after deleted archive exits 0" "rc=$rc out: $out"
fi
case "$out" in
*Modified:\ src/a.c*)
    ok "status after deleted archive lists modified file"
    ;;
*)
    fail "status after deleted archive lists modified file" "out: $out"
    ;;
esac

# --------------------------- 134. extract works after status archive deleted
# End-to-end build-script flow: `projeny extract` runs setup internally, so
# it must survive the deleted status archive and carry v2 plus local edits.
T134="$ROOT/t134"
make_tarballs "$T134" fake
write_projeny "$T134" fake 1.0 fake
(cd "$T134" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T134/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
rm "$T134/fake-1.0.tar.gz"
write_projeny "$T134" fake 2.0 fake
run_in "$T134" expect_ok "extract with deleted status archive exits 0" "$PROJENY" extract fake.projeny extracted
expect_file_contains "deleted-archive extract carries v2 content" "$T134/extracted/src/a.c" "alpha = 2"
expect_file_contains "deleted-archive extract carries local edit" "$T134/extracted/src/a.c" "delta = 100"

# ------------------ 135. legacy undotted status file migrates on first use
# Checkouts set up by older projenies hold "<f>.projeny.status"; the dotted
# ".<f>.projeny.status" name is canonical now. The first command that runs
# must rename the legacy file in place (content preserved, workdir state
# intact) — not just setup, but any command.
T135="$ROOT/t135"
make_tarballs "$T135" fake
write_projeny "$T135" fake 1.0 fake
(cd "$T135" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
mv "$T135/.fake.projeny.status" "$T135/fake.projeny.status"
python3 - "$T135/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int alpha = 1;", "int alpha = 55;")
open(p, "w").write(s)
EOF
run_in "$T135" expect_ok "legacy undotted status: setup exits 0" "$PROJENY" setup fake.projeny
if [ -f "$T135/fake.projeny.status" ]; then
    fail "legacy undotted status file is renamed away" "'$T135/fake.projeny.status' still exists"
else
    ok "legacy undotted status file is renamed away"
fi
if [ -f "$T135/.fake.projeny.status" ]; then
    ok "legacy undotted status renamed to dotted form"
else
    fail "legacy undotted status renamed to dotted form"
fi
expect_file_contains "renamed status keeps embedded copy" "$T135/.fake.projeny.status" "Archive: fake-1.0.tar.gz"
expect_file_contains "legacy status migration merges local edit" "$T135/fake/src/a.c" "alpha = 55"

# ---------------------------------- 136. legacy undotted snapshot migrates
# Same idea for the snapshot: with the archive deleted from git, the E tree
# can only come from the status file's snapshot — so a successful merge
# proves the legacy undotted snapshot was found, renamed to the dotted form,
# and used.
T136="$ROOT/t136"
make_tarballs "$T136" fake
write_projeny "$T136" fake 1.0 fake
(cd "$T136" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T136/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
rm "$T136/fake-1.0.tar.gz"
cp "$T136/.fake-1.0.tar.gz.snapshot" "$ROOT/t136-snap.before"
mv "$T136/.fake-1.0.tar.gz.snapshot" "$T136/fake-1.0.tar.gz.snapshot"
write_projeny "$T136" fake 2.0 fake
run_in "$T136" expect_ok "legacy undotted snapshot: setup exits 0" "$PROJENY" setup fake.projeny
if [ -f "$T136/fake-1.0.tar.gz.snapshot" ]; then
    fail "legacy undotted snapshot is renamed away" "'$T136/fake-1.0.tar.gz.snapshot' still exists"
else
    ok "legacy undotted snapshot is renamed away"
fi
if [ -f "$T136/.fake-1.0.tar.gz.snapshot" ] && \
   cmp -s "$T136/.fake-1.0.tar.gz.snapshot" "$ROOT/t136-snap.before"; then
    ok "legacy undotted snapshot renamed to dotted form intact"
else
    fail "legacy undotted snapshot renamed to dotted form intact"
fi
expect_file_contains "legacy snapshot migration merges local edit" "$T136/fake/src/a.c" "delta = 100"
expect_file_contains "legacy snapshot migration takes v2 content" "$T136/fake/src/a.c" "alpha = 2"

# ------------------------- 137. both forms present: dotted wins, undotted
# file is left untouched
T137="$ROOT/t137"
make_tarballs "$T137" fake
write_projeny "$T137" fake 1.0 fake
(cd "$T137" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
cp "$T137/.fake.projeny.status" "$T137/fake.projeny.status"
cp "$T137/.fake-1.0.tar.gz.snapshot" "$T137/fake-1.0.tar.gz.snapshot"
cp "$T137/fake.projeny.status" "$ROOT/t137-status-undotted.before"
cp "$T137/fake-1.0.tar.gz.snapshot" "$ROOT/t137-snap-undotted.before"
run_in "$T137" expect_ok "both forms present: setup exits 0" "$PROJENY" setup fake.projeny
if cmp -s "$T137/fake.projeny.status" "$ROOT/t137-status-undotted.before"; then
    ok "undotted status left untouched when dotted form exists"
else
    fail "undotted status left untouched when dotted form exists"
fi
if cmp -s "$T137/fake-1.0.tar.gz.snapshot" "$ROOT/t137-snap-undotted.before"; then
    ok "undotted snapshot left untouched when dotted form exists"
else
    fail "undotted snapshot left untouched when dotted form exists"
fi
expect_file_contains "dotted status still used when both exist" "$T137/.fake.projeny.status" "Status: setup"

# ----------------- 138. missing workdir: stale status/snapshot are renamed
# When the checkout directory is gone, the status file and snapshots are
# stale state: a warning names each destination and the files move to
# '<name>.stale', then a fresh setup proceeds.
T138="$ROOT/t138"
make_tarballs "$T138" fake
write_projeny "$T138" fake 1.0 fake
(cd "$T138" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T138/fake"
out="$(cd "$T138" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup with missing workdir and stale state exits 0"
else
    fail "setup with missing workdir and stale state exits 0" "rc=$rc out: $out"
fi
case "$out" in
*.fake.projeny.status.stale*)
    ok "warning names status .stale destination"
    ;;
*)
    fail "warning names status .stale destination" "out: $out"
    ;;
esac
case "$out" in
*.fake-1.0.tar.gz.snapshot.stale*)
    ok "warning names snapshot .stale destination"
    ;;
*)
    fail "warning names snapshot .stale destination" "out: $out"
    ;;
esac
if [ -f "$T138/.fake.projeny.status.stale" ]; then
    ok "stale status file renamed to .stale"
else
    fail "stale status file renamed to .stale" "ls: $(ls -a "$T138" 2>&1)"
fi
if [ -f "$T138/.fake-1.0.tar.gz.snapshot.stale" ]; then
    ok "stale snapshot renamed to .stale"
else
    fail "stale snapshot renamed to .stale" "ls: $(ls -a "$T138" 2>&1)"
fi
if [ ! -f "$T138/fake.projeny.status" ] && [ ! -f "$T138/fake-1.0.tar.gz.snapshot" ]; then
    ok "no undotted leftovers after stale rename"
else
    fail "no undotted leftovers after stale rename"
fi
expect_file_contains "fresh setup after stale rename checks out v1" "$T138/fake/README" "hello v1"
expect_file_contains "fresh status written after stale rename" "$T138/.fake.projeny.status" "Status: setup"

# ------------------------------- 139. missing workdir: .stale name already
# taken, so the next free name (.stale2, .stale3) is used
T139="$ROOT/t139"
make_tarballs "$T139" fake
write_projeny "$T139" fake 1.0 fake
(cd "$T139" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T139/fake"
printf 'older run\n' > "$T139/.fake.projeny.status.stale"
printf 'even older run\n' > "$T139/.fake.projeny.status.stale2"
printf 'older snapshot\n' > "$T139/.fake-1.0.tar.gz.snapshot.stale"
out="$(cd "$T139" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup with taken .stale names exits 0"
else
    fail "setup with taken .stale names exits 0" "rc=$rc out: $out"
fi
case "$out" in
*.fake.projeny.status.stale3*)
    ok "status uses .stale3 when .stale and .stale2 are taken"
    ;;
*)
    fail "status uses .stale3 when .stale and .stale2 are taken" "out: $out"
    ;;
esac
case "$out" in
*.fake-1.0.tar.gz.snapshot.stale2*)
    ok "snapshot uses .stale2 when .stale is taken"
    ;;
*)
    fail "snapshot uses .stale2 when .stale is taken" "out: $out"
    ;;
esac
if [ -f "$T139/.fake.projeny.status.stale3" ]; then
    ok "stale status file lands on .stale3"
else
    fail "stale status file lands on .stale3" "ls: $(ls -a "$T139" 2>&1)"
fi
if [ -f "$T139/.fake-1.0.tar.gz.snapshot.stale2" ]; then
    ok "stale snapshot lands on .stale2"
else
    fail "stale snapshot lands on .stale2" "ls: $(ls -a "$T139" 2>&1)"
fi
expect_file_contains "taken .stale name is not overwritten" "$T139/.fake.projeny.status.stale" "older run"
expect_file_contains "taken .stale2 name is not overwritten" "$T139/.fake.projeny.status.stale2" "even older run"

# -------------------- 140. missing workdir: undotted variants are also
# disregarded (renamed to the stale names)
T140="$ROOT/t140"
make_tarballs "$T140" fake
write_projeny "$T140" fake 1.0 fake
(cd "$T140" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T140/fake"
mv "$T140/.fake.projeny.status" "$T140/fake.projeny.status"
mv "$T140/.fake-1.0.tar.gz.snapshot" "$T140/fake-1.0.tar.gz.snapshot"
out="$(cd "$T140" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup with undotted stale state exits 0"
else
    fail "setup with undotted stale state exits 0" "rc=$rc out: $out"
fi
if [ ! -f "$T140/fake.projeny.status" ] && [ ! -f "$T140/fake-1.0.tar.gz.snapshot" ]; then
    ok "undotted status and snapshot are gone after setup"
else
    fail "undotted status and snapshot are gone after setup"
fi
if [ -f "$T140/.fake.projeny.status.stale" ]; then
    ok "undotted status renamed to dotted .stale"
else
    fail "undotted status renamed to dotted .stale" "ls: $(ls -a "$T140" 2>&1)"
fi
if [ -f "$T140/.fake-1.0.tar.gz.snapshot.stale" ]; then
    ok "undotted snapshot renamed to dotted .stale"
else
    fail "undotted snapshot renamed to dotted .stale" "ls: $(ls -a "$T140" 2>&1)"
fi
expect_file_contains "fresh setup after undotted stale rename checks out v1" "$T140/fake/README" "hello v1"

# ------------------- 141. workdir present but no status file: hard error
T141="$ROOT/t141"
make_tarballs "$T141" fake
write_projeny "$T141" fake 1.0 fake
(cd "$T141" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm "$T141/.fake.projeny.status"
out="$(cd "$T141" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "workdir without status file hard-errors"
else
    fail "workdir without status file hard-errors" "out: $out"
fi
case "$out" in
*.fake.projeny.status*)
    ok "hard error names the dotted status file"
    ;;
*)
    fail "hard error names the dotted status file" "out: $out"
    ;;
esac
if [ ! -f "$T141/.fake.projeny.status" ]; then
    ok "hard error does not fabricate a status file"
else
    fail "hard error does not fabricate a status file"
fi

# -------------------- 142. missing snapshot with tarball present is fine
# (setup recreates the dotted snapshot from the tarball)
T142="$ROOT/t142"
make_tarballs "$T142" fake
write_projeny "$T142" fake 1.0 fake
(cd "$T142" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm "$T142/.fake-1.0.tar.gz.snapshot"
run_in "$T142" expect_ok "setup with missing snapshot and present tarball exits 0" "$PROJENY" setup fake.projeny
if [ -f "$T142/.fake-1.0.tar.gz.snapshot" ] && \
   cmp -s "$T142/.fake-1.0.tar.gz.snapshot" "$T142/fake-1.0.tar.gz"; then
    ok "setup recreates dotted snapshot from tarball"
else
    fail "setup recreates dotted snapshot from tarball"
fi

# ----------------- 143. status with missing workdir: stale state renamed
# (a non-setup command performs the same reconciliation)
T143="$ROOT/t143"
make_tarballs "$T143" fake
write_projeny "$T143" fake 1.0 fake
(cd "$T143" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T143/fake"
out="$(cd "$T143" && "$PROJENY" status fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "status with missing workdir exits 0"
else
    fail "status with missing workdir exits 0" "rc=$rc out: $out"
fi
case "$out" in
*.fake.projeny.status.stale*)
    ok "status warning names status .stale destination"
    ;;
*)
    fail "status warning names status .stale destination" "out: $out"
    ;;
esac
case "$out" in
*.fake-1.0.tar.gz.snapshot.stale*)
    ok "status warning names snapshot .stale destination"
    ;;
*)
    fail "status warning names snapshot .stale destination" "out: $out"
    ;;
esac
if [ -f "$T143/.fake.projeny.status.stale" ] && \
   [ -f "$T143/.fake-1.0.tar.gz.snapshot.stale" ]; then
    ok "status renames stale state to .stale"
else
    fail "status renames stale state to .stale" "ls: $(ls -a "$T143" 2>&1)"
fi
if [ ! -f "$T143/.fake.projeny.status" ]; then
    ok "status leaves no dotted status file behind"
else
    fail "status leaves no dotted status file behind"
fi
case "$out" in
*Status:\ setup*)
    ok "status still reports the recorded state"
    ;;
*)
    fail "status still reports the recorded state" "out: $out"
    ;;
esac

# ------------------------- 144. status copy-on-fallback creates snapshot
# status must also work (and self-heal) when only the tarball exists.
T144="$ROOT/t144"
make_tarballs "$T144" fake
write_projeny "$T144" fake 1.0 fake
(cd "$T144" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm "$T144/.fake-1.0.tar.gz.snapshot"
out="$(cd "$T144" && "$PROJENY" status fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "status with missing snapshot and present tarball exits 0"
else
    fail "status with missing snapshot and present tarball exits 0" "rc=$rc out: $out"
fi
if [ -f "$T144/.fake-1.0.tar.gz.snapshot" ] && \
   cmp -s "$T144/.fake-1.0.tar.gz.snapshot" "$T144/fake-1.0.tar.gz"; then
    ok "status copy-on-fallback creates dotted snapshot"
else
    fail "status copy-on-fallback creates dotted snapshot"
fi

# -------------------- 145. commit with missing workdir: stale state is
# renamed, then the command still hard-errors
T145="$ROOT/t145"
make_tarballs "$T145" fake
write_projeny "$T145" fake 1.0 fake
(cd "$T145" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T145/fake"
out="$(cd "$T145" && "$PROJENY" commit fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "commit with missing workdir hard-errors"
else
    fail "commit with missing workdir hard-errors" "out: $out"
fi
case "$out" in
*.fake.projeny.status.stale*)
    ok "commit warning names status .stale destination"
    ;;
*)
    fail "commit warning names status .stale destination" "out: $out"
    ;;
esac
if [ -f "$T145/.fake.projeny.status.stale" ] && \
   [ -f "$T145/.fake-1.0.tar.gz.snapshot.stale" ]; then
    ok "commit renames stale state to .stale"
else
    fail "commit renames stale state to .stale" "ls: $(ls -a "$T145" 2>&1)"
fi

# --------------------- 146. add with missing workdir: stale state is
# renamed, then the command still hard-errors
T146="$ROOT/t146"
make_tarballs "$T146" fake
write_projeny "$T146" fake 1.0 fake
(cd "$T146" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T146/fake"
out="$(cd "$T146" && "$PROJENY" add fake.projeny fake/src/b.c 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "add with missing workdir hard-errors"
else
    fail "add with missing workdir hard-errors" "out: $out"
fi
case "$out" in
*workdir*)
    ok "add error mentions the missing workdir"
    ;;
*)
    fail "add error mentions the missing workdir" "out: $out"
    ;;
esac
if [ -f "$T146/.fake.projeny.status.stale" ]; then
    ok "add renames stale status to .stale"
else
    fail "add renames stale status to .stale" "ls: $(ls -a "$T146" 2>&1)"
fi

# --------------- 147. rebase with missing workdir: runs setup first, which
# renames the stale state (setup's reconciliation is inherited)
T147="$ROOT/t147"
make_tarballs "$T147" fake
write_projeny "$T147" fake 1.0 fake
(cd "$T147" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T147/fake"
out="$(cd "$T147" && "$PROJENY" rebase fake.projeny fake-2.0.tar.gz 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "rebase with missing workdir exits 0"
else
    fail "rebase with missing workdir exits 0" "rc=$rc out: $out"
fi
case "$out" in
*.fake.projeny.status.stale*)
    ok "rebase warning names status .stale destination"
    ;;
*)
    fail "rebase warning names status .stale destination" "out: $out"
    ;;
esac
if [ -f "$T147/.fake.projeny.status.stale" ]; then
    ok "rebase (via setup) renames stale status to .stale"
else
    fail "rebase (via setup) renames stale status to .stale" "ls: $(ls -a "$T147" 2>&1)"
fi
expect_file_contains "rebase after stale rename checks out v2" "$T147/fake/README" "hello v2"
expect_file_contains "rebase after stale rename embeds new archive" "$T147/.fake.projeny.status" "Archive: fake-2.0.tar.gz"

# ------- 148. journal crash window: no command stales recovery-critical state
# An interrupted conflicted setup leaves its setup journal (crash window:
# the journal was written; the status/.projeny/workdir updates did not all
# land — here the workdir is additionally gone and the interrupted rebase
# already deleted the old tarball). Recovery needs the status file (union
# bookkeeping/conflict disambiguation) and the v1 snapshot (the only
# remaining copy of the local side's archive), so NO command may stale them
# while the journal exists: a read-only `status` must leave the crash state
# untouched, and the rerun `setup` must recover using the snapshot.
T148="$ROOT/t148"
make_tarballs "$T148" fake
write_projeny "$T148" fake 1.0 fake
(cd "$T148" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
cp "$T148/fake.projeny" "$ROOT/t148-local.projeny"
# upstream rebase moved the checkout to the v2 tarball.
write_projeny "$T148" fake 2.0 fake
cp "$T148/fake.projeny" "$ROOT/t148-up.projeny"
# git-conflicted .projeny plus the journal recording both sides.
make_conflicted "$ROOT/t148-local.projeny" "$ROOT/t148-up.projeny" "$T148/conf.raw"
cp "$T148/conf.raw" "$T148/fake.projeny"
python3 - "$T148/conf.raw" "$T148/fake.projeny.setup-journal" <<'PYEOF'
import sys
raw = open(sys.argv[1]).read()
journal = "projeny setup journal v1\nupstream: theirs\n--- conflicted .projeny ---\n" + raw
open(sys.argv[2], "w").write(journal)
PYEOF
rm -rf "$T148/fake" "$T148/fake-1.0.tar.gz"
if [ -f "$T148/.fake.projeny.status" ] && [ -f "$T148/.fake-1.0.tar.gz.snapshot" ] && \
   [ ! -e "$T148/fake-1.0.tar.gz" ] && [ -f "$T148/fake.projeny.setup-journal" ] && \
   [ ! -e "$T148/fake" ]; then
    ok "journal crash state prepared (no workdir, old tarball deleted)"
else
    fail "journal crash state prepared (no workdir, old tarball deleted)" \
         "ls: $(ls -A "$T148" 2>&1)"
fi
# (a) a read-only command must leave the crash state exactly as it is.
out="$(cd "$T148" && "$PROJENY" status fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "status with setup journal exits 0"
else
    fail "status with setup journal exits 0" "rc=$rc out: $out"
fi
if [ -f "$T148/.fake.projeny.status" ] && [ -f "$T148/.fake-1.0.tar.gz.snapshot" ]; then
    ok "status leaves status file and snapshot in place while journal exists"
else
    fail "status leaves status file and snapshot in place while journal exists" \
         "ls: $(ls -A "$T148" 2>&1)"
fi
if [ -n "$(ls -A "$T148" | grep -F .stale)" ]; then
    fail "status stales nothing while a setup journal exists" "$(ls -A "$T148")"
else
    ok "status stales nothing while a setup journal exists"
fi
# (b) recovery proceeds using the snapshot (the tarball is gone) and
# reconstructs the checkout from the journal's upstream side.
out="$(cd "$T148" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup recovers from the journal using the snapshot"
else
    fail "setup recovers from the journal using the snapshot" "rc=$rc out: $out"
fi
if echo "$out" | grep -qi "recover"; then
    ok "recovery says it recovered (status untouched beforehand)"
else
    fail "recovery says it recovered (status untouched beforehand)" "out: $out"
fi
if cmp -s "$T148/fake.projeny" "$ROOT/t148-up.projeny"; then
    ok "journal recovery keeps upstream .projeny bytes"
else
    fail "journal recovery keeps upstream .projeny bytes" "$(cat "$T148/fake.projeny")"
fi
expect_file_contains "journal recovery checks out the upstream archive" "$T148/fake/README" "hello v2"
expect_file_contains "journal recovery checks out v2 content" "$T148/fake/src/a.c" "alpha = 2"
if [ -e "$T148/fake.projeny.setup-journal" ]; then
    fail "journal recovery removes the journal" "$(ls -A "$T148" 2>&1)"
else
    ok "journal recovery removes the journal"
fi
expect_file_contains "journal recovery writes the upstream status" "$T148/.fake.projeny.status" "Archive: fake-2.0.tar.gz"

# ------------- 149. missing workdir with BOTH forms: all four are staled
# Requirement: with the workdir gone, no status/snapshot file may remain
# under ANY of its names. When the dotted and legacy undotted forms both
# exist, each is disregarded under its own name (dotted -> dotted.stale,
# undotted -> undotted.stale), so nothing survives under either form.
T149="$ROOT/t149"
make_tarballs "$T149" fake
write_projeny "$T149" fake 1.0 fake
(cd "$T149" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T149/fake"
cp "$T149/.fake.projeny.status" "$T149/fake.projeny.status"
cp "$T149/.fake-1.0.tar.gz.snapshot" "$T149/fake-1.0.tar.gz.snapshot"
out="$(cd "$T149" && "$PROJENY" status fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "status with both forms and missing workdir exits 0"
else
    fail "status with both forms and missing workdir exits 0" "rc=$rc out: $out"
fi
if [ ! -e "$T149/.fake.projeny.status" ] && [ ! -e "$T149/fake.projeny.status" ] && \
   [ ! -e "$T149/.fake-1.0.tar.gz.snapshot" ] && [ ! -e "$T149/fake-1.0.tar.gz.snapshot" ]; then
    ok "both-forms stale reconciliation removes all four original names"
else
    fail "both-forms stale reconciliation removes all four original names" \
         "ls: $(ls -A "$T149" 2>&1)"
fi
if [ -f "$T149/.fake.projeny.status.stale" ] && [ -f "$T149/fake.projeny.status.stale" ] && \
   [ -f "$T149/.fake-1.0.tar.gz.snapshot.stale" ] && [ -f "$T149/fake-1.0.tar.gz.snapshot.stale" ]; then
    ok "both-forms stale reconciliation stales each form under its own name"
else
    fail "both-forms stale reconciliation stales each form under its own name" \
         "ls: $(ls -A "$T149" 2>&1)"
fi
case "$out" in
*".fake.projeny.status.stale"*)
    ok "warning names the dotted status destination"
    ;;
*)
    fail "warning names the dotted status destination" "out: $out"
    ;;
esac
case "$out" in
# The warning quotes each path, and '.fake...' contains 'fake...' as a
# substring — anchor on the quoted undotted destination.
*"'fake.projeny.status.stale'"*)
    ok "warning names the undotted status destination"
    ;;
*)
    fail "warning names the undotted status destination" "out: $out"
    ;;
esac
case "$out" in
*"'fake-1.0.tar.gz.snapshot.stale'"*)
    ok "warning names the undotted snapshot destination"
    ;;
*)
    fail "warning names the undotted snapshot destination" "out: $out"
    ;;
esac

# --------- 150. rebase to a new tarball: the OLD archive's snapshot is
# staled too (the status file's embedded Archive: differs from the current
# .projeny's), exercising the stale reconciliation's different-archive
# branch (the old archive is recovered from the status bytes).
T150="$ROOT/t150"
make_tarballs "$T150" fake
write_projeny "$T150" fake 1.0 fake
(cd "$T150" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm -rf "$T150/fake"
# An upstream rebase rewrote the .projeny to the new tarball; a snapshot of
# the new tarball already exists (byte-exact copy, as write_status keeps).
write_projeny "$T150" fake 2.0 fake
cp "$T150/fake-2.0.tar.gz" "$T150/.fake-2.0.tar.gz.snapshot"
rm "$T150/fake-1.0.tar.gz"
out="$(cd "$T150" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup with rebased archive and both snapshots exits 0"
else
    fail "setup with rebased archive and both snapshots exits 0" "rc=$rc out: $out"
fi
# `setup` stales the status file and then writes a FRESH one (unlike
# `status`, which leaves it gone), so the status assertions here check the
# .stale destination plus the rewritten file's embedded archive. The OLD
# archive's snapshot, by contrast, is gone for good: nothing recreates it.
if [ ! -e "$T150/.fake-1.0.tar.gz.snapshot" ]; then
    ok "rebase-stale removes the OLD archive's snapshot"
else
    fail "rebase-stale removes the OLD archive's snapshot" \
         "ls: $(ls -A "$T150" 2>&1)"
fi
if [ -f "$T150/.fake-1.0.tar.gz.snapshot.stale" ] && \
   [ -f "$T150/.fake-2.0.tar.gz.snapshot.stale" ] && \
   [ -f "$T150/.fake.projeny.status.stale" ]; then
    ok "rebase-stale stales status and BOTH archives' snapshots"
else
    fail "rebase-stale stales status and BOTH archives' snapshots" \
         "ls: $(ls -A "$T150" 2>&1)"
fi
if [ -f "$T150/.fake-2.0.tar.gz.snapshot" ] && \
   cmp -s "$T150/.fake-2.0.tar.gz.snapshot" "$T150/fake-2.0.tar.gz"; then
    ok "fresh setup recreates the new archive's snapshot"
else
    fail "fresh setup recreates the new archive's snapshot"
fi
expect_file_contains "fresh setup after rebase-stale checks out v2" "$T150/fake/README" "hello v2"
expect_file_contains "fresh status after rebase-stale embeds the new archive" "$T150/.fake.projeny.status" "Archive: fake-2.0.tar.gz"

# ------------------- 151. setup into an empty existing directory
# The workdir exists (empty) but no status file does: instead of the old
# unconditional hard error, setup adopts the directory in place, unpacks
# the tree into it, and writes the status file.
T151="$ROOT/t151"
make_tarballs "$T151" fake
write_projeny "$T151" fake 1.0 fake
mkdir "$T151/fake"
out="$(cd "$T151" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup into an empty existing directory exits 0"
else
    fail "setup into an empty existing directory exits 0" "rc=$rc out: $out"
fi
case "$out" in
*"into existing directory"*)
    ok "setup into an existing directory says so"
    ;;
*)
    fail "setup into an existing directory says so" "out: $out"
    ;;
esac
expect_file_contains "empty-dir setup checks out the tarball" "$T151/fake/README" "hello v1"
expect_file_contains "empty-dir setup writes src/a.c" "$T151/fake/src/a.c" "int alpha = 1;"
expect_file_contains "empty-dir setup writes src/b.c" "$T151/fake/src/b.c" "line one v1"
if [ -f "$T151/.fake.projeny.status" ]; then
    ok "empty-dir setup writes the status file"
else
    fail "empty-dir setup writes the status file"
fi
expect_file_contains "empty-dir setup status reports setup" "$T151/.fake.projeny.status" "Status: setup"
expect_file_contains "empty-dir setup status embeds the archive" "$T151/.fake.projeny.status" "Archive: fake-1.0.tar.gz"

# -------------- 152. setup into a compatible non-empty directory keeps files
# A directory holding only files the tarball and patch never touch is
# adopted in place: foreign files (nested, top-level, hidden) survive
# byte-for-byte next to the fresh checkout.
T152="$ROOT/t152"
make_tarballs "$T152" fake
write_projeny "$T152" fake 1.0 fake
mkdir -p "$T152/fake/src" "$T152/fake/.hidden"
printf 'my notes\n' > "$T152/fake/notes.txt"
printf 'int extra = 1;\n' > "$T152/fake/src/extra.c"
printf 'hidden state\n' > "$T152/fake/.hidden/keep"
printf 'dot file\n' > "$T152/fake/.dotfile"
printf 'my notes\n' > "$ROOT/t152-notes"
printf 'int extra = 1;\n' > "$ROOT/t152-extra"
printf 'hidden state\n' > "$ROOT/t152-keep"
printf 'dot file\n' > "$ROOT/t152-dot"
out="$(cd "$T152" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup into a compatible non-empty directory exits 0"
else
    fail "setup into a compatible non-empty directory exits 0" "rc=$rc out: $out"
fi
expect_file_eq "compatible setup keeps the nested foreign file" "$T152/fake/src/extra.c" "$ROOT/t152-extra"
expect_file_eq "compatible setup keeps the top-level foreign file" "$T152/fake/notes.txt" "$ROOT/t152-notes"
expect_file_eq "compatible setup keeps the hidden dir's file" "$T152/fake/.hidden/keep" "$ROOT/t152-keep"
expect_file_eq "compatible setup keeps the hidden file" "$T152/fake/.dotfile" "$ROOT/t152-dot"
expect_file_contains "compatible setup checks out the tarball" "$T152/fake/README" "hello v1"
expect_file_contains "compatible setup checks out src/a.c" "$T152/fake/src/a.c" "int alpha = 1;"
if [ -f "$T152/.fake.projeny.status" ]; then
    ok "compatible setup writes the status file"
else
    fail "compatible setup writes the status file"
fi

# ------------------------- 153. refusal: the tarball would overwrite a file
# A pre-existing file at a tarball path is a conflict: setup must refuse
# naming the path, leave the file byte-for-byte intact, leak nothing else
# into the directory, and write no status file.
T153="$ROOT/t153"
make_tarballs "$T153" fake
write_projeny "$T153" fake 1.0 fake
mkdir "$T153/fake"
printf 'local readme\n' > "$T153/fake/README"
printf 'local readme\n' > "$ROOT/t153-readme"
out="$(cd "$T153" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "setup refuses when the tarball would overwrite a file"
else
    fail "setup refuses when the tarball would overwrite a file" "out: $out"
fi
expect_file_eq "refusal leaves the pre-existing file untouched" "$T153/fake/README" "$ROOT/t153-readme"
case "$out" in
*"  README"*) ok "refusal names the offending path" ;;
*) fail "refusal names the offending path" "out: $out" ;;
esac
if [ ! -e "$T153/fake/src" ]; then
    ok "refusal leaks no tarball content into the directory"
else
    fail "refusal leaks no tarball content into the directory" \
         "ls: $(ls -A "$T153/fake" 2>&1)"
fi
if [ ! -f "$T153/.fake.projeny.status" ]; then
    ok "refusal writes no status file"
else
    fail "refusal writes no status file"
fi

# --------------------- 154. refusal: the patch would overwrite a file
# The same rule covers patch-added files (produced here by the real
# add/commit pipeline): a pre-existing file at a patch-add path refuses.
T154="$ROOT/t154"
make_tarballs "$T154" fake
write_projeny "$T154" fake 1.0 fake
(cd "$T154" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'added by the patch\n' > "$T154/fake/added.txt"
run_in "$T154" expect_ok "fixture: add the new file" "$PROJENY" add fake.projeny fake/added.txt
run_in "$T154" expect_ok "fixture: commit folds the add into the patch" "$PROJENY" commit fake.projeny
expect_file_contains "fixture: .projeny patch adds added.txt" "$T154/fake.projeny" "added.txt"
# A fresh directory holding only a file at the patch-add path.
rm -rf "$T154/fake" "$T154/.fake.projeny.status"
mkdir "$T154/fake"
printf 'local added\n' > "$T154/fake/added.txt"
printf 'local added\n' > "$ROOT/t154-added"
out="$(cd "$T154" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "setup refuses when the patch would overwrite a file"
else
    fail "setup refuses when the patch would overwrite a file" "out: $out"
fi
expect_file_eq "patch-add refusal leaves the pre-existing file untouched" "$T154/fake/added.txt" "$ROOT/t154-added"
case "$out" in
*"  added.txt"*) ok "patch-add refusal names the offending path" ;;
*) fail "patch-add refusal names the offending path" "out: $out" ;;
esac
if [ ! -e "$T154/fake/README" ]; then
    ok "patch-add refusal leaks no tarball content"
else
    fail "patch-add refusal leaks no tarball content"
fi
if [ ! -f "$T154/.fake.projeny.status" ]; then
    ok "patch-add refusal writes no status file"
else
    fail "patch-add refusal writes no status file"
fi

# ---------------- 155. refusal: a file sits where a directory is needed
T155="$ROOT/t155"
make_tarballs "$T155" fake
write_projeny "$T155" fake 1.0 fake
mkdir "$T155/fake"
printf 'not a directory\n' > "$T155/fake/src"
printf 'not a directory\n' > "$ROOT/t155-src"
out="$(cd "$T155" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "setup refuses when a file sits where a directory is needed"
else
    fail "setup refuses when a file sits where a directory is needed" "out: $out"
fi
expect_file_eq "file-for-directory refusal leaves the file untouched" "$T155/fake/src" "$ROOT/t155-src"
case "$out" in
*"  src"*) ok "file-for-directory refusal names the offending path" ;;
*) fail "file-for-directory refusal names the offending path" "out: $out" ;;
esac
if [ ! -e "$T155/fake/README" ] && [ ! -f "$T155/.fake.projeny.status" ]; then
    ok "file-for-directory refusal touches nothing else"
else
    fail "file-for-directory refusal touches nothing else" \
         "ls: $(ls -A "$T155/fake" 2>&1)"
fi

# ---------------- 156. refusal: a directory sits where a file is needed
T156="$ROOT/t156"
make_tarballs "$T156" fake
write_projeny "$T156" fake 1.0 fake
mkdir -p "$T156/fake/README"
printf 'inside\n' > "$T156/fake/README/inner"
out="$(cd "$T156" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "setup refuses when a directory sits where a file is needed"
else
    fail "setup refuses when a directory sits where a file is needed" "out: $out"
fi
if [ -d "$T156/fake/README" ] && [ "$(cat "$T156/fake/README/inner")" = "inside" ]; then
    ok "directory-for-file refusal leaves the directory untouched"
else
    fail "directory-for-file refusal leaves the directory untouched" \
         "ls: $(ls -A "$T156/fake" 2>&1)"
fi
case "$out" in
*"  README"*) ok "directory-for-file refusal names the offending path" ;;
*) fail "directory-for-file refusal names the offending path" "out: $out" ;;
esac
if [ ! -e "$T156/fake/src" ] && [ ! -f "$T156/.fake.projeny.status" ]; then
    ok "directory-for-file refusal touches nothing else"
else
    fail "directory-for-file refusal touches nothing else" \
         "ls: $(ls -A "$T156/fake" 2>&1)"
fi

# ------------------------- 157. foreign files persist across later setups
# After an into-existing-directory setup, the foreign files are ordinary
# untracked files: the next setup (status present, tracked files untouched)
# carries them along like user-added files.
T157="$ROOT/t157"
make_tarballs "$T157" fake
write_projeny "$T157" fake 1.0 fake
mkdir -p "$T157/fake/src" "$T157/fake/.hidden"
printf 'my notes\n' > "$T157/fake/notes.txt"
printf 'int extra = 1;\n' > "$T157/fake/src/extra.c"
printf 'hidden state\n' > "$T157/fake/.hidden/keep"
printf 'dot file\n' > "$T157/fake/.dotfile"
(cd "$T157" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T157" expect_ok "setup again after an into-existing setup exits 0" "$PROJENY" setup fake.projeny
expect_file_eq "re-setup keeps the top-level foreign file" "$T157/fake/notes.txt" "$ROOT/t152-notes"
expect_file_eq "re-setup keeps the nested foreign file" "$T157/fake/src/extra.c" "$ROOT/t152-extra"
expect_file_eq "re-setup keeps the hidden dir's file" "$T157/fake/.hidden/keep" "$ROOT/t152-keep"
expect_file_eq "re-setup keeps the hidden file" "$T157/fake/.dotfile" "$ROOT/t152-dot"
expect_file_contains "re-setup keeps the tarball content" "$T157/fake/README" "hello v1"
expect_file_contains "re-setup keeps the src content" "$T157/fake/src/a.c" "int alpha = 1;"

# ------------------- 158. refusal lists at most ten offending paths
# A 12-file tarball meeting 12 pre-existing files: the diagnostic caps the
# path list at ten with a "... and N more" tail, and still touches nothing.
T158="$ROOT/t158"
mkdir -p "$T158/fake-1.0"
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
    printf 'file %s\n' "$i" > "$T158/fake-1.0/f$i.c"
done
(cd "$T158" && tar -czf fake-1.0.tar.gz fake-1.0 && rm -rf fake-1.0)
printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    Many files.\n' > "$T158/fake.projeny"
mkdir "$T158/fake"
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
    printf 'mine %s\n' "$i" > "$T158/fake/f$i.c"
done
out="$(cd "$T158" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "setup refuses when a dozen files collide"
else
    fail "setup refuses when a dozen files collide" "out: $out"
fi
case "$out" in
*"... and 2 more"*) ok "refusal caps the path list at ten" ;;
*) fail "refusal caps the path list at ten" "out: $out" ;;
esac
if [ "$(cat "$T158/fake/f1.c")" = "mine 1" ] && [ "$(cat "$T158/fake/f12.c")" = "mine 12" ]; then
    ok "capped refusal leaves every file untouched"
else
    fail "capped refusal leaves every file untouched" \
         "ls: $(ls -A "$T158/fake" 2>&1)"
fi
if [ ! -f "$T158/.fake.projeny.status" ]; then
    ok "capped refusal writes no status file"
else
    fail "capped refusal writes no status file"
fi

# ----------------- 159. diff <f.projeny>: clean checkout and untracked files
# The projeny-file diff prints the uncommitted change against a fresh setup
# of the current .projeny file: nothing on a clean checkout, only registered
# changes otherwise, and untracked anything (text or binary) never appears.
T159="$ROOT/t159"
make_tarballs "$T159" fake
write_projeny "$T159" fake 1.0 fake
(cd "$T159" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
out="$(cd "$T159" && "$PROJENY" diff fake.projeny 2>"$T159/err")"
rc=$?
if [ $rc -eq 0 ] && [ -z "$out" ]; then
    ok "diff-projeny clean checkout prints nothing, exits 0"
else
    fail "diff-projeny clean checkout prints nothing, exits 0" \
         "rc=$rc out: $out"
fi
if [ ! -s "$T159/err" ]; then
    ok "diff-projeny clean checkout is silent on stderr too"
else
    fail "diff-projeny clean checkout is silent on stderr too" \
         "err: $(cat "$T159/err")"
fi
python3 - "$T159/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int alpha = 1;", "int alpha = 10;")
open(p, "w").write(s)
EOF
printf 'untracked text\n' > "$T159/fake/loose.txt"
python3 -c "open('$T159/fake/loose.bin','wb').write(b'un\x00tracked\n')"
out="$(cd "$T159" && "$PROJENY" diff fake.projeny 2>"$T159/err")"
rc=$?
if [ $rc -eq 0 ]; then
    ok "diff-projeny with edits exits 0"
else
    fail "diff-projeny with edits exits 0" "rc=$rc err: $(cat "$T159/err")"
fi
case "$out" in
*"+int alpha = 10;"*) ok "diff-projeny shows the tracked edit hunk" ;;
*) fail "diff-projeny shows the tracked edit hunk" "out: $out" ;;
esac
case "$out" in
*"loose.txt"*|*"loose.bin"*)
    fail "diff-projeny ignores untracked text and binary files" "out: $out"
    ;;
*) ok "diff-projeny ignores untracked text and binary files" ;;
esac

# --------------------------- 160. diff <f.projeny>: add folds in, tracked or not
# Only `projeny add`ed files count as additions: the added text file and the
# added binary file both appear, a second never-added file does not.
T160="$ROOT/t160"
make_tarballs "$T160" fake
write_projeny "$T160" fake 1.0 fake
(cd "$T160" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'brand new file\n' > "$T160/fake/src/added.c"
printf 'other new file\n' > "$T160/fake/src/stray.c"
python3 -c "open('$T160/fake/src/added.bin','wb').write(b'ab\x00cd\n')"
run_in "$T160" expect_ok "diff-projeny add marks text file" \
    "$PROJENY" add fake.projeny fake/src/added.c
run_in "$T160" expect_ok "diff-projeny add marks binary file" \
    "$PROJENY" add fake.projeny fake/src/added.bin
out="$(cd "$T160" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"new file mode"*) ok "diff-projeny shows new file mode for added file" ;;
*) fail "diff-projeny shows new file mode for added file" "out: $out" ;;
esac
case "$out" in
*"+brand new file"*) ok "diff-projeny shows added file content" ;;
*) fail "diff-projeny shows added file content" "out: $out" ;;
esac
case "$out" in
*"GIT binary patch"*) ok "diff-projeny carries added binary as binary block" ;;
*) fail "diff-projeny carries added binary as binary block" "out: $out" ;;
esac
case "$out" in
*"stray.c"*)
    fail "diff-projeny leaves never-added files out" "out: $out"
    ;;
*) ok "diff-projeny leaves never-added files out" ;;
esac

# --------------------------------------- 161. diff <f.projeny>: rm folds in
T161="$ROOT/t161"
make_tarballs "$T161" fake
write_projeny "$T161" fake 1.0 fake
(cd "$T161" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T161" expect_ok "diff-projeny rm marks deletion" \
    "$PROJENY" rm fake.projeny fake/src/b.c
out="$(cd "$T161" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"deleted file mode"*) ok "diff-projeny shows deleted file mode" ;;
*) fail "diff-projeny shows deleted file mode" "out: $out" ;;
esac
case "$out" in
*"-line one v1"*) ok "diff-projeny deletion block carries the old content" ;;
*) fail "diff-projeny deletion block carries the old content" "out: $out" ;;
esac

# ------------------------- 162. diff <f.projeny>: mv renders as rename
# An unchanged move is a pure rename block (no hunks); a move whose content
# diverged beyond the similarity threshold is STILL a rename block (forced
# pairing), with hunks carrying the edit.
T162="$ROOT/t162"
make_tarballs "$T162" fake
write_projeny "$T162" fake 1.0 fake
(cd "$T162" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T162" expect_ok "diff-projeny mv renames file" \
    "$PROJENY" mv fake.projeny fake/src/b.c fake/src/renamed.c
out="$(cd "$T162" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"rename from src/b.c"*) ok "diff-projeny mv shows rename from" ;;
*) fail "diff-projeny mv shows rename from" "out: $out" ;;
esac
case "$out" in
*"rename to src/renamed.c"*) ok "diff-projeny mv shows rename to" ;;
*) fail "diff-projeny mv shows rename to" "out: $out" ;;
esac
case "$out" in
*"@@ "*) fail "diff-projeny pure mv has no hunks" "out: $out" ;;
*) ok "diff-projeny pure mv has no hunks" ;;
esac
printf 'totally\nnew\ncontent\nhere\n' > "$T162/fake/src/renamed.c"
out="$(cd "$T162" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"rename from src/b.c"*)
    ok "diff-projeny divergent mv still renders as rename"
    ;;
*) fail "diff-projeny divergent mv still renders as rename" "out: $out" ;;
esac
case "$out" in
*"+totally"*) ok "diff-projeny divergent mv carries the edit hunks" ;;
*) fail "diff-projeny divergent mv carries the edit hunks" "out: $out" ;;
esac

# --------------------------- 163. diff <f.projeny>: chains and directories
T163="$ROOT/t163"
make_tarballs "$T163" fake
write_projeny "$T163" fake 1.0 fake
(cd "$T163" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
mkdir -p "$T163/fake/src/dir1"
printf 'one\n' > "$T163/fake/src/dir1/f1.c"
printf 'two\n' > "$T163/fake/src/dir1/f2.c"
run_in "$T163" expect_ok "diff-projeny add chain file one" \
    "$PROJENY" add fake.projeny fake/src/dir1/f1.c
run_in "$T163" expect_ok "diff-projeny add chain file two" \
    "$PROJENY" add fake.projeny fake/src/dir1/f2.c
run_in "$T163" expect_ok "diff-projeny commit dir content" \
    "$PROJENY" commit fake.projeny
run_in "$T163" expect_ok "diff-projeny mv chain step one" \
    "$PROJENY" mv fake.projeny fake/src/a.c fake/src/mid.c
run_in "$T163" expect_ok "diff-projeny mv chain step two" \
    "$PROJENY" mv fake.projeny fake/src/mid.c fake/src/end.c
out="$(cd "$T163" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"rename from src/a.c"*)
    ok "diff-projeny chained mv starts at the original path"
    ;;
*) fail "diff-projeny chained mv starts at the original path" "out: $out" ;;
esac
case "$out" in
*"rename to src/end.c"*) ok "diff-projeny chained mv ends at the final path" ;;
*) fail "diff-projeny chained mv ends at the final path" "out: $out" ;;
esac
case "$out" in
*"mid.c"*)
    fail "diff-projeny chained mv never mentions the middle name" "out: $out"
    ;;
*) ok "diff-projeny chained mv never mentions the middle name" ;;
esac
run_in "$T163" expect_ok "diff-projeny mv whole directory" \
    "$PROJENY" mv fake.projeny fake/src/dir1 fake/src/dir2
out="$(cd "$T163" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"rename from src/dir1/f1.c"*"rename to src/dir2/f1.c"*)
    ok "diff-projeny dir mv renames the contained files"
    ;;
*) fail "diff-projeny dir mv renames the contained files" "out: $out" ;;
esac
case "$out" in
*"rename to src/dir2/f2.c"*) ok "diff-projeny dir mv covers every file" ;;
*) fail "diff-projeny dir mv covers every file" "out: $out" ;;
esac

# ----------------- 164. diff <f.projeny>: disappeared files warn, never diff
# A tracked file deleted without `projeny rm` is not a deletion: the diff
# stays empty for it, warns on stderr, and a similar untracked file cannot
# smuggle a bogus rename block in. Registering the deletion makes it appear.
T164="$ROOT/t164"
make_tarballs "$T164" fake
write_projeny "$T164" fake 1.0 fake
(cd "$T164" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
rm "$T164/fake/src/a.c"
printf 'int alpha = 1;\n\nint beta = 77;\n' > "$T164/fake/src/moved.c"
out="$(cd "$T164" && "$PROJENY" diff fake.projeny 2>"$T164/err")"
rc=$?
if [ $rc -eq 0 ]; then
    ok "diff-projeny with disappeared file still exits 0"
else
    fail "diff-projeny with disappeared file still exits 0" \
         "rc=$rc out: $out"
fi
if [ -z "$out" ]; then
    ok "diff-projeny prints nothing for the disappeared file"
else
    fail "diff-projeny prints nothing for the disappeared file" "out: $out"
fi
case "$(cat "$T164/err")" in
*"'src/a.c' was removed locally but is not marked with"*)
    ok "diff-projeny warns naming the disappeared file"
    ;;
*) fail "diff-projeny warns naming the disappeared file" \
        "err: $(cat "$T164/err")" ;;
esac
case "$out" in
*"rename from"*|*"moved.c"*)
    fail "diff-projeny no bogus rename for disappeared+similar untracked" \
         "out: $out"
    ;;
*) ok "diff-projeny no bogus rename for disappeared+similar untracked" ;;
esac
# The warning names the exact command that registers the deletion, with the
# path spelled wid-prefixed ("fake/src/a.c") so the command works from any
# CWD: add/rm/mv resolve that form lexically, while the bare workdir-
# relative spelling dies with "outside the workdir" unless the CWD happens
# to be the workdir. Re-run the diff from $ROOT — deliberately a different
# directory than the pdir — with an explicit .projeny path to capture the
# suggestion, then execute it verbatim ("$PROJENY" stands in for the
# leading "projeny" word; unquoted $cmd word-splits the arguments).
out="$(cd "$ROOT" && "$PROJENY" diff "$T164/fake.projeny" \
    2>"$ROOT/t164-suggested.err")"
rc=$?
if [ $rc -eq 0 ]; then
    ok "diff-projeny diff works from an unrelated CWD"
else
    fail "diff-projeny diff works from an unrelated CWD" \
         "rc=$rc out: $out err: $(cat "$ROOT/t164-suggested.err")"
fi
cmd="$(grep "not marked with" "$ROOT/t164-suggested.err" |
    sed -e "s/.*not marked with 'projeny //" \
        -e "s/'; it will not appear in the diff\$//")"
case "$cmd" in
*" fake/src/a.c"*)
    ok "diff-projeny suggested command uses the wid-prefixed path"
    ;;
*) fail "diff-projeny suggested command uses the wid-prefixed path" \
        "cmd: $cmd" ;;
esac
run_in "$ROOT" expect_ok "diff-projeny suggested rm works verbatim from any CWD" \
    "$PROJENY" $cmd
out="$(cd "$T164" && "$PROJENY" diff fake.projeny 2>"$T164/err")"
case "$out" in
*"deleted file mode"*"src/a.c"*)
    ok "diff-projeny registered deletion appears"
    ;;
*) fail "diff-projeny registered deletion appears" "out: $out" ;;
esac
if [ ! -s "$T164/err" ]; then
    ok "diff-projeny registered deletion warns nothing"
else
    fail "diff-projeny registered deletion warns nothing" \
         "err: $(cat "$T164/err")"
fi

# ------------------------------- 165. diff <f.projeny>: add-then-mv, refusals
T165="$ROOT/t165"
make_tarballs "$T165" fake
write_projeny "$T165" fake 1.0 fake
(cd "$T165" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'fresh file\n' > "$T165/fake/src/newfile.c"
run_in "$T165" expect_ok "diff-projeny add pending file" \
    "$PROJENY" add fake.projeny fake/src/newfile.c
run_in "$T165" expect_ok "diff-projeny mv the pending-added file" \
    "$PROJENY" mv fake.projeny fake/src/newfile.c fake/src/final.c
out="$(cd "$T165" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"new file mode"*"src/final.c"*)
    ok "diff-projeny added-then-moved file shows under final name"
    ;;
*) fail "diff-projeny added-then-moved file shows under final name" \
        "out: $out" ;;
esac
case "$out" in
*"newfile.c"*)
    fail "diff-projeny added-then-moved file shows once" "out: $out"
    ;;
*) ok "diff-projeny added-then-moved file shows once" ;;
esac
# Refusal: the .projeny file differs from the status copy.
printf '\n    Extra prose.\n' >> "$T165/fake.projeny"
out="$(cd "$T165" && "$PROJENY" diff fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "diff-projeny refuses a changed .projeny file"
else
    fail "diff-projeny refuses a changed .projeny file" "out: $out"
fi
case "$out" in
*"run setup to merge first"*)
    ok "diff-projeny changed-.projeny error mentions setup"
    ;;
*) fail "diff-projeny changed-.projeny error mentions setup" "out: $out" ;;
esac
printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    Fake project fake for projeny tests.\n\n' > "$T165/fake.projeny"
run_in "$T165" expect_ok "diff-projeny restored .projeny setup" \
    "$PROJENY" setup fake.projeny
# Refusal: unresolved conflicts recorded in the status file.
sed -i 's/^--- projeny content ---$/Conflict: src\/a.c\n--- projeny content ---/' \
    "$T165/.fake.projeny.status"
out="$(cd "$T165" && "$PROJENY" diff fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "diff-projeny refuses with unresolved conflicts"
else
    fail "diff-projeny refuses with unresolved conflicts" "out: $out"
fi
case "$out" in
*"cannot diff with unresolved conflicts"*)
    ok "diff-projeny conflict error names the problem"
    ;;
*) fail "diff-projeny conflict error names the problem" "out: $out" ;;
esac
# Refusal: a pending add whose file is gone from the workdir. The injected
# conflict is dropped with `resolve` (setup would union it right back in).
run_in "$T165" expect_ok "diff-projeny resolve the injected conflict" \
    "$PROJENY" resolve fake.projeny src/a.c
rm "$T165/fake/src/final.c"
out="$(cd "$T165" && "$PROJENY" diff fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "diff-projeny refuses a pending add with a missing file"
else
    fail "diff-projeny refuses a pending add with a missing file" "out: $out"
fi
case "$out" in
*"pending add 'src/final.c' does not exist"*)
    ok "diff-projeny pending-add error names the file"
    ;;
*) fail "diff-projeny pending-add error names the file" "out: $out" ;;
esac

# --------------------- 166. diff <f.projeny>: roundtrip through projeny patch
# The diff of one checkout applies to a fresh checkout of the same .projeny
# file and reproduces the first workdir exactly (tracked edit, add, rm, mv).
T166="$ROOT/t166"
mkdir -p "$T166/a" "$T166/b"
for side in a b; do
    make_tarballs "$T166/$side" fake
    write_projeny "$T166/$side" fake 1.0 fake
done
(cd "$T166/a" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T166/a/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 5;")
open(p, "w").write(s)
EOF
printf 'carried file\n' > "$T166/a/fake/src/carried.c"
run_in "$T166/a" expect_ok "roundtrip add carried file" \
    "$PROJENY" add fake.projeny fake/src/carried.c
run_in "$T166/a" expect_ok "roundtrip rm src/b.c" \
    "$PROJENY" rm fake.projeny fake/src/b.c
run_in "$T166/a" expect_ok "roundtrip mv README" \
    "$PROJENY" mv fake.projeny fake/README fake/README.md
(cd "$T166/a" && "$PROJENY" diff fake.projeny > "$T166/uncommitted.patch" 2>"$T166/err")
if [ -s "$T166/uncommitted.patch" ]; then
    ok "roundtrip diff is nonempty"
else
    fail "roundtrip diff is nonempty" "err: $(cat "$T166/err")"
fi
(cd "$T166/b" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T166/b" expect_ok "roundtrip patch applies to fresh checkout" \
    "$PROJENY" patch fake "$T166/uncommitted.patch"
out="$(cd "$T166/a" && "$PROJENY" diff fake ../b/fake 2>&1)"
if [ -z "$out" ]; then
    ok "roundtrip reproduced the workdir exactly"
else
    fail "roundtrip reproduced the workdir exactly" "2-dir diff: $out"
fi

# ------------------ 167. commit honors forced renames, even over renames
# A divergent move commits as a rename block (not delete+add), and a move of
# an already-committed rename resolves back to the ORIGINAL archive path so
# the stored patch still renames the tarball's file.
T167="$ROOT/t167"
make_tarballs "$T167" fake
write_projeny "$T167" fake 1.0 fake
(cd "$T167" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T167" expect_ok "commit-rename mv first" \
    "$PROJENY" mv fake.projeny fake/src/a.c fake/src/renamed.c
printf 'divergent\ncontent\nentirely\n' > "$T167/fake/src/renamed.c"
run_in "$T167" expect_ok "commit-rename commit succeeds" \
    "$PROJENY" commit fake.projeny
expect_file_contains "commit stores forced rename from" \
    "$T167/fake.projeny" "rename from src/a.c"
expect_file_contains "commit stores forced rename to" \
    "$T167/fake.projeny" "rename to src/renamed.c"
rm -rf "$T167/fake" "$T167/.fake.projeny.status"
run_in "$T167" expect_ok "commit-rename fresh setup" "$PROJENY" setup fake.projeny
expect_file_contains "commit-rename setup reproduces content" \
    "$T167/fake/src/renamed.c" "divergent"
if [ ! -e "$T167/fake/src/a.c" ]; then
    ok "commit-rename setup keeps the old path gone"
else
    fail "commit-rename setup keeps the old path gone"
fi
# Committed-rename resolution: rename again, on top of the committed rename.
printf 'final\ndivergent\nbytes\n' > "$T167/fake/src/renamed.c"
run_in "$T167" expect_ok "commit-rename second mv" \
    "$PROJENY" mv fake.projeny fake/src/renamed.c fake/src/final.c
run_in "$T167" expect_ok "commit-rename second commit" \
    "$PROJENY" commit fake.projeny
expect_file_contains "re-commit renames the original archive path" \
    "$T167/fake.projeny" "rename from src/a.c"
expect_file_contains "re-commit renames to the final path" \
    "$T167/fake.projeny" "rename to src/final.c"
if grep -q "rename to src/renamed.c" "$T167/fake.projeny"; then
    fail "re-commit drops the intermediate name" "$(grep rename "$T167/fake.projeny")"
else
    ok "re-commit drops the intermediate name"
fi
rm -rf "$T167/fake" "$T167/.fake.projeny.status"
run_in "$T167" expect_ok "commit-rename setup of the twice-renamed patch" \
    "$PROJENY" setup fake.projeny
expect_file_contains "twice-renamed setup reproduces final content" \
    "$T167/fake/src/final.c" "final"
if [ ! -e "$T167/fake/src/renamed.c" ] && [ ! -e "$T167/fake/src/a.c" ]; then
    ok "twice-renamed setup leaves only the final path"
else
    fail "twice-renamed setup leaves only the final path" \
         "ls: $(ls "$T167/fake/src" 2>&1)"
fi

# ------------------------- 168. diff <f.projeny> after a committed add
# Once an add is committed the file is tracked: an edit shows as a plain
# modify block, with no add block (and untracked files still stay out).
T168="$ROOT/t168"
make_tarballs "$T168" fake
write_projeny "$T168" fake 1.0 fake
(cd "$T168" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'committed add\n' > "$T168/fake/src/added.c"
run_in "$T168" expect_ok "post-commit-add add file" \
    "$PROJENY" add fake.projeny fake/src/added.c
run_in "$T168" expect_ok "post-commit-add commit" "$PROJENY" commit fake.projeny
printf 'committed add, now edited\n' > "$T168/fake/src/added.c"
printf 'still untracked\n' > "$T168/fake/src/stray.c"
out="$(cd "$T168" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"new file mode"*)
    fail "diff-projeny after committed add has no add block" "out: $out"
    ;;
*) ok "diff-projeny after committed add has no add block" ;;
esac
case "$out" in
*"+committed add, now edited"*)
    ok "diff-projeny after committed add shows the modify hunk"
    ;;
*) fail "diff-projeny after committed add shows the modify hunk" "out: $out" ;;
esac
case "$out" in
*"stray.c"*)
    fail "diff-projeny after committed add ignores untracked" "out: $out"
    ;;
*) ok "diff-projeny after committed add ignores untracked" ;;
esac

# --------- 169. diff vs commit: the two baselines differ for renamed work
# The diff is relative to the checked-in tree (tarball + current patch,
# what a fresh setup would produce); the stored patch is relative to the
# raw tarball. For ordinary edits they are the same patch. They differ
# when a pending mv renames a file the last commit itself added or
# renamed: the diff describes the move against the checked-in paths, while
# commit re-derives the rename from the tarball's paths — a committed-
# added file commits as a plain add of its new name, and a committed
# rename re-traces to the original tarball path. Each output is correct
# for its own baseline.
T169="$ROOT/t169"
make_tarballs "$T169" fake
write_projeny "$T169" fake 1.0 fake
(cd "$T169" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'brand new\n' > "$T169/fake/src/added.c"
run_in "$T169" expect_ok "diff-vs-commit add the new file" \
    "$PROJENY" add fake.projeny fake/src/added.c
run_in "$T169" expect_ok "diff-vs-commit commit the add" \
    "$PROJENY" commit fake.projeny
run_in "$T169" expect_ok "diff-vs-commit mv the committed-added file" \
    "$PROJENY" mv fake.projeny fake/src/added.c fake/src/moved.c
out="$(cd "$T169" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"rename from src/added.c"*)
    ok "diff-vs-commit diff describes the mv against the checked-in path"
    ;;
*) fail "diff-vs-commit diff describes the mv against the checked-in path" \
        "out: $out" ;;
esac
case "$out" in
*"rename to src/moved.c"*)
    ok "diff-vs-commit diff names the moved-to path"
    ;;
*) fail "diff-vs-commit diff names the moved-to path" "out: $out" ;;
esac
run_in "$T169" expect_ok "diff-vs-commit commit the mv" \
    "$PROJENY" commit fake.projeny
expect_file_contains "diff-vs-commit commit stores a plain add" \
    "$T169/fake.projeny" "new file mode"
expect_file_contains "diff-vs-commit commit stores the moved-to path" \
    "$T169/fake.projeny" "src/moved.c"
if grep -q "rename from" "$T169/fake.projeny"; then
    fail "diff-vs-commit commit stores no rename for a committed-added file" \
         "$(grep -E '^(rename|new file|deleted file)' "$T169/fake.projeny")"
else
    ok "diff-vs-commit commit stores no rename for a committed-added file"
fi
rm -rf "$T169/fake" "$T169/.fake.projeny.status"
run_in "$T169" expect_ok "diff-vs-commit fresh setup" \
    "$PROJENY" setup fake.projeny
expect_file_contains "diff-vs-commit setup reproduces the moved content" \
    "$T169/fake/src/moved.c" "brand new"
if [ ! -e "$T169/fake/src/added.c" ]; then
    ok "diff-vs-commit setup keeps the pre-move name gone"
else
    fail "diff-vs-commit setup keeps the pre-move name gone"
fi
# Second scenario: a committed rename, moved again. The diff describes the
# second move against the committed path; commit re-traces to the original
# tarball path.
run_in "$T169" expect_ok "diff-vs-commit mv the original file" \
    "$PROJENY" mv fake.projeny fake/src/a.c fake/src/r1.c
run_in "$T169" expect_ok "diff-vs-commit commit the rename" \
    "$PROJENY" commit fake.projeny
run_in "$T169" expect_ok "diff-vs-commit mv the committed rename" \
    "$PROJENY" mv fake.projeny fake/src/r1.c fake/src/r2.c
out="$(cd "$T169" && "$PROJENY" diff fake.projeny 2>&1)"
case "$out" in
*"rename from src/r1.c"*)
    ok "diff-vs-commit diff describes the second mv against the committed path"
    ;;
*) fail "diff-vs-commit diff describes the second mv against the committed path" \
        "out: $out" ;;
esac
case "$out" in
*"rename to src/r2.c"*)
    ok "diff-vs-commit diff names the final path"
    ;;
*) fail "diff-vs-commit diff names the final path" "out: $out" ;;
esac
case "$out" in
*"rename from src/a.c"*)
    fail "diff-vs-commit diff never mentions the tarball path" "out: $out"
    ;;
*) ok "diff-vs-commit diff never mentions the tarball path" ;;
esac
run_in "$T169" expect_ok "diff-vs-commit commit the second mv" \
    "$PROJENY" commit fake.projeny
expect_file_contains "diff-vs-commit commit re-traces to the tarball path" \
    "$T169/fake.projeny" "rename from src/a.c"
expect_file_contains "diff-vs-commit commit renames to the final path" \
    "$T169/fake.projeny" "rename to src/r2.c"
rm -rf "$T169/fake" "$T169/.fake.projeny.status"
run_in "$T169" expect_ok "diff-vs-commit setup of the re-traced patch" \
    "$PROJENY" setup fake.projeny
expect_file_contains "diff-vs-commit re-traced setup reproduces content" \
    "$T169/fake/src/r2.c" "int alpha = 1;"
if [ ! -e "$T169/fake/src/a.c" ] && [ ! -e "$T169/fake/src/r1.c" ]; then
    ok "diff-vs-commit re-traced setup leaves the old names gone"
else
    fail "diff-vs-commit re-traced setup leaves the old names gone" \
         "ls: $(ls "$T169/fake/src" 2>&1)"
fi

# ------------ 170. dir mv: un-rm'd inner deletion, untracked riders
# A plain `rm` of a file inside a directory that a pending `mv` moved is
# NOT part of the move: the diff drops its deletion block and warns like
# the plain case, and commit refuses until `projeny rm` registers the
# deletion (rm records the moved-to path). An untracked file created under
# the move destination rides along in the diff (the destination is a
# registered add-side entry) and in the commit. Here the inner files are
# committed adds, so commit's raw-tarball baseline never had them: the
# stored patch carries plain adds of the new names (the diff-vs-commit
# baseline rule), never renames or deletions of the checked-in paths.
T170="$ROOT/t170"
make_tarballs "$T170" fake
write_projeny "$T170" fake 1.0 fake
(cd "$T170" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
mkdir "$T170/fake/src/dir1"
printf 'one\n' > "$T170/fake/src/dir1/f1.c"
printf 'two\n' > "$T170/fake/src/dir1/f2.c"
run_in "$T170" expect_ok "dir-mv-rm add inner file one" \
    "$PROJENY" add fake.projeny fake/src/dir1/f1.c
run_in "$T170" expect_ok "dir-mv-rm add inner file two" \
    "$PROJENY" add fake.projeny fake/src/dir1/f2.c
run_in "$T170" expect_ok "dir-mv-rm commit the directory" \
    "$PROJENY" commit fake.projeny
run_in "$T170" expect_ok "dir-mv-rm mv the directory" \
    "$PROJENY" mv fake.projeny fake/src/dir1 fake/src/dir2
printf 'untracked rider\n' > "$T170/fake/src/dir2/new.c"
rm "$T170/fake/src/dir2/f1.c"
out="$(cd "$T170" && "$PROJENY" diff fake.projeny 2>"$T170/err")"
rc=$?
if [ $rc -eq 0 ]; then
    ok "dir-mv-rm diff exits 0"
else
    fail "dir-mv-rm diff exits 0" "rc=$rc out: $out"
fi
case "$out" in
*"rename from src/dir1/f2"*)
    ok "dir-mv-rm diff renames the surviving inner file"
    ;;
*) fail "dir-mv-rm diff renames the surviving inner file" "out: $out" ;;
esac
case "$out" in
*"rename to src/dir2/f2"*)
    ok "dir-mv-rm diff names the moved destination"
    ;;
*) fail "dir-mv-rm diff names the moved destination" "out: $out" ;;
esac
case "$out" in
*"deleted file mode"*)
    fail "dir-mv-rm diff leaves the un-rm'd inner deletion out" "out: $out"
    ;;
*) ok "dir-mv-rm diff leaves the un-rm'd inner deletion out" ;;
esac
case "$out" in
*"new file mode"*"src/dir2/new.c"*)
    ok "dir-mv-rm diff shows the untracked rider under the destination"
    ;;
*) fail "dir-mv-rm diff shows the untracked rider under the destination" \
        "out: $out" ;;
esac
case "$(cat "$T170/err")" in
*"'src/dir1/f1.c' was removed locally but is not marked with"*)
    ok "dir-mv-rm diff warns about the un-rm'd inner deletion"
    ;;
*) fail "dir-mv-rm diff warns about the un-rm'd inner deletion" \
        "err: $(cat "$T170/err")" ;;
esac
out="$(cd "$T170" && "$PROJENY" commit fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "dir-mv-rm commit refuses while the deletion is unregistered"
else
    fail "dir-mv-rm commit refuses while the deletion is unregistered" \
         "out: $out"
fi
case "$out" in
*"cannot commit with disappeared files"*)
    ok "dir-mv-rm refusal names the disappeared-file error"
    ;;
*) fail "dir-mv-rm refusal names the disappeared-file error" "out: $out" ;;
esac
case "$out" in
*"src/dir1/f1"*)
    ok "dir-mv-rm refusal names the vanished inner file"
    ;;
*) fail "dir-mv-rm refusal names the vanished inner file" "out: $out" ;;
esac
run_in "$T170" expect_ok "dir-mv-rm rm registers the moved-to path" \
    "$PROJENY" rm fake.projeny fake/src/dir2/f1.c
run_in "$T170" expect_ok "dir-mv-rm commit after rm" \
    "$PROJENY" commit fake.projeny
expect_file_contains "dir-mv-rm commit stores the survivor under its new name" \
    "$T170/fake.projeny" "new file mode"
expect_file_contains "dir-mv-rm commit stores the moved-to path" \
    "$T170/fake.projeny" "src/dir2/f2.c"
expect_file_contains "dir-mv-rm commit carries the untracked rider" \
    "$T170/fake.projeny" "src/dir2/new.c"
if grep -q "rename from" "$T170/fake.projeny" || \
   grep -q "deleted file mode" "$T170/fake.projeny"; then
    fail "dir-mv-rm commit stores plain adds (the tarball never had the dir)" \
         "$(grep -E '^(rename|new file|deleted file)' "$T170/fake.projeny")"
else
    ok "dir-mv-rm commit stores plain adds (the tarball never had the dir)"
fi
rm -rf "$T170/fake" "$T170/.fake.projeny.status"
run_in "$T170" expect_ok "dir-mv-rm fresh setup" "$PROJENY" setup fake.projeny
if [ -f "$T170/fake/src/dir2/f2.c" ]; then
    ok "dir-mv-rm setup restores the surviving inner file"
else
    fail "dir-mv-rm setup restores the surviving inner file"
fi
expect_file_contains "dir-mv-rm setup restores the committed rider" \
    "$T170/fake/src/dir2/new.c" "untracked rider"
if [ ! -e "$T170/fake/src/dir1" ] && [ ! -e "$T170/fake/src/dir2/f1.c" ]; then
    ok "dir-mv-rm setup keeps the old dir and deleted file gone"
else
    fail "dir-mv-rm setup keeps the old dir and deleted file gone" \
         "ls: $(ls -A "$T170/fake/src" 2>&1)"
fi

# ------- 171. dir mv with the directory shipped in the tarball
# The same un-rm'd-inner-deletion flow, but with dir1 present in the raw
# tarball: now commit's baseline really does contain the checked-in paths,
# so after `projeny rm` registers the deletion the stored patch carries a
# rename of the tarball's file plus a deleted-file block — while before
# the rm the deletion is warned out of the diff and commit refuses.
T171="$ROOT/t171"
mkdir -p "$T171/fake-1.0/src/dir1"
printf 'int alpha = 1;\n' > "$T171/fake-1.0/src/a.c"
printf 'one\n' > "$T171/fake-1.0/src/dir1/f1.c"
printf 'two\n' > "$T171/fake-1.0/src/dir1/f2.c"
(cd "$T171" && tar -czf fake-1.0.tar.gz fake-1.0 && rm -rf fake-1.0)
printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    Fake project fake for projeny tests.\n' > "$T171/fake.projeny"
(cd "$T171" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T171" expect_ok "tarball-dir mv the directory" \
    "$PROJENY" mv fake.projeny fake/src/dir1 fake/src/dir2
rm "$T171/fake/src/dir2/f1.c"
out="$(cd "$T171" && "$PROJENY" diff fake.projeny 2>"$T171/err")"
rc=$?
if [ $rc -eq 0 ]; then
    ok "tarball-dir diff exits 0"
else
    fail "tarball-dir diff exits 0" "rc=$rc out: $out"
fi
case "$out" in
*"rename from src/dir1/f2"*)
    ok "tarball-dir diff renames the surviving inner file"
    ;;
*) fail "tarball-dir diff renames the surviving inner file" "out: $out" ;;
esac
case "$out" in
*"deleted file mode"*)
    fail "tarball-dir diff leaves the un-rm'd inner deletion out" "out: $out"
    ;;
*) ok "tarball-dir diff leaves the un-rm'd inner deletion out" ;;
esac
case "$(cat "$T171/err")" in
*"'src/dir1/f1.c' was removed locally but is not marked with"*)
    ok "tarball-dir diff warns about the un-rm'd inner deletion"
    ;;
*) fail "tarball-dir diff warns about the un-rm'd inner deletion" \
        "err: $(cat "$T171/err")" ;;
esac
out="$(cd "$T171" && "$PROJENY" commit fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "tarball-dir commit refuses while the deletion is unregistered"
else
    fail "tarball-dir commit refuses while the deletion is unregistered" \
         "out: $out"
fi
case "$out" in
*"cannot commit with disappeared files"*)
    ok "tarball-dir refusal names the disappeared-file error"
    ;;
*) fail "tarball-dir refusal names the disappeared-file error" "out: $out" ;;
esac
run_in "$T171" expect_ok "tarball-dir rm registers the moved-to path" \
    "$PROJENY" rm fake.projeny fake/src/dir2/f1.c
out="$(cd "$T171" && "$PROJENY" diff fake.projeny 2>"$T171/err")"
case "$out" in
*"deleted file mode"*"src/dir1/f1"*)
    ok "tarball-dir registered deletion appears against the original path"
    ;;
*) fail "tarball-dir registered deletion appears against the original path" \
        "out: $out" ;;
esac
if [ -s "$T171/err" ]; then
    fail "tarball-dir registered deletion warns nothing" \
         "err: $(cat "$T171/err")"
else
    ok "tarball-dir registered deletion warns nothing"
fi
run_in "$T171" expect_ok "tarball-dir commit after rm" \
    "$PROJENY" commit fake.projeny
expect_file_contains "tarball-dir commit renames the tarball's file" \
    "$T171/fake.projeny" "rename from src/dir1/f2.c"
expect_file_contains "tarball-dir commit names the moved destination" \
    "$T171/fake.projeny" "rename to src/dir2/f2.c"
expect_file_contains "tarball-dir commit stores the registered deletion" \
    "$T171/fake.projeny" "deleted file mode"
expect_file_contains "tarball-dir commit deletes the tarball's path" \
    "$T171/fake.projeny" "src/dir1/f1.c"
rm -rf "$T171/fake" "$T171/.fake.projeny.status"
run_in "$T171" expect_ok "tarball-dir fresh setup" "$PROJENY" setup fake.projeny
if [ -f "$T171/fake/src/dir2/f2.c" ] && \
   [ ! -e "$T171/fake/src/dir1/f1.c" ] && \
   [ ! -e "$T171/fake/src/dir1/f2.c" ] && \
   [ ! -e "$T171/fake/src/dir2/f1.c" ]; then
    ok "tarball-dir setup reproduces the moved+pruned directory"
else
    fail "tarball-dir setup reproduces the moved+pruned directory" \
         "ls: $(ls -A "$T171/fake/src" "$T171/fake/src/dir1" \
               "$T171/fake/src/dir2" 2>&1)"
fi

# ------------- 172. binary mv with a payload: renames must carry the bytes
# A moved binary whose content changed commits as a rename block carrying a
# `GIT binary patch` (new bytes first, then old). The binary ships in the
# tarball so the stored patch really is a rename-with-payload block: a
# committed-ADDED binary re-derives as a plain add on commit (see 169), and
# an add block never touches the rename path. Applying such a block must
# write the NEW bytes at the destination — the applier once took a pure-
# rename shortcut that silently kept the OLD bytes (fresh setup after
# `rm -rf`, and diff->patch onto a second checkout, both lost the edit with
# no error anywhere). Also pins the payload-free case: a pure binary mv
# still takes the move-only fast path (no payload, bytes untouched).
T172="$ROOT/t172"
mkdir -p "$T172/a" "$T172/b" "$T172/c"
for side in a b c; do
    mkdir -p "$T172/$side/fake-1.0/src"
    printf 'int alpha = 1;\n' > "$T172/$side/fake-1.0/src/a.c"
    printf 'line one v1\n' > "$T172/$side/fake-1.0/src/b.c"
    printf 'hello v1\n' > "$T172/$side/fake-1.0/README"
    # Content A: NUL-bearing. keep.bin never changes (payload-free rename).
    python3 -c \
        "open('$T172/$side/fake-1.0/src/blob.bin','wb').write(b'old\x00\x00\x00blob\x00A\x00payload\n')"
    python3 -c "open('$T172/$side/fake-1.0/src/keep.bin','wb').write(b'keep\x00\x00me\n')"
    (cd "$T172/$side" && tar -czf fake-1.0.tar.gz fake-1.0 && rm -rf fake-1.0)
    printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    Fake project fake for projeny tests.\n' > "$T172/$side/fake.projeny"
done
# Reference bytes: content B differs in length and NUL placement from A.
python3 -c \
    "open('$T172/blob-B.bin','wb').write(b'\x00\x00brand\x00new\x00blob-B payload with a different length\n')"
python3 -c "open('$T172/keep-C.bin','wb').write(b'keep\x00\x00me\n')"
run_in "$T172/a" expect_ok "bin-rename checkout setup" \
    "$PROJENY" setup fake.projeny
run_in "$T172/a" expect_ok "bin-rename mv the tracked binary" \
    "$PROJENY" mv fake.projeny fake/src/blob.bin fake/src/blob2.bin
cp "$T172/blob-B.bin" "$T172/a/fake/src/blob2.bin"
(cd "$T172/a" && "$PROJENY" diff fake.projeny > "$T172/uncommitted.patch" \
    2>"$T172/a/err")
rc=$?
if [ $rc -eq 0 ] && [ -s "$T172/uncommitted.patch" ]; then
    ok "bin-rename diff exits 0 with the pending rename"
else
    fail "bin-rename diff exits 0 with the pending rename" \
         "rc=$rc err: $(cat "$T172/a/err")"
fi
expect_file_contains "bin-rename diff carries rename from src/blob.bin" \
    "$T172/uncommitted.patch" "rename from src/blob.bin"
expect_file_contains "bin-rename diff carries rename to src/blob2.bin" \
    "$T172/uncommitted.patch" "rename to src/blob2.bin"
expect_file_contains "bin-rename diff carries the binary payload" \
    "$T172/uncommitted.patch" "GIT binary patch"
# Roundtrip: apply the diff to a second checkout of the same .projeny file.
run_in "$T172/b" expect_ok "bin-rename second checkout setup" \
    "$PROJENY" setup fake.projeny
run_in "$T172/b" expect_ok "bin-rename patch applies the rename payload" \
    "$PROJENY" patch fake "$T172/uncommitted.patch"
if cmp -s "$T172/b/fake/src/blob2.bin" "$T172/blob-B.bin"; then
    ok "bin-rename patch writes the new bytes at the destination"
else
    fail "bin-rename patch writes the new bytes at the destination" \
         "got: $(python3 -c "print(open('$T172/b/fake/src/blob2.bin','rb').read())" 2>&1)"
fi
if [ ! -e "$T172/b/fake/src/blob.bin" ]; then
    ok "bin-rename patch removes the source path"
else
    fail "bin-rename patch removes the source path"
fi
expect_file_eq "bin-rename patch leaves payload-free binaries untouched" \
    "$T172/b/fake/src/keep.bin" "$T172/keep-C.bin"
run_in "$T172/a" expect_ok "bin-rename commit" "$PROJENY" commit fake.projeny
expect_file_contains "bin-rename commit stores rename from src/blob.bin" \
    "$T172/a/fake.projeny" "rename from src/blob.bin"
expect_file_contains "bin-rename commit stores rename to src/blob2.bin" \
    "$T172/a/fake.projeny" "rename to src/blob2.bin"
expect_file_contains "bin-rename commit stores the binary payload" \
    "$T172/a/fake.projeny" "GIT binary patch"
# The bug: a fresh setup must restore the NEW bytes, not just move the old
# file. Wipe workdir and status, then set up from the committed patch.
rm -rf "$T172/a/fake" "$T172/a/.fake.projeny.status"
run_in "$T172/a" expect_ok "bin-rename fresh setup after wipe" \
    "$PROJENY" setup fake.projeny
if cmp -s "$T172/a/fake/src/blob2.bin" "$T172/blob-B.bin"; then
    ok "bin-rename setup restores the new bytes at the destination"
else
    fail "bin-rename setup restores the new bytes at the destination" \
         "got: $(python3 -c "print(open('$T172/a/fake/src/blob2.bin','rb').read())" 2>&1)"
fi
if [ ! -e "$T172/a/fake/src/blob.bin" ]; then
    ok "bin-rename setup keeps the source path gone"
else
    fail "bin-rename setup keeps the source path gone"
fi
expect_file_eq "bin-rename setup leaves payload-free binaries untouched" \
    "$T172/a/fake/src/keep.bin" "$T172/keep-C.bin"
# Idempotence: the second setup takes the already-applied path (source
# missing, destination holds the new bytes) and must change nothing.
run_in "$T172/a" expect_ok "bin-rename setup twice is a no-op" \
    "$PROJENY" setup fake.projeny
expect_file_eq "bin-rename setup twice keeps the new bytes" \
    "$T172/a/fake/src/blob2.bin" "$T172/blob-B.bin"
# Payload-free case in isolation: a pure binary mv (identical bytes) emits
# no payload, commits as a bare rename block, and a fresh setup reproduces
# the identical bytes at the new path via the move-only fast path.
run_in "$T172/c" expect_ok "bin-rename payload-free checkout setup" \
    "$PROJENY" setup fake.projeny
run_in "$T172/c" expect_ok "bin-rename payload-free mv the binary" \
    "$PROJENY" mv fake.projeny fake/src/keep.bin fake/src/keep2.bin
out="$(cd "$T172/c" && "$PROJENY" diff fake.projeny 2>"$T172/c/err")"
case "$out" in
*"rename from src/keep.bin"*)
    ok "bin-rename pure mv renders as rename"
    ;;
*) fail "bin-rename pure mv renders as rename" "out: $out" ;;
esac
case "$out" in
*"GIT binary patch"*)
    fail "bin-rename pure mv carries no payload" "out: $out"
    ;;
*) ok "bin-rename pure mv carries no payload" ;;
esac
run_in "$T172/c" expect_ok "bin-rename payload-free commit" \
    "$PROJENY" commit fake.projeny
expect_file_contains "bin-rename payload-free commit stores the rename" \
    "$T172/c/fake.projeny" "rename to src/keep2.bin"
if grep -q "GIT binary patch" "$T172/c/fake.projeny"; then
    fail "bin-rename payload-free commit stores no payload" \
         "$(grep -A2 "GIT binary patch" "$T172/c/fake.projeny")"
else
    ok "bin-rename payload-free commit stores no payload"
fi
rm -rf "$T172/c/fake" "$T172/c/.fake.projeny.status"
run_in "$T172/c" expect_ok "bin-rename payload-free fresh setup" \
    "$PROJENY" setup fake.projeny
expect_file_eq "bin-rename payload-free setup reproduces the bytes" \
    "$T172/c/fake/src/keep2.bin" "$T172/keep-C.bin"
if [ ! -e "$T172/c/fake/src/keep.bin" ]; then
    ok "bin-rename payload-free setup keeps the old path gone"
else
    fail "bin-rename payload-free setup keeps the old path gone"
fi
# --------- 173. re-commit after a committed rename keeps the renamed file
# Commit re-derives its patch from scratch against the raw archive, which
# predates every committed rename. When a committed rename's content has
# diverged beyond rename detection (a binary whose bytes changed - binaries
# never similarity-pair - or text below the 50% line-similarity threshold),
# that re-derivation yields delete(old) + add(new), and the add used to be
# dropped by the untracked-add keep filter: the keep list collected only
# PURE adds (both add-path helpers excluded rename blocks). The stored patch
# then lost the new file entirely, the next setup resurrected the old path,
# and the committed file was gone - rc 0 everywhere, no error. Both flows
# below commit a rename, commit AGAIN with no pending ops, and require the
# re-stored patch to still deliver the renamed file. The stored form is NOT
# pinned (a re-pairing rename and a delete+add are both correct): the
# OUTCOME is pinned - a fresh setup must reproduce the committed workdir.
T173="$ROOT/t173"
mkdir -p "$T173/a" "$T173/b"
for side in a b; do
    mkdir -p "$T173/$side/fake-1.0/src"
    printf 'int alpha = 1;\n' > "$T173/$side/fake-1.0/src/a.c"
    cat > "$T173/$side/fake-1.0/src/orig.c" <<'EOF'
int alpha = 1;

int beta = 1;

int gamma = 1;

int delta = 1;
EOF
    # Content A: NUL-bearing, shipped in the tarball so the committed block
    # really is a rename (a committed-ADDED binary re-derives as a plain add).
    python3 -c \
        "open('$T173/$side/fake-1.0/src/blob.bin','wb').write(b'old\x00\x00\x00blob\x00A\x00payload\n')"
    (cd "$T173/$side" && tar -czf fake-1.0.tar.gz fake-1.0 && rm -rf fake-1.0)
    printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    Fake project fake for projeny tests.\n' > "$T173/$side/fake.projeny"
done
# Reference bytes: content B differs in length and NUL placement from A.
python3 -c \
    "open('$T173/blob-B.bin','wb').write(b'\x00\x00brand\x00new\x00blob-B payload with a different length\n')"
# The wildly dissimilar text rewrite shares no line with the tarball original.
cat > "$T173/divergent.txt" <<'EOF'
zebra
quartz
mystic
plinth
EOF
# Binary flow: mv + overwrite with B, commit, commit again, fresh setup.
run_in "$T173/a" expect_ok "recommit-binary checkout setup" \
    "$PROJENY" setup fake.projeny
run_in "$T173/a" expect_ok "recommit-binary mv the tracked binary" \
    "$PROJENY" mv fake.projeny fake/src/blob.bin fake/src/blob2.bin
cp "$T173/blob-B.bin" "$T173/a/fake/src/blob2.bin"
run_in "$T173/a" expect_ok "recommit-binary first commit" \
    "$PROJENY" commit fake.projeny
expect_file_contains "recommit-binary stores rename from src/blob.bin" \
    "$T173/a/fake.projeny" "rename from src/blob.bin"
expect_file_contains "recommit-binary stores rename to src/blob2.bin" \
    "$T173/a/fake.projeny" "rename to src/blob2.bin"
expect_file_contains "recommit-binary stores the binary payload" \
    "$T173/a/fake.projeny" "GIT binary patch"
# The bug: the second commit (no pending ops) re-derives the diff against
# the raw archive; the diverged bytes never re-pair, and the re-derived add
# of blob2.bin used to be dropped as untracked - the file silently vanished.
run_in "$T173/a" expect_ok "recommit-binary second commit" \
    "$PROJENY" commit fake.projeny
expect_file_contains "recommit-binary re-stored patch still names blob2.bin" \
    "$T173/a/fake.projeny" "src/blob2.bin"
expect_file_contains "recommit-binary re-stored patch still carries bytes" \
    "$T173/a/fake.projeny" "GIT binary patch"
# Outcome, not form: a fresh setup must reproduce the committed workdir
# (rename-with-payload or delete+binary-add are both correct stored forms).
rm -rf "$T173/a/fake" "$T173/a/.fake.projeny.status"
run_in "$T173/a" expect_ok "recommit-binary fresh setup after wipe" \
    "$PROJENY" setup fake.projeny
if cmp -s "$T173/a/fake/src/blob2.bin" "$T173/blob-B.bin"; then
    ok "recommit-binary setup restores the new bytes at blob2.bin"
else
    fail "recommit-binary setup restores the new bytes at blob2.bin" \
         "got: $(python3 -c "print(open('$T173/a/fake/src/blob2.bin','rb').read())" 2>&1)"
fi
if [ ! -e "$T173/a/fake/src/blob.bin" ]; then
    ok "recommit-binary setup keeps the old path gone"
else
    fail "recommit-binary setup keeps the old path gone"
fi
# Text flow: mv + wildly dissimilar rewrite, commit, commit again, fresh
# setup. The forced-rename commit stores an arbitrarily divergent rename, so
# the second commit re-derives below the similarity threshold.
run_in "$T173/b" expect_ok "recommit-text checkout setup" \
    "$PROJENY" setup fake.projeny
run_in "$T173/b" expect_ok "recommit-text mv the tracked file" \
    "$PROJENY" mv fake.projeny fake/src/orig.c fake/src/renamed.c
cp "$T173/divergent.txt" "$T173/b/fake/src/renamed.c"
run_in "$T173/b" expect_ok "recommit-text first commit" \
    "$PROJENY" commit fake.projeny
expect_file_contains "recommit-text stores rename from src/orig.c" \
    "$T173/b/fake.projeny" "rename from src/orig.c"
expect_file_contains "recommit-text stores rename to src/renamed.c" \
    "$T173/b/fake.projeny" "rename to src/renamed.c"
run_in "$T173/b" expect_ok "recommit-text second commit" \
    "$PROJENY" commit fake.projeny
expect_file_contains "recommit-text re-stored patch still names renamed.c" \
    "$T173/b/fake.projeny" "src/renamed.c"
rm -rf "$T173/b/fake" "$T173/b/.fake.projeny.status"
run_in "$T173/b" expect_ok "recommit-text fresh setup after wipe" \
    "$PROJENY" setup fake.projeny
expect_file_eq "recommit-text setup restores the exact divergent content" \
    "$T173/b/fake/src/renamed.c" "$T173/divergent.txt"
if [ ! -e "$T173/b/fake/src/orig.c" ]; then
    ok "recommit-text setup keeps the old path gone"
else
    fail "recommit-text setup keeps the old path gone"
fi
# ------------------------- 174. flexible project arguments on every command
# Every project-taking command (setup, commit, add, rm, mv, resolve, rebase,
# status, and the 1-arg diff) resolves its first argument the way
# package/extract always have: the .projeny file itself, a directory holding
# exactly one .projeny file, a directory (typically the workdir) next to a
# "<dir>.projeny" sibling, or a bare name whose "<name>.projeny" sibling
# exists - so a checkout directory that was never created works too.
T174="$ROOT/t174"
make_tarballs "$T174" fake
write_projeny "$T174" fake 1.0 fake
# The explicitly new shape: only fake.projeny exists, fake/ never created.
run_in "$T174" expect_ok "setup accepts a never-set-up bare name" "$PROJENY" setup fake
if [ -f "$T174/fake/README" ] && [ -f "$T174/fake/src/a.c" ] && \
   [ -f "$T174/.fake.projeny.status" ]; then
    ok "bare-name setup created the checkout and the status file"
else
    fail "bare-name setup created the checkout and the status file" \
         "ls: $(ls -A "$T174" 2>&1)"
fi
expect_file_contains "bare-name setup checked out the tarball" "$T174/fake/README" "hello v1"
# status on a never-set-up bare name: resolves the right status file and
# reports not-set-up (exit 1) instead of crashing or reading the wrong file.
T174NS="$ROOT/t174ns"
mkdir -p "$T174NS"
write_projeny "$T174NS" fake 1.0 fake
out="$(cd "$T174NS" && "$PROJENY" status fake 2>&1)"; rc=$?
if [ $rc -ne 0 ]; then
    ok "status on a never-set-up bare name exits nonzero"
else
    fail "status on a never-set-up bare name exits nonzero" "out: $out"
fi
case "$out" in
*"fake.projeny"*"not set up"*|*"not set up"*"fake.projeny"*)
    ok "status on a never-set-up bare name resolves the .projeny file"
    ;;
*)
    fail "status on a never-set-up bare name resolves the .projeny file" \
         "out: $out"
    ;;
esac
# The checkout-directory form must behave exactly like the .projeny form:
# status output is identical either way.
o1="$(cd "$T174" && "$PROJENY" status fake 2>&1)"
o2="$(cd "$T174" && "$PROJENY" status fake.projeny 2>&1)"
if [ -n "$o1" ] && [ "$o1" = "$o2" ]; then
    ok "status via the workdir matches status via the .projeny file"
else
    fail "status via the workdir matches status via the .projeny file" \
         "dir: $o1 | projeny: $o2"
fi
# diff via the workdir: identical output to the .projeny form (non-empty
# diff, so an accidentally-empty result cannot pass).
printf 'brand new file\n' > "$T174/fake/src/added.c"
run_in "$T174" expect_ok "add via the workdir marks the file" "$PROJENY" add fake fake/src/added.c
expect_file_contains "add via the workdir records the pending op" "$T174/.fake.projeny.status" "Added: src/added.c"
d1="$(cd "$T174" && "$PROJENY" diff fake 2>&1)"
d2="$(cd "$T174" && "$PROJENY" diff fake.projeny 2>&1)"
if [ -n "$d1" ] && [ "$d1" = "$d2" ]; then
    ok "diff via the workdir matches diff via the .projeny file"
else
    fail "diff via the workdir matches diff via the .projeny file" \
         "dir: $d1 | projeny: $d2"
fi
# commit, mv, rm, rebase via the workdir.
run_in "$T174" expect_ok "commit via the workdir folds the add" "$PROJENY" commit fake
expect_file_contains "commit via the workdir stores the patch" "$T174/fake.projeny" "brand new file"
run_in "$T174" expect_ok "mv via the workdir renames" "$PROJENY" mv fake fake/src/added.c fake/src/renamed.c
expect_file_contains "mv via the workdir records the rename" "$T174/.fake.projeny.status" "Renamed: src/added.c -> src/renamed.c"
run_in "$T174" expect_ok "commit via the workdir folds the rename" "$PROJENY" commit fake
run_in "$T174" expect_ok "rm via the workdir deletes" "$PROJENY" rm fake fake/src/renamed.c
expect_file_contains "rm via the workdir records the removal" "$T174/.fake.projeny.status" "Removed: src/renamed.c"
run_in "$T174" expect_ok "commit via the workdir folds the rm" "$PROJENY" commit fake
run_in "$T174" expect_ok "rebase via the workdir re-points the archive" "$PROJENY" rebase fake fake-2.0.tar.gz
expect_file_contains "rebase via the workdir updates Archive" "$T174/fake.projeny" "Archive: fake-2.0.tar.gz"
# setup via the workdir on an existing checkout (re-setup of v2).
run_in "$T174" expect_ok "setup via the workdir re-sets up" "$PROJENY" setup fake
expect_file_contains "workdir re-setup keeps the committed state" "$T174/fake/README" "hello v2"
# resolve via the workdir, through the real conflict flow (test-4 style:
# local commit, upstream commit touching the same line, then a merge).
T174C="$ROOT/t174c"
make_tarballs "$T174C" fake
write_projeny "$T174C" fake 1.0 fake
(cd "$T174C" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T174C/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 10;")
open(p, "w").write(s)
EOF
(cd "$T174C" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T174C/fake.projeny" "$ROOT/t174-local.projeny"
rm -rf "$T174C/fake" "$T174C/.fake.projeny.status"
cp "$ROOT/t174-local.projeny" "$T174C/fake.projeny"
(cd "$T174C" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T174C/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 999;")
open(p, "w").write(s)
EOF
U174="$ROOT/t174up"
mkdir -p "$U174"
cp "$T174C/fake-1.0.tar.gz" "$U174/"
cp "$ROOT/t174-local.projeny" "$U174/fake.projeny"
(cd "$U174" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$U174/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 555;")
open(p, "w").write(s)
EOF
(cd "$U174" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$U174/fake.projeny" "$T174C/fake.projeny"
run_in "$T174C" expect_fail "conflicting merge via the workdir exits nonzero" "$PROJENY" setup fake
expect_file_contains "conflict recorded in the status file" "$T174C/.fake.projeny.status" "Conflict: src/a.c"
printf 'int alpha = 1;\n\nint beta = 777;\n\nint gamma = 1;\n\nint delta = 1;\n' > "$T174C/fake/src/a.c"
run_in "$T174C" expect_ok "resolve via the workdir clears the conflict" "$PROJENY" resolve fake fake/src/a.c
if grep -q "^Conflict:" "$T174C/.fake.projeny.status"; then
    fail "resolve via the workdir removes the conflict entry" "$(cat "$T174C/.fake.projeny.status")"
else
    ok "resolve via the workdir removes the conflict entry"
fi
run_in "$T174C" expect_ok "commit via the workdir after resolve" "$PROJENY" commit fake
# A directory holding exactly one .projeny file (test-95's proj/ fixture).
T174D="$ROOT/t174d"
mkdir -p "$T174D/proj"
make_tarballs "$T174D/proj" fake
write_projeny "$T174D/proj" fake 1.0 fake
run_in "$T174D" expect_ok "setup accepts a directory holding one .projeny" "$PROJENY" setup proj
run_in "$T174D" expect_ok "status accepts a directory holding one .projeny" "$PROJENY" status proj
if [ -f "$T174D/proj/fake/README" ]; then
    ok "single-projeny-dir setup checked out the workdir"
else
    fail "single-projeny-dir setup checked out the workdir" "ls: $(ls -R "$T174D/proj" 2>&1)"
fi
# Error shapes: a directory with no .projeny anywhere fails with the
# resolver's message; a directory holding two .projeny files asks for one.
T174E="$ROOT/t174e"
mkdir -p "$T174E/lonely"
out="$(cd "$T174E" && "$PROJENY" setup lonely 2>&1 || true)"
case "$out" in
*"directory holds no .projeny file"*)
    ok "a projeny-less directory fails with the resolver message"
    ;;
*)
    fail "a projeny-less directory fails with the resolver message" "out: $out"
    ;;
esac
T174T="$ROOT/t174t"
mkdir -p "$T174T/two"
make_tarballs "$T174T/two" fake
write_projeny "$T174T/two" fake 1.0 fake
write_projeny "$T174T/two" other 1.0 fake
out="$(cd "$T174T" && "$PROJENY" status two 2>&1 || true)"
case "$out" in
*"multiple .projeny files"*)
    ok "a directory holding two .projeny files asks for one explicitly"
    ;;
*)
    fail "a directory holding two .projeny files asks for one explicitly" \
         "out: $out"
    ;;
esac
# Consistency: package/extract take the never-set-up bare name too.
T174P="$ROOT/t174p"
make_tarballs "$T174P" fake
write_projeny "$T174P" fake 1.0 fake
run_in "$T174P" expect_ok "package accepts a never-set-up bare name" "$PROJENY" package fake pkg.tar.gz
if [ -f "$T174P/pkg.tar.gz" ]; then
    ok "bare-name package wrote the archive"
else
    fail "bare-name package wrote the archive" "ls: $(ls -A "$T174P")"
fi
run_in "$T174P" expect_ok "extract accepts a never-set-up bare name" "$PROJENY" extract fake dest
expect_file_contains "bare-name extract holds the tracked files" "$T174P/dest/README" "hello v1"

# --------------------- 175. setup reports honestly whether a merge happened
# U is the diff of the workdir against base+patch, so a pristine checkout
# diffs empty even when the committed patch is non-empty. setup used to
# compare U against the whole patch and therefore claimed "merged local
# changes" on every re-setup of every real (patched) project. These tests
# pin the honest reporting: fresh re-setup says "no local changes",
# untracked files ride along without being called a merge, and a real merge
# prints one line per file. The committed edits here stay in the delta
# region of src/a.c, which fake-2.0 leaves alone, so the rebase below stays
# clean (v2 rewrites src/b.c and README).
T175="$ROOT/t175"
make_tarballs "$T175" fake
write_projeny "$T175" fake 1.0 fake
(cd "$T175" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
# A NON-EMPTY committed patch (the standard fixture's patch is empty).
python3 - "$T175/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
(cd "$T175" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T175/fake.projeny" "$ROOT/t175-local.projeny"
out="$(cd "$T175" && "$PROJENY" setup fake.projeny 2>&1)"
case "$out" in
*"no local changes"*)
    ok "re-setup of a patched pristine checkout says no local changes"
    ;;
*)
    fail "re-setup of a patched pristine checkout says no local changes" \
         "out: $out"
    ;;
esac
case "$out" in
*"merged local changes"*)
    fail "re-setup of a patched pristine checkout claims no merge" "out: $out"
    ;;
*)
    ok "re-setup of a patched pristine checkout claims no merge"
    ;;
esac
expect_file_contains "re-setup keeps the committed content" "$T175/fake/src/a.c" "delta = 100"
# Only untracked files: still no merge, and the files survive.
printf 'rider\n' > "$T175/fake/untracked.txt"
out="$(cd "$T175" && "$PROJENY" setup fake.projeny 2>&1)"
case "$out" in
*"no local changes"*)
    ok "untracked files alone do not claim a merge"
    ;;
*)
    fail "untracked files alone do not claim a merge" "out: $out"
    ;;
esac
case "$out" in
*"merged local changes"*)
    fail "untracked-file setup never says merged" "out: $out"
    ;;
*)
    ok "untracked-file setup never says merged"
    ;;
esac
if [ -f "$T175/fake/untracked.txt" ] && [ "$(cat "$T175/fake/untracked.txt")" = "rider" ]; then
    ok "untracked file rides along the re-setup"
else
    fail "untracked file rides along the re-setup" \
         "ls: $(ls -A "$T175/fake" 2>&1)"
fi
rm -f "$T175/fake/untracked.txt" # it would dirty the rebase below
# A genuine merge: local base (v1, committed delta edit) plus an uncommitted
# beta edit, upstream rebased onto v2 with its own committed gamma edit.
(cd "$T175" && "$PROJENY" rebase fake.projeny fake-2.0.tar.gz >/dev/null 2>&1)
python3 - "$T175/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int gamma = 1;", "int gamma = 200;")
open(p, "w").write(s)
EOF
(cd "$T175" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T175/fake.projeny" "$ROOT/t175-up.projeny"
rm -rf "$T175/fake" "$T175/.fake.projeny.status"
cp "$ROOT/t175-local.projeny" "$T175/fake.projeny"
(cd "$T175" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T175/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 300;")
open(p, "w").write(s)
EOF
cp "$ROOT/t175-up.projeny" "$T175/fake.projeny"
out="$(cd "$T175" && "$PROJENY" setup fake.projeny 2>&1)"
case "$out" in
*"merged local changes onto"*)
    ok "a real merge says it merged"
    ;;
*)
    fail "a real merge says it merged" "out: $out"
    ;;
esac
case "$out" in
*"src/a.c"*)
    ok "the merge report names the merged file"
    ;;
*)
    fail "the merge report names the merged file" "out: $out"
    ;;
esac
expect_file_contains "the merge kept the local beta edit" "$T175/fake/src/a.c" "beta = 300"
expect_file_contains "the merge kept the committed delta edit" "$T175/fake/src/a.c" "delta = 100"
expect_file_contains "the merge took the upstream gamma edit" "$T175/fake/src/a.c" "gamma = 200"
expect_file_contains "the merge took the v2 alpha" "$T175/fake/src/a.c" "alpha = 2"
# A pending add carried through a merge is reported as added.
printf 'added through a merge\n' > "$T175/fake/src/merged-add.c"
run_in "$T175" expect_ok "pending add marked before the merge" "$PROJENY" add fake.projeny fake/src/merged-add.c
cp "$ROOT/t175-up.projeny" "$ROOT/t175-up2.projeny"
python3 - "$ROOT/t175-up2.projeny" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("gamma = 200", "gamma = 201")
open(p, "w").write(s)
EOF
cp "$ROOT/t175-up2.projeny" "$T175/fake.projeny"
out="$(cd "$T175" && "$PROJENY" setup fake.projeny 2>&1)"
case "$out" in
*"added: src/merged-add.c"*)
    ok "the merge report lists the pending add as added"
    ;;
*)
    fail "the merge report lists the pending add as added" "out: $out"
    ;;
esac
expect_file_contains "the status keeps the pending add" "$T175/.fake.projeny.status" "Added: src/merged-add.c"
if [ "$(cat "$T175/fake/src/merged-add.c")" = "added through a merge" ]; then
    ok "the pending add survived the merge"
else
    fail "the pending add survived the merge" "$(cat "$T175/fake/src/merged-add.c" 2>&1)"
fi
expect_file_contains "the merge took the second upstream edit" "$T175/fake/src/a.c" "gamma = 201"
# A pending rm carried through a merge is reported as deleted (v1-only
# upstream pair, so the deleted file is identical on both sides and the
# deletion applies cleanly).
T175R="$ROOT/t175r"
make_tarballs "$T175R" fake
write_projeny "$T175R" fake 1.0 fake
(cd "$T175R" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T175R/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
(cd "$T175R" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T175R/fake.projeny" "$ROOT/t175r-local.projeny"
cp "$ROOT/t175r-local.projeny" "$T175R/fake.projeny"
(cd "$T175R" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T175R/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int gamma = 1;", "int gamma = 200;")
open(p, "w").write(s)
EOF
(cd "$T175R" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T175R/fake.projeny" "$ROOT/t175r-up.projeny"
rm -rf "$T175R/fake" "$T175R/.fake.projeny.status"
cp "$ROOT/t175r-local.projeny" "$T175R/fake.projeny"
(cd "$T175R" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T175R" expect_ok "pending rm marked before the merge" "$PROJENY" rm fake.projeny fake/src/b.c
cp "$ROOT/t175r-up.projeny" "$T175R/fake.projeny"
out="$(cd "$T175R" && "$PROJENY" setup fake.projeny 2>&1)"
case "$out" in
*"deleted: src/b.c"*)
    ok "the merge report lists the pending rm as deleted"
    ;;
*)
    fail "the merge report lists the pending rm as deleted" "out: $out"
    ;;
esac
if [ ! -e "$T175R/fake/src/b.c" ]; then
    ok "the pending rm survived the merge"
else
    fail "the pending rm survived the merge" "ls: $(ls "$T175R/fake/src" 2>&1)"
fi
expect_file_contains "the rm merge took the upstream gamma edit" "$T175R/fake/src/a.c" "gamma = 200"
# A conflicting merge reports the conflict per file and exits 1; after
# resolve + commit everything is consistent again.
T175C="$ROOT/t175c"
make_tarballs "$T175C" fake
write_projeny "$T175C" fake 1.0 fake
(cd "$T175C" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T175C/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 10;")
open(p, "w").write(s)
EOF
(cd "$T175C" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$T175C/fake.projeny" "$ROOT/t175c-local.projeny"
rm -rf "$T175C/fake" "$T175C/.fake.projeny.status"
cp "$ROOT/t175c-local.projeny" "$T175C/fake.projeny"
(cd "$T175C" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$T175C/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 999;")
open(p, "w").write(s)
EOF
U175C="$ROOT/t175cup"
mkdir -p "$U175C"
cp "$T175C/fake-1.0.tar.gz" "$U175C/"
cp "$ROOT/t175c-local.projeny" "$U175C/fake.projeny"
(cd "$U175C" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$U175C/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 555;")
open(p, "w").write(s)
EOF
(cd "$U175C" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$U175C/fake.projeny" "$T175C/fake.projeny"
out="$(cd "$T175C" && "$PROJENY" setup fake.projeny 2>&1)"; rc=$?
if [ $rc -ne 0 ]; then
    ok "a conflicting merge exits 1"
else
    fail "a conflicting merge exits 1" "out: $out"
fi
case "$out" in
*"conflict: src/a.c"*)
    ok "the merge report lists the conflict per file"
    ;;
*)
    fail "the merge report lists the conflict per file" "out: $out"
    ;;
esac
case "$out" in
*"setup left conflicts (exit 1)"*)
    ok "the conflicting merge keeps the closing guidance line"
    ;;
*)
    fail "the conflicting merge keeps the closing guidance line" "out: $out"
    ;;
esac
printf 'int alpha = 1;\n\nint beta = 777;\n\nint gamma = 1;\n\nint delta = 1;\n' > "$T175C/fake/src/a.c"
run_in "$T175C" expect_ok "resolve after the reported conflict" "$PROJENY" resolve fake.projeny fake/src/a.c
run_in "$T175C" expect_ok "commit after the reported conflict" "$PROJENY" commit fake.projeny
expect_file_contains "the post-conflict commit stores the resolution" "$T175C/fake.projeny" "beta = 777"
# Regression: with the plain (unpatched) fixture, a double setup still says
# "(no local changes)".
T175P="$ROOT/t175p"
make_tarballs "$T175P" fake
write_projeny "$T175P" fake 1.0 fake
(cd "$T175P" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
out="$(cd "$T175P" && "$PROJENY" setup fake.projeny 2>&1)"
case "$out" in
*"(no local changes)"*)
    ok "empty-patch double setup still says no local changes"
    ;;
*)
    fail "empty-patch double setup still says no local changes" "out: $out"
    ;;
esac

# ----------------- 176. harder conflicted setup reports each conflict once
# Same fixture shape as the harder case in section 80: a conflicted .projeny
# file (markers in the patch region) on top of a checkout that also carries
# an uncommitted workdir edit — here to the very file (a.c) the merge
# conflicts on, so both merge stages report it. The report must list each
# conflicted file exactly once.
T176="$ROOT/t176"
mkdir -p "$T176"
cp "$T81/w-1.0.tar.gz" "$T176/"
cp "$ROOT/t81-local.projeny" "$T176/w.projeny"
(cd "$T176" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
# uncommitted edit to a.c, the file the two sides conflict on.
python3 - "$T176/w/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("delta 1", "delta UNCOMMITTED")
open(p, "w").write(s)
EOF
make_conflicted "$ROOT/t81-local.projeny" "$ROOT/t81-up.projeny" "$T176/w.projeny"
out="$(cd "$T176" && "$PROJENY" setup w.projeny 2>&1)"
if [ $? -ne 0 ]; then
    ok "harder conflicted setup (conflict in both stages) exits nonzero"
else
    fail "harder conflicted setup (conflict in both stages) exits nonzero" "out: $out"
fi
nconf="$(echo "$out" | grep -c 'conflict: a.c')"
if [ "$nconf" -eq 1 ]; then
    ok "harder conflicted setup lists 'conflict: a.c' exactly once"
else
    fail "harder conflicted setup lists 'conflict: a.c' exactly once" \
        "count=$nconf out: $out"
fi
if echo "$out" | grep -q "setup left conflicts"; then
    ok "harder conflicted setup prints the closing guidance"
else
    fail "harder conflicted setup prints the closing guidance" "out: $out"
fi
expect_file_contains "harder conflicted setup keeps uncommitted edit" \
    "$T176/w/a.c" "delta UNCOMMITTED"
expect_file_contains "harder conflicted setup leaves markers" \
    "$T176/w/a.c" "<<<<<<<"

# --------------- 177. setup reports a pending mv as renamed, not add+delete
# A pending `projeny mv` alone makes the next setup a merge (U holds the
# move's delete+add). The report must name the move as one "renamed:" line
# — the status file records Renamed: and `projeny diff` renders the same
# state as a rename — never a bare "added:" for the destination.
T177="$ROOT/t177"
make_tarballs "$T177" fake
write_projeny "$T177" fake 1.0 fake
(cd "$T177" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T177" expect_ok "setup-report mv records the rename" \
    "$PROJENY" mv fake.projeny fake/src/b.c fake/src/moved.c
out="$(cd "$T177" && "$PROJENY" setup fake.projeny 2>&1)"
if [ $? -eq 0 ]; then
    ok "setup after a pending mv exits 0"
else
    fail "setup after a pending mv exits 0" "out: $out"
fi
case "$out" in
*"renamed: src/b.c -> src/moved.c"*)
    ok "setup report lists a pending mv as renamed"
    ;;
*)
    fail "setup report lists a pending mv as renamed" "out: $out"
    ;;
esac
case "$out" in
*"added: src/moved.c"*)
    fail "setup report never calls a pending mv added" "out: $out"
    ;;
*)
    ok "setup report never calls a pending mv added"
    ;;
esac
expect_file_contains "the mv merge keeps the renamed status" \
    "$T177/.fake.projeny.status" "Renamed: src/b.c -> src/moved.c"
if [ -f "$T177/fake/src/moved.c" ] &&
   [ "$(cat "$T177/fake/src/moved.c")" = "line one v1" ] &&
   [ ! -e "$T177/fake/src/b.c" ]; then
    ok "the mv merge moved the file with its content"
else
    fail "the mv merge moved the file with its content" \
        "ls: $(ls "$T177/fake/src" 2>&1)"
fi

# -------------- 178. setup reports a divergent pending mv as renamed
# Regression: a pending `projeny mv` whose moved file was rewritten past
# rename-similarity detection split into "deleted: <src>" plus
# "added: <dst>" in the setup report (setup's workdir diff is a plain
# diff without the forced-rename pairing `projeny diff`/`commit` use),
# even though the status file records Renamed: and `projeny diff` renders
# the same state as a rename. The report must pair the sides back up
# from the pending rename list: one "renamed:" line, no add/delete.
T178="$ROOT/t178"
make_tarballs "$T178" fake
write_projeny "$T178" fake 1.0 fake
(cd "$T178" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
run_in "$T178" expect_ok "divergent mv records the rename" \
    "$PROJENY" mv fake.projeny fake/src/b.c fake/src/moved.c
# Rewrite the moved file completely so no content/similarity pairing can
# rematch it against src/b.c in a plain diff: every line differs.
python3 - "$T178/fake/src/moved.c" <<'EOF'
import sys
p = sys.argv[1]
open(p, "w").write(
    "".join("completely different content line %d\n" % i for i in range(12)))
EOF
out="$(cd "$T178" && "$PROJENY" setup fake.projeny 2>&1)"
if [ $? -eq 0 ]; then
    ok "setup after a divergent pending mv exits 0"
else
    fail "setup after a divergent pending mv exits 0" "out: $out"
fi
case "$out" in
*"renamed: src/b.c -> src/moved.c"*)
    ok "divergent pending mv reports renamed"
    ;;
*)
    fail "divergent pending mv reports renamed" "out: $out"
    ;;
esac
case "$out" in
*"added: src/moved.c"*)
    fail "divergent pending mv is not mis-reported as added" "out: $out"
    ;;
*)
    ok "divergent pending mv is not mis-reported as added"
    ;;
esac
case "$out" in
*"deleted: src/b.c"*)
    fail "divergent pending mv is not mis-reported as deleted" "out: $out"
    ;;
*)
    ok "divergent pending mv is not mis-reported as deleted"
    ;;
esac
expect_file_contains "divergent mv keeps the renamed status" \
    "$T178/.fake.projeny.status" "Renamed: src/b.c -> src/moved.c"
if [ -f "$T178/fake/src/moved.c" ] &&
   grep -q "completely different content line 11" "$T178/fake/src/moved.c" &&
   [ ! -e "$T178/fake/src/b.c" ]; then
    ok "divergent mv keeps the edited content at the destination"
else
    fail "divergent mv keeps the edited content at the destination" \
        "ls: $(ls "$T178/fake/src" 2>&1)"
fi

# -------- 179. conflicted .projeny setup reports a committed local add
# A git-conflicted .projeny whose LOCAL side carries only a committed add
# (no modifications): the stage-1 merge input is the committed patch, where
# every block is a real change, so the add must be reported — and the setup
# must never claim "no local changes to merge onto" when the merge brings
# the added file into the workdir.
T179="$ROOT/t179"
make_tarballs "$T179" fake
write_projeny "$T179" fake 1.0 fake
(cd "$T179" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
printf 'committed by the local side\n' > "$T179/fake/src/new.c"
run_in "$T179" expect_ok "fixture marks the local add" "$PROJENY" add fake.projeny fake/src/new.c
run_in "$T179" expect_ok "fixture commits the local add" "$PROJENY" commit fake.projeny
cp "$T179/fake.projeny" "$ROOT/t179-local.projeny"
# upstream twin: same base, its own committed edit to a different file.
U179="$ROOT/t179up"
make_tarballs "$U179" fake
write_projeny "$U179" fake 1.0 fake
(cd "$U179" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$U179/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int gamma = 1;", "int gamma = 200;")
open(p, "w").write(s)
EOF
(cd "$U179" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$U179/fake.projeny" "$ROOT/t179-up.projeny"
# git-style conflict between the committed-add local side and the upstream.
make_conflicted "$ROOT/t179-local.projeny" "$ROOT/t179-up.projeny" "$T179/fake.projeny"
out="$(cd "$T179" && "$PROJENY" setup fake.projeny 2>&1)"
if [ $? -ne 0 ]; then
    fail "conflicted setup with a committed local add exits 0" "out: $out"
else
    ok "conflicted setup with a committed local add exits 0"
fi
case "$out" in
*"added: src/new.c"*)
    ok "the merge report lists the committed add as added"
    ;;
*)
    fail "the merge report lists the committed add as added" "out: $out"
    ;;
esac
case "$out" in
*"merged local changes onto"*)
    ok "the committed-add merge prints the merged header"
    ;;
*)
    fail "the committed-add merge prints the merged header" "out: $out"
    ;;
esac
case "$out" in
*"no local changes to merge onto"*)
    fail "committed-add merge never says no local changes" "out: $out"
    ;;
*)
    ok "committed-add merge never says no local changes"
    ;;
esac
if cmp -s "$T179/fake.projeny" "$ROOT/t179-up.projeny"; then
    ok "committed-add setup force-takes upstream"
else
    fail "committed-add setup force-takes upstream" "$(cat "$T179/fake.projeny")"
fi
if [ -f "$T179/fake/src/new.c" ] &&
   [ "$(cat "$T179/fake/src/new.c")" = "committed by the local side" ]; then
    ok "the committed add landed in the workdir"
else
    fail "the committed add landed in the workdir" \
        "ls: $(ls "$T179/fake/src" 2>&1)"
fi
expect_file_contains "the merge took the upstream edit alongside the add" \
    "$T179/fake/src/a.c" "int gamma = 200;"
expect_file_contains "the status embeds the upstream copy" \
    "$T179/.fake.projeny.status" "int gamma = 200;"
expect_file_not_contains "committed-add merge records no conflicts" \
    "$T179/.fake.projeny.status" "Conflict:"
run_in "$T179" expect_ok "status reads consistent state after the add merge" \
    "$PROJENY" status fake.projeny

# ---------- 180. conflicted .projeny setup with an empty local side
# Markers between the base content and the upstream content: the local side
# contributes no patch at all, so the merge brings nothing of ours in and
# setup must say "no local changes to merge onto", exit 0, and leave the
# workdir at the upstream content.
T180="$ROOT/t180"
make_tarballs "$T180" fake
write_projeny "$T180" fake 1.0 fake
cp "$T180/fake.projeny" "$ROOT/t180-pristine.projeny"
(cd "$T180" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
# upstream twin: same base with one committed edit.
U180="$ROOT/t180up"
make_tarballs "$U180" fake
write_projeny "$U180" fake 1.0 fake
(cd "$U180" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$U180/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 77;")
open(p, "w").write(s)
EOF
(cd "$U180" && "$PROJENY" commit fake.projeny >/dev/null 2>&1)
cp "$U180/fake.projeny" "$ROOT/t180-up.projeny"
# git-style conflict: pristine local side vs the upstream commit.
make_conflicted "$ROOT/t180-pristine.projeny" "$ROOT/t180-up.projeny" "$T180/fake.projeny"
out="$(cd "$T180" && "$PROJENY" setup fake.projeny 2>&1)"
if [ $? -ne 0 ]; then
    fail "conflicted setup with an empty local side exits 0" "out: $out"
else
    ok "conflicted setup with an empty local side exits 0"
fi
case "$out" in
*"no local changes to merge onto"*)
    ok "empty local side says no local changes to merge onto"
    ;;
*)
    fail "empty local side says no local changes to merge onto" "out: $out"
    ;;
esac
case "$out" in
*"merged local changes"*)
    fail "empty local side never claims a merge" "out: $out"
    ;;
*)
    ok "empty local side never claims a merge"
    ;;
esac
if cmp -s "$T180/fake.projeny" "$ROOT/t180-up.projeny"; then
    ok "empty-local-side setup force-takes upstream"
else
    fail "empty-local-side setup force-takes upstream" "$(cat "$T180/fake.projeny")"
fi
expect_file_contains "the workdir ends at the upstream content" \
    "$T180/fake/src/a.c" "int beta = 77;"
expect_file_contains "the workdir keeps the untouched tarball content" \
    "$T180/fake/README" "hello v1"
expect_file_contains "the status embeds the upstream copy" \
    "$T180/.fake.projeny.status" "int beta = 77;"
expect_file_not_contains "empty-local-side merge records no conflicts" \
    "$T180/.fake.projeny.status" "Conflict:"

# ----------------- 181. '.' and '..' (any relative path) project arguments
# From inside the workdir, '.' names the project; from a workdir
# subdirectory, '..' does too. The argument is lexically normalized FIRST,
# so it cannot mutate into a bogus "<arg>.projeny" sibling ('.' ->
# '..projeny') or scan the wrong directory ('..' listing the parent). The
# workdir-sibling rule runs before the directory scan, so a workdir is
# recognized even when it holds stray .projeny files, and a plain workdir
# name keeps resolving the same way.
T181="$ROOT/t181"
make_tarballs "$T181" fake
write_projeny "$T181" fake 1.0 fake
run_in "$T181" expect_ok "dot setup from the project dir" "$PROJENY" setup .
if [ -f "$T181/fake/README" ] && [ -f "$T181/fake/src/a.c" ]; then
    ok "dot setup checked out the workdir"
else
    fail "dot setup checked out the workdir" "ls: $(ls -R "$T181" 2>&1)"
fi
# status with '.' from inside the workdir must match status via the file.
o1="$(cd "$T181/fake" && "$PROJENY" status . 2>&1)"
o2="$(cd "$T181" && "$PROJENY" status fake.projeny 2>&1)"
if [ -n "$o1" ] && [ "$o1" = "$o2" ]; then
    ok "dot status inside the workdir matches status via the file"
else
    fail "dot status inside the workdir matches status via the file" \
         "dot: $o1 | file: $o2"
fi
# edit + commit with '.' (the path argument is CWD-relative here, which also
# exercises workdir-relative path resolution against a '.' project argument).
printf 'dot arg file\n' > "$T181/fake/src/dotarg.c"
run_in "$T181/fake" expect_ok "dot add from inside the workdir" "$PROJENY" add . src/dotarg.c
expect_file_contains "dot add recorded the pending op" "$T181/.fake.projeny.status" "Added: src/dotarg.c"
run_in "$T181/fake" expect_ok "dot commit from inside the workdir" "$PROJENY" commit .
expect_file_contains "dot commit stored the patch" "$T181/fake.projeny" "dot arg file"
expect_file_contains "dot commit refreshed the status copy" \
    "$T181/.fake.projeny.status" "dot arg file"
run_in "$T181/fake" expect_ok "dot diff from inside the workdir" "$PROJENY" diff .
# '..' from a workdir subdirectory.
run_in "$T181/fake/src" expect_ok "dotdot status from a subdir" "$PROJENY" status ..
run_in "$T181/fake/src" expect_ok "dotdot diff from a subdir" "$PROJENY" diff ..
run_in "$T181/fake/src" expect_ok "dotdot setup from a subdir" "$PROJENY" setup ..
# Trailing slashes and './' spelling name the same project.
o1="$(cd "$T181/fake" && "$PROJENY" status ./ 2>&1)"
o2="$(cd "$T181" && "$PROJENY" status fake.projeny 2>&1)"
if [ "$o1" = "$o2" ]; then
    ok "trailing-slash dot status matches too"
else
    fail "trailing-slash dot status matches too" "dot: $o1 | file: $o2"
fi
# A plain workdir name still resolves via the sibling rule.
run_in "$T181" expect_ok "plain workdir name still resolves" "$PROJENY" status fake
# The workdir-sibling rule wins over a scan: a stray .projeny inside the
# workdir must not hijack '.' (pre-fix, the scan would see two .projeny
# files and die asking for one explicitly; with the rule, the project
# resolves normally — the stray file itself merely shows up as untracked).
cp "$T181/fake.projeny" "$T181/fake/stray.projeny"
o1="$(cd "$T181/fake" && "$PROJENY" status . 2>&1)"
rm -f "$T181/fake/stray.projeny"
case "$o1" in
*"multiple .projeny files"*)
    fail "dot ignores a stray .projeny inside the workdir" "out: $o1"
    ;;
*"Status: setup"*)
    ok "dot ignores a stray .projeny inside the workdir"
    ;;
*)
    fail "dot ignores a stray .projeny inside the workdir" "out: $o1"
    ;;
esac
# Error shapes: '.' in a directory with no .projeny anywhere fails with the
# resolver's message (now naming the correct sibling), and a directory
# holding two .projeny files still asks for one explicitly.
T181L="$ROOT/t181l"
mkdir -p "$T181L/lonely"
out="$(cd "$T181L/lonely" && "$PROJENY" setup . 2>&1 || true)"
case "$out" in
*"directory holds no .projeny file"*)
    ok "dot in a projeny-less directory fails with the resolver message"
    ;;
*)
    fail "dot in a projeny-less directory fails with the resolver message" \
         "out: $out"
    ;;
esac
T181T="$ROOT/t181t"
mkdir -p "$T181T/two"
write_projeny "$T181T/two" one 1.0 fake
write_projeny "$T181T/two" other 1.0 fake
out="$(cd "$T181T/two" && "$PROJENY" status . 2>&1 || true)"
case "$out" in
*"multiple .projeny files"*)
    ok "dot in a multi-projeny directory asks for one explicitly"
    ;;
*)
    fail "dot in a multi-projeny directory asks for one explicitly" \
         "out: $out"
    ;;
esac

# ------- 182. no-trailing-newline .projeny: the patch must not glue (mg bug)
# mg.projeny ends mid-line ("    mg 4.1 unmodified", no newline). Committing
# used to concatenate the patch right after that last prose line, so the
# next parse found no "diff --git " line and silently reduced the committed
# patch to prose — which made setup "merge" an edit that a .projeny-only
# revert should have wiped. These flows pin the fixed behavior.
#
# Flow 1: the committed patch starts at column 0.
T182="$ROOT/t182"
make_tarballs "$T182" fake
printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    mg 4.1 unmodified.' > "$T182/fake.projeny"
cp "$T182/fake.projeny" "$ROOT/t182-pre-commit.projeny"
run_in "$T182" expect_ok "no-newline mg-style setup" "$PROJENY" setup fake.projeny
printf 'COMMITTED-MARKER\n' >> "$T182/fake/README"
run_in "$T182" expect_ok "no-newline mg-style commit" "$PROJENY" commit fake.projeny
cp "$T182/fake.projeny" "$ROOT/t182-committed.projeny"
expect_file_contains "mg-style commit stores the edit" "$T182/fake.projeny" "COMMITTED-MARKER"
expect_file_not_contains "mg-style commit never glues the patch onto the prose" \
    "$T182/fake.projeny" "mg 4.1 unmodified.diff --git "
if grep -q '^diff --git ' "$T182/fake.projeny"; then
    ok "mg-style commit starts the patch at column 0"
else
    fail "mg-style commit starts the patch at column 0" "$(cat "$T182/fake.projeny")"
fi
expect_file_contains "mg-style commit refreshes the status copy" \
    "$T182/.fake.projeny.status" "COMMITTED-MARKER"
#
# Flow 2 (the bug): revert the .projeny FILE ONLY to its pre-commit bytes;
# the status file and the workdir keep the committed state. setup must
# report "no local changes" and REVERT the edit (the workdir returns to the
# fresh tarball+patch content), not "merge" it.
cp "$ROOT/t182-pre-commit.projeny" "$T182/fake.projeny"
out="$(cd "$T182" && "$PROJENY" setup fake.projeny 2>&1)"; rc=$?
if [ $rc -eq 0 ]; then
    ok "setup after .projeny-only revert exits 0"
else
    fail "setup after .projeny-only revert exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"no local changes"*)
    ok "setup after .projeny-only revert reports no local changes"
    ;;
*)
    fail "setup after .projeny-only revert reports no local changes" "out: $out"
    ;;
esac
case "$out" in
*"merged local changes"*)
    fail "setup after .projeny-only revert never claims a merge" "out: $out"
    ;;
*)
    ok "setup after .projeny-only revert never claims a merge"
    ;;
esac
expect_file_not_contains "setup after .projeny-only revert wipes the edit" \
    "$T182/fake/README" "COMMITTED-MARKER"
if cmp -s "$T182/fake.projeny" "$ROOT/t182-pre-commit.projeny"; then
    ok "setup after .projeny-only revert keeps the reverted file bytes"
else
    fail "setup after .projeny-only revert keeps the reverted file bytes" \
         "$(cat "$T182/fake.projeny")"
fi
#
# Flow 3: setup -> edit -> setup again (NO commit) -> the edit is preserved
# and reported as a merge.
printf 'UNCOMMITTED-MARKER\n' >> "$T182/fake/README"
out="$(cd "$T182" && "$PROJENY" setup fake.projeny 2>&1)"; rc=$?
if [ $rc -eq 0 ]; then
    ok "setup-again with an uncommitted edit exits 0"
else
    fail "setup-again with an uncommitted edit exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"merged local changes"*)
    ok "setup-again with an uncommitted edit reports a merge"
    ;;
*)
    fail "setup-again with an uncommitted edit reports a merge" "out: $out"
    ;;
esac
expect_file_contains "setup-again keeps the uncommitted edit" \
    "$T182/fake/README" "UNCOMMITTED-MARKER"
#
# Flow 4: setup -> edit -> commit -> revert BOTH the .projeny file AND the
# status file to their pre-commit bytes -> setup keeps the edit ("merged"):
# with the shared base gone from both bookkeeping files, the workdir edit is
# genuinely uncommitted again.
T182C="$ROOT/t182c"
make_tarballs "$T182C" fake
printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    mg 4.1 unmodified.' > "$T182C/fake.projeny"
run_in "$T182C" expect_ok "flow-4 fixture setup" "$PROJENY" setup fake.projeny
cp "$T182C/fake.projeny" "$ROOT/t182c-pre.projeny"
cp "$T182C/.fake.projeny.status" "$ROOT/t182c-pre.status"
printf 'BOTH-REVERT-MARKER\n' >> "$T182C/fake/README"
run_in "$T182C" expect_ok "flow-4 commit" "$PROJENY" commit fake.projeny
cp "$ROOT/t182c-pre.projeny" "$T182C/fake.projeny"
cp "$ROOT/t182c-pre.status" "$T182C/.fake.projeny.status"
out="$(cd "$T182C" && "$PROJENY" setup fake.projeny 2>&1)"; rc=$?
if [ $rc -eq 0 ]; then
    ok "setup after reverting both files exits 0"
else
    fail "setup after reverting both files exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"merged local changes"*)
    ok "setup after reverting both files reports a merge"
    ;;
*)
    fail "setup after reverting both files reports a merge" "out: $out"
    ;;
esac
expect_file_contains "setup after reverting both files keeps the edit" \
    "$T182C/fake/README" "BOTH-REVERT-MARKER"
#
# Flow 5: a no-op commit on a no-trailing-newline .projeny (empty patch)
# must leave the file byte-identical — the no-op may not gain a newline.
T182D="$ROOT/t182d"
make_tarballs "$T182D" fake
printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fake\n\n    mg 4.1 unmodified.' > "$T182D/fake.projeny"
cp "$T182D/fake.projeny" "$ROOT/t182d-pre.projeny"
run_in "$T182D" expect_ok "no-op commit fixture setup" "$PROJENY" setup fake.projeny
run_in "$T182D" expect_ok "no-op commit with no workdir changes" "$PROJENY" commit fake.projeny
if cmp -s "$T182D/fake.projeny" "$ROOT/t182d-pre.projeny"; then
    ok "no-op commit leaves the newline-less .projeny byte-identical"
else
    fail "no-op commit leaves the newline-less .projeny byte-identical" \
         "$(cat "$T182D/fake.projeny")"
fi

# ------------------------------- 183. freeze-mtime basics
# A frozen file's mtime is pinned to what the tarball wants: freeze stamps
# the checkout file with the archive member's mtime and records a
# `frozen-mtime <ts>` extended header in the .projeny patch (and the status
# copy, which stays byte-identical to it), immediately after the `diff --git`
# line; every setup re-stamps the file; unfreeze drops the header (dropping a
# block that held nothing else).
T183="$ROOT/t183"
mkdir -p "$T183/w-1.0/src"
printf 'int alpha = 1;\n' > "$T183/w-1.0/src/a.c"
printf 'hello v1\n' > "$T183/w-1.0/README"
printf 'plain\n' > "$T183/w-1.0/src/b.c"
ln -s a.c "$T183/w-1.0/src/link"
chmod 755 "$T183/w-1.0/src/b.c"
touch -d @1700000100 "$T183/w-1.0/src/a.c"
touch -d @1700000200 "$T183/w-1.0/README"
touch -d @1700000300 "$T183/w-1.0/src/b.c"
touch -h -d @1700000300 "$T183/w-1.0/src/link"
(cd "$T183" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Frozen mtimes.\n' > "$T183/w.projeny"
run_in "$T183" expect_ok "freeze fixture setup" "$PROJENY" setup w.projeny
if [ "$(stat -c %Y "$T183/w/README")" = "1700000200" ]; then
    ok "setup preserves the tarball mtimes to freeze"
else
    fail "setup preserves the tarball mtimes to freeze" "$(stat -c %Y "$T183/w/README")"
fi
# Freeze two files (from inside the workdir, with a '.'-family project arg).
run_in "$T183/w" expect_ok "freeze two files" "$PROJENY" freeze-mtime .. README src/a.c
if grep -q '^diff --git a/w/README b/w/README$' "$T183/w.projeny" && \
   [ "$(grep -A1 '^diff --git a/w/README b/w/README$' "$T183/w.projeny" | sed -n 2p)" = \
     "frozen-mtime 1700000200" ]; then
    ok "frozen header sits right after the diff --git line"
else
    fail "frozen header sits right after the diff --git line" \
         "$(grep -A1 '^diff --git a/w/README' "$T183/w.projeny")"
fi
expect_file_contains "frozen value is the tarball member mtime" \
    "$T183/w.projeny" "frozen-mtime 1700000200"
expect_file_contains "second frozen file carries its own value" \
    "$T183/w.projeny" "frozen-mtime 1700000100"
if [ "$(grep -c '^frozen-mtime ' "$T183/w.projeny")" = "2" ]; then
    ok "exactly two frozen headers recorded"
else
    fail "exactly two frozen headers recorded" "$(grep -c '^frozen-mtime ' "$T183/w.projeny")"
fi
if [ "$(stat -c %Y "$T183/w/README")" = "1700000200" ] && \
   [ "$(stat -c %Y "$T183/w/src/a.c")" = "1700000100" ]; then
    ok "freeze stamps the workdir files with the tarball mtimes"
else
    fail "freeze stamps the workdir files with the tarball mtimes" \
         "$(stat -c '%Y %n' "$T183/w/README" "$T183/w/src/a.c")"
fi
expect_file_contains "status copy carries the frozen header too" \
    "$T183/.w.projeny.status" "frozen-mtime 1700000200"
# list-frozen-mtimes output: "<path> <ts>" per frozen file (sorted by path).
out="$($PROJENY list-frozen-mtimes "$T183/w.projeny")"
case "$out" in
*"README 1700000200"*)
    ok "list-frozen-mtimes prints path then timestamp"
    ;;
*)
    fail "list-frozen-mtimes prints path then timestamp" "out: $out"
    ;;
esac
case "$out" in
*"src/a.c 1700000100"*)
    ok "list-frozen-mtimes covers every frozen file"
    ;;
*)
    fail "list-frozen-mtimes covers every frozen file" "out: $out"
    ;;
esac
# Touching frozen files is undone by the next setup.
touch "$T183/w/README" "$T183/w/src/a.c"
if [ "$(stat -c %Y "$T183/w/README")" != "1700000200" ]; then
    ok "touch actually moved the mtime"
else
    fail "touch actually moved the mtime" "touch did not change the mtime"
fi
run_in "$T183" expect_ok "setup after touching frozen files" "$PROJENY" setup w.projeny
if [ "$(stat -c %Y "$T183/w/README")" = "1700000200" ] && \
   [ "$(stat -c %Y "$T183/w/src/a.c")" = "1700000100" ]; then
    ok "setup restores frozen mtimes"
else
    fail "setup restores frozen mtimes" \
         "$(stat -c '%Y %n' "$T183/w/README" "$T183/w/src/a.c")"
fi
# Unfreeze drops the header; an attribute-only block disappears whole.
run_in "$T183/w" expect_ok "unfreeze one file" "$PROJENY" unfreeze-mtime .. src/a.c
if [ "$(grep -c '^frozen-mtime ' "$T183/w.projeny")" = "1" ]; then
    ok "unfreeze removed the header"
else
    fail "unfreeze removed the header" "$(grep -c '^frozen-mtime ' "$T183/w.projeny")"
fi
if grep -q '^diff --git a/w/src/a.c ' "$T183/w.projeny"; then
    fail "unfreeze drops the block when nothing is left" "$(cat "$T183/w.projeny")"
else
    ok "unfreeze drops the block when nothing is left"
fi
run_in "$T183" expect_ok "list still finds the remaining freeze" \
    "$PROJENY" list-frozen-mtimes w.projeny
run_in "$T183" expect_fail "unfreezing a non-frozen file fails" \
    "$PROJENY" unfreeze-mtime w.projeny w/src/a.c
# Re-freezing is idempotent (same value) and works from the pdir too.
run_in "$T183" expect_ok "refreeze updates in place" \
    "$PROJENY" freeze-mtime w.projeny w/src/a.c
expect_file_contains "refreeze restored the header" "$T183/w.projeny" "frozen-mtime 1700000100"
# Error shapes: a directory, a symlink, an untracked file, a missing file.
run_in "$T183/w" expect_fail "freeze refuses a directory" "$PROJENY" freeze-mtime .. src
run_in "$T183/w" expect_fail "freeze refuses a symlink" "$PROJENY" freeze-mtime .. src/link
printf 'untracked\n' > "$T183/w/junk.c"
run_in "$T183/w" expect_fail "freeze refuses an untracked file" "$PROJENY" freeze-mtime .. junk.c
run_in "$T183/w" expect_fail "freeze refuses a missing file" "$PROJENY" freeze-mtime .. nope.c

# --------------- 184. frozen mtimes survive commit, rebase, and package
# commit and rebase regenerate the patch from scratch, so the frozen set has
# to be threaded through the diff (attribute-only blocks for unchanged
# files, the header on modified ones) and refreshed from the archive — after
# a rebase, from the NEW archive's members. package/extract then carry the
# frozen mtimes into their outputs.
T184="$ROOT/t184"
for v in 1.0 2.0; do
    mkdir -p "$T184/w-$v/src"
    printf "int alpha = $v;\n" > "$T184/w-$v/src/a.c"
    printf "line one v$v\n" > "$T184/w-$v/src/b.c"
    printf "hello v$v\n" > "$T184/w-$v/README"
    case $v in
    1.0)
        touch -d @1700000100 "$T184/w-$v/src/a.c"
        touch -d @1700000200 "$T184/w-$v/README"
        ;;
    2.0)
        touch -d @1700009900 "$T184/w-$v/src/a.c"
        touch -d @1700009800 "$T184/w-$v/README"
        ;;
    esac
    (cd "$T184" && tar -czf "w-$v.tar.gz" "w-$v")
    rm -rf "$T184/w-$v"
done
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Frozen roundtrip.\n' > "$T184/w.projeny"
run_in "$T184" expect_ok "roundtrip fixture setup" "$PROJENY" setup w.projeny
run_in "$T184" expect_ok "roundtrip freeze" "$PROJENY" freeze-mtime w.projeny w/src/a.c w/README
# A commit that regenerates nothing but frozen attribute-only blocks is a
# content no-op: both headers survive and the values stay at the tarball's
# mtimes.
run_in "$T184" expect_ok "commit with only frozen mtimes is a content no-op" \
    "$PROJENY" commit w.projeny
expect_file_contains "commit keeps the frozen header (modified never)" \
    "$T184/w.projeny" "frozen-mtime 1700000100"
expect_file_contains "commit keeps the frozen header (README)" \
    "$T184/w.projeny" "frozen-mtime 1700000200"
if [ "$(stat -c %Y "$T184/w/src/a.c")" = "1700000100" ]; then
    ok "commit does not disturb the frozen mtime"
else
    fail "commit does not disturb the frozen mtime" "$(stat -c %Y "$T184/w/src/a.c")"
fi
# An edit to a frozen file commits with the header on its modify block; the
# next setup keeps both the content and the pinned mtime.
printf 'frozen edit\n' >> "$T184/w/src/a.c"
run_in "$T184" expect_ok "commit a frozen file's edit" "$PROJENY" commit w.projeny
if grep -A1 '^diff --git a/w/src/a.c b/w/src/a.c$' "$T184/w.projeny" | \
   grep -q '^frozen-mtime 1700000100$'; then
    ok "the modify block of a frozen file carries the header"
else
    fail "the modify block of a frozen file carries the header" "$(cat "$T184/w.projeny")"
fi
run_in "$T184" expect_ok "setup after committing the frozen edit" "$PROJENY" setup w.projeny
if grep -q 'frozen edit' "$T184/w/src/a.c" && \
   [ "$(stat -c %Y "$T184/w/src/a.c")" = "1700000100" ]; then
    ok "setup keeps the committed edit and the frozen mtime"
else
    fail "setup keeps the committed edit and the frozen mtime" \
         "$(cat "$T184/w/src/a.c"; stat -c %Y "$T184/w/src/a.c")"
fi
# Freeze a file the patch never touched: its attribute-only block survives
# the next commit and setup.
printf 'never patched\n' > "$T184/w/extra.c"
run_in "$T184" expect_ok "add a fresh file" "$PROJENY" add w.projeny w/extra.c
touch -d @1700000500 "$T184/w/extra.c"
run_in "$T184" expect_ok "commit the fresh file" "$PROJENY" commit w.projeny
run_in "$T184" expect_ok "freeze the committed add" "$PROJENY" freeze-mtime w.projeny w/extra.c
expect_file_contains "the committed add is frozen at its own mtime" \
    "$T184/w.projeny" "frozen-mtime 1700000500"
run_in "$T184" expect_ok "setup keeps the frozen committed add" "$PROJENY" setup w.projeny
if [ -f "$T184/w/extra.c" ] && [ "$(stat -c %Y "$T184/w/extra.c")" = "1700000500" ]; then
    ok "committed add survives setup with its frozen mtime"
else
    fail "committed add survives setup with its frozen mtime" \
         "$(ls "$T184/w"; stat -c %Y "$T184/w/extra.c" 2>&1)"
fi
# Rebase to v2: the frozen values refresh to the NEW archive's mtimes and
# the workdir is re-stamped. The old values must be gone from the patch
# entirely (a stale ts would re-stamp the file backwards on the next setup).
run_in "$T184" expect_ok "rebase the frozen checkout" "$PROJENY" rebase w.projeny w-2.0.tar.gz
expect_file_contains "rebase refreshes the value from the new archive" \
    "$T184/w.projeny" "frozen-mtime 1700009900"
expect_file_not_contains "rebase drops the old archive's value (a.c)" \
    "$T184/w.projeny" "frozen-mtime 1700000100"
expect_file_not_contains "rebase drops the old archive's value (README)" \
    "$T184/w.projeny" "frozen-mtime 1700000200"
if [ "$(stat -c %Y "$T184/w/src/a.c")" = "1700009900" ]; then
    ok "rebase re-stamps the workdir to the new archive's mtime"
else
    fail "rebase re-stamps the workdir to the new archive's mtime" \
         "$(stat -c %Y "$T184/w/src/a.c")"
fi
if grep -q 'frozen edit' "$T184/w/src/a.c" && [ "$(cat "$T184/w/README")" = "hello v2.0" ]; then
    ok "rebase kept the committed edits too"
else
    fail "rebase kept the committed edits too" "$(cat "$T184/w/src/a.c" "$T184/w/README")"
fi
# package carries the frozen mtime into the output tarball.
run_in "$T184" expect_ok "package a frozen checkout" "$PROJENY" package w.projeny pkg.tar.gz
rm -rf "$T184/pk"
mkdir -p "$T184/pk"
(cd "$T184" && tar -xzf pkg.tar.gz -C pk)
if [ "$(stat -c %Y "$T184/pk/pkg/src/a.c")" = "1700009900" ]; then
    ok "package output preserves the frozen mtime"
else
    fail "package output preserves the frozen mtime" "$(stat -c %Y "$T184/pk/pkg/src/a.c")"
fi
run_in "$T184" expect_ok "extract a frozen checkout" "$PROJENY" extract w.projeny ext
if [ "$(stat -c %Y "$T184/ext/src/a.c")" = "1700009900" ]; then
    ok "extract output preserves the frozen mtime"
else
    fail "extract output preserves the frozen mtime" "$(stat -c %Y "$T184/ext/src/a.c")"
fi

# ------------------------------- 185. get-attributes
# Special attributes are frozen mtimes and nonstandard modes (any exec bit on
# a regular file). Files without special attributes are not printed; with no
# paths, every tracked file is considered; a directory arg recurses.
T185="$ROOT/t185"
mkdir -p "$T185/w-1.0/src/deep"
printf 'frozen content\n' > "$T185/w-1.0/src/a.c"
printf '#!/bin/sh\necho hi\n' > "$T185/w-1.0/src/tool.sh"
printf 'plain\n' > "$T185/w-1.0/src/b.c"
printf 'nested\n' > "$T185/w-1.0/src/deep/n.c"
printf 'hello v1\n' > "$T185/w-1.0/README"
ln -s tool.sh "$T185/w-1.0/src/link"
chmod 755 "$T185/w-1.0/src/tool.sh"
touch -d @1700000200 "$T185/w-1.0/src/a.c"
touch -d @1700000300 "$T185/w-1.0/README"
touch -h -d @1700000300 "$T185/w-1.0/src/link"
(cd "$T185" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Attributes.\n' > "$T185/w.projeny"
run_in "$T185" expect_ok "attributes fixture setup" "$PROJENY" setup w.projeny
run_in "$T185" expect_ok "freeze for attributes" "$PROJENY" freeze-mtime w.projeny w/src/a.c w/README
$PROJENY get-attributes "$T185/w.projeny" > "$ROOT/t185-all.out"
expect_file_contains "get-attributes reports the frozen file" \
    "$ROOT/t185-all.out" "src/a.c: frozen-mtime 1700000200"
expect_file_contains "get-attributes reports the frozen README" \
    "$ROOT/t185-all.out" "README: frozen-mtime 1700000300"
expect_file_contains "get-attributes reports the exec bit" \
    "$ROOT/t185-all.out" "src/tool.sh: mode 100755"
if grep -q "^src/b.c:" "$ROOT/t185-all.out"; then
    fail "get-attributes skips plain files" "$(cat "$ROOT/t185-all.out")"
else
    ok "get-attributes skips plain files"
fi
if grep -q "^src/link:" "$ROOT/t185-all.out"; then
    fail "get-attributes skips symlinks" "$(cat "$ROOT/t185-all.out")"
else
    ok "get-attributes skips symlinks"
fi
# A directory argument recurses; a single file prints only that file.
$PROJENY get-attributes "$T185/w.projeny" "$T185/w/src" > "$ROOT/t185-dir.out"
expect_file_contains "directory arg recurses" "$ROOT/t185-dir.out" "src/tool.sh: mode 100755"
if grep -q "^README:" "$ROOT/t185-dir.out"; then
    fail "directory arg stays inside the directory" "$(cat "$ROOT/t185-dir.out")"
else
    ok "directory arg stays inside the directory"
fi
$PROJENY get-attributes "$T185/w.projeny" "$T185/w/src/deep" > "$ROOT/t185-deep.out"
if [ -s "$ROOT/t185-deep.out" ]; then
    fail "an attribute-less subtree prints nothing" "$(cat "$ROOT/t185-deep.out")"
else
    ok "an attribute-less subtree prints nothing"
fi
$PROJENY get-attributes "$T185/w.projeny" "$T185/w/README" > "$ROOT/t185-one.out"
expect_file_contains "a single file arg prints just that file" \
    "$ROOT/t185-one.out" "README: frozen-mtime 1700000300"
if grep -q "src/a.c" "$ROOT/t185-one.out"; then
    fail "a single file arg does not list others" "$(cat "$ROOT/t185-one.out")"
else
    ok "a single file arg does not list others"
fi
out="$($PROJENY get-attributes "$T185/w.projeny" "$T185/w/nope" 2>&1 || true)"
case "$out" in
*"nope"*)
    ok "a missing path is an error naming it"
    ;;
*)
    fail "a missing path is an error naming it" "out: $out"
    ;;
esac
printf 'untracked\n' > "$T185/w/junk.c"
out="$($PROJENY get-attributes "$T185/w.projeny" "$T185/w/junk.c" 2>&1 || true)"
case "$out" in
*"not a tracked file"*)
    ok "an untracked path is an error naming it"
    ;;
*)
    fail "an untracked path is an error naming it" "out: $out"
    ;;
esac
# The whole-tree form via `.` from inside the workdir (feature A interplay).
run_in "$T185/w" expect_ok "get-attributes with the whole-tree dot form" \
    "$PROJENY" get-attributes . README

# ------------------- 186. package re-stamps the frozen file before staging
# package runs setup first, and setup re-stamps frozen files; so a workdir
# file whose mtime drifted after the freeze is repaired, and the output
# tarball carries the frozen ts (package -> setup -> stamp -> stage_tracked).
TPR="$ROOT/t186"
mkdir -p "$TPR/w-1.0/src"
printf 'int alpha = 1;\n' > "$TPR/w-1.0/src/a.c"
printf 'hello v1\n' > "$TPR/w-1.0/README"
touch -d @1700001100 "$TPR/w-1.0/src/a.c"
touch -d @1700001200 "$TPR/w-1.0/README"
(cd "$TPR" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Package restamp.\n' > "$TPR/w.projeny"
run_in "$TPR" expect_ok "package restamp fixture setup" "$PROJENY" setup w.projeny
run_in "$TPR" expect_ok "package restamp freeze" "$PROJENY" freeze-mtime w.projeny w/README
# Drift the workdir stamp away from the frozen value, then package.
touch -d @202012312359 "$TPR/w/README"
run_in "$TPR" expect_ok "package a drifted frozen checkout" "$PROJENY" package w.projeny out.tar.gz
if [ "$(stat -c %Y "$TPR/w/README")" = "1700001200" ]; then
    ok "package's setup re-stamped the drifted workdir file"
else
    fail "package's setup re-stamped the drifted workdir file" \
         "$(stat -c %Y "$TPR/w/README")"
fi
rm -rf "$TPR/unp"
mkdir -p "$TPR/unp"
(cd "$TPR" && tar -xzf out.tar.gz -C unp)
if [ "$(stat -c %Y "$TPR/unp/out/README")" = "1700001200" ]; then
    ok "package output member keeps the frozen ts"
else
    fail "package output member keeps the frozen ts" \
         "$(stat -c %Y "$TPR/unp/out/README")"
fi

# ------------------- 187. a rename moves the frozen pin to the destination
# The frozen map is keyed by the path the user froze (the rename SOURCE),
# but diff blocks carry the attribute on their live path (the DESTINATION).
# commit and rebase re-key the entry, so `freeze f; mv f g; commit` leaves
# g frozen at f's original ts and the next setup re-stamps g.
TRN="$ROOT/t187"
make_project "$TRN" w 1.0
run_in "$TRN" expect_ok "rename-pin fixture setup" "$PROJENY" setup w.projeny
run_in "$TRN" expect_ok "rename-pin freeze" "$PROJENY" freeze-mtime w.projeny w/src/a.c
PINNED="$("$PROJENY" list-frozen-mtimes "$TRN/w.projeny" | awk '{print $2}')"
if [ -n "$PINNED" ]; then
    ok "rename-pin read the frozen ts"
else
    fail "rename-pin read the frozen ts" \
         "$("$PROJENY" list-frozen-mtimes "$TRN/w.projeny" 2>&1)"
fi
run_in "$TRN" expect_ok "rename-pin mv" "$PROJENY" mv w.projeny w/src/a.c w/src/renamed.c
run_in "$TRN" expect_ok "rename-pin commit" "$PROJENY" commit w.projeny
out="$("$PROJENY" list-frozen-mtimes "$TRN/w.projeny" 2>&1)"
case "$out" in
*"src/renamed.c $PINNED"*)
    ok "commit moves the pin to the rename destination with the original ts"
    ;;
*)
    fail "commit moves the pin to the rename destination with the original ts" \
         "want src/renamed.c $PINNED, got: $out"
    ;;
esac
case "$out" in
*"src/a.c "*)
    fail "the moved-away path no longer holds the pin" "$out"
    ;;
*)
    ok "the moved-away path no longer holds the pin"
    ;;
esac
touch -d @2000000000 "$TRN/w/src/renamed.c"
run_in "$TRN" expect_ok "rename-pin setup" "$PROJENY" setup w.projeny
if [ "$(stat -c %Y "$TRN/w/src/renamed.c")" = "$PINNED" ]; then
    ok "setup re-stamps the renamed destination"
else
    fail "setup re-stamps the renamed destination" \
         "$(stat -c %Y "$TRN/w/src/renamed.c")"
fi

# ------------------- 188. rm then commit prunes the pin
# A delete block never carries the header, so `freeze f; rm f; commit`
# drops the attribute entirely: list-frozen-mtimes hard-errors.
TRP="$ROOT/t188"
make_project "$TRP" w 1.0
run_in "$TRP" expect_ok "rm-pin fixture setup" "$PROJENY" setup w.projeny
run_in "$TRP" expect_ok "rm-pin freeze" "$PROJENY" freeze-mtime w.projeny w/src/b.c
run_in "$TRP" expect_ok "rm-pin rm" "$PROJENY" rm w.projeny w/src/b.c
run_in "$TRP" expect_ok "rm-pin commit" "$PROJENY" commit w.projeny
out="$("$PROJENY" list-frozen-mtimes "$TRP/w.projeny" 2>&1 || true)"
case "$out" in
*"no frozen mtimes"*)
    ok "rm then commit prunes the pin"
    ;;
*)
    fail "rm then commit prunes the pin" "out: $out"
    ;;
esac

# ------------------- 189. a symlinked parent resolves physically
# normalize_lexical collapses ".." lexically, so `sym/..` through a
# symlinked intermediate used to examine the wrong directory (silently
# resolving setup/status against a sibling project). An argument that
# exists on disk is resolved with realpath first: `l/w/..` names the
# physical target dir, whose <dir>.projeny sibling is the real project.
TSYM="$ROOT/t189"
mkdir -p "$TSYM/target" "$TSYM/other"
make_project "$TSYM/target" w 1.0
ln -sfn ../target "$TSYM/other/l"
# A decoy project file in the lexically-collapsed directory: resolving it
# would be a silent wrong-project answer.
printf 'Name: decoy\n' > "$TSYM/other/decoy.projeny"
run_in "$TSYM/target" expect_ok "symlink fixture setup" "$PROJENY" setup w.projeny
run_in "$TSYM/other" expect_out "status resolves through the symlinked parent" \
    "Status: setup" "$PROJENY" status l/w/..
run_in "$TSYM/other" expect_ok "setup resolves through the symlinked parent" \
    "$PROJENY" setup l/w/..
# `l/..` physically names the target's parent, which holds no .projeny at
# all: refuse loudly rather than silently picking the decoy the lexical
# collapse would have found.
out="$(cd "$TSYM/other" && "$PROJENY" status l/.. 2>&1 || true)"
case "$out" in
*"directory holds no .projeny file"*)
    ok "status through the symlink refuses rather than picking the decoy"
    ;;
*"decoy"*)
    fail "status through the symlink refuses rather than picking the decoy" \
         "resolved the decoy: $out"
    ;;
*)
    fail "status through the symlink refuses rather than picking the decoy" \
         "out: $out"
    ;;
esac

# ------------------- 190. mv from inside the moved directory
# cmd_mv absolutizes the project path up front, so moving the very
# directory the CWD sits in cannot re-point the relative status-file path
# mid-command (pre-fix, the rename was applied but never recorded).
TMV="$ROOT/t190"
make_project "$TMV" w 1.0
run_in "$TMV" expect_ok "mv-inside fixture setup" "$PROJENY" setup w.projeny
run_in "$TMV/w/src" expect_ok "mv the directory the CWD sits in" \
    "$PROJENY" mv .. . ../src2
run_in "$TMV" expect_out "the rename is recorded" "Renamed: src -> src2" \
    "$PROJENY" status w.projeny
run_in "$TMV" expect_ok "commit folds the recorded rename" "$PROJENY" commit w.projeny

# ------------------- 191. duplicate freeze/unfreeze arguments count once
TDUP="$ROOT/t191"
make_project "$TDUP" w 1.0
run_in "$TDUP" expect_ok "dup-args fixture setup" "$PROJENY" setup w.projeny
run_in "$TDUP" expect_out "duplicate freeze args count once" \
    "froze the mtime of 1 file(s)" \
    "$PROJENY" freeze-mtime w.projeny w/README w/README
run_in "$TDUP" expect_out "duplicate unfreeze args count once" \
    "unfroze the mtime of 1 file(s)" \
    "$PROJENY" unfreeze-mtime w.projeny w/README w/README

# ------------------- 192. frozen-mtime negative paths
TNEG="$ROOT/t192"
make_project "$TNEG" w 1.0
run_in "$TNEG" expect_ok "negatives fixture setup" "$PROJENY" setup w.projeny
# A file registered as removed is refused even though it exists on disk
# again (the user re-created it): the removal still owns the path.
run_in "$TNEG" expect_ok "negatives rm" "$PROJENY" rm w.projeny w/src/b.c
printf 'recreated\n' > "$TNEG/w/src/b.c"
out="$(cd "$TNEG" && "$PROJENY" freeze-mtime w.projeny w/src/b.c 2>&1 || true)"
case "$out" in
*"removed or renamed away"*)
    ok "freezing a pending-removed file is refused"
    ;;
*)
    fail "freezing a pending-removed file is refused" "out: $out"
    ;;
esac
# list-frozen-mtimes with nothing frozen hard-errors (empty-output policy).
TNEG2="$ROOT/t192b"
make_project "$TNEG2" w 1.0
run_in "$TNEG2" expect_ok "empty-pin fixture setup" "$PROJENY" setup w.projeny
out="$(cd "$TNEG2" && "$PROJENY" list-frozen-mtimes w.projeny 2>&1 || true)"
case "$out" in
*"no frozen mtimes"*)
    ok "list-frozen-mtimes with nothing frozen dies"
    ;;
*)
    fail "list-frozen-mtimes with nothing frozen dies" "out: $out"
    ;;
esac
# get-attributes on a never-set-up project errors.
TNEG3="$ROOT/t192c"
make_project "$TNEG3" w 1.0
out="$(cd "$TNEG3" && "$PROJENY" get-attributes w.projeny 2>&1 || true)"
case "$out" in
*"run setup first"*)
    ok "get-attributes on a never-set-up project errors"
    ;;
*)
    fail "get-attributes on a never-set-up project errors" "out: $out"
    ;;
esac
# A malformed frozen-mtime header dies instead of silently disabling the
# attribute: overflow used to clamp to ULLONG_MAX, garbage used to map to 0
# (= no header). Freeze to plant a valid header, corrupt it, then check.
TNEG4="$ROOT/t192d"
make_project "$TNEG4" w 1.0
run_in "$TNEG4" expect_ok "malformed-header fixture setup" "$PROJENY" setup w.projeny
run_in "$TNEG4" expect_ok "malformed-header freeze" "$PROJENY" freeze-mtime w.projeny w/README
sed -i 's/^frozen-mtime [0-9][0-9]*$/frozen-mtime 99999999999999999999999/' "$TNEG4/w.projeny"
out="$(cd "$TNEG4" && "$PROJENY" list-frozen-mtimes w.projeny 2>&1 || true)"
case "$out" in
*"malformed frozen-mtime header"*)
    ok "an overflowing frozen-mtime header is rejected"
    ;;
*)
    fail "an overflowing frozen-mtime header is rejected" "out: $out"
    ;;
esac
sed -i 's/^frozen-mtime .*/frozen-mtime garbage/' "$TNEG4/w.projeny"
out="$(cd "$TNEG4" && "$PROJENY" get-attributes w.projeny 2>&1 || true)"
case "$out" in
*"malformed frozen-mtime header"*)
    ok "a non-numeric frozen-mtime header is rejected"
    ;;
*)
    fail "a non-numeric frozen-mtime header is rejected" "out: $out"
    ;;
esac
# A valid value parses again (only malformed values are refused).
sed -i 's/^frozen-mtime .*/frozen-mtime 1700001234/' "$TNEG4/w.projeny"
run_in "$TNEG4" expect_ok "a valid header still lists" \
    "$PROJENY" list-frozen-mtimes w.projeny

# ------------------- 193. get-attributes report shape
# Sorted output; a file holding both attributes prints both lines (frozen
# first, then the current-workdir mode); untracked files inside a directory
# scope are filtered out; an attribute-less scope is silent.
TGA="$ROOT/t193"
mkdir -p "$TGA/w-1.0/src/deep"
printf 'frozen content\n' > "$TGA/w-1.0/src/a.c"
printf '#!/bin/sh\necho hi\n' > "$TGA/w-1.0/src/tool.sh"
printf 'plain\n' > "$TGA/w-1.0/src/b.c"
printf 'nested\n' > "$TGA/w-1.0/src/deep/n.c"
printf 'hello v1\n' > "$TGA/w-1.0/README"
(cd "$TGA" && tar -czf w-1.0.tar.gz w-1.0 && rm -rf w-1.0)
printf 'Archive: w-1.0.tar.gz\nOrigname: w-1.0\nName: w\n\n    Report shape.\n' > "$TGA/w.projeny"
run_in "$TGA" expect_ok "report-shape fixture setup" "$PROJENY" setup w.projeny
run_in "$TGA" expect_ok "report-shape freeze" \
    "$PROJENY" freeze-mtime w.projeny w/src/a.c w/README
chmod 755 "$TGA/w/src/a.c"
$PROJENY get-attributes "$TGA/w.projeny" > "$ROOT/t193-all.out"
if [ "$(cat "$ROOT/t193-all.out")" = "$(LC_ALL=C sort "$ROOT/t193-all.out")" ] &&
   [ -s "$ROOT/t193-all.out" ]; then
    ok "get-attributes output is sorted"
else
    fail "get-attributes output is sorted" "$(cat "$ROOT/t193-all.out")"
fi
expect_file_contains "a frozen exec file reports its frozen-mtime" \
    "$ROOT/t193-all.out" "src/a.c: frozen-mtime "
expect_file_contains "a frozen exec file reports its current mode" \
    "$ROOT/t193-all.out" "src/a.c: mode 100755"
if [ "$(grep -n '^src/a.c: frozen-mtime' "$ROOT/t193-all.out" | cut -d: -f1)" -lt \
     "$(grep -n '^src/a.c: mode' "$ROOT/t193-all.out" | cut -d: -f1)" ]; then
    ok "the frozen-mtime line precedes the mode line"
else
    fail "the frozen-mtime line precedes the mode line" \
         "$(cat "$ROOT/t193-all.out")"
fi
# An untracked file inside a directory scope is filtered from the report.
printf 'junk\n' > "$TGA/w/src/junk.c"
$PROJENY get-attributes "$TGA/w.projeny" "$TGA/w/src" > "$ROOT/t193-dir.out"
if grep -q "junk.c" "$ROOT/t193-dir.out"; then
    fail "an untracked file inside a scope is filtered" \
         "$(cat "$ROOT/t193-dir.out")"
else
    ok "an untracked file inside a scope is filtered"
fi
run_in "$TGA" expect_no_out "an attribute-less scope is silent" \
    "$PROJENY" get-attributes w.projeny w/src/deep

# ------------------- 194. rebase refuses on unresolved conflicts
# The conflicts die in cmd_rebase fires before any tree work; `resolve`
# with a '.' project arg then clears the entry.
TRB="$ROOT/t194"
make_project "$TRB" w 1.0
run_in "$TRB" expect_ok "rebase-refusal fixture setup" "$PROJENY" setup w.projeny
printf 'alpha = 1\nbeta LOCAL\ngamma = 1\n' > "$TRB/w/src/a.c"
run_in "$TRB" expect_ok "rebase-refusal local commit" "$PROJENY" commit w.projeny
cp "$TRB/w.projeny" "$ROOT/t194-local.projeny"
U194="$ROOT/t194up"
mkdir -p "$U194"
cp "$TRB/w-1.0.tar.gz" "$U194/"
cp "$ROOT/t194-local.projeny" "$U194/w.projeny"
(cd "$U194" && "$PROJENY" setup w.projeny >/dev/null 2>&1)
printf 'alpha = 1\nbeta UPSTREAM\ngamma = 1\n' > "$U194/w/src/a.c"
(cd "$U194" && "$PROJENY" commit w.projeny >/dev/null 2>&1)
cp "$U194/w.projeny" "$ROOT/t194-up.projeny"
make_conflicted "$ROOT/t194-local.projeny" "$ROOT/t194-up.projeny" "$TRB/w.projeny"
run_in "$TRB" expect_fail "conflicted setup exits nonzero" "$PROJENY" setup w.projeny
expect_file_contains "the conflict is recorded" \
    "$TRB/.w.projeny.status" "Conflict: src/a.c"
out="$(cd "$TRB" && "$PROJENY" rebase w.projeny w-1.0.tar.gz 2>&1 || true)"
case "$out" in
*"unresolved conflicts; resolve them before rebasing"*)
    ok "rebase refuses while conflicts are unresolved"
    ;;
*)
    fail "rebase refuses while conflicts are unresolved" "out: $out"
    ;;
esac
run_in "$TRB/w" expect_ok "resolve with a dot project arg" \
    "$PROJENY" resolve . src/a.c
if grep -q "Conflict:" "$TRB/.w.projeny.status"; then
    fail "the dot resolve cleared the conflict" \
         "$(cat "$TRB/.w.projeny.status")"
else
    ok "the dot resolve cleared the conflict"
fi

# ------------------- 195. package output naming: short suffixes and degenerates
TSUF="$ROOT/t195"
make_project "$TSUF" pkg 1.0
run_in "$TSUF" expect_ok "short-suffix fixture setup" "$PROJENY" setup pkg.projeny
for ext in tbz tbz2 txz; do
    run_in "$TSUF" expect_ok "package .$ext exits 0" "$PROJENY" package pkg.projeny "o.$ext"
done
if command -v zstd >/dev/null 2>&1 && tar --help 2>/dev/null | grep -q -- --zstd; then
    run_in "$TSUF" expect_ok "package .tzst exits 0" "$PROJENY" package pkg.projeny o.tzst
else
    ok "zstd absent; .tzst check skipped"
fi
(cd "$TSUF" && tar -tjf o.tbz | sort > s.tbz && tar -tjf o.tbz2 | sort > s.tbz2 && tar -tJf o.txz | sort > s.txz)
if cmp -s "$TSUF/s.tbz" "$TSUF/s.tbz2" && cmp -s "$TSUF/s.tbz" "$TSUF/s.txz"; then
    ok "the short suffixes hold the same members"
else
    fail "the short suffixes hold the same members" "$(cat "$TSUF/s.tbz" 2>&1)"
fi
# Degenerate names: the stem before the suffix would be empty or dotted.
run_in "$TSUF" expect_fail "a bare .tar.gz output name dies" \
    "$PROJENY" package pkg.projeny .tar.gz
run_in "$TSUF" expect_fail "a dotted prefix dies" \
    "$PROJENY" package pkg.projeny ..tar.gz
if [ -e "$TSUF/.tar.gz" ] || [ -e "$TSUF/..tar.gz" ]; then
    fail "failed package writes no archive" "$(ls -a "$TSUF")"
else
    ok "failed package writes no archive"
fi

# ------------------- 196. help topics for the attribute commands
for t in "package:tracked files" "extract:dest-dir" "freeze-mtime:frozen-mtime" \
         "unfreeze-mtime:frozen-mtime" "list-frozen-mtimes:Hard-errors" \
         "get-attributes:mode 100755"; do
    topic="${t%%:*}"
    want="${t##*:}"
    if "$PROJENY" help "$topic" 2>&1 | grep -qF -- "$want"; then
        ok "help $topic explains '$want'"
    else
        fail "help $topic explains '$want'"
    fi
done
run_in "$ROOT/t195" expect_out "help freeze-mtime mentions frozen-mtime" \
    "frozen-mtime" "$PROJENY" help freeze-mtime

# ------------------- 197. '.' and '..' project arguments on the rest
# §181 covers setup/status/add/commit/diff; the remaining commands accept
# the same forms. package/extract from inside the workdir replace that
# workdir (and the CWD) mid-command, so their output/destination args must
# be absolute — the project argument itself is absolutized up front.
TDOT="$ROOT/t197"
make_project "$TDOT" w 1.0
mkdir -p "$TDOT/w-2.0/src"
printf 'int alpha = 2;\n' > "$TDOT/w-2.0/src/a.c"
printf 'line one v2\n' > "$TDOT/w-2.0/src/b.c"
printf 'hello v2\n' > "$TDOT/w-2.0/README"
(cd "$TDOT" && tar -czf w-2.0.tar.gz w-2.0 && rm -rf w-2.0)
run_in "$TDOT" expect_ok "dot-args fixture setup" "$PROJENY" setup w.projeny
run_in "$TDOT/w" expect_ok "rm with a dot project arg" "$PROJENY" rm . src/b.c
run_in "$TDOT/w" expect_ok "rebase with a dot project arg" \
    "$PROJENY" rebase . ../w-2.0.tar.gz
run_in "$TDOT/w" expect_ok "package with a dot project arg" \
    "$PROJENY" package . "$TDOT/pkg-out.tar.gz"
run_in "$TDOT/w" expect_ok "extract with a dot project arg" \
    "$PROJENY" extract . "$TDOT/ext-out"
if [ -f "$TDOT/ext-out/src/a.c" ]; then
    ok "the dot extract produced the tree"
else
    fail "the dot extract produced the tree" "$(ls "$TDOT/ext-out" 2>&1)"
fi
run_in "$TDOT/w/src" expect_ok "get-attributes with a dotdot project arg" \
    "$PROJENY" get-attributes .. w/README

# ------------------- 198. patch reports the exact conflict count
TPC="$ROOT/t198"
mkdir -p "$TPC/A" "$TPC/B"
printf 'one\ntwo\nthree\n' > "$TPC/A/f.c"
printf 'one\nTWO\nthree\n' > "$TPC/B/f.c"
(cd "$TPC" && "$PROJENY" diff A B > f.diff 2>&1)
mkdir -p "$TPC/T"
printf 'one\nCHANGED\nthree\n' > "$TPC/T/f.c"
out="$(cd "$TPC" && "$PROJENY" patch T f.diff 2>&1 || true)"
case "$out" in
*"patched 'T' with 1 conflict(s):"*)
    ok "patch reports the exact conflict count"
    ;;
*)
    fail "patch reports the exact conflict count" "out: $out"
    ;;
esac
case "$out" in
*"  f.c"*)
    ok "patch names the conflicted file as a bullet"
    ;;
*)
    fail "patch names the conflicted file as a bullet" "out: $out"
    ;;
esac

# ------------------- 199. conflicted setup still stamps frozen mtimes
# Every setup that can parse the .projeny file runs the frozen-mtime stamp
# pass — including a setup that ends in workdir conflicts — so a frozen
# file keeps the archive's mtime even when its content ends up with
# conflict markers. Only a .projeny file that itself contains git conflict
# markers defers stamping to the next clean setup.
T199="$ROOT/t199"
mkdir -p "$T199/fake-1.0/src"
cat > "$T199/fake-1.0/src/a.c" <<'EOF'
int alpha = 1;

int beta = 1;

int gamma = 1;

int delta = 1;
EOF
printf 'line one v1\n' > "$T199/fake-1.0/src/b.c"
printf 'hello v1\n' > "$T199/fake-1.0/README"
touch -d @1700001500 "$T199/fake-1.0/src/a.c"
(cd "$T199" && tar -czf fake-1.0.tar.gz fake-1.0 && rm -rf fake-1.0)
write_projeny "$T199" fake 1.0 fake
run_in "$T199" expect_ok "conflict-stamp fixture setup" "$PROJENY" setup fake.projeny
run_in "$T199" expect_ok "conflict-stamp freeze" \
    "$PROJENY" freeze-mtime fake.projeny fake/src/a.c
if [ "$(stat -c %Y "$T199/fake/src/a.c")" = "1700001500" ]; then
    ok "the freeze pinned the archive's member mtime"
else
    fail "the freeze pinned the archive's member mtime" \
         "$(stat -c %Y "$T199/fake/src/a.c")"
fi
# local committed change (beta region), like section 4
python3 - "$T199/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 10;")
open(p, "w").write(s)
EOF
run_in "$T199" expect_ok "conflict-stamp local commit" "$PROJENY" commit fake.projeny
cp "$T199/fake.projeny" "$ROOT/t199-local.projeny"
rm -rf "$T199/fake" "$T199/.fake.projeny.status"
cp "$ROOT/t199-local.projeny" "$T199/fake.projeny"
run_in "$T199" expect_ok "conflict-stamp re-setup" "$PROJENY" setup fake.projeny
# uncommitted edit to the same region the upstream patch rewrites, then the
# upstream twin: same base, same region changed the other way (section 4's
# conflict recipe), so the re-setup's 3-way merge conflicts on the FROZEN
# file.
python3 - "$T199/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 999;")
open(p, "w").write(s)
EOF
U199="$ROOT/t199up"
mkdir -p "$U199"
cp "$T199/fake-1.0.tar.gz" "$U199/"
cp "$ROOT/t199-local.projeny" "$U199/fake.projeny"
(cd "$U199" && "$PROJENY" setup fake.projeny >/dev/null 2>&1)
python3 - "$U199/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 10;", "int beta = 555;")
open(p, "w").write(s)
EOF
run_in "$U199" expect_ok "upstream twin commit" "$PROJENY" commit fake.projeny
cp "$U199/fake.projeny" "$T199/fake.projeny"
run_in "$T199" expect_fail "conflicted re-setup exits nonzero" \
    "$PROJENY" setup fake.projeny
expect_file_contains "conflicted workdir has markers" "$T199/fake/src/a.c" "<<<<<<<"
if [ "$(stat -c %Y "$T199/fake/src/a.c")" = "1700001500" ]; then
    ok "conflicted setup still stamps the frozen file"
else
    fail "conflicted setup still stamps the frozen file" \
         "$(stat -c %Y "$T199/fake/src/a.c")"
fi
expect_file_contains "the taken patch keeps the frozen-mtime header" \
    "$T199/fake.projeny" "frozen-mtime"
# resolve by taking the upstream side (the patch's own content), so the
# next setup is clean; the stamp pass keeps the frozen ts.
printf 'int alpha = 1;\n\nint beta = 555;\n\nint gamma = 1;\n\nint delta = 1;\n' \
    > "$T199/fake/src/a.c"
run_in "$T199" expect_ok "resolve the frozen conflict" \
    "$PROJENY" resolve fake.projeny fake/src/a.c
run_in "$T199" expect_ok "clean setup after resolve" "$PROJENY" setup fake.projeny
if [ "$(stat -c %Y "$T199/fake/src/a.c")" = "1700001500" ]; then
    ok "clean setup after resolve keeps the frozen ts"
else
    fail "clean setup after resolve keeps the frozen ts" \
         "$(stat -c %Y "$T199/fake/src/a.c")"
fi

# ------------------- 200. commit warns when a frozen file becomes a symlink
# Freezing is a regular-file attribute, so a frozen file the workdir turned
# into a symlink loses its pin when commit regenerates the patch — loudly,
# never silently (vcs.h's standard for dropped attributes).
T200="$ROOT/t200"
make_project "$T200" tiny 3
run_in "$T200" expect_ok "symlink-pin fixture setup" "$PROJENY" setup tiny.projeny
run_in "$T200" expect_ok "symlink-pin freeze" \
    "$PROJENY" freeze-mtime tiny.projeny tiny/README
rm "$T200/tiny/README"
ln -s src/a.c "$T200/tiny/README"
run_in "$T200" expect_out "commit warns about the dropped frozen mtime" \
    "dropping the frozen mtime for 'README'" "$PROJENY" commit tiny.projeny
run_in "$T200" expect_fail "the typechanged file no longer holds the pin" \
    "$PROJENY" list-frozen-mtimes tiny.projeny
expect_file_not_contains "the committed patch carries no frozen-mtime header" \
    "$T200/tiny.projeny" "frozen-mtime"
# A pending mv whose destination is now a symlink rides the same skip path:
# symlink flips never pair as renames, so the move renders as delete+add
# and the re-keyed pin dies on the add block — with the same warning.
T200B="$ROOT/t200b"
make_project "$T200B" tiny 3
run_in "$T200B" expect_ok "rename-to-symlink fixture setup" "$PROJENY" setup tiny.projeny
run_in "$T200B" expect_ok "rename-to-symlink freeze" \
    "$PROJENY" freeze-mtime tiny.projeny tiny/README
run_in "$T200B" expect_ok "rename-to-symlink mv" \
    "$PROJENY" mv tiny.projeny tiny/README tiny/RENAMED
rm "$T200B/tiny/RENAMED"
ln -s src/a.c "$T200B/tiny/RENAMED"
run_in "$T200B" expect_out "commit warns about the re-keyed pin on the symlink" \
    "dropping the frozen mtime for 'RENAMED'" "$PROJENY" commit tiny.projeny
run_in "$T200B" expect_fail "no pins left after the rename-to-symlink commit" \
    "$PROJENY" list-frozen-mtimes tiny.projeny

# --------------------------- 201. the hash command (blake3)
# `projeny hash <file>` prints the digest a URL: header wants: exactly 64
# lowercase hex chars and nothing else, so the output pastes verbatim into
# the header. Pinned to two hardcoded digests (the empty string and a fixed
# two-line payload, both verified against the checked-in b3sum and an
# independent libblake3 build) so the suite is not fully self-referential.
T201="$ROOT/t201"
mkdir -p "$T201"
: > "$T201/empty"
out="$("$PROJENY" hash "$T201/empty")"
if [ "$out" = "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262" ]; then
    ok "hash of the empty file is the known blake3 digest"
else
    fail "hash of the empty file is the known blake3 digest" "out: $out"
fi
printf 'The quick brown fox jumps over the lazy dog\nfil-c projeny link test payload\n' \
    > "$T201/payload"
out="$("$PROJENY" hash "$T201/payload")"
if [ "$out" = "e539b337a62dc7631c09a87fe35954fef4489097b161c620e1f9fe8e16b5d044" ]; then
    ok "hash of a fixed payload is the known blake3 digest"
else
    fail "hash of a fixed payload is the known blake3 digest" "out: $out"
fi
if [ "${#out}" -eq 64 ]; then
    ok "the digest is exactly 64 chars"
else
    fail "the digest is exactly 64 chars" "len=${#out} out: $out"
fi
if [ -z "$(printf '%s' "$out" | tr -d '0-9a-f')" ]; then
    ok "the digest is lowercase hex and nothing else"
else
    fail "the digest is lowercase hex and nothing else" "out: $out"
fi
expect_fail "hash refuses a directory" "$PROJENY" hash "$T201"
expect_fail "hash refuses a missing file" "$PROJENY" hash "$T201/no-such-file-XYZ"

# ---------------------- 202. fresh setup from a file:// URL
# A URL:-based .projeny replaces Archive: with "URL: <url> <blake3-hash>"
# lines. The archive name is derived from the FIRST URL's basename and the
# download is cached byte-exact as the dotted .snapshot next to the .projeny
# file. file:// + an absolute path gives the correct triple slash, which
# keeps this suite hermetic (curl reads it straight off the filesystem).
T202="$ROOT/t202"
make_tarballs "$T202" fake
h202="$("$PROJENY" hash "$T202/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    URL-based project.\n\n' \
    "$T202" "$h202" > "$T202/fake.projeny"
out="$(cd "$T202" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "fresh setup from a file:// URL exits 0"
else
    fail "fresh setup from a file:// URL exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"set up 'fake' from 'fake-1.0.tar.gz'"*)
    ok "setup names the URL-derived archive"
    ;;
*)
    fail "setup names the URL-derived archive" "out: $out"
    ;;
esac
case "$out" in
*warning*)
    fail "a good first download prints no warning" "out: $out"
    ;;
*)
    ok "a good first download prints no warning"
    ;;
esac
if [ -f "$T202/fake/src/a.c" ] && [ -f "$T202/fake/README" ]; then
    ok "URL setup creates the workdir"
else
    fail "URL setup creates the workdir" "ls: $(ls -R "$T202" 2>&1)"
fi
expect_file_contains "URL setup workdir has v1 content" "$T202/fake/README" "hello v1"
if [ -f "$T202/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the snapshot is named after the URL basename"
else
    fail "the snapshot is named after the URL basename" "ls: $(ls -A "$T202" 2>&1)"
fi
if cmp -s "$T202/.fake-1.0.tar.gz.snapshot" "$T202/fake-1.0.tar.gz"; then
    ok "the snapshot is a byte-exact copy of the download"
else
    fail "the snapshot is a byte-exact copy of the download" "cmp: snapshot vs tarball"
fi
expect_file_contains "the status embeds the URL line" \
    "$T202/.fake.projeny.status" "URL: file://$T202/fake-1.0.tar.gz"

# ------------- 203. re-setup hits no network while the snapshot matches
# When the .snapshot exists and its blake3 matches ANY URL: hash, it IS the
# archive: setup must not download at all. Move the tarball (and every other
# copy) out of the way and re-setup — success with no warning proves the
# snapshot alone satisfied the URL: headers.
T203="$ROOT/t203"
make_tarballs "$T203" fake
h203="$("$PROJENY" hash "$T203/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    No network wanted.\n\n' \
    "$T203" "$h203" > "$T203/fake.projeny"
run_in "$T203" expect_ok "no-network fixture setup" "$PROJENY" setup fake.projeny
mv "$T203/fake-1.0.tar.gz" "$ROOT/t203-kept-fake-1.0.tar.gz"
out="$(cd "$T203" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "re-setup without the tarball exits 0"
else
    fail "re-setup without the tarball exits 0" "exit=$rc out: $out"
fi
case "$out" in
*warning*)
    fail "re-setup without the tarball downloads nothing" "out: $out"
    ;;
*)
    ok "re-setup without the tarball downloads nothing"
    ;;
esac
if cmp -s "$T203/.fake-1.0.tar.gz.snapshot" "$ROOT/t203-kept-fake-1.0.tar.gz"; then
    ok "the snapshot is untouched by the no-network re-setup"
else
    fail "the snapshot is untouched by the no-network re-setup" "cmp: snapshot vs kept tarball"
fi
expect_file_contains "the no-network re-setup keeps v1 content" \
    "$T203/fake/README" "hello v1"

# ------------------------- 204. corrupted snapshot self-heals
# A snapshot that matches no URL: hash (truncated, clobbered, stale) is not
# trusted: setup warns with the hash guidance and re-downloads, restoring the
# snapshot byte-exact.
T204="$ROOT/t204"
make_tarballs "$T204" fake
h204="$("$PROJENY" hash "$T204/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Self healing.\n\n' \
    "$T204" "$h204" > "$T204/fake.projeny"
run_in "$T204" expect_ok "self-heal fixture setup" "$PROJENY" setup fake.projeny
printf 'garbage that is no tarball\n' > "$T204/.fake-1.0.tar.gz.snapshot"
out="$(cd "$T204" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup re-downloads over a corrupted snapshot"
else
    fail "setup re-downloads over a corrupted snapshot" "exit=$rc out: $out"
fi
case "$out" in
*warning*)
    ok "the corrupted snapshot setup warns"
    ;;
*)
    fail "the corrupted snapshot setup warns" "out: $out"
    ;;
esac
case "$out" in
*"does not match any URL: hash"*)
    ok "the warning names the hash mismatch"
    ;;
*)
    fail "the warning names the hash mismatch" "out: $out"
    ;;
esac
if cmp -s "$T204/.fake-1.0.tar.gz.snapshot" "$T204/fake-1.0.tar.gz"; then
    ok "the snapshot is restored byte-exact"
else
    fail "the snapshot is restored byte-exact" "cmp: snapshot vs tarball"
fi
expect_file_contains "the self-healed checkout has v1 content" \
    "$T204/fake/README" "hello v1"

# ------------- 205. multiple URLs: the first unreachable, the second good
# URL: lines are tried in listed order; a download failure warns and falls
# through to the next. The snapshot is named after the FIRST URL's basename
# even when a later URL supplied the bytes.
T205="$ROOT/t205"
make_tarballs "$T205" fake
h205="$("$PROJENY" hash "$T205/fake-1.0.tar.gz")"
printf 'URL: file://%s/nonexistent-XYZ.tar.gz %s\nURL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Two places.\n\n' \
    "$T205" "$h205" "$T205" "$h205" > "$T205/fake.projeny"
out="$(cd "$T205" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "fallback over an unreachable first URL exits 0"
else
    fail "fallback over an unreachable first URL exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"could not download 'file://$T205/nonexistent-XYZ.tar.gz'"*)
    ok "the failed URL warns with its reason"
    ;;
*)
    fail "the failed URL warns with its reason" "out: $out"
    ;;
esac
case "$out" in
*"set up 'fake' from 'nonexistent-XYZ.tar.gz'"*)
    ok "the archive name still derives from the first URL"
    ;;
*)
    fail "the archive name still derives from the first URL" "out: $out"
    ;;
esac
if cmp -s "$T205/.nonexistent-XYZ.tar.gz.snapshot" "$T205/fake-1.0.tar.gz"; then
    ok "the second URL's bytes land in the snapshot byte-exact"
else
    fail "the second URL's bytes land in the snapshot byte-exact" \
         "cmp: snapshot vs tarball"
fi
expect_file_contains "the fallback checkout has v1 content" \
    "$T205/fake/README" "hello v1"

# ------------------- 206. multiple URLs: every URL unreachable
# Only when EVERY URL: line fails does setup hard-error; the error reports
# how many lines were tried, and no snapshot is left behind.
T206="$ROOT/t206"
make_tarballs "$T206" fake
h206="$("$PROJENY" hash "$T206/fake-1.0.tar.gz")"
printf 'URL: file://%s/nope-1.tar.gz %s\nURL: file://%s/nope-2.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    All gone.\n\n' \
    "$T206" "$h206" "$T206" "$h206" > "$T206/fake.projeny"
out="$(cd "$T206" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "all-URLs-unreachable setup exits nonzero"
else
    fail "all-URLs-unreachable setup exits nonzero" "out: $out"
fi
n206="$(printf '%s\n' "$out" | grep -c "could not download")"
if [ "$n206" -eq 2 ]; then
    ok "each unreachable URL warns once"
else
    fail "each unreachable URL warns once" "count=$n206 out: $out"
fi
case "$out" in
*"tried 2 URL line(s)"*)
    ok "the error reports how many URLs were tried"
    ;;
*)
    fail "the error reports how many URLs were tried" "out: $out"
    ;;
esac
if [ -e "$T206/.nope-1.tar.gz.snapshot" ]; then
    fail "a failed setup writes no snapshot" "$(ls -A "$T206")"
else
    ok "a failed setup writes no snapshot"
fi

# ------------- 207. multiple URLs: the first hash mismatch, the second good
# A URL that downloads but hashes wrong warns and falls through: the first
# line here points at the REAL 2.0 tarball but records the 1.0 hash, and the
# second line is the valid 1.0 pair. The checkout must end up v1 (the
# mismatched 2.0 bytes never win) and the snapshot — named after the first
# URL — must hold the verified 1.0 bytes.
T207="$ROOT/t207"
make_tarballs "$T207" fake
h207="$("$PROJENY" hash "$T207/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-2.0.tar.gz %s\nURL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Wrong hash first.\n\n' \
    "$T207" "$h207" "$T207" "$h207" > "$T207/fake.projeny"
out="$(cd "$T207" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "fallback over a hash-mismatched first URL exits 0"
else
    fail "fallback over a hash-mismatched first URL exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"but its blake3 hash is"*)
    ok "the mismatched download warns with the blake3 hash"
    ;;
*)
    fail "the mismatched download warns with the blake3 hash" "out: $out"
    ;;
esac
case "$out" in
*", expected $h207; trying the next URL"*)
    ok "the mismatch warning names the expected hash"
    ;;
*)
    fail "the mismatch warning names the expected hash" "out: $out"
    ;;
esac
if cmp -s "$T207/.fake-2.0.tar.gz.snapshot" "$T207/fake-1.0.tar.gz"; then
    ok "the verified 1.0 bytes land in the snapshot byte-exact"
else
    fail "the verified 1.0 bytes land in the snapshot byte-exact" \
         "cmp: snapshot vs tarball"
fi
expect_file_contains "the fallback checkout has v1 content, not the mismatched v2" \
    "$T207/fake/README" "hello v1"

# ---------------- 208. multiple URLs: every hash mismatching
# Downloads that arrive but verify wrong are just as dead as unreachable
# ones: every line warns, setup hard-errors, and nothing is cached.
T208="$ROOT/t208"
make_tarballs "$T208" fake
bad208="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
printf 'URL: file://%s/fake-2.0.tar.gz %s\nURL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    All wrong.\n\n' \
    "$T208" "$bad208" "$T208" "$bad208" > "$T208/fake.projeny"
out="$(cd "$T208" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "all-URLs-mismatch setup exits nonzero"
else
    fail "all-URLs-mismatch setup exits nonzero" "out: $out"
fi
n208="$(printf '%s\n' "$out" | grep -c "but its blake3 hash is")"
if [ "$n208" -eq 2 ]; then
    ok "each hash-mismatched download warns once"
else
    fail "each hash-mismatched download warns once" "count=$n208 out: $out"
fi
case "$out" in
*"could not obtain archive"*)
    ok "the hard error summarizes the exhausted URLs"
    ;;
*)
    fail "the hard error summarizes the exhausted URLs" "out: $out"
    ;;
esac
if [ -e "$T208/.fake-2.0.tar.gz.snapshot" ]; then
    fail "no snapshot is kept from a rejected download" "$(ls -A "$T208")"
else
    ok "no snapshot is kept from a rejected download"
fi

# ------------------- 209. malformed URL: headers are parse errors
# Each URL: line must be exactly "URL: <url> <64-hex-chars>". These all die
# at parse time, before any download is attempted (so the URLs need not even
# resolve). The hash is case-insensitive, which the next section covers.
T209="$ROOT/t209"
mkdir -p "$T209"
printf 'seed\n' > "$T209/seed"
h209="$("$PROJENY" hash "$T209/seed")"
nonhex209="zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
printf 'URL: file://%s/only-url.tar.gz\nOrigname: x-1.0\nName: x\n' "$T209" \
    > "$T209/one-token.projeny"
printf 'URL: file://%s/short-hash.tar.gz %s\nOrigname: x-1.0\nName: x\n' \
    "$T209" "${h209%?}" > "$T209/short-hash.projeny"
printf 'URL: file://%s/long-hash.tar.gz %s\nOrigname: x-1.0\nName: x\n' \
    "$T209" "${h209}f" > "$T209/long-hash.projeny"
printf 'URL: file://%s/nonhex-hash.tar.gz %s\nOrigname: x-1.0\nName: x\n' \
    "$T209" "$nonhex209" > "$T209/nonhex-hash.projeny"
printf 'URL: file://%s/split-hash.tar.gz %s %s\nOrigname: x-1.0\nName: x\n' \
    "$T209" "$h209" "$h209" > "$T209/three-token.projeny"
printf 'URL:\nOrigname: x-1.0\nName: x\n' > "$T209/empty-value.projeny"
out="$("$PROJENY" setup "$T209/one-token.projeny" 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "a URL header with no hash fails"
else
    fail "a URL header with no hash fails" "out: $out"
fi
case "$out" in
*"malformed URL: header"*)
    ok "the malformed-header error explains the wanted shape"
    ;;
*)
    fail "the malformed-header error explains the wanted shape" "out: $out"
    ;;
esac
expect_fail "a 63-char hash fails" "$PROJENY" setup "$T209/short-hash.projeny"
expect_fail "a 65-char hash fails" "$PROJENY" setup "$T209/long-hash.projeny"
expect_fail "a non-hex hash fails" "$PROJENY" setup "$T209/nonhex-hash.projeny"
expect_fail "a hash with a space in the middle fails" \
    "$PROJENY" setup "$T209/three-token.projeny"
expect_fail "an empty URL: value fails" "$PROJENY" setup "$T209/empty-value.projeny"

# --------- 210. Archive: and URL: are mutually exclusive; URL projects
# still need Origname: and Name:
T210="$ROOT/t210"
mkdir -p "$T210"
printf 'seed\n' > "$T210/seed"
h210="$("$PROJENY" hash "$T210/seed")"
printf 'Archive: seed.tar.gz\nURL: file://%s/seed.tar.gz %s\nOrigname: x-1.0\nName: x\n' \
    "$T210" "$h210" > "$T210/both.projeny"
out="$("$PROJENY" setup "$T210/both.projeny" 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "Archive: and URL: together fail"
else
    fail "Archive: and URL: together fail" "out: $out"
fi
case "$out" in
*"mutually exclusive"*)
    ok "the mutual-exclusion error says so"
    ;;
*)
    fail "the mutual-exclusion error says so" "out: $out"
    ;;
esac
printf 'URL: file://%s/seed.tar.gz %s\nName: x\n' "$T210" "$h210" \
    > "$T210/no-origname.projeny"
expect_fail "a URL project without Origname: fails" \
    "$PROJENY" setup "$T210/no-origname.projeny"
printf 'URL: file://%s/seed.tar.gz %s\nOrigname: x-1.0\n' "$T210" "$h210" \
    > "$T210/no-name.projeny"
expect_fail "a URL project without Name: fails" \
    "$PROJENY" setup "$T210/no-name.projeny"
out="$("$PROJENY" setup "$T210/no-origname.projeny" 2>&1)"
case "$out" in
*"missing a required header"*)
    ok "the missing-header error covers the URL: form"
    ;;
*)
    fail "the missing-header error covers the URL: form" "out: $out"
    ;;
esac

# ------------------------------- 211. uppercase hash accepted
# The 64 hex chars of a URL: header are case-insensitive (normalized to
# lowercase on parse), so a header pasted from an uppercase digest still
# verifies the snapshot.
T211="$ROOT/t211"
make_tarballs "$T211" fake
h211="$("$PROJENY" hash "$T211/fake-1.0.tar.gz")"
up211="$(printf '%s' "$h211" | tr 'a-f' 'A-F')"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Uppercase digest.\n\n' \
    "$T211" "$up211" > "$T211/fake.projeny"
run_in "$T211" expect_ok "setup with an uppercase hash exits 0" \
    "$PROJENY" setup fake.projeny
if cmp -s "$T211/.fake-1.0.tar.gz.snapshot" "$T211/fake-1.0.tar.gz"; then
    ok "the uppercase hash verified the download byte-exact"
else
    fail "the uppercase hash verified the download byte-exact" "cmp: snapshot vs tarball"
fi
mv "$T211/fake-1.0.tar.gz" "$ROOT/t211-kept-fake-1.0.tar.gz"
out="$(cd "$T211" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the uppercase hash also verifies the cached snapshot"
else
    fail "the uppercase hash also verifies the cached snapshot" \
         "exit=$rc out: $out"
fi
case "$out" in
*warning*)
    fail "the uppercase-hash snapshot needs no download" "out: $out"
    ;;
*)
    ok "the uppercase-hash snapshot needs no download"
    ;;
esac

# ---------------- 212. a query-string URL derives the bare archive name
# The archive name (and snapshot name) is the URL's basename with any query
# or fragment stripped: ".../fake-1.0.tar.gz?x=1" caches as
# .fake-1.0.tar.gz.snapshot, and the status keeps the URL verbatim.
T212="$ROOT/t212"
make_tarballs "$T212" fake
h212="$("$PROJENY" hash "$T212/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz?x=1 %s\nOrigname: fake-1.0\nName: fake\n\n    Query string.\n\n' \
    "$T212" "$h212" > "$T212/fake.projeny"
run_in "$T212" expect_ok "query-string URL setup exits 0" \
    "$PROJENY" setup fake.projeny
if [ -f "$T212/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the snapshot name drops the query string"
else
    fail "the snapshot name drops the query string" "ls: $(ls -A "$T212" 2>&1)"
fi
if [ -e "$T212/.fake-1.0.tar.gz?x=1.snapshot" ]; then
    fail "the query string never reaches the snapshot name" \
         "$(ls -A "$T212")"
else
    ok "the query string never reaches the snapshot name"
fi
if cmp -s "$T212/.fake-1.0.tar.gz.snapshot" "$T212/fake-1.0.tar.gz"; then
    ok "the query-string download is byte-exact"
else
    fail "the query-string download is byte-exact" "cmp: snapshot vs tarball"
fi
expect_file_contains "the status keeps the URL verbatim" \
    "$T212/.fake.projeny.status" "URL: file://$T212/fake-1.0.tar.gz?x=1"

# -------------------------- 213. commit on a URL project roundtrips
# Everything above the download layer is project-flavor-agnostic: an edit,
# commit, empty diff, clean status, and a patch folded into the .projeny
# file all work the same, and the following re-setup still needs no network
# (the snapshot still matches the URL: hash).
T213="$ROOT/t213"
make_tarballs "$T213" fake
h213="$("$PROJENY" hash "$T213/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Committing on a URL.\n\n' \
    "$T213" "$h213" > "$T213/fake.projeny"
run_in "$T213" expect_ok "URL commit fixture setup" "$PROJENY" setup fake.projeny
python3 - "$T213/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 42;")
open(p, "w").write(s)
EOF
run_in "$T213" expect_ok "commit on a URL project exits 0" \
    "$PROJENY" commit fake.projeny
expect_file_contains "the commit folds the diff into the URL projeny" \
    "$T213/fake.projeny" "beta = 42"
expect_file_contains "the committed diff is a git patch" \
    "$T213/fake.projeny" "diff --git"
run_in "$T213" expect_no_out "diff is empty after the URL-project commit" \
    "$PROJENY" diff fake.projeny
out="$(cd "$T213" && "$PROJENY" status fake.projeny 2>&1)"
case "$out" in
*"Modified:"*)
    fail "status shows no modification after the commit" "out: $out"
    ;;
*)
    ok "status shows no modification after the commit"
    ;;
esac
mv "$T213/fake-1.0.tar.gz" "$ROOT/t213-kept-fake-1.0.tar.gz"
out="$(cd "$T213" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "re-setup after the commit exits 0 without the tarball"
else
    fail "re-setup after the commit exits 0 without the tarball" \
         "exit=$rc out: $out"
fi
case "$out" in
*warning*)
    fail "re-setup after the commit downloads nothing" "out: $out"
    ;;
*)
    ok "re-setup after the commit downloads nothing"
    ;;
esac
expect_file_contains "the re-setup keeps the committed edit" \
    "$T213/fake/src/a.c" "beta = 42"

# ------------- 214. editing the URL: merges local work onto the new archive
# The URL-edit rebase flow: point the URL: header at a new tarball (with its
# hash and the new Origname) and re-run setup. The committed patch — kept to
# the delta region, which v2 leaves alone — applies to the new base, and the
# UNCOMMITTED beta edit rides through as a genuine 3-way merge.
T214="$ROOT/t214"
make_tarballs "$T214" fake
h214a="$("$PROJENY" hash "$T214/fake-1.0.tar.gz")"
h214b="$("$PROJENY" hash "$T214/fake-2.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Editing the URL.\n\n' \
    "$T214" "$h214a" > "$T214/fake.projeny"
run_in "$T214" expect_ok "url-edit fixture setup" "$PROJENY" setup fake.projeny
python3 - "$T214/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
run_in "$T214" expect_ok "commit the delta edit" "$PROJENY" commit fake.projeny
python3 - "$T214/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int beta = 1;", "int beta = 300;")
open(p, "w").write(s)
EOF
sed -i "s#fake-1.0.tar.gz $h214a#fake-2.0.tar.gz $h214b#; s#Origname: fake-1.0#Origname: fake-2.0#" \
    "$T214/fake.projeny"
expect_file_not_contains "the URL edit points the header at the new archive" \
    "$T214/fake.projeny" "fake-1.0.tar.gz"
out="$(cd "$T214" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup onto the edited URL exits 0"
else
    fail "setup onto the edited URL exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"merged local changes onto"*)
    ok "the URL-edit setup merges the local work"
    ;;
*)
    fail "the URL-edit setup merges the local work" "out: $out"
    ;;
esac
expect_file_contains "the merge took the v2 alpha" "$T214/fake/src/a.c" "alpha = 2"
expect_file_contains "the merge kept the uncommitted local edit" \
    "$T214/fake/src/a.c" "beta = 300"
expect_file_contains "the merge kept the committed edit" \
    "$T214/fake/src/a.c" "delta = 100"
expect_file_contains "the workdir moved to the v2 base" "$T214/fake/README" "hello v2"
if cmp -s "$T214/.fake-2.0.tar.gz.snapshot" "$T214/fake-2.0.tar.gz"; then
    ok "the snapshot now caches the new archive byte-exact"
else
    fail "the snapshot now caches the new archive byte-exact" \
         "cmp: snapshot vs tarball"
fi
expect_file_contains "the status records the new archive" \
    "$T214/.fake.projeny.status" "fake-2.0.tar.gz"

# ----------------------- 215. rebase refuses on a URL project
# A URL-based project has no checked-in tarball to rebase onto; the refusal
# explains the URL-edit flow instead of half-doing something.
T215="$ROOT/t215"
make_tarballs "$T215" fake
h215="$("$PROJENY" hash "$T215/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    No rebasing.\n\n' \
    "$T215" "$h215" > "$T215/fake.projeny"
run_in "$T215" expect_ok "rebase-refusal fixture setup" "$PROJENY" setup fake.projeny
out="$(cd "$T215" && "$PROJENY" rebase fake.projeny fake-2.0.tar.gz 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "rebase refuses on a URL project"
else
    fail "rebase refuses on a URL project" "out: $out"
fi
case "$out" in
*"URL-based project"*)
    ok "the refusal explains that URL projects have no Archive:"
    ;;
*)
    fail "the refusal explains that URL projects have no Archive:" \
         "out: $out"
    ;;
esac
case "$out" in
*"projeny hash <file>"*)
    ok "the refusal points at the URL-edit flow"
    ;;
*)
    fail "the refusal points at the URL-edit flow" "out: $out"
    ;;
esac
expect_file_contains "rebase left the URL: header alone" \
    "$T215/fake.projeny" "fake-1.0.tar.gz"

# ---------------- 216. status is tolerant when the snapshot is missing
# status is informational and must never die on missing state: with the
# snapshot gone and the URL unreachable, it warns about the archive but
# still exits 0 and reports the recorded state.
T216="$ROOT/t216"
make_tarballs "$T216" fake
h216="$("$PROJENY" hash "$T216/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Tolerant status.\n\n' \
    "$T216" "$h216" > "$T216/fake.projeny"
run_in "$T216" expect_ok "tolerant-status fixture setup" "$PROJENY" setup fake.projeny
rm -f "$T216/.fake-1.0.tar.gz.snapshot" "$T216/fake-1.0.tar.gz"
run_in "$T216" expect_ok "status exits 0 with the snapshot gone" \
    "$PROJENY" status fake.projeny
out="$(cd "$T216" && "$PROJENY" status fake.projeny 2>&1)"
case "$out" in
*"could not obtain archive"*)
    ok "status warns it could not get the archive"
    ;;
*)
    fail "status warns it could not get the archive" "out: $out"
    ;;
esac
case "$out" in
*"continuing without the live diff"*)
    ok "the warning says status continued without the live diff"
    ;;
*)
    fail "the warning says status continued without the live diff" \
         "out: $out"
    ;;
esac
case "$out" in
*"Status: setup"*)
    ok "status still reports the recorded state"
    ;;
*)
    fail "status still reports the recorded state" "out: $out"
    ;;
esac

# --------------- 216b. status is tolerant when the snapshot is corrupt
# The spec gives status no exception: an EXISTING .snapshot must be
# hash-verified against the URL: lines too (no network while it matches any
# of them). A snapshot that matches none — here, garbage clobbering the
# cache — is not trusted: status warns with the hash guidance, re-downloads
# (the source tarball is still there), still exits 0, and leaves the
# snapshot byte-exact again, so the live diff runs instead of being skipped.
T216B="$ROOT/t216b"
make_tarballs "$T216B" fake
h216b="$("$PROJENY" hash "$T216B/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Corrupt snapshot status.\n\n' \
    "$T216B" "$h216b" > "$T216B/fake.projeny"
run_in "$T216B" expect_ok "corrupt-snapshot fixture setup" "$PROJENY" setup fake.projeny
printf 'garbage that is no tarball\n' > "$T216B/.fake-1.0.tar.gz.snapshot"
out="$(cd "$T216B" && "$PROJENY" status fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "status exits 0 with a corrupted snapshot"
else
    fail "status exits 0 with a corrupted snapshot" "exit=$rc out: $out"
fi
case "$out" in
*warning*)
    ok "the corrupted-snapshot status warns"
    ;;
*)
    fail "the corrupted-snapshot status warns" "out: $out"
    ;;
esac
case "$out" in
*"does not match any URL: hash"*)
    ok "the warning names the hash mismatch"
    ;;
*)
    fail "the warning names the hash mismatch" "out: $out"
    ;;
esac
case "$out" in
*"could not obtain archive"*)
    fail "status heals the corrupted snapshot instead of skipping the diff" \
         "out: $out"
    ;;
*)
    ok "status heals the corrupted snapshot instead of skipping the diff"
    ;;
esac
if cmp -s "$T216B/.fake-1.0.tar.gz.snapshot" "$T216B/fake-1.0.tar.gz"; then
    ok "status re-downloaded and restored the snapshot byte-exact"
else
    fail "status re-downloaded and restored the snapshot byte-exact" \
         "cmp: snapshot vs tarball"
fi
out="$(cd "$T216B" && "$PROJENY" status fake.projeny 2>&1)"
case "$out" in
*"Modified:"*|*warning*)
    fail "a follow-up status is clean and needs no re-download" "out: $out"
    ;;
*)
    ok "a follow-up status is clean and needs no re-download"
    ;;
esac

# ------------- 217. package/extract/get-attributes on a URL project
# The payload commands materialize the archive through the same verified
# snapshot, so they work unchanged on URL-based projects.
T217="$ROOT/t217"
make_tarballs "$T217" fake
h217="$("$PROJENY" hash "$T217/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Packaging a URL.\n\n' \
    "$T217" "$h217" > "$T217/fake.projeny"
run_in "$T217" expect_ok "package fixture setup" "$PROJENY" setup fake.projeny
run_in "$T217" expect_no_out "get-attributes is silent on a plain URL project" \
    "$PROJENY" get-attributes fake.projeny
run_in "$T217" expect_ok "package a URL project" \
    "$PROJENY" package fake.projeny url-out.tar.gz
(cd "$T217" && tar -tzf url-out.tar.gz | sort > url-members.txt)
if grep -qx "url-out/README" "$T217/url-members.txt"; then
    ok "the package payload carries the README"
else
    fail "the package payload carries the README" "$(cat "$T217/url-members.txt")"
fi
run_in "$T217" expect_ok "extract a URL project" \
    "$PROJENY" extract fake.projeny extracted
expect_file_contains "the extracted README is correct" \
    "$T217/extracted/README" "hello v1"

# -------------------------- 218. freeze-mtime on a URL project
# Freezing pins the archive member's mtime in the .projeny patch; the pin
# survives, lists, and re-stamps without any download.
T218="$ROOT/t218"
make_tarballs "$T218" fake
h218="$("$PROJENY" hash "$T218/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Freezing a URL.\n\n' \
    "$T218" "$h218" > "$T218/fake.projeny"
run_in "$T218" expect_ok "freeze fixture setup" "$PROJENY" setup fake.projeny
run_in "$T218" expect_ok "freeze-mtime on a URL project" \
    "$PROJENY" freeze-mtime fake.projeny fake/README
ts218="$(stat -c %Y "$T218/fake/README")"
out="$("$PROJENY" list-frozen-mtimes "$T218/fake.projeny")"
case "$out" in
*"README $ts218"*)
    ok "list-frozen-mtimes shows the URL-project pin"
    ;;
*)
    fail "list-frozen-mtimes shows the URL-project pin" "out: $out"
    ;;
esac
expect_file_contains "the pin is recorded in the URL projeny" \
    "$T218/fake.projeny" "frozen-mtime $ts218"
# Drag the file's mtime somewhere wrong, drop the tarball, and re-setup: the
# stamp pass must restore the frozen mtime with no download.
touch -d @1700000400 "$T218/fake/README"
mv "$T218/fake-1.0.tar.gz" "$ROOT/t218-kept-fake-1.0.tar.gz"
out="$(cd "$T218" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "re-setup with a frozen file and no tarball exits 0"
else
    fail "re-setup with a frozen file and no tarball exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*warning*)
    fail "re-stamping the frozen file downloads nothing" "out: $out"
    ;;
*)
    ok "re-stamping the frozen file downloads nothing"
    ;;
esac
if [ "$(stat -c %Y "$T218/fake/README")" = "$ts218" ]; then
    ok "the re-setup re-stamped the frozen mtime"
else
    fail "the re-setup re-stamped the frozen mtime" \
         "got $(stat -c %Y "$T218/fake/README"), want $ts218"
fi

# --------- 219. conflicted URL setup with a shared archive basename
# A git merge that conflicts on the URL: line leaves both sides pointing at
# tarballs with the SAME basename (only the URL/hash differ), so both sides
# share one .snapshot. The local side must be reconstructed from that
# snapshot BEFORE the upstream materialization re-downloads over it: with
# the local tarball deleted, the snapshot is the only record of the local
# side, so a local-last ordering would clobber it and die with "could not
# obtain archive ... needed to reconstruct the local side".
T219="$ROOT/t219"
make_tarballs "$T219" fake
# theirs tarball: same fake-1.0.tar.gz basename and top dir as ours, but v2
# bytes, placed in a v2/ subdir so the two files can coexist. Derived from
# the helper's 1.0 tarball by carrying over exactly the member edits that
# make_tarballs' 2.0 flavor carries.
mkdir -p "$T219/v2"
rm -rf "$T219/v2/fake-1.0"
tar -xzf "$T219/fake-1.0.tar.gz" -C "$T219/v2"
python3 - "$T219/v2/fake-1.0/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int alpha = 1;", "int alpha = 2;")
open(p, "w").write(s)
EOF
printf 'line one v2\n' > "$T219/v2/fake-1.0/src/b.c"
printf 'hello v2\n' > "$T219/v2/fake-1.0/README"
(cd "$T219/v2" && tar -czf fake-1.0.tar.gz fake-1.0 && rm -rf fake-1.0)
h219a="$("$PROJENY" hash "$T219/fake-1.0.tar.gz")"
h219b="$("$PROJENY" hash "$T219/v2/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Conflicted URL.\n\n' \
    "$T219" "$h219a" > "$T219/fake.projeny"
run_in "$T219" expect_ok "conflicted-URL fixture setup" "$PROJENY" setup fake.projeny
python3 - "$T219/fake/src/a.c" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read().replace("int delta = 1;", "int delta = 100;")
open(p, "w").write(s)
EOF
run_in "$T219" expect_ok "commit the local edit" "$PROJENY" commit fake.projeny
# ours side: the status-embedded v1 URL copy (the committed .projeny file);
# theirs side: the v2 URL line with the same archive basename. Git-style
# conflict markers, ours (local) first, as a plain git merge would leave.
cp "$T219/fake.projeny" "$ROOT/t219-ours.projeny"
printf 'URL: file://%s/v2/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    Conflicted URL upstream.\n\n' \
    "$T219" "$h219b" > "$ROOT/t219-theirs.projeny"
make_conflicted "$ROOT/t219-ours.projeny" "$ROOT/t219-theirs.projeny" "$T219/fake.projeny"
# Delete the local tarball: the shared snapshot is now the only record of
# the local side.
rm "$T219/fake-1.0.tar.gz"
out="$(cd "$T219" && "$PROJENY" setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "conflicted URL setup with a shared snapshot exits 0"
else
    fail "conflicted URL setup with a shared snapshot exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"could not obtain archive"*)
    fail "the local side never needs the deleted tarball" "out: $out"
    ;;
*)
    ok "the local side never needs the deleted tarball"
    ;;
esac
# Exactly one mismatch warning: ours matched the snapshot (no download), and
# only theirs was re-downloaded into the shared snapshot.
n219="$(printf '%s\n' "$out" | grep -c "does not match any URL: hash")"
if [ "$n219" -eq 1 ]; then
    ok "only the upstream side was re-downloaded"
else
    fail "only the upstream side was re-downloaded" \
         "count=$n219 out: $out"
fi
case "$out" in
*"merged local changes onto"*)
    ok "the conflicted URL merge prints the merged header"
    ;;
*)
    fail "the conflicted URL merge prints the merged header" "out: $out"
    ;;
esac
if cmp -s "$T219/fake.projeny" "$ROOT/t219-theirs.projeny"; then
    ok "conflicted URL setup force-takes upstream"
else
    fail "conflicted URL setup force-takes upstream" "$(cat "$T219/fake.projeny")"
fi
expect_file_contains "the merge took the upstream alpha" \
    "$T219/fake/src/a.c" "int alpha = 2;"
expect_file_contains "the merge kept the committed local edit" \
    "$T219/fake/src/a.c" "int delta = 100;"
expect_file_contains "the workdir moved to the upstream base" \
    "$T219/fake/README" "hello v2"
expect_file_contains "the workdir took the upstream b.c" \
    "$T219/fake/src/b.c" "line one v2"
expect_file_not_contains "the URL merge records no conflicts" \
    "$T219/.fake.projeny.status" "Conflict:"
expect_file_contains "the status embeds the upstream URL" \
    "$T219/.fake.projeny.status" "file://$T219/v2/fake-1.0.tar.gz"
# Documented end state: the shared snapshot holds the upstream bytes, so it
# must verify against theirs' URL hash.
out219="$("$PROJENY" hash "$T219/.fake-1.0.tar.gz.snapshot")"
if [ "$out219" = "$h219b" ]; then
    ok "the shared snapshot now verifies against the upstream hash"
else
    fail "the shared snapshot now verifies against the upstream hash" \
         "snap=$out219 want=$h219b"
fi
if cmp -s "$T219/.fake-1.0.tar.gz.snapshot" "$T219/v2/fake-1.0.tar.gz"; then
    ok "the shared snapshot holds byte-exact upstream bytes"
else
    fail "the shared snapshot holds byte-exact upstream bytes" \
         "cmp: snapshot vs v2 tarball"
fi

# --------- 220. download feedback: announce, short 64 KiB progress, hash-verified, skip
# A fresh URL setup must SAY what it is doing, on stderr, in a form that
# survives being captured into a log:
#   - the attempt is announced: "downloading '<url>'" (the only line that
#     names the URL);
#   - progress arrives as short '\r'-terminated lines ("projeny: download
#     progress: ...") naming byte counts and a whole percent, printed only
#     when >= 64 KiB arrived since the last line (plus the closing line
#     after perform) — short enough to fit an 80-column terminal, with no
#     ANSI escapes and no backspaces, so a terminal redraws in place while
#     a log keeps every line;
#   - a verified download reports "blake3 hash verified" and the snapshot
#     it wrote.
# file:// ticks arrive quantized in 64 KiB read chunks, which makes the
# 64 KiB gate observable: 2 MiB = 32 ticks of exactly 65536 bytes plus the
# closing line, so ~33 '\r' lines. A regression to printing every tick or
# to printing nothing both move that count out of range.
# A re-setup over the matching snapshot then must NOT download (no
# announcement, no progress lines) and must report the skip; and the skip
# note must print once per run even though commit materializes the same
# archive twice (the unpack and the expected-tree build).
T220="$ROOT/t220"
mkdir -p "$T220/big-1.0"
printf 'hello v1\n' > "$T220/big-1.0/README"
# 32 * 65536 = 2 MiB of incompressible payload: many progress lines, but
# the fixture (and its gzip) still builds in well under a second.
dd if=/dev/urandom of="$T220/big-1.0/blob" bs=65536 count=32 2>/dev/null
(cd "$T220" && tar -czf big-1.0.tar.gz big-1.0)
h220="$("$PROJENY" hash "$T220/big-1.0.tar.gz")"
printf 'URL: file://%s/big-1.0.tar.gz %s\nOrigname: big-1.0\nName: big\n\n    Big enough to need progress lines.\n\n' \
    "$T220" "$h220" > "$T220/big.projeny"
out="$(cd "$T220" && "$PROJENY" setup big.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the 2 MiB URL setup exits 0"
else
    fail "the 2 MiB URL setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"projeny: downloading 'file://$T220/big-1.0.tar.gz'"*)
    ok "the download is announced with its URL"
    ;;
*)
    fail "the download is announced with its URL" "out: $out"
    ;;
esac
case "$out" in
*"blake3 hash verified"*)
    ok "the verified download reports the hash check"
    ;;
*)
    fail "the verified download reports the hash check" "out: $out"
    ;;
esac
case "$out" in
*"wrote snapshot '$T220/.big-1.0.tar.gz.snapshot'"*)
    ok "the verified download names the snapshot it wrote"
    ;;
*)
    fail "the verified download names the snapshot it wrote" "out: $out"
    ;;
esac
# Every '\r' in the captured output is one progress line: 32 throttled
# ticks plus the closing line, so 2..40 (0 would mean progress went
# silent, >40 that the 64 KiB gate stopped gating).
ncr="$(printf '%s' "$out" | tr -dc '\r' | wc -c)"
if [ "$ncr" -ge 2 ] && [ "$ncr" -le 40 ]; then
    ok "progress lines arrive in 64 KiB steps ($ncr lines)"
else
    fail "progress lines arrive in 64 KiB steps" \
         "got $ncr carriage-return-terminated lines, want 2..40"
fi
# Each of those lines must be a complete report row: byte counts and a
# whole percent (after swapping \r for \n, every \r line must match).
np="$(printf '%s' "$out" | tr '\r' '\n' | grep -c 'bytes ([0-9]*%)')"
if [ "$np" -eq "$ncr" ]; then
    ok "every progress line names byte counts and a whole percent"
else
    fail "every progress line names byte counts and a whole percent" \
         "$np of $ncr lines matched 'bytes (N%)'"
fi
# The lines must be short: the announcement already names the URL, so a
# progress line is just the "projeny: download progress: " prefix plus the
# counts — the 2 MiB fixture's longest is 56 characters, well within an
# 80-column terminal. URL-bearing lines (the old format) would overflow it,
# so this is a hard bound on regressing to them.
maxlen="$(printf '%s' "$out" | tr '\r' '\n' | grep '^projeny: download progress:' | awk '{ if (length($0) > m) m = length($0) } END { print m + 0 }')"
if [ "$maxlen" -gt 0 ] && [ "$maxlen" -le 80 ]; then
    ok "progress lines fit an 80-column terminal (longest is $maxlen chars)"
else
    fail "progress lines fit an 80-column terminal" \
         "longest progress line is $maxlen chars, want 1..80"
fi
# ... and must never repeat the URL (the announcement already named it).
if printf '%s' "$out" | tr '\r' '\n' | grep '^projeny: download progress:' | grep -q '://'; then
    fail "progress lines never repeat the URL" \
         "a progress line carried the URL the announcement already named"
else
    ok "progress lines never repeat the URL"
fi
case "$out" in
*$'\033'*|*$'\b'*)
    fail "no ANSI escapes or backspaces in the progress report" "out: $out"
    ;;
*)
    ok "no ANSI escapes or backspaces in the progress report"
    ;;
esac
# The report closes at the received count: a fully received transfer ends
# at 100% (curl does not promise a tick landing exactly there, so the
# closer is printed explicitly).
lastp="$(printf '%s' "$out" | tr '\r' '\n' | grep ' bytes (' | tail -1)"
case "$lastp" in
*"(100%)"*)
    ok "the progress report closes at 100%"
    ;;
*)
    fail "the progress report closes at 100%" "last line: $lastp"
    ;;
esac
# Re-setup over the matching snapshot: no download at all, and the skip is
# announced instead.
out2="$(cd "$T220" && "$PROJENY" setup big.projeny 2>&1)"
rc2=$?
if [ $rc2 -eq 0 ]; then
    ok "the re-setup over the matching snapshot exits 0"
else
    fail "the re-setup over the matching snapshot exits 0" \
         "exit=$rc2 out: $out2"
fi
case "$out2" in
*"using existing snapshot '$T220/.big-1.0.tar.gz.snapshot' (blake3 hash matches); skipping the download"*)
    ok "the snapshot hit is announced as a skipped download"
    ;;
*)
    fail "the snapshot hit is announced as a skipped download" "out: $out2"
    ;;
esac
case "$out2" in
*"projeny: downloading '"*)
    fail "the snapshot-hit setup never downloads" "out: $out2"
    ;;
*)
    ok "the snapshot-hit setup never downloads"
    ;;
esac
ncr2="$(printf '%s' "$out2" | tr -dc '\r' | wc -c)"
if [ "$ncr2" -eq 0 ]; then
    ok "the snapshot-hit setup prints no progress lines"
else
    fail "the snapshot-hit setup prints no progress lines" \
         "got $ncr2 carriage returns, want 0"
fi
# commit materializes the same archive twice (the unpack and the
# expected-tree build); the skip note must still print once per run, not
# once per materialization.
commitout="$(cd "$T220" && "$PROJENY" commit big.projeny 2>&1)"
rcc=$?
if [ $rcc -eq 0 ]; then
    ok "commit over the matching snapshot exits 0"
else
    fail "commit over the matching snapshot exits 0" \
         "exit=$rcc out: $commitout"
fi
count="$(printf '%s' "$commitout" | grep -c 'skipping the download')"
if [ "$count" -eq 1 ]; then
    ok "the skip note prints once per run, not once per materialization"
else
    fail "the skip note prints once per run, not once per materialization" \
         "count=$count out: $commitout"
fi

# --------- 221. a large download reports whole-percent progress, not 64 KiB spam
# Both progress gates must pass before a line prints: >= 64 KiB since the
# last line AND — when the total size is known — a whole-percent boundary
# crossed. 16 MiB / 64 KiB = 256 raw file:// ticks, but 1% of 16 MiB
# (167,800 B) is larger than 64 KiB, so the whole-percent gate caps the
# report at one line per percent plus the closing line (~101). That is the
# user-visible contract: a 70 MB tarball reports ~100 updates, not ~1100.
T221="$ROOT/t221"
mkdir -p "$T221/huge-1.0"
printf 'hello v1\n' > "$T221/huge-1.0/README"
dd if=/dev/urandom of="$T221/huge-1.0/blob" bs=1048576 count=16 2>/dev/null
(cd "$T221" && tar -czf huge-1.0.tar.gz huge-1.0)
h221="$("$PROJENY" hash "$T221/huge-1.0.tar.gz")"
printf 'URL: file://%s/huge-1.0.tar.gz %s\nOrigname: huge-1.0\nName: huge\n\n    Huge enough to need a throttled report.\n\n' \
    "$T221" "$h221" > "$T221/huge.projeny"
out="$(cd "$T221" && "$PROJENY" setup huge.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the 16 MiB URL setup exits 0"
else
    fail "the 16 MiB URL setup exits 0" "exit=$rc out: $out"
fi
ncr="$(printf '%s' "$out" | tr -dc '\r' | wc -c)"
if [ "$ncr" -ge 30 ] && [ "$ncr" -le 101 ]; then
    ok "the 16 MiB download is throttled to whole-percent steps ($ncr lines)"
else
    fail "the 16 MiB download is throttled to whole-percent steps" \
         "got $ncr carriage-return-terminated lines, want 30..101 (256 raw ticks capped at ~101)"
fi
lastp="$(printf '%s' "$out" | tr '\r' '\n' | grep ' bytes (' | tail -1)"
case "$lastp" in
*"(100%)"*)
    ok "the huge download's report still closes at 100%"
    ;;
*)
    fail "the huge download's report still closes at 100%" "last line: $lastp"
    ;;
esac

# --------- 220b. unknown Content-Length: progress without a percent
# Every URL exercised above is a file:// one, and file:// URLs always expose
# their size, so until now every progress line had the
# '<now>/<total> bytes (<pct>%)' form. The branch that prints bare
# '<now> bytes' lines (no percent) — used when the total is unknown — was
# therefore the one untested path. A tiny localhost HTTP server that sends
# NO Content-Length (HTTP/1.0, the body simply ends at close) exercises it
# hermetically: curl then reports dltotal 0, the whole-percent gate
# vanishes, and the 64 KiB step is the only throttle. The suite already
# requires python3, so the fixture adds no dependency.
T220B="$ROOT/t220b"
mkdir -p "$T220B"
cp "$T220/big-1.0.tar.gz" "$T220B/big-1.0.tar.gz"
h220b="$("$PROJENY" hash "$T220B/big-1.0.tar.gz")"
cat > "$T220B/serv.py" <<'PYEOF'
import socket, sys
# One-file HTTP/1.0 server with NO Content-Length: after the request it
# streams the bytes and closes, so the transfer total stays unknown and
# curl reports dltotal 0 — exactly the branch this test exists for.
path, portfile = sys.argv[1], sys.argv[2]
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 0))
s.listen(16)
# The port file is written only after listen(), so a client that reads
# it can always connect.
open(portfile, "w").write(str(s.getsockname()[1]))
while True:
    c, _ = s.accept()
    try:
        c.recv(65536)  # the GET; one connection per transfer
        c.sendall(b"HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n")
        with open(path, "rb") as f:
            while True:
                b = f.read(65536)
                if not b:
                    break
                c.sendall(b)
    except OSError:
        pass
    finally:
        c.close()
PYEOF
python3 "$T220B/serv.py" "$T220B/big-1.0.tar.gz" "$T220B/port" &
srvpid=$!
port220b=""
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if [ -s "$T220B/port" ]; then port220b="$(cat "$T220B/port")"; break; fi
    sleep 0.05
done
if [ -z "$port220b" ]; then
    kill "$srvpid" 2>/dev/null
    fail "the no-Content-Length fixture server starts" "port file never appeared"
else
    printf 'URL: http://127.0.0.1:%s/big-1.0.tar.gz %s\nOrigname: big-1.0\nName: big\n\n    Unknown total.\n\n' \
        "$port220b" "$h220b" > "$T220B/big.projeny"
    out="$(cd "$T220B" && "$PROJENY" setup big.projeny 2>&1)"
    rc=$?
    kill "$srvpid" 2>/dev/null
    wait "$srvpid" 2>/dev/null

    if [ $rc -eq 0 ]; then
        ok "the unknown-total URL setup exits 0"
    else
        fail "the unknown-total URL setup exits 0" "exit=$rc out: $out"
    fi
    case "$out" in
    *"projeny: downloading 'http://127.0.0.1:$port220b/big-1.0.tar.gz'"*)
        ok "the unknown-total download is announced"
        ;;
    *)
        fail "the unknown-total download is announced" "out: $out"
        ;;
    esac
    ncr="$(printf '%s' "$out" | tr -dc '\r' | wc -c)"
    if [ "$ncr" -ge 2 ] && [ "$ncr" -le 40 ]; then
        ok "progress still arrives without a known total ($ncr lines)"
    else
        fail "progress still arrives without a known total" \
             "got $ncr carriage-return-terminated lines, want 2..40"
    fi
    npct="$(printf '%s' "$out" | tr '\r' '\n' | grep -c 'bytes ([0-9]*%)')"
    if [ "$npct" -eq 0 ]; then
        ok "no percent is printed without a known total"
    else
        fail "no percent is printed without a known total" \
             "$npct percent-form lines"
    fi
    nn="$(printf '%s' "$out" | tr '\r' '\n' | grep -Ec "^projeny: download progress: [0-9]+ bytes$")"
    if [ "$nn" -eq "$ncr" ]; then
        ok "every progress line reports bare received bytes"
    else
        fail "every progress line reports bare received bytes" \
             "$nn of $ncr lines matched 'projeny: download progress: N bytes'"
    fi
    case "$out" in
    *"blake3 hash verified"*)
        ok "the unknown-length body still verifies"
        ;;
    *)
        fail "the unknown-length body still verifies" "out: $out"
        ;;
    esac
    if cmp -s "$T220B/.big-1.0.tar.gz.snapshot" "$T220B/big-1.0.tar.gz"; then
        ok "the unknown-length download is byte-exact"
    else
        fail "the unknown-length download is byte-exact" \
             "the snapshot differs from the served file"
    fi
fi

# --------- 220c. a redirect must not leak its Content-Length into the report
# One DownloadProgress spans the whole redirect chain (FOLLOWLOCATION).
# The hop below carries "Content-Length: 70000"; the transfer that
# actually fills the snapshot sends no total at all, and its final
# sub-64 KiB tail is stalled so the closing line always fires. When the
# closing line latched the hop's total, it printed e.g.
# "2097809/70000 bytes (2996%)" — a >100% line contradicting the bare
# in-flight lines of the same transfer. last_total must instead mirror
# the most recent callback, so the closer inherits the final transfer's
# view: bare bytes when that transfer had no Content-Length.
T220C="$ROOT/t220c"
mkdir -p "$T220C"
cp "$T220/big-1.0.tar.gz" "$T220C/big-1.0.tar.gz"
h220c="$("$PROJENY" hash "$T220C/big-1.0.tar.gz")"
cat > "$T220C/serv.py" <<'PYEOF'
import os, socket, sys, time
# Two-path HTTP/1.0 server for the redirect test: /hop.tar.gz answers
# 302 with a bogus Content-Length (curl follows the Location without
# draining that body, but its progress callback still sees the hop's
# total), and /final.tar.gz streams the file with NO Content-Length so
# the transfer total stays unknown.
path, portfile = sys.argv[1], sys.argv[2]
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 0))
s.listen(16)
# The port file is written only after listen(), so a client that reads
# it can always connect.
port = s.getsockname()[1]
open(portfile, "w").write(str(port))
while True:
    c, _ = s.accept()
    try:
        req = c.recv(65536).decode("latin1")
        if " /hop.tar.gz " in req:
            c.sendall(b"HTTP/1.0 302 Found\r\n"
                      b"Location: http://127.0.0.1:" + str(port).encode() +
                      b"/final.tar.gz\r\n"
                      b"Content-Length: 70000\r\n\r\n")
        else:
            c.sendall(b"HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n")
            # Withhold a sub-64 KiB tail behind a short stall: the last
            # in-flight progress print then lands short of the received
            # count, so the closing progress line deterministically
            # fires — which is exactly where the latch bug showed up.
            with open(path, "rb") as f:
                head = os.path.getsize(path) - 40000
                sent = 0
                while sent < head:
                    b = f.read(65536)
                    if not b:
                        break
                    c.sendall(b)
                    sent += len(b)
                time.sleep(0.5)
                c.sendall(f.read())
    except OSError:
        pass
    finally:
        c.close()
PYEOF
python3 "$T220C/serv.py" "$T220C/big-1.0.tar.gz" "$T220C/port" &
srvpid=$!
port220c=""
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if [ -s "$T220C/port" ]; then port220c="$(cat "$T220C/port")"; break; fi
    sleep 0.05
done
if [ -z "$port220c" ]; then
    kill "$srvpid" 2>/dev/null
    fail "the redirect fixture server starts" "port file never appeared"
else
    printf 'URL: http://127.0.0.1:%s/hop.tar.gz %s\nOrigname: big-1.0\nName: big\n\n    Redirected.\n\n' \
        "$port220c" "$h220c" > "$T220C/big.projeny"
    out="$(cd "$T220C" && "$PROJENY" setup big.projeny 2>&1)"
    rc=$?
    kill "$srvpid" 2>/dev/null
    wait "$srvpid" 2>/dev/null

    if [ $rc -eq 0 ]; then
        ok "the redirected URL setup exits 0"
    else
        fail "the redirected URL setup exits 0" "exit=$rc out: $out"
    fi
    case "$out" in
    *"projeny: downloading 'http://127.0.0.1:$port220c/hop.tar.gz'"*)
        ok "the redirect chain is announced with the requested URL"
        ;;
    *)
        fail "the redirect chain is announced with the requested URL" "out: $out"
        ;;
    esac
    ncr="$(printf '%s' "$out" | tr -dc '\r' | wc -c)"
    if [ "$ncr" -ge 2 ] && [ "$ncr" -le 40 ]; then
        ok "the redirected transfer reports progress ($ncr lines)"
    else
        fail "the redirected transfer reports progress" \
             "got $ncr carriage-return-terminated lines, want 2..40"
    fi
    # The stalled sub-64 KiB tail guarantees the closing line fires
    # here; with the latch bug that line was the >100% one, and every
    # guard below must stay at zero.
    nbig="$(printf '%s' "$out" | tr '\r' '\n' | grep -Ec 'bytes \([0-9]{3,}%\)')"
    if [ "$nbig" -eq 0 ]; then
        ok "no progress line ever exceeds 100%"
    else
        fail "no progress line ever exceeds 100%" "$nbig over-100% lines"
    fi
    npct="$(printf '%s' "$out" | tr '\r' '\n' | grep -c 'bytes ([0-9]*%)')"
    if [ "$npct" -eq 0 ]; then
        ok "the unknown-length final body never reports a percent"
    else
        fail "the unknown-length final body never reports a percent" \
             "$npct percent-form lines"
    fi
    nn="$(printf '%s' "$out" | tr '\r' '\n' | grep -Ec "^projeny: download progress: [0-9]+ bytes$")"
    if [ "$nn" -eq "$ncr" ]; then
        ok "every progress line reports bare received bytes"
    else
        fail "every progress line reports bare received bytes" \
             "$nn of $ncr lines matched 'projeny: download progress: N bytes'"
    fi
    size220c="$(stat -c %s "$T220C/big-1.0.tar.gz")"
    lastp="$(printf '%s' "$out" | tr '\r' '\n' | grep ' bytes' | tail -1)"
    case "$lastp" in
    *"projeny: download progress: $size220c bytes")
        ok "the closing line is the bare form at the received count"
        ;;
    *)
        fail "the closing line is the bare form at the received count" \
             "last line: $lastp"
        ;;
    esac
    case "$out" in
    *"blake3 hash verified"*)
        ok "the redirected body still verifies"
        ;;
    *)
        fail "the redirected body still verifies" "out: $out"
        ;;
    esac
    if cmp -s "$T220C/.hop.tar.gz.snapshot" "$T220C/big-1.0.tar.gz"; then
        ok "the redirected download is byte-exact"
    else
        fail "the redirected download is byte-exact" \
             "the snapshot differs from the served file"
    fi
fi

# ---------------------------------- 222. parallel setup (three projects)
# parallel-projeny.txt: "make sure that the projeny test suite has some kind
# of parallel setup/extract/package test that runs a few times in a row, and
# must succeed every time." Three URL-based projects in one directory share
# one archive basename: the planning phase collapses the downloads to
# exactly one, then the three setups run in parallel (-j threads).
T222="$ROOT/t222"
mkdir -p "$T222/fake-1.0/src"
printf 'int alpha = 1;\n' > "$T222/fake-1.0/src/a.c"
printf 'hello v1\n' > "$T222/fake-1.0/README"
(cd "$T222" && tar -czf fake-1.0.tar.gz fake-1.0 && rm -rf fake-1.0)
h222="$("$PROJENY" hash "$T222/fake-1.0.tar.gz")"
for name in a b c; do
    printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake%s\n\n    Parallel project %s.\n\n' \
        "$T222" "$h222" "$name" "$name" > "$T222/$name.projeny"
done
parallel_setup_round() {
    _round="$1"; _dir="$2"; _want_dl="$3"; shift 3
    out="$(run_in "$_dir" "$PROJENY" "$@" 2>&1)"
    rc=$?
    if [ $rc -eq 0 ]; then
        ok "parallel setup round $_round exits 0"
    else
        fail "parallel setup round $_round exits 0" "exit=$rc out: $out"
    fi
    ndl="$(printf '%s' "$out" | grep -c "^projeny: downloading 'fake-1.0.tar.gz' from '")"
    if [ "$ndl" -eq "$_want_dl" ]; then
        ok "parallel setup round $_round downloads the shared archive $_want_dl time(s)"
    else
        fail "parallel setup round $_round downloads the shared archive $_want_dl time(s)" \
             "got $ndl downloading lines, want exactly $_want_dl (out: $out)"
    fi
    for name in a b c; do
        if [ "$(cat "$_dir/fake$name/README" 2>/dev/null)" = "hello v1" ]; then
            ok "parallel setup round $_round checks out fake$name"
        else
            fail "parallel setup round $_round checks out fake$name" \
                 "README missing or wrong: $(cat "$_dir/fake$name/README" 2>&1)"
        fi
    done
    if cmp -s "$_dir/.fake-1.0.tar.gz.snapshot" "$_dir/fake-1.0.tar.gz"; then
        ok "parallel setup round $_round writes a byte-exact snapshot"
    else
        fail "parallel setup round $_round writes a byte-exact snapshot" \
             "the snapshot differs from the tarball"
    fi
}
T222R1="$ROOT/t222r1"
mkdir -p "$T222R1"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/c.projeny" \
   "$T222/fake-1.0.tar.gz" "$T222R1/"
parallel_setup_round 1 "$T222R1" 1 setup a.projeny b.projeny c.projeny
T222R2="$ROOT/t222r2"
mkdir -p "$T222R2"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/c.projeny" \
   "$T222/fake-1.0.tar.gz" "$T222R2/"
parallel_setup_round 2 "$T222R2" 1 setup -j2 a.projeny b.projeny c.projeny
# A repeat parallel setup over existing checkouts must also succeed (the
# no-op re-setup path runs in parallel too, and the verified snapshot means
# no download happens at all).
parallel_setup_round 3 "$T222R2" 0 setup a.projeny b.projeny c.projeny

# -------------------------------------- 223. duplicate projects collapse
# `projeny setup foo.projeny foo.projeny` warns and sets the project up
# exactly once — never two (not even sequential) setups of one .projeny.
T223="$ROOT/t223"
mkdir -p "$T223"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T223/"
out="$(run_in "$T223" "$PROJENY" setup a.projeny a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the duplicate-argument setup exits 0"
else
    fail "the duplicate-argument setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"'a.projeny' is listed more than once; setting it up only once"*)
    ok "the duplicate argument warns and collapses"
    ;;
*)
    fail "the duplicate argument warns and collapses" "out: $out"
    ;;
esac
ndl="$(printf '%s' "$out" | grep -c "^projeny: downloading 'fake-1.0.tar.gz' from '")"
if [ "$ndl" -eq 1 ]; then
    ok "the collapsed setup still downloads exactly once"
else
    fail "the collapsed setup still downloads exactly once" \
         "got $ndl downloading lines (out: $out)"
fi
if [ -d "$T223/fakea" ] && [ ! -e "$T223/fakea.projeny.x" ]; then
    ok "the collapsed setup produces one checkout"
else
    fail "the collapsed setup produces one checkout" "ls: $(ls "$T223" 2>&1)"
fi

# --------------------------------------- 224. -j/--jobs and -c/--curl-jobs
# The parallel commands accept -j N/-jN/--jobs N/--jobs=N and -c N/-cN/
# --curl-jobs N/--curl-jobs=N anywhere among their arguments; values must
# be integers >= 1; unknown option-looking tokens die.
T224="$ROOT/t224"
optidx224=0
for optform in "-j2" "--jobs 2" "--jobs=2" "-c2" "--curl-jobs 2" "--curl-jobs=2" \
               "-j2 -c2"; do
    optidx224=$((optidx224 + 1))
    T224D="$ROOT/t224-$optidx224"
    mkdir -p "$T224D"
    cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T224D/"
    run_in "$T224D" expect_ok "setup accepts '$optform'" "$PROJENY" \
        setup $optform a.projeny
    if [ -f "$T224D/fakea/README" ]; then
        ok "setup with '$optform' produced the checkout"
    else
        fail "setup with '$optform' produced the checkout" "ls: $(ls "$T224D")"
    fi
done
T224B="$ROOT/t224bad"
mkdir -p "$T224B"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T224B/"
run_in "$T224B" expect_fail "-j0 is refused" "$PROJENY" setup -j0 a.projeny
run_in "$T224B" expect_fail "-j abc is refused" "$PROJENY" setup -j abc \
    a.projeny
run_in "$T224B" expect_fail "-c0 is refused" "$PROJENY" setup -c0 a.projeny
run_in "$T224B" expect_fail "an unknown option dies" "$PROJENY" setup \
    --bogus a.projeny
out="$(run_in "$T224B" "$PROJENY" setup -j0 a.projeny 2>&1)"
case "$out" in
*"-j must be >= 1"*)
    ok "the -j0 error says -j must be >= 1"
    ;;
*)
    fail "the -j0 error says -j must be >= 1" "out: $out"
    ;;
esac
out="$(run_in "$T224B" "$PROJENY" setup -j abc a.projeny 2>&1)"
case "$out" in
*"invalid value for -j: 'abc'"*)
    ok "the -j abc error names the bad value"
    ;;
*)
    fail "the -j abc error names the bad value" "out: $out"
    ;;
esac
out="$(run_in "$T224B" "$PROJENY" setup --bogus a.projeny 2>&1)"
case "$out" in
*"unknown option '--bogus'"*)
    ok "the unknown-option error names the option"
    ;;
*)
    fail "the unknown-option error names the option" "out: $out"
    ;;
esac
# The paired parallel forms: package/extract take (project, output/dest)
# pairs, so an odd or too-short argument count is a usage error.
run_in "$T224B" expect_fail "package with one argument is a usage error" \
    "$PROJENY" package a.projeny
run_in "$T224B" expect_fail "package with three arguments is a usage error" \
    "$PROJENY" package a.projeny o.tar.gz a.projeny
run_in "$T224B" expect_fail "extract with one argument is a usage error" \
    "$PROJENY" extract a.projeny
run_in "$T224B" expect_fail "setup with no project is a usage error" \
    "$PROJENY" setup

# ------------------------------------------- 225. loud shared-package warnings
# Two files naming the same archive basename with different URL sets get the
# ALL-CAPS URL-set warning; the same URL with different hashes gets the
# louder hash warning. Both proceed: one download feeds every file.
T225="$ROOT/t225"
mkdir -p "$T225/d1/fake-1.0" "$T225/d2/fake-1.0"
printf 'version one\n' > "$T225/d1/fake-1.0/README"
printf 'version two\n' > "$T225/d2/fake-1.0/README"
(cd "$T225/d1" && tar -czf fake-1.0.tar.gz fake-1.0)
(cd "$T225/d2" && tar -czf fake-1.0.tar.gz fake-1.0)
h225a="$("$PROJENY" hash "$T225/d1/fake-1.0.tar.gz")"
h225b="$("$PROJENY" hash "$T225/d2/fake-1.0.tar.gz")"
mkdir -p "$T225/sets"
printf 'URL: file://%s/d1/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakex\n\n    X.\n\n' \
    "$T225" "$h225a" > "$T225/sets/x.projeny"
printf 'URL: file://%s/d2/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakey\n\n    Y.\n\n' \
    "$T225" "$h225b" > "$T225/sets/y.projeny"
out="$(run_in "$T225/sets" "$PROJENY" setup x.projeny y.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the different-URL-set parallel setup exits 0"
else
    fail "the different-URL-set parallel setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"WARNING: 2 PROJENY FILES DOWNLOAD ARCHIVES WITH THE SAME NAME 'fake-1.0.tar.gz' BUT WITH DIFFERENT URL SETS: "*)
    ok "the different-URL-set warning prints"
    ;;
*)
    fail "the different-URL-set warning prints" "out: $out"
    ;;
esac
ndl="$(printf '%s' "$out" | grep -c "^projeny: downloading 'fake-1.0.tar.gz' from '")"
if [ "$ndl" -eq 1 ]; then
    ok "the differently-URLed files still share one download"
else
    fail "the differently-URLed files still share one download" \
         "got $ndl downloading lines (out: $out)"
fi
if [ -f "$T225/sets/fakex/README" ] && [ -f "$T225/sets/fakey/README" ]; then
    ok "both differently-URLed projects still set up"
else
    fail "both differently-URLed projects still set up" "out: $out"
fi
T225H="$ROOT/t225h"
mkdir -p "$T225H"
printf 'URL: file://%s/d1/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakep\n\n    P.\n\n' \
    "$T225" "$h225a" > "$T225H/p.projeny"
printf 'URL: file://%s/d1/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakeq\n\n    Q.\n\n' \
    "$T225" "$h225b" > "$T225H/q.projeny"
out="$(run_in "$T225H" "$PROJENY" setup p.projeny q.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the same-URL-different-hash parallel setup exits 0"
else
    fail "the same-URL-different-hash parallel setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"WARNING: URL 'file://$T225/d1/fake-1.0.tar.gz' IS LISTED WITH DIFFERENT BLAKE3 HASHES ($h225a, $h225b) IN "*)
    ok "the different-hash warning prints"
    ;;
*)
    fail "the different-hash warning prints" "out: $out"
    ;;
esac
case "$out" in
*"THIS IS ALMOST CERTAINLY A MISTAKE. ALL OF THESE FILES WILL RECEIVE THE SAME DOWNLOADED ARCHIVE."*)
    ok "the different-hash warning says what happens"
    ;;
*)
    fail "the different-hash warning says what happens" "out: $out"
    ;;
esac
if [ -f "$T225H/fakep/README" ] && [ -f "$T225H/fakeq/README" ]; then
    ok "both same-URL projects still set up"
else
    fail "both same-URL projects still set up" "out: $out"
fi

# --------------------------------- 226. parallel setup mixes Archive and URL
# A classic Archive:-based project (checked-in tarball) contributes nothing
# to the download batch; a URL project in the same run still batch-downloads
# once.
T226="$ROOT/t226"
mkdir -p "$T226"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T226/"
printf 'Archive: classic-1.0.tar.gz\nOrigname: fake-1.0\nName: classic\n\n    Classic.\n\n' \
    > "$T226/classic.projeny"
cp "$T222/fake-1.0.tar.gz" "$T226/classic-1.0.tar.gz"
out="$(run_in "$T226" "$PROJENY" setup a.projeny classic.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the mixed Archive/URL parallel setup exits 0"
else
    fail "the mixed Archive/URL parallel setup exits 0" "exit=$rc out: $out"
fi
ndl="$(printf '%s' "$out" | grep -c "^projeny: downloading '")"
if [ "$ndl" -eq 1 ]; then
    ok "only the URL project's archive downloads"
else
    fail "only the URL project's archive downloads" \
         "got $ndl downloading lines (out: $out)"
fi
if [ -f "$T226/fakea/README" ] && [ -f "$T226/classic/README" ]; then
    ok "both the Archive and URL projects check out"
else
    fail "both the Archive and URL projects check out" "ls: $(ls "$T226")"
fi

# ------------------------------- 227. parallel package and parallel extract
# (project, output) pairs package in parallel, and (project, dest) pairs
# extract in parallel; the payloads carry exactly the tracked files.
T227="$ROOT/t227"
mkdir -p "$T227"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/fake-1.0.tar.gz" "$T227/"
run_in "$T227" expect_ok "parallel package exits 0" "$PROJENY" \
    package a.projeny o1.tar.gz b.projeny o2.tar.gz
if [ -f "$T227/o1.tar.gz" ] && [ -f "$T227/o2.tar.gz" ]; then
    ok "parallel package writes both outputs"
else
    fail "parallel package writes both outputs" "ls: $(ls "$T227")"
fi
if tar -tzf "$T227/o1.tar.gz" | grep -q "^o1/README$" &&
    tar -tzf "$T227/o2.tar.gz" | grep -q "^o2/src/a.c$"; then
    ok "the parallel packages hold the tracked files"
else
    fail "the parallel packages hold the tracked files" \
         "o1: $(tar -tzf "$T227/o1.tar.gz" 2>&1); o2: $(tar -tzf "$T227/o2.tar.gz" 2>&1)"
fi
T227E="$ROOT/t227e"
mkdir -p "$T227E"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/fake-1.0.tar.gz" "$T227E/"
run_in "$T227E" expect_ok "parallel extract exits 0" "$PROJENY" \
    extract a.projeny e1 b.projeny e2
if [ "$(cat "$T227E/e1/README" 2>/dev/null)" = "hello v1" ] &&
    [ "$(cat "$T227E/e2/src/a.c" 2>/dev/null)" = "int alpha = 1;" ]; then
    ok "parallel extract fills both destinations"
else
    fail "parallel extract fills both destinations" \
         "e1: $(ls "$T227E/e1" 2>&1); e2: $(ls "$T227E/e2" 2>&1)"
fi
run_in "$T227E" expect_fail "re-extracting over a filled dir fails" \
    "$PROJENY" extract a.projeny e1 b.projeny e2

# ------------------------------- 228. one failing project does not stop the
# others: its package fails the batch (after the retry pass), its setup dies
# with a labeled error naming it, the good projects still check out, and the
# command exits nonzero with a one-line summary.
T228="$ROOT/t228"
mkdir -p "$T228/sub"
cp "$T222/a.projeny" "$T222/b.projeny" "$T228/"
cp "$T222/fake-1.0.tar.gz" "$T228/fake-bad-1.0.tar.gz"
printf 'URL: file://%s/fake-bad-1.0.tar.gz %s\nOrigname: fake-1.0\nName: badone\n\n    Bad.\n\n' \
    "$T228" "0000000000000000000000000000000000000000000000000000000000000000" \
    > "$T228/sub/bad.projeny"
out="$(run_in "$T228" "$PROJENY" setup a.projeny sub/bad.projeny b.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "the mixed good/bad parallel setup exits nonzero"
else
    fail "the mixed good/bad parallel setup exits nonzero" "out: $out"
fi
case "$out" in
*"[bad.projeny] error: 'badone' needs archive 'fake-bad-1.0.tar.gz', but the parallel download phase failed to obtain it"*)
    ok "the failed project dies with a labeled error"
    ;;
*)
    fail "the failed project dies with a labeled error" "out: $out"
    ;;
esac
case "$out" in
*"projeny: 1 of 3 setup(s) failed: bad.projeny"*)
    ok "the summary line names the failed project"
    ;;
*)
    fail "the summary line names the failed project" "out: $out"
    ;;
esac
if [ -f "$T228/fakea/README" ] && [ -f "$T228/fakeb/README" ]; then
    ok "the good projects still set up"
else
    fail "the good projects still set up" "ls: $(ls "$T228")"
fi
if [ ! -e "$T228/badone" ]; then
    ok "the failed project leaves no checkout"
else
    fail "the failed project leaves no checkout" "ls: $(ls "$T228")"
fi

# ------------------------------------------------- 229. the download command
# URL HASH pairs download into the cwd, named after the URL's basename; a
# matching local file is kept ("already have"); a mismatching or failing
# download exits nonzero after every other package finished.
T229="$ROOT/t229"
T229SRC="$ROOT/t229src"
mkdir -p "$T229" "$T229SRC"
cp "$T222/fake-1.0.tar.gz" "$T229SRC/fake-1.0.tar.gz"
run_in "$T229" expect_ok "download fetches and verifies" "$PROJENY" \
    download "file://$T229SRC/fake-1.0.tar.gz" "$h222"
if cmp -s "$T229/fake-1.0.tar.gz" "$T229SRC/fake-1.0.tar.gz"; then
    ok "the downloaded file is byte-exact"
else
    fail "the downloaded file is byte-exact" "the file differs from the source"
fi
out="$(run_in "$T229" "$PROJENY" download "file://$T229SRC/fake-1.0.tar.gz" "$h222" 2>&1)"
case "$out" in
*"already have fake-1.0.tar.gz (blake3 hash verified)"*)
    ok "a matching local file is kept"
    ;;
*)
    fail "a matching local file is kept" "out: $out"
    ;;
esac
out="$(run_in "$T229" "$PROJENY" download "file://$T229SRC/fake-1.0.tar.gz" \
    0000000000000000000000000000000000000000000000000000000000000000 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "a bad hash exits nonzero"
else
    fail "a bad hash exits nonzero" "out: $out"
fi
case "$out" in
*"failed to download 1 of 1 package(s)"*)
    ok "the bad-hash error summarizes the failure"
    ;;
*)
    fail "the bad-hash error summarizes the failure" "out: $out"
    ;;
esac
run_in "$T229" expect_fail "an odd argument count is a usage error" "$PROJENY" \
    download "file://$T229SRC/fake-1.0.tar.gz"
run_in "$T229" expect_fail "a non-hex hash is refused" "$PROJENY" download \
    "file://$T229SRC/fake-1.0.tar.gz" zz
run_in "$T229" expect_fail "a URL naming no file is refused" "$PROJENY" \
    download "https://foo.dev/" "$h222"
rm -f "$T229/fake-1.0.tar.gz"
run_in "$T229" expect_ok "download accepts -j1 -c1" "$PROJENY" download -j1 \
    -c1 "file://$T229SRC/fake-1.0.tar.gz" "$h222"
if cmp -s "$T229/fake-1.0.tar.gz" "$T229SRC/fake-1.0.tar.gz"; then
    ok "the -j1 -c1 download is byte-exact"
else
    fail "the -j1 -c1 download is byte-exact" "the file differs from the source"
fi

# --------------------------------- 230. one download, two directories
# Two projects in DIFFERENT directories sharing an archive basename download
# once; each directory gets its own verified snapshot copy.
T230="$ROOT/t230"
mkdir -p "$T230/pa" "$T230/pb"
cp "$T222/fake-1.0.tar.gz" "$T230/fake-1.0.tar.gz"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakepa\n\n    PA.\n\n' \
    "$T230" "$h222" > "$T230/pa/pa.projeny"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakepb\n\n    PB.\n\n' \
    "$T230" "$h222" > "$T230/pb/pb.projeny"
out="$(run_in "$T230" "$PROJENY" setup pa/pa.projeny pb/pb.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the two-directory parallel setup exits 0"
else
    fail "the two-directory parallel setup exits 0" "exit=$rc out: $out"
fi
ndl="$(printf '%s' "$out" | grep -c "^projeny: downloading 'fake-1.0.tar.gz' from '")"
if [ "$ndl" -eq 1 ]; then
    ok "the two directories share one download"
else
    fail "the two directories share one download" \
         "got $ndl downloading lines (out: $out)"
fi
if [ -f "$T230/pa/fakepa/README" ] && [ -f "$T230/pb/fakepb/README" ]; then
    ok "both directories check out"
else
    fail "both directories check out" "ls: $(ls "$T230/pa" "$T230/pb" 2>&1)"
fi
if cmp -s "$T230/pa/.fake-1.0.tar.gz.snapshot" "$T230/fake-1.0.tar.gz" &&
    cmp -s "$T230/pb/.fake-1.0.tar.gz.snapshot" "$T230/fake-1.0.tar.gz"; then
    ok "each directory gets its own byte-exact snapshot"
else
    fail "each directory gets its own byte-exact snapshot" \
         "the snapshots differ from the tarball"
fi

# --------------------------------- 231. the flakiness loop: parallel
# setup + extract + package succeed on every repeat
# parallel-projeny.txt: "make sure that the projeny test suite has some kind
# of parallel setup/extract/package test that runs a few times in a row, and
# must succeed every time." Each round below re-creates its directory from
# scratch (fresh .projeny copies and tarballs — never a prior round's
# workdirs or snapshots) and runs a representative scenario: parallel setup
# of three URL projects sharing one archive plus a classic Archive: project,
# then parallel extract, then parallel package. The rounds vary -j from the
# default (no flag at all) through -j1, -j2, and a make-style -j100, so a
# scheduling or shared-state race cannot hide behind one thread count.
T231="$ROOT/t231"
mkdir -p "$T231"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/c.projeny" \
   "$T222/fake-1.0.tar.gz" "$T231/"
printf 'Archive: classic-1.0.tar.gz\nOrigname: fake-1.0\nName: classic\n\n    Classic.\n\n' \
    > "$T231/classic.projeny"
cp "$T222/fake-1.0.tar.gz" "$T231/classic-1.0.tar.gz"
flakiness_round() {
    _r231="$1"; _d231="$2"; _j231="$3"; _jdisp231="$4"
    mkdir -p "$_d231"
    cp "$T231/a.projeny" "$T231/b.projeny" "$T231/c.projeny" \
       "$T231/classic.projeny" "$T231/fake-1.0.tar.gz" \
       "$T231/classic-1.0.tar.gz" "$_d231/"
    out="$(run_in "$_d231" "$PROJENY" setup $_j231 a.projeny b.projeny \
        c.projeny classic.projeny 2>&1)"
    rc=$?
    if [ $rc -eq 0 ]; then
        ok "flakiness round $_r231 ($_jdisp231): parallel setup exits 0"
    else
        fail "flakiness round $_r231 ($_jdisp231): parallel setup exits 0" \
             "exit=$rc out: $out"
    fi
    ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'fake-1.0.tar.gz' from '")"
    if [ "$ndl" -eq 1 ]; then
        ok "flakiness round $_r231 ($_jdisp231): the shared archive downloads once"
    else
        fail "flakiness round $_r231 ($_jdisp231): the shared archive downloads once" \
             "got $ndl downloading lines (out: $out)"
    fi
    for _wd231 in fakea fakeb fakec classic; do
        if [ "$(cat "$_d231/$_wd231/README" 2>/dev/null)" = "hello v1" ]; then
            ok "flakiness round $_r231 ($_jdisp231): $_wd231 checked out"
        else
            fail "flakiness round $_r231 ($_jdisp231): $_wd231 checked out" \
                 "README missing or wrong: $(cat "$_d231/$_wd231/README" 2>&1)"
        fi
    done
    out="$(run_in "$_d231" "$PROJENY" extract $_j231 a.projeny ex1 \
        c.projeny ex3 2>&1)"
    rc=$?
    if [ $rc -eq 0 ]; then
        ok "flakiness round $_r231 ($_jdisp231): parallel extract exits 0"
    else
        fail "flakiness round $_r231 ($_jdisp231): parallel extract exits 0" \
             "exit=$rc out: $out"
    fi
    ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading '")"
    if [ "$ndl" -eq 0 ]; then
        ok "flakiness round $_r231 ($_jdisp231): extract re-downloads nothing"
    else
        fail "flakiness round $_r231 ($_jdisp231): extract re-downloads nothing" \
             "got $ndl downloading lines (out: $out)"
    fi
    for _dest231 in ex1 ex3; do
        if [ "$(cat "$_d231/$_dest231/README" 2>/dev/null)" = "hello v1" ]; then
            ok "flakiness round $_r231 ($_jdisp231): extract filled $_dest231"
        else
            fail "flakiness round $_r231 ($_jdisp231): extract filled $_dest231" \
                 "README missing or wrong: $(cat "$_d231/$_dest231/README" 2>&1)"
        fi
    done
    out="$(run_in "$_d231" "$PROJENY" package $_j231 b.projeny pk2.tar.gz \
        classic.projeny pkc.tar.gz 2>&1)"
    rc=$?
    if [ $rc -eq 0 ]; then
        ok "flakiness round $_r231 ($_jdisp231): parallel package exits 0"
    else
        fail "flakiness round $_r231 ($_jdisp231): parallel package exits 0" \
             "exit=$rc out: $out"
    fi
    for _pkg231 in pk2.tar.gz pkc.tar.gz; do
        if [ -f "$_d231/$_pkg231" ]; then
            ok "flakiness round $_r231 ($_jdisp231): package wrote $_pkg231"
        else
            fail "flakiness round $_r231 ($_jdisp231): package wrote $_pkg231" \
                 "ls: $(ls "$_d231" 2>&1)"
        fi
    done
}
i231=0
for jflag231 in "" "-j1" "-j2" "-j100"; do
    i231=$((i231 + 1))
    flakiness_round "$i231" "$ROOT/t231r$i231" "$jflag231" \
        "${jflag231:-default -j}"
done

# ------------------------------ 232. shared packages prefer their common URLs
# parallel-projeny.txt: "If multiple projeny files want the same package but
# have different URLs, then you should prioritize trying whatever URLs they
# have in common; otherwise just pick one." x.projeny lists a shared mirror
# first and a private one second; y.projeny lists the shared mirror and a
# different private one. The batch must download the archive exactly once,
# from the URL both files share — never from either private mirror.
T232="$ROOT/t232"
mkdir -p "$T232/uniqx" "$T232/uniqy"
cp "$T222/fake-1.0.tar.gz" "$T232/fake-1.0.tar.gz"
cp "$T222/fake-1.0.tar.gz" "$T232/uniqx/fake-1.0.tar.gz"
cp "$T222/fake-1.0.tar.gz" "$T232/uniqy/fake-1.0.tar.gz"
h232="$h222"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nURL: file://%s/uniqx/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakex\n\n    Shared mirror first.\n\n' \
    "$T232" "$h232" "$T232" "$h232" > "$T232/x.projeny"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nURL: file://%s/uniqy/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakey\n\n    Shared mirror first too.\n\n' \
    "$T232" "$h232" "$T232" "$h232" > "$T232/y.projeny"
out="$(run_in "$T232" "$PROJENY" setup x.projeny y.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the overlapping-URL-set parallel setup exits 0"
else
    fail "the overlapping-URL-set parallel setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"WARNING: 2 PROJENY FILES DOWNLOAD ARCHIVES WITH THE SAME NAME 'fake-1.0.tar.gz' BUT WITH DIFFERENT URL SETS: "*)
    ok "the overlapping sets still warn (the URL sets differ)"
    ;;
*)
    fail "the overlapping sets still warn (the URL sets differ)" "out: $out"
    ;;
esac
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'fake-1.0.tar.gz' from '")"
if [ "$ndl" -eq 1 ]; then
    ok "the shared package downloads exactly once"
else
    fail "the shared package downloads exactly once" \
         "got $ndl downloading lines (out: $out)"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'fake-1.0.tar.gz' from 'file://$T232/fake-1.0.tar.gz'")"
if [ "$ndl" -eq 1 ]; then
    ok "the download used the URL both files share"
else
    fail "the download used the URL both files share" \
         "got $ndl downloading lines from the common URL (out: $out)"
fi
if printf '%s\n' "$out" | grep -q "downloading 'fake-1.0.tar.gz' from 'file://$T232/uniqx/"; then
    fail "no download ever comes from x's private mirror" "out: $out"
else
    ok "no download ever comes from x's private mirror"
fi
if printf '%s\n' "$out" | grep -q "downloading 'fake-1.0.tar.gz' from 'file://$T232/uniqy/"; then
    fail "no download ever comes from y's private mirror" "out: $out"
else
    ok "no download ever comes from y's private mirror"
fi
if [ -f "$T232/fakex/README" ] && [ -f "$T232/fakey/README" ]; then
    ok "both overlapping-URL-set projects check out"
else
    fail "both overlapping-URL-set projects check out" "ls: $(ls "$T232" 2>&1)"
fi

# ---------------- 233. a failed mirror falls through inside the batch
# A package's candidate URLs are mirrors: the batch scheduler moves to the
# next candidate when one fails to transfer, and a package that succeeds on
# a later mirror never reaches the retry pass at all. m.projeny names a
# nonexistent first URL (whose basename still names the archive and the
# snapshot) and the real tarball second; o.projeny names only the real one.
T233="$ROOT/t233"
mkdir -p "$T233"
cp "$T222/fake-1.0.tar.gz" "$T233/"
h233="$h222"
printf 'URL: file://%s/nonexistent-XYZ.tar.gz %s\nURL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakem\n\n    Mirror fallback in the batch.\n\n' \
    "$T233" "$h233" "$T233" "$h233" > "$T233/m.projeny"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakeo\n\n    Plain.\n\n' \
    "$T233" "$h233" > "$T233/o.projeny"
out="$(run_in "$T233" "$PROJENY" setup m.projeny o.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the batch mirror fallback setup exits 0"
else
    fail "the batch mirror fallback setup exits 0" "exit=$rc out: $out"
fi
nfail="$(printf '%s\n' "$out" | grep -c "^projeny: warning: failed to download 'nonexistent-XYZ.tar.gz' from 'file://$T233/nonexistent-XYZ.tar.gz'")"
if [ "$nfail" -eq 1 ]; then
    ok "the dead mirror warned exactly once"
else
    fail "the dead mirror warned exactly once" "got $nfail lines (out: $out)"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'nonexistent-XYZ.tar.gz' from '")"
if [ "$ndl" -eq 2 ]; then
    ok "the package tried the dead mirror then its live one"
else
    fail "the package tried the dead mirror then its live one" \
         "got $ndl downloading lines for the package (out: $out)"
fi
nretry="$(printf '%s\n' "$out" | grep -c "^projeny: retrying ")"
if [ "$nretry" -eq 0 ]; then
    ok "a package that recovered on a later mirror needs no retry pass"
else
    fail "a package that recovered on a later mirror needs no retry pass" \
         "got $nretry retrying lines (out: $out)"
fi
if [ "$(cat "$T233/fakem/README" 2>/dev/null)" = "hello v1" ]; then
    ok "the mirror-fallback project checked out"
else
    fail "the mirror-fallback project checked out" \
         "README: $(cat "$T233/fakem/README" 2>&1)"
fi
if cmp -s "$T233/.nonexistent-XYZ.tar.gz.snapshot" "$T233/fake-1.0.tar.gz"; then
    ok "the snapshot is still named after the first URL's basename"
else
    fail "the snapshot is still named after the first URL's basename" \
         "the snapshot is missing or differs from the tarball"
fi

# ---------------- 234. the retry pass: announced once, then the guard dies
# A package whose every URL fails is retried exactly once (the retry pass
# restarts from the first candidate URL), the retry is announced with the
# failed packages' names, and only then does the batch give up: the
# contributing project dies with the labeled guard error and the command
# exits nonzero with the summary line — while the good projects still set
# up. A second invocation with TWO failing packages pins the parallel
# retry pass's announcement listing both names.
T234="$ROOT/t234"
mkdir -p "$T234/sub"
cp "$T222/fake-1.0.tar.gz" "$T234/"
h234="$h222"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakeg\n\n    Good.\n\n' \
    "$T234" "$h234" > "$T234/good.projeny"
printf 'URL: file://%s/gone-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakebad\n\n    Unreachable.\n\n' \
    "$T234" "$h234" > "$T234/sub/bad.projeny"
out="$(run_in "$T234" "$PROJENY" setup good.projeny sub/bad.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "the unreachable-package setup exits nonzero"
else
    fail "the unreachable-package setup exits nonzero" "out: $out"
fi
nretry="$(printf '%s\n' "$out" | grep -c "^projeny: retrying 1 failed download(s): gone-1.0.tar.gz$")"
if [ "$nretry" -eq 1 ]; then
    ok "the retry pass is announced with the failed package's name"
else
    fail "the retry pass is announced with the failed package's name" \
         "got $nretry retrying lines (out: $out)"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'gone-1.0.tar.gz' from '")"
if [ "$ndl" -eq 2 ]; then
    ok "the failed package was attempted once per pass (2 downloading lines)"
else
    fail "the failed package was attempted once per pass (2 downloading lines)" \
         "got $ndl downloading lines (out: $out)"
fi
nfail="$(printf '%s\n' "$out" | grep -c "^projeny: warning: failed to obtain 'gone-1.0.tar.gz':")"
if [ "$nfail" -eq 1 ]; then
    ok "the batch failure is summarized with 'failed to obtain'"
else
    fail "the batch failure is summarized with 'failed to obtain'" \
         "got $nfail lines (out: $out)"
fi
case "$out" in
*"[bad.projeny] error: 'fakebad' needs archive 'gone-1.0.tar.gz', but the parallel download phase failed to obtain it"*)
    ok "the guarded project dies with the labeled guard error"
    ;;
*)
    fail "the guarded project dies with the labeled guard error" "out: $out"
    ;;
esac
case "$out" in
*"projeny: 1 of 2 setup(s) failed: bad.projeny"*)
    ok "the summary line names the failed project"
    ;;
*)
    fail "the summary line names the failed project" "out: $out"
    ;;
esac
if [ -f "$T234/fakeg/README" ]; then
    ok "the good project still set up alongside the failed one"
else
    fail "the good project still set up alongside the failed one" \
         "ls: $(ls "$T234" 2>&1)"
fi
if [ -e "$T234/fakebad" ]; then
    fail "the failed project leaves no checkout" "ls: $(ls "$T234")"
else
    ok "the failed project leaves no checkout"
fi
T234B="$ROOT/t234b"
mkdir -p "$T234B/sub"
cp "$T222/fake-1.0.tar.gz" "$T234B/"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakeg2\n\n    Good.\n\n' \
    "$T234B" "$h234" > "$T234B/good.projeny"
printf 'URL: file://%s/gone1-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakeb1\n\n    Unreachable.\n\n' \
    "$T234B" "$h234" > "$T234B/sub/bad1.projeny"
printf 'URL: file://%s/gone2-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakeb2\n\n    Unreachable.\n\n' \
    "$T234B" "$h234" > "$T234B/sub/bad2.projeny"
out="$(run_in "$T234B" "$PROJENY" setup good.projeny sub/bad1.projeny \
    sub/bad2.projeny 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "the two-unreachable-packages setup exits nonzero"
else
    fail "the two-unreachable-packages setup exits nonzero" "out: $out"
fi
nretry="$(printf '%s\n' "$out" | grep -c "^projeny: retrying 2 failed download(s): gone1-1.0.tar.gz, gone2-1.0.tar.gz$")"
if [ "$nretry" -eq 1 ]; then
    ok "the parallel retry pass lists every failed package"
else
    fail "the parallel retry pass lists every failed package" \
         "got $nretry retrying lines (out: $out)"
fi
case "$out" in
*"projeny: 2 of 3 setup(s) failed: bad1.projeny, bad2.projeny"*)
    ok "the summary lists every failed project"
    ;;
*)
    fail "the summary lists every failed project" "out: $out"
    ;;
esac
if [ -f "$T234B/fakeg2/README" ] && [ ! -e "$T234B/fakeb1" ] &&
    [ ! -e "$T234B/fakeb2" ]; then
    ok "the good project set up and the failed ones left nothing"
else
    fail "the good project set up and the failed ones left nothing" \
         "ls: $(ls "$T234B" 2>&1)"
fi

# ---------------- 235. duplicate package/extract pairs collapse
# The duplicate-argument rule covers the paired forms too: naming one
# project twice in one package/extract invocation warns and runs the
# operation exactly once — the second pair's output/destination is named
# in the warning and is never created.
T235="$ROOT/t235"
mkdir -p "$T235"
cp "$T222/fake-1.0.tar.gz" "$T235/classic-1.0.tar.gz"
printf 'Archive: classic-1.0.tar.gz\nOrigname: fake-1.0\nName: classic\n\n    Classic.\n\n' \
    > "$T235/classic.projeny"
out="$(run_in "$T235" "$PROJENY" package classic.projeny out1.tar.gz \
    classic.projeny out2.tar.gz 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the duplicate-pair package exits 0"
else
    fail "the duplicate-pair package exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"'classic.projeny' is listed more than once; packaging it only once (the output 'out2.tar.gz' is ignored)"*)
    ok "the duplicate pair warns and names the ignored output"
    ;;
*)
    fail "the duplicate pair warns and names the ignored output" "out: $out"
    ;;
esac
if [ -f "$T235/out1.tar.gz" ]; then
    ok "the first pair's output was packaged"
else
    fail "the first pair's output was packaged" "ls: $(ls "$T235" 2>&1)"
fi
if [ -e "$T235/out2.tar.gz" ]; then
    fail "the ignored output was never created" "ls: $(ls "$T235")"
else
    ok "the ignored output was never created"
fi
if tar -tzf "$T235/out1.tar.gz" | grep -q "^out1/README$"; then
    ok "the collapsed package still holds the tracked files"
else
    fail "the collapsed package still holds the tracked files" \
         "tar: $(tar -tzf "$T235/out1.tar.gz" 2>&1)"
fi
out="$(run_in "$T235" "$PROJENY" extract classic.projeny ex1 \
    classic.projeny ex2 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the duplicate-pair extract exits 0"
else
    fail "the duplicate-pair extract exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"'classic.projeny' is listed more than once; extracting it only once (the destination 'ex2' is ignored)"*)
    ok "the duplicate pair warns and names the ignored destination"
    ;;
*)
    fail "the duplicate pair warns and names the ignored destination" \
         "out: $out"
    ;;
esac
if [ "$(cat "$T235/ex1/README" 2>/dev/null)" = "hello v1" ]; then
    ok "the first pair's destination was extracted"
else
    fail "the first pair's destination was extracted" \
         "ls: $(ls "$T235/ex1" 2>&1)"
fi
if [ -e "$T235/ex2" ]; then
    fail "the ignored destination was never created" "ls: $(ls "$T235")"
else
    ok "the ignored destination was never created"
fi

# Two DIFFERENT projects whose outputs (or destinations) resolve to the same
# path would race on one file when the workers run in parallel, so run_multi
# warns once per colliding group after dedupe. The spellings differ but
# absolutize to one path; the racy runs still proceed, so only the warning
# itself is asserted here (not the exit code or the surviving bytes).
T235B="$ROOT/t235b"
mkdir -p "$T235B"
cp "$T235/classic-1.0.tar.gz" "$T235/classic.projeny" "$T235B/"
printf 'Archive: classic-1.0.tar.gz\nOrigname: fake-1.0\nName: double\n\n    Twin.\n\n' \
    > "$T235B/twin.projeny"
out="$(run_in "$T235B" "$PROJENY" package classic.projeny same.tar.gz \
    twin.projeny ./same.tar.gz 2>&1)"
case "$out" in
*"package outputs 'same.tar.gz' and './same.tar.gz' both resolve to "*\
*"the parallel runs write the same file"*)
    ok "the colliding package outputs warn"
    ;;
*)
    fail "the colliding package outputs warn" "out: $out"
    ;;
esac
nout235="$(printf '%s\n' "$out" | grep -c "the parallel runs write the same file")"
if [ "$nout235" -eq 1 ]; then
    ok "the colliding package outputs warn exactly once"
else
    fail "the colliding package outputs warn exactly once" \
         "got $nout235 warning lines (out: $out)"
fi
out="$(run_in "$T235B" "$PROJENY" extract classic.projeny samedest \
    twin.projeny ./samedest 2>&1)"
case "$out" in
*"extract destinations 'samedest' and './samedest' both resolve to "*\
*"the parallel runs write the same file"*)
    ok "the colliding extract destinations warn"
    ;;
*)
    fail "the colliding extract destinations warn" "out: $out"
    ;;
esac

# ---------------- 236. download: several pairs, and the already-have skip
# One invocation takes any number of URL HASH pairs: every file lands in
# the cwd byte-exact, named after its URL's basename, with one note per
# verified package. A second run over files that already verify is a
# no-op — each reports the keep ("already have ... (blake3 hash
# verified)") and nothing downloads at all.
T236="$ROOT/t236"
mkdir -p "$T236/dlb-1.0"
printf 'hello dl-b\n' > "$T236/dlb-1.0/README"
(cd "$T236" && tar -czf dl-b-1.0.tar.gz dlb-1.0 && rm -rf dlb-1.0)
cp "$T222/fake-1.0.tar.gz" "$T236/dl-a-1.0.tar.gz"
h236a="$("$PROJENY" hash "$T236/dl-a-1.0.tar.gz")"
h236b="$("$PROJENY" hash "$T236/dl-b-1.0.tar.gz")"
T236R="$ROOT/t236r"
mkdir -p "$T236R"
out="$(run_in "$T236R" "$PROJENY" download "file://$T236/dl-a-1.0.tar.gz" \
    "$h236a" "file://$T236/dl-b-1.0.tar.gz" "$h236b" 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the two-pair download exits 0"
else
    fail "the two-pair download exits 0" "exit=$rc out: $out"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading '")"
if [ "$ndl" -eq 2 ]; then
    ok "both pairs downloaded (2 downloading lines)"
else
    fail "both pairs downloaded (2 downloading lines)" \
         "got $ndl downloading lines (out: $out)"
fi
if printf '%s\n' "$out" | grep -qF 'projeny: wrote dl-a-1.0.tar.gz (' &&
    printf '%s\n' "$out" | grep -qF 'projeny: wrote dl-b-1.0.tar.gz ('; then
    ok "each verified package reports the file it wrote"
else
    fail "each verified package reports the file it wrote" "out: $out"
fi
if cmp -s "$T236R/dl-a-1.0.tar.gz" "$T236/dl-a-1.0.tar.gz" &&
    cmp -s "$T236R/dl-b-1.0.tar.gz" "$T236/dl-b-1.0.tar.gz"; then
    ok "both downloaded files are byte-exact in the cwd"
else
    fail "both downloaded files are byte-exact in the cwd" \
         "ls: $(ls "$T236R" 2>&1)"
fi
out="$(run_in "$T236R" "$PROJENY" download "file://$T236/dl-a-1.0.tar.gz" \
    "$h236a" "file://$T236/dl-b-1.0.tar.gz" "$h236b" 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the re-run over present files exits 0"
else
    fail "the re-run over present files exits 0" "exit=$rc out: $out"
fi
if printf '%s\n' "$out" | grep -qF 'projeny: already have dl-a-1.0.tar.gz (blake3 hash verified)' &&
    printf '%s\n' "$out" | grep -qF 'projeny: already have dl-b-1.0.tar.gz (blake3 hash verified)'; then
    ok "both present files report the already-have keep"
else
    fail "both present files report the already-have keep" "out: $out"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading '")"
if [ "$ndl" -eq 0 ]; then
    ok "the already-have run downloads nothing"
else
    fail "the already-have run downloads nothing" \
         "got $ndl downloading lines (out: $out)"
fi
if cmp -s "$T236R/dl-a-1.0.tar.gz" "$T236/dl-a-1.0.tar.gz" &&
    cmp -s "$T236R/dl-b-1.0.tar.gz" "$T236/dl-b-1.0.tar.gz"; then
    ok "the already-have run leaves both files untouched"
else
    fail "the already-have run leaves both files untouched" \
         "the files differ from their sources"
fi

# ---------------- 237. download: conflicting hashes, uppercase, -j2 -c2
# The same URL listed twice with two different hashes gets the loud
# (ALL-CAPS) hash warning exactly once and still downloads (bytes
# matching EITHER listed hash verify — one of the two hashes is simply
# wrong, and the warning already said so). Hashes are case-insensitive:
# an uppercase digest downloads and verifies, and a follow-up lowercase
# run proves the normalization by reporting the already-have skip.
# -j2 -c2 is accepted alongside the pairs.
T237="$ROOT/t237"
mkdir -p "$T237"
cp "$T222/fake-1.0.tar.gz" "$T237/"
h237="$h222"
zero237="0000000000000000000000000000000000000000000000000000000000000000"
T237A="$ROOT/t237a"
mkdir -p "$T237A"
out="$(run_in "$T237A" "$PROJENY" download "file://$T237/fake-1.0.tar.gz" \
    "$h237" "file://$T237/fake-1.0.tar.gz" "$zero237" 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the same-URL-two-hashes download exits 0"
else
    fail "the same-URL-two-hashes download exits 0" "exit=$rc out: $out"
fi
nwarn="$(printf '%s\n' "$out" | grep -cF "WARNING: URL 'file://$T237/fake-1.0.tar.gz' IS LISTED WITH DIFFERENT BLAKE3 HASHES ($h237, $zero237)")"
if [ "$nwarn" -eq 1 ]; then
    ok "the conflicting hashes warn loudly exactly once"
else
    fail "the conflicting hashes warn loudly exactly once" \
         "got $nwarn warning lines (out: $out)"
fi
if cmp -s "$T237A/fake-1.0.tar.gz" "$T237/fake-1.0.tar.gz"; then
    ok "the conflicting-hash download still wrote the verified bytes"
else
    fail "the conflicting-hash download still wrote the verified bytes" \
         "the file differs from the source"
fi
T237B="$ROOT/t237b"
mkdir -p "$T237B"
up237="$(printf '%s' "$h237" | tr 'a-f' 'A-F')"
run_in "$T237B" expect_ok "download accepts an uppercase hash" "$PROJENY" \
    download "file://$T237/fake-1.0.tar.gz" "$up237"
if cmp -s "$T237B/fake-1.0.tar.gz" "$T237/fake-1.0.tar.gz"; then
    ok "the uppercase-hash download is byte-exact"
else
    fail "the uppercase-hash download is byte-exact" \
         "the file differs from the source"
fi
out="$(run_in "$T237B" "$PROJENY" download "file://$T237/fake-1.0.tar.gz" \
    "$h237" 2>&1)"
rc=$?
case "$out" in
*"projeny: already have fake-1.0.tar.gz (blake3 hash verified)"*)
    if [ $rc -eq 0 ]; then
        ok "the lowercase re-run keeps the uppercase download (hashes normalize)"
    else
        fail "the lowercase re-run keeps the uppercase download (hashes normalize)" \
             "output matched but exit=$rc out: $out"
    fi
    ;;
*)
    fail "the lowercase re-run keeps the uppercase download (hashes normalize)" \
         "exit=$rc out: $out"
    ;;
esac
T237C="$ROOT/t237c"
mkdir -p "$T237C"
run_in "$T237C" expect_ok "download accepts -j2 -c2" "$PROJENY" download \
    -j2 -c2 "file://$T237/fake-1.0.tar.gz" "$h237"
if cmp -s "$T237C/fake-1.0.tar.gz" "$T237/fake-1.0.tar.gz"; then
    ok "the -j2 -c2 download is byte-exact"
else
    fail "the -j2 -c2 download is byte-exact" "the file differs from the source"
fi

# ---------------- 238. download: the exact hard-error wordings
# Section 229 pins that these refusals fail; this section pins the exact
# die messages: a hash that is not exactly 64 hex chars dies naming the
# hash and the URL, and a URL whose basename would be empty dies with
# "does not name a file".
T238="$ROOT/t238"
mkdir -p "$T238"
short238="${h222%?}" # 63 of the 64 hex chars
out="$(run_in "$T238" "$PROJENY" download "file://$T238/no-such.tar.gz" \
    "$short238" 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "the 63-char hash is refused"
else
    fail "the 63-char hash is refused" "out: $out"
fi
case "$out" in
*"invalid blake3 hash '$short238' for file://$T238/no-such.tar.gz"*)
    ok "the short-hash error names the hash and the URL"
    ;;
*)
    fail "the short-hash error names the hash and the URL" "out: $out"
    ;;
esac
out="$(run_in "$T238" "$PROJENY" download "file://$T238/" "$h222" 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "the trailing-slash URL is refused"
else
    fail "the trailing-slash URL is refused" "out: $out"
fi
case "$out" in
*"URL 'file://$T238/' does not name a file"*)
    ok "the no-basename error says the URL does not name a file"
    ;;
*)
    fail "the no-basename error says the URL does not name a file" \
         "out: $out"
    ;;
esac

# ---------------- 239. two spellings of one project collapse
# The dedupe keys on the resolved .projeny path, not the spelling: the
# bare-name alias (`a` for a.projeny — the checkout does not exist yet, so
# the <arg>.projeny sibling rule resolves it) and the explicit file name
# collapse into one setup with one warning. Once a checkout exists, the
# workdir-sibling spelling (the workdir named by Name: is `fakeb`, sitting
# next to `fakeb.projeny`) aliases the same way.
T239="$ROOT/t239"
mkdir -p "$T239"
cp "$T222/fake-1.0.tar.gz" "$T239/"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakea\n\n    Alias.\n\n' \
    "$T239" "$h222" > "$T239/a.projeny"
out="$(run_in "$T239" "$PROJENY" setup a a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the aliased-argument setup exits 0"
else
    fail "the aliased-argument setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"'a.projeny' is listed more than once; setting it up only once"*)
    ok "the alias collapse warns"
    ;;
*)
    fail "the alias collapse warns" "out: $out"
    ;;
esac
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading '")"
if [ "$ndl" -eq 1 ]; then
    ok "the collapsed alias setup downloads exactly once"
else
    fail "the collapsed alias setup downloads exactly once" \
         "got $ndl downloading lines (out: $out)"
fi
nset="$(printf '%s\n' "$out" | grep -c "up 'fakea' from 'fake-1.0.tar.gz")"
if [ "$nset" -eq 1 ]; then
    ok "the collapsed alias setup runs the project once"
else
    fail "the collapsed alias setup runs the project once" \
         "got $nset setup lines (out: $out)"
fi
if [ -d "$T239/fakea" ] && [ -f "$T239/fakea/README" ]; then
    ok "the collapsed alias setup produced the checkout"
else
    fail "the collapsed alias setup produced the checkout" \
         "ls: $(ls "$T239" 2>&1)"
fi
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fakeb\n\n    Workdir alias.\n\n' \
    "$T239" "$h222" > "$T239/fakeb.projeny"
run_in "$T239" expect_ok "the workdir-alias fixture sets up" "$PROJENY" \
    setup fakeb.projeny
out="$(run_in "$T239" "$PROJENY" setup fakeb fakeb.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the workdir-sibling alias setup exits 0"
else
    fail "the workdir-sibling alias setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"'fakeb.projeny' is listed more than once; setting it up only once"*)
    ok "the workdir-sibling alias collapse warns too"
    ;;
*)
    fail "the workdir-sibling alias collapse warns too" "out: $out"
    ;;
esac
nset="$(printf '%s\n' "$out" | grep -c "up 'fakeb' from 'fake-1.0.tar.gz")"
if [ "$nset" -eq 1 ]; then
    ok "the workdir-sibling alias runs the project once"
else
    fail "the workdir-sibling alias runs the project once" \
         "got $nset setup lines (out: $out)"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading '")"
if [ "$ndl" -eq 0 ]; then
    ok "the workdir-sibling alias re-setup downloads nothing"
else
    fail "the workdir-sibling alias re-setup downloads nothing" \
         "got $ndl downloading lines (out: $out)"
fi

# ---------------- 240. -j/-c are setup/package/extract/download-only
# Every other command keeps its exact argument shape: a -j2 (or --jobs=2)
# token after `status`/`commit` is just one argument too many, so the
# command prints the usage text and exits 1 — the parallel options are
# never silently eaten by the non-parallel commands.
T240="$ROOT/t240"
mkdir -p "$T240"
cp "$T222/fake-1.0.tar.gz" "$T240/"
cp "$T222/a.projeny" "$T240/"
out="$(run_in "$T240" "$PROJENY" status a.projeny -j2 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "status with -j2 exits nonzero"
else
    fail "status with -j2 exits nonzero" "out: $out"
fi
case "$out" in
*"usage: "*"status <f.projeny|dir>"*)
    ok "status with -j2 prints the usage text"
    ;;
*)
    fail "status with -j2 prints the usage text" "out: $out"
    ;;
esac
out="$(run_in "$T240" "$PROJENY" status a.projeny --jobs=2 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "status with --jobs=2 exits nonzero"
else
    fail "status with --jobs=2 exits nonzero" "out: $out"
fi
case "$out" in
*"usage: "*)
    ok "status with --jobs=2 prints the usage text"
    ;;
*)
    fail "status with --jobs=2 prints the usage text" "out: $out"
    ;;
esac
out="$(run_in "$T240" "$PROJENY" commit a.projeny -j2 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "commit with -j2 exits nonzero"
else
    fail "commit with -j2 exits nonzero" "out: $out"
fi
case "$out" in
*"usage: "*)
    ok "commit with -j2 prints the usage text"
    ;;
*)
    fail "commit with -j2 prints the usage text" "out: $out"
    ;;
esac

# ------------------------------------------ 241. parallel conflicted setup
# Two git-conflicted .projeny files in ONE directory sharing one archive
# basename (so both projects share one .<archive>.snapshot), set up in one
# parallel `projeny setup a.projeny b.projeny -j2`. Neither file parses
# during the planning phase (conflict markers), so the batch plan is EMPTY
# and no batch download runs; each per-project worker then needs the shared
# archive outside the plan. The serialized fallback (mutex + re-check under
# it) must yield exactly one real download regardless of interleaving, the
# conflicted fresh-setup path must not stale the shared snapshot (the setup
# journal exists from setup_conflicted_merge's first write onward, so the
# journal guard keeps disregard_stale_state out), and both workdirs must
# come out complete. Both conflict sides are IDENTICAL, so the force-resolve
# is deterministic and the merge trivial; the opener is merge-convention
# `<<<<<<< HEAD` (which never votes) and the closer names upstream, so
# conflict_sides_swapped resolves decisively with no status file present.
# The whole section loops over fresh directories and thread counts so an
# interleaving-dependent race cannot hide behind one lucky schedule.
T241="$ROOT/t241"
mkdir -p "$T241/fake-1.0/src"
printf 'int alpha = 1;\n' > "$T241/fake-1.0/src/a.c"
printf 'hello v1\n' > "$T241/fake-1.0/README"
(cd "$T241" && tar -czf fake-1.0.tar.gz fake-1.0 && rm -rf fake-1.0)
h241="$("$PROJENY" hash "$T241/fake-1.0.tar.gz")"
# The resolved (force-taken upstream) form of a conflicted fixture file, and
# the conflicted form itself: git-style markers around the URL: header only,
# both sides byte-identical, `>>>>>>> upstream` closer so the direction
# resolves without a status copy.
plain_241() {
    _dir="$1"; _name="$2"
    printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake%s\n\n    Conflicted parallel %s.\n\n' \
        "$_dir" "$h241" "$_name" "$_name" > "$_dir/$_name.projeny"
}
conflicted_241() {
    _dir="$1"; _name="$2"
    {
        printf '<<<<<<< HEAD\n'
        sed -n '1p' "$_dir/$_name.projeny"
        printf '=======\n'
        sed -n '1p' "$_dir/$_name.projeny"
        printf '>>>>>>> upstream\n'
        sed -n '2,$p' "$_dir/$_name.projeny"
    } > "$_dir/$_name.projeny.conflicted"
    mv "$_dir/$_name.projeny.conflicted" "$_dir/$_name.projeny"
}
conflicted_round_241() {
    _r241="$1"; _d241="$2"; _j241="$3"; _jdisp241="$4"
    mkdir -p "$_d241"
    cp "$T241/fake-1.0.tar.gz" "$_d241/"
    plain_241 "$_d241" a
    cp "$_d241/a.projeny" "$_d241/expected-a.projeny"
    conflicted_241 "$_d241" a
    plain_241 "$_d241" b
    cp "$_d241/b.projeny" "$_d241/expected-b.projeny"
    conflicted_241 "$_d241" b
    out="$(run_in "$_d241" "$PROJENY" setup $_j241 a.projeny b.projeny 2>&1)"
    rc=$?
    if [ $rc -eq 0 ]; then
        ok "conflicted round $_r241 ($_jdisp241): parallel setup exits 0"
    else
        fail "conflicted round $_r241 ($_jdisp241): parallel setup exits 0" \
             "exit=$rc out: $out"
    fi
    # The plan is empty, so the batch download phase never runs: zero
    # batch-style lines ("downloading '<archive>' from '<url>'").
    nbatch="$(printf '%s\n' "$out" | grep -c "downloading 'fake-1.0.tar.gz' from '")"
    if [ "$nbatch" -eq 0 ]; then
        ok "conflicted round $_r241 ($_jdisp241): no batch download"
    else
        fail "conflicted round $_r241 ($_jdisp241): no batch download" \
             "got $nbatch batch-style lines (out: $out)"
    fi
    # Exactly one worker downloads outside the batch phase, announces the
    # URL exactly once, and reports exactly one hash verification.
    nfb="$(printf '%s\n' "$out" | grep -c "downloading 'fake-1.0.tar.gz' outside the batch download phase (conflicted projeny file)")"
    if [ "$nfb" -eq 1 ]; then
        ok "conflicted round $_r241 ($_jdisp241): the fallback note appears once"
    else
        fail "conflicted round $_r241 ($_jdisp241): the fallback note appears once" \
             "got $nfb fallback notes (out: $out)"
    fi
    nurl="$(printf '%s\n' "$out" | grep -c "downloading 'file://")"
    if [ "$nurl" -eq 1 ]; then
        ok "conflicted round $_r241 ($_jdisp241): the URL announcement appears once"
    else
        fail "conflicted round $_r241 ($_jdisp241): the URL announcement appears once" \
             "got $nurl URL announcements (out: $out)"
    fi
    nh241="$(printf '%s\n' "$out" | grep -c "blake3 hash verified")"
    if [ "$nh241" -eq 1 ]; then
        ok "conflicted round $_r241 ($_jdisp241): one hash verification"
    else
        fail "conflicted round $_r241 ($_jdisp241): one hash verification" \
             "got $nh241 hash-verified lines (out: $out)"
    fi
    nret="$(printf '%s\n' "$out" | grep -c "retrying")"
    if [ "$nret" -eq 0 ]; then
        ok "conflicted round $_r241 ($_jdisp241): no retrying line"
    else
        fail "conflicted round $_r241 ($_jdisp241): no retrying line" \
             "got $nret retrying lines (out: $out)"
    fi
    # Both workdirs exist with the archive's contents.
    for _wd241 in fakea fakeb; do
        if [ "$(cat "$_d241/$_wd241/README" 2>/dev/null)" = "hello v1" ] &&
           [ "$(cat "$_d241/$_wd241/src/a.c" 2>/dev/null)" = "int alpha = 1;" ]; then
            ok "conflicted round $_r241 ($_jdisp241): $_wd241 checked out"
        else
            fail "conflicted round $_r241 ($_jdisp241): $_wd241 checked out" \
                 "workdir missing or wrong: $(ls "$_d241" 2>&1)"
        fi
    done
    # Both .projeny files were force-resolved to the (identical) upstream
    # text: byte-equal to the plain form saved before the markers were
    # wrapped around the URL line.
    if cmp -s "$_d241/a.projeny" "$_d241/expected-a.projeny"; then
        ok "conflicted round $_r241 ($_jdisp241): a.projeny force-resolved"
    else
        fail "conflicted round $_r241 ($_jdisp241): a.projeny force-resolved" \
             "$(cat "$_d241/a.projeny" 2>&1)"
    fi
    if cmp -s "$_d241/b.projeny" "$_d241/expected-b.projeny"; then
        ok "conflicted round $_r241 ($_jdisp241): b.projeny force-resolved"
    else
        fail "conflicted round $_r241 ($_jdisp241): b.projeny force-resolved" \
             "$(cat "$_d241/b.projeny" 2>&1)"
    fi
    # One shared snapshot, byte-exact with the tarball.
    if cmp -s "$_d241/.fake-1.0.tar.gz.snapshot" "$_d241/fake-1.0.tar.gz"; then
        ok "conflicted round $_r241 ($_jdisp241): the shared snapshot is byte-exact"
    else
        fail "conflicted round $_r241 ($_jdisp241): the shared snapshot is byte-exact" \
             "the snapshot differs from the tarball"
    fi
    # Both status files record no conflicts (identical sides merge cleanly).
    nconf="$(cat "$_d241/.a.projeny.status" "$_d241/.b.projeny.status" 2>/dev/null | grep -c '^Conflict:')"
    if [ "$nconf" -eq 0 ]; then
        ok "conflicted round $_r241 ($_jdisp241): no conflicts recorded"
    else
        fail "conflicted round $_r241 ($_jdisp241): no conflicts recorded" \
             "got $nconf Conflict: lines"
    fi
    # No stale-renamed snapshot left behind: the journal guard kept
    # disregard_stale_state from staling the shared snapshot mid-flight.
    if [ ! -e "$_d241/.fake-1.0.tar.gz.snapshot.stale" ] &&
       [ ! -e "$_d241/fake-1.0.tar.gz.snapshot.stale" ]; then
        ok "conflicted round $_r241 ($_jdisp241): the snapshot was never staled"
    else
        fail "conflicted round $_r241 ($_jdisp241): the snapshot was never staled" \
             "ls: $(ls "$_d241" 2>&1)"
    fi
}
# Twelve rounds (three passes over four thread counts), each in a fresh
# directory: every round must satisfy every assertion above.
i241=0
for pass241 in 1 2 3; do
    for jflag241 in "" "-j1" "-j2" "-j100"; do
        i241=$((i241 + 1))
        conflicted_round_241 "$i241" "$ROOT/t241r$i241" "$jflag241" \
            "${jflag241:-default -j}"
    done
done

# --------- 242. parallel batch downloads: the combined progress line and
# the silent per-project snapshot phase
# Three URL-based projects sharing nothing (distinct archive basenames),
# each tarball big enough to make progress visible (the 220 fixture: 2 MiB
# of incompressible payload), set up in one parallel `projeny setup`. The
# batch download phase then reports ONE combined progress line
# ("projeny: download progress: <e1> <e2> ...") over the pass's ROSTER:
# one single-token entry per package announced so far, in
# first-announcement order — entry N is the Nth "downloading" announcement,
# and the roster only ever grows (a completed transfer stays listed at
# "100%"), so positions never shift. Each entry is a whole-percent ("N%")
# or a compact byte count ("NB" below 1 KiB, else one-decimal
# KiB/MiB/GiB) — never "?". Re-renders fire only when >= 200ms passed AND
# some entry's rendered string changed, and the round closes with exactly
# one deterministic final line listing every package of the round in
# announcement order, "100%" per transferred package. The per-project
# phase that follows must be SILENT
# about snapshots: "using existing snapshot" lines right after the batch's
# own download report would read like an error (the snapshot exists because
# the batch just wrote it); that note belongs to single-project runs only.
T242="$ROOT/t242"
mkdir -p "$T242"
for n242 in pa pb pc; do
    mkdir -p "$T242/$n242-1.0"
    printf 'hello %s\n' "$n242" > "$T242/$n242-1.0/README"
    # 32 * 65536 = 2 MiB of incompressible payload per tarball: progress is
    # visible, and the fixtures still build in well under a second.
    dd if=/dev/urandom of="$T242/$n242-1.0/blob" bs=65536 count=32 2>/dev/null
    (cd "$T242" && tar -czf "$n242-1.0.tar.gz" "$n242-1.0")
    rm -rf "$T242/$n242-1.0"
done
for n242 in pa pb pc; do
    h242="$("$PROJENY" hash "$T242/$n242-1.0.tar.gz")"
    printf 'URL: file://%s/%s-1.0.tar.gz %s\nOrigname: %s-1.0\nName: fake%s\n\n    Parallel progress project %s.\n\n' \
        "$T242" "$n242" "$h242" "$n242" "$n242" "$n242" > "$T242/$n242.projeny"
done
progress_lines() {
    # Every combined progress line of the captured output, with the '\r'
    # terminators turned into newlines (one line per '\r').
    printf '%s' "$1" | tr '\r' '\n' | grep '^projeny: download progress:'
}
# One combined-progress-line entry token: a whole-percent ("N%") or a
# compact byte count ("NB" below 1 KiB, else one-decimal KiB/MiB/GiB —
# never "?"). Always used inside a group; the alternation binds loosely.
ptok='[0-9]+%|[0-9]+(\.[0-9])?(KiB|MiB|GiB)|[0-9]+B'
T242R="$ROOT/t242r"
mkdir -p "$T242R"
cp "$T242/pa.projeny" "$T242/pb.projeny" "$T242/pc.projeny" \
   "$T242/pa-1.0.tar.gz" "$T242/pb-1.0.tar.gz" "$T242/pc-1.0.tar.gz" \
   "$T242R/"
out="$(run_in "$T242R" "$PROJENY" setup pa.projeny pb.projeny pc.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the three-package parallel setup exits 0"
else
    fail "the three-package parallel setup exits 0" "exit=$rc out: $out"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading '.*' from '")"
if [ "$ndl" -eq 3 ]; then
    ok "each of the three packages announced its download"
else
    fail "each of the three packages announced its download" \
         "got $ndl downloading lines (out: $out)"
fi
nprog="$(progress_lines "$out" | wc -l)"
if [ "$nprog" -ge 1 ]; then
    ok "the batch download reports progress lines ($nprog lines)"
else
    fail "the batch download reports progress lines" \
         "got $nprog 'projeny: download progress:' lines (out: $out)"
fi
nfmt="$(progress_lines "$out" | grep -Ec "^projeny: download progress: ($ptok)( ($ptok))*$")"
if [ "$nfmt" -eq "$nprog" ]; then
    ok "every progress line is the prefix plus one single-token entry per package"
else
    fail "every progress line is the prefix plus one single-token entry per package" \
         "$nfmt of $nprog lines matched 'projeny: download progress: (N%|bytes)...'"
fi
nwide="$(progress_lines "$out" | grep -Ec "^projeny: download progress: ($ptok)( ($ptok))( ($ptok))$")"
if [ "$nwide" -eq "$nprog" ]; then
    ok "every progress line carries the stable three-package roster"
else
    fail "every progress line carries the stable three-package roster" \
         "$nwide of $nprog lines had exactly three entries (out: $out)"
fi
if progress_lines "$out" | grep -q '?'; then
    fail "no progress line ever prints a '?'" \
         "a progress line carried a useless '?' entry (out: $out)"
else
    ok "no progress line ever prints a '?'"
fi
if progress_lines "$out" | grep -q '://'; then
    fail "progress lines never repeat a URL" \
         "a progress line carried a URL the announcements already named"
else
    ok "progress lines never repeat a URL"
fi
lastp="$(progress_lines "$out" | tail -1)"
if [ "$lastp" = "projeny: download progress: 100% 100% 100%" ]; then
    ok "the final progress line lists every package at 100%"
else
    fail "the final progress line lists every package at 100%" \
         "final line: $lastp"
fi
nsnap="$(printf '%s\n' "$out" | grep -c 'using existing snapshot')"
if [ "$nsnap" -eq 0 ]; then
    ok "the parallel setup never claims to be 'using existing snapshot'"
else
    fail "the parallel setup never claims to be 'using existing snapshot'" \
         "got $nsnap snapshot notes (out: $out)"
fi
for n242 in pa pb pc; do
    if [ "$(cat "$T242R/fake$n242/README" 2>/dev/null)" = "hello $n242" ]; then
        ok "the parallel setup checked out fake$n242"
    else
        fail "the parallel setup checked out fake$n242" \
             "README missing or wrong: $(cat "$T242R/fake$n242/README" 2>&1)"
    fi
done

# The same parallel setup with one project's snapshot PRE-SEEDED: planning
# skips that package (no download, no announcement) and the other two still
# batch-download; the final progress line then has exactly two entries, and
# the pre-seeded project's per-project phase must be as silent as the
# downloaded ones (before the multi-mode silence this printed the note).
T242S="$ROOT/t242s"
mkdir -p "$T242S"
cp "$T242/pa.projeny" "$T242/pb.projeny" "$T242/pc.projeny" \
   "$T242/pa-1.0.tar.gz" "$T242/pb-1.0.tar.gz" "$T242/pc-1.0.tar.gz" \
   "$T242S/"
cp "$T242/pa-1.0.tar.gz" "$T242S/.pa-1.0.tar.gz.snapshot"
out="$(run_in "$T242S" "$PROJENY" setup pa.projeny pb.projeny pc.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the parallel setup over a pre-seeded snapshot exits 0"
else
    fail "the parallel setup over a pre-seeded snapshot exits 0" \
         "exit=$rc out: $out"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading '.*' from '")"
if [ "$ndl" -eq 2 ]; then
    ok "the pre-seeded package downloads nothing and the other two download"
else
    fail "the pre-seeded package downloads nothing and the other two download" \
         "got $ndl downloading lines (out: $out)"
fi
nprog="$(progress_lines "$out" | wc -l)"
nfmt="$(progress_lines "$out" | grep -Ec "^projeny: download progress: ($ptok)( ($ptok))*$")"
nwide="$(progress_lines "$out" | grep -Ec "^projeny: download progress: ($ptok)( ($ptok))$")"
if [ "$nprog" -ge 1 ] && [ "$nfmt" -eq "$nprog" ] && [ "$nwide" -eq "$nprog" ]; then
    ok "the pre-seeded setup still reports well-formed two-entry progress lines"
else
    fail "the pre-seeded setup still reports well-formed two-entry progress lines" \
         "$nprog lines, $nfmt well-formed, $nwide two-entry (out: $out)"
fi
lastp="$(progress_lines "$out" | tail -1)"
if [ "$lastp" = "projeny: download progress: 100% 100%" ]; then
    ok "the final progress line covers exactly the downloaded packages"
else
    fail "the final progress line covers exactly the downloaded packages" \
         "final line: $lastp"
fi
nsnap="$(printf '%s\n' "$out" | grep -c 'using existing snapshot')"
if [ "$nsnap" -eq 0 ]; then
    ok "the pre-seeded snapshot is silent in parallel mode too"
else
    fail "the pre-seeded snapshot is silent in parallel mode too" \
         "got $nsnap snapshot notes (out: $out)"
fi
if [ "$(cat "$T242S/fakepa/README" 2>/dev/null)" = "hello pa" ]; then
    ok "the pre-seeded snapshot still set its project up"
else
    fail "the pre-seeded snapshot still set its project up" \
         "README: $(cat "$T242S/fakepa/README" 2>&1)"
fi

# An unknown transfer total (no Content-Length, the 220b one-file HTTP/1.0
# server, slowed to one 64 KiB chunk per 20ms so the transfer stays in
# flight across scheduler iterations) renders the compact byte count of
# what has arrived so far while in flight — growing "<n>B"/"<n.n>UNIT"
# tokens, never "?" — and its round still closes at "100%" for every
# transferred package: completing is what the closing line reports, no
# Content-Length required. Paired with a file:// package, which completes
# inside the first scheduler iteration and then stays listed at "100%".
# The roster follows the announcements, and specs sort by archive basename
# (fast-1.0.tar.gz announces before slow-1.0.tar.gz), so every in-flight
# line is the completed package's "100%" followed by the unknown-total
# transfer's byte entry.
T242U="$ROOT/t242u"
mkdir -p "$T242U"
mkdir -p "$T242U/slow-1.0"
printf 'hello slow\n' > "$T242U/slow-1.0/README"
dd if=/dev/urandom of="$T242U/slow-1.0/blob" bs=65536 count=32 2>/dev/null
(cd "$T242U" && tar -czf slow-1.0.tar.gz slow-1.0 && rm -rf slow-1.0)
mkdir -p "$T242U/fast-1.0"
printf 'hello fast\n' > "$T242U/fast-1.0/README"
dd if=/dev/urandom of="$T242U/fast-1.0/blob" bs=65536 count=32 2>/dev/null
(cd "$T242U" && tar -czf fast-1.0.tar.gz fast-1.0 && rm -rf fast-1.0)
hs242="$("$PROJENY" hash "$T242U/slow-1.0.tar.gz")"
hf242="$("$PROJENY" hash "$T242U/fast-1.0.tar.gz")"
cat > "$T242U/serv.py" <<'PYEOF'
import socket, sys, time
# One-file HTTP/1.0 server with NO Content-Length, dripping one 64 KiB
# chunk per 20ms: the transfer total stays unknown (dltotal 0) and the
# download stays in flight long enough for the batch scheduler's throttled
# in-flight progress renders to fire.
path, portfile = sys.argv[1], sys.argv[2]
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 0))
s.listen(16)
open(portfile, "w").write(str(s.getsockname()[1]))
while True:
    c, _ = s.accept()
    try:
        c.recv(65536)  # the GET; one connection per transfer
        c.sendall(b"HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n")
        with open(path, "rb") as f:
            while True:
                b = f.read(65536)
                if not b:
                    break
                c.sendall(b)
                time.sleep(0.02)
    except OSError:
        pass
    finally:
        c.close()
PYEOF
python3 "$T242U/serv.py" "$T242U/slow-1.0.tar.gz" "$T242U/port" &
srvpid=$!
port242u=""
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if [ -s "$T242U/port" ]; then port242u="$(cat "$T242U/port")"; break; fi
    sleep 0.05
done
if [ -z "$port242u" ]; then
    kill "$srvpid" 2>/dev/null
    fail "the slow no-Content-Length fixture server starts" \
         "port file never appeared"
else
    T242UD="$ROOT/t242u-run"
    mkdir -p "$T242UD"
    printf 'URL: http://127.0.0.1:%s/slow-1.0.tar.gz %s\nOrigname: slow-1.0\nName: fakeslow\n\n    Unknown total.\n\n' \
        "$port242u" "$hs242" > "$T242UD/slow.projeny"
    printf 'URL: file://%s/fast-1.0.tar.gz %s\nOrigname: fast-1.0\nName: fakefast\n\n    Known total.\n\n' \
        "$T242U" "$hf242" > "$T242UD/fast.projeny"
    out="$(run_in "$T242UD" "$PROJENY" setup slow.projeny fast.projeny 2>&1)"
    rc=$?
    kill "$srvpid" 2>/dev/null
    wait "$srvpid" 2>/dev/null

    if [ $rc -eq 0 ]; then
        ok "the unknown-total batch setup exits 0"
    else
        fail "the unknown-total batch setup exits 0" "exit=$rc out: $out"
    fi
    nprog="$(progress_lines "$out" | wc -l)"
    if [ "$nprog" -ge 2 ]; then
        ok "the unknown-total batch reports in-flight and closing lines ($nprog)"
    else
        fail "the unknown-total batch reports in-flight and closing lines" \
             "got $nprog progress lines (out: $out)"
    fi
    ninf="$(progress_lines "$out" | head -n -1 | wc -l)"
    nby="$(progress_lines "$out" | head -n -1 | \
          grep -Ec '^projeny: download progress: ([0-9]+%) ([0-9]+(\.[0-9])?(B|KiB|MiB|GiB))$')"
    if [ "$ninf" -ge 1 ] && [ "$nby" -eq "$ninf" ]; then
        ok "every in-flight line carries the unknown-total transfer's byte entry"
    else
        fail "every in-flight line carries the unknown-total transfer's byte entry" \
             "$nby of $ninf in-flight lines carried a byte entry (out: $out)"
    fi
    ndtok="$(progress_lines "$out" | head -n -1 | \
            sed 's/^projeny: download progress: .* //' | sort -u | wc -l)"
    if [ "$ndtok" -ge 2 ]; then
        ok "the unknown-total entry grows through byte tokens ($ndtok distinct)"
    else
        fail "the unknown-total entry grows through byte tokens" \
             "only $ndtok distinct byte token(s) over $ninf in-flight lines (out: $out)"
    fi
    if progress_lines "$out" | grep -q '?'; then
        fail "the unknown-total round never prints a '?'" \
             "a progress line carried a useless '?' entry (out: $out)"
    else
        ok "the unknown-total round never prints a '?'"
    fi
    lastp="$(progress_lines "$out" | tail -1)"
    if [ "$lastp" = "projeny: download progress: 100% 100%" ]; then
        ok "the unknown-total round still closes at 100% per package"
    else
        fail "the unknown-total round still closes at 100% per package" \
             "final line: $lastp"
    fi
    if cmp -s "$T242UD/.slow-1.0.tar.gz.snapshot" "$T242U/slow-1.0.tar.gz"; then
        ok "the unknown-total batch download is byte-exact"
    else
        fail "the unknown-total batch download is byte-exact" \
             "the snapshot differs from the served file"
    fi
fi

# --------- 243. single-project setup keeps the "using existing snapshot" note
# The note suppression is multi-mode-only: exactly one .projeny argument
# takes the legacy single-project fast path (no planning phase, no batch),
# so a URL project whose snapshot already verifies still prints
# "using existing snapshot ... (blake3 hash matches); skipping the
# download" — byte-identical to the pre-parallel behavior the 220 section
# pins.
T243="$ROOT/t243"
mkdir -p "$T243"
cp "$T242/pa.projeny" "$T242/pa-1.0.tar.gz" "$T243/"
out="$(run_in "$T243" "$PROJENY" setup pa.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the single-project setup exits 0"
else
    fail "the single-project setup exits 0" "exit=$rc out: $out"
fi
nsnap="$(printf '%s\n' "$out" | grep -c 'using existing snapshot')"
if [ "$nsnap" -eq 0 ]; then
    ok "the fresh single-project setup has no snapshot to announce"
else
    fail "the fresh single-project setup has no snapshot to announce" \
         "got $nsnap snapshot notes (out: $out)"
fi
out="$(run_in "$T243" "$PROJENY" setup pa.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the single-project re-setup exits 0"
else
    fail "the single-project re-setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"using existing snapshot '$T243/.pa-1.0.tar.gz.snapshot' (blake3 hash matches); skipping the download"*)
    ok "the single-project re-setup still announces the snapshot skip"
    ;;
*)
    fail "the single-project re-setup still announces the snapshot skip" \
         "out: $out"
    ;;
esac

# --------- 244. the download command reports the combined progress line
# `projeny download` runs its URL HASH pairs through the same batch, so it
# inherits the combined progress line: well-formed entries, one final line
# listing every package of the round. With a good pair named to sort FIRST
# and a nonexistent URL named to sort LAST, the round-0 closing line pins
# the announcement order ("100% 0B" — the failed package never received a
# byte and has no total, so it closes at "0B", never "?") and the retry
# pass prints its own closing line ("0B") for the package it re-attempted.
T244="$ROOT/t244"
mkdir -p "$T244"
cp "$T242/pa-1.0.tar.gz" "$T244/aa-good-1.0.tar.gz"
cp "$T242/pb-1.0.tar.gz" "$T244/mm-good-1.0.tar.gz"
ha244="$("$PROJENY" hash "$T244/aa-good-1.0.tar.gz")"
hm244="$("$PROJENY" hash "$T244/mm-good-1.0.tar.gz")"
T244R="$ROOT/t244r"
mkdir -p "$T244R"
out="$(run_in "$T244R" "$PROJENY" download "file://$T244/aa-good-1.0.tar.gz" \
    "$ha244" "file://$T244/mm-good-1.0.tar.gz" "$hm244" 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the two-pair download exits 0"
else
    fail "the two-pair download exits 0" "exit=$rc out: $out"
fi
nprog="$(progress_lines "$out" | wc -l)"
nfmt="$(progress_lines "$out" | grep -Ec "^projeny: download progress: ($ptok)( ($ptok))*$")"
if [ "$nprog" -ge 1 ] && [ "$nfmt" -eq "$nprog" ]; then
    ok "the download command reports well-formed progress lines"
else
    fail "the download command reports well-formed progress lines" \
         "$nprog lines, $nfmt well-formed (out: $out)"
fi
lastp="$(progress_lines "$out" | tail -1)"
if [ "$lastp" = "projeny: download progress: 100% 100%" ]; then
    ok "the download command's final progress line covers both packages"
else
    fail "the download command's final progress line covers both packages" \
         "final line: $lastp"
fi
if cmp -s "$T244R/aa-good-1.0.tar.gz" "$T244/aa-good-1.0.tar.gz" && \
    cmp -s "$T244R/mm-good-1.0.tar.gz" "$T244/mm-good-1.0.tar.gz"; then
    ok "the download command's files are byte-exact"
else
    fail "the download command's files are byte-exact" \
         "ls: $(ls "$T244R" 2>&1)"
fi
T244F="$ROOT/t244f"
mkdir -p "$T244F"
out="$(run_in "$T244F" "$PROJENY" download "file://$T244/aa-good-1.0.tar.gz" \
    "$ha244" "file://$T244/zz-gone-1.0.tar.gz" "$ha244" 2>&1)"
rc=$?
if [ $rc -ne 0 ]; then
    ok "the download with one nonexistent URL exits nonzero"
else
    fail "the download with one nonexistent URL exits nonzero" "out: $out"
fi
secondlast="$(progress_lines "$out" | tail -2 | head -1)"
if [ "$secondlast" = "projeny: download progress: 100% 0B" ]; then
    ok "the round-0 closing line lists every package in announcement order"
else
    fail "the round-0 closing line lists every package in announcement order" \
         "round-0 line: $secondlast"
fi
lastp="$(progress_lines "$out" | tail -1)"
if [ "$lastp" = "projeny: download progress: 0B" ]; then
    ok "the retry pass prints its own closing line for its own round"
else
    fail "the retry pass prints its own closing line for its own round" \
         "final line: $lastp"
fi

# --------- 245. erase-setup deletes the checkout and the status file
# `projeny erase-setup <f.projeny>` is the rm -rf of the checkout named by
# Name: plus the .<f>.projeny.status file. The .projeny file, the tarball,
# and the archive snapshot stay (the snapshot only goes with
# --erase-snapshots, see 247). Running it AGAIN warns that nothing was
# there and still exits 0: missing things are never errors. No --force is
# given anywhere in this section: the freshly set up project is clean (its
# status reports nothing but what setup wrote, and only untracked-free
# nothing at that), so the no-force check passes it and the erase runs
# exactly as it always has; the second run's checkout is gone, which the
# check treats as clean too (there is nothing left to destroy).
T245="$ROOT/t245"
mkdir -p "$T245"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T245/"
run_in "$T245" expect_ok "the erase-setup fixture sets up" "$PROJENY" \
    setup a.projeny
if [ -d "$T245/fakea" ] && [ -f "$T245/.a.projeny.status" ]; then
    ok "the erase-setup fixture has a checkout and a status file"
else
    fail "the erase-setup fixture has a checkout and a status file" \
         "ls: $(ls -A "$T245" 2>&1)"
fi
out="$(run_in "$T245" "$PROJENY" erase-setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "erase-setup exits 0"
else
    fail "erase-setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'fakea' (checkout 'fakea', status '.a.projeny.status')"*)
    ok "erase-setup reports what it erased"
    ;;
*)
    fail "erase-setup reports what it erased" "out: $out"
    ;;
esac
if [ ! -e "$T245/fakea" ] && [ ! -e "$T245/.a.projeny.status" ]; then
    ok "erase-setup removes the workdir and the status file"
else
    fail "erase-setup removes the workdir and the status file" \
         "ls: $(ls -A "$T245" 2>&1)"
fi
if [ -f "$T245/a.projeny" ] && [ -f "$T245/fake-1.0.tar.gz" ]; then
    ok "erase-setup keeps the .projeny file and the tarball"
else
    fail "erase-setup keeps the .projeny file and the tarball" \
         "ls: $(ls -A "$T245" 2>&1)"
fi
if [ -f "$T245/.fake-1.0.tar.gz.snapshot" ]; then
    ok "erase-setup keeps the snapshot without --erase-snapshots"
else
    fail "erase-setup keeps the snapshot without --erase-snapshots" \
         "ls: $(ls -A "$T245" 2>&1)"
fi
out="$(run_in "$T245" "$PROJENY" erase-setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the second erase-setup exits 0"
else
    fail "the second erase-setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"'fakea' did not exist; nothing to erase"*)
    ok "the second run warns the checkout was already gone"
    ;;
*)
    fail "the second run warns the checkout was already gone" "out: $out"
    ;;
esac
case "$out" in
*"'.a.projeny.status' did not exist; nothing to erase"*)
    ok "the second run warns the status file was already gone"
    ;;
*)
    fail "the second run warns the status file was already gone" "out: $out"
    ;;
esac
nerr="$(printf '%s\n' "$out" | grep -c 'error:')"
if [ "$nerr" -eq 0 ]; then
    ok "the second run prints no error"
else
    fail "the second run prints no error" "got $nerr error lines (out: $out)"
fi
case "$out" in
*"nothing to erase for 'fakea'"*)
    ok "the second run reports there was nothing to erase"
    ;;
*)
    fail "the second run reports there was nothing to erase" "out: $out"
    ;;
esac

# --------- 246. with --force, erase-setup discards uncommitted edits and
# the next setup is a fresh checkout, not a merge
# erase-setup --force discards the checkout whole, uncommitted changes
# included (the README edit below makes the project dirty, so without
# --force this erase would be refused — see 259), so the next setup starts
# from the tarball: a plain fresh-setup line (no "(no local changes)"
# re-setup wording, no merged: lines) and the local edit is gone.
T246="$ROOT/t246"
mkdir -p "$T246"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T246/"
run_in "$T246" expect_ok "the fresh-again fixture sets up" "$PROJENY" \
    setup a.projeny
printf 'a local uncommitted edit\n' >> "$T246/fakea/README"
expect_file_contains "the local edit is in the workdir" "$T246/fakea/README" \
    "a local uncommitted edit"
run_in "$T246" expect_ok "erase-setup --force of the edited checkout exits 0" \
    "$PROJENY" erase-setup a.projeny --force
if [ ! -e "$T246/fakea" ]; then
    ok "the edited checkout is gone"
else
    fail "the edited checkout is gone" "ls: $(ls -A "$T246" 2>&1)"
fi
out="$(run_in "$T246" "$PROJENY" setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "setup after erase-setup exits 0"
else
    fail "setup after erase-setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"set up 'fakea' from 'fake-1.0.tar.gz'"*)
    ok "setup after erase-setup reports the fresh checkout"
    ;;
*)
    fail "setup after erase-setup reports the fresh checkout" "out: $out"
    ;;
esac
nmerge="$(printf '%s\n' "$out" | grep -c 'no local changes')"
nmerged="$(printf '%s\n' "$out" | grep -c 'merged')"
if [ "$nmerge" -eq 0 ] && [ "$nmerged" -eq 0 ]; then
    ok "setup after erase-setup did not merge"
else
    fail "setup after erase-setup did not merge" \
         "$nmerge 'no local changes' and $nmerged 'merged' lines (out: $out)"
fi
if [ "$(cat "$T246/fakea/README" 2>/dev/null)" = "hello v1" ]; then
    ok "the uncommitted edit is gone after erase-setup + setup"
else
    fail "the uncommitted edit is gone after erase-setup + setup" \
         "README: $(cat "$T246/fakea/README" 2>&1)"
fi
if [ -f "$T246/.a.projeny.status" ]; then
    ok "setup after erase-setup wrote a fresh status file"
else
    fail "setup after erase-setup wrote a fresh status file" \
         "ls: $(ls -A "$T246" 2>&1)"
fi

# --------- 247. --erase-snapshots on a URL project: the exact snapshot only
# With --erase-snapshots, the .<archive>.snapshot the next setup would pick
# up (the URL's basename for a URL: project) is deleted, so the next setup
# re-downloads. A SIMILARLY named snapshot for a different version survives:
# only the exact basename goes.
T247="$ROOT/t247"
mkdir -p "$T247"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T247/"
run_in "$T247" expect_ok "the snapshot fixture sets up" "$PROJENY" \
    setup a.projeny
cp "$T247/.fake-1.0.tar.gz.snapshot" "$T247/.fake-9.9.9.tar.gz.snapshot"
out="$(run_in "$T247" "$PROJENY" erase-setup a.projeny --erase-snapshots 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "erase-setup --erase-snapshots (flag after the args) exits 0"
else
    fail "erase-setup --erase-snapshots (flag after the args) exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"snapshot '.fake-1.0.tar.gz.snapshot'"*)
    ok "the note names the erased snapshot"
    ;;
*)
    fail "the note names the erased snapshot" "out: $out"
    ;;
esac
if [ ! -e "$T247/.fake-1.0.tar.gz.snapshot" ] && \
   [ -f "$T247/.fake-9.9.9.tar.gz.snapshot" ]; then
    ok "the exact snapshot went and the similar-named one stayed"
else
    fail "the exact snapshot went and the similar-named one stayed" \
         "ls: $(ls -A "$T247" 2>&1)"
fi
if [ ! -e "$T247/fakea" ] && [ ! -e "$T247/.a.projeny.status" ]; then
    ok "the snapshot erase still removed the checkout and status file"
else
    fail "the snapshot erase still removed the checkout and status file" \
         "ls: $(ls -A "$T247" 2>&1)"
fi
out="$(run_in "$T247" "$PROJENY" setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the setup after a snapshot erase exits 0"
else
    fail "the setup after a snapshot erase exits 0" "exit=$rc out: $out"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'file://")"
if [ "$ndl" -ge 1 ]; then
    ok "the next setup re-downloads the archive"
else
    fail "the next setup re-downloads the archive" \
         "got $ndl downloading lines (out: $out)"
fi
if [ -f "$T247/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the next setup recreated the snapshot"
else
    fail "the next setup recreated the snapshot" "ls: $(ls -A "$T247" 2>&1)"
fi
if [ -f "$T247/.fake-9.9.9.tar.gz.snapshot" ]; then
    ok "the similar-named snapshot still survives the re-setup"
else
    fail "the similar-named snapshot still survives the re-setup" \
         "ls: $(ls -A "$T247" 2>&1)"
fi

# --------- 248. --erase-snapshots on a classic Archive: project
# The dotted snapshot goes; the checked-in tarball (git-tracked!) stays, so
# the next setup unpacks from it without any download and recreates the
# snapshot.
T248="$ROOT/t248"
make_project "$T248" classic 1.0
run_in "$T248" expect_ok "the classic snapshot fixture sets up" "$PROJENY" \
    setup classic.projeny
if [ -f "$T248/.classic-1.0.tar.gz.snapshot" ]; then
    ok "the classic fixture has a snapshot"
else
    fail "the classic fixture has a snapshot" "ls: $(ls -A "$T248" 2>&1)"
fi
run_in "$T248" expect_ok "erase-setup --erase-snapshots on the classic project exits 0" \
    "$PROJENY" erase-setup --erase-snapshots classic.projeny
if [ ! -e "$T248/.classic-1.0.tar.gz.snapshot" ]; then
    ok "the classic project's dotted snapshot is deleted"
else
    fail "the classic project's dotted snapshot is deleted" \
         "ls: $(ls -A "$T248" 2>&1)"
fi
if [ -f "$T248/classic-1.0.tar.gz" ]; then
    ok "the checked-in tarball survives"
else
    fail "the checked-in tarball survives" "ls: $(ls -A "$T248" 2>&1)"
fi
if [ ! -e "$T248/classic" ] && [ ! -e "$T248/.classic.projeny.status" ]; then
    ok "the classic checkout and status file are gone"
else
    fail "the classic checkout and status file are gone" \
         "ls: $(ls -A "$T248" 2>&1)"
fi
out="$(run_in "$T248" "$PROJENY" setup classic.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the classic setup after the erase exits 0"
else
    fail "the classic setup after the erase exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"set up 'classic' from 'classic-1.0.tar.gz'"*)
    ok "the next classic setup unpacks from the checked-in tarball"
    ;;
*)
    fail "the next classic setup unpacks from the checked-in tarball" \
         "out: $out"
    ;;
esac
ndl="$(printf '%s\n' "$out" | grep -c "downloading")"
if [ "$ndl" -eq 0 ]; then
    ok "the next classic setup downloads nothing"
else
    fail "the next classic setup downloads nothing" \
         "got $ndl downloading lines (out: $out)"
fi
if [ -f "$T248/.classic-1.0.tar.gz.snapshot" ]; then
    ok "the snapshot is recreated by the next setup"
else
    fail "the snapshot is recreated by the next setup" \
         "ls: $(ls -A "$T248" 2>&1)"
fi
if [ "$(cat "$T248/classic/README" 2>/dev/null)" = "hello v1.0" ]; then
    ok "the classic checkout is whole again"
else
    fail "the classic checkout is whole again" \
         "README: $(cat "$T248/classic/README" 2>&1)"
fi

# --------- 249. parallel erase-setup with one already-erased project
# `erase-setup a.projeny b.projeny c.projeny -j2` erases the three projects
# on the parallel machinery; the project whose checkout was already removed
# only warns about it, every status file goes, and the run exits 0. No
# --force is given: the check phase passes the batch because fakeb and
# fakec are fresh clean checkouts and fakea has no workdir — a missing
# checkout is clean by definition (there is nothing left to destroy), so
# the erase phase still warns and deletes the status file.
T249="$ROOT/t249"
mkdir -p "$T249"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/c.projeny" \
   "$T222/fake-1.0.tar.gz" "$T249/"
run_in "$T249" expect_ok "the parallel erase fixture sets up" "$PROJENY" \
    setup a.projeny b.projeny c.projeny
rm -rf "$T249/fakea"
out="$(run_in "$T249" "$PROJENY" erase-setup a.projeny b.projeny c.projeny -j2 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the parallel erase-setup with one missing checkout exits 0"
else
    fail "the parallel erase-setup with one missing checkout exits 0" \
         "exit=$rc out: $out"
fi
nwarn="$(printf '%s\n' "$out" | grep -c 'did not exist; nothing to erase')"
if [ "$nwarn" -eq 1 ]; then
    ok "the already-erased checkout warns exactly once"
else
    fail "the already-erased checkout warns exactly once" \
         "got $nwarn warnings (out: $out)"
fi
nerr="$(printf '%s\n' "$out" | grep -c 'error:')"
if [ "$nerr" -eq 0 ]; then
    ok "the parallel erase-setup prints no error"
else
    fail "the parallel erase-setup prints no error" \
         "got $nerr error lines (out: $out)"
fi
if [ ! -e "$T249/fakea" ] && [ ! -e "$T249/.a.projeny.status" ] && \
   [ ! -e "$T249/fakeb" ] && [ ! -e "$T249/.b.projeny.status" ] && \
   [ ! -e "$T249/fakec" ] && [ ! -e "$T249/.c.projeny.status" ]; then
    ok "all three projects' setup state is gone"
else
    fail "all three projects' setup state is gone" \
         "ls: $(ls -A "$T249" 2>&1)"
fi
nfail="$(printf '%s\n' "$out" | grep -c 'failed:')"
if [ "$nfail" -eq 0 ]; then
    ok "the all-success run has no summary line"
else
    fail "the all-success run has no summary line" "out: $out"
fi

# --------- 250. Name vs stem mismatch
# The checkout is named by the Name: header, not by the .projeny file's
# stem: `weird.projeny` saying `Name: realname` erases `realname/` and
# leaves a `weird/` directory (and anything else) alone.
T250="$ROOT/t250"
make_project "$T250" realname 1.0
mv "$T250/realname.projeny" "$T250/weird.projeny"
mkdir -p "$T250/weird"
printf 'keep me\n' > "$T250/weird/keepme"
run_in "$T250" expect_ok "the mismatched-stem fixture sets up" "$PROJENY" \
    setup weird.projeny
if [ -d "$T250/realname" ]; then
    ok "the mismatched-stem checkout is named by Name:"
else
    fail "the mismatched-stem checkout is named by Name:" \
         "ls: $(ls -A "$T250" 2>&1)"
fi
out="$(run_in "$T250" "$PROJENY" erase-setup weird.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the mismatched-stem erase-setup exits 0"
else
    fail "the mismatched-stem erase-setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'realname' (checkout 'realname', status '.weird.projeny.status')"*)
    ok "the note names the Name: checkout and the file's status"
    ;;
*)
    fail "the note names the Name: checkout and the file's status" \
         "out: $out"
    ;;
esac
if [ ! -e "$T250/realname" ] && [ -d "$T250/weird" ] && \
   [ "$(cat "$T250/weird/keepme" 2>/dev/null)" = "keep me" ]; then
    ok "the Name: checkout is erased and the stem-named directory survives"
else
    fail "the Name: checkout is erased and the stem-named directory survives" \
         "ls: $(ls -A "$T250" 2>&1)"
fi

# --------- 251. a failed deletion fails its project but not the rest
# The status path is made undeletable deterministically by putting a
# non-empty DIRECTORY there (unlink on a directory fails even for root).
# That project fails (exit 1, an error naming the path), its checkout is
# STILL erased ("try to finish the rest of the deletion"), the other
# project in the same invocation is fully erased, and the summary line
# lists the failed label. This is the --force path: the no-force check
# would refuse the whole invocation before erasing anything (the status
# file cannot even be READ through a directory — see the variant at the
# end of this section), and only --force reaches the deletions.
T251="$ROOT/t251"
mkdir -p "$T251"
cp "$T222/a.projeny" "$T222/c.projeny" "$T222/fake-1.0.tar.gz" "$T251/"
run_in "$T251" expect_ok "the failure fixture sets up" "$PROJENY" \
    setup a.projeny c.projeny
rm -f "$T251/.a.projeny.status"
mkdir "$T251/.a.projeny.status"
printf 'blocker\n' > "$T251/.a.projeny.status/inner"
out="$(run_in "$T251" "$PROJENY" erase-setup a.projeny c.projeny --force 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the erase-setup with an undeletable status path exits 1"
else
    fail "the erase-setup with an undeletable status path exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"error: cannot remove '"*".a.projeny.status': Is a directory"*)
    ok "the failed deletion prints an error naming the path"
    ;;
*)
    fail "the failed deletion prints an error naming the path" "out: $out"
    ;;
esac
if [ ! -e "$T251/fakea" ]; then
    ok "the failed project's checkout was still erased"
else
    fail "the failed project's checkout was still erased" \
         "ls: $(ls -A "$T251" 2>&1)"
fi
if [ -d "$T251/.a.projeny.status" ] && \
   [ -f "$T251/.a.projeny.status/inner" ]; then
    ok "the undeletable status directory is untouched"
else
    fail "the undeletable status directory is untouched" \
         "ls: $(ls -A "$T251/.a.projeny.status" 2>&1)"
fi
if [ ! -e "$T251/fakec" ] && [ ! -e "$T251/.c.projeny.status" ]; then
    ok "the healthy project was fully erased"
else
    fail "the healthy project was fully erased" "ls: $(ls -A "$T251" 2>&1)"
fi
case "$out" in
*"projeny: 1 of 2 erase-setup(s) failed: a.projeny"*)
    ok "the summary line lists the failed label"
    ;;
*)
    fail "the summary line lists the failed label" "out: $out"
    ;;
esac
# The no-force variant of the same fixture: without --force the check phase
# runs first, and it cannot even READ the status "file" (it is a directory),
# so the whole invocation refuses before erasing anything — the undeletable
# status directory, BOTH checkouts, and BOTH status files survive, and the
# refusal carries the canonical cannot-read report as a bullet.
T251N="$ROOT/t251-noforce"
mkdir -p "$T251N"
cp "$T222/a.projeny" "$T222/c.projeny" "$T222/fake-1.0.tar.gz" "$T251N/"
run_in "$T251N" expect_ok "the no-force failure fixture sets up" "$PROJENY" \
    setup a.projeny c.projeny
rm -f "$T251N/.a.projeny.status"
mkdir "$T251N/.a.projeny.status"
printf 'blocker\n' > "$T251N/.a.projeny.status/inner"
out="$(run_in "$T251N" "$PROJENY" erase-setup a.projeny c.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup over an unreadable status exits 1"
else
    fail "the no-force erase-setup over an unreadable status exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"cannot check 1 of 2 project(s) for uncommitted changes; refusing to " | \
*"erase anything (use --force to erase anyway)"*)
    ok "the no-force refusal names the check it could not run"
    ;;
*)
    fail "the no-force refusal names the check it could not run" "out: $out"
    ;;
esac
T251NP="$(cd "$T251N" && pwd -P)"
nread="$(printf '%s\n' "$out" | \
    grep -cF "  'a.projeny': cannot read file '$T251NP/.a.projeny.status': Is a directory")"
if [ "$nread" -eq 1 ]; then
    ok "the refusal bullet names the project and the unreadable status"
else
    fail "the refusal bullet names the project and the unreadable status" \
         "got $nread matching lines (out: $out)"
fi
if [ -d "$T251N/.a.projeny.status" ] && \
   [ -f "$T251N/.a.projeny.status/inner" ] && \
   [ -d "$T251N/fakea" ] && [ -d "$T251N/fakec" ] && \
   [ -f "$T251N/.c.projeny.status" ]; then
    ok "the no-force refusal erased nothing at all"
else
    fail "the no-force refusal erased nothing at all" \
         "ls: $(ls -A "$T251N" 2>&1)"
fi
if [ "$(printf '%s\n' "$out" | grep -c 'erased setup state for')" -eq 0 ]; then
    ok "the no-force refusal prints no erased-setup-state notes"
else
    fail "the no-force refusal prints no erased-setup-state notes" "out: $out"
fi

# --------- 252. an unparseable .projeny fails only its own project (--force)
# The .projeny file names what would be deleted, so a garbage file fails
# its own project with the canonical parse error (nothing erased for it)
# while the healthy projects erase. This is the --force path; without
# --force the unparseable project refuses the WHOLE invocation before
# anything is erased (see the variant at the end of this section).
T252="$ROOT/t252"
mkdir -p "$T252"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T252/"
printf 'this is not a projeny file\n' > "$T252/bad.projeny"
out="$(run_in "$T252" "$PROJENY" erase-setup a.projeny bad.projeny --force 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the erase-setup with an unparseable project exits 1"
else
    fail "the erase-setup with an unparseable project exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"projeny: [bad.projeny] error: file "*)
    ok "the unparseable project fails with the labeled canonical error"
    ;;
*)
    fail "the unparseable project fails with the labeled canonical error" \
         "out: $out"
    ;;
esac
case "$out" in
*"is missing a required header"*)
    ok "the failure is the canonical parse error"
    ;;
*)
    fail "the failure is the canonical parse error" "out: $out"
    ;;
esac
if [ ! -e "$T252/fakea" ] && [ ! -e "$T252/.a.projeny.status" ]; then
    ok "the healthy project was fully erased"
else
    fail "the healthy project was fully erased" "ls: $(ls -A "$T252" 2>&1)"
fi
if [ -f "$T252/bad.projeny" ]; then
    ok "the unparseable file itself is left alone"
else
    fail "the unparseable file itself is left alone" \
         "ls: $(ls -A "$T252" 2>&1)"
fi
case "$out" in
*"projeny: 1 of 2 erase-setup(s) failed: bad.projeny"*)
    ok "the summary line lists the unparseable project"
    ;;
*)
    fail "the summary line lists the unparseable project" "out: $out"
    ;;
esac
# The no-force variant of the same fixture: without --force the garbage
# .projeny cannot be assessed, so the WHOLE invocation refuses before
# erasing anything — the healthy project keeps its checkout and status file,
# the refusal headline counts the unassessable project, and its bullet is
# the canonical parse error (no labeled per-project report and no summary
# line: the erase phase never ran).
T252N="$ROOT/t252-noforce"
mkdir -p "$T252N"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T252N/"
printf 'this is not a projeny file\n' > "$T252N/bad.projeny"
run_in "$T252N" expect_ok "the no-force garbage fixture sets up" "$PROJENY" \
    setup a.projeny
out="$(run_in "$T252N" "$PROJENY" erase-setup a.projeny bad.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup with an unparseable project exits 1"
else
    fail "the no-force erase-setup with an unparseable project exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"cannot check 1 of 2 project(s) for uncommitted changes; refusing to " | \
*"erase anything (use --force to erase anyway)"*)
    ok "the no-force refusal counts the unassessable project"
    ;;
*)
    fail "the no-force refusal counts the unassessable project" "out: $out"
    ;;
esac
T252NP="$(cd "$T252N" && pwd -P)"
nbad="$(printf '%s\n' "$out" | \
    grep -cF "  'bad.projeny': file '$T252NP/bad.projeny' is missing a required header")"
if [ "$nbad" -eq 1 ]; then
    ok "the refusal bullet is the canonical parse error for bad.projeny"
else
    fail "the refusal bullet is the canonical parse error for bad.projeny" \
         "got $nbad matching lines (out: $out)"
fi
if [ -d "$T252N/fakea" ] && [ -f "$T252N/.a.projeny.status" ]; then
    ok "the healthy project's setup state survived the no-force refusal"
else
    fail "the healthy project's setup state survived the no-force refusal" \
         "ls: $(ls -A "$T252N" 2>&1)"
fi
if [ "$(printf '%s\n' "$out" | grep -c 'erased setup state for')" -eq 0 ]; then
    ok "the no-force garbage refusal prints no erased-setup-state notes"
else
    fail "the no-force garbage refusal prints no erased-setup-state notes" \
         "out: $out"
fi

# --------- 253. erase-setup options
# -j/--jobs in every spelling (anywhere among the arguments), the
# --erase-snapshots and --force flags in any position and combined, the
# rejections (-c/--curl-jobs has no meaning without a download phase,
# --erase-snapshots takes no value, --force is erase-setup-only and unknown
# for every other command, -j0 is refused, no project is a usage error),
# and duplicate arguments collapsing into one erase with a warning.
T253="$ROOT/t253"
optidx253=0
for optform in "-j1" "-j100" "--jobs=2" "--jobs 2"; do
    optidx253=$((optidx253 + 1))
    T253D="$ROOT/t253-$optidx253"
    mkdir -p "$T253D"
    cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T253D/"
    run_in "$T253D" expect_ok "erase-setup accepts '$optform'" "$PROJENY" \
        erase-setup $optform a.projeny
    if [ ! -e "$T253D/fakea" ] && [ ! -e "$T253D/.a.projeny.status" ]; then
        ok "erase-setup with '$optform' erased the project"
    else
        fail "erase-setup with '$optform' erased the project" \
             "ls: $(ls -A "$T253D" 2>&1)"
    fi
done
T253S="$ROOT/t253s"
mkdir -p "$T253S"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T253S/"
run_in "$T253S" expect_ok "the option-order fixture sets up" "$PROJENY" \
    setup a.projeny
run_in "$T253S" expect_ok "erase-setup accepts --erase-snapshots after the args" \
    "$PROJENY" erase-setup a.projeny --erase-snapshots
if [ ! -e "$T253S/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the flag after the args deleted the snapshot"
else
    fail "the flag after the args deleted the snapshot" \
         "ls: $(ls -A "$T253S" 2>&1)"
fi
# --force is erase-setup's second valueless flag: accepted anywhere among
# the arguments (like every erase-setup option), combinable with
# --erase-snapshots, and an unknown option for every other command.
T253F="$ROOT/t253-force"
mkdir -p "$T253F"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T253F/"
run_in "$T253F" expect_ok "the --force option fixture sets up" "$PROJENY" \
    setup a.projeny
run_in "$T253F" expect_ok "erase-setup accepts --force before the args" \
    "$PROJENY" erase-setup --force a.projeny
if [ ! -e "$T253F/fakea" ] && [ ! -e "$T253F/.a.projeny.status" ]; then
    ok "erase-setup with --force erased the clean project"
else
    fail "erase-setup with --force erased the clean project" \
         "ls: $(ls -A "$T253F" 2>&1)"
fi
run_in "$T253F" expect_ok "the --force + --erase-snapshots fixture sets up" \
    "$PROJENY" setup a.projeny
run_in "$T253F" expect_ok "erase-setup accepts --force with --erase-snapshots" \
    "$PROJENY" erase-setup a.projeny --force --erase-snapshots
if [ ! -e "$T253F/fakea" ] && [ ! -e "$T253F/.a.projeny.status" ] && \
   [ ! -e "$T253F/.fake-1.0.tar.gz.snapshot" ]; then
    ok "--force combines with --erase-snapshots"
else
    fail "--force combines with --erase-snapshots" \
         "ls: $(ls -A "$T253F" 2>&1)"
fi
run_in "$T253F" expect_ok "the --force rejection fixture sets up" "$PROJENY" \
    setup a.projeny
run_in "$T253F" expect_fail "--force is an unknown option for setup" \
    "$PROJENY" setup a.projeny --force
out="$(run_in "$T253F" "$PROJENY" setup a.projeny --force 2>&1)"
case "$out" in
*"unknown option '--force'"*)
    ok "the setup --force error names the option"
    ;;
*)
    fail "the setup --force error names the option" "out: $out"
    ;;
esac
if [ -d "$T253F/fakea" ]; then
    ok "the refused setup --force left the checkout alone"
else
    fail "the refused setup --force left the checkout alone" \
         "ls: $(ls -A "$T253F" 2>&1)"
fi
run_in "$T253F" expect_fail "--force is an unknown option for commit" \
    "$PROJENY" commit a.projeny --force
T253B="$ROOT/t253bad"
mkdir -p "$T253B"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T253B/"
run_in "$T253B" expect_fail "-c is an unknown option for erase-setup" \
    "$PROJENY" erase-setup -c2 a.projeny
out="$(run_in "$T253B" "$PROJENY" erase-setup -c2 a.projeny 2>&1)"
case "$out" in
*"unknown option '-c'"*)
    ok "the -c error names the option"
    ;;
*)
    fail "the -c error names the option" "out: $out"
    ;;
esac
run_in "$T253B" expect_fail "--curl-jobs is an unknown option for erase-setup" \
    "$PROJENY" erase-setup --curl-jobs 2 a.projeny
run_in "$T253B" expect_fail "--erase-snapshots with a value is an unknown option" \
    "$PROJENY" erase-setup --erase-snapshots=x a.projeny
run_in "$T253B" expect_fail "erase-setup refuses -j0" "$PROJENY" \
    erase-setup -j0 a.projeny
run_in "$T253B" expect_fail "erase-setup with no project is a usage error" \
    "$PROJENY" erase-setup
T253U="$ROOT/t253dup"
mkdir -p "$T253U"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T253U/"
run_in "$T253U" expect_ok "the duplicate fixture sets up" "$PROJENY" \
    setup a.projeny
out="$(run_in "$T253U" "$PROJENY" erase-setup a.projeny a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the duplicate-argument erase-setup exits 0"
else
    fail "the duplicate-argument erase-setup exits 0" "exit=$rc out: $out"
fi
case "$out" in
*"'a.projeny' is listed more than once; erasing it only once"*)
    ok "the duplicate argument warns and collapses"
    ;;
*)
    fail "the duplicate argument warns and collapses" "out: $out"
    ;;
esac
nnote="$(printf '%s\n' "$out" | grep -c 'erased setup state for')"
if [ "$nnote" -eq 1 ]; then
    ok "the collapsed erase runs the project once"
else
    fail "the collapsed erase runs the project once" \
         "got $nnote notes (out: $out)"
fi
if [ ! -e "$T253U/fakea" ] && [ ! -e "$T253U/.a.projeny.status" ]; then
    ok "the collapsed erase erased the checkout and status file"
else
    fail "the collapsed erase erased the checkout and status file" \
         "ls: $(ls -A "$T253U" 2>&1)"
fi

# --------- 254. the dir argument form
# The checkout directory argument resolves through the workdir-sibling rule
# (the directory sitting next to <dir>.projeny IS that project's workdir),
# and erases exactly the same things the file-name form does.
T254="$ROOT/t254"
make_project "$T254" match 1.0
run_in "$T254" expect_ok "the dir-argument fixture sets up" "$PROJENY" \
    setup match.projeny
out="$(run_in "$T254" "$PROJENY" erase-setup match 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "erase-setup accepts the checkout directory as the argument"
else
    fail "erase-setup accepts the checkout directory as the argument" \
         "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'match' (checkout 'match', status '.match.projeny.status')"*)
    ok "the dir-form erase reports the same note"
    ;;
*)
    fail "the dir-form erase reports the same note" "out: $out"
    ;;
esac
if [ ! -e "$T254/match" ] && [ ! -e "$T254/.match.projeny.status" ]; then
    ok "the dir-form erase removed the checkout and the status file"
else
    fail "the dir-form erase removed the checkout and the status file" \
         "ls: $(ls -A "$T254" 2>&1)"
fi

# --------- 255. the intra-pass retry re-renders progress from scratch
# When a package's first candidate URL delivers bytes and then dies
# mid-transfer, the scheduler starts its next candidate URL in the SAME
# pass (no retry pass). The re-render throttle's per-attempt baseline (the
# package's last rendered entry token, reset to the fresh attempt's own
# "0B" state) resets with the transfer state itself, so the new
# attempt re-renders from ITS OWN early percentages: the combined progress
# line must never keep the dead attempt's stale, higher-than-actual
# percentage on display while the retry is still young. The fixture runs
# `projeny download` with two URL HASH pairs sharing one archive basename —
# exactly one package whose candidate order is the pair order, so every
# progress line carries exactly one entry and percentages compare
# directly. URL 1 declares a 4 MiB Content-Length, drips 24 64 KiB chunks,
# then hangs up far short of the declared length (curl fails the transfer
# after real bytes arrived and rendered: a partial-file error); URL 2
# serves the real tarball, dripping its first chunks slowly so the retry's
# early whole-percents render far below the dead attempt's last one.
T255="$ROOT/t255"
mkdir -p "$T255"
mkdir -p "$T255/pd-1.0"
printf 'hello pd\n' > "$T255/pd-1.0/README"
dd if=/dev/urandom of="$T255/pd-1.0/blob" bs=65536 count=32 2>/dev/null
(cd "$T255" && tar -czf pd-1.0.tar.gz pd-1.0 && rm -rf pd-1.0)
h255="$("$PROJENY" hash "$T255/pd-1.0.tar.gz")"
cat > "$T255/serv.py" <<'PYEOF'
import socket, sys, time

# mode "die":  declare a 4 MiB Content-Length, drip 24 64 KiB chunks (one
#              per 60ms) of the real file, then close with the body far
#              short of the declared length: curl fails the transfer after
#              real, rendered bytes arrived (a partial-file error), which
#              is exactly the intra-pass-retry window this section pins.
# mode "slow": honest Content-Length; drip the first 6 64 KiB chunks (one
#              per 250ms) so the retry's early whole-percents render, then
#              send the rest at full speed.
mode, path, portfile = sys.argv[1], sys.argv[2], sys.argv[3]
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 0))
s.listen(16)
open(portfile, "w").write(str(s.getsockname()[1]))
while True:
    c, _ = s.accept()
    try:
        c.recv(65536)  # the GET; one connection per transfer
        with open(path, "rb") as f:
            body = f.read()
        if mode == "die":
            c.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 4194304\r\n"
                      b"Connection: close\r\n\r\n")
            for k in range(24):
                c.sendall(body[k * 65536:(k + 1) * 65536])
                time.sleep(0.06)
        else:
            c.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: "
                      + str(len(body)).encode() + b"\r\n"
                      b"Connection: close\r\n\r\n")
            for k in range(6):
                c.sendall(body[k * 65536:(k + 1) * 65536])
                time.sleep(0.25)
            c.sendall(body[6 * 65536:])
    except OSError:
        pass
    finally:
        c.close()
PYEOF
python3 "$T255/serv.py" die "$T255/pd-1.0.tar.gz" "$T255/port1" &
srv1=$!
python3 "$T255/serv.py" slow "$T255/pd-1.0.tar.gz" "$T255/port2" &
srv2=$!
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if [ -s "$T255/port1" ] && [ -s "$T255/port2" ]; then break; fi
    sleep 0.05
done
p255a="$(cat "$T255/port1" 2>/dev/null)"
p255b="$(cat "$T255/port2" 2>/dev/null)"
if [ -z "$p255a" ] || [ -z "$p255b" ]; then
    kill "$srv1" "$srv2" 2>/dev/null
    wait "$srv1" "$srv2" 2>/dev/null
    fail "the partial-file fixture servers start" "port file never appeared"
else
    T255D="$ROOT/t255-run"
    mkdir -p "$T255D"
    out="$(run_in "$T255D" "$PROJENY" download \
        "http://127.0.0.1:$p255a/pd-1.0.tar.gz" "$h255" \
        "http://127.0.0.1:$p255b/pd-1.0.tar.gz" "$h255" 2>&1)"
    rc=$?
    kill "$srv1" "$srv2" 2>/dev/null
    wait "$srv1" "$srv2" 2>/dev/null
    if [ $rc -eq 0 ]; then
        ok "the partial-file-then-good download exits 0"
    else
        fail "the partial-file-then-good download exits 0" \
             "exit=$rc out: $out"
    fi
    na="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'pd-1.0.tar.gz' from 'http://127.0.0.1:$p255a/")"
    if [ "$na" -eq 1 ]; then
        ok "the dying first URL announced exactly once"
    else
        fail "the dying first URL announced exactly once" \
             "got $na announcements (out: $out)"
    fi
    nb="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'pd-1.0.tar.gz' from 'http://127.0.0.1:$p255b/")"
    if [ "$nb" -eq 1 ]; then
        ok "the good second URL announced exactly once"
    else
        fail "the good second URL announced exactly once" \
             "got $nb announcements (out: $out)"
    fi
    # The failure warning follows \r-terminated progress lines on the same
    # output line, so the '\r's are turned into newlines before anchoring.
    nfail="$(printf '%s' "$out" | tr '\r' '\n' | \
        grep -c "^projeny: warning: failed to download 'pd-1.0.tar.gz' from 'http://127.0.0.1:$p255a/")"
    if [ "$nfail" -eq 1 ]; then
        ok "the mid-transfer death warned exactly once"
    else
        fail "the mid-transfer death warned exactly once" \
             "got $nfail warnings (out: $out)"
    fi
    nretry="$(printf '%s' "$out" | tr '\r' '\n' | grep -c "^projeny: retrying ")"
    if [ "$nretry" -eq 0 ]; then
        ok "the fallback to the second URL stayed inside one pass"
    else
        fail "the fallback to the second URL stayed inside one pass" \
             "got $nretry retrying lines (out: $out)"
    fi
    # The dead attempt must actually have rendered (the premise of the
    # pin): its in-flight percentages are the progress lines before the
    # failure warning; the retry's are those after it. One pass, one
    # failure, one package — so the boundary is exact.
    pstats="$(printf '%s' "$out" | tr '\r' '\n' | awk '
        /^projeny: download progress: [0-9]+%$/ {
            v = $0
            sub(/^projeny: download progress: /, "", v)
            sub(/%$/, "", v)
            if (dead) {
                if (minpost == "" || v + 0 < minpost)
                    minpost = v + 0
            } else {
                npre++
                if (v + 0 > maxpre + 0)
                    maxpre = v + 0
            }
            next
        }
        /failed to download/ { dead = 1 }
        END { print npre + 0, maxpre + 0, minpost + 0 }
    ')"
    npre="$(echo "$pstats" | cut -d' ' -f1)"
    maxpre="$(echo "$pstats" | cut -d' ' -f2)"
    minpost="$(echo "$pstats" | cut -d' ' -f3)"
    if [ "$npre" -ge 2 ]; then
        ok "the dead attempt rendered its progress ($npre lines, up to $maxpre%)"
    else
        fail "the dead attempt rendered its progress ($npre lines, up to $maxpre%)" \
             "the mid-transfer drip never rendered; cannot pin the retry"
    fi
    if [ "$minpost" -lt "$maxpre" ]; then
        ok "the retry re-rendered below the dead attempt's last percentage ($minpost% < $maxpre%)"
    else
        fail "the retry re-rendered below the dead attempt's last percentage ($minpost% < $maxpre%)" \
             "the retry's renders stayed gated by the dead attempt's baseline"
    fi
    nprog="$(progress_lines "$out" | wc -l)"
    nfmt="$(progress_lines "$out" | grep -Ec '^projeny: download progress: [0-9]+%$')"
    if [ "$nprog" -ge 1 ] && [ "$nfmt" -eq "$nprog" ]; then
        ok "every progress line is the prefix plus one whole-percent entry"
    else
        fail "every progress line is the prefix plus one whole-percent entry" \
             "$nfmt of $nprog lines matched (out: $out)"
    fi
    lastp="$(progress_lines "$out" | tail -1)"
    if [ "$lastp" = "projeny: download progress: 100%" ]; then
        ok "the round still closes at 100% for the verified package"
    else
        fail "the round still closes at 100% for the verified package" \
             "final line: $lastp"
    fi
    if cmp -s "$T255D/pd-1.0.tar.gz" "$T255/pd-1.0.tar.gz"; then
        ok "the verified bytes are the second URL's, byte-exact"
    else
        fail "the verified bytes are the second URL's, byte-exact" \
             "the written file differs from the fixture tarball"
    fi
fi

# --------- 256. erase-setup deletes the legacy undotted status file too
# Before the dot-prefixed bookkeeping names, projenies wrote the status as
# "<f>.projeny.status" (no leading dot) and the snapshot as
# "<archive>.snapshot". resolve_ctx migrates a legacy status on first use,
# and erase-setup deletes BOTH forms of the status: the canonical dotted
# one and the legacy undotted one next to it, with no missing-item warning
# when both exist. Without --erase-snapshots the snapshots stay — the
# legacy undotted form included; with it, both snapshot forms go and the
# next setup re-downloads.
T256="$ROOT/t256"
mkdir -p "$T256"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T256/"
run_in "$T256" expect_ok "the legacy-status fixture sets up" "$PROJENY" \
    setup a.projeny
if [ -f "$T256/.a.projeny.status" ] && \
   [ -f "$T256/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the fixture has the dotted status and snapshot"
else
    fail "the fixture has the dotted status and snapshot" \
         "ls: $(ls -A "$T256" 2>&1)"
fi
cp "$T256/.a.projeny.status" "$T256/a.projeny.status"
cp "$T256/.fake-1.0.tar.gz.snapshot" "$T256/fake-1.0.tar.gz.snapshot"
out="$(run_in "$T256" "$PROJENY" erase-setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the erase-setup over both status forms exits 0"
else
    fail "the erase-setup over both status forms exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'fakea' (checkout 'fakea', status '.a.projeny.status', status 'a.projeny.status')"*)
    ok "the note names the dotted and the legacy status file"
    ;;
*)
    fail "the note names the dotted and the legacy status file" "out: $out"
    ;;
esac
if [ ! -e "$T256/.a.projeny.status" ] && \
   [ ! -e "$T256/a.projeny.status" ]; then
    ok "both status forms are gone"
else
    fail "both status forms are gone" "ls: $(ls -A "$T256" 2>&1)"
fi
if [ -f "$T256/.fake-1.0.tar.gz.snapshot" ] && \
   [ -f "$T256/fake-1.0.tar.gz.snapshot" ]; then
    ok "without --erase-snapshots both snapshot forms survive"
else
    fail "without --erase-snapshots both snapshot forms survive" \
         "ls: $(ls -A "$T256" 2>&1)"
fi
if [ ! -e "$T256/fakea" ]; then
    ok "the checkout is gone"
else
    fail "the checkout is gone" "ls: $(ls -A "$T256" 2>&1)"
fi
nmiss="$(printf '%s\n' "$out" | grep -c 'did not exist')"
if [ "$nmiss" -eq 0 ]; then
    ok "the both-forms-present erase warns about nothing missing"
else
    fail "the both-forms-present erase warns about nothing missing" \
         "got $nmiss warnings (out: $out)"
fi
T256S="$ROOT/t256-snap"
mkdir -p "$T256S"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/fake-1.0.tar.gz" "$T256S/"
run_in "$T256S" expect_ok "the legacy-snapshot fixture sets up" "$PROJENY" \
    setup a.projeny
if [ -f "$T256S/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the snapshot fixture has a dotted snapshot"
else
    fail "the snapshot fixture has a dotted snapshot" \
         "ls: $(ls -A "$T256S" 2>&1)"
fi
cp "$T256S/.a.projeny.status" "$T256S/a.projeny.status"
cp "$T256S/.fake-1.0.tar.gz.snapshot" "$T256S/fake-1.0.tar.gz.snapshot"
out="$(run_in "$T256S" "$PROJENY" erase-setup a.projeny --erase-snapshots 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the --erase-snapshots erase over both forms exits 0"
else
    fail "the --erase-snapshots erase over both forms exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'fakea' (checkout 'fakea', status '.a.projeny.status', status 'a.projeny.status', snapshot '.fake-1.0.tar.gz.snapshot', snapshot 'fake-1.0.tar.gz.snapshot')"*)
    ok "the note names both snapshot forms as erased"
    ;;
*)
    fail "the note names both snapshot forms as erased" "out: $out"
    ;;
esac
if [ ! -e "$T256S/.fake-1.0.tar.gz.snapshot" ] && \
   [ ! -e "$T256S/fake-1.0.tar.gz.snapshot" ]; then
    ok "both snapshot forms are gone with --erase-snapshots"
else
    fail "both snapshot forms are gone with --erase-snapshots" \
         "ls: $(ls -A "$T256S" 2>&1)"
fi
if [ ! -e "$T256S/.a.projeny.status" ] && \
   [ ! -e "$T256S/a.projeny.status" ]; then
    ok "both status forms are gone with --erase-snapshots too"
else
    fail "both status forms are gone with --erase-snapshots too" \
         "ls: $(ls -A "$T256S" 2>&1)"
fi
nmiss="$(printf '%s\n' "$out" | grep -c 'did not exist')"
if [ "$nmiss" -eq 0 ]; then
    ok "the snapshot erase also warns about nothing missing"
else
    fail "the snapshot erase also warns about nothing missing" \
         "got $nmiss warnings (out: $out)"
fi
nerr="$(printf '%s\n' "$out" | grep -c 'error:')"
if [ "$nerr" -eq 0 ]; then
    ok "the snapshot erase prints no error"
else
    fail "the snapshot erase prints no error" \
         "got $nerr error lines (out: $out)"
fi
out="$(run_in "$T256S" "$PROJENY" setup a.projeny b.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the setup after the snapshot erase exits 0"
else
    fail "the setup after the snapshot erase exits 0" "exit=$rc out: $out"
fi
ndl="$(printf '%s\n' "$out" | grep -c "^projeny: downloading 'fake-1.0.tar.gz' from '")"
if [ "$ndl" -eq 1 ]; then
    ok "the next setup re-downloads the erased archive (once, shared)"
else
    fail "the next setup re-downloads the erased archive (once, shared)" \
         "got $ndl downloading lines (out: $out)"
fi
if [ -f "$T256S/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the next setup recreated the dotted snapshot"
else
    fail "the next setup recreated the dotted snapshot" \
         "ls: $(ls -A "$T256S" 2>&1)"
fi
if [ "$(cat "$T256S/fakea/README" 2>/dev/null)" = "hello v1" ] && \
   [ "$(cat "$T256S/fakeb/README" 2>/dev/null)" = "hello v1" ]; then
    ok "both projects checked out from the re-download"
else
    fail "both projects checked out from the re-download" \
         "ls: $(ls -A "$T256S" 2>&1)"
fi

# --------- 257. a nonexistent .projeny argument erases nothing
# The .projeny file names what would be deleted, so one that cannot be
# read fails its own project — the canonical "cannot read" error, labeled —
# BEFORE anything is deleted: a checkout directory and status files a real
# project of that name would have had survive untouched, the run exits 1
# after the rest of the invocation finished, and the summary lists only the
# bad label. These are the --force runs (see the no-force variants at the
# end of this section: without --force the whole invocation refuses before
# the erase phase runs, so there is no per-project summary line at all).
# (The genuinely-unreadable-file variant of the same message is
# not portable here: this suite runs as root, and root reads chmod 000
# files, so only the missing-file spelling is exercised.)
T257="$ROOT/t257"
mkdir -p "$T257/ghost"
printf 'keep me\n' > "$T257/ghost/keepme"
printf 'pre-existing\n' > "$T257/.ghost.projeny.status"
printf 'pre-existing\n' > "$T257/ghost.projeny.status"
out="$(run_in "$T257" "$PROJENY" erase-setup ghost.projeny --force 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the erase-setup of a nonexistent project exits 1"
else
    fail "the erase-setup of a nonexistent project exits 1" \
         "exit=$rc out: $out"
fi
# absolutize spells the path with the process's working directory, which is
# the physical one: derive the expected prefix the same way.
T257P="$(cd "$T257" && pwd -P)"
nread="$(printf '%s\n' "$out" | \
    grep -cF "cannot read '$T257P/ghost.projeny' (missing?); refusing to erase anything for it (the .projeny file names what would be deleted)")"
if [ "$nread" -eq 1 ]; then
    ok "the error pins the exact cannot-read wording with the abs path"
else
    fail "the error pins the exact cannot-read wording with the abs path" \
         "got $nread matching lines (out: $out)"
fi
case "$out" in
*"projeny: [ghost.projeny] error: cannot read '"*)
    ok "the error is labeled with the project"
    ;;
*)
    fail "the error is labeled with the project" "out: $out"
    ;;
esac
if [ -d "$T257/ghost" ] && \
   [ "$(cat "$T257/ghost/keepme" 2>/dev/null)" = "keep me" ]; then
    ok "the checkout the project would have had survives"
else
    fail "the checkout the project would have had survives" \
         "ls: $(ls -A "$T257/ghost" 2>&1)"
fi
if [ "$(cat "$T257/.ghost.projeny.status" 2>/dev/null)" = "pre-existing" ] && \
   [ "$(cat "$T257/ghost.projeny.status" 2>/dev/null)" = "pre-existing" ]; then
    ok "both status files the project would have had survive"
else
    fail "both status files the project would have had survive" \
         "ls: $(ls -A "$T257" 2>&1)"
fi
case "$out" in
*"projeny: 1 of 1 erase-setup(s) failed: ghost.projeny"*)
    ok "the summary line lists the one bad label"
    ;;
*)
    fail "the summary line lists the one bad label" "out: $out"
    ;;
esac
if [ ! -e "$T257/ghost.projeny" ]; then
    ok "the failed run created nothing"
else
    fail "the failed run created nothing" "ls: $(ls -A "$T257" 2>&1)"
fi
T257M="$ROOT/t257-mix"
mkdir -p "$T257M"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T257M/"
run_in "$T257M" expect_ok "the mixed fixture sets up" "$PROJENY" \
    setup a.projeny
mkdir -p "$T257M/ghost"
printf 'keep me\n' > "$T257M/ghost/keepme"
printf 'pre-existing\n' > "$T257M/.ghost.projeny.status"
T257MP="$(cd "$T257M" && pwd -P)"
out="$(run_in "$T257M" "$PROJENY" erase-setup ghost.projeny a.projeny --force 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the mixed erase-setup with one bad path exits 1"
else
    fail "the mixed erase-setup with one bad path exits 1" \
         "exit=$rc out: $out"
fi
if [ ! -e "$T257M/fakea" ] && [ ! -e "$T257M/.a.projeny.status" ]; then
    ok "the healthy project was still fully erased"
else
    fail "the healthy project was still fully erased" \
         "ls: $(ls -A "$T257M" 2>&1)"
fi
if [ -d "$T257M/ghost" ] && \
   [ -f "$T257M/.ghost.projeny.status" ] && \
   [ "$(cat "$T257M/ghost/keepme" 2>/dev/null)" = "keep me" ]; then
    ok "the bad project's would-be state survives the mixed run"
else
    fail "the bad project's would-be state survives the mixed run" \
         "ls: $(ls -A "$T257M" 2>&1)"
fi
case "$out" in
*"projeny: 1 of 2 erase-setup(s) failed: ghost.projeny"*)
    ok "the mixed summary lists only the bad label"
    ;;
*)
    fail "the mixed summary lists only the bad label" "out: $out"
    ;;
esac
nread="$(printf '%s\n' "$out" | \
    grep -cF "cannot read '$T257MP/ghost.projeny' (missing?); refusing to erase anything for it (the .projeny file names what would be deleted)")"
if [ "$nread" -eq 1 ]; then
    ok "the mixed run reports the bad path once"
else
    fail "the mixed run reports the bad path once" \
         "got $nread matching lines (out: $out)"
fi
# The no-force variants: a .projeny that cannot be read cannot be assessed
# for uncommitted changes, so without --force the WHOLE invocation refuses
# before the erase phase runs — one unlabeled die report (no per-project
# labels, no summary line), nothing erased anywhere.
T257N="$ROOT/t257-noforce"
mkdir -p "$T257N/ghost"
printf 'keep me\n' > "$T257N/ghost/keepme"
printf 'pre-existing\n' > "$T257N/.ghost.projeny.status"
out="$(run_in "$T257N" "$PROJENY" erase-setup ghost.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup of a nonexistent project exits 1"
else
    fail "the no-force erase-setup of a nonexistent project exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"cannot check 1 of 1 project(s) for uncommitted changes; refusing to " | \
*"erase anything (use --force to erase anyway)"*)
    ok "the no-force nonexistent-project refusal is the whole report"
    ;;
*)
    fail "the no-force nonexistent-project refusal is the whole report" \
         "out: $out"
    ;;
esac
T257NP="$(cd "$T257N" && pwd -P)"
nread="$(printf '%s\n' "$out" | \
    grep -cF "  'ghost.projeny': cannot read '$T257NP/ghost.projeny' (missing?); refusing to erase anything for it (the .projeny file names what would be deleted)")"
if [ "$nread" -eq 1 ]; then
    ok "the no-force refusal bullet is the canonical cannot-read error"
else
    fail "the no-force refusal bullet is the canonical cannot-read error" \
         "got $nread matching lines (out: $out)"
fi
if [ -d "$T257N/ghost" ] && \
   [ "$(cat "$T257N/.ghost.projeny.status" 2>/dev/null)" = "pre-existing" ] && \
   [ "$(printf '%s\n' "$out" | grep -c 'erase-setup(s) failed')" -eq 0 ]; then
    ok "the no-force nonexistent-project refusal erased nothing and has no summary"
else
    fail "the no-force nonexistent-project refusal erased nothing and has no summary" \
         "ls: $(ls -A "$T257N" 2>&1) out: $out"
fi
T257NM="$ROOT/t257-noforce-mix"
mkdir -p "$T257NM"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T257NM/"
run_in "$T257NM" expect_ok "the no-force mixed fixture sets up" "$PROJENY" \
    setup a.projeny
mkdir -p "$T257NM/ghost"
printf 'pre-existing\n' > "$T257NM/.ghost.projeny.status"
out="$(run_in "$T257NM" "$PROJENY" erase-setup ghost.projeny a.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force mixed erase-setup with one bad path exits 1"
else
    fail "the no-force mixed erase-setup with one bad path exits 1" \
         "exit=$rc out: $out"
fi
if [ -d "$T257NM/fakea" ] && [ -f "$T257NM/.a.projeny.status" ] && \
   [ -f "$T257NM/.ghost.projeny.status" ]; then
    ok "the no-force mixed run erased nothing, healthy project included"
else
    fail "the no-force mixed run erased nothing, healthy project included" \
         "ls: $(ls -A "$T257NM" 2>&1)"
fi
case "$out" in
*"cannot check 1 of 2 project(s) for uncommitted changes; refusing to " | \
*"erase anything (use --force to erase anyway)"*)
    ok "the no-force mixed refusal counts the bad project of two"
    ;;
*)
    fail "the no-force mixed refusal counts the bad project of two" \
         "out: $out"
    ;;
esac

# --------- 258. combined progress line: unknown-total and known-total together
# The batch line's two entry forms side by side in ONE roster: a package
# whose server sends NO Content-Length (its entry is the compact byte count
# of what has arrived so far) and a package whose server declares an honest
# Content-Length and drips slowly (its entry moves through whole percents).
# Both are announced up front (two packages, default curl jobs), so the
# roster is [unknown, known] — first-announcement order, which here is the
# .projeny argument order — on every line from the first render through the
# closing line: every in-flight line carries BOTH entries in that order
# ("<bytes> <pct>"), the byte tokens grow, the percents climb, and no "?"
# appears anywhere in the progress output.
T258="$ROOT/t258"
mkdir -p "$T258"
for n258 in uk wk; do
    mkdir -p "$T258/$n258-1.0"
    printf 'hello %s\n' "$n258" > "$T258/$n258-1.0/README"
    dd if=/dev/urandom of="$T258/$n258-1.0/blob" bs=65536 count=32 2>/dev/null
    (cd "$T258" && tar -czf "$n258-1.0.tar.gz" "$n258-1.0" && rm -rf "$n258-1.0")
done
hu258="$("$PROJENY" hash "$T258/uk-1.0.tar.gz")"
hw258="$("$PROJENY" hash "$T258/wk-1.0.tar.gz")"
cat > "$T258/serv.py" <<'PYEOF'
import socket, sys, time

# mode "unknown": NO Content-Length; drip the whole file one 64 KiB chunk
#                per 50ms, so the transfer's total stays unknown and the
#                download stays in flight across many scheduler iterations.
# mode "slow":   honest Content-Length; drip the first 6 64 KiB chunks (one
#                per 250ms) so early whole-percents render, then send the
#                rest at full speed.
mode, path, portfile = sys.argv[1], sys.argv[2], sys.argv[3]
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 0))
s.listen(16)
open(portfile, "w").write(str(s.getsockname()[1]))
while True:
    c, _ = s.accept()
    try:
        c.recv(65536)  # the GET; one connection per transfer
        with open(path, "rb") as f:
            body = f.read()
        if mode == "unknown":
            c.sendall(b"HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n")
            pos = 0
            while pos < len(body):
                c.sendall(body[pos:pos + 65536])
                pos += 65536
                time.sleep(0.05)
        else:
            c.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: "
                      + str(len(body)).encode() + b"\r\n"
                      b"Connection: close\r\n\r\n")
            for k in range(6):
                c.sendall(body[k * 65536:(k + 1) * 65536])
                time.sleep(0.25)
            c.sendall(body[6 * 65536:])
    except OSError:
        pass
    finally:
        c.close()
PYEOF
python3 "$T258/serv.py" unknown "$T258/uk-1.0.tar.gz" "$T258/port1" &
srv1=$!
python3 "$T258/serv.py" slow "$T258/wk-1.0.tar.gz" "$T258/port2" &
srv2=$!
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if [ -s "$T258/port1" ] && [ -s "$T258/port2" ]; then break; fi
    sleep 0.05
done
p258u="$(cat "$T258/port1" 2>/dev/null)"
p258k="$(cat "$T258/port2" 2>/dev/null)"
if [ -z "$p258u" ] || [ -z "$p258k" ]; then
    kill "$srv1" "$srv2" 2>/dev/null
    wait "$srv1" "$srv2" 2>/dev/null
    fail "the mixed-total fixture servers start" "port file never appeared"
else
    T258D="$ROOT/t258-run"
    mkdir -p "$T258D"
    printf 'URL: http://127.0.0.1:%s/uk-1.0.tar.gz %s\nOrigname: uk-1.0\nName: fakeuk\n\n    Unknown total.\n\n' \
        "$p258u" "$hu258" > "$T258D/unknown.projeny"
    printf 'URL: http://127.0.0.1:%s/wk-1.0.tar.gz %s\nOrigname: wk-1.0\nName: fakewk\n\n    Known total.\n\n' \
        "$p258k" "$hw258" > "$T258D/known.projeny"
    out="$(run_in "$T258D" "$PROJENY" setup unknown.projeny known.projeny 2>&1)"
    rc=$?
    kill "$srv1" "$srv2" 2>/dev/null
    wait "$srv1" "$srv2" 2>/dev/null
    if [ $rc -eq 0 ]; then
        ok "the mixed-total batch setup exits 0"
    else
        fail "the mixed-total batch setup exits 0" "exit=$rc out: $out"
    fi
    # The unknown-total package is the first argument, so its announcement
    # — and therefore its roster position — is first.
    first="$(printf '%s\n' "$out" | grep "^projeny: downloading '.*' from '" | head -1)"
    case "$first" in
    *"from 'http://127.0.0.1:$p258u/"*)
        ok "the unknown-total package is announced (and rostered) first"
        ;;
    *)
        fail "the unknown-total package is announced (and rostered) first" \
             "first announcement: $first"
        ;;
    esac
    nprog="$(progress_lines "$out" | wc -l)"
    if [ "$nprog" -ge 3 ]; then
        ok "the mixed-total batch reports several progress lines ($nprog)"
    else
        fail "the mixed-total batch reports several progress lines" \
             "got $nprog progress lines (out: $out)"
    fi
    nfmt="$(progress_lines "$out" | grep -Ec "^projeny: download progress: ($ptok)( ($ptok))*$")"
    if [ "$nfmt" -eq "$nprog" ]; then
        ok "every mixed-total progress line is well-formed"
    else
        fail "every mixed-total progress line is well-formed" \
             "$nfmt of $nprog lines well-formed (out: $out)"
    fi
    # Every in-flight line carries BOTH roster entries in announcement
    # order: the unknown-total transfer's byte token, then the
    # known-total transfer's whole-percent. The FIRST in-flight render is
    # exempt: the known-total transfer's total arrives with its response
    # headers, and under load that first progress tick can land after the
    # first render (whose other entry already changed), so that one line
    # can honestly read "0B" for the known-total package.
    ninf="$(progress_lines "$out" | head -n -1 | tail -n +2 | wc -l)"
    nboth="$(progress_lines "$out" | head -n -1 | tail -n +2 | \
            grep -Ec '^projeny: download progress: ([0-9]+(\.[0-9])?(B|KiB|MiB|GiB)) ([0-9]+%)$')"
    if [ "$ninf" -ge 2 ] && [ "$nboth" -eq "$ninf" ]; then
        ok "every in-flight line carries both entries in announcement order"
    else
        fail "every in-flight line carries both entries in announcement order" \
             "$nboth of $ninf in-flight lines were '<bytes> <pct>' (out: $out)"
    fi
    ndtok="$(progress_lines "$out" | head -n -1 | \
            sed 's/^projeny: download progress: //; s/ .*$//' | sort -u | wc -l)"
    if [ "$ndtok" -ge 2 ]; then
        ok "the unknown-total entry grows through byte tokens ($ndtok distinct)"
    else
        fail "the unknown-total entry grows through byte tokens" \
             "only $ndtok distinct byte token(s) (out: $out)"
    fi
    npct="$(progress_lines "$out" | head -n -1 | \
           sed 's/^projeny: download progress: .* //; s/%$//' | sort -n -u | wc -l)"
    if [ "$npct" -ge 2 ]; then
        ok "the known-total entry moves through percents ($npct distinct)"
    else
        fail "the known-total entry moves through percents" \
             "only $npct distinct percent value(s) (out: $out)"
    fi
    if progress_lines "$out" | grep -q '?'; then
        fail "no '?' appears anywhere in the mixed-total progress output" \
             "a progress line carried a '?' (out: $out)"
    else
        ok "no '?' appears anywhere in the mixed-total progress output"
    fi
    lastp="$(progress_lines "$out" | tail -1)"
    if [ "$lastp" = "projeny: download progress: 100% 100%" ]; then
        ok "the mixed-total round closes at 100% per package"
    else
        fail "the mixed-total round closes at 100% per package" \
             "final line: $lastp"
    fi
    if cmp -s "$T258D/.uk-1.0.tar.gz.snapshot" "$T258/uk-1.0.tar.gz" && \
       cmp -s "$T258D/.wk-1.0.tar.gz.snapshot" "$T258/wk-1.0.tar.gz"; then
        ok "both mixed-total downloads are byte-exact"
    else
        fail "both mixed-total downloads are byte-exact" \
             "a snapshot differs from its served file"
    fi
fi

# --------- 259. without --force, erase-setup refuses a modified checkout
# erase-setup destroys the checkout, so without --force it first checks —
# in parallel, subject to -j — that every project's status reports nothing
# a commit would fold in. A modified tracked file is exactly such a change:
# the whole invocation refuses with one error naming the project and the
# file, and NOTHING is erased (workdir, status file, and snapshot all
# stay). A tracked file deleted by hand (never `projeny rm`-ed) shows up as
# Disappeared: in status and refuses the same way.
T259="$ROOT/t259"
mkdir -p "$T259"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T259/"
run_in "$T259" expect_ok "the modified-checkout fixture sets up" "$PROJENY" \
    setup a.projeny
printf 'an uncommitted edit\n' >> "$T259/fakea/README"
out="$(run_in "$T259" "$PROJENY" erase-setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup of a modified checkout exits 1"
else
    fail "the no-force erase-setup of a modified checkout exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"projeny: error: refusing to erase 1 of 1 project(s) with uncommitted " | \
*"changes (use --force to erase anyway)"*)
    ok "the refusal is one die-style error with the --force hint"
    ;;
*)
    fail "the refusal is one die-style error with the --force hint" \
         "out: $out"
    ;;
esac
case "$out" in
*"'a.projeny': modified: 'README'"*)
    ok "the refusal bullet names the project and the modified file"
    ;;
*)
    fail "the refusal bullet names the project and the modified file" \
         "out: $out"
    ;;
esac
if [ -d "$T259/fakea" ] && \
   [ -f "$T259/.a.projeny.status" ] && \
   [ -f "$T259/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the refused run left the workdir, status file, and snapshot alone"
else
    fail "the refused run left the workdir, status file, and snapshot alone" \
         "ls: $(ls -A "$T259" 2>&1)"
fi
if grep -q "an uncommitted edit" "$T259/fakea/README" 2>/dev/null; then
    ok "the uncommitted edit itself is untouched by the refusal"
else
    fail "the uncommitted edit itself is untouched by the refusal" \
         "README: $(cat "$T259/fakea/README" 2>&1)"
fi
if [ "$(printf '%s\n' "$out" | grep -c 'erased setup state for')" -eq 0 ]; then
    ok "the refusal prints no erased-setup-state notes"
else
    fail "the refusal prints no erased-setup-state notes" "out: $out"
fi
# Same fixture, now with the README restored and a tracked file deleted by
# hand: status reports Disappeared:, which is a change a commit would fold
# in (the deletion would land in the patch), so the refusal names it.
printf 'hello v1\n' > "$T259/fakea/README"
rm "$T259/fakea/src/a.c"
out="$(run_in "$T259" "$PROJENY" erase-setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup of a disappeared-file checkout exits 1"
else
    fail "the no-force erase-setup of a disappeared-file checkout exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"'a.projeny': disappeared: 'src/a.c'"*)
    ok "the refusal bullet names the disappeared file"
    ;;
*)
    fail "the refusal bullet names the disappeared file" "out: $out"
    ;;
esac
if [ -d "$T259/fakea" ] && [ -f "$T259/.a.projeny.status" ]; then
    ok "the disappeared-file refusal still erased nothing"
else
    fail "the disappeared-file refusal still erased nothing" \
         "ls: $(ls -A "$T259" 2>&1)"
fi

# --------- 260. untracked files alone do not block a no-force erase-setup
# The check's definition of dirty is "status reports anything other than
# untracked files", so a checkout whose only difference from the expected
# tree is untracked files (never added, never committed) is erased happily
# without --force — uncommitted untracked content goes with the checkout,
# exactly as the erasure always has.
T260="$ROOT/t260"
mkdir -p "$T260"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T260/"
run_in "$T260" expect_ok "the untracked fixture sets up" "$PROJENY" \
    setup a.projeny
printf 'scratch notes\n' > "$T260/fakea/notes.txt"
mkdir -p "$T260/fakea/scratch/deep"
printf 'deeper\n' > "$T260/fakea/scratch/deep/blob"
out="$(run_in "$T260" "$PROJENY" status a.projeny 2>&1)"
case "$out" in
*"Untracked: notes.txt"*)
    ok "status reports the untracked file"
    ;;
*)
    fail "status reports the untracked file" "out: $out"
    ;;
esac
out="$(run_in "$T260" "$PROJENY" erase-setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the untracked-only no-force erase-setup exits 0"
else
    fail "the untracked-only no-force erase-setup exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'fakea' (checkout 'fakea', status '.a.projeny.status')"*)
    ok "the untracked-only erase reports the same note as always"
    ;;
*)
    fail "the untracked-only erase reports the same note as always" \
         "out: $out"
    ;;
esac
if [ ! -e "$T260/fakea" ] && [ ! -e "$T260/.a.projeny.status" ]; then
    ok "the untracked-only erase removed the checkout and status file"
else
    fail "the untracked-only erase removed the checkout and status file" \
         "ls: $(ls -A "$T260" 2>&1)"
fi

# --------- 261. pending add/rm/mv refuse a no-force erase-setup
# Pending operations are recorded in the status file the moment `projeny
# add`, `rm`, or `mv` runs — a commit would fold each of them into the
# patch, so each makes the project dirty and the no-force check refuses.
# Every refusal leaves the workdir and status file in place; --force erases
# all three.
T261="$ROOT/t261"
mkdir -p "$T261"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/c.projeny" \
   "$T222/fake-1.0.tar.gz" "$T261/"
run_in "$T261" expect_ok "the pending-ops fixture sets up" "$PROJENY" \
    setup a.projeny b.projeny c.projeny
printf 'not yet committed\n' > "$T261/fakea/notes.txt"
run_in "$T261" expect_ok "the fixture marks a file as added" "$PROJENY" \
    add a.projeny fakea/notes.txt
run_in "$T261" expect_ok "the fixture marks a file as removed" "$PROJENY" \
    rm b.projeny fakeb/README
run_in "$T261" expect_ok "the fixture marks a file as renamed" "$PROJENY" \
    mv c.projeny fakec/src/a.c fakec/src/renamed.c
for name in a b c; do
    out="$(run_in "$T261" "$PROJENY" erase-setup $name.projeny 2>&1)"
    rc=$?
    if [ $rc -eq 1 ]; then
        ok "the no-force erase-setup of the pending-$name project exits 1"
    else
        fail "the no-force erase-setup of the pending-$name project exits 1" \
             "exit=$rc out: $out"
    fi
    if [ -d "$T261/fake$name" ] && [ -f "$T261/.$name.projeny.status" ]; then
        ok "the pending-$name refusal leaves the workdir and status file"
    else
        fail "the pending-$name refusal leaves the workdir and status file" \
             "ls: $(ls -A "$T261" 2>&1)"
    fi
    if [ "$(printf '%s\n' "$out" | grep -c 'erased setup state for')" -eq 0 ]
    then
        ok "the pending-$name refusal erases nothing"
    else
        fail "the pending-$name refusal erases nothing" "out: $out"
    fi
done
out="$(run_in "$T261" "$PROJENY" erase-setup a.projeny 2>&1)"
case "$out" in
*"'a.projeny': added: 'notes.txt'"*)
    ok "the pending-add refusal names the added file"
    ;;
*)
    fail "the pending-add refusal names the added file" "out: $out"
    ;;
esac
out="$(run_in "$T261" "$PROJENY" erase-setup b.projeny 2>&1)"
case "$out" in
*"'b.projeny': removed: 'README'"*)
    ok "the pending-rm refusal names the removed file"
    ;;
*)
    fail "the pending-rm refusal names the removed file" "out: $out"
    ;;
esac
out="$(run_in "$T261" "$PROJENY" erase-setup c.projeny 2>&1)"
case "$out" in
*"'c.projeny': renamed: 'src/a.c' -> 'src/renamed.c'"*)
    ok "the pending-mv refusal names both sides of the rename"
    ;;
*)
    fail "the pending-mv refusal names both sides of the rename" "out: $out"
    ;;
esac
out="$(run_in "$T261" "$PROJENY" erase-setup a.projeny b.projeny c.projeny \
    --force 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the --force erase of the three pending-op projects exits 0"
else
    fail "the --force erase of the three pending-op projects exits 0" \
         "exit=$rc out: $out"
fi
nexpect="$(printf '%s\n' "$out" | grep -c 'erased setup state for')"
if [ "$nexpect" -eq 3 ]; then
    ok "--force erased all three pending-op projects"
else
    fail "--force erased all three pending-op projects" \
         "got $nexpect notes (out: $out)"
fi
if [ ! -e "$T261/fakea" ] && [ ! -e "$T261/fakeb" ] && \
   [ ! -e "$T261/fakec" ] && [ ! -e "$T261/.a.projeny.status" ] && \
   [ ! -e "$T261/.b.projeny.status" ] && \
   [ ! -e "$T261/.c.projeny.status" ]; then
    ok "--force left no setup state behind"
else
    fail "--force left no setup state behind" "ls: $(ls -A "$T261" 2>&1)"
fi

# --------- 262. a conflicted checkout refuses a no-force erase-setup
# Conflicts are recorded as Conflict: entries in the status file until
# `resolve`d, and a commit is refused while any remain — so the no-force
# check treats them as work a commit would have to finish. The conflict is
# produced by the same dance section 4 uses: a local edit committed and
# re-set-up, an uncommitted edit on top, and an upstream commit changing
# the same region.
T262="$ROOT/t262"
make_tarballs "$T262" fake
write_projeny "$T262" fake 1.0 fake
run_in "$T262" expect_ok "the conflict fixture sets up" "$PROJENY" \
    setup fake.projeny
sed -i 's/^int beta = 1;$/int beta = 10;/' "$T262/fake/src/a.c"
run_in "$T262" expect_ok "the conflict fixture commits the local edit" \
    "$PROJENY" commit fake.projeny
cp "$T262/fake.projeny" "$ROOT/t262-local.projeny"
rm -rf "$T262/fake" "$T262/.fake.projeny.status"
cp "$ROOT/t262-local.projeny" "$T262/fake.projeny"
run_in "$T262" expect_ok "the conflict fixture re-setups the local state" \
    "$PROJENY" setup fake.projeny
sed -i 's/^int beta = 10;$/int beta = 999;/' "$T262/fake/src/a.c"
T262U="$ROOT/t262up"
mkdir -p "$T262U"
cp "$T262/fake-1.0.tar.gz" "$T262U/"
cp "$ROOT/t262-local.projeny" "$T262U/fake.projeny"
run_in "$T262U" expect_ok "the upstream side of the conflict sets up" \
    "$PROJENY" setup fake.projeny
sed -i 's/^int beta = 10;$/int beta = 555;/' "$T262U/fake/src/a.c"
run_in "$T262U" expect_ok "the upstream side commits its edit" "$PROJENY" \
    commit fake.projeny
cp "$T262U/fake.projeny" "$T262/fake.projeny"
run_in "$T262" expect_fail "the conflicting setup merge exits nonzero" \
    "$PROJENY" setup fake.projeny
expect_file_contains "the conflict fixture has a Conflict: entry" \
    "$T262/.fake.projeny.status" "Conflict: src/a.c"
out="$(run_in "$T262" "$PROJENY" erase-setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup of a conflicted checkout exits 1"
else
    fail "the no-force erase-setup of a conflicted checkout exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"'fake.projeny': conflict: 'src/a.c'"*)
    ok "the refusal bullet names the conflicted file"
    ;;
*)
    fail "the refusal bullet names the conflicted file" "out: $out"
    ;;
esac
if [ -d "$T262/fake" ] && [ -f "$T262/.fake.projeny.status" ]; then
    ok "the conflicted refusal leaves the workdir and status file"
else
    fail "the conflicted refusal leaves the workdir and status file" \
         "ls: $(ls -A "$T262" 2>&1)"
fi
if grep -q "<<<<<<<" "$T262/fake/src/a.c"; then
    ok "the conflict markers themselves survive the refusal"
else
    fail "the conflict markers themselves survive the refusal" \
         "src/a.c: $(cat "$T262/fake/src/a.c" 2>&1)"
fi

# --------- 263. one dirty project refuses the whole batch, clean included
# The no-force check is all-or-nothing across the invocation: with one
# dirty project and one clean one, NOTHING is erased — the clean project's
# workdir and status file stay too — and the refusal bullet list names only
# the dirty project. The same invocation with --force erases both.
T263="$ROOT/t263"
mkdir -p "$T263"
cp "$T222/a.projeny" "$T222/b.projeny" "$T222/fake-1.0.tar.gz" "$T263/"
run_in "$T263" expect_ok "the mixed-dirtiness fixture sets up" "$PROJENY" \
    setup a.projeny b.projeny
printf 'an uncommitted edit\n' >> "$T263/fakea/README"
out="$(run_in "$T263" "$PROJENY" erase-setup a.projeny b.projeny -j2 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the mixed batch with one dirty project exits 1"
else
    fail "the mixed batch with one dirty project exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"projeny: error: refusing to erase 1 of 2 project(s) with uncommitted " | \
*"changes (use --force to erase anyway)"*)
    ok "the mixed refusal counts the dirty project of two"
    ;;
*)
    fail "the mixed refusal counts the dirty project of two" "out: $out"
    ;;
esac
nbullets="$(printf '%s\n' "$out" | grep -c '^  ')"
if [ "$nbullets" -eq 1 ]; then
    ok "the mixed refusal has exactly one bullet"
else
    fail "the mixed refusal has exactly one bullet" \
         "got $nbullets bullets (out: $out)"
fi
case "$out" in
*"'a.projeny': modified: 'README'"*)
    ok "the mixed refusal names only the dirty project"
    ;;
*)
    fail "the mixed refusal names only the dirty project" "out: $out"
    ;;
esac
if [ -d "$T263/fakea" ] && [ -f "$T263/.a.projeny.status" ] && \
   [ -d "$T263/fakeb" ] && [ -f "$T263/.b.projeny.status" ]; then
    ok "the mixed refusal erased nothing, clean project included"
else
    fail "the mixed refusal erased nothing, clean project included" \
         "ls: $(ls -A "$T263" 2>&1)"
fi
out="$(run_in "$T263" "$PROJENY" erase-setup a.projeny b.projeny -j2 --force 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the same mixed batch with --force exits 0"
else
    fail "the same mixed batch with --force exits 0" "exit=$rc out: $out"
fi
if [ ! -e "$T263/fakea" ] && [ ! -e "$T263/.a.projeny.status" ] && \
   [ ! -e "$T263/fakeb" ] && [ ! -e "$T263/.b.projeny.status" ]; then
    ok "the --force mixed batch erased both projects"
else
    fail "the --force mixed batch erased both projects" \
         "ls: $(ls -A "$T263" 2>&1)"
fi

# --------- 264. a missing checkout is clean without --force
# The no-force check's first rule: a workdir that is gone means there is
# nothing to destroy, so the project is clean no matter what the status
# file recorded — the erase phase proceeds and warns the workdir missing,
# exactly as it always has (the check never runs the status machinery on a
# missing workdir, so nothing is renamed to .stale behind the user's back).
T264="$ROOT/t264"
mkdir -p "$T264"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T264/"
run_in "$T264" expect_ok "the missing-checkout fixture sets up" "$PROJENY" \
    setup a.projeny
rm -rf "$T264/fakea"
out="$(run_in "$T264" "$PROJENY" erase-setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the no-force erase-setup of a missing checkout exits 0"
else
    fail "the no-force erase-setup of a missing checkout exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"'fakea' did not exist; nothing to erase"*)
    ok "the erase warns the checkout was missing"
    ;;
*)
    fail "the erase warns the checkout was missing" "out: $out"
    ;;
esac
if [ ! -e "$T264/.a.projeny.status" ] && [ ! -e "$T264/fakea" ]; then
    ok "the status file still went"
else
    fail "the status file still went" "ls: $(ls -A "$T264" 2>&1)"
fi
nstale="$(printf '%s' "$(ls -A "$T264")" | grep -c 'stale')"
if [ "$nstale" -eq 0 ]; then
    ok "the check left no .stale renames behind"
else
    fail "the check left no .stale renames behind" \
         "ls: $(ls -A "$T264" 2>&1)"
fi

# --------- 265. --force erases a dirty project, snapshots included
# --force is the escape hatch: the check is skipped entirely and a dirty
# project is erased no matter what its status reports. Combined with
# --erase-snapshots even the snapshot goes, so the next setup re-downloads.
T265="$ROOT/t265"
mkdir -p "$T265"
cp "$T222/a.projeny" "$T222/fake-1.0.tar.gz" "$T265/"
run_in "$T265" expect_ok "the force-dirty fixture sets up" "$PROJENY" \
    setup a.projeny
printf 'an uncommitted edit\n' >> "$T265/fakea/README"
out="$(run_in "$T265" "$PROJENY" erase-setup a.projeny --force 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the --force erase of a dirty project exits 0"
else
    fail "the --force erase of a dirty project exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'fakea' (checkout 'fakea', status '.a.projeny.status')"*)
    ok "the --force erase reports the usual note"
    ;;
*)
    fail "the --force erase reports the usual note" "out: $out"
    ;;
esac
if [ ! -e "$T265/fakea" ] && [ ! -e "$T265/.a.projeny.status" ] && \
   [ -f "$T265/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the dirty checkout and status went but the snapshot stayed"
else
    fail "the dirty checkout and status went but the snapshot stayed" \
         "ls: $(ls -A "$T265" 2>&1)"
fi
out="$(run_in "$T265" "$PROJENY" setup a.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the setup after the --force erase of the dirty project exits 0"
else
    fail "the setup after the --force erase of the dirty project exits 0" \
         "exit=$rc out: $out"
fi
if [ "$(cat "$T265/fakea/README" 2>/dev/null)" = "hello v1" ]; then
    ok "the re-setup checked out the pristine file"
else
    fail "the re-setup checked out the pristine file" \
         "README: $(cat "$T265/fakea/README" 2>&1)"
fi
printf 'another uncommitted edit\n' >> "$T265/fakea/README"
run_in "$T265" expect_ok "erase-setup --force --erase-snapshots exits 0" \
    "$PROJENY" erase-setup a.projeny --force --erase-snapshots
if [ ! -e "$T265/fakea" ] && [ ! -e "$T265/.a.projeny.status" ] && \
   [ ! -e "$T265/.fake-1.0.tar.gz.snapshot" ]; then
    ok "--force with --erase-snapshots erased the snapshot too"
else
    fail "--force with --erase-snapshots erased the snapshot too" \
         "ls: $(ls -A "$T265" 2>&1)"
fi

# --------- 266. a deleted archive and snapshot refuse a no-force erase-setup
# The reported fail-open hole: when BOTH the checked-in tarball and its
# .snapshot are gone, the check phase cannot build the expected tree, so the
# live diff cannot run and NOTHING is known about the checkout. A project
# whose state cannot be assessed is never clean — the whole invocation
# refuses (the unverifiable refusal, exit 1, same shape as a check that
# died) and nothing is erased, down to the byte; --force erases anyway.
# Beside the unverifiable project stands a healthy one, to pin the
# all-or-nothing guarantee: the healthy project's setup state survives the
# refusal too, and the bullet list names only the project that could not be
# checked.
T266="$ROOT/t266"
make_tarballs "$T266" fake
# Two Archive:-based projects (write_projeny names the file after the stem,
# so the a/b pair is spelled out here): a.projeny -> fakea from fake-1.0,
# b.projeny -> fakeb from fake-2.0, same format write_projeny emits.
printf 'Archive: fake-1.0.tar.gz\nOrigname: fake-1.0\nName: fakea\n\n    Fake project a for projeny tests.\n\n' \
    > "$T266/a.projeny"
printf 'Archive: fake-2.0.tar.gz\nOrigname: fake-2.0\nName: fakeb\n\n    Fake project b for projeny tests.\n\n' \
    > "$T266/b.projeny"
run_in "$T266" expect_ok "the unassessable-archive fixture sets up" "$PROJENY" \
    setup a.projeny b.projeny
if [ -f "$T266/fake-1.0.tar.gz" ] && [ -f "$T266/.fake-1.0.tar.gz.snapshot" ] \
   && [ -f "$T266/fake-2.0.tar.gz" ] && \
   [ -f "$T266/.fake-2.0.tar.gz.snapshot" ]; then
    ok "the fixture has both archives and both snapshots"
else
    fail "the fixture has both archives and both snapshots" \
         "ls: $(ls -A "$T266" 2>&1)"
fi
printf 'an uncommitted edit\n' >> "$T266/fakea/README"
if grep -q "uncommitted edit" "$T266/fakea/README"; then
    ok "the fixture has an uncommitted edit in the checkout"
else
    fail "the fixture has an uncommitted edit in the checkout" \
         "README: $(cat "$T266/fakea/README" 2>&1)"
fi
rm "$T266/fake-1.0.tar.gz" "$T266/.fake-1.0.tar.gz.snapshot"
if [ ! -e "$T266/fake-1.0.tar.gz" ] && \
   [ ! -e "$T266/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the archive and its snapshot are gone"
else
    fail "the archive and its snapshot are gone" "ls: $(ls -A "$T266" 2>&1)"
fi
cp -a "$T266" "$ROOT/t266keep"
out="$(run_in "$T266" "$PROJENY" erase-setup a.projeny b.projeny -j2 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup over a deleted archive+snapshot exits 1"
else
    fail "the no-force erase-setup over a deleted archive+snapshot exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"cannot check 1 of 2 project(s) for uncommitted changes; refusing to " | \
*"erase anything (use --force to erase anyway)"*)
    ok "the refusal counts the unverifiable project of two"
    ;;
*)
    fail "the refusal counts the unverifiable project of two" "out: $out"
    ;;
esac
nbullets="$(printf '%s\n' "$out" | grep -c '^  ')"
if [ "$nbullets" -eq 1 ]; then
    ok "the refusal has exactly one bullet"
else
    fail "the refusal has exactly one bullet" \
         "got $nbullets bullets (out: $out)"
fi
nver="$(printf '%s\n' "$out" | grep -cF \
    "  'a.projeny': cannot verify the checkout against its archive (the archive and its snapshot are missing or unusable); it cannot be checked for uncommitted changes")"
if [ "$nver" -eq 1 ]; then
    ok "the refusal bullet says why the checkout cannot be checked"
else
    fail "the refusal bullet says why the checkout cannot be checked" \
         "got $nver matching lines (out: $out)"
fi
if [ "$(printf '%s\n' "$out" | grep -c 'erased setup state for')" -eq 0 ]
then
    ok "the unverifiable refusal erases nothing"
else
    fail "the unverifiable refusal erases nothing" "out: $out"
fi
if diff -r "$T266" "$ROOT/t266keep" >/dev/null 2>&1; then
    ok "the refusal leaves every remaining file byte-identical"
else
    fail "the refusal leaves every remaining file byte-identical" \
         "$(diff -r "$T266" "$ROOT/t266keep" 2>&1 | head -5)"
fi
if grep -q "uncommitted edit" "$T266/fakea/README"; then
    ok "the uncommitted edit survives the refusal"
else
    fail "the uncommitted edit survives the refusal" \
         "README: $(cat "$T266/fakea/README" 2>&1)"
fi
out="$(run_in "$T266" "$PROJENY" erase-setup a.projeny b.projeny -j2 --force \
    2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the same state with --force exits 0"
else
    fail "the same state with --force exits 0" "exit=$rc out: $out"
fi
nexpect="$(printf '%s\n' "$out" | grep -c 'erased setup state for')"
if [ "$nexpect" -eq 2 ]; then
    ok "--force erased both projects despite the deleted archive"
else
    fail "--force erased both projects despite the deleted archive" \
         "got $nexpect notes (out: $out)"
fi
if [ ! -e "$T266/fakea" ] && [ ! -e "$T266/.a.projeny.status" ] && \
   [ ! -e "$T266/fakeb" ] && [ ! -e "$T266/.b.projeny.status" ]; then
    ok "--force left no setup state behind"
else
    fail "--force left no setup state behind" "ls: $(ls -A "$T266" 2>&1)"
fi

# --------- 267. the URL variant refuses the same way when the source is gone
# A URL:-based project has no checked-in tarball: its snapshot is the
# download cache. Delete the snapshot AND the file:// source tarball and the
# check phase cannot materialize the archive even by re-downloading — the
# same fail-closed refusal, exit 1, nothing erased; --force erases anyway.
# The download failure still warns status's "(continuing without the live
# diff)" line first, exactly as status has always done.
T267="$ROOT/t267"
make_tarballs "$T267" fake
h267="$("$PROJENY" hash "$T267/fake-1.0.tar.gz")"
printf 'URL: file://%s/fake-1.0.tar.gz %s\nOrigname: fake-1.0\nName: fake\n\n    URL project with nowhere to re-download from.\n\n' \
    "$T267" "$h267" > "$T267/fake.projeny"
run_in "$T267" expect_ok "the unassessable-URL fixture sets up" "$PROJENY" \
    setup fake.projeny
if [ -f "$T267/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the URL fixture has a snapshot"
else
    fail "the URL fixture has a snapshot" "ls: $(ls -A "$T267" 2>&1)"
fi
printf 'an uncommitted edit\n' >> "$T267/fake/README"
rm "$T267/.fake-1.0.tar.gz.snapshot" "$T267/fake-1.0.tar.gz"
if [ ! -e "$T267/.fake-1.0.tar.gz.snapshot" ] && \
   [ ! -e "$T267/fake-1.0.tar.gz" ]; then
    ok "the URL snapshot and its source tarball are gone"
else
    fail "the URL snapshot and its source tarball are gone" \
         "ls: $(ls -A "$T267" 2>&1)"
fi
cp -a "$T267" "$ROOT/t267keep"
out="$(run_in "$T267" "$PROJENY" erase-setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup over an unreachable URL source exits 1"
else
    fail "the no-force erase-setup over an unreachable URL source exits 1" \
         "exit=$rc out: $out"
fi
case "$out" in
*"cannot check 1 of 1 project(s) for uncommitted changes; refusing to " | \
*"erase anything (use --force to erase anyway)"*)
    ok "the URL-variant refusal counts the unverifiable project"
    ;;
*)
    fail "the URL-variant refusal counts the unverifiable project" \
         "out: $out"
    ;;
esac
nver="$(printf '%s\n' "$out" | grep -cF \
    "  'fake.projeny': cannot verify the checkout against its archive (the archive and its snapshot are missing or unusable); it cannot be checked for uncommitted changes")"
if [ "$nver" -eq 1 ]; then
    ok "the URL-variant refusal bullet says why the checkout cannot be checked"
else
    fail "the URL-variant refusal bullet says why the checkout cannot be checked" \
         "got $nver matching lines (out: $out)"
fi
ncont="$(printf '%s\n' "$out" | grep -cF '(continuing without the live diff)')"
if [ "$ncont" -eq 1 ]; then
    ok "the failed download still warns before the refusal"
else
    fail "the failed download still warns before the refusal" \
         "got $ncont matching lines (out: $out)"
fi
if diff -r "$T267" "$ROOT/t267keep" >/dev/null 2>&1; then
    ok "the URL-variant refusal leaves every remaining file byte-identical"
else
    fail "the URL-variant refusal leaves every remaining file byte-identical" \
         "$(diff -r "$T267" "$ROOT/t267keep" 2>&1 | head -5)"
fi
if grep -q "uncommitted edit" "$T267/fake/README"; then
    ok "the URL checkout's uncommitted edit survives the refusal"
else
    fail "the URL checkout's uncommitted edit survives the refusal" \
         "README: $(cat "$T267/fake/README" 2>&1)"
fi
out="$(run_in "$T267" "$PROJENY" erase-setup fake.projeny --force 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the URL variant with --force exits 0"
else
    fail "the URL variant with --force exits 0" "exit=$rc out: $out"
fi
if [ ! -e "$T267/fake" ] && [ ! -e "$T267/.fake.projeny.status" ]; then
    ok "--force erased the URL project's setup state"
else
    fail "--force erased the URL project's setup state" \
         "ls: $(ls -A "$T267" 2>&1)"
fi

# --------- 268. fail closed even when the checkout happens to be clean
# The refusal is not about dirtiness — it is about knowability: a CLEAN
# checkout whose archive and snapshot are both gone refuses too, because
# erase-setup cannot know it is clean. Restoring the tarball makes the check
# runnable again (the copy-on-fallback re-materializes the snapshot) and the
# same no-force erase then proceeds, so the conservatism is recoverable and
# never a wedge.
T268="$ROOT/t268"
make_tarballs "$T268" fake
write_projeny "$T268" fake 1.0 fake
run_in "$T268" expect_ok "the clean-unassessable fixture sets up" "$PROJENY" \
    setup fake.projeny
mkdir -p "$ROOT/t268keep"
cp "$T268/fake-1.0.tar.gz" "$ROOT/t268keep/fake-1.0.tar.gz"
rm "$T268/fake-1.0.tar.gz" "$T268/.fake-1.0.tar.gz.snapshot"
out="$(run_in "$T268" "$PROJENY" erase-setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase-setup of a clean but unverifiable checkout exits 1"
else
    fail "the no-force erase-setup of a clean but unverifiable checkout exits 1" \
         "exit=$rc out: $out"
fi
nver="$(printf '%s\n' "$out" | grep -cF \
    "  'fake.projeny': cannot verify the checkout against its archive (the archive and its snapshot are missing or unusable); it cannot be checked for uncommitted changes")"
if [ "$nver" -eq 1 ]; then
    ok "the clean-unverifiable refusal carries the same bullet"
else
    fail "the clean-unverifiable refusal carries the same bullet" \
         "got $nver matching lines (out: $out)"
fi
if [ -d "$T268/fake" ] && [ -f "$T268/.fake.projeny.status" ]; then
    ok "the clean-unverifiable refusal erased nothing"
else
    fail "the clean-unverifiable refusal erased nothing" \
         "ls: $(ls -A "$T268" 2>&1)"
fi
cp "$ROOT/t268keep/fake-1.0.tar.gz" "$T268/fake-1.0.tar.gz"
out="$(run_in "$T268" "$PROJENY" erase-setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "restoring the tarball makes the same no-force erase exit 0"
else
    fail "restoring the tarball makes the same no-force erase exit 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'fake' (checkout 'fake', status '.fake.projeny.status')"*)
    ok "the recovered erase reports the usual note"
    ;;
*)
    fail "the recovered erase reports the usual note" "out: $out"
    ;;
esac
if [ ! -e "$T268/fake" ] && [ ! -e "$T268/.fake.projeny.status" ]; then
    ok "the recovered erase removed the checkout and status file"
else
    fail "the recovered erase removed the checkout and status file" \
         "ls: $(ls -A "$T268" 2>&1)"
fi
if [ -f "$T268/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the check phase recreated the snapshot before erasing"
else
    fail "the check phase recreated the snapshot before erasing" \
         "ls: $(ls -A "$T268" 2>&1)"
fi

# --------- 269. regression guard: healthy projects behave exactly as before
# With the archive and snapshot intact the check runs, so a clean project
# still erases without --force (exit 0, the usual note, snapshot kept), and
# a dirty one still refuses in status's own vocabulary — the new fail-closed
# path must never shadow or reword the ordinary dirtiness refusal.
T269="$ROOT/t269"
make_tarballs "$T269" fake
write_projeny "$T269" fake 1.0 fake
run_in "$T269" expect_ok "the healthy fixture sets up" "$PROJENY" \
    setup fake.projeny
out="$(run_in "$T269" "$PROJENY" erase-setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the no-force erase of a healthy clean project exits 0"
else
    fail "the no-force erase of a healthy clean project exits 0" \
         "exit=$rc out: $out"
fi
case "$out" in
*"erased setup state for 'fake' (checkout 'fake', status '.fake.projeny.status')"*)
    ok "the healthy erase reports the usual note"
    ;;
*)
    fail "the healthy erase reports the usual note" "out: $out"
    ;;
esac
if [ ! -e "$T269/fake" ] && [ ! -e "$T269/.fake.projeny.status" ] && \
   [ -f "$T269/fake-1.0.tar.gz" ] && \
   [ -f "$T269/.fake-1.0.tar.gz.snapshot" ]; then
    ok "the healthy erase took the checkout and status but kept the archive"
else
    fail "the healthy erase took the checkout and status but kept the archive" \
         "ls: $(ls -A "$T269" 2>&1)"
fi
write_projeny "$T269" fake 1.0 fake
run_in "$T269" expect_ok "the healthy-dirty fixture sets up" "$PROJENY" \
    setup fake.projeny
printf 'an uncommitted edit\n' >> "$T269/fake/README"
out="$(run_in "$T269" "$PROJENY" erase-setup fake.projeny 2>&1)"
rc=$?
if [ $rc -eq 1 ]; then
    ok "the no-force erase of a healthy dirty project exits 1"
else
    fail "the no-force erase of a healthy dirty project exits 1" \
         "exit=$rc out: $out"
fi
nmod="$(printf '%s\n' "$out" | grep -cF "  'fake.projeny': modified: 'README'")"
if [ "$nmod" -eq 1 ]; then
    ok "the dirty refusal still names the modified file"
else
    fail "the dirty refusal still names the modified file" \
         "got $nmod matching lines (out: $out)"
fi
nver="$(printf '%s\n' "$out" | grep -c 'cannot verify the checkout')"
if [ "$nver" -eq 0 ]; then
    ok "the dirty refusal is not reworded into the unverifiable one"
else
    fail "the dirty refusal is not reworded into the unverifiable one" \
         "got $nver matching lines (out: $out)"
fi
out="$(run_in "$T269" "$PROJENY" erase-setup fake.projeny --force 2>&1)"
rc=$?
if [ $rc -eq 0 ]; then
    ok "the healthy dirty project still erases with --force"
else
    fail "the healthy dirty project still erases with --force" \
         "exit=$rc out: $out"
fi

# ------------------------------------------------------------- summary
echo "---"
echo "passed: $PASS, failed: $FAIL"
rm -rf "$ROOT"
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
