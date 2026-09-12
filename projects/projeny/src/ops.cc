/*
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "ops.h"

#include "projeny_file.h"
#include "tree.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <unistd.h>

#include <sys/stat.h>

Ctx resolve_ctx(const std::string& projeny_arg)
{
    Ctx c;
    c.projeny_arg = projeny_arg;
    c.pdir = dirname_of(projeny_arg);
    // Canonical, dot-prefixed status file: ".<f>.projeny.status" next to the
    // .projeny file. Older projenies wrote "<f>.projeny.status"; when only
    // the legacy form exists, rename it into the canonical form (lazy
    // migration at first use). This runs for EVERY command, so any command
    // completes the upgrade; it is idempotent and a no-op when the
    // canonical file exists or neither form does. The crash-recovery
    // journal (journal_path_for below) keeps its name: it was never part of
    // the undotted ".status" naming, so there is no legacy form to migrate
    // and journal-based recovery is unaffected by the upgrade.
    c.statusfile = dotname(projeny_arg) + ".status";
    std::string legacy_statusfile = projeny_arg + ".status";
    if (!path_exists(c.statusfile) && path_exists(legacy_statusfile))
        move_path(legacy_statusfile, c.statusfile);
    return c;
}

namespace {

// Normalize a user-supplied path (CWD-relative or absolute, or already
// workdir-relative like "lua/src/f.c", or prefixed like "lua/src/f.c" where
// the first component equals the workdir name) to workdir-relative form.
// Dies if the path escapes the workdir.
std::string normalize_workdir_rel(const std::string& workdir, const std::string& wid,
                                  const std::string& user_path)
{
    std::string wabs = absolutize(strip_trailing_slashes(workdir));
    std::string p = user_path;
    // "wid/..." style: strip the leading workdir name.
    if (p == wid || starts_with(p, wid + "/")) {
        std::string wrel = (p == wid) ? "" : p.substr(wid.size() + 1);
        if (wrel.empty())
            die("path '" + user_path + "' refers to the workdir itself");
        // Validate components lexically (reject ".." escapes).
        std::vector<std::string> parts;
        size_t i = 0;
        while (i <= wrel.size()) {
            size_t j = wrel.find('/', i);
            std::string comp = (j == std::string::npos) ? wrel.substr(i)
                                                       : wrel.substr(i, j - i);
            if (j == std::string::npos)
                i = wrel.size() + 1;
            else
                i = j + 1;
            if (comp.empty() || comp == ".")
                continue;
            if (comp == "..")
                die("path '" + user_path + "' escapes the workdir");
            parts.push_back(comp);
        }
        if (parts.empty())
            die("path '" + user_path + "' refers to the workdir itself");
        std::string out = parts[0];
        for (size_t k = 1; k < parts.size(); ++k)
            out += "/" + parts[k];
        return out;
    }
    std::string abs = absolutize(p);
    std::string w = wabs;
    if (abs == w)
        die("path '" + user_path + "' refers to the workdir itself");
    if (!starts_with(abs, w + "/"))
        die("path '" + user_path + "' is outside the workdir '" + workdir + "'");
    return abs.substr(w.size() + 1);
}

std::string scratch_parent_for(const std::string& pdir)
{
    // Scratch temp dirs must NEVER live inside the workdir (or pdir): a crash
    // between temp creation and cleanup would otherwise leave junk behind
    // that pollutes the next diff. Use the system temp dir; TempDir removes
    // the tree on all paths (RAII) and die() removes registered temp dirs
    // too. diff_trees additionally filters any legacy ".projeny-tmp*" entries
    // left behind by older crashed runs. (pdir is unused; kept for call-site
    // readability.)
    (void)pdir;
    return system_scratch_parent();
}

// Snapshot path for an archive: the dot-prefixed sibling of the archive
// (foo-1.2.3.tar.gz -> .foo-1.2.3.tar.gz.snapshot). write_status maintains
// the snapshot as a byte-exact copy of the archive, so a later setup can
// reconstruct the tree recorded by the status file even after the archive
// itself is deleted from git (e.g. an upstream rebase removes the old
// tarball). Older projenies wrote the undotted "<Archive>.snapshot" form;
// migrate_snapshot below renames that lazily at each point of use.
std::string snapshot_path_for(const std::string& archive)
{
    return dotname(archive) + ".snapshot";
}

// Legacy (pre-dot-naming) snapshot path: the archive plus ".snapshot",
// normalized the same way dotname normalizes the dotted form ("./a.tar.gz"
// and "a.tar.gz" name the same file and must warn/compare alike; dotname
// drops the "./" via its dirname=="." case).
std::string legacy_snapshot_path_for(const std::string& archive)
{
    std::string bare = archive;
    if (starts_with(bare, "./"))
        bare = bare.substr(2);
    return bare + ".snapshot";
}

// If the dotted snapshot is missing but the legacy undotted one exists,
// rename it into the canonical dotted form. Called at every point of use
// (ensure_snapshot, resolve_status_archive, status's live diff, stale-state
// reconciliation), so any command finishes the upgrade. Idempotent no-op
// when the dotted snapshot exists or neither form does.
void migrate_snapshot(const std::string& archive)
{
    std::string snap = snapshot_path_for(archive);
    if (path_exists(snap))
        return;
    std::string legacy = legacy_snapshot_path_for(archive);
    if (!path_exists(legacy))
        return;
    move_path(legacy, snap);
}

// Byte-equality of two regular files: sizes first, then a chunked content
// compare (tarballs can be tens of megabytes, so neither file is ever
// loaded whole into memory). A missing file is equal to nothing.
bool files_equal(const std::string& a, const std::string& b)
{
    struct stat sta, stb;
    if (stat(a.c_str(), &sta) != 0 || stat(b.c_str(), &stb) != 0)
        return false;
    if (!S_ISREG(sta.st_mode) || !S_ISREG(stb.st_mode))
        return false;
    if (sta.st_size != stb.st_size)
        return false;
    std::ifstream fa(a.c_str(), std::ios::binary);
    std::ifstream fb(b.c_str(), std::ios::binary);
    if (!fa || !fb)
        return false;
    // Sizes match, so the two streams advance in lockstep and the loop
    // ends when both hit EOF.
    char bufa[65536], bufb[65536];
    for (;;) {
        fa.read(bufa, sizeof(bufa));
        fb.read(bufb, sizeof(bufb));
        std::streamsize na = fa.gcount();
        std::streamsize nb = fb.gcount();
        if (na != nb)
            return false;
        if (na == 0)
            return true;
        if (memcmp(bufa, bufb, (size_t)na) != 0)
            return false;
    }
}

// Make sure `archive` has a snapshot copy (see snapshot_path_for): a no-op
// when the snapshot already matches the archive (the hot path: every
// build's re-setup lands here), a rewrite when it does not, and a kept
// snapshot when only the snapshot survives. A legacy undotted snapshot is
// renamed into the dotted form first. Called from write_status — i.e.
// exactly when setup/commit/rebase records what it used — so the
// snapshot is never refreshed before the trees derived from the OLD
// snapshot were built, and never left stale relative to the status file.
void ensure_snapshot(const std::string& archive)
{
    migrate_snapshot(archive);
    std::string snap = snapshot_path_for(archive);
    if (!path_exists(archive)) {
        // The archive is gone (git deleted it): the snapshot, if any, is
        // now the only record of what the last setup used, so keep it.
        if (path_exists(snap))
            return;
        die("archive '" + archive +
            "' does not exist and neither does its snapshot '" + snap +
            "'; it is needed to record which archive this setup used in the "
            "status file. This usually means the archive was deleted from "
            "git (for example by a rebase to a newer tarball) and this "
            "checkout was last set up by a projeny that did not keep "
            "snapshots. Restore the archive (for example 'git checkout "
            "<commit> -- <archive>') and run 'projeny setup' again");
    }
    if (path_exists(snap) && files_equal(archive, snap))
        return;
    // write_file_bytes is temp file + fsync + rename, so the snapshot
    // switches atomically and a crash never leaves a half-written copy.
    // It is byte-exact for regular files, which tarballs are.
    write_file_bytes(snap, read_file_bytes(archive));
}

// Resolve which file to read when reconstructing what the LAST setup used
// (a tree referred to by the status file or by one side of a git-conflicted
// .projeny): prefer the snapshot — it is the byte-exact copy of what that
// setup actually unpacked, so an in-place rewritten tarball does not corrupt
// the reconstruction either — and fall back to the archive itself for
// checkouts set up before snapshots existed. That fallback also COPIES the
// archive into the dotted snapshot right away, so the next run finds the
// snapshot instead of repeating the fallback (checkouts set up by an older
// projeny migrate themselves on first use). `what` is a human-readable
// phrase for the error (e.g. "reconstruct the tree recorded by '<file>'").
// Dies with recovery guidance when neither file exists. Archives named by
// the CURRENT .projeny file must not go through here: those files must
// exist, and unpack_single_top's plain error is the right one.
std::string resolve_status_archive(const std::string& archive,
                                   const std::string& what)
{
    migrate_snapshot(archive);
    std::string snap = snapshot_path_for(archive);
    if (path_exists(snap))
        return snap;
    if (path_exists(archive)) {
        // No snapshot yet: snapshot the archive now (same copy
        // write_status's ensure_snapshot would make) and use it.
        write_file_bytes(snap, read_file_bytes(archive));
        return snap;
    }
    die("archive '" + archive +
        "' does not exist and neither does its snapshot '" + snap +
        "'; it is needed to " + what +
        ". This usually means the archive was deleted from git (for example "
        "by a rebase to a newer tarball) and this checkout was last set up "
        "by a projeny that did not keep snapshots. If the workdir has no "
        "local changes, remove the status file and the workdir and run "
        "'projeny setup' again; otherwise restore the archive (for example "
        "'git checkout <commit> -- <archive>') and run 'projeny setup' "
        "again");
}

// Best-effort extraction of the "Archive:" value from a status file's
// bytes: the embedded .projeny copy (everything after the
// "--- projeny content ---" delimiter, see kStatusDelim in
// projeny_file.cc) begins with its header block, one of whose lines is
// "Archive: <value>". Used only by the stale-state reconciliation to find
// the archive whose snapshot belongs to the status being discarded.
// Returns "" when the file is too garbled to tell — this must never die,
// because an unparseable status file is renamed out of the way just the
// same.
std::string archive_from_status_bytes(const std::string& data)
{
    size_t pos = data.find(kStatusDelim);
    if (pos == std::string::npos)
        return "";
    pos = data.find('\n', pos);
    if (pos == std::string::npos)
        return "";
    ++pos; // first byte of the embedded .projeny copy
    // Scan the first lines of the embedded copy for the "Archive:" header.
    // Header lines are unindented "Key: value" lines; prose is indented and
    // blank lines are empty. Keep going through headers and prose, but stop
    // (returning "") at anything else — patch bodies, garbage.
    for (int scanned = 0; scanned < 16 && pos < data.size(); ++scanned) {
        size_t nl = data.find('\n', pos);
        std::string line =
            data.substr(pos, nl == std::string::npos ? std::string::npos
                                                     : nl - pos);
        if (starts_with(line, "Archive: "))
            return trim(line.substr(9));
        if (line.empty() || line[0] == ' ' || line[0] == '\t') {
            // blank line or prose: keep scanning
        } else {
            // Header-shaped (a header other than Archive:)? keep going;
            // anything else (patch body, garbage) stops the scan.
            size_t colon = line.find(':');
            bool header_shaped =
                colon != std::string::npos && colon > 0 &&
                line.find_first_of(" \t") > colon;
            if (!header_shaped)
                return "";
        }
        if (nl == std::string::npos)
            break;
        pos = nl + 1;
    }
    return "";
}

// Path of the crash-recovery setup journal (defined with the conflicted-
// setup machinery below); needed here by disregard_stale_state's guard.
std::string journal_path_for(const Ctx& ctx);

// Stale-state reconciliation (checkout directory gone): without a workdir,
// the status file and the archive snapshots describe a checkout that no
// longer exists, so they are disregarded — renamed out of the way with a
// warning naming both paths — instead of being left behind to confuse a
// fresh setup. Each target is renamed to '<name>.stale', or '<name>.stale2',
// '<name>.stale3', ... when that name is taken (any existing path counts as
// taken, including directories).
//
// Every naming form is reconciled: after this runs, NO status/snapshot file
// remains under ANY of its names. When the canonical dotted form and the
// legacy undotted form both exist, each is staled under its own name
// (dotted -> dotted.stale, undotted -> undotted.stale, with the same
// .stale2/.stale3 numbering based on the undotted name); when only the
// legacy form exists, it is first migrated into the dotted form (existing
// migrate_snapshot for snapshots, direct rename for the status), so it ends
// up under the dotted .stale name.
//
// This helper must never run while a setup journal exists, and it ENFORCES
// that here so every caller is safe by construction (cmd_setup's own journal
// early-return precedes it on the fresh path; status/commit/add/rm/mv/
// resolve call this unconditionally and cannot tell a crash window from an
// abandoned checkout). The journal marks an interrupted conflicted setup
// whose recovery (setup_recover) still needs the status file — it drives
// conflict-side disambiguation and the union bookkeeping — and the archive
// snapshot, which is often the only copy of the local side's archive left
// (the interrupted rebase already deleted the old tarball from git).
// Stale-renaming either in that window makes the subsequent `projeny setup`
// die with "archive ... does not exist and neither does its snapshot"
// instead of recovering, and needs hand-restoration.
void disregard_stale_state(const Ctx& ctx,
                           const std::vector<std::string>& archives)
{
    if (path_exists(journal_path_for(ctx)))
        return;

    auto next_stale_name = [](const std::string& target) -> std::string {
        for (int n = 1;; ++n) {
            std::string cand = target + ".stale";
            if (n > 1)
                cand += std::to_string(n);
            if (!path_exists(cand))
                return cand;
        }
    };

    // Stale one logical file under both of its names: `dotted` is the
    // canonical name, `legacy` the pre-dot-naming name.
    auto stale_pair = [&](const std::string& dotted,
                          const std::string& legacy) {
        if (!path_exists(dotted) && path_exists(legacy)) {
            // Only the legacy form exists: migrate it into the dotted form
            // first, so it is staled under the dotted .stale name.
            move_path(legacy, dotted);
        }
        if (!path_exists(dotted))
            return;
        std::string dest = next_stale_name(dotted);
        warn("workdir for '" + ctx.projeny_arg + "' is missing; renaming "
             "stale '" +
             dotted + "' to '" + dest + "'");
        move_path(dotted, dest);
        if (path_exists(legacy)) {
            // Both forms existed: disregard the legacy copy too, under its
            // own name (same numbering, based on the undotted name).
            std::string ldest = next_stale_name(legacy);
            warn("workdir for '" + ctx.projeny_arg + "' is missing; renaming "
                 "stale '" +
                 legacy + "' to '" + ldest + "'");
            move_path(legacy, ldest);
        }
    };

    stale_pair(ctx.statusfile, ctx.projeny_arg + ".status");
    for (const auto& a : archives) {
        if (a.empty())
            continue;
        std::string archive = join_path(ctx.pdir, a);
        migrate_snapshot(archive);
        stale_pair(snapshot_path_for(archive),
                   legacy_snapshot_path_for(archive));
    }
}

// Unpack `archive` expecting top dir `origname`, then apply `patch_wid`
// (WID-label form, wid == `wid`) inside the unpacked tree. Returns the path
// of the resulting tree (inside our temp dir). Caller owns tmp lifetime.
std::string build_tree_from_patch(TempDir& tmp, const std::string& archive,
                                  const std::string& origname, const std::string& wid,
                                  const std::string& patch_wid,
                                  const std::string& what)
{
    std::string scratch = scratch_parent_for(tmp.path);
    unpack_single_top(archive, tmp.path, origname);
    std::string tree = join_path(tmp.path, origname);
    if (normalize_patch_text(patch_wid).empty())
        return tree;
    if (!apply_patch_whole(tree, patch_wid, wid, scratch)) {
        // Retry per-file to produce a precise error. Failures carry
        // workdir-relative paths from the single patch parser (no second
        // parser, so block counts cannot diverge into OOB/wrong names).
        std::vector<VcsFailure> bad = apply_patch_per_file(tree, patch_wid, wid);
        std::string detail;
        for (const auto& f : bad) {
            detail += "  " + f.display + "\n";
        }
        die("cannot apply " + what + ": patch does not apply cleanly", detail);
    }
    return tree;
}

// Merge user diff U (WID labels, wid `uwid`) onto fresh tree N (on-disk
// path), using base tree E for 3-way merges. `user_tree` is the on-disk
// user workdir holding the THEIRS content; it may differ from N (in setup,
// N is a fresh temp tree while the user workdir has the local edits).
// Records conflicts into `conflicts` (workdir-relative paths). Applies
// file-by-file; files that apply cleanly via the internal applier are
// applied directly, the rest go through merge_one_file.
void merge_user_diff_onto(const std::string& base_tree, const std::string& fresh_tree,
                          const std::string& user_tree, const std::string& workdir,
                          const std::string& wid, const std::string& uwid,
                          const std::string& upatch,
                          std::vector<std::string>* conflicts)
{
    // Parse U with its own wid (uwid): U was diffed with the old wid, which
    // may differ from the fresh tree's wid. Failures already carry
    // workdir-relative paths, so no second parser and no wid stripping here.
    (void)wid;
    std::vector<VcsFailure> bad = apply_patch_per_file(workdir, upatch, uwid);
    if (bad.empty())
        return;
    // For each failed block, do a 3-way merge of base/ours(=fresh)/theirs.
    std::string scratch = scratch_parent_for(workdir);
    for (const auto& f : bad) {
        std::vector<std::string> touched = f.paths;
        // Opaque blocks (e.g. combined diffs) carry no parseable path but
        // still name the file in display; fall back to deriving rels from
        // display when it looks like a plain path.
        if (touched.empty() && !f.display.empty() &&
            f.display.find('\n') == std::string::npos &&
            f.display.find(" -> ") == std::string::npos &&
            f.display.compare(0, 5, "diff ") != 0 &&
            f.display != "<unknown file>" && f.display != "<combined diff>" &&
            f.display != "<rename>") {
            touched.push_back(f.display);
        }
        if (touched.empty()) {
            std::string d = f.display.empty() ? "<unknown file>" : f.display;
            die("cannot merge local changes: unsupported patch block '" + d +
                    "' cannot be merged; resolve manually",
                "  " + d + "\n");
        }
        for (const std::string& rel : touched) {
            if (rel.empty())
                continue;
            // base = E-file, ours = N-file (fresh, possibly partially
            // patched with the clean files), theirs = user workdir file.
            // Failed blocks are skipped atomically per --include, so the
            // user file still holds the theirs content. Snapshot it first:
            // merge_one_file writes its output to the N-tree path, which
            // for in-place merges IS the user path (snapshot protects us).
            TempDir one(scratch, "projeny-mg-");
            std::string bf = join_path(base_tree, rel);
            std::string of = join_path(fresh_tree, rel);
            std::string tf = join_path(user_tree, rel);
            std::string dst = join_path(workdir, rel);
            std::string snapshot;
            bool had_theirs = path_exists(tf);
            if (had_theirs) {
                snapshot = join_path(one.path, "theirs");
                copy_recursive(tf, snapshot);
            }
            bool clean = merge_one_file(bf, of, had_theirs ? snapshot : tf, dst,
                                        workdir);
            if (!clean)
                conflicts->push_back(rel);
        }
    }
}

void sort_unique(std::vector<std::string>* v)
{
    std::sort(v->begin(), v->end());
    v->erase(std::unique(v->begin(), v->end()), v->end());
}

// True when `path` exists as a regular file whose bytes contain a NUL.
// Missing paths and non-regular files are not binary.
bool is_binary_file(const std::string& path)
{
    struct stat st;
    if (lstat(path.c_str(), &st) != 0)
        return false;
    if (!S_ISREG(st.st_mode))
        return false;
    return read_file_bytes(path).find(char(0)) != std::string::npos;
}

// Binary-file policy for workdir replacement (setup/rebase) and commit.
//
// Diffs carry NUL-bearing files as base64 binary blocks, so tracked-binary
// edits, deletions, and explicitly added binaries all roundtrip through the
// patch like text. The only files diffs must still ignore are untracked
// binaries that were never committed and never `add`ed (like a package
// tarball written into the workdir, or build junk): commit filters those
// back out of the patch (see cmd_commit), while setup/rebase carry them
// across the workdir replacement here so they survive on disk.
//
// Comparing `workdir` against `other_tree` (the incoming tree for
// setup/rebase; unused for commit, which passes carry=false and diffs
// everything):
//   - a workdir binary missing from other_tree is untracked: with carry it
//     is copied over (setup/rebase preserve it on disk); commit passes
//     carry=false and leaves it to the patch filter instead.
//   - every other binary state (tracked edits, tracked deletions, explicit
//     adds) is represented in the diff itself and needs no help here: setup
//     merges user diffs onto the fresh tree (accidental deletions are
//     filtered back out so they restore instead of deleting), and commit
//     folds them into the patch.
void reconcile_binaries(const std::string& workdir,
                        const std::string& other_tree, bool carry)
{
    if (!carry)
        return; // commit: the diff plus its untracked-binary filter decide
    if (!is_dir(workdir) || !is_dir(other_tree))
        return; // fresh setup: nothing to compare or preserve yet
    std::vector<std::string> stack;
    stack.push_back("");
    while (!stack.empty()) {
        std::string d = stack.back();
        stack.pop_back();
        std::string abs = d.empty() ? workdir : join_path(workdir, d);
        for (const std::string& name : list_dir_names(abs)) {
            std::string rel = d.empty() ? name : d + "/" + name;
            std::string full = join_path(workdir, rel);
            struct stat lst;
            if (lstat(full.c_str(), &lst) != 0)
                die("cannot stat '" + full + "': " + strerror(errno));
            if (S_ISDIR(lst.st_mode)) {
                stack.push_back(rel);
                continue;
            }
            if (!S_ISREG(lst.st_mode) || !is_binary_file(full))
                continue;
            std::string o = join_path(other_tree, rel);
            if (!path_exists(o)) {
                make_dirs(dirname_of(o));
                copy_path_preserving(full, o);
            }
        }
    }
}

void write_status(const Ctx& ctx, const StatusData& sd)
{
    // Maintain the archive snapshot (see snapshot_path_for) so later setups
    // can reconstruct the tree this status file records even after git
    // deletes the archive. This is the single funnel for every status-file
    // write in the codebase, and it runs only AFTER all tree work is done,
    // so a setup's E-tree always sees the snapshot of the archive the
    // PREVIOUS status recorded — a snapshot for the new .projeny is never
    // written too early. The parse is safe: every caller of write_status
    // just obtained sd.embedded from a .projeny that already parsed.
    if (!trim(sd.embedded).empty()) {
        ProjenyFile pf = ProjenyFile::parse_bytes(
            sd.embedded, "embedded copy for '" + ctx.statusfile + "'");
        ensure_snapshot(join_path(ctx.pdir, pf.archive));
    }
    write_file_bytes(ctx.statusfile, sd.serialize());
}

// Build the fresh tree for `cur` inside `tmp`: unpack the current archive
// and apply the current patch. Returns the tree path (inside tmp). Shared
// by every fresh-setup flavor so they all see exactly the same set of
// paths setup would write (tarball files, patch modifications, and
// patch-added files alike). Unpacks before anything is touched (a bad
// patch dies leaving the checkout intact).
std::string build_fresh_tree(const Ctx& ctx, const ProjenyFile& cur,
                             TempDir& tmp)
{
    unpack_single_top(join_path(ctx.pdir, cur.archive), tmp.path, cur.origname);
    std::string fresh = join_path(tmp.path, cur.origname);
    if (!apply_patch_whole(fresh, cur.patch, cur.name,
                           scratch_parent_for(ctx.pdir))) {
        std::vector<VcsFailure> bad =
            apply_patch_per_file(fresh, cur.patch, cur.name);
        std::string detail;
        for (const auto& f : bad) {
            detail += "  " + f.display + "\n";
        }
        die("patch in '" + ctx.projeny_arg + "' does not apply to archive '" +
                cur.archive + "'",
            detail);
    }
    return fresh;
}

// Fresh setup: unpack the current archive, apply the current patch, and move
// the result into place. Unpacks before touching the workdir (a bad patch
// dies leaving the checkout intact), and carries untracked binaries across
// the replacement so build junk and package tarballs survive.
void do_fresh_setup(const Ctx& ctx, const ProjenyFile& cur)
{
    std::string workdir = join_path(ctx.pdir, cur.name);
    TempDir tmp(scratch_parent_for(ctx.pdir), "projeny-setup-");
    std::string fresh = build_fresh_tree(ctx, cur, tmp);
    if (path_exists(workdir))
        reconcile_binaries(workdir, fresh, true);
    if (path_exists(workdir) && !remove_recursive(workdir))
        die("cannot remove existing workdir '" + workdir + "'");
    move_path(fresh, workdir);
    tmp.release(); // fresh moved out; don't delete it
}

// Inventory a tree as rel path -> is-it-a-directory, recursing into
// directories. lstat kinds throughout: a symlink is never a directory, so a
// symlink to a directory counts as a non-dir (soundness for the
// compatibility rule below — a link must never be descended into or
// silently classify as a directory). Hidden entries count; empty
// directories appear as their own entries.
void collect_rel_entries(const std::string& root, const std::string& prefix,
                         std::map<std::string, bool>* out)
{
    std::string dir = prefix.empty() ? root : join_path(root, prefix);
    for (const std::string& name : list_dir_names(dir)) {
        std::string rel = prefix.empty() ? name : prefix + "/" + name;
        std::string full = join_path(root, rel);
        struct stat st;
        if (lstat(full.c_str(), &st) != 0)
            die("cannot stat '" + full + "': " + strerror(errno));
        bool isdir = S_ISDIR(st.st_mode) != 0;
        (*out)[rel] = isdir;
        if (isdir)
            collect_rel_entries(root, rel, out);
    }
}

// Move every entry of from_dir into to_dir. Directory-on-directory pairs
// recurse (keeping what is already inside); everything else is moved with
// move_path — the caller's compatibility check has proven those
// destinations free. Entries of to_dir that from_dir never mentions stay
// put.
void merge_tree_into(const std::string& from_dir, const std::string& to_dir)
{
    for (const std::string& name : list_dir_names(from_dir)) {
        std::string src = join_path(from_dir, name);
        std::string dst = join_path(to_dir, name);
        if (is_dir(src) && is_dir(dst)) {
            merge_tree_into(src, dst);
            continue;
        }
        move_path(src, dst);
    }
}

// Set up into an existing directory that has no status file: the directory
// was never set up by projeny (or its bookkeeping was deleted), so there is
// no base to merge against. The directory can still be adopted in place
// when it holds NOTHING that this setup would write: the fresh tree is
// built first in a temp dir (no side effects on the workdir, so a refusal
// leaves it completely untouched), its relative paths are compared against
// the directory's, and the setup proceeds only when every shared path is a
// directory on both sides — i.e. the trees merely nest into each other. A
// file (or symlink) at a path the tarball or patch would write — or a
// directory where a file is needed — is a conflict. Anything already in
// the directory that setup never touches (notes, VCS metadata, build
// scripts) is kept as-is and rides along like a user-added file.
void do_fresh_setup_into_existing(const Ctx& ctx, const ProjenyFile& cur)
{
    std::string workdir = join_path(ctx.pdir, cur.name);
    TempDir tmp(scratch_parent_for(ctx.pdir), "projeny-setup-");
    std::string fresh = build_fresh_tree(ctx, cur, tmp);

    // Compatibility check: conflict iff a relative path exists in both
    // trees and the two entries are not both directories.
    std::map<std::string, bool> incoming, existing;
    collect_rel_entries(fresh, "", &incoming);
    collect_rel_entries(workdir, "", &existing);
    std::vector<std::string> conflicts;
    for (const auto& e : incoming) {
        auto it = existing.find(e.first);
        if (it == existing.end())
            continue; // new path: nothing there to overwrite
        if (e.second && it->second)
            continue; // directories on both sides: the trees just nest
        conflicts.push_back(e.first);
    }
    if (!conflicts.empty()) {
        const size_t kMaxListed = 10;
        std::string detail = "setup would overwrite these existing paths:\n";
        size_t shown = std::min(conflicts.size(), kMaxListed);
        for (size_t i = 0; i < shown; ++i)
            detail += "  " + conflicts[i] + "\n";
        if (conflicts.size() > shown)
            detail += "  ... and " +
                      std::to_string(conflicts.size() - shown) + " more\n";
        die("workdir '" + workdir + "' exists but status file '" +
                ctx.statusfile + "' is missing, and setup would overwrite " +
                std::to_string(conflicts.size()) +
                " existing path(s) in it; refusing (remove those files or "
                "the whole workdir, or restore the status file)",
            detail);
    }

    // Compatible: merge the fresh tree's entries into the directory.
    merge_tree_into(fresh, workdir);
    tmp.release(); // entries moved out; don't delete the temp tree
}

// Conflicted setup: the .projeny file holds git conflict markers (from
// `git pull --rebase`, `git stash pop`, `git merge`, ...). This is the ONLY
// path that handles them; every other command refuses outright.
//
// Git swaps the conflict sides depending on the operation:
//
//   git merge:        <<<<<<< HEAD holds local (ours), >>>>>>> names the
//                     merged branch (upstream/theirs).
//   git pull --rebase / rebase: HEAD is the upstream commit the local
//                     commits replay onto, so <<<<<<< HEAD holds upstream
//                     and >>>>>>> names the local commit.
//   git stash pop:    <<<<<<< Updated upstream holds the checkout,
//                     >>>>>>> Stashed changes holds the local change.
//
// The direction is resolved by conflict_sides_swapped() (labels first,
// then the status copy, dying loudly on ambiguity instead of defaulting
// to the merge convention). The upstream side is force-taken for the .projeny file
// itself and the status copy, and the workdir becomes the local changes
// plus conflicts: the local-side patch merged onto a fresh upstream-side
// setup, with a genuine three-way merge per conflicting file. When the
// checkout additionally holds uncommitted changes versus the status file
// (harder case), those are applied on top afterwards. Conflicts are
// recorded in the status file exactly like normal setup conflicts, so
// `resolve`/`commit` work unchanged.
//
// Nothing is written (no .projeny/.status/workdir changes) until every
// input has parsed and the merged tree is fully computed, so failures here
// never lose data.
std::string lower_copy(const std::string& s)
{
    std::string o = s;
    for (char& c : o)
        c = (char)tolower((unsigned char)c);
    return o;
}

// True when `text` is empty or only whitespace: one side of a git
// delete/modify conflict (git leaves the deleted side blank).
bool is_blank_text(const std::string& text)
{
    return trim(text).empty();
}

// Resolve which split side holds the upstream content. Returns true when
// the sides are swapped relative to merge convention, i.e. split.ours
// (the <<<<<<< side) actually holds upstream. Dies loudly when the
// direction is ambiguous instead of guessing. Signals, strongest first:
//
//  1. branch labels: the full phrases "updated upstream" (the `git stash
//     pop` opener, holding the checkout/upstream content) and "stashed
//     changes" (the `git stash pop` closer, holding the local content)
//     vote for their side. A bare "stash"/"stashed" token only votes when
//     paired with a "changes"/"wip"/"index" token, so a branch that merely
//     mentions stash (e.g. "stash-cleanup") does NOT vote: it is just a
//     branch name, not a stash operation. "upstream"/"origin"/"remote"/
//     "theirs" vote only as whole '/'-separated ref components (so a real
//     ref like "origin/master" votes, while "original-work",
//     "upstream-fix" or "remotely" do not: their components never equal a
//     keyword). A bare "HEAD" never votes: it is local in merges but
//     upstream in rebases. Branch names that merely contain a keyword
//     inside a longer token (e.g. "remotely") do not vote.
//  2. rebase-style closer: `git rebase`, `git pull --rebase`, `git
//     cherry-pick` and friends label the closing side with the replayed
//     commit ("<short-sha> (<subject>)", e.g. "99d93bd (local beta
//     work)"), while the <<<<<<< HEAD side holds upstream. A closer that
//     starts with a hex commit hash therefore votes "swapped". This is
//     what rescues the no-status (and stale-status) rebase case, where
//     neither the keyword vote nor the status copy below can fire.
//  3. the status copy: it embeds the .projeny as of the last
//     setup/commit. When exactly one side equals it byte-for-byte, that
//     side continues the local lineage.
//  4. otherwise: die ("ambiguous conflict direction") instead of defaulting
//     to the merge convention. A short-hex branch name ("cafe", "beef"),
//     a stash-mentioning branch ("stash-cleanup"), or any other voteless
//     label pair must never silently pick a side: with no status copy to
//     break the tie there is no evidence at all, and taking the local side
//     by default would discard upstream (or vice versa). The same holds
//     when the status copy matches neither side (or there is none) and the
//     votes tie: even a one-sided-validity guess is refused, because the
//     "valid" side may be stale lineage while the other holds the true
//     upstream (or both may be garbage that deserves a precise error, not
//     a silent pick).
bool conflict_sides_swapped(const ProjenyConflictSplit& split,
                            const StatusData* old, const std::string& where)
{
    std::string op = lower_copy(split.opener_label);
    std::string cl = lower_copy(split.closer_label);
    // Whole-token match over the lowercased label `l`: split on
    // non-alphanumeric characters and compare each token against `word`
    // (same tokenization as the stash matcher below, so "original-work"
    // never matches "origin" and "remotely" never matches "remote").
    auto has_token_word = [](const std::string& l, const char* word) {
        size_t wn = strlen(word);
        size_t i = 0;
        while (i < l.size()) {
            while (i < l.size() && !isalnum((unsigned char)l[i]))
                ++i;
            size_t j = i;
            while (j < l.size() && isalnum((unsigned char)l[j]))
                ++j;
            if (j - i == wn && l.compare(i, j - i, word) == 0)
                return true;
            i = j;
        }
        return false;
    };
    auto has_upstream_word = [&](const std::string& l) {
        // The `git stash pop` opener phrase always marks upstream.
        if (l.find("updated upstream") != std::string::npos)
            return true;
        // Otherwise only whole '/'-separated ref components count, so a
        // real ref like "origin/master" votes (its first component is
        // exactly "origin") while hyphen-joined branch names like
        // "original-work" or "upstream-fix" do not (their single component
        // never equals a keyword). Tokenizing on every non-alphanumeric
        // character here would false-positive on those branch names.
        size_t i = 0;
        for (;;) {
            size_t j = l.find('/', i);
            std::string comp =
                (j == std::string::npos) ? l.substr(i) : l.substr(i, j - i);
            comp = trim(comp);
            if (comp == "upstream" || comp == "origin" ||
                comp == "remote" || comp == "theirs")
                return true;
            if (j == std::string::npos)
                break;
            i = j + 1;
        }
        return false;
    };
    auto has_stash_word = [&](const std::string& l) {
        // The `git stash pop` closer phrase always marks the local side.
        if (l.find("stashed changes") != std::string::npos)
            return true;
        // Otherwise require a stash token PAIRED with a changes/wip/index
        // token: a lone "stash" token is usually just a branch name (e.g.
        // "stash-cleanup", "stash@{0}") and must not hijack a normal merge.
        bool stash = has_token_word(l, "stashed") || has_token_word(l, "stash");
        if (!stash)
            return false;
        return has_token_word(l, "changes") || has_token_word(l, "wip") ||
               has_token_word(l, "index");
    };
    // True when the closer looks like a rebased/cherry-picked commit label:
    // "<short-sha> (<subject>)" or a bare "<short-sha>" (e.g. "99d93bd
    // (local beta work)"). `git rebase`, `git pull --rebase` and `git
    // cherry-pick` all emit this shape with the local commit on the closing
    // side, so it votes "swapped" exactly like a "Stashed changes" closer.
    // A merge closer is a branch name, which only collides when someone
    // names a branch as 7-40 lowercase hex characters (vanishingly rare:
    // short branch names like "cafe" or "beef" are only 4 hex chars and
    // must NOT vote, since git short SHAs are 7+ characters). Matching
    // runs on the raw (non-lowercased) label and requires lowercase hex,
    // exactly as git abbreviates SHAs, so an uppercase branch name like
    // "DEADBEEF" does not vote.
    auto has_rebase_commit_closer = [](const std::string& l) {
        auto is_sha_char = [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        };
        size_t i = 0;
        while (i < l.size() && (l[i] == ' ' || l[i] == '\t'))
            ++i;
        size_t j = i;
        while (j < l.size() && is_sha_char(l[j]))
            ++j;
        size_t n = j - i;
        // Short SHAs are 7+ hex digits (git's default abbreviation is 7,
        // growing as needed); cap at 40 (full SHA-1). The hash must stand
        // alone: EOL, space, tab, or " (<subject>)" may follow, nothing
        // else glued on (so a branch like "deadbeef2u" does not vote).
        // Four-char hex branch names ("cafe", "beef") are deliberately
        // excluded: they are far below git's abbreviation length.
        if (n < 7 || n > 40)
            return false;
        if (j == l.size())
            return true;
        char c = l[j];
        return c == ' ' || c == '\t';
    };
    int swap_votes = 0, keep_votes = 0;
    if (has_upstream_word(op))
        ++swap_votes; // opener holds upstream content
    if (has_upstream_word(cl))
        ++keep_votes; // closer holds upstream content
    if (has_stash_word(op))
        ++keep_votes; // opener holds the local (stashed) content
    if (has_stash_word(cl))
        ++swap_votes; // closer holds the local (stashed) content
    if (has_rebase_commit_closer(split.closer_label))
        ++swap_votes; // closer names the replayed local commit (rebase)
    if (swap_votes != keep_votes)
        return swap_votes > keep_votes;
    if (old != nullptr) {
        bool ours_is_local = (split.ours == old->embedded);
        bool theirs_is_local = (split.theirs == old->embedded);
        if (ours_is_local != theirs_is_local)
            return theirs_is_local;
    }
    die("ambiguous conflict direction in '" + where + "' (labels '" +
        split.opener_label + "' / '" + split.closer_label +
        "' vote for neither side, and no status copy breaks the tie); "
        "projeny cannot tell the upstream side from the local side without "
        "guessing. Pick a side with 'git checkout --ours/--theirs -- " +
        where + "' (for 'git pull --rebase' the sides are swapped: "
        "--theirs is your local commit), or restore the file with 'git "
        "checkout -- <file>', then run 'projeny setup' again");
    return false; // unreachable
}

// Crash-recovery journal for conflicted setup.
//
// Writing the resolved .projeny file, the status file, and the workdir
// cannot be done in one atomic rename: a crash between the renames leaves
// a split brain (e.g. new .projeny + old .status with the markers gone),
// and a naive rerun then takes the normal merge path and silently discards
// the local patch (or takes the fresh-setup path when the workdir is
// missing and drops it entirely). The journal closes every crash window:
// it is written BEFORE anything else is touched and removed only after
// the new tree, the .projeny file, and the status file are all durable,
// so any interruption is detectable on rerun and the merge is recomputed
// deterministically from the journaled conflict instead of guessed.
//
// Format (LF, strict):
//   projeny setup journal v1\n
//   upstream: ours|theirs\n        (which split side holds upstream)
//   --- conflicted .projeny ---\n
//   <verbatim conflicted .projeny bytes to EOF>
//
// The recorded direction makes recovery unambiguous even when the status
// file was already overwritten by the crashed run (its embedded copy then
// equals the upstream side, which the status-copy rule would misread as
// the local lineage). The recorded raw bytes provide both sides, so the
// local side is never lost even after the .projeny file itself was
// overwritten.
struct ConflictJournal {
    bool ours_is_upstream = false;
    std::string raw;
};

std::string journal_path_for(const Ctx& ctx)
{
    return ctx.projeny_arg + ".setup-journal";
}

std::string serialize_journal(bool ours_is_upstream, const std::string& raw)
{
    std::string out = "projeny setup journal v1\n";
    out += ours_is_upstream ? "upstream: ours\n" : "upstream: theirs\n";
    out += "--- conflicted .projeny ---\n";
    out += raw;
    return out;
}

ConflictJournal parse_journal(const std::string& data,
                              const std::string& journal_path)
{
    auto corrupt = [&]() -> ConflictJournal {
        die("setup journal '" + journal_path +
            "' is corrupt; refusing to guess (the checkout may be a "
            "half-written conflicted merge). Restore the .projeny file "
            "with 'git checkout --ours/--theirs -- <file>' (or 'git "
            "checkout -- <file>'), delete '" + journal_path +
            "' and the workdir if it is stale, then run 'projeny setup' "
            "again");
        return ConflictJournal(); // unreachable
    };
    // Header lines are ours (LF); the raw tail is byte-exact (it may hold
    // CRLF from the conflicted file), so split on the first three '\n'.
    size_t p1 = data.find('\n');
    if (p1 == std::string::npos)
        return corrupt();
    size_t p2 = data.find('\n', p1 + 1);
    if (p2 == std::string::npos)
        return corrupt();
    size_t p3 = data.find('\n', p2 + 1);
    if (p3 == std::string::npos)
        return corrupt();
    if (data.compare(0, p1, "projeny setup journal v1") != 0)
        return corrupt();
    std::string side = data.substr(p1 + 1, p2 - p1 - 1);
    if (side != "upstream: ours" && side != "upstream: theirs")
        return corrupt();
    if (data.compare(p2 + 1, p3 - p2 - 1, "--- conflicted .projeny ---") !=
        0)
        return corrupt();
    ConflictJournal j;
    j.ours_is_upstream = (side == "upstream: ours");
    j.raw = data.substr(p3 + 1);
    return j;
}

int setup_conflicted_merge(const Ctx& ctx, const std::string& local_text,
                           const std::string& upstream_text, bool swapped,
                           const std::string* journal_to_write,
                           const StatusData* union_status,
                           const std::string& status_name,
                           const ProjenyFile* harder_base);

int setup_conflicted(const Ctx& ctx, const std::string& raw)
{
    ProjenyConflictSplit split =
        split_projeny_conflicts(raw, "'" + ctx.projeny_arg + "'");
    if (!split.conflicted)
        die("internal error: expected conflict markers");

    // One side deleted the file (delete/modify conflict): there is no
    // merged content to compute, so give git recovery guidance instead of
    // a bare "missing header" error.
    bool ours_gone = is_blank_text(split.ours);
    bool theirs_gone = is_blank_text(split.theirs);
    if (ours_gone || theirs_gone) {
        std::string which =
            (ours_gone && theirs_gone)
                ? "both sides are"
                : (ours_gone ? "the first (<<<<<<<) side is"
                             : "the second (>>>>>>>) side is");
        die("'" + ctx.projeny_arg +
            "' was deleted on one side of the git conflict (" + which +
            " empty); projeny cannot merge a deletion automatically. Pick a "
            "side with 'git checkout --ours/--theirs -- " +
            ctx.projeny_arg + "' (for 'git pull --rebase' the sides are "
            "swapped: --theirs is your local commit), or restore the file "
            "with 'git checkout -- <file>', then run 'projeny setup' again");
    }

    bool have_status = path_exists(ctx.statusfile);
    StatusData old;
    ProjenyFile statuspf;
    bool status_ok = false;
    if (have_status) {
        old = StatusData::parse(ctx.statusfile);
        if (projeny_has_conflict_markers(old.embedded)) {
            die("status file '" + ctx.statusfile +
                "' embeds a conflicted .projeny copy; delete '" +
                ctx.statusfile + "' (and the workdir if it is stale) and run "
                "'projeny setup' again");
        }
        statuspf = ProjenyFile::parse_bytes(
            old.embedded, "embedded copy in '" + ctx.statusfile + "'");
        status_ok = true;
    }

    // Direction: rebase/stash swap the sides relative to merge convention.
    // Dies loudly when the labels and the status copy leave the direction
    // ambiguous rather than guessing a side.
    bool swapped = conflict_sides_swapped(split, status_ok ? &old : nullptr,
                                          ctx.projeny_arg);
    std::string local_text = swapped ? split.theirs : split.ours;
    std::string upstream_text = swapped ? split.ours : split.theirs;
    std::string journal = serialize_journal(swapped, raw);
    return setup_conflicted_merge(
        ctx, local_text, upstream_text, swapped, &journal,
        status_ok ? &old : nullptr, status_ok ? statuspf.name : "",
        status_ok ? &statuspf : nullptr);
}

// Recover an interrupted conflicted setup using its journal (see
// ConflictJournal). Every crash window of setup_conflicted_merge converges
// here with enough state to recompute the same merge deterministically:
// the journal holds both conflict sides plus the recorded direction, so
// the local side survives even when the .projeny file was already
// overwritten, and the harder-case base is rebuilt from the recorded
// local side (never from a possibly already-overwritten status copy).
// Anything unrecognizable dies loudly with manual-recovery guidance;
// recovery never guesses and never silently discards.
int setup_recover(const Ctx& ctx, const std::string& raw)
{
    std::string journal_path = journal_path_for(ctx);
    std::string journal_raw = read_file_bytes(journal_path);
    ConflictJournal j = parse_journal(journal_raw, journal_path);
    ProjenyConflictSplit jsplit = split_projeny_conflicts(
        j.raw, "journal '" + journal_path + "'");
    if (!jsplit.conflicted)
        die("setup journal '" + journal_path +
            "' does not contain a conflicted .projeny copy; refusing to "
            "guess (restore the .projeny file with 'git checkout "
            "--ours/--theirs -- <file>' (or 'git checkout -- <file>'), "
            "delete '" + journal_path +
            "' and the workdir if it is stale, then run 'projeny setup' "
            "again)");
    bool cur_has_markers = projeny_has_conflict_markers(raw);
    if (cur_has_markers && raw != j.raw) {
        // The .projeny file holds a NEWER conflict than the journal (e.g.
        // another pull arrived mid-recovery): the journal is stale, so run
        // a fresh conflicted setup, which journals the new conflict.
        return setup_conflicted(ctx, raw);
    }
    std::string journal_upstream =
        j.ours_is_upstream ? jsplit.ours : jsplit.theirs;
    std::string journal_local =
        j.ours_is_upstream ? jsplit.theirs : jsplit.ours;
    if (!cur_has_markers && raw != journal_upstream) {
        // Split brain plus drift: the .projeny file is clean but matches
        // neither recorded side (hand-edited after the crash?). There is
        // no safe automatic choice — say so instead of discarding.
        die("'" + ctx.projeny_arg + "' is clean but matches neither side "
            "recorded in setup journal '" + journal_path +
            "' (interrupted conflicted setup plus later edits?); projeny "
            "cannot tell the upstream side from the local side. Restore "
            "the .projeny file with 'git checkout --ours/--theirs -- " +
            ctx.projeny_arg + "' (for 'git pull --rebase' the sides are "
            "swapped: --theirs is your local commit), or write the "
            "upstream content into it by hand, delete '" + journal_path +
            "' and the workdir if it is stale, then run 'projeny setup' "
            "again");
    }
    printf("projeny: found setup journal '%s'; recovering the interrupted "
           "conflicted setup\n",
           journal_path.c_str());
    // Union bookkeeping from the current status file when it parses (it
    // may be the pre-crash copy or the already-overwritten one — both are
    // well-formed, and the union is idempotent either way). The harder
    // case is ALWAYS based on the recorded local side: the status copy
    // may already embed the upstream side (crash after the status write),
    // which would mis-base the uncommitted diff.
    StatusData cur_status;
    const StatusData* union_status = nullptr;
    std::string status_name;
    if (path_exists(ctx.statusfile)) {
        cur_status = StatusData::parse(ctx.statusfile);
        if (projeny_has_conflict_markers(cur_status.embedded)) {
            die("status file '" + ctx.statusfile +
                "' embeds a conflicted .projeny copy; delete '" +
                ctx.statusfile + "' (and the workdir if it is stale) and "
                "run 'projeny setup' again");
        }
        ProjenyFile statuspf = ProjenyFile::parse_bytes(
            cur_status.embedded, "embedded copy in '" + ctx.statusfile + "'");
        status_name = statuspf.name;
        union_status = &cur_status;
    }
    ProjenyFile local_base = ProjenyFile::parse_bytes(
        journal_local, "local side recorded in '" + journal_path + "'");
    // `swapped` mirrors conflict_sides_swapped's convention (true when the
    // <<<<<<< side holds upstream); it only drives the "detected
    // rebase/stash-style" note, since the sides themselves are fixed above.
    bool swapped = j.ours_is_upstream;
    return setup_conflicted_merge(ctx, journal_local, journal_upstream,
                                  swapped, nullptr, union_status,
                                  status_name, &local_base);
}

// Shared conflicted-merge core used by setup_conflicted (fresh conflict)
// and setup_recover (journal-guided rerun after a crash). All inputs are
// read-only until the write phase; `union_status` supplies the
// added/removed/renamed bookkeeping and the stale-conflict union (null
// when there is no status file), `status_name` is the Name: recorded in
// that status ("" when none, used only as an extra workdir candidate),
// and `harder_base` supplies the base for the harder case (uncommitted
// workdir-vs-status diff re-applied on top; null disables it). When
// `journal_to_write` is non-null it is written as the crash-recovery
// journal before anything else is touched; a null pointer means the
// journal already exists (recovery) and is kept until the end.
int setup_conflicted_merge(const Ctx& ctx, const std::string& local_text,
                           const std::string& upstream_text, bool swapped,
                           const std::string* journal_to_write,
                           const StatusData* union_status,
                           const std::string& status_name,
                           const ProjenyFile* harder_base)
{
    ProjenyFile local = ProjenyFile::parse_bytes(
        local_text, "local side of '" + ctx.projeny_arg + "'");
    ProjenyFile cur = ProjenyFile::parse_bytes(
        upstream_text, "upstream side of '" + ctx.projeny_arg + "'");

    std::string cur_workdir = join_path(ctx.pdir, cur.name);
    std::string local_workdir = join_path(ctx.pdir, local.name);
    std::string status_workdir =
        status_name.empty() ? "" : join_path(ctx.pdir, status_name);
    std::vector<std::string> candidates;
    candidates.push_back(cur_workdir);
    if (local_workdir != cur_workdir)
        candidates.push_back(local_workdir);
    if (!status_workdir.empty() && status_workdir != cur_workdir &&
        status_workdir != local_workdir)
        candidates.push_back(status_workdir);
    std::string actual_workdir;
    int found = 0;
    for (auto& c : candidates) {
        if (path_exists(c) && !is_dir(c))
            die("'" + c + "' exists but is not a directory");
        if (is_dir(c)) {
            if (actual_workdir.empty())
                actual_workdir = c;
            ++found;
        }
    }
    if (found > 1) {
        std::string detail;
        for (auto& c : candidates) {
            if (is_dir(c))
                detail += "  " + c + "\n";
        }
        die("multiple workdirs exist next to conflicted '" + ctx.projeny_arg +
                "'; remove or rename all but one and run 'projeny setup' again",
            detail);
    }
    bool have_workdir = found > 0;

    std::string scratch = scratch_parent_for(ctx.pdir);
    // All trees below are temp-only until the write phase at the end.
    TempDir tN(scratch, "projeny-cN-");
    TempDir tO(scratch, "projeny-cO-");
    TempDir tB(scratch, "projeny-cB-");
    TempDir tE(scratch, "projeny-cE-");
    TempDir tS(scratch, "projeny-cS-");

    // Harder case first (reads only): diff the checkout against the base
    // reconstruction while the workdir is still the original.
    std::string U_unc;
    std::string snap;
    std::string Etree;
    bool harder = false;
    if (have_workdir && harder_base != nullptr) {
        // harder_base is status- or journal-derived, so its archive may
        // have been deleted from git since the last setup: use the
        // snapshot when it exists.
        Etree = build_tree_from_patch(
            tE, resolve_status_archive(join_path(ctx.pdir, harder_base->archive),
                                       "reconstruct the base tree for '" +
                                           ctx.projeny_arg + "'"),
            harder_base->origname, harder_base->name, harder_base->patch,
            "embedded patch in '" + ctx.statusfile + "'");
        U_unc = diff_trees(Etree, actual_workdir, harder_base->name);
        // Same accidental-deletion rule as normal setup: files that vanished
        // without an explicit rm/rename are restored, not deleted.
        {
            std::vector<std::string> keep;
            if (union_status != nullptr) {
                for (const auto& r : union_status->removed)
                    keep.push_back(r);
                for (const auto& rn : union_status->renamed)
                    keep.push_back(rn.first);
            }
            U_unc = vcs_drop_deletes_not_in(U_unc, harder_base->name, keep);
        }
        harder = !normalize_patch_text(U_unc).empty();
        if (harder) {
            snap = join_path(tS.path, "snap");
            copy_recursive(actual_workdir, snap);
        }
    }

    // Fresh upstream (theirs) and local (ours) trees. The local side comes
    // from a git-conflicted .projeny (or the setup journal), so git may
    // have just deleted its archive (upstream rebase to a new tarball):
    // read the snapshot when it exists.
    std::string Ntree = build_tree_from_patch(
        tN, join_path(ctx.pdir, cur.archive), cur.origname, cur.name, cur.patch,
        "upstream side of '" + ctx.projeny_arg + "'");
    std::string Otree = build_tree_from_patch(
        tO, resolve_status_archive(join_path(ctx.pdir, local.archive),
                                   "reconstruct the local side of '" +
                                       ctx.projeny_arg + "'"),
        local.origname, local.name, local.patch,
        "local side of '" + ctx.projeny_arg + "'");

    // Merge base: the shared base archive when both sides name the same
    // tarball and top dir (the common git-conflict case). Otherwise there
    // is no common ancestor, so the base stays an empty dir (missing files):
    // anything both sides changed differently then conflicts instead of
    // silently picking a side. No data loss either way.
    std::string Btree = tB.path;
    if (local.archive == cur.archive && local.origname == cur.origname) {
        unpack_single_top(resolve_status_archive(join_path(ctx.pdir, local.archive),
                                                 "reconstruct the merge base "
                                                 "for '" + ctx.projeny_arg +
                                                 "'"),
                          tB.path, local.origname);
        Btree = join_path(tB.path, local.origname);
    }

    // Stage 1: merge the committed local patch onto the fresh upstream tree.
    std::vector<std::string> conflicts;
    merge_user_diff_onto(Btree, Ntree, Otree, Ntree, cur.name, local.name,
                         local.patch, &conflicts);
    // Stage 2 (harder case): apply the uncommitted workdir-vs-base diff
    // on top of the conflicted checkout.
    if (harder) {
        merge_user_diff_onto(Etree, Ntree, snap, Ntree, cur.name,
                             harder_base->name, U_unc, &conflicts);
    }
    sort_unique(&conflicts);

    // Write phase: everything computed, now replace the checkout.
    //
    // Order matters for crash recovery (see ConflictJournal above). Each
    // file write is itself atomic (temp file + fsync + rename inside
    // write_file_bytes, so a crash never leaves a half-written file), but
    // the journal, the status file, the .projeny file, and the workdir
    // cannot all flip in one rename. The order below keeps every crash
    // window recoverable by just rerunning 'projeny setup', which finds
    // the journal and recomputes this same merge deterministically:
    //   1. write the journal (records the conflicted raw bytes plus which
    //      side is upstream) — before anything else is touched;
    //   2. write the status file (embedding the upstream text);
    //   3. write the .projeny file (upstream text);
    //   4. move the merged tree into place, fsync the parent dir;
    //   5. remove stale workdirs (other names) — only once the new tree
    //      and both files are durable, so a crash never loses a checkout
    //      that is still the only copy of some state;
    //   6. remove the journal last (the commit point: no journal means
    //      the merge completed).
    // A crash before (1) leaves the old checkout untouched (no journal, so
    // the rerun is just a fresh conflicted setup). Any later crash leaves
    // the journal behind and the rerun recovers: both conflict sides are
    // still available (from the journal when the .projeny file was already
    // overwritten), so the local patch can never be silently discarded.
    // Previously recorded (still unresolved) status conflicts are unioned
    // into the new list so a rerun or a stale status never silently drops
    // them.
    std::string journal_path = journal_path_for(ctx);
    if (journal_to_write != nullptr)
        write_file_bytes(journal_path, *journal_to_write);
    StatusData sd;
    sd.status = "setup";
    sd.conflicts = conflicts;
    if (union_status != nullptr) {
        sd.added = union_status->added;
        sd.removed = union_status->removed;
        sd.renamed = union_status->renamed;
        for (const auto& c : union_status->conflicts) {
            if (std::find(sd.conflicts.begin(), sd.conflicts.end(), c) ==
                sd.conflicts.end())
                sd.conflicts.push_back(c);
        }
    }
    sort_unique(&sd.conflicts);
    sd.embedded = upstream_text;
    write_status(ctx, sd);
    write_file_bytes(ctx.projeny_arg, upstream_text);
    if (!actual_workdir.empty())
        reconcile_binaries(actual_workdir, Ntree, true);
    if (path_exists(cur_workdir) && !remove_recursive(cur_workdir))
        die("cannot remove existing workdir '" + cur_workdir + "'");
    move_path(Ntree, cur_workdir);
    tN.release(); // Ntree moved out; don't delete it
    fsync_dir(ctx.pdir.empty() ? "." : ctx.pdir);
    for (auto& c : candidates) {
        if (c != cur_workdir && path_exists(c) && !remove_recursive(c))
            die("cannot remove existing workdir '" + c + "'");
    }
    if (unlink(journal_path.c_str()) != 0 && errno != ENOENT)
        die("cannot remove journal '" + journal_path + "': " +
            strerror(errno));
    if (swapped)
        printf("projeny: detected rebase/stash-style markers (upstream "
               "content first); taking that side for the .projeny file\n");
    if (!have_workdir && union_status == nullptr)
        printf("projeny: conflicted '%s' had no checkout; checked out upstream "
               "with local patch merged in\n",
               ctx.projeny_arg.c_str());
    else
        printf("projeny: resolved conflicted '%s' by taking upstream for the "
               ".projeny file and merging local changes into '%s'\n",
               ctx.projeny_arg.c_str(), cur_workdir.c_str());
    if (!sd.conflicts.empty()) {
        printf("projeny: merged with %zu conflict(s):\n", sd.conflicts.size());
        for (auto& c : sd.conflicts)
            printf("  %s\n", c.c_str());
        printf("projeny: setup left conflicts (exit 1); fix them, then "
               "`resolve` each file and `commit`\n");
        return 1;
    } else {
        printf("projeny: merged local changes onto '%s'\n", cur_workdir.c_str());
    }
    return 0;
}

} // namespace

int cmd_setup(const std::string& projeny_arg)
{
    Ctx ctx = resolve_ctx(projeny_arg);
    std::string raw;
    if (!try_read_file_bytes(ctx.projeny_arg, &raw)) {
        die("cannot read '" + ctx.projeny_arg +
            "' (missing?); if a git merge deleted the .projeny file, restore "
            "it with 'git checkout --ours/--theirs -- <file>' (for 'git pull "
            "--rebase' the sides are swapped: --theirs is your local commit) "
            "or 'git checkout -- <file>', then run 'projeny setup' again");
    }
    if (raw.find((char)0) != std::string::npos) {
        die("file '" + ctx.projeny_arg +
            "' contains NUL bytes (binary merge garbage?); refusing without "
            "touching the workdir or status file (restore the .projeny file "
            "with 'git checkout -- <file>' and run 'projeny setup' again)");
    }
    if (raw.find('\r') != std::string::npos) {
        warn("'" + ctx.projeny_arg +
             "' has CRLF line endings (check core.autocrlf/.gitattributes); "
             "proceeding with LF normalization");
    }
    // Only setup handles conflict markers; every other command dies in
    // ProjenyFile::parse_bytes.
    //
    // A leftover setup journal means an earlier conflicted setup was
    // interrupted between its renames (split brain: the .projeny file may
    // already be overwritten while the status/workdir lag behind). Recover
    // first — before the markers test below — so the rerun recomputes the
    // recorded merge instead of taking the normal path and silently
    // discarding the local patch.
    if (path_exists(journal_path_for(ctx)))
        return setup_recover(ctx, raw);
    if (projeny_has_conflict_markers(raw))
        return setup_conflicted(ctx, raw);
    {
        std::string problem = validate_projeny_bytes(raw);
        if (!problem.empty()) {
            die("file '" + ctx.projeny_arg + "' " + problem +
                "; refusing without touching the workdir or status file (if "
                "this came from a git merge, restore it with 'git checkout "
                "--ours/--theirs -- <file>' or 'git checkout -- <file>' and "
                "run 'projeny setup' again)");
        }
    }
    ProjenyFile cur =
        ProjenyFile::parse_bytes(raw, "'" + ctx.projeny_arg + "'");
    std::string workdir = join_path(ctx.pdir, cur.name);

    if (!path_exists(workdir)) {
        if (is_dir(workdir))
            die("internal error"); // unreachable
        // The checkout directory is gone, so the status file and any
        // archive snapshots are stale state from the removed checkout:
        // disregard them (warn + rename to '<name>.stale', '.stale2', ...)
        // before the fresh setup rebuilds everything. This is the plain
        // fresh-setup path — journal recovery and conflict handling
        // returned above — so the reconciliation is safe here. The old
        // status is parsed best-effort only to also catch the snapshot of
        // the archive IT recorded (which may differ from the current one);
        // an unparseable status file is renamed just the same.
        std::vector<std::string> stale_archives;
        stale_archives.push_back(cur.archive);
        {
            std::string old_status_raw;
            if (try_read_file_bytes(ctx.statusfile, &old_status_raw)) {
                std::string old_archive =
                    archive_from_status_bytes(old_status_raw);
                if (!old_archive.empty() && old_archive != cur.archive)
                    stale_archives.push_back(old_archive);
            }
        }
        disregard_stale_state(ctx, stale_archives);
        do_fresh_setup(ctx, cur);
        StatusData sd;
        sd.status = "setup";
        sd.embedded = cur.raw;
        write_status(ctx, sd);
        printf("projeny: set up '%s' from '%s'\n", workdir.c_str(),
               cur.archive.c_str());
        return 0;
    }
    if (!is_dir(workdir))
        die("workdir '" + workdir + "' exists but is not a directory");

    // Workdir exists but no status file does (neither the dotted form nor
    // the migrated legacy one). The directory was never set up by projeny,
    // so there is no base to merge against — but it can still be adopted in
    // place when it holds nothing that this setup would overwrite: it may
    // be empty, or hold only files the tarball and patch never touch
    // (notes, VCS metadata, ...), which are kept and ride along like
    // user-added files. Otherwise refuse: without a status file there is no
    // way to merge, so overwriting any existing path could destroy the only
    // copy of it.
    if (!path_exists(ctx.statusfile)) {
        do_fresh_setup_into_existing(ctx, cur);
        StatusData sd;
        sd.status = "setup";
        sd.embedded = cur.raw;
        write_status(ctx, sd);
        printf("projeny: set up '%s' from '%s' (into existing directory)\n",
               workdir.c_str(), cur.archive.c_str());
        return 0;
    }

    StatusData old = StatusData::parse(ctx.statusfile);
    ProjenyFile oldpf =
        ProjenyFile::parse_bytes(old.embedded, "embedded copy in '" + ctx.statusfile + "'");
    std::string old_workdir = join_path(ctx.pdir, oldpf.name);

    // Reconstruct the expected tree E from the statusfile's copy. The
    // archive comes from the status file, so it may have been deleted from
    // git since (upstream rebase): read the snapshot copy when it exists.
    TempDir tE(scratch_parent_for(ctx.pdir), "projeny-E-");
    TempDir tN(scratch_parent_for(ctx.pdir), "projeny-N-");
    std::string Etree = build_tree_from_patch(
        tE, resolve_status_archive(join_path(ctx.pdir, oldpf.archive),
                                   "reconstruct the tree recorded by '" +
                                       ctx.statusfile + "'"),
        oldpf.origname, oldpf.name, oldpf.patch,
        "embedded patch in '" + ctx.statusfile + "'");

    // User diff U = workdir vs E. Note diff direction: diff_trees(base,
    // workdir) so applying U to a fresh tree reproduces the workdir.
    // Careful: if old Name != current Name, workdir lives at the OLD name.
    // The spec assumes Name is stable across the merge; if it changed, the
    // current workdir path (new name) doesn't exist. Handle: work at the
    // actual existing dir.
    std::string actual_workdir = workdir;
    std::string actual_wid = cur.name;
    if (!path_exists(actual_workdir) && path_exists(old_workdir) &&
        oldpf.name != cur.name) {
        actual_workdir = old_workdir;
        actual_wid = oldpf.name;
    }
    std::string U = diff_trees(Etree, actual_workdir, oldpf.name);
    // Files that vanished from the workdir without an explicit `projeny rm`
    // (or rename source) are accidental loss — a deleted build artifact, a
    // make bug removing a file, a stray `rm` — not intended deletions. Drop
    // those delete blocks so setup restores the files to fresh-setup state
    // (tarball + patch) instead of deleting them from the new tree. Only
    // pending removals and pending rename sources survive as deletions.
    {
        std::vector<std::string> keep;
        for (const auto& r : old.removed)
            keep.push_back(r);
        for (const auto& rn : old.renamed)
            keep.push_back(rn.first);
        U = vcs_drop_deletes_not_in(U, oldpf.name, keep);
    }

    if (normalize_patch_text(U) == normalize_patch_text(oldpf.patch)) {
        // No local changes: plain fresh setup from CURRENT .projeny. Keep
        // pending add/rm/mv ops (documented choice); previously recorded
        // conflicts are unioned, never silently dropped (like the
        // conflicted-.projeny path below).
        do_fresh_setup(ctx, cur);
        StatusData sd;
        sd.status = "setup";
        sd.added = old.added;
        sd.removed = old.removed;
        sd.renamed = old.renamed;
        sd.conflicts = old.conflicts;
        sort_unique(&sd.conflicts);
        sd.embedded = cur.raw;
        write_status(ctx, sd);
        if (!sd.conflicts.empty()) {
            printf("projeny: re-set up '%s' from '%s' (no local changes) but "
                   "%zu conflict(s) are still unresolved:\n",
                   workdir.c_str(), cur.archive.c_str(), sd.conflicts.size());
            for (auto& c : sd.conflicts)
                printf("  %s\n", c.c_str());
            return 1;
        }
        printf("projeny: re-set up '%s' from '%s' (no local changes)\n",
               workdir.c_str(), cur.archive.c_str());
        return 0;
    }

    // MERGE: fresh setup from current .projeny into a temp tree N, then apply
    // U onto N file-by-file, then move N into place. U carries the OLD wid
    // (oldpf.name); N carries the CURRENT wid (cur.name).
    std::string Ntree = build_tree_from_patch(
        tN, join_path(ctx.pdir, cur.archive), cur.origname, cur.name, cur.patch,
        "patch in '" + ctx.projeny_arg + "'");
    std::vector<std::string> conflicts;
    merge_user_diff_onto(Etree, Ntree, actual_workdir, Ntree, cur.name,
                         oldpf.name, U, &conflicts);
    // Move merged N into place: remove workdir (at old or new name), move N,
    // and if the name changed, the old dir is gone. Untracked binaries ride
    // along via reconcile first.
    reconcile_binaries(actual_workdir, Ntree, true);
    if (path_exists(old_workdir) && old_workdir != workdir) {
        if (!remove_recursive(old_workdir))
            die("cannot remove old workdir '" + old_workdir + "'");
    }
    if (path_exists(workdir) && !remove_recursive(workdir))
        die("cannot remove existing workdir '" + workdir + "'");
    move_path(Ntree, workdir);
    tN.release(); // Ntree moved out; don't delete it

    StatusData sd;
    sd.status = "setup";
    sd.conflicts = conflicts;
    // Previously recorded (still unresolved) conflicts are unioned into the
    // new list, never silently dropped: a clean re-merge of marker lines
    // as ordinary content must not clear them behind the user's back
    // (matches the conflicted-.projeny path, which unions the same way).
    for (auto& c : old.conflicts) {
        if (std::find(sd.conflicts.begin(), sd.conflicts.end(), c) ==
            sd.conflicts.end())
            sd.conflicts.push_back(c);
    }
    sort_unique(&sd.conflicts);
    // Pending ops refer to workdir-relative files; keep them (documented).
    sd.added = old.added;
    sd.removed = old.removed;
    sd.renamed = old.renamed;
    sd.embedded = cur.raw;
    write_status(ctx, sd);
    if (!sd.conflicts.empty()) {
        printf("projeny: merged with %zu conflict(s):\n", sd.conflicts.size());
        for (auto& c : sd.conflicts)
            printf("  %s\n", c.c_str());
        printf("projeny: setup left conflicts (exit 1); fix them, then "
               "`resolve` each file and `commit`\n");
        return 1;
    } else {
        printf("projeny: merged local changes onto '%s'\n", workdir.c_str());
    }
    return 0;
}

int cmd_commit(const std::string& projeny_arg)
{
    Ctx ctx = resolve_ctx(projeny_arg);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string cur_raw = read_file_bytes(ctx.projeny_arg);
    if (cur_raw != st.embedded)
        die("'" + ctx.projeny_arg +
            "' differs from the copy in '" + ctx.statusfile +
            "'; run setup to merge first");
    if (!st.conflicts.empty()) {
        std::string detail;
        for (auto& c : st.conflicts)
            detail += "  " + c + "\n";
        die("cannot commit with unresolved conflicts", detail);
    }
    ProjenyFile cur = ProjenyFile::parse_bytes(cur_raw, "'" + ctx.projeny_arg + "'");
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then keep the hard
        // error — there is nothing to diff against.
        disregard_stale_state(ctx, {cur.archive});
        die("workdir '" + workdir + "' is missing; run setup first");
    }

    // Validate pending ops against the workdir, then fold them into the
    // status bookkeeping (the diff itself already reflects on-disk state).
    for (const auto& a : st.added) {
        if (!path_exists(join_path(workdir, a)))
            die("pending add '" + a + "' does not exist in '" + workdir + "'");
    }
    for (const auto& rn : st.renamed) {
        if (!path_exists(join_path(workdir, rn.second)))
            die("pending rename '" + rn.first + " -> " + rn.second +
                "': destination missing in '" + workdir + "'");
        if (path_exists(join_path(workdir, rn.first)))
            die("pending rename '" + rn.first + " -> " + rn.second +
                "': source still exists in '" + workdir + "'");
    }
    // Removed entries: file should be gone from workdir (rm deletes it).
    // Tolerate either state; the diff decides.

    TempDir tmp(scratch_parent_for(ctx.pdir), "projeny-commit-");
    unpack_single_top(join_path(ctx.pdir, cur.archive), tmp.path, cur.origname);
    std::string base = join_path(tmp.path, cur.origname);
    // Binary files travel in the patch as base64 blocks (add/delete/modify),
    // so tracked-binary changes and explicitly added binaries commit like
    // text. Untracked binaries (never committed, never `add`ed — like a
    // package tarball written into the workdir) are filtered back out of the
    // patch, the way git leaves untracked files out of commits.
    reconcile_binaries(workdir, base, false);
    // diff_trees already returns canonical "a/<wid>/..." labels, so no
    // second canonicalization pass is needed (the old code double-wrapped
    // absolute paths and produced garbage labels). normalize_patch_text
    // already ends the patch with exactly one newline.
    std::string raw_patch = diff_trees(base, workdir, cur.name);
    // Commit filtering: only explicitly marked files change tracking state.
    //   - a file that disappeared without `projeny rm` (or a `projeny mv`
    //     source) is an error: refuse instead of silently deleting.
    //   - a new file that appeared without `projeny add` (or a `projeny mv`
    //     destination) is untracked: leave it out of the patch, like git.
    // Rename blocks (including mv pairs the differ paired by content) are
    // always kept, so `projeny mv` renders as a rename diff.
    // Previously committed adds/deletes are tracked too: the diff is
    // regenerated from scratch on every commit, so without them a recommit
    // would silently drop every committed addition or re-error on every
    // committed deletion. `keep` entries may name directories (add/rm take
    // dirs): files under them match via prefix, as is_tracked_path does.
    {
        std::vector<std::string> del_keep;
        for (const auto& r : st.removed)
            del_keep.push_back(r);
        for (const auto& rn : st.renamed)
            del_keep.push_back(rn.first);
        for (const auto& p : vcs_deleted_paths(cur.patch, cur.name)) {
            if (std::find(del_keep.begin(), del_keep.end(), p) == del_keep.end())
                del_keep.push_back(p);
        }
        auto covered = [&del_keep](const std::string& rel) -> bool {
            for (auto& k : del_keep) {
                if (k.empty())
                    continue;
                if (rel == k)
                    return true;
                if (rel.size() > k.size() && rel.compare(0, k.size(), k) == 0 &&
                    rel[k.size()] == '/')
                    return true;
            }
            return false;
        };
        // Authoritative disappeared check: walk the expected tree E (base
        // archive plus the current patch) and require every file missing
        // from the workdir to be covered above. This runs on the trees
        // themselves rather than the raw diff blocks, so content-based
        // rename pairing can never attribute a disappearance to the wrong
        // source when several same-content files changed hands at once.
        TempDir tmpE(scratch_parent_for(ctx.pdir), "projeny-commit-exp-");
        std::string Etree = build_tree_from_patch(
            tmpE, join_path(ctx.pdir, cur.archive), cur.origname, cur.name,
            cur.patch, "patch in '" + ctx.projeny_arg + "'");
        std::vector<std::string> disappeared;
        std::vector<std::string> stack;
        stack.push_back("");
        while (!stack.empty()) {
            std::string d = stack.back();
            stack.pop_back();
            std::string abs = d.empty() ? Etree : join_path(Etree, d);
            for (const std::string& name : list_dir_names(abs)) {
                std::string rel = d.empty() ? name : d + "/" + name;
                if (rel == ".projeny-tmp" ||
                    rel.compare(0, 13, ".projeny-tmp") == 0 ||
                    rel.find("/.projeny-tmp") != std::string::npos)
                    continue;
                std::string efull = join_path(Etree, rel);
                struct stat lst;
                if (lstat(efull.c_str(), &lst) != 0)
                    die("cannot stat '" + efull + "': " + strerror(errno));
                if (S_ISDIR(lst.st_mode)) {
                    stack.push_back(rel);
                    continue;
                }
                if (!path_exists(join_path(workdir, rel)) && !covered(rel))
                    disappeared.push_back(rel);
            }
        }
        sort_unique(&disappeared);
        if (!disappeared.empty()) {
            std::string detail;
            for (auto& dd : disappeared)
                detail += "  " + dd + "\n";
            die("cannot commit with disappeared files (deleted on disk but not "
                "marked with 'projeny rm'; restore them or run 'projeny rm' "
                "first)",
                detail);
        }
    }
    {
        std::vector<std::string> keep;
        for (const auto& a : st.added)
            keep.push_back(a);
        for (const auto& rn : st.renamed)
            keep.push_back(rn.second);
        // Previously committed adds are tracked too: the diff is
        // regenerated from scratch on every commit, so without them a
        // recommit would silently drop every committed addition.
        for (const auto& p : vcs_add_paths(cur.patch, cur.name)) {
            if (std::find(keep.begin(), keep.end(), p) == keep.end())
                keep.push_back(p);
        }
        for (const auto& p : vcs_binary_add_paths(cur.patch, cur.name)) {
            if (std::find(keep.begin(), keep.end(), p) == keep.end())
                keep.push_back(p);
        }
        raw_patch = vcs_drop_adds_not_in(raw_patch, cur.name, keep);
        raw_patch = vcs_drop_binary_adds_not_in(raw_patch, cur.name, keep);
    }
    std::string new_patch = normalize_patch_text(raw_patch);

    cur.rebuild(new_patch);
    write_file_bytes(ctx.projeny_arg, cur.raw);
    StatusData sd;
    sd.status = "setup";
    sd.embedded = cur.raw;
    write_status(ctx, sd);
    printf("projeny: committed new patch to '%s'\n", ctx.projeny_arg.c_str());
    return 0;
}

int cmd_add(const std::string& projeny_arg, const std::string& path)
{
    Ctx ctx = resolve_ctx(projeny_arg);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error —
        // there is no workdir to add anything to.
        disregard_stale_state(ctx, {cur.archive});
        die("workdir '" + workdir + "' is missing; run setup first");
    }
    std::string rel = normalize_workdir_rel(workdir, cur.name, path);
    if (!path_exists(join_path(workdir, rel)))
        die("path '" + path + "' does not exist in workdir '" + workdir + "'");
    // Adding a file that's already tracked by the base patch is allowed but
    // pointless; just record it idempotently.
    for (auto& a : st.added) {
        if (a == rel) {
            printf("projeny: '%s' already marked as added\n", rel.c_str());
            return 0;
        }
    }
    // Un-remove if it was pending removal.
    st.removed.erase(std::remove(st.removed.begin(), st.removed.end(), rel),
                     st.removed.end());
    st.added.push_back(rel);
    write_status(ctx, st);
    printf("projeny: marked '%s' as added\n", rel.c_str());
    return 0;
}

int cmd_rm(const std::string& projeny_arg, const std::string& path)
{
    Ctx ctx = resolve_ctx(projeny_arg);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error —
        // there is no workdir to remove anything from.
        disregard_stale_state(ctx, {cur.archive});
        die("workdir '" + workdir + "' is missing; run setup first");
    }
    std::string rel = normalize_workdir_rel(workdir, cur.name, path);
    std::string full = join_path(workdir, rel);
    // rm deletes the file from the workdir immediately AND records the
    // pending removal, so the deletion shows in the next commit's diff.
    if (path_exists(full) && !remove_recursive(full))
        die("cannot remove '" + full + "'");
    st.added.erase(std::remove(st.added.begin(), st.added.end(), rel), st.added.end());
    // Drop renames touching this path.
    {
        std::vector<std::pair<std::string, std::string>> kept;
        for (auto& rn : st.renamed) {
            if (rn.first != rel && rn.second != rel)
                kept.push_back(rn);
        }
        st.renamed = kept;
    }
    if (std::find(st.removed.begin(), st.removed.end(), rel) == st.removed.end())
        st.removed.push_back(rel);
    write_status(ctx, st);
    printf("projeny: marked '%s' as removed\n", rel.c_str());
    return 0;
}

int cmd_mv(const std::string& projeny_arg, const std::string& src,
           const std::string& dst)
{
    Ctx ctx = resolve_ctx(projeny_arg);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error —
        // there is no workdir to rename anything in.
        disregard_stale_state(ctx, {cur.archive});
        die("workdir '" + workdir + "' is missing; run setup first");
    }
    std::string srel = normalize_workdir_rel(workdir, cur.name, src);
    std::string drel = normalize_workdir_rel(workdir, cur.name, dst);
    if (srel == drel)
        die("source and destination are the same");
    std::string sfull = join_path(workdir, srel);
    std::string dfull = join_path(workdir, drel);
    if (!path_exists(sfull))
        die("source '" + src + "' does not exist in workdir '" + workdir + "'");
    if (path_exists(dfull))
        die("destination '" + dst + "' already exists in workdir '" + workdir + "'");
    make_dirs(dirname_of(dfull));
    move_path(sfull, dfull);
    // Maintain pending-op bookkeeping: moving a pending-added file keeps it
    // added under the new name; otherwise chain/append the rename.
    bool was_added = std::find(st.added.begin(), st.added.end(), srel) != st.added.end();
    if (was_added) {
        st.added.erase(std::remove(st.added.begin(), st.added.end(), srel),
                       st.added.end());
        if (std::find(st.added.begin(), st.added.end(), drel) == st.added.end())
            st.added.push_back(drel);
    } else {
        // If srel was itself a rename destination, chain to the original.
        std::string orig = srel;
        std::vector<std::pair<std::string, std::string>> kept;
        for (auto& rn : st.renamed) {
            if (rn.second == srel)
                orig = rn.first;
            else
                kept.push_back(rn);
        }
        st.renamed = kept;
        // Drop any rename whose source is now drel (cycles shouldn't happen).
        if (orig != drel)
            st.renamed.push_back({orig, drel});
    }
    // Moving onto a pending-removed path un-removes it.
    st.removed.erase(std::remove(st.removed.begin(), st.removed.end(), drel),
                     st.removed.end());
    write_status(ctx, st);
    printf("projeny: renamed '%s' -> '%s'\n", srel.c_str(), drel.c_str());
    return 0;
}

int cmd_resolve(const std::string& projeny_arg, const std::string& path)
{
    Ctx ctx = resolve_ctx(projeny_arg);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error — a
        // conflict list about files that no longer exist is meaningless.
        disregard_stale_state(ctx, {cur.archive});
        die("workdir '" + workdir + "' is missing; run setup first");
    }
    // Conflict entries are stored wid-relative (e.g. "src/a.c"), but users
    // naturally type the on-disk form ("<Name>/src/a.c", a CWD-relative
    // path, or an absolute path). Accept any of them: try (a) as-given
    // against the conflict list, (b) with a leading "<Name>/" prefix
    // stripped, (c) normalized against the workdir (CWD-relative/absolute).
    // Accept whichever hits the conflict list; error otherwise.
    std::string rel;
    bool hit = false;
    if (std::find(st.conflicts.begin(), st.conflicts.end(), path) !=
        st.conflicts.end()) {
        rel = path;
        hit = true;
    }
    if (!hit) {
        std::string stripped = path;
        if (stripped == cur.name)
            stripped = "";
        else if (starts_with(stripped, cur.name + "/"))
            stripped = stripped.substr(cur.name.size() + 1);
        if (!stripped.empty() &&
            std::find(st.conflicts.begin(), st.conflicts.end(), stripped) !=
                st.conflicts.end()) {
            rel = stripped;
            hit = true;
        }
    }
    if (!hit) {
        std::string norm = normalize_workdir_rel(workdir, cur.name, path);
        if (std::find(st.conflicts.begin(), st.conflicts.end(), norm) ==
            st.conflicts.end())
            die("'" + norm + "' is not listed as conflicted");
        rel = norm;
        hit = true;
    }
    size_t before = st.conflicts.size();
    st.conflicts.erase(std::remove(st.conflicts.begin(), st.conflicts.end(), rel),
                       st.conflicts.end());
    if (st.conflicts.size() == before)
        die("'" + rel + "' is not listed as conflicted");
    // Warn (don't error) when the file still carries conflict markers:
    // resolving then committing would bake "<<<<<<<" into the patch.
    std::string full = join_path(workdir, rel);
    std::string content;
    if (try_read_file_bytes(full, &content)) {
        bool has_markers = false;
        for (const std::string& line : split_lines(content)) {
            if (starts_with(line, "<<<<<<<") || starts_with(line, "=======") ||
                starts_with(line, ">>>>>>>") || starts_with(line, "|||||||")) {
                has_markers = true;
                break;
            }
        }
        if (has_markers)
            warn("'" + rel + "' still contains conflict markers");
    }
    write_status(ctx, st);
    printf("projeny: resolved '%s'\n", rel.c_str());
    return 0;
}

int cmd_rebase(const std::string& projeny_arg, const std::string& new_tarball)
{
    Ctx ctx = resolve_ctx(projeny_arg);

    // If status/workdir are missing, do a setup first.
    ProjenyFile cur0 = ProjenyFile::parse(ctx.projeny_arg);
    std::string workdir0 = join_path(ctx.pdir, cur0.name);
    if (!path_exists(ctx.statusfile) || !is_dir(workdir0)) {
        printf("projeny: no setup yet; running setup first\n");
        cmd_setup(projeny_arg);
    }

    StatusData st = StatusData::parse(ctx.statusfile);
    std::string cur_raw = read_file_bytes(ctx.projeny_arg);
    if (cur_raw != st.embedded)
        die("'" + ctx.projeny_arg +
            "' differs from the copy in '" + ctx.statusfile +
            "'; run setup to merge first");
    ProjenyFile cur = ProjenyFile::parse_bytes(cur_raw, "'" + ctx.projeny_arg + "'");
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir))
        die("workdir '" + workdir + "' is missing; run setup first");

    // Require a clean tree: workdir diff must equal the current patch. The
    // workdir diff MUST be computed with the same wid as the stored patch
    // (diff_workdir_vs_base uses the workdir basename, which differs when
    // Name != Origname... actually wid IS Name in both; but be explicit:
    // compare canonical forms). Also refuse when conflicts are pending:
    // conflict markers in the tree would otherwise be diffed as content.
    // Pending add/rm/mv ops are honored: the tree is "clean" when it matches
    // base+patch modulo exactly the recorded pending ops (verified below by
    // replaying the ops onto a fresh base+patch tree and diffing).
    if (!st.conflicts.empty()) {
        std::string detail;
        for (auto& c : st.conflicts)
            detail += "  " + c + "\n";
        die("workdir '" + workdir +
            "' has unresolved conflicts; resolve them before rebasing",
            detail);
    }
    {
        TempDir tC(scratch_parent_for(ctx.pdir), "projeny-rebase-clean-");
        unpack_single_top(join_path(ctx.pdir, cur.archive), tC.path,
                          cur.origname);
        std::string expect = join_path(tC.path, cur.origname);
        if (!normalize_patch_text(cur.patch).empty() &&
            !apply_patch_whole(expect, cur.patch, cur.name,
                               scratch_parent_for(ctx.pdir)))
            die("patch in '" + ctx.projeny_arg +
                "' does not apply to its own base archive '" + cur.archive +
                "'; run setup to repair first");
        // Replay recorded pending ops onto the expected tree, then require
        // the workdir to match exactly: any other delta is uncommitted work.
        for (const auto& a : st.added) {
            std::string wf = join_path(workdir, a);
            if (!path_exists(wf))
                die("pending add '" + a + "' does not exist in '" + workdir +
                    "'; resolve pending ops before rebasing");
            make_dirs(dirname_of(join_path(expect, a)));
            copy_path_preserving(wf, join_path(expect, a));
        }
        for (const auto& rn : st.renamed) {
            if (!path_exists(join_path(workdir, rn.second)))
                die("pending rename '" + rn.first + " -> " + rn.second +
                    "': destination missing in '" + workdir + "'");
            remove_recursive(join_path(expect, rn.first));
            make_dirs(dirname_of(join_path(expect, rn.second)));
            copy_path_preserving(join_path(workdir, rn.second),
                                 join_path(expect, rn.second));
        }
        for (const auto& r : st.removed)
            remove_recursive(join_path(expect, r));
        std::string U = diff_trees(expect, workdir, cur.name);
        if (!normalize_patch_text(U).empty())
            die("workdir '" + workdir +
                "' has uncommitted changes; commit or revert before rebasing");
    }

    if (!path_exists(new_tarball))
        die("new tarball '" + new_tarball + "' does not exist");
    std::string new_base = basename_of(new_tarball);
    if (new_base.empty() || new_base.find('/') != std::string::npos)
        die("bad tarball path '" + new_tarball + "'");

    // Copy the tarball into pdir if it isn't already there. When the new
    // tarball's basename matches the current Archive but its bytes differ,
    // warn (content changed under a familiar name) and continue with the new
    // file — never silently keep the old bytes.
    std::string dest_archive = join_path(ctx.pdir, new_base);
    std::string new_abs = absolutize(new_tarball);
    std::string dest_abs = absolutize(dest_archive);
    if (new_base == cur.archive && path_exists(dest_archive) &&
        new_abs != dest_abs) {
        if (file_hash_hex(new_tarball) != file_hash_hex(dest_archive) ||
            file_size_bytes(new_tarball) != file_size_bytes(dest_archive))
            warn("tarball '" + new_base +
                 "' differs from the current '" + cur.archive +
                 "'; using the new file");
    }
    if (new_abs != dest_abs)
        copy_file_bytes(new_tarball, dest_archive);

    std::string new_origname = archive_single_top_name(dest_archive);

    // Build the new tree: unpack new tarball, apply current patch, then
    // replay pending add/rm/mv ops so the result includes the pending intent
    // (the ops were validated against base+patch by the clean-check above).
    TempDir tmp(scratch_parent_for(ctx.pdir), "projeny-rebase-");
    unpack_single_top(dest_archive, tmp.path, new_origname);
    std::string tree = join_path(tmp.path, new_origname);
    std::vector<std::string> conflicts;
    if (!normalize_patch_text(cur.patch).empty() &&
        !apply_patch_whole(tree, cur.patch, cur.name,
                           scratch_parent_for(ctx.pdir))) {
        std::vector<VcsFailure> bad =
            apply_patch_per_file(tree, cur.patch, cur.name);
        // 3-way merge each failed file: base = OLD tree file, ours = new
        // tree file (patched except failed parts), theirs = old tree file.
        TempDir tO(scratch_parent_for(ctx.pdir), "projeny-rebase-old-");
        unpack_single_top(join_path(ctx.pdir, cur.archive), tO.path, cur.origname);
        std::string old_tree = join_path(tO.path, cur.origname);
        // Apply the old patch to the old tree to get the "theirs" content?
        // The old tree unpacked is the base; workdir == base+patch (clean
        // check above), so theirs-file == workdir file.
        for (const auto& f : bad) {
            std::vector<std::string> touched = f.paths;
            if (touched.empty() && !f.display.empty() &&
                f.display.find('\n') == std::string::npos &&
                f.display.find(" -> ") == std::string::npos &&
                f.display.compare(0, 5, "diff ") != 0 &&
                f.display != "<unknown file>" && f.display != "<combined diff>" &&
                f.display != "<rename>") {
                touched.push_back(f.display);
            }
            if (touched.empty()) {
                std::string d = f.display.empty() ? "<unknown file>" : f.display;
                die("cannot rebase: unsupported patch block '" + d +
                        "' cannot be merged; resolve manually",
                    "  " + d + "\n");
            }
            sort_unique(&touched);
            for (const std::string& rel : touched) {
                if (rel.empty())
                    continue;
                bool clean = merge_one_file(join_path(old_tree, rel),
                                            join_path(tree, rel),
                                            join_path(workdir, rel),
                                            join_path(tree, rel), tree);
                if (!clean)
                    conflicts.push_back(rel);
            }
        }
    }

    // Replay pending ops onto the rebased tree: adds/renames copy the user's
    // on-disk content forward (the new tree may differ textually from the old
    // base), removals delete, and renames remove the source. The regenerated
    // patch below is diffed against this final tree, so pending intent is
    // folded into the stored patch while the ops ALSO stay listed as pending
    // (commit validates and formally folds them).
    for (const auto& a : st.added) {
        std::string wf = join_path(workdir, a);
        if (!path_exists(wf))
            die("pending add '" + a + "' vanished from '" + workdir + "'");
        make_dirs(dirname_of(join_path(tree, a)));
        copy_path_preserving(wf, join_path(tree, a));
    }
    for (const auto& rn : st.renamed) {
        std::string wf = join_path(workdir, rn.second);
        if (!path_exists(wf))
            die("pending rename '" + rn.first + " -> " + rn.second +
                "' vanished from '" + workdir + "'");
        remove_recursive(join_path(tree, rn.first));
        make_dirs(dirname_of(join_path(tree, rn.second)));
        copy_path_preserving(wf, join_path(tree, rn.second));
    }
    for (const auto& r : st.removed)
        remove_recursive(join_path(tree, r));

    // Update headers: Archive: -> new basename, Origname: -> new top dir.
    // The patch body's wid labels already use Name (unchanged).
    cur.head = replace_header_value(cur.head, "Archive", new_base);
    cur.head = replace_header_value(cur.head, "Origname", new_origname);
    cur.archive = new_base;
    cur.origname = new_origname;
    // Regenerate the patch against the new base so hunk positions/counts are
    // exact (content is base+patch by construction).
    {
        TempDir tB(scratch_parent_for(ctx.pdir), "projeny-rebase-base-");
        unpack_single_top(dest_archive, tB.path, new_origname);
        // diff_trees already returns canonical labels; normalize ends with
        // exactly one newline.
        std::string regen = normalize_patch_text(
            diff_trees(join_path(tB.path, new_origname), tree, cur.name));
        cur.rebuild(regen);
    }
    write_file_bytes(ctx.projeny_arg, cur.raw);

    reconcile_binaries(workdir, tree, true);
    if (path_exists(workdir) && !remove_recursive(workdir))
        die("cannot remove existing workdir '" + workdir + "'");
    move_path(tree, workdir);
    tmp.release();

    StatusData sd;
    sd.status = "setup";
    sd.conflicts = conflicts;
    sort_unique(&sd.conflicts);
    // Pending add/rm/mv operations survive the rebase (they describe
    // workdir-relative intent, still valid against the new base), matching
    // setup's merge path which also keeps them.
    sd.added = st.added;
    sd.removed = st.removed;
    sd.renamed = st.renamed;
    sd.embedded = cur.raw;
    write_status(ctx, sd);
    if (!sd.conflicts.empty()) {
        printf("projeny: rebased onto '%s' with %zu conflict(s):\n",
               new_base.c_str(), sd.conflicts.size());
        for (auto& c : sd.conflicts)
            printf("  %s\n", c.c_str());
    } else {
        printf("projeny: rebased onto '%s'\n", new_base.c_str());
    }
    if (!sd.added.empty() || !sd.removed.empty() || !sd.renamed.empty())
        printf("projeny: kept %zu added, %zu removed, %zu renamed pending op(s)\n",
               sd.added.size(), sd.removed.size(), sd.renamed.size());
    return 0;
}

int cmd_status(const std::string& projeny_arg)
{
    Ctx ctx = resolve_ctx(projeny_arg);
    if (!path_exists(ctx.statusfile)) {
        printf("projeny: '%s' is not set up (no status file)\n",
               projeny_arg.c_str());
        return 1;
    }
    StatusData st = StatusData::parse(ctx.statusfile);

    // The embedded .projeny copy names the tree the status file records; the
    // workdir lives at its Name, falling back to the current .projeny Name's
    // dir when only that one exists (Name may have changed since the last
    // setup/commit).
    ProjenyFile emb = ProjenyFile::parse_bytes(
        st.embedded, "embedded copy in '" + ctx.statusfile + "'");
    std::string workdir = join_path(ctx.pdir, emb.name);
    std::string cur_archive;
    {
        std::string cur_raw;
        if (try_read_file_bytes(ctx.projeny_arg, &cur_raw) &&
            !projeny_has_conflict_markers(cur_raw) &&
            validate_projeny_bytes(cur_raw).empty()) {
            ProjenyFile curpf =
                ProjenyFile::parse_bytes(cur_raw, "'" + ctx.projeny_arg + "'");
            cur_archive = curpf.archive;
            std::string curdir = join_path(ctx.pdir, curpf.name);
            if (!is_dir(workdir) && is_dir(curdir))
                workdir = curdir;
        }
    }
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status file and the archive
        // snapshots are stale state from the removed checkout. Disregard
        // them (warn + rename to '<name>.stale', '.stale2', ...), report the
        // recorded state below, and skip the live diff (nothing to diff).
        std::vector<std::string> archives;
        archives.push_back(emb.archive);
        if (!cur_archive.empty() && cur_archive != emb.archive)
            archives.push_back(cur_archive);
        disregard_stale_state(ctx, archives);
    }

    printf("Status: %s\n", st.status.c_str());
    for (auto& c : st.conflicts)
        printf("Conflict: %s\n", c.c_str());
    for (auto& a : st.added)
        printf("Added: %s\n", a.c_str());
    for (auto& r : st.removed)
        printf("Removed: %s\n", r.c_str());
    for (auto& rn : st.renamed)
        printf("Renamed: %s -> %s\n", rn.first.c_str(), rn.second.c_str());

    // Live workdir state vs the expected tree (base archive + embedded
    // patch): modified files, disappeared files (in the expected tree but
    // missing on disk and not marked removed/rename-source), and untracked
    // files (on disk but in neither the expected tree nor the pending
    // added/rename-destination sets). Pending ops themselves stay on their
    // Added:/Removed:/Renamed: lines above and are not repeated here.
    do {
        if (!is_dir(workdir))
            break;
        // Snapshot-aware and fully tolerant: prefer the snapshot (the
        // byte-exact copy of what the last setup actually used, which
        // survives git deleting the archive), fall back to the archive
        // itself (checkouts set up before snapshots existed — and copy it
        // into the snapshot, best effort, so the next run finds one), and
        // skip the live-diff section entirely when neither exists.
        std::string archive_path = join_path(ctx.pdir, emb.archive);
        migrate_snapshot(archive_path);
        std::string snap = snapshot_path_for(archive_path);
        if (path_exists(snap)) {
            archive_path = snap;
        } else if (path_exists(archive_path)) {
            // Copy-on-fallback, best effort: status must never hard-fail
            // just because the snapshot cannot be written.
            std::string err;
            if (try_copy_file_bytes(archive_path, snap, &err))
                archive_path = snap;
            else
                warn("could not create snapshot '" + snap + "' from archive '" +
                     archive_path + "': " + err + " (continuing)");
        } else {
            break;
        }
        TempDir tmp(scratch_parent_for(ctx.pdir), "projeny-status-");
        std::string Etree = build_tree_from_patch(
            tmp, archive_path, emb.origname, emb.name, emb.patch,
            "embedded patch in '" + ctx.statusfile + "'");

        struct Entry {
            int kind = 0; // 0=regular, 1=symlink, 2=other
            std::string content; // regular: bytes; symlink: target
            bool exec = false;
        };
        auto is_scratch = [](const std::string& rel) -> bool {
            if (rel == ".projeny-tmp" ||
                rel.compare(0, 13, ".projeny-tmp") == 0)
                return true;
            return rel.find("/.projeny-tmp") != std::string::npos;
        };
        std::function<void(const std::string&, const std::string&,
                           std::map<std::string, Entry>&)>
            collect = [&](const std::string& root, const std::string& rel,
                          std::map<std::string, Entry>& out) {
                std::string full = rel.empty() ? root : join_path(root, rel);
                struct stat lst;
                if (lstat(full.c_str(), &lst) != 0)
                    return; // raced deletion; diff will catch it next time
                if (S_ISDIR(lst.st_mode)) {
                    for (const std::string& name : list_dir_names(full)) {
                        std::string child =
                            rel.empty() ? name : rel + "/" + name;
                        if (is_scratch(child))
                            continue;
                        collect(root, child, out);
                    }
                    return;
                }
                if (is_scratch(rel))
                    return;
                Entry e;
                if (S_ISLNK(lst.st_mode)) {
                    e.kind = 1;
                    std::vector<char> buf(lst.st_size > 0
                                              ? (size_t)lst.st_size + 1
                                              : 4096);
                    ssize_t r =
                        readlink(full.c_str(), buf.data(), buf.size());
                    if (r >= 0)
                        e.content.assign(buf.data(), (size_t)r);
                } else if (S_ISREG(lst.st_mode)) {
                    e.kind = 0;
                    e.exec = (lst.st_mode & 0111) != 0;
                    std::string data;
                    if (try_read_file_bytes(full, &data))
                        e.content = data;
                } else {
                    e.kind = 2;
                }
                out[rel] = e;
            };
        std::map<std::string, Entry> emap, wmap;
        collect(Etree, "", emap);
        collect(workdir, "", wmap);

        auto covered_by = [](const std::vector<std::string>& lst,
                             const std::string& rel) -> bool {
            for (auto& k : lst) {
                if (k.empty())
                    continue;
                if (rel == k)
                    return true;
                if (rel.size() > k.size() && rel.compare(0, k.size(), k) == 0 &&
                    rel[k.size()] == '/')
                    return true;
            }
            return false;
        };
        std::vector<std::string> rm_cover = st.removed;
        std::vector<std::string> add_cover = st.added;
        for (auto& rn : st.renamed) {
            rm_cover.push_back(rn.first);
            add_cover.push_back(rn.second);
        }
        std::vector<std::string> modified, disappeared, untracked;
        for (auto& kv : emap) {
            auto it = wmap.find(kv.first);
            if (it == wmap.end()) {
                if (!covered_by(rm_cover, kv.first))
                    disappeared.push_back(kv.first);
            } else {
                const Entry& a = kv.second;
                const Entry& b = it->second;
                bool same = (a.kind == b.kind && a.content == b.content &&
                             a.exec == b.exec);
                if (!same && !covered_by(rm_cover, kv.first))
                    modified.push_back(kv.first);
            }
        }
        for (auto& kv : wmap) {
            if (emap.find(kv.first) == emap.end() &&
                !covered_by(add_cover, kv.first))
                untracked.push_back(kv.first);
        }
        sort_unique(&modified);
        sort_unique(&disappeared);
        sort_unique(&untracked);
        for (auto& m : modified)
            printf("Modified: %s\n", m.c_str());
        for (auto& d : disappeared)
            printf("Disappeared: %s\n", d.c_str());
        for (auto& u : untracked)
            printf("Untracked: %s\n", u.c_str());
    } while (0);
    return 0;
}

int cmd_diff(const std::string& dir, const std::string& other_dir)
{
    if (!is_dir(dir))
        die("cannot diff: '" + dir + "' is not a directory");
    if (!is_dir(other_dir))
        die("cannot diff: '" + other_dir + "' is not a directory");
    // Labels use the second directory's basename, so `projeny diff A B`
    // prints a patch that `projeny patch` can apply to a copy of A.
    std::string wid = basename_of(strip_trailing_slashes(other_dir));
    if (wid.empty() || wid == "." || wid == "..")
        die("cannot diff: bad directory name '" + other_dir + "'");
    std::string patch = diff_trees(dir, other_dir, wid);
    if (!patch.empty())
        fwrite(patch.data(), 1, patch.size(), stdout);
    return 0;
}

namespace {
// First path component shared by every file side of every "diff --git"
// block in `patch` (a/b prefixes stripped, git C-quotes honored), or ""
// when there is none. A projeny-style patch labels every file
// "a/<wid>/..." / "b/<wid>/...", so the shared component is the patch's
// wid; a plain a/b-label patch spanning several top-level directories has
// no shared component. Single-component sides ("a/f.c", top-level files)
// never count as wid evidence.
std::string patch_wid_hint(const std::string& patch)
{
    std::string shared;
    bool any = false;
    bool ok = true;
    for (const std::string& raw_line : split_lines(patch)) {
        std::string line = ltrim(raw_line);
        if (!starts_with(line, "diff --git "))
            continue;
        std::string rest = line.substr(11);
        std::vector<std::string> sides;
        while (!rest.empty() && sides.size() < 2) {
            if (rest[0] == '"') {
                size_t j = 1;
                while (j < rest.size()) {
                    if (rest[j] == '\\') {
                        j += 2;
                        continue;
                    }
                    if (rest[j] == '"')
                        break;
                    ++j;
                }
                if (j >= rest.size())
                    break; // unterminated: give up on this line
                sides.push_back(rest.substr(0, j + 1));
                rest = ltrim(rest.substr(j + 1));
            } else {
                size_t sp = rest.find(' ');
                if (sp == std::string::npos) {
                    sides.push_back(rest);
                    rest = "";
                } else {
                    sides.push_back(rest.substr(0, sp));
                    rest = ltrim(rest.substr(sp + 1));
                }
            }
        }
        if (sides.size() != 2) {
            ok = false;
            break;
        }
        for (auto& side : sides) {
            std::string u = unquote_git_path(side);
            if (u == "/dev/null")
                continue;
            if (starts_with(u, "a/") || starts_with(u, "b/"))
                u = u.substr(2);
            size_t slash = u.find('/');
            if (slash == std::string::npos || slash == 0) {
                ok = false;
                break;
            }
            std::string comp = u.substr(0, slash);
            if (!any) {
                shared = comp;
                any = true;
            } else if (comp != shared) {
                ok = false;
                break;
            }
        }
        if (!ok)
            break;
    }
    if (!ok || !any)
        return "";
    return shared;
}
} // namespace

int cmd_patch(const std::string& dir, const std::string& patch_file)
{
    if (!is_dir(dir))
        die("cannot patch: '" + dir + "' is not a directory");
    std::string patch = read_file_bytes(patch_file);
    std::string wid = basename_of(strip_trailing_slashes(dir));
    if (wid.empty() || wid == "." || wid == "..")
        die("cannot patch: bad directory name '" + dir + "'");
    if (!normalize_patch_text(patch).empty()) {
        // Refuse garbage input early: a non-empty patch file must hold at
        // least one diff block, otherwise a typo'd path would "succeed".
        bool any = false;
        for (const std::string& line : split_lines(patch)) {
            std::string t = ltrim(line);
            if (starts_with(t, "diff --git ") || starts_with(t, "diff --cc ") ||
                starts_with(t, "diff --combined ")) {
                any = true;
                break;
            }
        }
        if (!any)
            die("patch file '" + patch_file + "' contains no diff blocks");
    }
    // The patch's wid need not match the target directory's basename
    // (`projeny diff A B` labels with B's name; the patch may then be
    // applied to a copy of A under any name). Score each candidate wid by
    // how many touched paths already exist in the target: the right wid
    // resolves to real files, a wrong one to missing subdir-prefixed
    // paths (trial application would "succeed" there by creating junk, so
    // existence scoring is used instead). Ties prefer the patch-derived
    // wid (authoritative when consistent), then the basename, then none.
    std::vector<std::string> candidates;
    {
        std::string hint = patch_wid_hint(patch);
        if (!hint.empty())
            candidates.push_back(hint);
        if (std::find(candidates.begin(), candidates.end(), wid) ==
            candidates.end())
            candidates.push_back(wid);
        if (std::find(candidates.begin(), candidates.end(), "") ==
            candidates.end())
            candidates.push_back("");
    }
    std::string use_wid = candidates[0];
    if (!normalize_patch_text(patch).empty()) {
        size_t best = 0;
        bool have = false;
        for (auto& cand : candidates) {
            size_t hits = 0;
            for (auto& rel : patch_touched_paths(patch, cand)) {
                if (path_exists(join_path(dir, rel)))
                    ++hits;
            }
            if (!have || hits > best) {
                best = hits;
                use_wid = cand;
                have = true;
            }
        }
    }
    std::vector<std::string> conflicts;
    bool clean = apply_patch_with_conflicts(dir, patch, use_wid,
                                            system_scratch_parent(),
                                            &conflicts);
    sort_unique(&conflicts);
    if (clean) {
        printf("projeny: patched '%s'\n", dir.c_str());
        return 0;
    }
    printf("projeny: patched '%s' with %zu conflict(s):\n", dir.c_str(),
           conflicts.size());
    for (auto& c : conflicts)
        printf("  %s\n", c.c_str());
    return 0;
}

// ---- package / extract ----
//
// `package` is the projeny equivalent of package-source.sh: run `setup`
// (so uncommitted workdir changes are included, and conflicts fail the
// command), then tar up exactly the tracked files — the fresh base+patch
// tree plus pending adds/renames, minus pending removals. Untracked files
// (never committed, never `add`ed) are left out, just like `git archive`
// leaves them out. `extract` does the same except it populates a directory
// instead of creating an archive (the projeny variant of extract_source).

namespace {

// Resolve a `package`/`extract` project argument to a .projeny file path.
// Accepts either the .projeny file itself or a directory: a directory holding
// exactly one "*.projeny" file names it implicitly, otherwise a "<dir>.projeny"
// sibling is used (so both the workdir and a project dir work). Dies otherwise.
std::string resolve_projeny_path(const std::string& arg, const char* cmd)

{
    std::string a = strip_trailing_slashes(arg);
    if (a.empty())
        a = ".";
    if (is_dir(a)) {
        std::vector<std::string> cands;
        for (const std::string& n : list_dir_names(a)) {
            if (ends_with(n, ".projeny"))
                cands.push_back(join_path(a, n));
        }
        if (cands.size() == 1)
            return cands[0];
        if (cands.empty()) {
            std::string sib = a + ".projeny";
            if (path_exists(sib) && !is_dir(sib))
                return sib;
            die(std::string("cannot ") + cmd + " '" + arg +
                "': directory holds no .projeny file (nor a '" + sib +
                "' sibling); name the .projeny file explicitly");
        }
        std::string detail;
        for (auto& c : cands)
            detail += "  " + c + "\n";
        die(std::string("cannot ") + cmd + " '" + arg +
            "': directory holds multiple .projeny files; name one explicitly",
            detail);
    }
    return arg;
}

// Compression for `package`, autodetected from the output name, plus the
// top-level directory name stored in the archive (the output basename with
// the compression suffix stripped, like package-source.sh's $name prefix).
struct ArchiveKind {
    std::string comp; // "" | "-z" | "-j" | "-J" | "--zstd" (tar filter flag)
    std::string prefix;
};

ArchiveKind classify_package_output(const std::string& output)
{
    std::string base =
        basename_of(strip_trailing_slashes(output));
    static const struct {
        const char* suf;
        const char* comp;
    } table[] = {
        {".tar.gz", "-z"}, {".tgz", "-z"},
        {".tar.bz2", "-j"}, {".tbz2", "-j"}, {".tbz", "-j"},
        {".tar.xz", "-J"}, {".txz", "-J"},
        {".tar.zst", "--zstd"}, {".tzst", "--zstd"},
        {".tar", ""},
    };
    for (auto& e : table) {
        std::string suf = e.suf;
        if (base.size() > suf.size() &&
            base.compare(base.size() - suf.size(), suf.size(), suf) == 0) {
            std::string prefix = base.substr(0, base.size() - suf.size());
            if (prefix.empty() || prefix == "." || prefix == ".." ||
                prefix.find('/') != std::string::npos)
                die("bad package output name '" + output + "'");
            return ArchiveKind{e.comp, prefix};
        }
    }
    die("cannot determine archive format from '" + output +
        "': expected one of .tar, .tar.gz/.tgz, .tar.bz2/.tbz2/.tbz, "
        ".tar.xz/.txz, .tar.zst/.tzst");
    return ArchiveKind{};
}

bool under_path(const std::string& rel, const std::string& base)
{
    return rel == base || starts_with(rel, base + "/");
}

// True when workdir-relative `rel` is tracked: present in the fresh
// base+patch tree, pending-added (or under a pending-added dir), or a pending
// rename destination — and not pending-removed. Untracked files (in neither)
// are left out of packages, like `git archive` leaves them out.
bool is_tracked_path(const std::string& rel, const std::string& fresh_root,
                     const StatusData& st)
{
    for (auto& r : st.removed) {
        if (under_path(rel, r))
            return false;
    }
    for (auto& rn : st.renamed) {
        if (under_path(rel, rn.second))
            return true;
    }
    for (auto& a : st.added) {
        if (under_path(rel, a))
            return true;
    }
    return path_exists(join_path(fresh_root, rel));
}

// Copy every tracked file/symlink under `workdir` into `dest` (which must
// already exist), recreating parent dirs on demand. Untracked files are
// skipped; `skip_abs` (the package output itself, when it sits inside the
// workdir) is skipped too. Empty dirs are never created (diffs cannot
// represent them either). Dies on tracked files of unsupported type.
void stage_tracked(const std::string& workdir, const std::string& fresh_root,
                   const StatusData& st, const std::string& skip_abs,
                   const std::string& dest, size_t* count)
{
    std::vector<std::string> stack;
    stack.push_back("");
    while (!stack.empty()) {
        std::string d = stack.back();
        stack.pop_back();
        std::string abs = d.empty() ? workdir : join_path(workdir, d);
        for (const std::string& name : list_dir_names(abs)) {
            std::string rel = d.empty() ? name : d + "/" + name;
            std::string full = join_path(workdir, rel);
            struct stat lst;
            if (lstat(full.c_str(), &lst) != 0)
                die("cannot stat '" + full + "': " + strerror(errno));
            bool is_dir = S_ISDIR(lst.st_mode) != 0;
            bool is_link = S_ISLNK(lst.st_mode) != 0;
            bool is_reg = S_ISREG(lst.st_mode) != 0;
            if (is_dir)
                stack.push_back(rel);
            if (is_dir)
                continue; // dirs are created on demand as parents
            if (!is_link && !is_reg) {
                if (is_tracked_path(rel, fresh_root, st))
                    die("cannot package '" + rel +
                        "': unsupported file type; only regular files, "
                        "symlinks and directories are supported");
                continue; // untracked special file: leave it out
            }
            if (!is_tracked_path(rel, fresh_root, st))
                continue;
            if (!skip_abs.empty() && absolutize(full) == skip_abs)
                continue;
            std::string dst = join_path(dest, rel);
            make_dirs(dirname_of(dst));
            copy_path_preserving(full, dst);
            ++(*count);
        }
    }
}

} // namespace

int cmd_package(const std::string& projeny_arg, const std::string& output)
{
    std::string pj = resolve_projeny_path(projeny_arg, "package");
    ArchiveKind kind = classify_package_output(output); // fail fast on bad names
    // Like `commit`, refuse upfront when a previous setup left unresolved
    // conflicts: re-running setup here would otherwise re-merge the marker
    // lines as ordinary content and silently clear the conflict list.
    {
        Ctx pre_ctx = resolve_ctx(pj);
        if (path_exists(pre_ctx.statusfile)) {
            StatusData pre = StatusData::parse(pre_ctx.statusfile);
            if (!pre.conflicts.empty()) {
                std::string detail;
                for (auto& c : pre.conflicts)
                    detail += "  " + c + "\n";
                die("cannot package '" + pj +
                    "' with unresolved conflicts from a previous setup; fix "
                    "them and `projeny resolve` each file first",
                    detail);
            }
        }
    }
    int rc = cmd_setup(pj);
    if (rc != 0) {
        warn("not packaging '" + pj + "': setup reported conflicts");
        return rc;
    }
    Ctx ctx = resolve_ctx(pj);
    StatusData st = StatusData::parse(ctx.statusfile);
    if (!st.conflicts.empty()) {
        std::string detail;
        for (auto& c : st.conflicts)
            detail += "  " + c + "\n";
        die("cannot package '" + pj + "' with unresolved conflicts", detail);
    }
    ProjenyFile cur = ProjenyFile::parse(pj);
    std::string workdir = join_path(ctx.pdir, cur.name);

    // Fresh reference tree: what "tracked" means right now.
    TempDir tref(system_scratch_parent(), "projeny-pkg-ref-");
    std::string ref = build_tree_from_patch(
        tref, join_path(ctx.pdir, cur.archive), cur.origname, cur.name,
        cur.patch, "patch in '" + pj + "'");

    TempDir tstage(system_scratch_parent(), "projeny-pkg-");
    std::string payload = join_path(tstage.path, kind.prefix);
    make_dirs(payload);
    size_t count = 0;
    stage_tracked(workdir, ref, st, absolutize(output), payload, &count);

    std::string dd = dirname_of(output);
    if (dd != "." && dd != "" && !path_exists(dd))
        make_dirs(dd);
    std::vector<std::string> argv;
    argv.push_back("tar");
    argv.push_back("-c");
    if (!kind.comp.empty())
        argv.push_back(kind.comp);
    argv.push_back("-f");
    argv.push_back(absolutize(output));
    argv.push_back("-C");
    argv.push_back(absolutize(tstage.path));
    argv.push_back(kind.prefix);
    CmdResult r = run_cmd(argv);
    if (r.code != 0)
        die("failed to create archive '" + output + "'", r.output);
    fsync_dir(dd.empty() ? "." : dd);
    printf("projeny: packaged %zu file(s) from '%s' into '%s' (%s/)\n",
           count, workdir.c_str(), output.c_str(), kind.prefix.c_str());
    return 0;
}

int cmd_extract(const std::string& projeny_arg, const std::string& dest_dir)
{
    std::string pj = resolve_projeny_path(projeny_arg, "extract");
    // Like `commit`, refuse upfront on conflicts left by a previous setup
    // (see cmd_package: re-running setup would absorb the markers silently).
    {
        Ctx pre_ctx = resolve_ctx(pj);
        if (path_exists(pre_ctx.statusfile)) {
            StatusData pre = StatusData::parse(pre_ctx.statusfile);
            if (!pre.conflicts.empty()) {
                std::string detail;
                for (auto& c : pre.conflicts)
                    detail += "  " + c + "\n";
                die("cannot extract '" + pj +
                    "' with unresolved conflicts from a previous setup; fix "
                    "them and `projeny resolve` each file first",
                    detail);
            }
        }
    }
    int rc = cmd_setup(pj);
    if (rc != 0) {
        warn("not extracting '" + pj + "': setup reported conflicts");
        return rc;
    }
    Ctx ctx = resolve_ctx(pj);
    StatusData st = StatusData::parse(ctx.statusfile);
    if (!st.conflicts.empty()) {
        std::string detail;
        for (auto& c : st.conflicts)
            detail += "  " + c + "\n";
        die("cannot extract '" + pj + "' with unresolved conflicts", detail);
    }
    ProjenyFile cur = ProjenyFile::parse(pj);
    std::string workdir = join_path(ctx.pdir, cur.name);

    std::string dest = strip_trailing_slashes(dest_dir);
    if (dest.empty())
        dest = ".";
    if (path_exists(dest)) {
        if (!is_dir(dest))
            die("cannot extract to '" + dest_dir +
                "': path exists but is not a directory");
        if (!list_dir_names(dest).empty())
            die("cannot extract to '" + dest_dir +
                "': directory already exists and is not empty; remove it first");
    } else {
        make_dirs(dest);
    }

    TempDir tref(system_scratch_parent(), "projeny-ext-ref-");
    std::string ref = build_tree_from_patch(
        tref, join_path(ctx.pdir, cur.archive), cur.origname, cur.name,
        cur.patch, "patch in '" + pj + "'");

    size_t count = 0;
    stage_tracked(workdir, ref, st, "", dest, &count);
    fsync_dir(dest);
    printf("projeny: extracted %zu file(s) from '%s' to '%s'\n", count,
           workdir.c_str(), dest.c_str());
    return 0;
}

int cmd_help(const std::string& arg0)
{
    printf("usage: %s <command> [args]\n", arg0.c_str());
    printf("\n"
           "Manage \"project = release tarball + patch\" pairs.\n"
           "\n"
           "  setup <f.projeny>                unpack archive, apply patch\n"
           "  commit <f.projeny>               fold workdir changes into the patch\n"
           "  add <f.projeny> <path>           mark a file as added\n"
           "  rm <f.projeny> <path>            delete a file, mark as removed\n"
           "  mv <f.projeny> <src> <dst>       rename a file, mark as renamed\n"
           "  resolve <f.projeny> <path>       clear a conflict marker entry\n"
           "  rebase <f.projeny> <tarball>     point the project at a new tarball\n"
           "  status <f.projeny>               show setup/conflict/pending state\n"
           "  diff <dir> <other-dir>           print the diff between two trees\n"
           "  patch <dir> <patch-file>         apply a patch file to a tree\n"
           "  package <f.projeny|dir> <out>    setup, then tar the tracked files\n"
           "  extract <f.projeny|dir> <dest>   setup, then copy tracked files to a dir\n"
           "  help [command]                   show this message or command help\n"
           "\n"
           "Paths into the work tree may be CWD-relative, absolute, or\n"
           "workdir-relative (\"<Name>/...\"). They are stored relative to\n"
           "the workdir.\n"
           "\n"
           "Run `%s help <command>` for a detailed explanation of one command.\n",
           arg0.c_str());
    return 0;
}

int cmd_help_topic(const std::string& arg0, const std::string& topic)
{
    const char* t = arg0.c_str();
    if (topic == "setup") {
        printf("%s setup <f.projeny>\n"
               "\n"
               "Unpack the release tarball named by the Archive: header and\n"
               "apply the patch, creating the workdir named by Name: (plus a\n"
               "dot-prefixed .<f>.projeny.status bookkeeping file, never\n"
               "tracked by git). The tarball must unpack to exactly one\n"
               "top-level directory named by Origname: (hard error\n"
               "otherwise); it is renamed to Name:.\n"
               "\n"
               "When the workdir already exists, the status file is\n"
               "required: projeny reconstructs the expected tree from the\n"
               "status copy, diffs it against the workdir to find your\n"
               "uncommitted changes, and merges them onto a fresh setup of\n"
               "the CURRENT .projeny file (which may name a different\n"
               "Archive:). Merge failures leave conflict markers in the\n"
               "workdir and record the files in the status file; fix them,\n"
               "then `resolve` each file and `commit`. A setup that leaves\n"
               "conflicts still finishes (workdir, .projeny file, and\n"
               "status are all updated) but exits 1, so scripts running\n"
               "under `set -e` (like `projeny package`) stop instead of\n"
               "building from a conflicted tree.\n"
               "\n"
               "When the workdir exists but the status file does not, the\n"
               "directory was never set up by projeny. Setup then unpacks\n"
               "into the existing directory, keeping the files it already\n"
               "had, if it holds nothing the tarball or patch would\n"
               "overwrite (it may be empty, or hold only files setup never\n"
               "touches, which ride along like user-added files). If setup\n"
               "would overwrite anything already there, it refuses and\n"
               "lists the offending paths: without a status file there is\n"
               "no base to merge against, so nothing existing may be\n"
               "destroyed.\n"
               "\n"
               "setup also maintains .<Archive>.snapshot, a byte-exact copy\n"
               "of the tarball it used, next to the archive; the status\n"
               "copy's tree is later reconstructed from that snapshot, so\n"
               "setup keeps working after git deleted the archive (e.g. an\n"
               "upstream rebase to a newer tarball). A missing snapshot is\n"
               "not an error when the tarball still exists: it is recreated\n"
               "from the tarball on first use. Snapshots are plain untracked\n"
               "files, safe to delete.\n"
               "\n"
               "Status and snapshot files keep their older undotted names\n"
               "(<f>.projeny.status, <Archive>.snapshot) working too: on\n"
               "first use they are renamed to the dotted forms. When the\n"
               "workdir is missing, stale status/snapshot files are renamed\n"
               "to <name>.stale (then .stale2, .stale3, ...) with a warning\n"
               "instead of confusing a fresh setup; when the workdir exists\n"
               "but the status file does not, setup unpacks into it if it\n"
               "holds nothing setup would overwrite (keeping the files it\n"
               "already had), and refuses with the offending paths listed\n"
               "otherwise.\n"
               "\n"
               "Files that vanished from the workdir without an explicit\n"
               "`projeny rm` (or rename) are treated as accidental loss, not\n"
               "as intended deletions: setup restores them to fresh-setup\n"
               "state (tarball plus patch), whether text or binary. Only\n"
               "deletions recorded with `projeny rm` (pending removals and\n"
               "pending rename sources) are re-applied to the new tree.\n"
               "\n"
               "setup is also the ONLY command that tolerates git conflict\n"
               "markers (<<<<<<<, =======, >>>>>>>, |||||||) in the .projeny\n"
               "file itself (from `git pull --rebase`, `stash pop`, `merge`,\n"
               "...). It force-takes the upstream side for the\n"
               ".projeny file and status copy, and merges the local\n"
               "patch into the workdir with conflicts marked,\n"
               "additionally re-applying any uncommitted workdir-vs-status\n"
               "changes on top. Which side is upstream is auto-detected\n"
               "(merge keeps ours=local; rebase and stash pop swap the\n"
               "sides, so <<<<<<< HEAD holds upstream there). Every other\n"
               "command refuses a conflicted\n"
               ".projeny file outright. A truncated or binary-garbled\n"
               ".projeny file is refused without touching the workdir or\n"
               "status file.\n",
               t);
        return 0;
    }
    if (topic == "commit") {
        printf("%s commit <f.projeny>\n"
               "\n"
               "Fold the workdir changes into the patch: diff the workdir\n"
               "against the base archive and store the new patch in both the\n"
               ".projeny file and the status copy. Pending add/rm/mv\n"
               "operations are validated and folded in.\n"
               "\n"
               "Only explicitly marked files change tracking state: a file\n"
               "that vanished without `projeny rm` (or a `projeny mv`\n"
               "source) is a hard error (restore it or `rm` it first),\n"
               "and a new file that appeared without `projeny add` (or a\n"
               "`projeny mv` destination) is left out of the patch, like\n"
               "git leaves untracked files out. Rename pairs recorded\n"
               "with `projeny mv` render as rename diffs.\n"
               "\n"
               "NUL-bearing (binary) files travel in the patch as base64\n"
               "`GIT binary patch` blocks: tracked binaries that changed or\n"
               "vanished, and explicitly `add`ed (or rename-destination)\n"
               "binaries, commit like text. Untracked binaries that were\n"
               "never committed and never `add`ed (build junk, a package\n"
               "tarball written into the workdir) are left out of the patch,\n"
               "the way git leaves untracked files out of commits.\n"
               "\n"
               "Requires the .projeny file to match the status copy exactly\n"
               "(else hard error: run `setup` to merge first) and refuses\n"
               "while conflicts are pending (resolve them first).\n",
               t);
        return 0;
    }
    if (topic == "add") {
        printf("%s add <f.projeny> <path>\n"
               "\n"
               "Mark a file in the workdir as added-but-not-committed (stored\n"
               "in the status file; folded into the patch by the next\n"
               "`commit`). The path may be CWD-relative, absolute, or\n"
               "workdir-relative (<Name>/...); it is stored relative to the\n"
               "workdir. The file must exist. If a later `setup` also adds\n"
               "the same file, that is a merge conflict.\n",
               t);
        return 0;
    }
    if (topic == "rm") {
        printf("%s rm <f.projeny> <path>\n"
               "\n"
               "Delete a file from the workdir and mark it as\n"
               "removed-but-not-committed (folded into the patch by the next\n"
               "`commit`). Path forms are the same as for `add`.\n",
               t);
        return 0;
    }
    if (topic == "mv") {
        printf("%s mv <f.projeny> <src> <dst>\n"
               "\n"
               "Rename a file inside the workdir and record the rename as\n"
               "pending (folded into the patch by the next `commit`, which\n"
               "renders it as a rename diff when the content is similar\n"
               "enough). Moving a pending-added file keeps it added under\n"
               "the new name. Path forms are the same as for `add`.\n",
               t);
        return 0;
    }
    if (topic == "resolve") {
        printf("%s resolve <f.projeny> <path>\n"
               "\n"
               "Drop a file from the status conflict list after you fixed\n"
               "its conflict markers by hand. Accepts the stored\n"
               "workdir-relative form (src/a.c), the on-disk <Name>/.../\n"
               "form, a CWD-relative path, or an absolute path. If the file\n"
               "still contains conflict-marker lines, projeny warns on\n"
               "stderr but still resolves (committing markers would bake\n"
               "them into the patch).\n",
               t);
        return 0;
    }
    if (topic == "rebase") {
        printf("%s rebase <f.projeny> <new-tarball>\n"
               "\n"
               "Point the project at a new tarball: apply the current patch\n"
               "onto the new base, rewrite the Archive:/Origname: headers,\n"
               "regenerate the patch, and move the result into the workdir.\n"
               "The tree must be clean (no uncommitted changes) and have no\n"
               "pending conflicts; when never set up, runs `setup` first.\n"
               "Conflicts leave markers and are recorded in the status file.\n"
               "Pending add/rm/mv operations are preserved. When the new\n"
               "tarball reuses the current Archive basename with different\n"
               "bytes, projeny warns on stderr and uses the new file.\n",
               t);
        return 0;
    }
    if (topic == "status") {
        printf("%s status <f.projeny>\n"
               "\n"
               "Show the setup state from the status file: the Status: line\n"
               "plus pending Conflict:/Added:/Removed:/Renamed: entries,\n"
               "plus the live workdir state versus the expected tree:\n"
               "Modified: (tracked files with uncommitted edits),\n"
               "Disappeared: (tracked files missing on disk and not marked\n"
               "removed/renamed), and Untracked: (new files not marked\n"
               "added/renamed) lines.\n"
               "Exits nonzero when never set up (no status file).\n",
               t);
        return 0;
    }
    if (topic == "diff") {
        printf("%s diff <dir> <other-dir>\n"
               "\n"
               "Print the unified diff between two on-disk trees to stdout\n"
               "(git-compatible: `diff --git a/... b/...`, ---/+++, @@\n"
               "hunks, new/deleted/rename entries; text hunks are accepted\n"
               "by `git apply` and `patch -p1` as well as `projeny patch`).\n"
               "The diff is minimal: an internal Myers line diff with rename\n"
               "detection. NUL-bearing (binary) files travel as base64\n"
               "`GIT binary patch` blocks (projeny-internal encoding);\n"
               "everything else roundtrips exactly, including trailing\n"
               "blank lines.\n"
               "Labels use the second directory's basename, so\n"
               "`projeny diff A B > p` plus `projeny patch C p` reproduces\n"
               "B from a copy C of A. Both arguments must be directories.\n",
               t);
        return 0;
    }
    if (topic == "patch") {
        printf("%s patch <dir> <patch-file>\n"
               "\n"
               "Apply a unified-diff patch file to a directory with -p1\n"
               "semantics and fuzz (git-style diffs: a/b prefixes,\n"
               "/dev/null sides, new/deleted/mode/rename lines). The\n"
               "patch's workdir label (if any) is detected automatically,\n"
               "so patches from `projeny diff` apply under any directory\n"
               "name. Blocks that already applied are skipped, so\n"
               "re-applying is safe.\n"
               "Blocks that do not apply become conflicts: conflict markers\n"
               "(<<<<<<< current / ======= / >>>>>>> patched) are written\n"
               "inline for text (matching hunks still apply; new files pit\n"
               "current against desired bytes; deletions, renames, and\n"
               "binary blocks are left for you) and the conflicted files\n"
               "are listed on stdout. The\n"
               "command exits 0 even with conflicts; fix the markers by\n"
               "hand afterwards. A patch file with no diff blocks or a\n"
               "missing directory is a hard error.\n",
               t);
        return 0;
    }
    if (topic == "package") {
        printf("%s package <f.projeny|dir> <output-tarball>\n"
               "\n"
               "Run `setup` on the project, then tar up exactly the tracked\n"
               "files into <output-tarball>: the base archive plus the patch\n"
               "plus any uncommitted workdir changes (pending add/rm/mv ops\n"
               "included), while untracked files (never committed, never\n"
               "`add`ed) are left out — like `git archive` and\n"
               "package-source.sh. The first argument may be the .projeny\n"
               "file or a directory holding (or naming, as \"<dir>.projeny\"\n"
               "for a \"<dir>\" workdir) exactly one .projeny file.\n"
               "The archive holds a single top-level directory named after\n"
               "the output file (pizlonated-libffi.tar.gz holds\n"
               "pizlonated-libffi/...). Compression is autodetected from the\n"
               "output extension: .tar (none), .tar.gz/.tgz (gzip),\n"
               ".tar.bz2/.tbz2/.tbz (bzip2), .tar.xz/.txz (xz),\n"
               ".tar.zst/.tzst (zstd). When `setup` reports conflicts the\n"
               "command prints them and exits nonzero without writing any\n"
               "archive.\n",
               t);
        return 0;
    }
    if (topic == "extract") {
        printf("%s extract <f.projeny|dir> <dest-dir>\n"
               "\n"
               "Run `setup` on the project, then copy exactly the tracked\n"
               "files (the same set `package` would archive: base plus patch\n"
               "plus uncommitted changes, minus untracked files) into\n"
               "<dest-dir>, so builds run outside the checkout — the projeny\n"
               "variant of extract_source. The first argument may be the\n"
               ".projeny file or a directory holding (or naming, as\n"
               "\"<dir>.projeny\" for a \"<dir>\" workdir) exactly one\n"
               ".projeny file. The destination must not exist or must be an\n"
               "empty directory (remove it first to redo an extraction).\n"
               "When `setup` reports conflicts the command prints them and\n"
               "exits nonzero without writing anything.\n",
               t);
        return 0;
    }
    if (topic == "help") {
        printf("%s help [command]\n"
               "\n"
               "With no arguments, list all commands. With a command name\n"
               "(setup, commit, add, rm, mv, resolve, rebase, status, diff,\n"
               "patch, package, extract, help), print a detailed explanation\n"
               "of that command.\n",
               t);
        return 0;
    }
    fprintf(stderr, "projeny: error: unknown help topic '%s'\n",
            topic.c_str());
    return 1;
}
