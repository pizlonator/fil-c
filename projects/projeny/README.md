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

Every command that names a project takes a project argument (relative or
absolute): the `.projeny` file itself, the work tree (or any directory
holding exactly one `.projeny` file), or a path whose `<arg>.projeny`
sibling exists — typically the checkout directory before it was ever
created, or a bare name like `lua` for `lua.projeny`. The work-tree
sibling rule wins over the scan: a directory sitting next to a
`<dir>.projeny` file IS that project's workdir, even when it holds stray
`.projeny` files of its own. Relative arguments
are lexically normalized first, so from inside the work tree `.` names
the project and, from a work tree subdirectory, `..` does too; an
argument that exists on disk is resolved physically (symlinks and all),
so `sym/..` through a symlinked intermediate names the directory the
symlink really leads to. The
tarball is
looked up next to the `.projeny` file — or, for a URL:-based project,
downloaded and verified (see the `.projeny` format section below) — and
the work tree is created next
to it as well (named by the `Name:` header).

```
projeny setup <f.projeny|dir> [...]         unpack archive(s), apply patch(es), in parallel
projeny commit <f.projeny|dir>              fold workdir changes into the patch
projeny add <f.projeny|dir> <path>          mark a file as added
projeny rm <f.projeny|dir> <path>           delete a file, mark as removed
projeny mv <f.projeny|dir> <src> <dst>      rename a file, mark as renamed
projeny resolve <f.projeny|dir> <path>      clear a conflict entry
projeny rebase <f.projeny|dir> <tarball>    point the project at a new tarball
projeny status <f.projeny|dir>              show setup/conflict/pending state
projeny diff <f.projeny|dir>                print a checkout's uncommitted diff
projeny diff <dir> <other-dir>              print the diff between two trees
projeny patch <dir> <patch-file>            apply a patch file to a tree
projeny package <f.projeny|dir> <out>       setup, then tar the tracked files; pairs run in parallel
projeny extract <f.projeny|dir> <dest>      setup, then copy tracked files; pairs run in parallel
projeny download <url> <hash> [...]         download URL/hash pairs into the cwd
projeny erase-setup <f.projeny|dir> [...]   delete the checkout + status file (DESTRUCTIVE); parallel
projeny freeze-mtime <f.projeny|dir> <file>...
projeny unfreeze-mtime <f.projeny|dir> <file>...
projeny list-frozen-mtimes <f.projeny|dir>
projeny get-attributes <f.projeny|dir> [<path or paths or directories>]
projeny hash <file>                         print the blake3 hash of a file
projeny help [command]                      show help (per-command with a name)
```

`setup`, `package`, and `extract` also accept several projects at once and
run them in parallel (`package`/`extract` take (project,
output/destination) pairs), `projeny download <url> <hash>...` fetches
URL/hash pairs into the current directory, and `projeny erase-setup`
erases several projects' setup state at once (see
[Erasing a setup](#erasing-a-setup)). `setup`/`package`/`extract`/`download`
are controlled with -j/--jobs and -c/--curl-jobs; `erase-setup` takes
-j/--jobs plus the --erase-snapshots and --force flags (it has no download
phase, so -c/--curl-jobs is an unknown option there); see
[Parallel setup, package, extract, and download](#parallel-setup-package-extract-and-download).

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
  expected tree from the status copy, diffs it against the workdir to find
  your uncommitted changes, and merges them onto a fresh setup of the
  *current* `.projeny` (which may name a different `Archive:` — e.g.
  upstream moved to a newer tarball). Merge failures leave conflict markers
  in the workdir and record the files in the status file. A setup that
  leaves conflicts still finishes (workdir, `.projeny` file, and status are
  all updated) but exits 1, so scripts under `set -e` stop instead of
  building from a conflicted tree; fix the files, `resolve` each one, and
  `commit`. The reporting is honest about whether a merge happened: a
  checkout with no local changes re-sets up fresh ("no local changes"),
  untracked files (never `projeny add`ed) merely ride along into the new
  tree ("no local changes; kept N untracked file(s)"), and a real merge
  prints one line per file — `merged:`, `added:`, `deleted:`, or
  `renamed:` — plus a `conflict:` line for every file left conflicted
  (exactly the status file's `Conflict:` entries).
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

- `diff <f.projeny|dir>`: prints the checkout's uncommitted change to stdout —
  the diff of the workdir against what a *fresh* `setup` of the current
  `.projeny` file would check out (tarball plus current patch). For
  ordinary edits this is also the patch `commit` would store; the two
  differ only when a pending `mv` renames a file that the last commit
  itself added or renamed (commit re-derives the rename from the tarball's
  paths: a committed-added file becomes a plain add, a committed rename
  re-traces to the original tarball path). See "The uncommitted diff"
  below for the exact semantics.
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
  for a `<dir>` workdir, naming) exactly one of them, or a path whose
  `<arg>.projeny` sibling exists (a missing checkout directory also works
  then). The archive holds a
  single top-level directory named after the output file, and compression
  is autodetected from its extension (`.tar`, `.tar.gz`/`.tgz`,
  `.tar.bz2`/`.tbz2`/`.tbz`, `.tar.xz`/`.txz`, `.tar.zst`/`.tzst`).
- `extract <f.projeny|dir> <dest-dir>`: like `package`, except the tracked
  files are copied into `<dest-dir>` instead of archived — the projeny
  variant of `extract_source`, for building outside the checkout. The
  destination must not exist or must be empty.
- `freeze-mtime <f.projeny|dir> <filenames...>`: pin the mtimes of tracked
  regular files to what the tarball wants (see
  [Frozen mtimes](#frozen-mtimes)). At the time the command runs, the
  checked out files are stamped with the archive member's mtimes, and the
  attribute is recorded in the `.projeny` patch as a `frozen-mtime <ts>`
  extended header so every later setup restores it.
- `unfreeze-mtime <f.projeny|dir> <filenames...>`: drop the attribute again
  (future setups stop re-stamping the file; the current mtime is left
  alone). A block that held nothing but the attribute is removed entirely.
- `list-frozen-mtimes <f.projeny|dir>`: one line per frozen file,
  `<workdir-relative path> <unix-epoch timestamp>`; hard-errors when
  nothing is frozen.
- `get-attributes <f.projeny|dir> [<path>...]`: print
  the special attributes (frozen mtime, nonstandard mode — see below) of
  the requested tracked files: all of them when no paths are given, a
  directory's subtree recursively, or a single file. Files without special
  attributes are not printed; a project with nothing to report prints
  nothing (the command is silent by design, unlike `list-frozen-mtimes`,
  which hard-errors when nothing is frozen). The mode line reports the
  current workdir mode, and a disappeared tracked file only ever reports
  a frozen mtime.
- `hash <file>`: prints the blake3 hash of a regular file as 64 lowercase
  hex chars and nothing else — the value to paste into a `URL: <url>
  <blake3-hash>` header of a [URL:-based](#projeny-format) `.projeny`
  file. Refuses anything that is not a regular readable file.
- `download <url> <hash> [<url> <hash>...]`: downloads every URL into the
  current directory (named after the URL's basename) as one parallel
  batch, verifying each against its blake3 hash; a file already present
  with a matching hash is kept instead of re-downloaded. See
  [Parallel setup, package, extract, and download](#parallel-setup-package-extract-and-download).
- `help [command]`: with a command name, prints a detailed explanation
  of that command.

## Frozen mtimes

A patched file gets a fresh timestamp when the patch is applied, which
makes timestamp-driven build machinery (autoconf comparing `configure.ac`
against a generated `tests/local.mk`, make comparing sources against
generated files) rebuild things it need not rebuild. `projeny freeze-mtime
<project> <filenames...>` pins a tracked file's checkout mtime to what the
tarball wants:

- At the time the command runs, the workdir file is stamped with the
  archive member's mtime.
- The attribute is recorded in the `.projeny` patch (and the status copy,
  which stays byte-identical to it) as an extended header inside the
  file's `diff --git` block, placed right after the `diff --git` line and
  before any `old mode`/`new mode` lines — the same region the parser
  scans for modes:

  ```
  diff --git a/lua/tests/local.mk b/lua/tests/local.mk
  frozen-mtime 1734567890
  ```

  The value is the tarball's own member mtime as unix-epoch seconds. A
  file may be frozen without any content or mode change: that stores an
  attribute-only block (`diff --git` line plus the `frozen-mtime` line,
  no hunks — the analogue of a mode-only block). Re-freezing updates the
  value; a delete block never carries the header, so a frozen entry dies
  with the file. A rename moves the pin: `commit` and `rebase` re-key the
  frozen entry to the rename's destination (keeping the original
  timestamp), so `freeze f`, `mv f g`, `commit` leaves `g` frozen. The
  `frozen-mtime` header is projeny-internal: plain `git apply` does not
  understand it and rejects attribute-only patches outright, while
  `patch -p1` applies the same patches fine.
- Every `setup` (and `rebase`, and the setups `package`/`extract` run)
  re-stamps frozen files after the workdir is in place, so a file the
  patch rewrote ends at the archive's mtime. `commit` and `rebase` keep
  the header in the regenerated patch and refresh the stored values from
  the archive (after a rebase: the *new* archive's members), so the
  invariant holds that a frozen value is always the archive's member
  mtime for that file. Every setup that can parse the `.projeny` file
  stamps frozen files — including setups that end in conflicts — so a
  frozen file keeps the archive's mtime even when its content ends up
  with conflict markers. Only a `.projeny` file that itself contains git
  conflict markers defers stamping to the next clean setup.
  `freeze-mtime` itself re-stamps all frozen files of the project, not
  just the ones it names.
  `package`/`extract` then carry those mtimes into
  output tarballs / extracted trees, because tracked files are staged
  with their times preserved.
- `unfreeze-mtime` removes the attribute (a block that held nothing but
  the attribute is dropped whole) and leaves the current file mtime
  alone; `list-frozen-mtimes` prints `<path> <ts>` per frozen file.
- `get-attributes` reports the special attributes of tracked files:
  `frozen-mtime <ts>` and a nonstandard mode — `mode 100755` for a
  regular file with any exec bit (symlinks are standard and never
  reported). Files with no special attribute are not printed.

The file arguments of `freeze-mtime`/`unfreeze-mtime`/`get-attributes` use
the same forms as `add`/`rm`/`mv` (CWD-relative, absolute, or
workdir-relative `<Name>/...`). Freezing refuses directories, symlinks,
untracked files, and pending (uncommitted) adds; a file the committed
patch adds (no archive member) freezes at its current workdir mtime. The
mutating commands require the `.projeny` file to match the status copy
and refuse while conflicts are pending, like `commit`.

## The uncommitted diff

`projeny diff <f.projeny|dir>` prints the uncommitted change of a checkout: the
diff of the workdir against what a fresh `setup` of the current `.projeny`
file would check out — the tarball plus the current patch. It takes the
same refusals `commit` does (the `.projeny` file must match the status
copy, conflicts must be resolved, pending ops must match the workdir).

The diff and `commit` answer different questions about the same workdir:
the diff is relative to the checked-in tree (what a fresh `setup` would
produce), while the stored patch is relative to the raw tarball. For
ordinary edits they are the same patch. They differ only when a pending
`mv` renames a file that the last commit itself added or renamed: the
diff describes the move against the checked-in paths, while `commit`
re-derives the rename from the tarball's paths — a committed-added file
commits as a plain add of its new name, and a committed rename re-traces
to the original tarball path. Each output is correct for its own baseline.

Which changes appear is decided by the pending ops, the same way `commit`
decides:

- Files added with `projeny add` (and `projeny mv` destinations) show as
  `new file mode` blocks; files removed with `projeny rm` (and `projeny mv`
  sources) show as `deleted file mode` blocks. `add`/`rm` take directories,
  and a directory entry covers everything under it.
- `projeny mv` renders as a `rename from`/`rename to` block — always, even
  when the moved file's content diverged beyond the rename similarity
  threshold. Chained moves (`mv a b`, then `mv b c`) collapse into a single
  rename `a -> c` with no mention of `b`, and moving a directory renames
  each contained file.
- Everything untracked — created without `projeny add`, whether text,
  symlink, or binary — stays out of the diff, like git leaves untracked
  files out of commits (one exception: files created inside a directory
  that a pending `mv` moved — the move destination is registered on the
  add side, so new files under it ride along in the diff and in the
  commit).
- A tracked file deleted *without* `projeny rm` is not part of the diff
  either (the next `setup` would restore it); each such file instead
  produces a warning on stderr naming the `projeny rm` command that would
  record it, so an otherwise empty output cannot masquerade as "no local
  changes".

The exit status is 0 whenever the diff itself succeeds — even when it
prints changes or warnings. stdout carries only the patch; warnings go to
stderr. Combined with `projeny patch`, the diff moves a change between
checkouts:

```
projeny diff mylib.projeny > /tmp/uncommitted.patch
# ...in a second checkout of the same .projeny file:
projeny patch mylib /tmp/uncommitted.patch
```

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
workdir-vs-status changes on top; the final report lists each merged
file (`merged:`/`added:`/`deleted:`/`renamed:` plus `conflict:` lines),
or says there was nothing to merge when the local side is empty. Which
side is upstream is auto-detected:
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

Instead of an `Archive:` header, the tarball may be fetched from the
network with one or more `URL:` headers (no tarball checked into git):

```
URL: http://gondor.apana.org.au/~herbert/dash/files/dash-0.5.13.5.tar.gz 7871678c86c4fda68266f79b4914d38792a8c4604e221cc3fc8d901da21560a1
Origname: dash-0.5.13.5
Name: dash
```

Each `URL:` line is exactly `URL: <url> <blake3-hash>` — the tarball's URL
and the blake3 hash of its bytes (compute the hash with
`projeny hash <file>`). The URL lines are mirrors and are tried in the
order listed: a download that fails or does not match its hash prints a
warning and the next line is tried, and it is a hard error only when no
URL yields a download matching its recorded hash. Every download is
announced on stderr (`projeny: downloading '<url>'` — the only line that
names the URL), with short progress lines (`projeny: download progress:
...`, sized to fit an 80-column terminal) printed every 64 KiB received —
capped at whole-percent steps when the total size is known, so a 70 MB
tarball reports ~100 updates, not ~1100. Progress lines end with a bare
carriage return and use no other terminal tricks, so a terminal redraws
the line in place while a run captured to a log retains every one. A
verified download reports `blake3 hash verified` and the snapshot it
wrote; a mismatched one warns and falls through to the next URL. The
archive name (and the snapshot's name) is derived from the URL's basename
(after stripping any scheme, `?query`, and `#fragment`), so
the URL must name the tarball file itself. `Archive:` and `URL:` headers
are mutually exclusive; see "Archive snapshots" below for how the download
is cached and verified.

Headers (`Origname:`, `Name:` — required, plus exactly one of `Archive:`
or at least one `URL:` line; extra headers are
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

A `.projeny` file may end without a trailing newline (common in
hand-written files); the prose is preserved byte-for-byte on round-trip,
and a commit or rebase that stores no patch (a no-op) keeps the file
byte-identical. When a patch is (re)written, it always starts on its own
line: projeny adds exactly one newline after prose that lacks it, so the
first `diff --git` line can never glue onto the last prose line (gluing
would make the next parse treat the whole patch as prose and silently
drop it).

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

For a URL:-based project the snapshot IS the archive's local copy: the
tarball is not checked into git anywhere, so `setup` downloads it (linked-in
libcurl, no `curl` subprocess) and stores it as `.<archive>.snapshot` next
to the `.projeny` file, where `<archive>` is derived from the URL's
basename. Before any network access, the existing snapshot is verified
against the URL: hashes: while it matches at least one of them, it IS the
archive and nothing is downloaded; only a missing snapshot — or one that no
longer matches any hash (a tampered or truncated file) — triggers a
re-download, and a mismatching snapshot is warned about and replaced. Each
`URL:` line is tried in order (a download that fails or does not match its
hash warns and falls through to the next mirror), and the first verified
download replaces the snapshot atomically. When the snapshot already
matches, the commands that materialize the archive (`setup`, `commit`,
`package`, `extract`) say so — `using existing snapshot '<path>' (blake3
hash matches); skipping the download` — once per run, no matter how many
times the same archive is materialized, while the read-only reconstruction
paths (`status`, `diff`, `get-attributes`, `freeze-mtime`) stay silent
about a matching snapshot unless they actually have to download. Moving the
project to a new tarball is done by editing the URL: header(s) to the new
URL and hash (compute it with `projeny hash <file>`) and running `setup`,
which re-downloads and merges local changes onto the new base; `rebase`
refuses URL:-based projects.

## Parallel setup, package, extract, and download

`setup`, `package`, and `extract` accept any number of projects and run
them in parallel:

```
projeny setup projects/foo.projeny projects/bar.projeny projects/baz.projeny
projeny package foo.projeny foo.tar.gz bar.projeny bar.tar.gz
projeny extract foo.projeny dest-foo bar.projeny dest-bar
```

`package` and `extract` take (project, output/destination) pairs. Two
options control the parallelism; they may appear anywhere among the
arguments, and are accepted by these three commands and `download` only
(every other command keeps its exact argument shape):

- `-j N` / `--jobs N` (also spelled `-jN` or `--jobs=N`) caps both the
  per-project threads and the parallel blake3 hash checks at N; the
  default is the CPU count.
- `-c N` / `--curl-jobs N` (also spelled `-cN` or `--curl-jobs=N`) caps
  the curl transfers in flight at N; the default is 8.

Naming one project twice collapses into one operation — keyed on the
resolved `.projeny` file, so two spellings of the same project (the
checkout directory and the file, say) dedupe as well — with a warning
naming the dropped argument (`'a.projeny' is listed more than once;
setting it up only once`; the `package`/`extract` forms name the ignored
output/destination). Two DIFFERENT projects whose package outputs or
extract destinations resolve to the same path are warned about too
(`package outputs 'x.tar.gz' and './x.tar.gz' both resolve to
'/abs/x.tar.gz'; the parallel runs write the same file`), once per
colliding group. One project's failure does not stop the others:
each project's report is labeled with its file name (`[f.projeny] ...`),
a project whose archive could not be obtained dies with `'<Name>'
needs archive '<pkg>', but the parallel download phase failed
to obtain it`, and a failed run ends with one summary line
(`projeny: 1 of 3 setup(s) failed: f.projeny`) and a nonzero exit —
after every other project finished. A single project runs the plain
single-project command, byte-identically: no planning phase, no labels,
no summary.

The download work is separated from the per-project work entirely. Every
named `.projeny` file is read first and all of its `URL:` headers are
collected; the downloads then run as one batch — the curl multi API with
at most `-c` transfers in flight, never two concurrent transfers of the
same package, blake3 checks on at most `-j` threads, and one full retry
pass over whatever still failed (parallel retries when several packages
failed) — and only then do the projects themselves run, on at most `-j`
threads.

Because several files may want the same archive (the same URL basename),
the batch is deduplicated by archive name: a shared archive downloads
exactly once, and every file asking for it receives the same downloaded
archive file. The candidate URLs tried for a shared package are the URLs
all of its contributors have in common first (the remaining mirrors
after that). Two loud warnings cover the conflicts this can hide:

- files downloading archives with the same name but from DIFFERENT URL
  sets get `WARNING: N PROJENY FILES DOWNLOAD ARCHIVES WITH THE SAME
  NAME '<archive>' BUT WITH DIFFERENT URL SETS: ...` (naming the files;
  the warning ends `DOWNLOADS ARE DEDUPLICATED BY ARCHIVE NAME, SO EVERY
  ONE OF THEM WILL GET THE SAME ARCHIVE FILE.`), and
- the same URL listed with different blake3 hashes warns even louder:
  `WARNING: URL '<url>' IS LISTED WITH DIFFERENT BLAKE3 HASHES (<hash>,
  <hash>) IN <files>. THIS IS ALMOST CERTAINLY A MISTAKE. ALL OF THESE
  FILES WILL RECEIVE THE SAME DOWNLOADED ARCHIVE.`

Both only warn — the download is deduplicated either way. A project
whose snapshot already verifies against one of the hashes its archive
was listed with needs no download at all. Each attempt is announced
(`projeny: downloading '<archive>' from '<url>'`), a verified one
reports `downloaded '<archive>' (<N> bytes); blake3 hash verified`, and
a failing batch announces `retrying <k> failed download(s): <names>`
before the retry pass. The whole batch shares ONE combined progress line
(`projeny: download progress: <e1> <e2> ...`, redrawn in place like the
single-project line): one single-token entry per package the pass has
announced so far, in first-announcement order — entry N is the Nth
announcement, and the roster only ever grows, a completed transfer staying
listed at `100%`. An entry is the transfer's whole-percent (`N%`) when its
total size is known, otherwise the bytes received so far in compact units
(`0B`, `65535B`, `37KiB`, `1.2MiB`) — never `?`. Renders are throttled: at
most one per scheduler iteration, only after at least 200ms, and only when
some entry's rendered text changed. Each
download round closes with one deterministic final progress line listing
every package it announced — `100%` per transferred package, `0B` for one
that never finished one — on its own line. The per-project phase of a
parallel command stays silent about snapshots: a `using existing
snapshot` note belongs to single-project runs only, since in parallel
mode the batch phase has already said what it downloaded (the
single-project download keeps its own progress form, see
[`.projeny` format](#projeny-format)).

`projeny download <url> <hash> [<url> <hash>...]` runs any number of
URL/hash pairs through the same machinery and writes each verified
download into the current directory, named after the URL's basename
(`https://foo.dev/foo-1.2.3.tar.gz` is saved as `foo-1.2.3.tar.gz`).
The hashes are the same 64-hex-char blake3 values a `URL:` header wants
(`projeny hash <file>` computes them; uppercase hex is accepted). A file
already present with a matching hash is kept instead of re-downloaded
(`already have <name> (blake3 hash verified)`); a verified download
reports `wrote <name> (<N> bytes)`. Pairs sharing an archive basename
deduplicate exactly like the setup batch above (exact duplicate URL/hash
pairs collapse silently), and any failure — after every other package
finished — exits nonzero.

## Erasing a setup

`projeny erase-setup <f.projeny|dir> [...]` deletes what a `setup` created
for each named project, in parallel (at most `-j/--jobs` threads, default
the CPU count; options may appear anywhere among the arguments). This is
DESTRUCTIVE and cannot be undone — the checkout is discarded whole,
uncommitted changes included. Without `--force`, erase-setup refuses to
erase anything when a project's `status` reports changes a commit would
fold in (see
[The no-force check](#the-no-force-check-uncommitted-changes-are-protected-by-default)
below); with `--force` it erases unconditionally:

- the checkout directory — the workdir named by the `Name:` header, e.g.
  `lua/` for `lua.projeny` — removed recursively, exactly like
  `rm -rf lua/` (note the project argument's stem is irrelevant: a
  `weird.projeny` whose header says `Name: realname` erases `realname/`),
- the status file `.<f>.projeny.status`, plus the legacy undotted
  `<f>.projeny.status` when it exists (both names, so nothing survives
  under either form),
- the crash-recovery setup journal `<f>.projeny.setup-journal`, deleted
  silently (it is not worth a warning when absent, and leaving it would
  make the next `setup` run crash recovery against a checkout that no
  longer exists).

The `.projeny` file itself, the checked-in `Archive:` tarball, and
everything else next to them are left alone. With `--erase-snapshots`, the
`.<archive>.snapshot` file the next setup would pick up is deleted too —
for a URL:-based project the snapshot named after the first URL's
basename, for a classic project the snapshot of the `Archive:` tarball,
plus the legacy undotted `<Archive>.snapshot` form (setup would otherwise
migrate it back into use). ONLY that exact snapshot goes: a similarly
named snapshot for a different version (the `.<old-archive>.snapshot` a
rebase left behind, say) survives, and the checked-in tarball is never
touched. The next `setup` then re-downloads (URL: projects) or unpacks
from the checked-in tarball (Archive: projects).

Missing things are not errors: a checkout that is already gone or a status
file that does not exist only prints a warning (`'<path>' did not exist;
nothing to erase`) and counts as success, so erasing twice is harmless.
Anything that cannot be deleted prints an error naming the path (with the
errno detail) and fails that project — but erase-setup still tries to
finish the rest of that project's deletions and the other projects': a
failed run ends with the usual one-line summary
(`projeny: 1 of 2 erase-setup(s) failed: f.projeny`) and exit status 1
after everything else finished. Each successful project reports one line
naming what it erased (`erased setup state for 'lua' (checkout 'lua',
status '.lua.projeny.status'[, snapshot '.lua-5.4.7.tar.bz2.snapshot'])`);
a project with nothing left reports `nothing to erase for '<Name>'`.

The `.projeny` file must be readable and parseable — it names what would
be deleted — so a missing, garbage, or git-conflicted file fails its own
project without anything being erased for it. With `--force` the other
projects still erase; without it, the whole invocation refuses before
erasing anything (see below). Naming one project twice collapses into a
single erase with a warning
(`'a.projeny' is listed more than once; erasing it only once`), keyed on
the resolved `.projeny` path like every parallel command. There is no
`-c/--curl-jobs` for erase-setup: it never downloads anything, so that
spelling is rejected as an unknown option.

### The no-force check: uncommitted changes are protected by default

By itself (without `--force`), erase-setup refuses to erase anything until
every named project has passed a check: in parallel (subject to
`-j/--jobs`), each project is asked whether a commit would have anything
to do — that is, whether `projeny status` reports anything other than
untracked files. Any `Conflict:`, `Added:`, `Removed:`, `Renamed:`,
`Modified:`, or `Disappeared:` entry makes the project dirty; untracked
files alone do not (they are not changes a commit would fold in, and they
go with the checkout as they always have).

If ANY project is dirty, the whole invocation refuses with one error and
exit status 1, and NOTHING is erased — not even the clean projects' setup
state:

```
projeny: error: refusing to erase 1 of 2 project(s) with uncommitted changes (use --force to erase anyway)
  'lua.projeny': modified: 'src/luaconf.h'; added: 'notes.txt'
```

The check is all-or-nothing across the invocation, and each dirty project
is named in argument order with its changes in status's own vocabulary.
A project whose state cannot even be assessed refuses the whole
invocation the same way: a missing, garbage, or git-conflicted `.projeny`
file (`cannot check 1 of 2 project(s) for uncommitted changes; refusing
to erase anything (use --force to erase anyway)`), a status file that
cannot be read or parsed, or a checkout whose archive and its snapshot
are both missing or unusable — the live diff that would compare the
checkout against the recorded tree cannot run, and a project that cannot
be checked is never assumed clean, so even a checkout that happens to be
clean refuses (erase-setup cannot know it is clean; `--force` is the
override). A checkout directory that is already gone — or a project with
no status file — is clean by definition: there is nothing there to
destroy, so it erases normally (with the usual did-not-exist warnings),
and the check never renames anything to `.stale` behind the user's back.

`--force` skips the check entirely and restores the unconditional
erasure: every named project is erased no matter what its status reports
— the escape hatch for deliberately discarding uncommitted work. It
combines freely with `--erase-snapshots`.

## File naming and migration

Two bookkeeping files live next to the tracked files, and both are hidden
(dot-prefixed) so they never clutter directory listings or accidental
checkins:

- `.<f>.projeny.status` — the status file for `f.projeny` (e.g.
  `.lua.projeny.status`), and
- `.<Archive>.snapshot` — the snapshot copy of an archive (e.g.
  `.lua-5.4.7.tar.bz2.snapshot`). For a URL:-based project the same name
  (derived from the URL's basename) holds the verified download.

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
while files rewritten by patch application get fresh timestamps — except
files with a [frozen mtime](#frozen-mtimes), which are re-stamped to the
archive's mtime after every setup.

libcurl is the only external library linked into the binary (never invoked
as a subprocess): it (the `curl_easy` API) downloads the tarball of a
URL:-based project, and it must be installed with its dev package
(`libcurl4-openssl-dev` on Debian/Ubuntu; linked with `-lcurl`). blake3
(the `blake3_hasher` API) verifies downloads and backs `projeny hash`; it
is vendored as plain portable C in `src/blake3/` (upstream 1.8.7 files,
SIMD disabled, CC0-licensed — see `src/blake3/LICENSE_CC0` and
`src/blake3/README`) and compiled in by the Makefile, so no blake3 package
is needed.

## Build and test

```
make -j$(nproc)     # builds ./projeny with $(CXX) (default g++)
make test           # builds (if needed) and runs tests/run_tests.sh
make clean          # removes the binary and all .o/.d files
```

The Makefile honors `CC` and `CXX` overrides (the projeny sources are C++
and the vendored blake3 is C; build_projeny.sh passes both spellings) and
generates header dependencies (`-MMD -MP`) so parallel builds work. The
code is warning-free with `-Wall -Wextra` under both system `g++`/`cc` and
the Fil-C compiler:

```
make clean && make CC=$(pwd)/../../build/bin/clang CXX=$(pwd)/../../build/bin/clang++ -j$(nproc) && make test
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

After `./build_all_fast.sh`, `build/bin/clang` and `build/bin/clang++`
exist at the repo root. Build and test projeny with them (absolute `CC`
and `CXX` paths, from this directory — both are needed, since the vendored
blake3 is C and the rest is C++):

```
make clean BUILD_DIR=build-filc
make CC=/path/to/fil-c/build/bin/clang CXX=/path/to/fil-c/build/bin/clang++ BUILD_DIR=build-filc -j$(nproc)
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
