# projeny — project tarball+patch manager

BSD 2-clause licensed (see LICENSE.txt). Has zero third-party dependencies
(C++ standard library + POSIX only; at runtime it shells out to `tar` and
`cp` — see Runtime dependencies; diff, patch application, and three-way
merge are implemented internally with git-compatible unified diffs, plus
base64 `GIT binary patch` blocks for NUL-bearing files).

`projeny` replaces the old workflow where `projects/` held full copies of
external release tarballs plus Fil-C commits on top (huge git checkins).
Instead, git tracks two small files per project:

- the release tarball (e.g. `lua-5.4.7.tar.bz2`), and
- a `.projeny` file naming that tarball plus a git-style patch.

The unpacked work tree (`lua/`) and the dot-prefixed
`.lua.projeny.status` bookkeeping file are never tracked by git (see
[File naming and migration](#file-naming-and-migration) below).

## Usage

All commands take the `.projeny` path (relative or absolute). The tarball is
looked up next to the `.projeny` file, and the work tree is created next to
it as well (named by the `Name:` header).

```
projeny setup <f.projeny>                unpack archive, apply patch
projeny commit <f.projeny>               fold workdir changes into the patch
projeny add <f.projeny> <path>           mark a file as added
projeny rm <f.projeny> <path>            delete a file, mark as removed
projeny mv <f.projeny> <src> <dst>       rename a file, mark as renamed
projeny resolve <f.projeny> <path>       clear a conflict entry
projeny rebase <f.projeny> <tarball>     point the project at a new tarball
projeny status <f.projeny>               show setup/conflict/pending state
projeny diff <dir> <other-dir>           print the diff between two trees
projeny patch <dir> <patch-file>         apply a patch file to a tree
projeny package <f.projeny|dir> <out>    setup, then tar the tracked files
projeny extract <f.projeny|dir> <dest>   setup, then copy tracked files to a dir
projeny help [command]                   show help (per-command with a name)
```

Paths into the work tree may be CWD-relative, absolute, or workdir-relative
(`<Name>/...`); they are stored relative to the workdir.

- `setup`: unpacks `Archive:` next to the `.projeny` file, requires it to
  produce exactly one top-level directory named by `Origname:` (hard error
  otherwise), renames it to `Name:`, applies the patch, and writes the
  dot-prefixed `.<f>.projeny.status`. It also maintains a snapshot copy of
  the archive (see "Archive snapshots" below). When the workdir exists but
  the status file does not, setup adopts the directory in place if it
  holds nothing the tarball or patch would overwrite (an empty directory,
  or one holding only files setup never touches, which are kept and ride
  along like user-added files); otherwise it refuses, listing the paths it
  would have overwritten (see "File naming and migration" below). If the
  workdir exists and the status file does too, projeny reconstructs the
  expected
  tree from the status copy, diffs it against the workdir to find your
  uncommitted changes, and merges them onto a fresh setup of the *current*
  `.projeny` (which may name a different `Archive:` — e.g. upstream moved
  to a newer tarball). Merge failures leave conflict markers in the workdir
  and record the files in the status file. A setup that leaves conflicts
  still finishes (workdir, `.projeny` file, and status are all updated) but
  exits 1, so scripts under `set -e` stop instead of building from a
  conflicted tree; fix the files, `resolve` each one, and `commit`.
- `commit`: requires the `.projeny` file to match the status copy exactly
  (else hard error: run `setup` to merge first) and refuses when conflicts
  are pending. Otherwise it diffs the workdir against the base archive and
  stores the new patch in both `.projeny` and the status file (pending
  add/rm/mv ops are validated and folded in — the diff already reflects
  the on-disk renames/deletions).
- `add`/`rm`/`mv`: record pending added/removed/renamed files in the
  status file (`rm` also deletes the file; `mv` also renames it). Adding a
  file that a later `setup` also adds is a merge conflict.
- `resolve`: drops a file from the status conflict list after you fix it.
  Conflict paths are stored workdir-relative (e.g. `src/a.c`), and `resolve`
  accepts any of: the stored form as-is, the on-disk `<Name>/...`-prefixed
  form, a CWD-relative path, or an absolute path — whichever matches the
  conflict entry. If the file still contains conflict-marker lines
  (`<<<<<<<`, `=======`, `>>>>>>>`, `|||||||`), projeny prints a warning to
  stderr but still resolves (warn, don't error), since committing markers
  would bake them into the patch.
- `rebase <new-tarball>`: requires a clean tree (workdir matches the
  current patch) and no pending conflicts. If never set up, it runs
  `setup` first. It applies the current patch onto the new tarball,
  rewrites `Archive:`/`Origname:`, regenerates the patch, and moves the
  result into place; conflicts leave markers and are recorded in status.
  Pending add/rm/mv operations are preserved across the rebase (they are
  workdir-relative intent, still valid against the new base), matching how
  `setup` merges keep them. If the new tarball's basename equals the
  current `Archive:` but its content differs (size/hash compare), projeny
  warns to stderr and proceeds with the new file — it never silently keeps
  the old bytes.

- `diff <dir> <other-dir>`: prints the minimal unified diff between
  two on-disk trees to stdout (labels use the second directory's
  basename, so the output feeds `projeny patch`, `git apply`, and
  `patch -p1`). Both arguments must be directories.
- `patch <dir> <patch-file>`: applies a patch file to a directory with
  fuzz; already-applied blocks are skipped. Unapplyable blocks become
  conflicts: markers (`<<<<<<< current` / `=======` / `>>>>>>> patched`)
  go inline and the conflicted files are listed on stdout (exit stays 0).
- `package <f.projeny|dir> <output-tarball>`: runs `setup` (so uncommitted
  workdir changes are included, and conflicts fail the command with a
  nonzero exit and no archive), then tars up exactly the tracked files —
  base archive plus patch plus pending add/rm/mv ops, minus untracked
  files — like `git archive` / `package-source.sh`. The first argument may
  be the `.projeny` file or a directory holding (or, as `<dir>.projeny`
  for a `<dir>` workdir, naming) exactly one of them. The archive holds a
  single top-level directory named after the output file, and compression
  is autodetected from its extension (`.tar`, `.tar.gz`/`.tgz`,
  `.tar.bz2`, `.tar.xz`, `.tar.zst`).
- `extract <f.projeny|dir> <dest-dir>`: like `package`, except the tracked
  files are copied into `<dest-dir>` instead of archived — the projeny
  variant of `extract_source`, for building outside the checkout. The
  destination must not exist or must be empty.
- `help [command]`: with a command name, prints a detailed explanation
  of that command.

## Recovering a git-conflicted `.projeny` file

When a `git pull --rebase`, `stash pop`, or `merge` leaves conflict
markers (`<<<<<<<`, `=======`, `>>>>>>>`, `|||||||`) in a `.projeny`
file, every command except `setup` refuses to run. Marker-shaped lines
can never occur in a well-formed `.projeny` file (headers are
`Key: value`, patch structure lines start with other characters, and
every hunk-body line carries a prefix byte, while free-text prose always
carries its leading-space indent), so detection is exact —
hand-written prose must indent every non-empty line with a leading space.

`setup` force-takes the upstream side for the `.projeny` file
and status copy, merges the local-side patch into the workdir
with conflicts marked (three-way against the shared base archive when
both sides name the same tarball), and re-applies any uncommitted
workdir-vs-status changes on top. Which side is upstream is auto-detected:
`git merge` keeps ours=local, while `git pull --rebase`/`rebase` and
`stash pop` swap the sides (`<<<<<<< HEAD` holds upstream there) —
projeny reads the branch labels (`upstream`/`origin`/`remote`/`theirs`,
`stashed`), the status-file copy, and one-sided validity, defaulting to
merge convention. A truncated or binary-garbled
`.projeny` file is refused without touching the workdir or status file
(restore it with `git checkout -- <file>` and run `setup` again).
A delete/modify conflict (one side deleted the `.projeny` file) cannot be
merged automatically: pick a side with
`git checkout --ours/--theirs -- <file>` and run `setup` again.

## `.projeny` format

```
Archive: lua-5.4.7.tar.bz2
Origname: lua-5.4.7
Name: lua

    Free text (anything) between the headers and the patch.

diff --git a/lua/makefile b/lua/makefile
...
```

Headers (`Archive:`, `Origname:`, `Name:` — all required; extra headers are
preserved verbatim) end at the first blank line. Everything up to the first
`diff --git` line is free text. Every non-empty free-text line starts with
a space or tab (projeny prepends a single space on write when one is
missing, idempotently — already-indented prose roundtrips byte-for-byte —
so hand-written prose migrates on the next commit/rebase). Because of that
rule, marker-shaped lines (`<<<<<<<`, `=======`, `>>>>>>>`, `|||||||`)
at column 0 can never appear in a well-formed file (headers are
`Key: value`, patch structure lines start with other characters, and
hunk-body lines carry a prefix byte), so git conflict markers in a
`.projeny` file are always detected exactly — free-text prose must therefore
use the leading-space indent (a column-0 marker-shaped prose line is refused
as a conflict, with a hint to indent it). Patch labels use the workdir name
(`a/<Name>/...`, `b/<Name>/...`); rename entries use bare workdir-relative
paths. The patch may be empty (plain tarball, no changes). Paths with spaces,
tabs, quotes, backslashes, `->`, or other special bytes are stored git
C-quoted (`"a/<Name>/my file.c"`); plain paths stay unquoted. Either form is
accepted on input, so hand-written patches need no special handling.

## `.status` format

Text file `.<f>.projeny.status` (untracked by git; older projenies wrote

`<f>.projeny.status`, which is renamed into the dotted form on first use):

```
Status: setup
Conflict: src/a.c        (repeatable, optional)
Added: src/new.c         (repeatable, optional)
Removed: src/old.c       (repeatable, optional)
Renamed: src/a.c -> src/b.c   (repeatable, optional)
--- projeny content ---
<verbatim copy of the .projeny file as of the last setup/commit>
```

`Status: setup` means the workdir was set up; the embedded copy lets
`setup` reconstruct the expected tree later (even across `Archive:`
changes); conflicts and pending add/rm/mv operations are listed above the
delimiter.

## Archive snapshots

Every status-file write also maintains `.<Archive>.snapshot` — a byte-exact
copy of the tarball — next to the archive itself (e.g.
`.lua-5.5.4.tar.gz.snapshot`). Trees are reconstructed from the status
file's embedded `.projeny` copy (and from the local side of a
git-conflicted `.projeny`), so their archives may no longer exist: setup
and `status` prefer the snapshot over the archive and fall back to the
archive for checkouts set up by older projenies — and that fallback copies
the archive into the snapshot right away, so the next run finds a snapshot.
This keeps `projeny setup` (and therefore `package`/`extract` and every
build script) working after the archive was deleted from git — typically
because an upstream rebase to a newer tarball did `git rm` on the old one.
A missing snapshot is not an error while the tarball still exists: it is
recreated from the tarball on first use (including by `status`). Snapshots
are plain untracked files, safe to delete at any time (the next setup
recreates them); when both the archive and its snapshot are missing, setup
fails with recovery guidance. Snapshots left behind by a tarball that no
longer exists are not auto-cleaned.

## File naming and migration

Two bookkeeping files live next to the tracked files, and both are hidden
(dot-prefixed) so they never clutter directory listings or accidental
checkins:

- `.<f>.projeny.status` — the status file for `f.projeny` (e.g.
  `.lua.projeny.status`), and
- `.<Archive>.snapshot` — the snapshot copy of an archive (e.g.
  `.lua-5.4.7.tar.bz2.snapshot`).

Older projenies wrote the undotted forms (`f.projeny.status` and
`<Archive>.snapshot`). Both forms keep working: on first use — by any
command, since every command resolves the status path up front — a legacy
undotted file is renamed to its dotted name (content preserved, so a
checkout set up by an older projeny migrates itself the moment any projeny
command touches it). When both forms exist, the dotted one wins and the
undotted one is left untouched. The crash-recovery journal
(`f.projeny.setup-journal`) never had an undotted status-style form, so it
needs no migration.

When the workdir (e.g. `lua/`) does not exist, the status file and
snapshots describe a checkout that is gone; they are stale state. Commands
disregard them instead of tripping over them: each stale file is renamed to
`<name>.stale` (then `.stale2`, `.stale3`, ... when a name is taken) with a
warning naming where it went, and the command then proceeds as if the
checkout had never been set up (`setup` does a fresh setup; mutating
commands like `commit`/`add`/`rm`/`mv`/`resolve` still hard-error, since
there is nothing left to work on). This covers both naming forms: when the
dotted and undotted versions of a file both exist, each is staled under its
own name, so nothing survives under an original name. The one exception is
enforced for every command: nothing is disregarded while a setup journal
(`f.projeny.setup-journal`) exists. The journal marks an interrupted
conflicted setup whose recovery still needs the status file (conflict-side
disambiguation and union bookkeeping) and the snapshot (often the only
remaining copy of the local side's archive once the rebase deleted the old
tarball) — renaming them in that crash window would make the recovery that
a rerun `setup` performs die instead. When the workdir exists but the
status file does not (in either form), the directory was never set up by
projeny (or its bookkeeping is gone). Setup then adopts the directory in
place when it holds nothing the tarball or patch would overwrite — it
unpacks into the existing directory, keeping the files it already had
(nested directories the checkout also has are merged into). Otherwise it
is a hard error listing the offending paths (capped at ten): the status
file is what makes uncommitted changes mergeable, and its absence means
the checkout's provenance is lost, so overwriting anything could destroy
the only copy of it (remove the conflicting files or the whole workdir,
or restore the status file).

## Runtime dependencies

Exactly two external programs (no shell, no `system()`/`popen()` anywhere
— every helper runs via direct `posix_spawnp` with an argv list):

- `tar` to unpack (`-xf ... --no-same-owner --no-same-permissions`, so the
  archive's ownership/permission bits never leak onto the workdir) and to
  list archives (`-tf` for top-dir discovery, `-tvf` for the
  symlink/hardlink-escape audit). Before unpacking, projeny hard-errors on
  absolute member paths, `..` member components, and symlink/hardlink
  members whose target does not stay inside the tree: absolute targets are
  refused, and relative targets are resolved lexically against the member's
  directory (a symlink's target is member-directory-relative; a hardlink's
  target is archive-root-relative, matching how tar links) — a target like
  `b3sum/LICENSE_A2 -> ../LICENSE_A2` resolves back inside the tree and is
  kept, while any target that climbs above the tree root is refused. The
  same resolve-based check applies to the links `commit` collects from the
  workdir and to the links a patch or merge creates.
- `cp -a` for whole-tree copies (moving trees across filesystems when
  `rename(2)` returns `EXDEV` — scratch dirs live in the system temp dir,
  never inside the workdir — and snapshotting files for three-way merges).

Diffing, patch application, and three-way merging are implemented internally
in C++ (no `git` invocation anywhere): text files travel as git-compatible
unified diffs (`diff --git a/... b/...`, `---`/`+++`, `@@` hunks,
new/deleted file entries) accepted by `git apply` and `patch -p1`, and the
applier accepts git-style diffs (including `a/`/`b/` prefixes, `/dev/null`
sides, and `new file mode` lines) with fuzz. NUL-bearing (binary) files
travel as base64 `GIT binary patch` literal blocks (a projeny-internal
encoding: the `literal` sizes are raw byte counts and the bodies are plain
base64, not git's base85+deflate, so binary blocks roundtrip through
`projeny patch` but are not `git apply`-compatible): binary
add/delete/modify, renames, mode changes, and text/binary transitions all
commit, merge (byte-wise three-way: agreement or a one-sided change wins,
divergent changes keep the local bytes and conflict), and package/extract
like text.
Untracked binaries that were never committed and never `add`ed (like a
package tarball written into the workdir, or build junk) stay out of patches
until explicitly added, the way git leaves untracked files out of commits;
`setup` carries them across workdir replacement instead of deleting them.
Files that vanish from the workdir without an explicit `projeny rm` (or
rename) are treated as accidental loss: the next `setup` restores them to
fresh-setup state (tarball plus patch) instead of deleting them from the new
tree — only pending removals and pending rename sources are re-applied as
deletions. Timestamps from the tarball are preserved end to end: files the
patch never touches keep their archive mtimes through `setup`, `package`,
and `extract` (so builds like libffi's skip up-to-date steps, e.g. `doc`),
while files rewritten by patch application get fresh timestamps.

## Build and test

```
make -j$(nproc)     # builds ./projeny with $(CXX) (default g++)
make test           # builds (if needed) and runs tests/run_tests.sh
make clean          # removes the binary and all .o/.d files
```

The Makefile supports `CXX`/`CC` overrides and generates header
dependencies (`-MMD -MP`) so parallel builds work. The code is warning-free
with `-Wall -Wextra` under both system `g++` and the Fil-C compiler:

```
make clean && make CXX=$(pwd)/../../build/bin/clang++ -j$(nproc) && make test
```

Builds can also be out-of-tree. Pass `BUILD_DIR=<dir>` to `make` and all
build products (object files, dep files, and the binary) land in `<dir>`
instead of next to the sources; `make clean` removes them again. Different
build directories are independent of each other, so multiple compilers can
share one source tree without clobbering each other's build. For example,
Fil-C's build scripts use `BUILD_DIR=build-yolo` for the yolo build
(`build_projeny_yolo.sh`) and `BUILD_DIR=build-filc` for the Fil-C build
(`build_projeny.sh`).

## Testing with the Fil-C compiler

After `./build_all_fast.sh`, `build/bin/clang++` exists at the repo root.
Build and test projeny with it (absolute `CXX` path, from this directory):

```
make clean BUILD_DIR=build-filc
make CXX=/path/to/fil-c/build/bin/clang++ BUILD_DIR=build-filc -j$(nproc)
make test BUILD_DIR=build-filc
make clean BUILD_DIR=build-filc
```

This builds out-of-tree into `build-filc/`, leaving any in-tree build
products untouched.

The test suite (`tests/run_tests.sh`) builds tiny fake-project tarballs
v1/v2 in a temp dir and exercises fresh setup, edit+commit roundtrips,
setup-again noops, divergent merges (clean and conflicting, including
across `Archive:` versions), resolve+commit, add/rm/mv+commit, clean and
conflicting rebases, the setup-first rebase fallback, rebase with pending
ops preserved, bare wid-relative `resolve`, marker-warning `resolve`,
trailing-whitespace commit→setup roundtrips, trailing blank lines, pax-metadata tarballs,
newline-less `.projeny` setup→commit, `" -> "` filenames in the status
file, tar-escape rejection, and every hard-error path (missing status,
multi-top-level tarball, commit-after-edit, dirty rebase). It further covers
diff/patch edge cases — empty files, missing trailing newlines, CRLF line
endings, large files with distant/adjacent hunks, binary add/delete/modify
(including renames, mode changes, text/binary transitions, clean and
conflicting merges, and diff/patch roundtrips),
executable-bit preservation (including content-only changes on `+x` files),
new executable files, rename-with-modification, manual delete+add rename
detection, plain `a/`/`b/` and hand-written `p0` patch forms, shifted hunk
offsets (fuzz), symlinks (preserve/retarget/escape rejection, with in-tree
`..` targets resolving inside the tree and kept),
long single-line files, tabs, quote/tab filenames, extra-header
preservation, no-change commits, and CLI error paths — plus optional
`git apply --check` / `patch -p1 --dry-run` compatibility spot-checks that
verify stored patches with those tools when they are installed, plus
`diff`/`patch` roundtrips and minimum-diff cases, `patch` conflict
markers with console lists, setup-restores-accidentally-deleted-files
(text and binary; explicit `rm` stays deleted), tarball-mtime preservation
through setup/extract/package (patched files go newer), untracked-binary
tolerance (binaries ride along across setups and stay out of patches until
explicitly added), git-conflicted `.projeny` recovery via
`setup` (simple, harder, and no-checkout cases, with real `git` merges
when `git` is installed and hand-made markers otherwise), malformed /
truncated `.projeny` refusal, per-command `help <cmd>` texts, and
`package`/`extract` (tracked-only payloads, uncommitted-change inclusion,
compression autodetect, conflict refusal, `extract`-equals-`package`). It
also covers the dot-prefixed bookkeeping names: legacy undotted
`f.projeny.status`/`<Archive>.snapshot` migration on first use (by `setup`
and by other commands), dotted-wins-when-both-exist, stale-state
reconciliation when the workdir is gone (`<name>.stale`, `.stale2`,
`.stale3`, with warnings naming the destinations, for `setup`, `status`,
`commit`, `add`, and via `setup` for `rebase`; both naming forms staled
under their own names, plus the previous archive's snapshot when the
status names a tarball the current `.projeny` no longer does; nothing
staled while a setup journal exists, with journal recovery then succeeding
via the snapshot), the workdir-present-but-no-status adopt-or-refuse rule
(setup unpacks into an existing directory that holds nothing it would
overwrite, keeping the foreign files; it refuses with the offending paths
listed otherwise), and snapshot copy-on-fallback from the tarball (by
`setup` and `status`). It prints
`ok`/`FAIL` lines with a `passed/failed` summary and exits nonzero on
any failure. A conflicting `setup` exits 1 (with markers left behind),
so the suite asserts `expect_fail` for every conflict-leaving setup.
