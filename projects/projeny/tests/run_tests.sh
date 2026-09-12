#!/bin/bash
# projeny test suite: fixture-based shell tests over tiny fake-project
# tarballs v1/v2. Covers fresh setup, edit+commit roundtrips, setup-again
# noops, divergent merges (clean + conflicting, the latter exiting nonzero),
# add/rm/mv + commit, rebase (clean + conflict), package/extract (tracked-only
# payloads, compression autodetect, conflict refusal), and the hard-error paths.
# Later sections cover the dot-prefixed bookkeeping names: legacy undotted
# .projeny.status / .snapshot migration on first use, stale-state
# reconciliation (.stale, .stale2, ...) when the workdir is gone (both
# naming forms, plus the previous archive's snapshot after a rebase), the
# setup-journal crash window (nothing is staled while recovery state is
# needed), the workdir-without-status hard error, and snapshot
# copy-on-fallback from the tarball (setup and status).
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
# (bytes preserved exactly; commit compares raw bytes).
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

# ------------------------------------------------------------- summary
echo "---"
echo "passed: $PASS, failed: $FAIL"
rm -rf "$ROOT"
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
