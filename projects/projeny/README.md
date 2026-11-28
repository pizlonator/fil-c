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

The `.projeny` file names its base one of three ways: a checked-in
tarball (`Archive:`), a network fetch (`URL:` lines), or — for a
project layered on top of another projeny-managed project — another
`.projeny` file (`Projeny:`), whose committed extraction is the base
(see [`.projeny` format](#projeny-format) and
[Projeny-based projects](#projeny-based-projects)).

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
`.projeny` files of its own — but only when the file says so: its
`Name:` header must equal the directory's basename, since the checkout
is always named by `Name:`. On mismatch the command refuses
(`'<dir>.projeny' names the checkout directory '<other>', not '<dir>'`);
a sibling that cannot be parsed keeps the old behavior, so a
git-conflicted `.projeny` still recovers through `setup`. Relative arguments
are lexically normalized first, so from inside the work tree `.` names
the project and, from a work tree subdirectory, `..` does too; an
argument that exists on disk is resolved physically (symlinks and all),
so `sym/..` through a symlinked intermediate names the directory the
symlink really leads to. The tarball is looked up next to the
`.projeny` file — or, for a URL:-based project, downloaded and verified
(see the `.projeny` format section below), or, for a Projeny:-based
project, replaced by another `.projeny` file's committed extraction (see
[Projeny-based projects](#projeny-based-projects)) — and the work tree
is created next to it as well (named by the `Name:` header).

```
projeny setup <f.projeny|dir> [...]         unpack archive(s), apply patch(es), in parallel
projeny commit <f.projeny|dir>              fold workdir changes into the patch
projeny add <f.projeny|dir> <path>          mark a file as added
projeny rm <f.projeny|dir> <path>           delete a file, mark as removed
projeny mv <f.projeny|dir> <src> <dst>      rename a file, mark as renamed
projeny resolve <f.projeny|dir> <path>      clear a conflict entry
projeny rebase <f.projeny|dir>              re-extract a Projeny:-based project's committed base
projeny rebase <f.projeny|dir> <base.projeny>      ...or switch to a different .projeny base file
projeny rebase <f.projeny|dir> <tarball>    point the project at a new tarball
projeny rebase <f.projeny|dir> <url> [<hash>]...   ...or at new URL: header(s)
projeny create <f.projeny> <base.projeny>   make a new .projeny file based on another one
projeny create <f.projeny> <tarball>        make a new .projeny file and set it up
projeny create <f.projeny> <url> [<hash>]...   ...or from new URL: header(s)
projeny status <f.projeny|dir>              show setup/conflict/pending state
projeny diff <f.projeny|dir>                print a checkout's uncommitted diff
projeny diff <dir> <other-dir>              print the diff between two trees
projeny patch <dir> <patch-file>            apply a patch file to a tree
projeny apply <f.projeny|dir> <patch-file>  apply a patch inside a checkout
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
phase, so -c/--curl-jobs is an unknown option there); `create` takes
--comment, --origname, --force, and --erase-snapshots; see
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
  `commit`. That conflicted landing is not limited to setups with a status
  file to merge against: a fresh setup of a
  [Projeny:-based project](#projeny-based-projects) whose patch conflicts
  with its base chain's committed extraction (the chain may have moved
  under it — see
  [Chains and rebasing an upstream link](#chains-and-rebasing-an-upstream-link))
  lands the same way instead of dying: the patch is applied with inline
  `<<<<<<< current` / `>>>>>>> patched` markers, the conflicts are
  recorded in the status file, and the run reports `projeny: set up
  '<workdir>' from '<base>' with N conflict(s):` — spelled `...(into
  existing directory) with N conflict(s):` when an existing directory was
  adopted — followed by `projeny: setup left conflicts (exit 1)` and the
  usual fix-the-files, `resolve`-each-one, `commit` instruction. Classic
  `Archive:` and
  URL: setups keep the hard die on an unapplicable patch (`cannot apply
  patch in '<f>' (archive '<a>'): patch does not apply cleanly`). The
  reporting is honest about whether a merge happened: a
  checkout with no local changes re-sets up fresh ("no local changes"),
  untracked files (never `projeny add`ed) merely ride along into the new
  tree ("no local changes; kept N untracked file(s)"), and a real merge
  prints one line per file — `merged:`, `added:`, `deleted:`, or
  `renamed:` — plus a `conflict:` line for every file left conflicted
  (exactly the status file's `Conflict:` entries).

  A [Projeny:-based project](#projeny-based-projects) runs every flow
  above unchanged — fresh setup, re-setup, the three-way merge of local
  changes, conflicted-setup journal recovery, untracked carry-along —
  except that its base is the frozen base file's committed extraction,
  and the progress lines say `from '<base file>'` (the `Projeny:` value)
  where an archive name would appear. A circular `Projeny:` chain and an
  `Origname:` that does not match the base extraction's top-level
  directory are hard errors, and a setup whose base file and its frozen
  snapshot are both missing dies with recovery guidance naming both
  files (all quoted in
  [Projeny-based projects](#projeny-based-projects)). When the frozen
  base chain no longer reconstructs the recorded base and the patch no
  longer applies onto the moved content — the aftermath of an upstream
  link being rebased — the read-only consumers of the checkout die with
  per-command recovery guidance (`status` while nothing is conflicted;
  once the recovery lands conflict markers, `status` prints the recorded
  state and exits 0 instead) while `rebase` recovers from the workdir
  as-is (all quoted in
  [Chains and rebasing an upstream link](#chains-and-rebasing-an-upstream-link)).
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
- `rebase`: requires a clean tree (workdir matches the current patch)
  and no pending conflicts. If never set up, it runs `setup` first. The
  classic clean-tree law still governs every form of an `Archive:`- or
  `URL:`-based project — their base bytes are frozen (the archive is
  checked in, the URL snapshot is hash-pinned), so any mismatch is
  uncommitted work (`workdir '<path>' has uncommitted changes; commit or
  revert before rebasing`). A
  [Projeny:-based project](#projeny-based-projects) decides by its
  frozen chain instead, in every form (the zero-arg and base-switch
  forms, and the conversions off the mode): when the chain's expected
  tree no longer builds (its patch no longer applies onto the moved
  content — an upstream link was rebased), or when the tree builds but
  the recorded base has moved under the files the patch touches, the
  rebase proceeds from the workdir as-is: the workdir is reverse-patched
  into the old merge base, serves as the local side of the three-way
  merges, and the merged output stands everywhere — inside the hunk
  pre-images it is trusted (the reverse's success proves the workdir
  matched base+patch exactly there), outside them the moved chain's
  content wins, with every file it displaced or replaced flagged in
  `Conflict:` (the decision tree and the per-file classes are enumerated
  in [Chains and rebasing an upstream link](#chains-and-rebasing-an-upstream-link)).
  When the tree builds and every touched file matches it — the mismatch
  lies entirely outside the patch's file set — the Projeny:-based forms
  refuse with the decision tree's residual message (quoted in the same
  section).
  It applies the current patch onto the new base, rewrites
  the archive-location headers (the zero-arg Projeny form changes no base
  header — it re-extracts the base the `Projeny:` header already names),
  regenerates the patch, and moves the
  result into place; conflicts leave markers and are recorded in status.
  Pending add/rm/mv operations are preserved across the rebase (they are
  workdir-relative intent, still valid against the new base), matching how
  `setup` merges keep them. The new-archive arguments take one of several
  forms, decided by the FIRST argument: whether it is a URL is decided by
  asking libcurl's URL parser (the `curl_url` API — the same parser that
  would fetch it), so plain filenames, including `./x.tar.gz`, are never
  mistaken for URLs while `http://`/`https://`/`ftp://`/`ftps://`/`file://`
  arguments are; a name ending in `.projeny` starts the Projeny-base form,
  and an EMPTY argument list is the zero-arg form (valid only for a
  Projeny:-based project — see
  [Projeny-based projects](#projeny-based-projects)).

  - `rebase <f>` (no further argument) re-extracts a Projeny:-based
    project's committed base: what the file the `Projeny:` header names
    holds NOW — its own committed extraction — not the frozen snapshot
    the last setup froze. This is how committed changes to the base file
    flow into the project (see
    [Projeny-based projects](#projeny-based-projects)): commit changes
    in `foo.projeny`, then `projeny rebase projects/bar.projeny` moves
    bar's patch on top. The standard rebase mechanics apply (per-file
    three-way merge, `Origname:` refreshed from the base's own parse,
    conflicts leave markers and the
    `projeny: rebased onto '<x>' with N conflict(s):` report), and the
    frozen `.<stem>.snapshot` is OVERWRITTEN with the base file's current
    bytes. A clean tree takes the normal path. The clean-tree
    requirement is waived when the frozen base chain no longer
    reconstructs the recorded base: when the chain's expected tree no
    longer builds (the patch no longer applies onto the moved content)
    or the recorded base has moved under the files the patch touches,
    that state rebases from the workdir as-is instead; a mismatch that
    lies outside the patch's file set refuses with the residual message
    (both quoted in
    [Chains and rebasing an upstream link](#chains-and-rebasing-an-upstream-link)).
    Any other project refuses the form (`cannot rebase '<f>'
    without a new base argument: it is not a Projeny-based project (pass
    a tarball path or URL(s), or, for a Projeny-based project, no
    argument at all to re-extract its committed base)`), and a missing
    live base file dies (`base file '<path>' does not exist; it is
    needed to rebase '<f>'`).
  - `rebase <f> <base.projeny>` (one `.projeny` argument) points the
    project at another `.projeny` file's committed extraction — a base
    switch for a Projeny:-based project, a conversion to the
    Projeny:-based form for an `Archive:`-based one: the base must live
    in the SAME directory as `<f>`, must exist and parse, and must not
    close a cycle in the `Projeny:` chain (the live chain is pre-checked
    before anything is touched). The `Projeny:` header is rewritten,
    `Origname:` comes from the new base's own parse, the patch moves
    onto the new base's committed extraction, and the frozen snapshot is
    overwritten with the new base's bytes. A URL:-based project refuses
    this form like it refuses a tarball path.
  - `rebase <f> <new-tarball>` (one local tarball path) keeps the project
    Archive:-based: the tarball is copied next to the `.projeny` file and
    `Archive:`/`Origname:` are rewritten. If the new tarball's basename
    equals the current `Archive:` but its content differs (size/hash
    compare), projeny warns to stderr and proceeds with the new file — it
    never silently keeps the old bytes. A URL:-based project refuses this
    form (it has no checked-in tarball); rebase it with URLs instead. A
    Projeny:-based project converts to Archive:-based: the `Projeny:`
    header is replaced by the new `Archive:` line, and the old
    `.<stem>.snapshot` lingers as a stale file (stale snapshots are never
    auto-cleaned).
  - `rebase <f> <url> [<hash>] [<url> [<hash>]...]` (one or more URLs,
    each optionally followed by its blake3 hash — 64 hex chars, uppercase
    accepted, the value `projeny hash <file>` prints) rebases onto those
    URLs, which become the file's `URL:` headers, tried in order by later
    setups. An Archive:- or Projeny:-based project converts to URL:-based
    (the `Archive:`/`Projeny:` header is replaced; remove the old
    checked-in tarball from git by hand, and a converted Projeny:-based
    project's old `.<stem>.snapshot` lingers as a stale file), and a
    URL:-based project accepts only this form. Every
    listed URL is downloaded and verified during the rebase (each distinct
    URL once, in order): an omitted hash is computed from the download and
    reported (`projeny: computed blake3 hash <hash> for '<url>'`); a
    provided hash that mismatches the download is a hard error (the hash
    was asserted, so nothing is written); a hashless URL that cannot be
    downloaded is a hard error (no hash could be computed — pass the hash
    explicitly); and a hashed URL that fails to download only warns, the
    mirror staying listed so a later `setup` falls through it as usual. If
    no URL verifies, nothing is rebased and the `.projeny` file, status
    file, snapshot, and workdir are all untouched. The first verified
    download's bytes are written to the snapshot — named after the FIRST
    URL's basename, matching what the `.projeny` parse derives — and are
    the base the patch applies onto; the rest of the rebase flow (the
    clean-tree rules above, conflicts leave markers, `resolve` +
    `commit`) is unchanged.

- `create <f.projeny> <new-base-args> [--comment <text>] [--origname
  <name>] [--force] [--erase-snapshots]`: creates a brand-new `.projeny`
  file naming an archive (or basing itself on another `.projeny` file),
  then runs `setup` on it — a new project is one
  command away from a checked-out, patched work tree with a fresh status
  file. Options may appear anywhere among the arguments. The first
  argument is the `.projeny` file to create: it must end in `.projeny`
  (the command refuses otherwise) and may carry a directory path; the
  directory that would hold it (the pdir) must exist. Its name — the
  basename minus `.projeny` — is the default `Name:` header, so the
  checkout directory defaults to `<pdir>/<name>` next to the new file.
  The remaining arguments name the base with exactly `rebase`'s
  grammar, decided by the FIRST argument (libcurl's URL parser decides
  what a URL is; a name ending in `.projeny` starts the Projeny-base
  form; see the rebase bullet above):

  - `create <f> <base.projeny>` bases the new file on another
    `.projeny` file's committed extraction: the file gets a `Projeny:`
    header naming the base — a plain filename, so the base must live in
    the same directory as the new file — and `Origname:` comes from the
    base's own parse. Extra arguments after the `.projeny` file die
    (`create takes exactly one .projeny base file (no hash pairs);
    '<extra>' is extra`), `--origname` must equal the base's
    `Origname:` (the create refuses otherwise: `the base file '<p>'
    extracts to '<o>', not '<g>' (setup requires Origname: to match the
    base file's extraction)`), and a base whose live chain would close
    a cycle through the new file is refused before anything is written.
    The created-line reports `base '<foo.projeny>'` for this form, and
    the setup it runs freezes the base file's current bytes as the new
    project's `.<stem>.snapshot`.
  - `create <f> <new-tarball>` gives the file an `Archive:` header and
    copies the tarball next to it — rebase's exact behavior, including
    the warn-and-use-the-new-file rule when the copy's basename matches
    the replaced project's `Archive:` while its bytes differ.
  - `create <f> <url> [<hash>] [<url> [<hash>]...]` writes the URLs as
    the file's `URL:` headers (the project is born URL:-based — no
    tarball is checked into git). Every listed URL is downloaded and
    verified during the create, each distinct URL once: an omitted hash
    is computed from the download and reported, a provided hash that
    mismatches is a hard error, a hashless URL that cannot be downloaded
    is a hard error, and a hashed URL that fails to download only warns
    (the mirror stays listed). If no URL verifies, nothing is created.
    The first verified download's bytes are written to the snapshot,
    named after the FIRST URL's basename — the same derivation the
    `.projeny` parse applies — so the setup below downloads nothing.

  Headers are composed in the conventional order: the `Projeny:`/
  `Archive:`/`URL:` lines, then `Origname:`, then `Name:`, followed by
  the prose.
  `Origname:` is inferred from the archive itself: its single top-level
  directory (an archive holding several is a hard error) — or, for the
  `.projeny` form, taken from the base file's own `Origname:`.
  `--origname
  <name>` names the checkout directory instead: the created file gets
  BOTH `Name: <name>` AND `Origname: <name>` — the workdir is always
  named by `Name:`, so the checkout directory becomes `<pdir>/<name>`
  while the `.projeny` file stays `<f.projeny>` — and `<name>` must
  match the archive's top directory, or the base file's extraction for
  the `.projeny` form (the create refuses before anything
  is written otherwise). `--comment <text>` stores `<text>` as the
  file's free-text prose, indented like every tool-written `.projeny`
  file's comments; repeating the option keeps only the last text.

  When `<f.projeny>` already exists, `--force` decides what happens to
  the project it still names (the erase-setup semantics;
  `--erase-snapshots` passes through to every erase): with no `--force`
  the create refuses and nothing is touched; with one `--force` the
  existing project's setup state is erased, but only after the no-force
  check passes — a checkout with uncommitted changes or unresolved
  conflicts refuses with erase-setup's own wording (and so does a
  project whose state cannot be assessed); with `--force` twice the
  erase is unconditional and uncommitted work is discarded.

  A checkout directory named by `--origname` gets one more guard: a
  pre-existing `<pdir>/<name>` is refused without `--force`; at
  `--force` or above it is erased (at the same force level) only when it
  is attributable, and the file being replaced is considered first —
  when `<f.projeny>` itself parses and carries `Name: <name>`, the
  directory is its checkout and its own erase removes it, so a
  `--force` re-run of the same create works. Otherwise its sibling
  `<pdir>/<name>.projeny` must exist, parse cleanly, and carry
  `Name: <name>`, which is what makes the directory that project's
  checkout. A directory that cannot be attributed that way is NEVER
  removed, no matter how many `--force` flags are given (erase-setup
  `--force` also only erases named projects): the create refuses
  instead, naming the sibling it checked. An eviction never deletes the
  evicted project's `.projeny` file (projeny never deletes `.projeny`
  files), so the leftover file still carries `Name: <name>` and keeps
  claiming a checkout directory that now belongs to the new file: the
  create prints a warning naming the leftover and the fix, and the file
  should be removed or renamed if it is no longer wanted. Without
  `--origname`, a pre-existing directory named by `Name:` is left to
  setup's own adopt-or-refuse rule.

- `diff <f.projeny|dir>`: prints the checkout's uncommitted change to stdout —
  the diff of the workdir against what a *fresh* `setup` of the current
  `.projeny` file would check out (tarball plus current patch; the frozen
  base extraction plus current patch for a
  [Projeny:-based project](#projeny-based-projects)). For
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
  fuzz; plain unified diffs (no `diff --git` header) are accepted
  alongside git-style diffs, with or without a prose preamble (cover
  notes, diffstats, mail signatures around the blocks are ignored), and
  label paths are interpreted like `patch -p1` inside the tree (one `a/`
  or `b/` component stripped, remainder relative to the tree root — a
  leftover workdir-name component is stripped too). Prefix-free plain
  labels — the `--- old/f` / `+++ new/f` shape `diff -u` and recursive
  `diff -ruN` emit — resolve with the same -p1 fallback, guided by the
  tree: the whole label path when it exists in the tree, else one
  component stripped (so a genuinely tree-relative `--- src/a.c` stays
  whole); a block the tree cannot decide follows the patch's first
  decidable block, and when nothing is decidable a label whose first
  component is absent from the tree is treated as the emitter's own
  directory and stripped. GNU `diff -N`'s epoch timestamps
  (`1970-01-01 00:00:00 +0000` after the tab) mark the nonexistent side
  of an add/delete block: such a side reads as `/dev/null`, so those
  blocks apply as creations and removals — including removing a file the
  patch empties — exactly like GNU patch. GNU diff renders the stamp in
  the local zone, so west of UTC it reads `1969-12-31 16:00:00 -0800`;
  projeny accepts any spelling naming the epoch instant. A plain block
  whose two sides
  name different paths applies to the old-side name, like GNU patch
  (`patching file <old name>`): plain diffs never rename — only explicit
  `rename from/to` headers do. A `---`/`+++` pair
  with no `@@` hunk and no `/dev/null` side is prose, not a block, and a
  hunk-less `/dev/null` pair creates or deletes an empty file.
  Already-applied blocks are skipped (already-applied means the
  file already holds the hunk's exact post-image with every deleted run
  anchored — leading deleted runs, and slid matches of mid-hunk deleted
  runs, also require the file line above the post-image span not to be
  the run's last deleted line — plus the hunk's exact pre-image must not
  survive as a proper sub-window of the file — blank-adjacent shapes
  whose pre-image spans the whole file, and pure insertions, remain
  patch(1)-style heuristics).
  Unapplyable blocks become
  conflicts: markers (`<<<<<<< current` / `=======` / `>>>>>>> patched`)
  go inline and the conflicted files are listed on stdout (exit stays 0).
- `apply <f.projeny|dir> <patch-file>`: applies a patch file inside a
  projeny checkout, on top of the uncommitted changes it already has.
  The patch may be any git diff or any projeny diff (including `projeny
  diff` output; the workdir label, if any, is detected automatically).
  Plain unified diffs (no `diff --git` header) are accepted too, with or
  without a prose preamble (ignored); paths are interpreted like `patch
  -p1` inside the checkout (one `a/` or `b/` component stripped,
  remainder relative to the tree root — a leftover workdir-name component
  is stripped too; prefix-free plain labels, the `diff -u`/`diff -ruN`
  shape, fall back to a one-component strip guided by the checkout: the
  whole label path when it exists there, else one component stripped, so
  `diff -ruN oldtree newtree` output applies like `patch -p1`). GNU
  `diff -N`'s epoch timestamps mark the nonexistent side of an
  add/delete block, which then applies as a creation or a removal (a
  file emptied by an epoch-marked block is removed, like GNU patch; one
  emptied by an ordinary real-timestamp block stays on disk empty). GNU
  diff renders the stamp in the local zone, so west of UTC the epoch
  reads `1969-12-31 16:00:00 -0800`; projeny accepts any spelling naming
  the epoch instant. A
  plain block whose two sides name different paths applies to the
  old-side name, like GNU patch: plain diffs never rename — only
  explicit `rename from/to` headers do. A
  `---`/`+++` pair with no `@@` hunk and no
  `/dev/null` side is prose, not a block, and a hunk-less `/dev/null`
  pair creates or deletes an empty file (booked added/removed so `commit`
  folds it in).
  Unlike `patch`, the command also books the patch into the status file
  so a later `commit` folds it into the `.projeny` patch: files the patch
  adds are marked added, files it deletes are marked removed, renames are
  recorded as pending renames (chained through existing pending renames
  exactly like `mv`), and mode changes (exec bit, symlinks) are applied
  to the files. Already-applied blocks are skipped (the file must hold
  the hunk's exact post-image with every deleted run anchored — leading
  deleted runs, and slid matches of mid-hunk deleted runs, also require
  the file line above the post-image span not to be the run's last
  deleted line — plus the hunk's exact pre-image must not survive as a
  proper sub-window of the file — whole-file-pre-image blank shapes and
  pure insertions remain heuristic; setup applies .projeny patches fresh
  to the pristine tarball and never consults already-applied detection at
  all, and neither does rebase's check that the patch applies to its own
  base), so re-applying is idempotent.
  Unapplyable blocks get the same
  inline conflict markers and
  are marked conflicted in the status file, and the command exits 1 —
  resolve with `projeny resolve`, then `commit`. When the patch touches a
  file that is already marked conflicted nothing is applied at all (no
  filesystem change, no status write) and the command hard-errors.
  An empty patch file is a no-op; a non-empty patch with no diff blocks
  is a hard error. git base85 binary payloads (and payload-less "Binary
  files ... differ" stanzas) cannot be applied and become conflicts,
  while projeny's own base64 binary sections apply.
- `package <f.projeny|dir> <output-tarball>`: runs `setup` (so uncommitted
  workdir changes are included, and conflicts fail the command with a
  nonzero exit and no archive), then tars up exactly the tracked files —
  base (the archive, or a
  [Projeny:-based project](#projeny-based-projects)'s frozen base
  extraction) plus patch plus pending add/rm/mv ops, minus untracked
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
  mtime for that file (for a
  [Projeny:-based project](#projeny-based-projects) the member-mtime
  source is the frozen base extraction — the extraction's own file
  mtimes, so a file the base's own committed patch rewrote carries that
  application's timestamp). Every setup that can parse the `.projeny` file
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
checkouts (`projeny apply` is the checkout-aware variant: it also records
the patch's adds/deletes/renames in the status file, so a later `commit`
folds them in):

```
projeny diff mylib.projeny > /tmp/uncommitted.patch
# ...in a second checkout of the same .projeny file:
projeny patch mylib /tmp/uncommitted.patch
# ...or, booking the change for the next commit:
projeny apply mylib /tmp/uncommitted.patch
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
both sides name the same tarball — or the shared base file's committed
extraction, when two Projeny:-based sides name the same base file), and
re-applies any uncommitted
workdir-vs-status changes on top; the final report lists each merged
file (`merged:`/`added:`/`deleted:`/`renamed:` plus `conflict:` lines),
or says there was nothing to merge when the local side is empty. When
the frozen base chain of a Projeny:-based side no longer reconstructs
the recorded base, that reconstruction dies with the chain-moved
guidance of
[Chains and rebasing an upstream link](#chains-and-rebasing-an-upstream-link).
Which
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
URL yields a download matching its recorded hash. Before that hard error,
every listed line gets one more try: when all of a package's URL lines
have failed, projeny prints that it is waiting (`all N URL line(s) failed
for '<archive>'; waiting <W> second(s) before trying them one more time`),
waits, and then gives every line one more try in order — only a second
total failure dies, with the pass count in the report (`could not obtain
archive '<archive>' (tried N URL line(s) on 2 pass(es); every download
failed or did not match its recorded blake3 hash)`). The wait length is
the `PROJENY_URL_RETRY_SECONDS` environment variable, in seconds: 60 by
default; a non-negative integer is used as given, where 0 disables the
wait AND the retry pass entirely (the legacy single-pass behavior, with
byte-identical output); anything invalid — including a value above
2147483647 — falls back to 60. The same wait-and-one-more-try covers
single-project commands, `rebase`/`create`'s URL arguments (a second
walk re-attempts only the downloads that failed), and the `projeny
download` batch (one more parallel pass re-attempts exactly the packages
that failed). In a parallel run a
fallback download — a conflicted file's package, or a Projeny:-based
project's on-demand chain base — fails fast instead (the inline wait must
never stall the shared download lock), and the coordinator's single
wait-and-retry stage covers it, as the
[parallel setup section](#parallel-setup-package-extract-and-download)
documents below. Every download is
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
the URL must name the tarball file itself. `Archive:`, `URL:`, and
`Projeny:` headers are mutually exclusive (each pairing dies with its own
message — the two `Projeny:` pairings are quoted below); see "Archive
snapshots" below for how the download
is cached and verified.

Instead of an `Archive:` or `URL:` header, a `.projeny` file may base
itself on ANOTHER `.projeny` file with a `Projeny:` header — the third
base mode, for a project layered on top of another projeny-managed
project:

```
Projeny: foo.projeny
Origname: foo-1.2
Name: bar
```

The `Projeny:` value is a plain filename (no `/`, not `.` or `..`;
anything else dies with `file '<p>': Projeny: must be a plain filename`),
resolved in the SAME directory as the referencing file — the same search
law the `Archive:` tarball follows — so a base always lives next to the
file that names it. Exactly one of `Archive:`, at least one `URL:` line,
or a `Projeny:` line may appear: mixing `Projeny:` with `Archive:` dies
(`file '<p>' has both Archive: and Projeny: headers; they are mutually
exclusive (either check the tarball into git next to the .projeny file
and name it with Archive:, or base the project on another .projeny file
with Projeny:)`), and mixing it with `URL:` dies (`file '<p>' has both
URL: and Projeny: headers; they are mutually exclusive (either fetch the
tarball with one or more URL: lines, or base the project on another
.projeny file with Projeny:)`). A file with none of the three dies with
`file '<p>' is missing a required header (need Archive:, at least one
URL: line, or a Projeny: line, plus Origname: and Name:)`. A `Projeny:`
line with an empty value is refused in its own right (`file '<p>': empty
Projeny: value (name the base .projeny file after 'Projeny:')`), ahead of
the exclusivity and required-header checks — with `Archive:` or `URL:`
headers present it would otherwise pass as that mode, and without them it
would die as the vague missing-required-header above. `setup` reports the
same reason wrapped in its own refusal — the file, the reason above
spelled `has an empty Projeny: value`, and its usual note about not
touching the workdir or status file — while every other command's parse
dies with the bare form. What the base
contributes — the committed extraction, the frozen snapshot, chains, and
the rebase workflow — is described in
[Projeny-based projects](#projeny-based-projects).

Headers (`Origname:`, `Name:` — required, plus exactly one of `Archive:`,
at least one `URL:` line, or a `Projeny:` line; extra headers are
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
project to a new tarball is done by `rebase` with URL arguments
(`projeny rebase <f.projeny|dir> <url> [<blake3-hash>] ...` downloads and
verifies every listed URL, rewrites the URL: headers, and merges local
changes onto the new base); editing the URL: header(s) by hand (compute the
hash with `projeny hash <file>`) and running `setup` works too. A tarball
path argument, on the other hand, is refused for a URL:-based project: it
has no checked-in tarball to rebase onto.

### The frozen base snapshot of a Projeny:-based project

A [Projeny:-based project](#projeny-based-projects) keeps its base in a
snapshot too, but the name follows the REFERENCING file, not an archive:
`bar.projeny` keeps `.bar.snapshot` (dot + the file's basename minus
`.projeny` + `.snapshot`) next to `bar.projeny`, holding the byte-exact
bytes of its base file as-of when bar adopted it. The snapshot never
tracks the base file: every `setup`, `commit`, `status`, `diff`,
`package`, and `extract` of bar goes through it, so the checkout keeps
using the recorded base even after the base file gains committed changes
— or after the base file is deleted from git, when the frozen snapshot is
the only copy of the base left, exactly like an archive snapshot after
the tarball was `git rm`'d (when BOTH the live base file and the snapshot
are missing, setup dies with recovery guidance naming both files). It is
re-frozen (overwritten) only by the lost-snapshot fallback — any first
use while the snapshot is missing freezes the live base file's current
bytes into it, mirroring the classic archive-snapshot fallback — by a
`rebase` in either of its Projeny forms (the zero-argument
re-extract and the base switch), or by a setup that adopts a hand-edited
`Projeny:` header — one naming a different base file than the status
records, or one whose status records a base of ANOTHER mode entirely (a
hand-conversion back into the form; see
[Adoption: hand-editing the Projeny: header](#adoption-hand-editing-the-projeny-header)).
Every status-file write ensures the snapshot exists (created from the
live base file when missing) but never overwrites it.
`erase-setup --erase-snapshots` deletes exactly that snapshot file — this
mode has no legacy undotted snapshot form — and a workdir-gone setup
stales it like any other snapshot bookkeeping (see
[File naming and migration](#file-naming-and-migration), which also
covers the naming overlap with a classic project's archive snapshot when
a checked-in tarball is named exactly like this `.projeny` file's stem).

## Projeny-based projects

A `.projeny` file with a `Projeny:` header (see
[`.projeny` format](#projeny-format)) is based on the COMMITTED
EXTRACTION of another `.projeny` file in the same directory: unpack the
base file's archive (snapshot-preferred, exactly like every archive
read) and apply the base file's committed patch. The base file's own
workdir — its unpacked checkout with any uncommitted changes — is
ignored everywhere; "committed" means what a `projeny commit` of the
base project folded into the base `.projeny` file, and git is never
consulted. Chains compose: a Projeny:-based base is itself extracted
through its own chain, each link resolving ITS base through its OWN
frozen snapshot (see
[The frozen base snapshot of a Projeny:-based project](#the-frozen-base-snapshot-of-a-projeny-based-project)),
so chains may be arbitrarily long. A chain that closes on itself — a
self-loop (`Projeny: <this file>`) or any longer cycle — is a hard error
everywhere a chain is materialized, naming the whole walk
(`the Projeny: base chain is circular: '<a>' -> '<b>' -> '<a>'; Projeny:
chains must not contain cycles`), and `create`/`rebase` pre-check the
LIVE chain — each link's `Projeny:` header read from its live bytes —
seeded with the target file itself, before touching anything.

### Worked example

Say `projects/foo.projeny` is an ordinary `Archive:`- or URL:-based
project (`Origname: foo-1.2`), and `projects/bar.projeny` names it:

```
Projeny: foo.projeny
Origname: foo-1.2
Name: bar
```

`projeny setup projects/bar.projeny` extracts foo's committed content
(foo's tarball plus foo's committed patch — the top-level directory
stays `foo-1.2`), applies bar's own patch on top, checks the result out
as the workdir named by `Name:` (`projects/bar/`), and freezes
foo.projeny's current bytes into `.bar.snapshot` on the way. bar's
`Origname:` must equal the base extraction's top-level directory — the
base file's own `Origname:`; a mismatch is a hard error wherever the
tree is built (`file '<path>': Origname: is '<orig>' but the base
extraction's top-level directory is '<actual>'; fix the Origname: header
or rebase`). Organically `create` and `rebase` keep the header synced
with the base extraction, so this only fires for a hand-edited file
(and `rebase` self-heals it, refreshing `Origname:` from the new base).

From here bar is a fully ordinary project, and every setup flow works
unchanged — fresh setup, re-setup, the three-way merge of local changes,
conflicted-setup journal recovery, untracked carry-along (see
[Usage](#usage)) — except that the base is the frozen base extraction,
and the progress lines say `from 'foo.projeny'` where an archive name
would appear. `commit` diffs the workdir against the frozen base's FULL
extraction (the base's committed patch is part of the base, since bar's
patch sits on top of it); all other commit laws are unchanged.
`status`/`diff`/`package`/`extract` build their expected trees from the
frozen extraction, with `package`/`extract` running `setup` first as
usual — so they include bar's uncommitted changes but NOT foo's.
`freeze-mtime` takes its frozen-mtime values from the frozen base
extraction's file mtimes (a file the base's own committed patch rewrote
carries that application's timestamp, exactly as `commit`'s refresh
sees it), and
`get-attributes`/`apply`/`unfreeze-mtime`/`list-frozen-mtimes` work
unchanged.

### Adoption: hand-editing the Projeny: header

Editing bar's `Projeny:` header by hand to name a DIFFERENT base file
than the status-embedded copy names is adopted by the next `setup`: the
newly named base file's live bytes OVERWRITE the frozen snapshot and
become the base the fresh tree builds from (the reconstruction of what
the last setup used still sees the snapshot's old bytes, so bar's local
changes merge onto the new base). The same adoption fires when the
status-embedded copy is not Projeny:-based at all — the file was
converted away from the form (`rebase <f> <tarball>` or a URL rebase,
which leaves the old `.<stem>.snapshot` lingering; see
[Mode conversions](#mode-conversions)) and the header was then
hand-edited back to `Projeny: <other>.projeny`: without the adoption the
lingering pre-conversion snapshot would silently supply a stale base
while the header names a different one; with it, the checkout is rebuilt
from the newly named base's committed extraction and the stale snapshot
is overwritten with the new base's live bytes — replaced, not used. The
newly named base must exist
(`base file '<path>' does not exist; it is needed to <what>`), and
nothing is adopted without a status file: no status means no recorded
base to differ from, and the frozen-or-first-freeze law governs (see
[The frozen base snapshot of a Projeny:-based project](#the-frozen-base-snapshot-of-a-projeny-based-project)).

### Rebasing onto the base's committed changes

The point of the mode: committed changes to foo flow into bar on
demand, without bar's patch ever being touched by hand. Edit foo's
checkout, then run `projeny commit projects/foo.projeny` — foo.projeny's
patch now holds the change, but bar's checkout still sits on the frozen
base. Then run the zero-argument form of rebase:

```
projeny rebase projects/bar.projeny
```

This re-extracts the LIVE committed base — what foo.projeny holds NOW,
not the frozen snapshot — and moves bar's patch onto it with the
standard per-file three-way merge: a clean rebase reports
`projeny: rebased onto 'foo.projeny'`, a conflicting one exits 0 with
inline markers and `projeny: rebased onto 'foo.projeny' with N
conflict(s):` (fix the files, `resolve` each one, `commit`, as always).
`Origname:` is refreshed when the base's own differs, `.bar.snapshot`
is OVERWRITTEN with foo.projeny's current bytes, and the status embedded
copy is updated. The tree must be clean, as rebase always requires — the
exceptions being the moved-chain fallbacks described in
[Chains and rebasing an upstream link](#chains-and-rebasing-an-upstream-link),
where the frozen base chain no longer reconstructs the recorded base —
whether because its expected tree no longer builds or because the
recorded base moved under the patch's touched files — and the rebase
proceeds from the workdir as-is, converging onto the moved chain
conflict-free only when the upstream move stays inside the patch's hunk
pre-images (flagged otherwise), while a mismatch that lies outside the
patch's file set refuses with the decision tree's residual message. The
frozen snapshot cannot serve this form — picking up what the base file
holds now is its whole point — so a missing live base file dies
(`base file '<path>' does not exist; it is needed to rebase '<f>'`), and
every non-Projeny project refuses the form outright (`cannot rebase
'<f>' without a new base argument: it is not a Projeny-based project
(pass a tarball path or URL(s), or, for a Projeny-based project, no
argument at all to re-extract its committed base)`).

### Chains and rebasing an upstream link

Every link of a chain resolves its base through ITS OWN frozen snapshot
(see
[The frozen base snapshot of a Projeny:-based project](#the-frozen-base-snapshot-of-a-projeny-based-project)),
so a chain's committed extraction composes link by link and chains may be
arbitrarily long. That composition has a consequence for rebasing an
UPSTREAM link: re-freezing that link's `.<stem>.snapshot` (the
zero-argument rebase, a base switch, or an adoption setup — anything that
overwrites a link's frozen base) changes what the frozen bytes of every
DOWNSTREAM project extract to, while its workdir still holds the old
upstream state. A downstream project then sits in one of two states,
decided by whether its committed patch still applies onto the moved
extraction.

When the patch still applies (the moved regions do not collide with it),
the expected tree builds — it has simply moved: `projeny status` reports
the upstream deltas as `Modified:` entries, a dirty state that is not the
user's own work. Committing in this state folds the spurious deltas into
the downstream patch — permanently reverting the upstream change for this
project — so do not; a merge-path `setup` is no safer, because it takes
the workdir's divergence from the moved tree as the local side of its
merge and carries it into the tree as if it were ordinary uncommitted
work, which the next commit then folds in. Zero-argument `rebase` (and
the base-switch form) no longer refuses this state wholesale: it
classifies the workdir's mismatch from the moved tree first, building the
fallback candidate old side — a copy of the workdir with the embedded
patch strictly reverse-applied, the exact construction the workdir-as-is
fallback below uses — and comparing it against the frozen chain's
patch-free extraction restricted to the files the embedded patch touches
(presence, type, mode, and content count). If ANY touched file differs —
because the recorded base moved there, or because the workdir carries
uncommitted edits there outside the hunk pre-images (the reversed old
side then differs from the base even though the base never moved) — the
base the patch was diffed against is no longer intact where the patch
needs it, and the rebase takes the workdir-as-is fallback below
(printing the same workdir-as-is note, quoted there): with a clean
workdir the reversal is exact, so the merge converges onto the moved
chain — conflict-free only when the upstream move stays inside the
patch's hunk pre-images (a move on files the patch does not touch rides
the untouched-file class below, flagged too) — the frozen snapshot
re-freezes with the live base's bytes, and the committed patch
regenerates against the moved base: content-identical to what the
erase+setup dance below produces on the same state, while the fallback
keeps the checkout and preserves whatever the machinery can
(reverse-failed files, structural cases) instead of discarding it. A
move that falls outside the pre-images (the same file, a different
region) still converges onto the moved chain, but the moved file is
flagged in `Conflict:` — the displacement is never silent; with
uncommitted work on the touched files outside the pre-images, that work
is likewise displaced (the moved chain's content — the committed value —
wins) and the file is flagged. Users who want that work kept must commit
it before rebasing: commit folds it into the project's patch, where the
strict reverse gate (it refuses a pre-image the workdir no longer holds
verbatim) and the three-way protect it; after the fact, only a
deliberate resolve — restoring the bytes by hand before clearing the
flag — brings it back, folding the restored edit into the regenerated
patch. If EVERY touched file matches, the
mismatch lies entirely outside the patch's file set — uncommitted work
on files the patch does not touch and/or upstream deltas on such files —
so the base the patch was diffed against is intact exactly where the
patch needs it, and the residual refusal stands, worded so it does not
misdiagnose a clean tree: the mismatch may be uncommitted work or an
upstream move under files the patch does not touch, and committing is
never prescribed as the way out (when a link did move, committing would
fold those upstream deltas into the patch):

```
workdir '<workdir>' does not match the recorded base outside the files the patch touches: either it has uncommitted changes, or an upstream Projeny: link was rebased and the base chain moved under files the patch does not touch (committing would fold those upstream deltas into the patch); recover with 'projeny erase-setup --force <f>' followed by 'projeny setup <f>'
```

The refusal touches nothing and names both possibilities the mismatch
can hide — uncommitted changes, or an upstream Projeny: link rebased so
the base chain moved under files the patch does not touch (on a clean
tree the status shows only the spurious `Modified:` of the upstream
move; nothing is uncommitted). It never prescribes `commit` — when a
link did move, committing would fold those upstream deltas into the
patch — and names no `revert` (there is no such command); the recovery
it gives works for both hypotheses. For that residual state — and only
it; a base that moved under the patch's files no longer needs the
dance — the recovery that converges the project onto the new upstream
state discards the checkout and re-extracts the composed chain:

```
projeny erase-setup <downstream>.projeny --force
projeny setup <downstream>.projeny
```

The erase discards the downstream checkout's uncommitted changes (that is
what the `--force` means here), and the next setup — finding no workdir —
stales the downstream frozen snapshot, re-freezes the upstream link's
current bytes, and extracts the new upstream state.

When the patch no longer applies (an upstream commit rewrote what it
touches), every command that builds the expected tree dies with
per-command recovery guidance: `diff`, the merge path of
`setup`, `package`/`extract` (through their setup-first and their own
reference trees), `get-attributes`, and the `erase-setup` no-force check
all die with `cannot status '<f>': the frozen base chain no longer
reconstructs the recorded base (an upstream Projeny: link was rebased)
and the patch does not apply onto the new content; run 'projeny rebase
<f>' to rebase from the workdir as-is, or 'projeny erase-setup --force
<f>' followed by 'projeny setup <f>' to re-extract and re-apply with
conflict markers`, with the verb spelled for the command (`cannot set
up`, `cannot diff`, `cannot package`, `cannot extract`, `cannot get the
attributes of`, `cannot erase-setup`). `status` dies with the guidance
too — while the recorded state carries no unresolved conflicts. Once the
erase+setup recovery below lands its conflict markers (or any other
conflicted landing leaves the recorded patch unapplicable), the guidance
would be circular — it names `rebase` and `commit`, both refused while
conflicts are unresolved — so `status` instead prints the recorded
`Status:` and `Conflict:` lines (exactly like the classic conflicted
state) and exits 0 with `projeny: the live diff is unavailable while the
recorded patch does not apply onto the frozen base chain; resolve the
conflicts and commit`; `diff` and every other consumer of the live diff
still die with the guidance. `commit` and `freeze-mtime` build
the bare chain — the composed extraction WITHOUT this project's patch —
so they die with the same
guidance only when the chain itself no longer extracts (an
upstream link's own recorded patch fails onto its own base); when the
chain still extracts and only this project's patch fails, `commit` runs —
and folds the spurious deltas exactly as in the applicable-patch state
above, so the rule is the same here: rebase the project or erase and
re-set-up it instead of committing a spurious tree.

Two recoveries work. The first is zero-argument `rebase` (the
base-switch form `rebase <f> <base.projeny>` too): with an intact
workdir it proceeds FROM THE WORKDIR AS-IS instead of refusing — the
same machinery, note included, that the state-(i) decision tree above
engages when the chain still builds but the recorded base moved under
the patch's files (there a clean workdir converges conflict-free when
the upstream move stays inside the patch's hunk pre-images and with the
moved file flagged otherwise; here the patch no longer applies at all,
so the machinery is the only way forward). It
prints `projeny: the frozen base chain of '<f>' no longer reconstructs
the recorded base (an upstream Projeny: link was rebased); rebasing from
the workdir as-is; files the patch does not touch take the new base's
content (flagged as conflicts)` (the note names only the untouched-file
clause; a touched file whose outside-pre-image bytes the merge displaced
is flagged the same way — see the file classes below), skips the
clean-tree check, and
rebuilds the old merge base by strictly reverse-applying the embedded
patch onto a copy of the workdir — the workdir is the last local record
of base + patch, and reversing the patch off it reconstructs the base
the merge engine needs. That reconstruction is byte-exact base content
only INSIDE the patch's hunk pre-images; outside them the old side IS
the workdir's own bytes, where the workdir's content cannot be told
apart from stale committed content — the true old base is
unreconstructable there. The merge itself is the normal per-file three-way
engine against the live base's committed extraction (base = the
reconstructed old side, ours = the new side, theirs = the workdir's
file), and what each file ends up holding follows from that boundary,
file class by file class.

Files the patch's hunks touch whose reversal fully succeeds keep the
merge's decisions in the hunk regions: a region where the committed
change and the moved chain genuinely overlap carries inline markers
with both sides (`<<<<<<< projeny (new setup)` / `=======` /
`>>>>>>> projeny (local changes)` — the merge engine's labels; the
fresh-setup landing below writes `<<<<<<< current` / `>>>>>>> patched`
instead), and a region that merged cleanly rides as merged — the
pre-images are exactly where the merge's resolutions and conflict
markers live. Files whose hunks apply cleanly onto the moved chain keep
the applied patch in those regions the same way. Inside the pre-images
the reverse's success is positive evidence the workdir matched
base+patch exactly, so the merge's output there is trusted as produced.
Outside them the reconstruction's base is the workdir's own bytes — the
true old base is unreconstructable there — so the merged content (what
the moved chain holds) stands everywhere on the file, symmetric with
the untouched-file class below, and a displacement analysis after the
merge (run identically for merged files and cleanly-applied ones,
anchored in old-side coordinates) reports whether any changed old-side
range lies outside the pre-images: when one does, the file is flagged
in `Conflict:` — markerless, since the reverse succeeded and the merge
was clean — so the displacement is visible, never silent. The committed
change itself is never a displacement candidate (a changed base range
covering the applied pre-image core always intersects the pre-image
ranges). A user who wants uncommitted outside-pre-image work kept must
commit it first: commit folds it into the project's patch, where the
strict reverse gate (it refuses a pre-image the workdir no longer holds
verbatim) and the three-way protect it. Once the fallback has displaced
that work, only a deliberate resolve — restoring the bytes by hand
before clearing the flag — brings it back, and the restored edit folds
into the regenerated patch against the moved base through the usual
resolve + commit.

Files whose reversal fails — a binary block (its literals are
one-directional), an opaque or unparseable block, or a text block whose
pre-image the workdir no longer holds verbatim (an uncommitted edit
overlapping the hunk) — are left out of the reversal, and their files
take the conservative path: they keep their workdir bytes whole,
without markers, and are flagged in `Conflict:` (the merge cannot know
their true base, so where the merged content differs the workdir wins).
The same conservative path is what keeps an uncommitted edit riding on
top of a file the patch adds (the edit breaks the reversal of that
block — the reversed delete no longer matches the edited bytes — so the
workdir's bytes win whole, flagged), and what restores a file the patch
deletes that the user locally recreated (the reversed re-creation no
longer matches; the workdir's bytes go back into the tree, flagged).

Files NO hunk of the project's patch names take the new base's content
— convergence, since in the typical bricked state those untouched files
are exactly the upstream-moved ones: when the workdir's content
differed from what the new base holds, the file is flagged in
`Conflict:` so the replacement is visible and resolvable (mirroring the
divergent-binary rule — the new side wins, conflict recorded; resolving
it by keeping the new base's content folds nothing into the patch),
and when it was equal the file is a no-op — the same
new-content-wins-and-flag policy the displacement analysis applies to
touched files outside their pre-images. Rename-shaped and binary
blocks stay with the merge and the conservative path as always — the
displacement and untouched-file passes skip them (no old-side anchor to
classify against, and the reverse gate refuses binary literals) — and
untracked workdir files are out of scope for all of this: they are not
in the new tree, so a plain untracked text file goes away with the
replaced workdir, while untracked binaries ride across the replacement
as `setup` and rebase carry binaries. The rebase exits 0 with the usual `projeny:
rebased onto '<x>' with N conflict(s):` report, each flagged file's
path listed under it and each recorded as a `Conflict: <path>` entry in
the status file, re-freezes `.<stem>.snapshot` with the live base's
bytes, and the usual resolve + commit round-trip completes the
recovery.

The second recovery is the erase+setup pair
above — the dance the residual refusal names for
the outside-the-file-set state too: with the checkout and status gone,
the fresh setup re-freezes the
base from the live base file the header names (whose own chain is
self-consistent) and lands the recorded patch with inline `<<<<<<<
current` / `>>>>>>> patched` markers, reporting `projeny: set up
'<workdir>' from '<base>' with N conflict(s):` followed by `projeny:
setup left conflicts (exit 1)` and the usual fix-the-files,
`resolve`-each-one, `commit` instruction — and the resolve + commit
round-trip completes this
recovery too. Between those markers and their resolution the checkout
sits in the conflicted interim described above: `projeny status`
reports the recorded state and exits 0 with the
live-diff-unavailable note, while `diff` still dies with the guidance.
That conflicted
landing is not specific to the bricked state: any fresh setup of a
Projeny:-based project whose patch conflicts with its (possibly moved)
chain lands the same way instead of dying (see [Usage](#usage)), and a
project with no workdir has no fallback — its setup-first re-freezes the
base from the live base file the header names, producing either exactly
that conflicted fresh checkout (which `rebase` refuses until it is
resolved) or the fresh-chain hard die below. The one fresh-setup hard
die left in the mode is a chain
that cannot even extract — a link's recorded patch failing onto its own
frozen base with no checkout to rebase — which dies with snapshot-level
guidance (`cannot set up '<f>': the frozen base chain no longer
reconstructs a consistent base (a link's recorded patch no longer
applies to its own frozen base — usually an upstream Projeny: link was
rebased); remove '<snapshot path>' to re-freeze this project's base from
the live base file it names, or restore that snapshot, then run
'projeny setup <f>' again`).

### Switching to a different base file

`projeny rebase <f> <base.projeny>` (one `.projeny` argument) points
the project at a different base file — a base switch for a
Projeny:-based project, a conversion for an `Archive:`-based one. The
base must live in the SAME directory as `<f>` (`cannot <cmd> '<f>' with
base '<given>': the Projeny: base file must live in the same directory
as '<f>'`), must exist (`base file '<path>' does not exist; it is
needed to <cmd> '<f>'`), must parse as a `.projeny` file, and must not
close a cycle in the `Projeny:` chain: the live chain is pre-checked,
seeded with the target file itself, before anything is touched
(`cannot <cmd> '<f>': basing it on '<base>' would create a cycle
('<base>' transitively depends on '<f>'); Projeny: chains must not
contain cycles`, plus a variant naming the file reached twice in the
chain). On success the `Projeny:` header is rewritten to the new base,
`Origname:` comes from the new base's own parse, the patch moves onto
the new base's committed extraction, and the frozen snapshot is
overwritten with the new base's bytes. A URL:-based project accepts
only URL arguments (see [Usage](#usage)), so it cannot rebase itself
onto a `.projeny` file. `create <f.projeny> <base.projeny>` (see
[Usage](#usage)) applies the same same-directory, existence,
parseability, and cycle rules to a brand-new file, plus `--origname`'s
match with the base's `Origname:`.

### Mode conversions

`rebase` converts between the base modes in both directions (see
[Usage](#usage)): tarball or URL arguments on a Projeny:-based project
convert it away from the form — the `Projeny:` header is replaced by
the new `Archive:` or `URL:` header(s), the new form's snapshot is
managed as usual, and the old `.<stem>.snapshot` LINGERS as a stale
file (stale snapshots are never auto-cleaned) — while a `.projeny`
argument on an `Archive:`-based project converts it to Projeny:-based,
and `create <f> <base.projeny>` builds a Projeny:-based project from
scratch. A conversion can also be undone by hand: rewriting the
`Archive:`/`URL:` header back to `Projeny: <other>.projeny` by hand is
an adoption at the next `setup` (see
[Adoption: hand-editing the Projeny: header](#adoption-hand-editing-the-projeny-header))
— the checkout is rebuilt from the newly named base's committed
extraction and the lingering pre-conversion snapshot is overwritten with
the new base's live bytes, so the stale snapshot is replaced rather than
silently used as the base.
`erase-setup --erase-snapshots` deletes exactly the snapshot
the CURRENT mode names, so a conversion can leave the other mode's
snapshot behind (see
[Erasing a setup](#erasing-a-setup)).

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
threads. A package that survives neither pass is not given up on yet:
the coordinated retry stage below gives every failed package one more
try after all the projects are done.

A [Projeny:-based project](#projeny-based-projects) contributes nothing
to this phase — it names no URLs, and its base file's URLs (if any) are
fetched on demand during that project's own setup — so mixing
Projeny:-based projects into `-j` runs works.

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

A project whose downloads never verified does not fail alone. Its
per-project phase reports the failure as usual (`'<Name>' needs archive
'<pkg>', but the parallel download phase failed to obtain it`, or the
download-failure report of an on-demand fetch — a Projeny:-based
project's URL:-based chain base, fetched during that project's own
setup), and once every other per-project operation has finished, ONE
coordinated retry stage runs — at most once per invocation: it prints one
waiting note (`projeny: N setup(s) could not obtain their download(s)
(<archives>); waiting <W> second(s) before one more try`, spelled with the
command's own noun and naming every failed archive), waits once,
re-downloads every failed package in one parallel batch pass (the
batch's own immediate retry pass is suppressed for it — the wait IS the
one more try), and then re-runs only the projects whose failures were
download-caused. Waiting after the other setups, rather than before
them, is the point: a temporary outage fails several downloads at once,
and one wait followed by one parallel retry covers them all. A failure
for any other reason (a conflicted `.projeny` file, a bad header) is
never retried and is counted once by the summary line. The wait length
is `PROJENY_URL_RETRY_SECONDS` seconds (default 60); `0` skips the stage
entirely, so a failed download stays failed with byte-identical output.

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
finished, and after one wait-and-one-more-try (`projeny: N of M
download(s) failed (<names>); waiting <W> second(s) before one more try`,
then one parallel re-pass over exactly the failed packages, the batch's
own immediate retry pass suppressed for it) — exits nonzero.

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
migrate it back into use), and for a
[Projeny:-based project](#projeny-based-projects) the frozen base
snapshot `.<stem>.snapshot` (that mode has no legacy undotted form).
ONLY that exact snapshot goes: a similarly
named snapshot for a different version (the `.<old-archive>.snapshot` a
rebase left behind, say) survives, and the checked-in tarball is never
touched. The next `setup` then re-downloads (URL: projects), unpacks
from the checked-in tarball (Archive: projects), or re-freezes the
current live base file (Projeny: projects).

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
`-c/--curl-jobs` for erase-setup: the only downloading it can do is the
no-force check's on-demand fetch of a missing URL archive (one pass, no
wait-and-retry), not the batch download whose in-flight curl transfers
the option caps, so that spelling is rejected as an unknown option.

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
cannot be read or parsed, a checkout whose archive and its snapshot
are both missing or unusable (for a
[Projeny:-based project](#projeny-based-projects): whose base file and
its frozen snapshot are both missing), or a Projeny:-based checkout
whose frozen base chain no longer reconstructs the recorded base and
whose patch no longer applies onto the moved content — the chain-moved
guidance of
[Chains and rebasing an upstream link](#chains-and-rebasing-an-upstream-link)
is the per-project error there. In every case the live diff that would
compare the checkout against the recorded tree cannot run, and a project
that cannot
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
  (derived from the URL's basename) holds the verified download; for a
  [Projeny:-based project](#projeny-based-projects) the snapshot of the
  same shape holds the frozen base file's bytes but is named after the
  `.projeny` file itself (`.bar.snapshot` for `bar.projeny`).

The shape sharing has one sharp edge: a checked-in tarball named EXACTLY
like a `.projeny` file's stem (an `Archive: bar` for a `bar.projeny`)
would give a classic project's archive snapshot the same `.<stem>.snapshot`
path that a Projeny:-based `bar.projeny` uses for its frozen base —
avoid naming a checked-in tarball exactly after the `.projeny` file's
stem (see
[The frozen base snapshot of a Projeny:-based project](#the-frozen-base-snapshot-of-a-projeny-based-project)).

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
own name, so nothing survives under an original name. One snapshot is
never stale state: a URL:-based project's snapshot whose blake3 hash one
of the current `.projeny` file's own `URL:` headers verifies IS the
archive the file records (the bytes a `create` just fetched, say), so a
fresh `setup` keeps it and skips the download — the single-project mirror
of the parallel batch's rule that a plan-verified shared download must
survive. The one exception is
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
  members whose target does not stay inside the tree: relative targets are
  resolved lexically against the member's directory (a symlink's target is
  member-directory-relative; a hardlink's target is archive-root-relative,
  matching how tar links) — a target like
  `b3sum/LICENSE_A2 -> ../LICENSE_A2` resolves back inside the tree and is
  kept, while any target that climbs above the tree root is refused (leaf
  or not, keeping the `..` shape strict like git does). An absolute symlink
  target is refused too, unless the symlink is a leaf of the archive — no
  other member's path lives inside the symlink's path — in which case
  nothing can be written through the link while tar unpacks and the inert
  link is kept; upstream tarballs ship such fixtures (gnome-autoar's
  `tests/files/extract/test-symlink-parent/reference/arextract -> /tmp`),
  and refusing them would force repacking pristine tarballs. A non-leaf
  absolute escape (some member would extract through the link) and every
  hardlink escape (a hardlink binds tree content to outside content) are
  still refused. The same resolve-based check applies to the links a patch
  or merge creates, and to the links commit collects from the workdir —
  where a link that merely rides along from the archive is recorded, not
  refused, while any patch block that would create or retarget an escaping
  link dies at emit time, so commit never stores a patch that setup would
  later have to refuse.
- `cp -a` for whole-tree copies (moving trees across filesystems when
  `rename(2)` returns `EXDEV` — scratch dirs live next to the .projeny file
  (see below), never inside the workdir — and snapshotting files for
  three-way merges).

Scratch directories (where every command builds the trees it later moves
into place) live NEXT TO the `.projeny` file, not in `/tmp`: for
`projects/dash.projeny` they are hidden, uniquely named
`projects/.dash.projeny.<role>.XXXXXX` directories (`<role>` spells the
purpose — `setup`, `E`, `N`, `rebase-url`, ... — and `XXXXXX` is the
mkdtemp suffix). Same-filesystem placement makes moving a freshly built
tree into the checkout a plain `rename(2)`; being hidden keeps them out of
every directory scan (only names ending in `.projeny` are ever matched).
They are always cleaned: the RAII destructor removes the (possibly already
moved-out) dir on every exit path — success, hard error, and signal (the
process installs a sigwait thread for SIGINT/SIGTERM/SIGHUP/SIGQUIT
that sweeps first and then dies by the signal, so exit statuses stay
shell-correct). Scratch dirs are created next to the `.projeny` file only
when that directory accepts new directories: on a read-only checkout (or a
full filesystem) projeny warns once and falls back to the system scratch
parent (`$TMPDIR`, else `/tmp`), keeping the same hidden name shape there,
so the read-only commands (`status`, `diff`, `package`, `extract`,
`get-attributes`) keep working; moving trees out of such a fallback dir is
no slower than usual (`move_path`'s `EXDEV` `cp -a` fallback already
handles the cross-device case). Each TempDir also holds an exclusive
`flock` on its own directory for its lifetime: a startup sweep reaps
scratch-shaped dirs whose flock is unclaimed (i.e. left behind by a
SIGKILL'ed run) both next to the `.projeny` file and — once per process at
startup — in `$TMPDIR`//`/tmp`, where BOTH shapes are reaped: the legacy
`projeny-<role>-XXXXXX` shape the pre-2026 binaries scattered there and the
fallback's current hidden shape, so upgrading cleans up everything the old
binaries ever leaked and a SIGKILL'ed run's fallback dirs are not leaked
either.

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
`..` targets resolving inside the tree and kept, leaf absolute targets
allowed, and non-leaf/`..`/hardlink escapes refused),
long single-line files, tabs, quote/tab filenames, extra-header
preservation, no-change commits, and CLI error paths — plus optional
`git apply --check` / `patch -p1 --dry-run` compatibility spot-checks that
verify stored patches with those tools when they are installed, plus
`diff`/`patch` roundtrips and minimum-diff cases, `patch` conflict
markers with console lists, `apply` inside a checkout (projeny and git
diffs on top of uncommitted changes, add/delete/rename/mode marks that
`commit` folds in, the conflict exit-1 flow, the
refuse-when-a-touched-file-is-conflicted guarantee, idempotent re-apply
with chained renames, binary-add roundtrips),
setup-restores-accidentally-deleted-files
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
`setup` and `status`), plus the Projeny:-base sections, which cover the
third base mode — `create` from another `.projeny` file (the composed
`Projeny:`/`Origname:`/`Name:` headers, `--origname` match/mismatch, the
same-directory law, the arity rule, and the frozen snapshot holding the
base file's bytes), a hand-written Projeny:-based file whose setup
extracts the base's COMMITTED EXTRACTION, the frozen law
(setup/commit/status/package/extract keep using the snapshot-recorded
base after the base file gains committed changes), the zero-arg rebase
(clean, conflicting, and the refusals), the base-switch rebase and its
refusals, the mode conversions away from the `Projeny:` form (to a
tarball and to a `URL:` list, the old snapshot lingering), cycles
(self/two/three-file loops at setup plus the create/rebase pre-checks),
a four-link chain with a middle-link rebase, the frozen snapshot
surviving a deleted base file (the recovery message,
`erase-setup --erase-snapshots`, and workdir-gone staling), the
Projeny:-mode parse errors (including the empty-`Projeny:`-value
refusal that fires ahead of the exclusivity and required-header
checks), setup's adoption of a hand-edited
`Projeny:` header (including the hand-conversion back INTO the form
after a tarball/URL rebase, the stale pre-conversion snapshot
overwritten with the newly named base's live bytes), a URL:-based base,
the everyday
status/diff/package/extract surface, `freeze-mtime` against the frozen
base extraction, parallel runs with a Projeny:-based project in the
mix, and the frozen-chain-recovery coverage — the brick an upstream
middle-link rebase leaves downstream (the per-command guidance die for
`diff`, the merge path of `setup`, and the `erase-setup`
check, with `status` dying too while nothing is conflicted; the
zero-arg rebase's rebuild-from-the-workdir fallback with its
workdir-as-is note naming the untouched-files clause, the strict
reverse-apply, the three-way merge, the merged-conflict markers, the
displacement analysis that keeps the merged (moved-chain) content
everywhere on reverse-succeeded touched files and flags those whose
outside-pre-image workdir bytes the merge displaced, and the
conservative keep-workdir-bytes path for files whose reversal fails —
an uncommitted edit overlapping the hunk, a committed binary modify —
plus the untouched-file convergence flagged in `Conflict:`; the
`erase-setup --force` + `setup` re-apply recovery with its conflict
markers; the repro-A regression (a clean-workdir fallback whose
upstream move sits far outside the hunk pre-images converges flagged
onto the moved chain, content-equal to the erase-setup + setup
recovery on the identical state and carrying no reversion hunk in the
regenerated patch, followed by its state-(ii) variant: one upstream
change inside the pre-image keeps markers and the user's resolution
while the outside change converges, the file flagged either way); and
the state-(i) decision tree for the non-colliding moved
chain — the spurious `Modified:`, the clean workdir converging through
the touched-files comparison (the fallback engages conflict-free when
the move stays inside the hunk pre-images,
the snapshot re-freezes to the live base bytes, the committed patch
regenerates against the moved base, and a re-setup is a noop), the
residual refusal when the mismatch lies outside the patch's file set
(the misdiagnosis-free message, which keeps both hypotheses and
prescribes neither `commit` nor `revert`, leaving the
workdir, committed patch, status file, and snapshot untouched), the
healthy-dirty guard (a base that never moved refuses with the same
message for work on a non-touched file, while work on a touched file
outside the pre-images takes the fallback, is displaced — the
committed value wins — and flagged, brought back only deliberately
through resolve + commit), the dirty
state-(i) displacement (an upstream move inside a hunk pre-image
converges while uncommitted work outside it is displaced and flagged,
the commit folding nothing but the project's own delta), and the
conflicted-interim `status` printing the recorded state with the
live-diff-unavailable note and exiting 0 once the recovery lands
markers, with the no-conflict contrast still dying), the fresh
Projeny:-based
setup that applies a chain-conflicting patch with markers and exits 1
(the classic `Archive:` control keeps the hard die). The `create`
sections cover the new project
command: tarball and URL (with and without a hash — the computed hash
equals `projeny hash`) forms, the copied-in tarball and the verified
snapshot, `--comment` (last occurrence wins), the existing-file rule
(refuse / `--force` with the no-force check / `--force --force`
unconditional / `--erase-snapshots` passthrough), `--origname` naming the
checkout directory (`Name:` AND `Origname:` become `<name>`; a mismatch
with the archive's top dir refuses before anything is written), the
`--origname` corner guard (a pre-existing checkout directory is erased
only when attributable — the replaced file itself counts first, so a
`--force` re-run of the same create works — and never removed
otherwise, even with two `--force` flags), the warning about the
evicted sibling's leftover `.projeny` file (it still claims the
checkout name; the file is never deleted), the stale-state pin that
keeps a URL project's hash-verified snapshot alive across an
erase-setup without `--erase-snapshots` (the next setup re-uses it and
downloads nothing), and the checkout-directory
validation in project-argument resolution (mismatch refuses, match works,
git-conflicted siblings still recover via `setup`). It prints
`ok`/`FAIL` lines with a `passed/failed` summary and exits nonzero on
any failure. A conflicting `setup` exits 1 (with markers left behind),
so the suite asserts `expect_fail` for every conflict-leaving setup.
