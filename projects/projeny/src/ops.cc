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

#include "download.h"
#include "projeny_file.h"
#include "tree.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>

namespace {

// Defined below the conflicted-setup paths that display it; it converts an
// absolute path to the user-relative spelling for messages.
std::string rel_to_cwd(const std::string& abs);

} // namespace

Ctx resolve_ctx(const std::string& projeny_arg)
{
    Ctx c;
    c.projeny_arg = projeny_arg;
    c.pdir = dirname_of(projeny_arg);
    c.filebase = basename_of(projeny_arg);
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

// Render a bullet list for die() detail blocks: two spaces per line.
std::string bullet_list(const std::vector<std::string>& items)
{
    std::string out;
    for (const auto& item : items)
        out += "  " + item + "\n";
    return out;
}

// Same, over patch-failure display blocks (the "does not apply" details).
std::string bullet_list(const std::vector<VcsFailure>& bad)
{
    std::string out;
    for (const auto& f : bad)
        out += "  " + f.display + "\n";
    return out;
}

// Require the "already set up, and the .projeny file matches the status
// copy" precondition shared by commit/rebase/diff and the frozen-mtime
// commands: the status file must exist, and its embedded .projeny copy must
// be byte-identical to the live file (a git merge that touched either needs
// `setup` to reconcile first). Returns the parsed status.
StatusData require_status_matches(const Ctx& ctx)
{
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string cur_raw = read_file_bytes(ctx.projeny_arg);
    if (cur_raw != st.embedded)
        die("'" + ctx.projeny_arg +
            "' differs from the copy in '" + ctx.statusfile +
            "'; run setup to merge first");
    return st;
}

// Require a conflict-free status; `what` is the full die headline (each
// command words its refusal differently). The bullet list names every
// unresolved file.
void require_no_conflicts(const StatusData& st, const std::string& what)
{
    if (st.conflicts.empty())
        return;
    die(what, bullet_list(st.conflicts));
}

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

// Scratch dirs live NEXT TO the .projeny file (same filesystem as the
// checkout, so moving a freshly built tree into place is a plain rename),
// named ".<file>.<role>.XXXXXX" (mkdtemp suffix), e.g. projects/
// .dash.projeny.setup.k3Xm9Q for projects/dash.projeny. They are hidden,
// unique, never inside a workdir, and are ignored by every pdir scan
// (resolve_projeny_path only matches names ENDING in ".projeny").
// `filebase` is the .projeny file's basename ("dash.projeny") and `role` a
// short purpose word ("setup", "E", "rebase-url", ...) standing in for the
// old "/tmp/projeny-<role>-" prefixes. The first use of a pdir in this
// process reaps the stale scratch dirs a SIGKILL'ed run left there (see
// reap_stale_scratch in util.h) before adding a new one.
//
// Read-only fallback: a pdir that cannot take new directories (a read-only
// checkout, or a full filesystem) must not break the commands that only
// READ the checkout (status/diff/package/extract/get-attributes all build
// their trees in scratch dirs). When creating the scratch dir next to the
// file fails, make_scratch warns ONCE per unwritable pdir per process and
// retries in system_scratch_parent() ($TMPDIR, else /tmp) with the SAME
// hidden name shape — cross-device tree moves are already handled by
// move_path's EXDEV cp -a fallback, and the startup sweep of the system
// temp dir reaps both shapes there (see reap_legacy_tmp_scratch), so
// fallback leftovers from a SIGKILL'ed run are not leaked either. The
// memoized pdirs skip straight to the fallback, so a read-only checkout
// never pays the failing attempt more than once.
TempDir make_scratch(const std::string& pdir, const std::string& filebase,
                     const std::string& role)
{
    reap_stale_scratch(pdir);
    const std::string prefix = "." + filebase + "." + role + ".";

    static std::mutex mu;
    static std::set<std::string> unwritable;
    std::string err;
    bool known_bad;
    {
        std::lock_guard<std::mutex> lk(mu);
        known_bad = unwritable.count(pdir) != 0;
    }

    TempDir scratch;
    if (!known_bad) {
        if (scratch.try_init(pdir, prefix, &err))
            return scratch; // named RVO: the move hands the dir to the caller
        bool first;
        {
            std::lock_guard<std::mutex> lk(mu);
            first = unwritable.insert(pdir).second;
        }
        if (first) {
            warn("cannot create scratch dirs in '" + pdir + "' (" + err +
                 "); using '" + system_scratch_parent() + "' instead");
        }
        // try_init failed, so `scratch` is empty again (its own failure
        // path removed whatever the attempt minted) — the fallback below
        // can fill the same named local.
    }
    if (!scratch.try_init(system_scratch_parent(), prefix, &err))
        die("cannot create a scratch dir in '" + pdir + "' or in '" +
            system_scratch_parent() + "': " + err);
    return scratch; // named RVO: the move hands the dir to the caller
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

// The frozen-base-snapshot path of a Projeny:-based .projeny file: the
// hidden sibling ".<stem>.snapshot" (bar.projeny -> .bar.snapshot, stem
// being the basename minus its trailing ".projeny"; a basename without the
// suffix keeps its whole name: "weird" -> ".weird.snapshot"). The file holds
// the BYTE-EXACT bytes of the base file as-of when this project adopted it,
// and it is FROZEN: it never tracks the live base file, so every
// setup/commit/status/diff/package/extract of the referencing project
// reconstructs exactly the base it was set up from, even after the base
// file is deleted from git. Only a missing snapshot (first use), an
// adoption setup (a hand-edited Projeny: header naming a different base
// file than the status records), or `rebase` ever (re)writes it — see
// resolve_committed_base_bytes below.
std::string projeny_snapshot_path_for(const std::string& projeny_path)
{
    std::string base = basename_of(projeny_path);
    const std::string suffix = ".projeny";
    std::string stem = ends_with(base, suffix)
                           ? base.substr(0, base.size() - suffix.size())
                           : base;
    return join_path(dirname_of(projeny_path), "." + stem + ".snapshot");
}

// Move src to dst, tolerating a lost race against a parallel sibling that
// moved src first: multi-project commands can legitimately have two workers
// performing the same bookkeeping move on one shared path (the legacy
// snapshot of an archive two projects share, say), and the rename is then a
// race whose loser's source has already moved. The goal of such a move is
// only "src is no longer at src", so a failure that leaves src gone means
// the goal is already met. Any failure that leaves src in place is still
// fatal (rethrown verbatim). Returns true when THIS call performed the
// move, so callers can keep their one warning line per actual move.
bool move_path_shared(const std::string& src, const std::string& dst)
{
    try {
        move_path(src, dst);
        return true;
    } catch (const ProjenyFatalError&) {
        if (path_exists(src))
            throw; // not a lost race: the source is still there
        return false;
    }
}

// If the dotted snapshot is missing but the legacy undotted one exists,
// rename it into the canonical dotted form. Called at every point of use
// (ensure_snapshot, resolve_status_archive, status's live diff, stale-state
// reconciliation), so any command finishes the upgrade. Idempotent no-op
// when the dotted snapshot exists or neither form does. Parallel workers of
// one multi-project command can run this on the same archive concurrently;
// move_path_shared makes the loser of that race a no-op instead of a
// spurious hard error.
void migrate_snapshot(const std::string& archive)
{
    std::string snap = snapshot_path_for(archive);
    if (path_exists(snap))
        return;
    std::string legacy = legacy_snapshot_path_for(archive);
    if (!path_exists(legacy))
        return;
    move_path_shared(legacy, snap);
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
    // write_file_bytes is temp file + rename, so the snapshot
    // switches atomically and a crash never leaves a half-written copy.
    // It is byte-exact for regular files, which tarballs are.
    write_file_bytes(snap, read_file_bytes(archive));
}

// True when `path` names an existing, readable regular file. Used to guard
// the die-on-unreadable hash helpers in the non-dying (try_) paths.
bool is_readable_file(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        return false;
    return access(path.c_str(), R_OK) == 0;
}

// The .snapshot path for a URL-based project (pf.archive is the URL-derived
// archive name): pdir/.<archive>.snapshot — same naming as classic projects.
std::string url_snapshot_path(const std::string& pdir, const ProjenyFile& pf)
{
    return snapshot_path_for(join_path(pdir, pf.archive));
}

// Last snapshot path whose "skipping the download" note has been printed by
// try_ensure_url_snapshot. Setup (through write_status) re-ensures the URL
// snapshot it just materialized, and commit materializes the same archive
// twice (the unpack and the expected-tree build), so without this a healthy
// run would print the identical skip line two or three times. Tracking one
// snapshot path keeps the message to once per snapshot per thread, while a
// genuinely different snapshot — the two sides of a conflicted merge, say —
// still gets its own line. thread_local (not shared): parallel workers
// touch it concurrently, and per-thread dedup is exactly right — two
// workers never share a snapshot path (projects are deduplicated and each
// snapshot lives in its project's own directory).
thread_local std::string g_skip_announced_for;

// One archive package shared by one or more contributing .projeny files,
// keyed by the archive basename (downloads are deduplicated by name). The
// planning phase of a multi-project command fills one per package BEFORE
// any per-project work starts; the per-project phase's snapshot guard (see
// try_ensure_url_snapshot) consults the map to accept the batch's download
// and to fail cleanly when the batch failed.
struct PlannedPackage {
    // Candidate URLs in priority order (the URLs every contributor lists
    // first, then the rest) and the union of every contributor's URL
    // hashes: the batch download verifies against all of them, so one
    // package can satisfy files that recorded different hashes.
    std::vector<std::string> urls;
    std::set<std::string> accepted_hashes;
    // Distinct (sorted) directories of the contributing .projeny files:
    // each gets its own .<archive>.snapshot copy of the download.
    std::vector<std::string> pdirs;
    // Filled by the download phase: attempted/ok record whether the batch
    // tried (and managed) to obtain the package; verified_hashes collects
    // every blake3 hash known good for this package (the batch download's
    // hash plus the hashes of pre-existing satisfying snapshots); errors
    // carries the batch's per-attempt failure lines for the guard's report.
    bool attempted = false;
    bool ok = false;
    std::set<std::string> verified_hashes;
    std::vector<std::string> errors;
    // Per-pdir "already satisfied when planning ran" flags (parallel to
    // pdirs): only unsatisfied dirs receive the batch's bytes.
    std::vector<bool> pdir_satisfied;
};

// The multi-project plan while a multi command's per-project phase runs;
// null otherwise (single-project commands, the planning phase itself). A
// non-null pointer IS the "multi mode" flag: the snapshot guard below does
// nothing unless this is set, so legacy single-project output stays
// byte-identical. Set before run_parallel starts the workers and cleared
// (RAII) after they join, so no thread ever writes it while workers exist.
const std::map<std::string, PlannedPackage>* g_multi_plan = nullptr;

// Serializes the unplanned-package fallback download (a conflicted —
// therefore unparseable — .projeny file whose package the batch never
// covered): never two concurrent downloads of the same package, even for
// packages the planning phase could not see. erase-setup's no-force check
// phase takes the same mutex around its snapshot-materializing step (inside
// compute_live_diff): it runs with no batch plan at all, so every
// materialization there (a URL re-download, a classic archive copied into
// a missing snapshot) is a "fallback" in this sense. The mutex serializes
// materializations; it does not pin a snapshot's bytes — checked projects
// sharing an archive basename while disagreeing on its expected hashes
// re-materialize the same snapshot path in turn, each write an atomic
// temp+rename outside the other's critical section, so a settled snapshot
// can still change under a concurrent reader (fail-closed; see the
// comment in compute_live_diff).
std::mutex g_fallback_download_mu;

// The sequential download loop shared by the legacy single-project path and
// the multi-mode unplanned-package fallback: download+verify each URL: line
// in order (warn + next on failure/mismatch, per the spec) and atomically
// write the first good download to the snapshot via write_file_bytes. The
// caller has already printed the "re-downloading"/fallback announcements.
// Returns true + *out = snapshot path, or false + *err when every URL
// failed.
bool download_url_snapshot_sequential(const std::string& snap,
                                      const ProjenyFile& pf,
                                      std::string* out, std::string* err)
{
    std::string last_err;
    size_t tried = 0;
    for (const ProjenyUrl& u : pf.urls) {
        ++tried;
        std::string data;
        std::string derr;
        if (!try_download(u.url, &data, &derr)) {
            warn("could not download '" + u.url + "': " + derr +
                 "; trying the next URL");
            last_err = derr;
            continue;
        }
        std::string have = blake3_hash_hex(data);
        if (have != u.hash) {
            warn("downloaded '" + u.url + "' but its blake3 hash is " + have +
                 ", expected " + u.hash + "; trying the next URL");
            last_err = "its blake3 hash is " + have + ", expected " + u.hash;
            continue;
        }
        // write_file_bytes is temp file + rename, so the snapshot
        // switches atomically and a crash never leaves a half-written copy
        // (the next run simply re-downloads).
        write_file_bytes(snap, data);
        note("downloaded '" + u.url +
             "'; blake3 hash verified; wrote snapshot '" + snap + "'");
        *out = snap;
        return true;
    }
    *err = "could not obtain archive '" + pf.archive +
           "' (tried " + std::to_string(tried) +
           " URL line(s); every download failed or did not match its "
           "recorded blake3 hash)" +
           (last_err.empty() ? "" : "; the last error was: " + last_err);
    return false;
}

// Make sure the URL archive's snapshot exists and matches at least one URL:
// hash. No-op (no network!) when the snapshot already matches. Otherwise
// download+verify each URL: line in order (warn + next on failure/mismatch,
// per the spec) and atomically write the first good download to the snapshot
// via write_file_bytes. Dies (mentioning `what`) only when every URL failed.
//
// Feedback, all on stderr: try_download announces each attempt and reports
// '\r'-terminated progress itself; here a verified download reports the
// snapshot it wrote, and — only when `announce_skip` is true AND this is a
// single-project run (g_multi_plan null) — an already-matching snapshot
// reports that the download is skipped (deduplicated once per snapshot per
// thread by g_skip_announced_for). In multi mode the note is suppressed:
// the batch download phase already reported every package it fetched
// ("downloading ... / downloaded ... (N bytes); blake3 hash verified") and
// silently verified the pre-existing ones, so a "using existing snapshot"
// note in the per-project phase would only echo the batch's own lines —
// the snapshot exists BECAUSE the batch just wrote it — and read like an
// error. announce_skip is false
// on the read-only reconstruction paths (resolve_status_archive, the
// write_status bookkeeping re-assertion, status's live-diff section): those
// must stay completely silent on a healthy checkout — `projeny diff` and
// `projeny get-attributes` print nothing when the snapshot matches — while
// materialize_archive (setup/commit) passes true. The download-side messages
// are never gated: they only exist when a real download happens.
//
// In multi mode (g_multi_plan set — the per-project phase of a parallel
// setup/package/extract), the batch download phase has already run, so this
// never downloads a planned package: the guard accepts the batch's snapshot
// when it verifies against the plan's hashes (the package may have come
// from a URL another contributing file listed — loudly warned during
// planning), dies cleanly when the batch failed, and only a package the
// plan cannot know about falls back to a serialized download here — a
// conflicted (therefore unparseable) .projeny file, or a Projeny:-based
// project's URL:-based chain base (chain bases are never batch
// contributors; see run_multi's planning loop). `fallback_reason` is the
// parenthetical the fallback note carries: it defaults to "conflicted
// projeny file", and only the chain-bottom fetch (resolve_status_archive
// under extract_committed_projeny) overrides it. Every snapshot in multi
// mode is silent about the skip — including one whose bytes a fallback
// download just wrote — while the "does not match any URL: hash;
// re-downloading" warning prints in every mode.
std::string ensure_url_snapshot(const std::string& pdir, const ProjenyFile& pf,
                                const std::string& what, bool announce_skip,
                                const std::string& fallback_reason);

// Non-dying variant for informational commands: returns true + *out = snapshot
// path, or false + *err.
bool try_ensure_url_snapshot(const std::string& pdir, const ProjenyFile& pf,
                             std::string* out, std::string* err,
                             bool announce_skip,
                             const std::string& fallback_reason =
                                 "conflicted projeny file")
{
    std::string snap = url_snapshot_path(pdir, pf);
    // The snapshot is the local cache of the downloaded archive: when it
    // already matches at least one URL: hash, it IS the archive — verify and
    // use it without touching the network. Only a missing snapshot (or one
    // that matches no hash) is (re-)downloaded.
    std::string have;
    bool snapshot_readable = false;
    if (is_readable_file(snap)) {
        have = blake3_file_hash_hex(snap);
        snapshot_readable = true;
        for (const ProjenyUrl& u : pf.urls) {
            if (have == u.hash) {
                // Single-project runs announce the skip (once per snapshot
                // per thread). Multi mode stays silent: the batch download
                // phase already reported every download it made and
                // silently verified the pre-existing snapshots, so this
                // note would only repeat the batch's own report — and right
                // after it, it reads like an error.
                if (announce_skip && !g_multi_plan &&
                    g_skip_announced_for != snap) {
                    note("using existing snapshot '" + snap +
                         "' (blake3 hash matches); skipping the download");
                    g_skip_announced_for = snap;
                }
                *out = snap;
                return true;
            }
        }
    }
    // Multi-mode guard (never reached on legacy single-project paths: the
    // plan pointer is null there). The file's own URL hashes were just
    // checked above, so only the plan's hashes can still accept the
    // snapshot.
    if (g_multi_plan) {
        auto it = g_multi_plan->find(pf.archive);
        if (it != g_multi_plan->end()) {
            const PlannedPackage& plan = it->second;
            if (snapshot_readable && plan.verified_hashes.count(have) > 0) {
                // The batch download phase put this archive here (or it was
                // already satisfying another contributor): it verifies
                // against a hash some contributing file listed, which the
                // planning phase's loud warnings covered. Just do it.
                *out = snap;
                return true;
            }
            if (!plan.ok && plan.attempted) {
                // The batch tried and failed to obtain this package; a
                // retry here would just fail the same way.
                die("'" + pf.name + "' needs archive '" + pf.archive +
                        "', but the parallel download phase failed to "
                        "obtain it",
                    bullet_list(plan.errors));
            }
            // The batch succeeded (or never needed to run), but this
            // project's directory holds no usable snapshot. Two things can
            // get here: a conflicted — therefore unparseable, therefore
            // not a batch contributor, therefore not one of the pdirs the
            // batch wrote its verified snapshot into — .projeny file, and
            // a Projeny:-based project's URL:-based chain base (chain
            // bases are never batch contributors by design: run_multi
            // plans only direct URL: arguments, so the chain bottom's
            // archive is fetched on demand right here). Fall through to
            // the serialized download below: it re-checks the snapshot
            // under the fallback mutex (a worker that lost such a race may
            // have just written one) and downloads only if none appeared.
        }
        // Not in the plan (or in the plan without a usable local
        // snapshot): a conflicted (therefore deferred, unparseable)
        // .projeny file, or a Projeny:-based project's chain base (never a
        // batch contributor by design — see run_multi's planning loop).
        // Download serialized: never two concurrent downloads of the same
        // package, and a worker that lost the race re-checks the snapshot
        // another worker may have just written before going to the network
        // itself.
        std::lock_guard<std::mutex> lk(g_fallback_download_mu);
        std::string rehave;
        bool re_readable = is_readable_file(snap);
        if (re_readable)
            rehave = blake3_file_hash_hex(snap);
        for (const ProjenyUrl& u : pf.urls) {
            if (re_readable && rehave == u.hash) {
                *out = snap;
                return true;
            }
        }
        if (re_readable)
            warn("existing snapshot '" + snap +
                 "' does not match any URL: hash; re-downloading");
        note("downloading '" + pf.archive +
             "' outside the batch download phase (" + fallback_reason + ")");
        return download_url_snapshot_sequential(snap, pf, out, err);
    }
    if (snapshot_readable)
        warn("existing snapshot '" + snap +
             "' does not match any URL: hash; re-downloading");
    return download_url_snapshot_sequential(snap, pf, out, err);
}

// The dying wrapper: same behavior, but dies naming `what` when every URL
// failed.
std::string ensure_url_snapshot(const std::string& pdir, const ProjenyFile& pf,
                                const std::string& what, bool announce_skip,
                                const std::string& fallback_reason =
                                    "conflicted projeny file")
{
    std::string out;
    std::string err;
    if (!try_ensure_url_snapshot(pdir, pf, &out, &err, announce_skip,
                                 fallback_reason))
        die(err + "; it is needed to " + what);
    return out;
}

// Path of the tarball to use for `pf`: materializes URL archives, returns
// join_path(pdir, pf.archive) for classic ones.
std::string materialize_archive(const std::string& pdir, const ProjenyFile& pf,
                                const std::string& what)
{
    if (pf.is_url_based())
        // announce_skip = true: setup/commit output is allowed to talk, and
        // the g_skip_announced_for dedup keeps the repeat materializations
        // (commit unpacks the archive twice) from repeating the line.
        return ensure_url_snapshot(pdir, pf, what, true);
    return join_path(pdir, pf.archive);
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
// exist, and unpack_single_top's plain error is the right one. (Exception:
// the pending-aware `projeny diff <f.projeny>` goes through here, because
// it requires the .projeny file to match the status copy first — so its
// archive is the status-recorded archive.)
//
// URL-based projects have no checked-in archive at all: their snapshot (the
// verified download cache) is the only record, so this re-fetches it when it
// is missing or no longer matches any URL: hash, and uses it as-is when it
// does match.
std::string resolve_status_archive(const std::string& pdir,
                                   const ProjenyFile& pf,
                                   const std::string& what,
                                   const std::string& fallback_reason =
                                       "conflicted projeny file")
{
    if (pf.is_url_based())
        // announce_skip = false: read-only reconstruction (diff against the
        // recorded state, conflicted-merge side reads) must stay silent on
        // a snapshot that already matches — the user asked to inspect, not
        // to set up, and whatever announced this snapshot already ran.
        return ensure_url_snapshot(pdir, pf, what, false, fallback_reason);
    std::string archive = join_path(pdir, pf.archive);
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

// Best-effort extraction of the archive name from a status file's
// bytes: the embedded .projeny copy (everything after the
// "--- projeny content ---" delimiter, see kStatusDelim in
// projeny_file.cc) begins with its header block, one of whose lines is
// "Archive: <value>" — or, for a URL-based project, one or more
// "URL: <url> <hash>" lines whose URL's basename names the archive.
// Used only by the stale-state reconciliation to find the archive whose
// snapshot belongs to the status being discarded. A Projeny:-based
// embedded copy names no archive at all: *projeny_based (when non-null)
// reports that mode instead, and the caller stales the project file's own
// frozen base snapshot (projeny_snapshot_path_for) — the scanner only sees
// the bytes, so it cannot spell that path itself.
// Returns "" when the file is too garbled to tell — this must never die,
// because an unparseable status file is renamed out of the way just the
// same.
std::string archive_from_status_bytes(const std::string& data,
                                      bool* projeny_based = nullptr)
{
    if (projeny_based != nullptr)
        *projeny_based = false;
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
    // (returning whatever we have) at anything else — patch bodies, garbage.
    // A URL-based project has no Archive: line; remember the first URL:
    // line's derived archive name and return it if no Archive: shows up.
    // A Projeny:-based project has neither; remember that it is
    // Projeny:-based (its base filename is NOT an archive name and is never
    // returned here).
    std::string from_url;
    for (int scanned = 0; scanned < 16 && pos < data.size(); ++scanned) {
        size_t nl = data.find('\n', pos);
        std::string line =
            data.substr(pos, nl == std::string::npos ? std::string::npos
                                                     : nl - pos);
        if (starts_with(line, "Archive: "))
            return trim(line.substr(9));
        if (starts_with(line, "Projeny: ") && projeny_based != nullptr)
            *projeny_based = true;
        if (starts_with(line, "URL: ") && from_url.empty()) {
            // "URL: <url> <blake3-hash>": the archive name is the URL's
            // basename (best effort; a malformed line is simply ignored).
            std::string value = trim(line.substr(5));
            size_t sp = value.find_first_of(" \t");
            std::string url =
                sp == std::string::npos ? value : value.substr(0, sp);
            from_url = archive_name_from_url(url);
        }
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
                return from_url;
        }
        if (nl == std::string::npos)
            break;
        pos = nl + 1;
    }
    return from_url;
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
//
// `url_hashes` (optional) lists the blake3 hashes of the CURRENT .projeny
// file's own URL: headers. A snapshot that one of them verifies is NOT
// stale state from a removed checkout — it IS the archive the file
// records (the bytes a `create` just fetched, or the download a previous
// setup cached), so it is kept and the fresh setup skips its download.
// This is the single-project mirror of the multi-project batch rule below:
// a plan-verified snapshot must survive a workdir-less project's setup or
// the batch's own download would be destroyed.
//
// `extra_files` (optional) lists FULL paths of further single-named files
// to stale (no legacy form) — the frozen base snapshot of a Projeny:-based
// project, which the callers add when the discarded status (or the current
// file) is Projeny:-based: it is checkout bookkeeping exactly like the
// archive snapshots are.
void disregard_stale_state(const Ctx& ctx,
                           const std::vector<std::string>& archives,
                           const std::set<std::string>* url_hashes = nullptr,
                           const std::vector<std::string>* extra_files = nullptr)
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
    // canonical name, `legacy` the pre-dot-naming name. Parallel workers
    // of one multi-project command can stale the same shared file at the
    // same time (several projects using one archive); move_path_shared
    // makes the losers of those races silent no-ops instead of spurious
    // hard errors, and the warning lines then name only real moves.
    auto stale_pair = [&](const std::string& dotted,
                          const std::string& legacy) {
        if (!path_exists(dotted) && path_exists(legacy)) {
            // Only the legacy form exists: migrate it into the dotted form
            // first, so it is staled under the dotted .stale name.
            move_path_shared(legacy, dotted);
        }
        if (!path_exists(dotted))
            return;
        std::string dest = next_stale_name(dotted);
        if (move_path_shared(dotted, dest))
            warn("workdir for '" + ctx.projeny_arg + "' is missing; "
                 "renaming stale '" +
                 dotted + "' to '" + dest + "'");
        if (path_exists(legacy)) {
            // Both forms existed: disregard the legacy copy too, under its
            // own name (same numbering, based on the undotted name).
            std::string ldest = next_stale_name(legacy);
            if (move_path_shared(legacy, ldest))
                warn("workdir for '" + ctx.projeny_arg + "' is missing; "
                     "renaming stale '" +
                     legacy + "' to '" + ldest + "'");
        }
    };

    stale_pair(ctx.statusfile, ctx.projeny_arg + ".status");
    for (const auto& a : archives) {
        if (a.empty())
            continue;
        std::string archive = join_path(ctx.pdir, a);
        migrate_snapshot(archive);
        // Multi mode: a snapshot that the plan knows is good is NOT stale
        // state from a removed checkout — it is the archive the batch
        // download phase just fetched (or verified) for every project in
        // this directory, some of which may not have a workdir yet. Staling
        // it would destroy the shared download and break the other
        // projects' setups, so a plan-verifying snapshot stays. Single-
        // project runs (no plan) keep the exact legacy behavior.
        if (g_multi_plan) {
            auto it = g_multi_plan->find(a);
            if (it != g_multi_plan->end()) {
                std::string snap = snapshot_path_for(archive);
                if (is_readable_file(snap) &&
                    it->second.verified_hashes.count(
                        blake3_file_hash_hex(snap)) > 0)
                    continue;
            }
        }
        // Single-project mirror of the plan rule above: a snapshot the
        // caller's own .projeny URL: hashes verify stays (see the
        // url_hashes parameter's comment).
        if (url_hashes != nullptr) {
            std::string snap = snapshot_path_for(archive);
            if (is_readable_file(snap) &&
                url_hashes->count(blake3_file_hash_hex(snap)) > 0)
                continue;
        }
        stale_pair(snapshot_path_for(archive),
                   legacy_snapshot_path_for(archive));
    }
    // The callers' further single-named files (a Projeny:-based project's
    // frozen base snapshot): staled under their own name, no legacy form.
    if (extra_files != nullptr) {
        for (const auto& f : *extra_files) {
            if (f.empty())
                continue;
            stale_pair(f, "");
        }
    }
}

// disregard_stale_state for one parsed .projeny file's stale bookkeeping:
// the given archive basenames (as the plain calls take) plus — for a
// Projeny:-based file — the project file's own frozen base snapshot
// (projeny_snapshot_path_for), which is checkout bookkeeping exactly like
// the archive snapshots are. Used by the workdir-gone branches that hold
// the parsed file (commit/add/rm/mv/resolve/diff/status); setup's branch
// calls disregard_stale_state directly so it can also stale the snapshot
// the OLD status records.
void disregard_stale_state_for(const Ctx& ctx, const ProjenyFile& pf,
                               std::vector<std::string> archives)
{
    std::vector<std::string> extra;
    if (pf.is_projeny_based())
        extra.push_back(projeny_snapshot_path_for(ctx.projeny_arg));
    disregard_stale_state(ctx, archives, nullptr,
                          extra.empty() ? nullptr : &extra);
}

// A patch application inside a Projeny: tree build failed. Thrown by
// apply_fresh_patch under a TolerantPatchApply scope INSTEAD of dying: the
// bare report is NOT printed, so the catching site decides — print it
// verbatim (rethrow_patch_apply), replace it with recovery guidance (the
// frozen-chain-moved die of every read-only consumer), or recover outright
// (rebase's workdir fallback, setup's conflicted fresh setup). Derives from
// ProjenyFatalError so an unhandled escape keeps the process's fatal-error
// exit contract (every throw site is paired with a catch that prints
// something, so an escape can only be a bug elsewhere).
struct PatchApplyError final : ProjenyFatalError {
    std::string detail; // the bullet report die() would have appended
    PatchApplyError(std::string msg, std::string det)
        : ProjenyFatalError(std::move(msg)), detail(std::move(det))
    {
    }
};

// Tolerant patch application is a scoped, thread-local flag: parallel
// workers (multi setup, the erase-setup check phase) each build their own
// trees, so one thread's scope never leaks into another's. RAII restores
// the previous value through every unwind — including the PatchApplyError
// throw itself.
thread_local bool g_tolerant_patch_apply = false;

class TolerantPatchApply
{
  public:
    TolerantPatchApply() : prev_(g_tolerant_patch_apply)
    {
        g_tolerant_patch_apply = true;
    }
    ~TolerantPatchApply() { g_tolerant_patch_apply = prev_; }
    TolerantPatchApply(const TolerantPatchApply&) = delete;
    TolerantPatchApply& operator=(const TolerantPatchApply&) = delete;

  private:
    bool prev_;
};

// The bare behavior, for catch sites that decide not to recover: print the
// report exactly like the die() this exception replaced and throw
// ProjenyFatalError. die() prints the message line plus the detail verbatim
// and throws ProjenyFatalError(msg), so this reproduces the original bytes
// byte-identically.
[[noreturn]] void rethrow_patch_apply(const PatchApplyError& e)
{
    die(e.message(), e.detail);
}

// The recovery guidance every frozen-chain tree build carries when the
// chain no longer reconstructs the recorded base and a patch no longer
// applies onto it: an upstream Projeny: link was rebased, so every tree
// built through the frozen snapshots moved under the checkout. Worded per
// command (`cmd_verb` — "set up", "status", "diff", ...) against the
// .projeny path the user named. Both recoveries are working ones: `rebase`
// rebuilds from the workdir as-is (cmd_rebase's fallback), and
// erase-setup --force + setup re-extracts and re-applies with conflict
// markers (setup's conflicted fresh setup).
[[noreturn]] void die_projeny_chain_moved(const std::string& cmd_verb,
                                          const std::string& file_arg)
{
    die("cannot " + cmd_verb + " '" + file_arg +
        "': the frozen base chain no longer reconstructs the recorded base "
        "(an upstream Projeny: link was rebased) and the patch does not "
        "apply onto the new content; run 'projeny rebase " +
        file_arg + "' to rebase from the workdir as-is, or 'projeny "
                  "erase-setup --force " +
        file_arg + "' followed by 'projeny setup " + file_arg +
        "' to re-extract and re-apply with conflict markers");
}

// The fresh-setup flavor of the same failure: a chain link's own recorded
// patch no longer applies to ITS frozen base (the checkout does not exist
// yet, so neither the rebase fallback nor erase-setup applies). The
// accurate recovery is snapshot-level: removing this project's frozen base
// snapshot re-freezes it from the live base file the header names (whose
// own chain is self-consistent), or the snapshot can be restored from git.
[[noreturn]] void die_fresh_projeny_chain_broken(const Ctx& ctx)
{
    die("cannot set up '" + ctx.projeny_arg +
        "': the frozen base chain no longer reconstructs a consistent base "
        "(a link's recorded patch no longer applies to its own frozen base "
        "— usually an upstream Projeny: link was rebased); remove '" +
        projeny_snapshot_path_for(ctx.projeny_arg) +
        "' to re-freeze this project's base from the live base file it "
        "names, or restore that snapshot, then run 'projeny setup " +
        ctx.projeny_arg + "' again");
}

// Apply `patch_wid` (wid `wid`) inside the freshly built tree `tree`, dying
// with a per-file report when it does not apply (or throwing PatchApplyError
// under a TolerantPatchApply scope — see above). Shared by
// build_tree_from_patch (the archive unpack path) and the Projeny chain
// below (each link's committed patch lands on the extracted tree).
void apply_fresh_patch(const std::string& tree, const std::string& wid,
                       const std::string& patch_wid, const std::string& what)
{
    if (normalize_patch_text(patch_wid).empty())
        return;
    if (!apply_patch_whole(tree, patch_wid, wid,
                           VcsApplyMode::kFreshApply)) {
        // Retry per-file to produce a precise error. Failures carry
        // workdir-relative paths from the single patch parser (no second
        // parser, so block counts cannot diverge into OOB/wrong names).
        // kFreshApply: the tree was just built from its base content, so
        // "already applied" is impossible and the reverse-match path is
        // never consulted — a needed deletion cannot be silently dropped
        // here.
        std::vector<VcsFailure> bad = apply_patch_per_file(
            tree, patch_wid, wid, VcsApplyMode::kFreshApply);
        std::string msg =
            "cannot apply " + what + ": patch does not apply cleanly";
        std::string detail = bullet_list(bad);
        if (g_tolerant_patch_apply)
            // Tolerant scope: hand the failure to the caller unprinted (see
            // PatchApplyError) — the caller prints it verbatim, replaces it
            // with recovery guidance, or recovers outright.
            throw PatchApplyError(msg, detail);
        die(msg, detail);
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
    unpack_single_top(archive, tmp.path, origname);
    std::string tree = join_path(tmp.path, origname);
    apply_fresh_patch(tree, wid, patch_wid, what);
    return tree;
}

// ---- Projeny:-based projects ----
//
// A "Projeny: <file>" header bases a project on the COMMITTED EXTRACTION of
// another .projeny file in the same directory: unpack the base file's
// archive (snapshot-preferred) and apply the base file's committed patch.
// The base file's own workdir — its checkout with uncommitted changes — is
// ignored everywhere here. Chains work: a Projeny:-based base is itself
// extracted through its own chain, each link resolving ITS base through its
// OWN frozen snapshot (see resolve_committed_base_bytes).
//
// The frozen snapshot law: a Projeny:-based file X keeps the byte-exact
// bytes of its base file as-of when X adopted it in ".<X-stem>.snapshot"
// next to X (projeny_snapshot_path_for). The snapshot never tracks the live
// base file: every read of the base goes through it when it exists, and
// only a missing snapshot (first use), an adoption setup (a hand-edited
// Projeny: header naming a different base file than the status records, or
// a status-recorded base of ANOTHER mode — a hand-conversion into the
// Projeny: form), or `rebase` ever (re)writes it.

// Physical form of `path`, for the Projeny: chain's cycle identities, with
// the lexical fallback for paths that do not exist (a base file whose
// frozen snapshot survives it is a supported state, not a race): the whole
// walk must key its visited set on ONE consistent spelling per file, and
// physical_path — which dies on failure — is only guaranteed for paths
// that exist. The existence pre-check keeps the fallback SILENT for a
// missing base file (physical_path's die prints its error line before
// throwing, and a healthy frozen-snapshot read must not print one).
std::string physical_or_lexical(const std::string& path)
{
    if (!path_exists(path))
        return normalize_lexical(absolutize(path));
    try {
        return physical_path(path);
    } catch (const ProjenyFatalError&) {
        return normalize_lexical(absolutize(path));
    }
}

// The bytes of the base file that `referencing_path` (a Projeny:-based
// file, parsed as `pf`) is based on: the frozen snapshot's bytes when the
// snapshot exists, else the live bytes of the named base file — written
// into the snapshot first (the sanctioned first-use freeze), so every later
// read sees the same bytes even after the base file is deleted from git.
// Never overwrites an existing snapshot. Dies with recovery guidance when
// neither the snapshot nor the live base file exists. `what` is the
// human-readable phrase for the error (e.g. "set up '<file>'").
std::string resolve_committed_base_bytes(const std::string& referencing_path,
                                         const ProjenyFile& pf,
                                         const std::string& what)
{
    std::string snap = projeny_snapshot_path_for(referencing_path);
    if (path_exists(snap))
        return read_file_bytes(snap);
    std::string base_path = join_path(dirname_of(referencing_path), pf.base);
    if (!path_exists(base_path))
        die("base file '" + base_path +
            "' does not exist and neither does its snapshot '" + snap +
            "'; it is needed to " + what +
            ". This usually means the base file was deleted from git (for "
            "example by a rebase). If the workdir has no local changes, "
            "remove the status file and the workdir and run 'projeny setup' "
            "again; otherwise restore the base file (for example 'git "
            "checkout <commit> -- <file>') and run 'projeny setup' again");
    // write_file_bytes is temp file + rename, so the snapshot switches
    // atomically (the same protocol ensure_snapshot uses for archives).
    std::string bytes = read_file_bytes(base_path);
    write_file_bytes(snap, bytes);
    return bytes;
}

// The single top-level directory name of the extraction directory `dir` —
// the archive unpack and every chain link keep it at exactly one — checked
// against `pf`'s Origname: with the hard error that surfaces a hand-edited
// inconsistency. Organically create/rebase keep Origname synced with the
// base extraction, so this cannot fire for an unedited file; `file_path`
// names the file whose Origname: is wrong.
std::string checked_extraction_top(const std::string& file_path,
                                   const ProjenyFile& pf,
                                   const std::string& dir)
{
    std::vector<std::string> tops = list_dir_names(dir);
    std::string top = tops.size() == 1 ? tops[0] : "";
    if (top != pf.origname)
        die("file '" + file_path + "': Origname: is '" + pf.origname +
            "' but the base extraction's top-level directory is '" + top +
            "'; fix the Origname: header or rebase");
    return top;
}

// Extract the committed content of the .projeny file whose bytes are
// `bytes` and whose real path is `file_path` (the path is used for its
// directory, for its own frozen-snapshot naming, and for cycle identities)
// into a fresh ".chain.XXXXXX" directory inside `scratch_parent`, which the
// CALLER makes with make_scratch next to the top-level .projeny file: every
// link of the chain nests its extraction under that one directory, so the
// whole chain dies with the caller's TempDir and the final rename into the
// workdir never crosses filesystems. Returns the extraction directory,
// which contains exactly one top-level directory named pf.origname. Recurses
// through Projeny: chains (each link resolves its base through its OWN
// snapshot via resolve_committed_base_bytes). `chain`/`visited` carry the
// walk for cycle detection: chain is the file_path sequence (shown verbatim
// in the cycle message, repeat included), visited holds physical paths; the
// CALLER seeds both with the top-level project's own .projeny path. Dies on
// cycles and on Origname: mismatches. `what` is the human phrase the error
// messages name ("set up '<file>'").
std::string extract_committed_projeny(const std::string& bytes,
                                      const std::string& file_path,
                                      std::vector<std::string>& chain,
                                      std::set<std::string>& visited,
                                      const std::string& what,
                                      const std::string& scratch_parent)
{
    ProjenyFile pf = ProjenyFile::parse_bytes(bytes, "'" + file_path + "'");

    // Cycle check at every entry: the seed already holds the top-level
    // project, so a chain that walks back to any earlier file — a self-loop
    // ("Projeny: <this file>") or a longer cycle — dies here with the whole
    // walk shown.
    std::string self = physical_or_lexical(file_path);
    if (visited.count(self) != 0) {
        std::string shown;
        for (const std::string& step : chain) {
            if (!shown.empty())
                shown += " -> ";
            shown += "'" + step + "'";
        }
        shown += " -> '" + file_path + "'";
        die("the Projeny: base chain is circular: " + shown +
            "; Projeny: chains must not contain cycles");
    }
    visited.insert(self);
    chain.push_back(file_path);

    if (pf.is_projeny_based()) {
        // Recurse through this link's own frozen snapshot, then apply THIS
        // file's committed patch on top of the returned tree (patch paths
        // are relative to the tree contents, same as any checkout patch).
        std::string base_bytes =
            resolve_committed_base_bytes(file_path, pf, what);
        std::string dir = extract_committed_projeny(
            base_bytes, join_path(dirname_of(file_path), pf.base), chain,
            visited, what, scratch_parent);
        std::string top = checked_extraction_top(file_path, pf, dir);
        apply_fresh_patch(join_path(dir, top), pf.name, pf.patch,
                          "patch in '" + file_path + "'");
        return dir;
    }

    // Chain bottom: an Archive:- or URL:-based file. Its committed
    // extraction is the archive unpack (resolved snapshot-preferred,
    // exactly like the status/expected-tree machinery: the snapshot
    // survives git deleting the tarball) plus this file's committed patch.
    std::string work = make_tempdir(scratch_parent, ".chain.");
    std::string archive = resolve_status_archive(
        dirname_of(file_path), pf, what,
        "Projeny-based project: its base chain's archive is fetched on "
        "demand");
    unpack_single_top(archive, work, pf.origname);
    apply_fresh_patch(join_path(work, pf.origname), pf.name, pf.patch,
                      "patch in '" + file_path + "'");
    return work;
}

// Build the tree a Projeny:-based `pf` records for its .projeny file at
// `referencing_path`: the committed extraction of its base chain (see
// extract_committed_projeny) plus `patch` — the referencing file's own
// patch, applied with wid `pf.name` (empty for the callers that want the
// bare committed base, i.e. commit's diff base). `base_bytes` carries the
// already-resolved frozen base bytes (resolve_committed_base_bytes, or the
// setup paths' status-aware resolution below). `scratch` is the caller's
// make_scratch directory next to the .projeny file, so the result can be
// renamed into the workdir without crossing filesystems. Returns the tree
// path (inside scratch), mirroring build_tree_from_patch's contract.
// `what` names the command for error messages; `patch_what` names the patch
// for the apply-failure report.
// The CHAIN part of build_projeny_tree, shared with the callers that need
// the extraction tree BEFORE the referencing project's own patch lands on
// it (the conflicted-fresh setup builder): seeds the cycle walk, extracts
// the committed chain, and checks the referencing project's Origname:
// against the extraction's top directory — everything except the patch
// application.
std::string extract_projeny_base_tree(const std::string& referencing_path,
                                      const ProjenyFile& pf,
                                      const std::string& base_bytes,
                                      const std::string& scratch,
                                      const std::string& what)
{
    std::vector<std::string> chain;
    std::set<std::string> visited;
    // Seed the cycle walk with the referencing project itself: a base chain
    // that leads back here (most directly "Projeny: <this file>") is
    // circular.
    visited.insert(physical_or_lexical(referencing_path));
    chain.push_back(referencing_path);
    std::string dir = extract_committed_projeny(
        base_bytes, join_path(dirname_of(referencing_path), pf.base), chain,
        visited, what, scratch);
    // The referencing project's own Origname: must match the extraction's
    // top directory, exactly like every chain link's does.
    std::string top = checked_extraction_top(referencing_path, pf, dir);
    return join_path(dir, top);
}

std::string build_projeny_tree(const std::string& referencing_path,
                               const ProjenyFile& pf,
                               const std::string& base_bytes,
                               const std::string& patch,
                               const std::string& patch_what,
                               const std::string& scratch,
                               const std::string& what)
{
    std::string tree =
        extract_projeny_base_tree(referencing_path, pf, base_bytes, scratch,
                                  what);
    apply_fresh_patch(tree, pf.name, patch, patch_what);
    return tree;
}

// What the status file records about the base, for the status-aware base
// resolution below: whether a status file exists at all (`has` — its
// absence keeps the frozen-or-first-freeze law: a snapshot beside a
// status-less project is a frozen base, never re-frozen by a hand edit),
// whether the embedded copy is Projeny:-based (`projeny`), and the base
// file it names (`base`, empty unless `projeny`). Only setup paths read
// this, and each has either no status file or one that already parsed
// successfully, so these parses cannot fail newly.
struct StatusEmbeddedBase {
    bool has = false;
    bool projeny = false;
    std::string base;
};

StatusEmbeddedBase status_projeny_base(const Ctx& ctx)
{
    StatusEmbeddedBase out;
    if (!path_exists(ctx.statusfile))
        return out;
    out.has = true;
    StatusData st = StatusData::parse(ctx.statusfile);
    ProjenyFile emb = ProjenyFile::parse_bytes(
        st.embedded, "embedded copy in '" + ctx.statusfile + "'");
    out.projeny = emb.is_projeny_based();
    out.base = emb.base;
    return out;
}

// Adoption: a hand-edited Projeny: header is adopted by setup whenever the
// status records a DIFFERENT base — an embedded copy that names a different
// base FILE, or an embedded copy of ANOTHER MODE (an Archive:/URL: era the
// file was rebased away from and then hand-converted back out of; without
// this, the lingering frozen snapshot of the old Projeny: era would
// silently supply a stale base while the header names a different one).
// The live bytes of the newly named base OVERWRITE the frozen snapshot (the
// sanctioned re-freeze besides `rebase`) and become the base the fresh tree
// builds from. Called after the status-recorded trees are built (setup's
// E-tree), so the reconstruction of what the last setup used still sees the
// snapshot's old bytes. No adoption without a status file (`emb.has` false:
// the frozen-or-first-freeze law governs, documented and tested).
void adopt_projeny_base(const Ctx& ctx, const ProjenyFile& pf,
                        const StatusEmbeddedBase& emb, const std::string& what)
{
    if (!emb.has)
        return;
    if (emb.projeny && emb.base == pf.base)
        return;
    std::string base_path = join_path(ctx.pdir, pf.base);
    if (!path_exists(base_path))
        die("base file '" + base_path + "' does not exist; it is needed to " +
            what);
    write_file_bytes(projeny_snapshot_path_for(ctx.projeny_arg),
                     read_file_bytes(base_path));
}

// Setup's Projeny:-based tree builder: the committed extraction of the base
// chain plus `patch`, with the base resolved under the status-aware law —
// an adoption first (see adopt_projeny_base), then the frozen snapshot (or
// its first-use freeze). `scratch` is the caller's make_scratch dir.
std::string build_setup_projeny_tree(const Ctx& ctx, const ProjenyFile& pf,
                                     const StatusEmbeddedBase& emb,
                                     const std::string& patch,
                                     const std::string& patch_what,
                                     const std::string& scratch,
                                     const std::string& what)
{
    adopt_projeny_base(ctx, pf, emb, what);
    std::string bytes =
        resolve_committed_base_bytes(ctx.projeny_arg, pf, what);
    return build_projeny_tree(ctx.projeny_arg, pf, bytes, patch, patch_what,
                              scratch, what);
}

// Setup's Projeny:-based tree builder, CONFLICTED-FRESH flavor: the same
// status-aware base resolution, but the project's own patch lands with
// inline conflict markers (vcs_apply_with_conflicts, kFreshApply — the
// target is a freshly built chain extraction, so already-applied is
// impossible by construction) and every conflicted workdir-relative path
// is appended to *conflicts. This is what makes `erase-setup --force` +
// `setup` a working recovery when the frozen chain moved under the
// checkout (a patch that no longer applies produces a marker-bearing
// checkout the user resolves, then commits). The chain itself must still
// extract cleanly: a chain-internal patch failure dies with the
// fresh-setup chain guidance (there is no checkout to merge and no status
// to erase, so markers for a broken link have no recovery — see
// die_fresh_projeny_chain_broken).
std::string extract_projeny_base_tree(const std::string& referencing_path,
                                      const ProjenyFile& pf,
                                      const std::string& base_bytes,
                                      const std::string& scratch,
                                      const std::string& what);
std::string build_setup_projeny_tree_with_conflicts(
    const Ctx& ctx, const ProjenyFile& pf, const StatusEmbeddedBase& emb,
    const std::string& patch, const std::string& what,
    const std::string& scratch, std::vector<std::string>* conflicts)
{
    adopt_projeny_base(ctx, pf, emb, what);
    std::string bytes =
        resolve_committed_base_bytes(ctx.projeny_arg, pf, what);
    std::string tree;
    {
        TolerantPatchApply tol;
        try {
            tree = extract_projeny_base_tree(ctx.projeny_arg, pf, bytes,
                                             scratch, what);
        } catch (const PatchApplyError& e) {
            (void)e;
            die_fresh_projeny_chain_broken(ctx);
        }
    }
    if (!normalize_patch_text(patch).empty())
        vcs_apply_with_conflicts(tree, patch, pf.name, conflicts,
                                 VcsApplyMode::kFreshApply);
    return tree;
}

// The "from '<X>'" tail of setup's progress lines: the archive name for the
// classic/URL forms, the Projeny: base file for a Projeny:-based project
// (whose archive field is empty by construction).
std::string setup_from_name(const ProjenyFile& pf)
{
    return pf.is_projeny_based() ? pf.base : pf.archive;
}

// The committed-extraction TREE (the single top-level directory inside
// extract_committed_projeny's result) of the .projeny file whose bytes are
// `bytes` and whose real path is `file_path`: the bytes' own Origname: names
// it (the extraction enforces that at every link). `rebase`'s engine works
// on these trees directly and deliberately skips the REFERENCING project's
// own Origname: check — a rebase self-heals a stale Origname: header — so
// the callers go through here instead of build_projeny_tree, seeding
// chain/visited with the referencing project themselves (cycle-checked:
// a chain that reaches back to the project dies with the circular-chain
// error).
std::string extract_committed_projeny_tree(const std::string& bytes,
                                           const std::string& file_path,
                                           std::vector<std::string>& chain,
                                           std::set<std::string>& visited,
                                           const std::string& what,
                                           const std::string& scratch_parent)
{
    ProjenyFile pf = ProjenyFile::parse_bytes(bytes, "'" + file_path + "'");
    std::string dir = extract_committed_projeny(bytes, file_path, chain,
                                                visited, what, scratch_parent);
    return join_path(dir, pf.origname);
}

// Validate the .projeny-base form of rebase/create's base argument (the
// parse_archive_args flavor whose FIRST argument ends in ".projeny") against
// the TARGET .projeny file `target_path`:
//
//   - a plain filename resolves in the target's own directory (the Projeny:
//     header stores a plain filename, so a base always lives next to the
//     file that names it); an argument with a '/' resolves against the CWD
//     and must then land in the target's directory anyway;
//   - the base file must exist and parse (ProjenyFile::parse dies with its
//     own messages);
//   - the LIVE Projeny: chain walked FROM the base (each link's Projeny:
//     header read from its live bytes, each base resolved against that
//     link's own directory) must not reach the target or repeat a file —
//     both are cycle refusals, checked BEFORE anything is touched.
//
// Returns the resolved base path and fills *out_pf with the base's parse
// (create needs its Origname:; rebase needs it too). Every die is a pure
// refusal: nothing has been read but these files.
std::string validate_projeny_base_arg(const std::string& raw,
                                      const std::string& target_path,
                                      const char* cmd, ProjenyFile* out_pf)
{
    std::string pdir = dirname_of(target_path);
    std::string resolved = raw.find('/') == std::string::npos
                               ? join_path(pdir, raw)
                               : absolutize(raw);
    if (!path_exists(resolved))
        die("base file '" + resolved + "' does not exist; it is needed to " +
            std::string(cmd) + " '" + target_path + "'");
    if (physical_or_lexical(dirname_of(resolved)) !=
        physical_or_lexical(pdir))
        die(std::string("cannot ") + cmd + " '" + target_path +
            "' with base '" + raw +
            "': the Projeny: base file must live in the same directory as '" +
            target_path + "'");
    *out_pf = ProjenyFile::parse(resolved);

    // Cycle pre-check on the LIVE chain: the walk is seeded with the
    // target's own physical path (lexical when it does not exist yet — a
    // create), so reaching the target — most directly the self-loop
    // "Projeny: <this file>" — or repeating any other file dies here.
    std::string target_id = physical_or_lexical(target_path);
    std::set<std::string> visited;
    visited.insert(target_id);
    std::string base = resolved;
    for (;;) {
        std::string self = physical_or_lexical(base);
        if (visited.count(self) != 0) {
            if (self == target_id)
                die(std::string("cannot ") + cmd + " '" + target_path +
                    "': basing it on '" + resolved +
                    "' would create a cycle ('" + resolved +
                    "' transitively depends on '" + target_path +
                    "'); Projeny: chains must not contain cycles");
            die(std::string("cannot ") + cmd + " '" + target_path +
                "': basing it on '" + resolved + "' would create a cycle ('" +
                base + "' is reached twice in the chain); Projeny: chains "
                       "must not contain cycles");
        }
        visited.insert(self);
        ProjenyFile pf = ProjenyFile::parse(base);
        if (!pf.is_projeny_based())
            break; // chain bottom: an Archive:/URL:-based file
        base = join_path(dirname_of(base), pf.base);
    }
    return resolved;
}

// Merge user diff U (WID labels, wid `uwid`) onto fresh tree N (on-disk
// path), using base tree E for 3-way merges. `user_tree` is the on-disk
// user workdir holding the THEIRS content; it may differ from N (in setup,
// N is a fresh temp tree while the user workdir has the local edits).
// Records conflicts into `conflicts` (workdir-relative paths). Applies
// file-by-file; files that apply cleanly via the internal applier are
// applied directly, the rest go through merge_one_file.
// Derive the workdir-relative paths a failed patch block touches: the
// parser's paths when present, else the display string when it looks like a
// plain path (opaque blocks). Dies naming the block when neither yields a
// path; `verb` spells the command for the message.
std::vector<std::string> failure_paths_or_die(const VcsFailure& f,
                                              const char* verb)
{
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
        die(std::string("cannot ") + verb + ": unsupported patch block '" + d +
                "' cannot be merged; resolve manually",
            "  " + d + "\n");
    }
    return touched;
}

void merge_user_diff_onto(const std::string& base_tree, const std::string& fresh_tree,
                          const std::string& user_tree, const std::string& workdir,
                          const std::string& wid, const std::string& uwid,
                          const std::string& upatch,
                          const std::string& scratch_pdir,
                          const std::string& scratch_filebase,
                          std::vector<std::string>* conflicts)
{
    // Parse U with its own wid (uwid): U was diffed with the old wid, which
    // may differ from the fresh tree's wid. Failures already carry
    // workdir-relative paths, so no second parser and no wid stripping here.
    // kAllowAlready: the target tree N is fresh, but U is the user's
    // recorded diff — the new base may already contain part of it (upstream
    // incorporated the same change), and skipping those blocks is what keeps
    // that a clean merge instead of a spurious conflict.
    (void)wid;
    std::vector<VcsFailure> bad =
        apply_patch_per_file(workdir, upatch, uwid, VcsApplyMode::kAllowAlready);
    if (bad.empty())
        return;
    // For each failed block, do a 3-way merge of base/ours(=fresh)/theirs.
    for (const auto& f : bad) {
        std::vector<std::string> touched =
            failure_paths_or_die(f, "merge local changes");
        for (const std::string& rel : touched) {
            if (rel.empty())
                continue;
            // base = E-file, ours = N-file (fresh, possibly partially
            // patched with the clean files), theirs = user workdir file.
            // Failed blocks are skipped atomically per --include, so the
            // user file still holds the theirs content. Snapshot it first:
            // merge_one_file writes its output to the N-tree path, which
            // for in-place merges IS the user path (snapshot protects us).
            TempDir one(make_scratch(scratch_pdir, scratch_filebase, "mg"));
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
    // URL-based projects have no checked-in archive: their snapshot is the
    // verified download (see ensure_url_snapshot), which is a no-op — no
    // network — whenever it already matches a URL: hash. Projeny:-based
    // projects have no archive either: their frozen base snapshot (see
    // projeny_snapshot_path_for) is created-if-missing from the live base
    // file's bytes here and never overwritten — the snapshot is frozen, and
    // only an adoption setup (a hand-edited Projeny: header) or a rebase
    // re-freezes it.
    if (!trim(sd.embedded).empty()) {
        ProjenyFile pf = ProjenyFile::parse_bytes(
            sd.embedded, "embedded copy for '" + ctx.statusfile + "'");
        if (pf.is_url_based())
            // announce_skip = false: bookkeeping re-assertion of a snapshot
            // this same setup run just materialized and already reported
            // (see g_skip_announced_for in try_ensure_url_snapshot).
            ensure_url_snapshot(
                ctx.pdir, pf,
                "record which archive this setup used in the status file",
                false);
        else if (pf.is_projeny_based())
            resolve_committed_base_bytes(
                ctx.projeny_arg, pf,
                "record which base file this setup used in the status file");
        else
            ensure_snapshot(join_path(ctx.pdir, pf.archive));
    }
    write_file_bytes(ctx.statusfile, sd.serialize());
}

// Build the fresh tree for `cur` inside `tmp`: the current base content
// (the archive, or the committed extraction of the Projeny: base chain)
// plus the current patch. Returns the tree path (inside tmp). Shared by
// every fresh-setup flavor so they all see exactly the same set of paths
// setup would write (tarball files, patch modifications, and patch-added
// files alike). Unpacks before anything is touched (a bad patch dies
// leaving the checkout intact). For a URL-based project the "current
// archive" is the verified .snapshot download (materialized on first use).
// For a Projeny:-based project the base is the frozen snapshot's committed
// extraction, resolved under the status-aware law (an adoption of a
// hand-edited Projeny: header — or of a status-recorded base of another
// mode — included: a status file that records no Projeny: base keeps the
// frozen-or-first-freeze law, the normal fresh-setup case). When
// `out_conflicts` is non-null, a Projeny:-based project's patch lands with
// inline conflict markers and its conflicted paths are appended there
// (see build_setup_projeny_tree_with_conflicts); classic Archive:/URL:
// projects keep the hard die on an unapplicable patch, byte-identically.
std::string build_fresh_tree(const Ctx& ctx, const ProjenyFile& cur,
                             TempDir& tmp,
                             std::vector<std::string>* out_conflicts = nullptr)
{
    if (cur.is_projeny_based()) {
        if (out_conflicts != nullptr)
            return build_setup_projeny_tree_with_conflicts(
                ctx, cur, status_projeny_base(ctx), cur.patch,
                "set up '" + ctx.projeny_arg + "'", tmp.path, out_conflicts);
        return build_setup_projeny_tree(
            ctx, cur, status_projeny_base(ctx), cur.patch,
            "patch in '" + ctx.projeny_arg + "' (base '" + cur.base + "')",
            tmp.path, "set up '" + ctx.projeny_arg + "'");
    }
    return build_tree_from_patch(
        tmp,
        materialize_archive(ctx.pdir, cur,
                            "set up '" + ctx.projeny_arg + "'"),
        cur.origname, cur.name, cur.patch,
        "patch in '" + ctx.projeny_arg + "' (archive '" + cur.archive + "')");
}

// Fresh setup: unpack the current archive, apply the current patch, and move
// the result into place. Unpacks before touching the workdir (a bad patch
// dies leaving the checkout intact), and carries untracked binaries across
// the replacement so build junk and package tarballs survive.
// `out_conflicts` (see build_fresh_tree) collects a Projeny:-based
// project's conflict-markered paths; the caller records and reports them.
void do_fresh_setup(const Ctx& ctx, const ProjenyFile& cur,
                    std::vector<std::string>* out_conflicts = nullptr)
{
    std::string workdir = join_path(ctx.pdir, cur.name);
    TempDir tmp(make_scratch(ctx.pdir, ctx.filebase, "setup"));
    std::string fresh = build_fresh_tree(ctx, cur, tmp, out_conflicts);
    if (path_exists(workdir))
        reconcile_binaries(workdir, fresh, true);
    if (path_exists(workdir) && !remove_recursive(workdir))
        die("cannot remove existing workdir '" + workdir + "'");
    move_path(fresh, workdir);
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
void do_fresh_setup_into_existing(
    const Ctx& ctx, const ProjenyFile& cur,
    std::vector<std::string>* out_conflicts = nullptr)
{
    std::string workdir = join_path(ctx.pdir, cur.name);
    TempDir tmp(make_scratch(ctx.pdir, ctx.filebase, "setup"));
    std::string fresh = build_fresh_tree(ctx, cur, tmp, out_conflicts);

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
        size_t shown = std::min(conflicts.size(), kMaxListed);
        std::string detail = "setup would overwrite these existing paths:\n" +
                             bullet_list(std::vector<std::string>(
                                 conflicts.begin(),
                                 conflicts.begin() + (long)shown));
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
           rel_to_cwd(journal_path).c_str());
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

// Workdir-relative form of a diff-label repo path: split_file_diffs strips
// the a/ b/ prefixes but keeps the wid component ("<wid>/rel"), while
// pending-op keep lists and conflict entries are workdir-relative.
std::string wid_rel(const std::string& repo_path, const std::string& wid)
{
    if (repo_path == wid)
        return "";
    if (!wid.empty() && starts_with(repo_path, wid + "/"))
        return repo_path.substr(wid.size() + 1);
    return repo_path;
}

// True when diff block `b` is a pure addition (its old side is absent).
// The "diff --git" line cannot tell: for adds and deletes both projeny and
// git repeat the present path on both sides, so the absent side shows only
// in the header below it ("new file mode" / a "/dev/null" --- label) —
// except for foreign patches, which do put /dev/null on the header line.
// Scanning stops at the first hunk so hunk bodies can never vote (a
// removed line could otherwise mimic a marker).
bool block_is_pure_add(const FileDiff& b)
{
    if (b.a_path.empty())
        return !b.b_path.empty(); // foreign "/dev/null b/x" header form
    if (b.a_path != b.b_path)
        return false; // rename: both sides present
    for (const std::string& line : split_lines(b.text)) {
        if (starts_with(line, "@@"))
            break; // hunks start: the header is over
        if (starts_with(line, "new file mode ") ||
            starts_with(line, "--- /dev/null"))
            return true;
        if (starts_with(line, "deleted file mode ") ||
            starts_with(line, "+++ /dev/null"))
            return false;
    }
    return false;
}

// True when diff block `b` is a pure deletion (its new side is absent);
// mirror image of block_is_pure_add.
bool block_is_pure_delete(const FileDiff& b)
{
    if (b.b_path.empty())
        return !b.a_path.empty(); // foreign "a/x /dev/null" header form
    if (b.a_path != b.b_path)
        return false; // rename: both sides present
    for (const std::string& line : split_lines(b.text)) {
        if (starts_with(line, "@@"))
            break; // hunks start: the header is over
        if (starts_with(line, "deleted file mode ") ||
            starts_with(line, "+++ /dev/null"))
            return true;
        if (starts_with(line, "new file mode ") ||
            starts_with(line, "--- /dev/null"))
            return false;
    }
    return false;
}

// Expected destination of `rel` under a pending rename (s -> d): rel is
// covered when it names the source itself (a file move) or lives under it
// (a directory move), and the covered path's destination is d plus the
// remainder. Returns "" when the entry does not cover rel.
// (vcs_covers_keep_path answers only bool; the report needs the rewritten
// destination to find the matching add side.)
std::string rename_dest_for(const std::string& rel,
                            const std::pair<std::string, std::string>& rn)
{
    const std::string& s = rn.first;
    if (rel == s)
        return rn.second;
    if (rel.size() > s.size() && rel.compare(0, s.size(), s) == 0 &&
        rel[s.size()] == '/')
        return rn.second + rel.substr(s.size());
    return "";
}

// Per-file merge report for a setup that had real local changes: one
// "merged:"/"added:"/"deleted:"/"renamed:" line per FileDiff block, in
// block order, plus a "conflict:" line for every path in `conflicts` (a
// conflicted file reports as "conflict:" instead of its block's own line;
// a conflicted rename therefore names the destination, which is the path
// the status file records). Pure additions the pending ops never
// registered — when `keep` is non-null — are untracked files that ride
// along into the new tree without being changes, so they stay out. Every
// conflict path is guaranteed a line even when no block names it (e.g. a
// conflict carried over from an earlier setup).
//
// The blocks usually come from a plain workdir-vs-expected diff without
// the forced-rename pairing `projeny diff`/`commit` use, so a pending
// `projeny mv` whose moved file diverged past the rename-similarity
// threshold renders as an independent pure delete plus a pure add.
// `renames` (the pending list from the status file; null when none is
// known) pairs those sides back up: a pure delete whose path a pending
// rename covers — exactly, or under a moved directory — and the pure add
// sitting at the expected destination report one
// "renamed: <src> -> <dst>" line at the first of the two blocks' positions
// instead of their own lines. Sides without a counterpart behave exactly
// as before (a new file inside a moved directory stays "added:", a delete
// with no add side stays "deleted:"); a conflict on either side suppresses
// the pair's line like any other, covered by the trailing "conflict:".
std::vector<std::string> merge_report_lines(
    const std::vector<FileDiff>& blocks, const std::string& wid,
    const std::vector<std::string>* keep,
    const std::vector<std::string>& conflicts,
    const std::vector<std::pair<std::string, std::string>>* renames)
{
    auto is_conflicted = [&](const std::string& rel) {
        return std::find(conflicts.begin(), conflicts.end(), rel) !=
               conflicts.end();
    };
    std::vector<std::string> lines;
    auto emit = [&](const std::string& line) {
        if (std::find(lines.begin(), lines.end(), line) == lines.end())
            lines.push_back(line);
    };
    // Pre-pass: pair pending-rename sides before anything is emitted.
    // used[i] marks a block consumed by a pair; pair_line holds the pair's
    // "renamed:" line, parked at the first of the two block positions
    // (empty when a conflict on either side suppresses it).
    std::vector<bool> used(blocks.size(), false);
    std::vector<std::string> pair_line(blocks.size(), "");
    if (renames != nullptr) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (used[i] || !block_is_pure_delete(blocks[i]))
                continue;
            std::string a = wid_rel(blocks[i].a_path, wid);
            std::string dest;
            for (const auto& rn : *renames) {
                dest = rename_dest_for(a, rn);
                if (!dest.empty())
                    break;
            }
            if (dest.empty())
                continue; // no pending rename covers this delete
            for (size_t j = 0; j < blocks.size(); ++j) {
                if (used[j] || !block_is_pure_add(blocks[j]))
                    continue;
                if (wid_rel(blocks[j].b_path, wid) != dest)
                    continue; // not this rename's add side
                used[i] = used[j] = true;
                if (!is_conflicted(a) && !is_conflicted(dest))
                    pair_line[i < j ? i : j] =
                        "renamed: " + a + " -> " + dest;
                break;
            }
        }
    }
    for (size_t i = 0; i < blocks.size(); ++i) {
        const FileDiff& b = blocks[i];
        std::string a = wid_rel(b.a_path, wid);
        std::string r = wid_rel(b.b_path, wid);
        if (b.a_path.empty() && b.b_path.empty())
            continue; // opaque block: the merge dies before any report
        if (used[i]) {
            // Half of a paired pending rename: its line (when not
            // conflicted away) was emitted at the first block's position.
            if (!pair_line[i].empty())
                emit(pair_line[i]);
            continue;
        }
        if (block_is_pure_add(b)) {
            // `keep` filters only untracked riders (diffs taken against a
            // workdir, where files the pending ops never registered ride
            // along): with a null `keep` — the committed-patch caller,
            // where every block is a real change — every add reports.
            if (keep != nullptr && !vcs_covers_keep_path(*keep, r))
                continue; // untracked rider, not a change
            if (!is_conflicted(r))
                emit("added: " + r);
        } else if (block_is_pure_delete(b)) {
            if (!is_conflicted(a))
                emit("deleted: " + a);
        } else if (a != r) {
            if (!is_conflicted(a) && !is_conflicted(r))
                emit("renamed: " + a + " -> " + r);
        } else {
            if (!is_conflicted(r))
                emit("merged: " + r);
        }
    }
    for (const auto& c : conflicts)
        emit("conflict: " + c);
    return lines;
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
        std::vector<std::string> dirs;
        for (auto& c : candidates) {
            if (is_dir(c))
                dirs.push_back(c);
        }
        std::string detail = bullet_list(dirs);
        die("multiple workdirs exist next to conflicted '" + ctx.projeny_arg +
                "'; remove or rename all but one and run 'projeny setup' again",
            detail);
    }
    bool have_workdir = found > 0;

    // All trees below are temp-only until the write phase at the end, and
    // all of them are scratch dirs next to the .projeny file (make_scratch).
    TempDir tN(make_scratch(ctx.pdir, ctx.filebase, "cN"));
    TempDir tO(make_scratch(ctx.pdir, ctx.filebase, "cO"));
    TempDir tB(make_scratch(ctx.pdir, ctx.filebase, "cB"));
    TempDir tE(make_scratch(ctx.pdir, ctx.filebase, "cE"));
    TempDir tS(make_scratch(ctx.pdir, ctx.filebase, "cS"));

    // Harder case first (reads only): diff the checkout against the base
    // reconstruction while the workdir is still the original.
    std::string U_unc;
    std::string snap;
    std::string Etree;
    bool harder = false;
    if (have_workdir && harder_base != nullptr) {
        // harder_base is status- or journal-derived, so its archive may
        // have been deleted from git since the last setup: use the
        // snapshot when it exists. A Projeny:-based harder base extracts
        // its committed content from the project's frozen base snapshot
        // instead (the same resolution every other read of this project's
        // base uses).
        if (harder_base->is_projeny_based()) {
            // Tolerant: the frozen-chain-moved state (an upstream Projeny:
            // link was rebased) bricks this reconstruction like any other;
            // die with the recovery guidance instead of the bare apply
            // report.
            try {
                TolerantPatchApply tol;
                Etree = build_projeny_tree(
                    ctx.projeny_arg, *harder_base,
                    resolve_committed_base_bytes(
                        ctx.projeny_arg, *harder_base,
                        "reconstruct the base tree for '" + ctx.projeny_arg +
                            "'"),
                    harder_base->patch,
                    "embedded patch in '" + ctx.statusfile + "'", tE.path,
                    "reconstruct the base tree for '" + ctx.projeny_arg + "'");
            } catch (const PatchApplyError&) {
                die_projeny_chain_moved("set up", ctx.projeny_arg);
            }
        } else {
            Etree = build_tree_from_patch(
                tE, resolve_status_archive(ctx.pdir, *harder_base,
                                           "reconstruct the base tree for '" +
                                               ctx.projeny_arg + "'"),
                harder_base->origname, harder_base->name, harder_base->patch,
                "embedded patch in '" + ctx.statusfile + "'");
        }
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

    // Fresh local (ours) tree FIRST. The local side comes from a
    // git-conflicted .projeny (or the setup journal), so git may have just
    // deleted its archive (upstream rebase to a new tarball): read the
    // snapshot when it exists. A Projeny:-based local side extracts its
    // committed content from the project's frozen base snapshot (created
    // from the local side's own base file when it is missing) plus its
    // patch. Resolve it exactly once, and BEFORE the
    // upstream materialization below: when the two sides share the derived
    // archive name — the normal case when a git merge conflicts on the
    // URL: lines and both sides keep the same tarball filename — they also
    // share one .snapshot, and materializing the upstream side re-downloads
    // over it, so the local side must be reconstructed from the shared
    // snapshot before that can happen (mirrors setup_impl's
    // E-tree-before-N-tree ordering). The same applies to the Projeny:
    // form's shared frozen snapshot: both conflict sides of one file
    // resolve their base through the SAME .<stem>.snapshot (a base-switching
    // conflict is the rare case where the second side then sees the first
    // side's frozen bytes — fail-closed bookkeeping, never data loss; a
    // later `rebase` re-freezes).
    std::string Otree;
    if (local.is_projeny_based()) {
        // Tolerant with recovery guidance, like every frozen-chain build.
        try {
            TolerantPatchApply tol;
            Otree = build_projeny_tree(
                ctx.projeny_arg, local,
                resolve_committed_base_bytes(
                    ctx.projeny_arg, local,
                    "reconstruct the local side of '" + ctx.projeny_arg + "'"),
                local.patch, "local side of '" + ctx.projeny_arg + "'",
                tO.path, "reconstruct the local side of '" + ctx.projeny_arg +
                             "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("set up", ctx.projeny_arg);
        }
    } else {
        std::string local_archive = resolve_status_archive(
            ctx.pdir, local,
            "reconstruct the local side of '" + ctx.projeny_arg + "'");
        Otree = build_tree_from_patch(
            tO, local_archive, local.origname, local.name, local.patch,
            "local side of '" + ctx.projeny_arg + "'");
    }

    // Merge base: the shared base archive when both sides name the same
    // tarball and top dir (the common git-conflict case) — or, for two
    // Projeny:-based sides, the shared base file and top dir, extracted
    // from the frozen snapshot with NEITHER side's patch. Otherwise there
    // is no common ancestor, so the base stays an empty dir (missing files):
    // anything both sides changed differently then conflicts instead of
    // silently picking a side. No data loss either way. Built from the same
    // already-resolved local base as Otree.
    std::string Btree = tB.path;
    if (local.is_projeny_based() && cur.is_projeny_based()) {
        if (local.base == cur.base && local.origname == cur.origname) {
            // Tolerant with recovery guidance, like every frozen-chain
            // build.
            try {
                TolerantPatchApply tol;
                Btree = build_projeny_tree(
                    ctx.projeny_arg, local,
                    resolve_committed_base_bytes(
                        ctx.projeny_arg, local,
                        "reconstruct the shared base of '" + ctx.projeny_arg +
                            "'"),
                    "", "", tB.path,
                    "reconstruct the shared base of '" + ctx.projeny_arg +
                        "'");
            } catch (const PatchApplyError&) {
                die_projeny_chain_moved("set up", ctx.projeny_arg);
            }
        }
    } else if (local.archive == cur.archive &&
               local.origname == cur.origname) {
        unpack_single_top(resolve_status_archive(
                              ctx.pdir, local,
                              "reconstruct the local side of '" +
                                  ctx.projeny_arg + "'"),
                          tB.path, local.origname);
        Btree = join_path(tB.path, local.origname);
    }

    // Fresh upstream (theirs) tree, materialized LAST: a URL-based upstream
    // side re-fetches the shared .snapshot whenever its bytes do not match
    // the upstream URL: hashes (the local side normally holds them at this
    // point), overwriting it with the upstream bytes. That is safe only
    // because both local trees above were already unpacked from it. A
    // Projeny:-based upstream side extracts its committed content through
    // the same frozen snapshot the local side used (resolve_committed_base_
    // bytes never overwrites an existing snapshot).
    std::string Ntree;
    if (cur.is_projeny_based()) {
        // Tolerant with recovery guidance, like every frozen-chain build.
        try {
            TolerantPatchApply tol;
            Ntree = build_projeny_tree(
                ctx.projeny_arg, cur,
                resolve_committed_base_bytes(
                    ctx.projeny_arg, cur,
                    "check out the upstream side of '" + ctx.projeny_arg +
                        "'"),
                cur.patch, "upstream side of '" + ctx.projeny_arg + "'",
                tN.path,
                "check out the upstream side of '" + ctx.projeny_arg + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("set up", ctx.projeny_arg);
        }
    } else {
        Ntree = build_tree_from_patch(
            tN, materialize_archive(ctx.pdir, cur,
                                    "check out the upstream side of '" +
                                        ctx.projeny_arg + "'"),
            cur.origname, cur.name, cur.patch,
            "upstream side of '" + ctx.projeny_arg + "'");
    }

    // Stage 1: merge the committed local patch onto the fresh upstream tree.
    std::vector<std::string> conflicts;
    merge_user_diff_onto(Btree, Ntree, Otree, Ntree, cur.name, local.name,
                         local.patch, ctx.pdir, ctx.filebase, &conflicts);
    // Stage 2 (harder case): apply the uncommitted workdir-vs-base diff
    // on top of the conflicted checkout.
    if (harder) {
        merge_user_diff_onto(Etree, Ntree, snap, Ntree, cur.name,
                             harder_base->name, U_unc, ctx.pdir, ctx.filebase,
                             &conflicts);
    }
    sort_unique(&conflicts);

    // Write phase: everything computed, now replace the checkout.
    //
    // Order matters for crash recovery (see ConflictJournal above). Each
    // file write is itself atomic (temp file + rename inside
    // write_file_bytes, so a crash never leaves a half-written file), but
    // the journal, the status file, the .projeny file, and the workdir
    // cannot all flip in one rename. The order below keeps every crash
    // window recoverable by just rerunning 'projeny setup', which finds
    // the journal and recomputes this same merge deterministically:
    //   1. write the journal (records the conflicted raw bytes plus which
    //      side is upstream) — before anything else is touched;
    //   2. write the status file (embedding the upstream text);
    //   3. write the .projeny file (upstream text);
    //   4. move the merged tree into place;
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
               rel_to_cwd(ctx.projeny_arg).c_str());
    else
        printf("projeny: resolved conflicted '%s' by taking upstream for the "
               ".projeny file and merging local changes into '%s'\n",
               rel_to_cwd(ctx.projeny_arg).c_str(),
               rel_to_cwd(cur_workdir).c_str());
    // Per-file report of what the merge brought in: the local side's
    // committed patch (stage 1) plus, in the harder case, the uncommitted
    // workdir-vs-status diff (stage 2). Untracked riders in the harder diff
    // (never `projeny add`ed) are not changes and stay out of the report.
    // The pending renames ride along too: stage 2's diff is a plain
    // workdir-vs-expected diff, so a divergent pending mv would otherwise
    // split into delete+add in the report.
    std::vector<std::string> report = merge_report_lines(
        split_file_diffs(local.patch), local.name, nullptr, sd.conflicts,
        union_status != nullptr ? &union_status->renamed : nullptr);
    if (harder) {
        std::vector<std::string> harder_keep;
        if (union_status != nullptr) {
            for (const auto& a : union_status->added)
                harder_keep.push_back(a);
            for (const auto& rn : union_status->renamed)
                harder_keep.push_back(rn.second);
        }
        std::vector<std::string> more = merge_report_lines(
            split_file_diffs(U_unc), harder_base->name,
            union_status != nullptr ? &harder_keep : nullptr, sd.conflicts,
            union_status != nullptr ? &union_status->renamed : nullptr);
        // The two stages dedup only within themselves, and each names every
        // conflict, so a file conflicted in both stages (e.g. changed in the
        // committed patch and again uncommitted) would print twice. Append
        // only lines the first stage did not already report, preserving the
        // stage order (sort_unique would scramble it).
        for (const auto& l : more) {
            if (std::find(report.begin(), report.end(), l) == report.end())
                report.push_back(l);
        }
    }
    if (report.empty()) {
        // Nothing was merged (empty local side and no uncommitted diff):
        // say so instead of claiming a merge. Conflicts can never reach
        // this branch: merge_report_lines appends one "conflict:" line per
        // entry, so any unresolved conflict keeps the report non-empty.
        printf("projeny: no local changes to merge onto '%s'\n",
               rel_to_cwd(cur_workdir).c_str());
        return 0;
    }
    printf("projeny: merged local changes onto '%s':\n",
           rel_to_cwd(cur_workdir).c_str());
    for (const auto& l : report)
        printf("  %s\n", l.c_str());
    if (!sd.conflicts.empty()) {
        printf("projeny: setup left conflicts (exit 1); fix them, then "
               "`resolve` each file and `commit`\n");
        return 1;
    }
    return 0;
}

// Express the absolute path `abs` relative to the current directory,
// lexically (both sides normalized first): the shared component prefix is
// dropped, one ".." is emitted per remaining current-directory component,
// and the rest of `abs` follows. The workdir-sibling rule below uses this to
// keep returning relative .projeny paths for relative arguments — so
// messages keep naming files the way the user does — including the shapes
// that make '.' and '..' work: from inside the workdir, '.' yields
// "../<workdir>.projeny"; from a workdir subdirectory, '..' yields
// "../<workdir>.projeny" too. Paths that share no prefix (different roots)
// come back absolute.
std::string rel_to_cwd(const std::string& abs)
{
    // get_cwd() can fail here: setup/package/extract replace the workdir,
    // which may delete the directory the process's CWD sits in, and these
    // displays run after that. With no CWD there is nothing to be relative
    // to, so keep the absolute spelling.
    char buf[8192];
    if (!getcwd(buf, sizeof(buf)))
        return normalize_lexical(abs);
    std::string cwd = normalize_lexical(buf);
    std::string a = normalize_lexical(abs);
    // Defensive: with both sides normalized, equal component lists mean the
    // equal strings caught above, so the walk below always climbs or
    // descends; keep the "." case explicit anyway.
    if (a == cwd)
        return ".";
    std::vector<std::string> cv = split_path_components(cwd, true),
                             av = split_path_components(a, true);
    size_t common = 0;
    while (common < cv.size() && common < av.size() && cv[common] == av[common])
        ++common;
    std::string out;
    for (size_t i = common; i < cv.size(); ++i) {
        if (!out.empty())
            out += "/";
        out += "..";
    }
    for (size_t i = common; i < av.size(); ++i) {
        if (!out.empty())
            out += "/";
        out += av[i];
    }
    return out;
}

// Softly parse `raw` as a .projeny file: true with *out filled, false when
// the bytes are not a usable .projeny file — without printing anything on
// the common failure shapes. validate_projeny_bytes is silent and rejects
// exactly the shapes that matter for the callers below (git conflict
// markers, NUL bytes, missing headers), so a conflicted or garbage file
// falls back cleanly; parse_bytes runs inside a catch for the remaining
// malformed-header cases. Callers use this to CHECK a .projeny file they
// do not own (a workdir sibling, a corner-guard candidate): any failure
// means "not attributable", never an error report of its own.
bool try_parse_projeny_softly(const std::string& raw, const std::string& what,
                              ProjenyFile* out)
{
    if (validate_projeny_bytes(raw) != "")
        return false;
    try {
        *out = ProjenyFile::parse_bytes(raw, what);
    } catch (const ProjenyFatalError&) {
        return false;
    }
    return true;
}

// Resolve a project argument to a .projeny file path. Accepts either the
// .projeny file itself or a directory: a directory next to a
// "<dir>.projeny" sibling names it implicitly (the workdir rule — this is
// what makes '.' from inside the workdir and '..' from a workdir
// subdirectory work), otherwise a directory holding exactly one
// "*.projeny" file names that one. A non-directory path also resolves to
// its "<arg>.projeny" sibling when one exists (the checkout directory was
// never created, or a bare name was given). Anything else comes back
// unchanged so the caller's own error reports it — including a missing
// "*.projeny" path, whose read failure carries recovery guidance no generic
// resolver error can improve on. Dies otherwise.
//
// The workdir-sibling rule carries one validation: the workdir is always
// named by the Name: header, so the sibling must actually claim THIS
// directory — its parsed Name: must equal the directory's basename. A
// directory sitting next to a "<dir>.projeny" whose Name: differs is NOT
// that project's checkout (it just happens to share the file's stem), and
// resolving into it would let a command read or destroy the wrong
// project's state; that mismatch is a hard error. The sibling is parsed
// SOFTLY (see try_parse_projeny_softly): a file that cannot be parsed
// keeps the historical behavior — git-conflicted .projeny recovery via
// `setup` depends on resolving the directory and then hitting setup's own
// recovery path, not on a resolver refusal.
//
// Arguments that exist on disk are resolved PHYSICALLY first, on the raw
// (trailing-slash-stripped) spelling, BEFORE lexical normalization:
// normalize_lexical collapses ".." lexically, so a directory arg like
// "sym/.." (a symlinked intermediate) would examine the wrong directory —
// silently resolving, say, `setup` against a sibling project. realpath
// resolves symlinks and ".." physically, so the sibling rule and the
// .projeny scan run on the directory the argument really names. Results are
// spelled the way the user spells paths: relative arguments come back as
// rel_to_cwd forms, absolute arguments stay absolute. Arguments that do NOT
// exist keep the lexical path so typo errors report the user's spelling.
std::string resolve_projeny_path(const std::string& arg, const char* cmd)
{
    std::string raw = strip_trailing_slashes(arg);
    if (raw.empty())
        raw = ".";
    bool absolute_arg = !raw.empty() && raw[0] == '/';
    // Spell a resolved absolute path the way the user spelled the argument.
    auto spell = [&](const std::string& abs_path) -> std::string {
        return absolute_arg ? abs_path : rel_to_cwd(abs_path);
    };
    // The lexical form is still used for the file/sibling checks and for
    // arguments that name nothing on disk.
    std::string a = normalize_lexical(raw);
    if (path_exists(raw)) {
        std::string phys = physical_path(raw);
        if (is_dir(phys)) {
            // Workdir-sibling rule, checked BEFORE the scan: a directory
            // sitting next to a "<dir>.projeny" file IS that project's
            // workdir — but only when the file says so: its Name: header
            // must name this directory (the checkout is always named by
            // Name:). This is what makes '.' and '..' resolve, and it wins
            // even when the directory happens to hold stray .projeny files
            // of its own.
            std::string sib_base = basename_of(phys);
            std::string sib_abs = join_path(dirname_of(phys),
                                            sib_base + ".projeny");
            if (path_exists(sib_abs) && !is_dir(sib_abs)) {
                std::string raw;
                ProjenyFile pf;
                if (try_read_file_bytes(sib_abs, &raw) &&
                    try_parse_projeny_softly(raw, "'" + spell(sib_abs) + "'",
                                             &pf) &&
                    pf.name != sib_base) {
                    die(std::string("cannot ") + cmd + " '" + arg +
                        "': '" + spell(sib_abs) +
                        "' names the checkout directory '" + pf.name +
                        "', not '" + sib_base + "'");
                }
                return spell(sib_abs);
            }
            std::vector<std::string> cands;
            for (const std::string& n : list_dir_names(phys)) {
                if (ends_with(n, ".projeny"))
                    cands.push_back(spell(join_path(phys, n)));
            }
            if (cands.size() == 1)
                return cands[0];
            if (cands.empty()) {
                die(std::string("cannot ") + cmd + " '" + arg +
                    "': directory holds no .projeny file (nor a '" +
                    spell(sib_abs) + "' sibling); name the .projeny file "
                    "explicitly");
            }
            die(std::string("cannot ") + cmd + " '" + arg +
                "': directory holds multiple .projeny files; name one "
                "explicitly",
                bullet_list(cands));
        }
    }
    if (ends_with(a, ".projeny"))
        return arg;
    std::string sib = a + ".projeny";
    if (path_exists(sib) && !is_dir(sib))
        return sib;
    return arg;
}

// ---- frozen-mtime support ----
//
// A frozen mtime pins a tracked file's checkout timestamp to what the
// archive wants it to be (so builds that would otherwise see a patched file
// as newer than its inputs — autoconf's "configure.ac is newer than
// local.mk" dance — keep skipping their up-to-date steps). The attribute is
// recorded in the .projeny patch as an extended header `frozen-mtime <ts>`
// (unix-epoch seconds) inside the file's `diff --git` block, right where the
// mode lines live, so it survives every patch regeneration. The invariant
// maintained by setup/commit/rebase: the stored value is always the
// archive's own member mtime for that file (refreshed from a fresh unpack),
// and after any setup/rebase the workdir copy is stamped to it.

// Stamp every frozen-mtime file named by `patch` (wid-label form) inside
// `workdir` to its recorded unix-epoch timestamp. Files missing from the
// workdir (deleted) or not regular files are skipped; a stat failure on a
// regular file dies. Called after setup/rebase put the workdir in place, so
// files the applier rewrote (which get a "now" timestamp from
// write_file_bytes) end up with the archive's mtime, like the tarball
// wanted.
void stamp_frozen_mtimes(const std::string& workdir, const std::string& patch,
                         const std::string& wid)
{
    std::map<std::string, uint64_t> frozen = vcs_frozen_mtimes(patch, wid);
    for (const auto& kv : frozen) {
        std::string full = join_path(workdir, kv.first);
        struct stat st;
        if (lstat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue; // gone (or replaced by something else): nothing to stamp
        struct timespec ts[2];
        ts[0].tv_sec = (time_t)kv.second;
        ts[0].tv_nsec = 0;
        ts[1] = ts[0];
        if (utimensat(AT_FDCWD, full.c_str(), ts, 0) != 0)
            die("cannot set the frozen mtime on '" + full + "': " +
                strerror(errno));
    }
}

// The frozen set recorded in `patch`, refreshed against `tree` (a freshly
// unpacked archive): each entry's value becomes that member's mtime, so the
// invariant "frozen value == archive member mtime" holds after commit and
// rebase regenerate the patch. Entries whose file is not in `tree` (a
// committed patch-added file, or one the new archive no longer ships) keep
// their previous value: there is no archive mtime to refresh from, and the
// applier will create the file before the stamp pass pins it.
std::map<std::string, uint64_t> refresh_frozen_from_tree(
    const std::map<std::string, uint64_t>& frozen, const std::string& tree)
{
    std::map<std::string, uint64_t> out = frozen;
    for (auto& kv : out) {
        std::string full = join_path(tree, kv.first);
        struct stat st;
        if (lstat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        kv.second = (uint64_t)st.st_mtim.tv_sec;
    }
    return out;
}

// True when workdir-relative `rel` is a tracked file of the project
// described by the current patch and status: present in the fresh base+patch
// tree, or a committed/pending add (or rename destination), and not
// pending-removed or a rename source. This is package's tracking rule; the
// frozen-mtime and get-attributes commands use it so a path that projeny
// does not manage dies with a clear name instead of being frozen/listed.
// True when `rel` is `base` itself or lives under it (keep entries may name
// directories, which then cover everything under them).
bool under_path(const std::string& rel, const std::string& base)
{
    return rel == base || starts_with(rel, base + "/");
}

// True when workdir-relative `rel` is tracked. One predicate for every
// family (freeze-mtime, get-attributes, package/extract staging); the
// precedence is fixed and documented:
//   1. removed, or under a removed entry -> false;
//   2. a pending rename source, or under one -> false (the path moved away);
//   3. a pending rename destination, or under one -> true;
//   4. pending-added, or under one -> true;
//   5. otherwise: whether the fresh base+patch tree holds it.
// The rename rules matter for the fresh-tree walk (a pending rename source
// still exists THERE until commit folds the op) and for files re-created at
// a pending rename source path (untracked: the lineage lives at the
// destination now). Untracked files (in neither tree nor any pending op)
// are left out of packages and attribute reports, like `git archive` leaves
// them out.
bool is_tracked_rel(const std::string& rel, const std::string& fresh_root,
                    const StatusData& st)
{
    for (auto& r : st.removed)
        if (under_path(rel, r))
            return false;
    for (auto& rn : st.renamed)
        if (under_path(rel, rn.first))
            return false;
    for (auto& rn : st.renamed)
        if (under_path(rel, rn.second))
            return true;
    for (auto& a : st.added)
        if (under_path(rel, a))
            return true;
    return path_exists(join_path(fresh_root, rel));
}

// The setup body, factored out so cmd_setup can run its frozen-mtime stamp
// pass after every setup flavor (fresh, adopt, re-setup, merge, conflicted
// recovery) has put the workdir in place. Takes the Ctx of the
// ALREADY-RESOLVED and absolutized .projeny path (see cmd_setup), which
// resolves the Ctx exactly once for the whole command.
int setup_impl(const Ctx& ctx);

} // namespace

int cmd_setup(const std::string& projeny_arg)
{
    // Resolve and ABSOLUTIZE before any tree work: a setup can remove the
    // directory the process's CWD sits in (`projeny setup ..` from a workdir
    // subdirectory deletes the whole workdir, CWD included), and every path
    // resolution after that point — including the stamp pass below — would
    // otherwise fail on a dead CWD.
    std::string pj = absolutize(resolve_projeny_path(projeny_arg, "setup"));
    Ctx ctx = resolve_ctx(pj);
    int rc = setup_impl(ctx);
    // setup must ALWAYS leave frozen-mtime files stamped (the whole point of
    // the attribute: whatever the checkout went through — fresh unpack,
    // adopt-into-existing, re-setup, merge, or conflicted recovery — the
    // file's mtime ends at what the archive wants). The stamp pass reads the
    // FINAL .projeny file (setup may have replaced it) and needs no archive
    // access: by the invariant above, the stored value is the archive's
    // member mtime.
    std::string raw;
    if (try_read_file_bytes(ctx.projeny_arg, &raw) &&
        !projeny_has_conflict_markers(raw)) {
        ProjenyFile cur =
            ProjenyFile::parse_bytes(raw, "'" + ctx.projeny_arg + "'");
        stamp_frozen_mtimes(join_path(ctx.pdir, cur.name), cur.patch, cur.name);
    }
    return rc;
}

namespace {

int setup_impl(const Ctx& ctx)
{
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
        // For a Projeny:-based file — the current one or the one the old
        // status recorded — the frozen base snapshot next to the .projeny
        // file is the same kind of checkout bookkeeping as the archive
        // snapshots and goes stale with the rest.
        std::vector<std::string> stale_extra;
        if (cur.is_projeny_based())
            stale_extra.push_back(projeny_snapshot_path_for(ctx.projeny_arg));
        {
            std::string old_status_raw;
            if (try_read_file_bytes(ctx.statusfile, &old_status_raw)) {
                bool old_projeny = false;
                std::string old_archive =
                    archive_from_status_bytes(old_status_raw, &old_projeny);
                if (old_projeny && stale_extra.empty())
                    stale_extra.push_back(
                        projeny_snapshot_path_for(ctx.projeny_arg));
                if (!old_archive.empty() && old_archive != cur.archive)
                    stale_archives.push_back(old_archive);
            }
        }
        // A URL: project's snapshot that the file's own hashes verify is
        // the archive this very setup wants (e.g. the bytes `create` just
        // wrote): keep it so the fresh setup below downloads nothing.
        std::set<std::string> url_hashes;
        for (const ProjenyUrl& u : cur.urls)
            url_hashes.insert(u.hash);
        disregard_stale_state(ctx, stale_archives,
                              url_hashes.empty() ? nullptr : &url_hashes,
                              stale_extra.empty() ? nullptr : &stale_extra);
        // A Projeny:-based project whose patch no longer applies onto the
        // (moved) chain extraction sets up WITH CONFLICT MARKERS here rather
        // than dying: the fresh checkout carries <<<<<<< current /
        // >>>>>>> patched regions the user resolves, then commits — the
        // second working recovery for a chain an upstream rebase moved.
        // Classic Archive:/URL: fresh setups keep the hard die
        // (build_tree_from_patch), byte-identically.
        std::vector<std::string> fresh_conflicts;
        do_fresh_setup(ctx, cur, &fresh_conflicts);
        StatusData sd;
        sd.status = "setup";
        sd.conflicts = fresh_conflicts;
        sort_unique(&sd.conflicts);
        sd.embedded = cur.raw;
        write_status(ctx, sd);
        if (!sd.conflicts.empty()) {
            printf("projeny: set up '%s' from '%s' with %zu conflict(s):\n",
                   rel_to_cwd(workdir).c_str(), setup_from_name(cur).c_str(),
                   sd.conflicts.size());
            for (auto& c : sd.conflicts)
                printf("  %s\n", c.c_str());
            printf("projeny: setup left conflicts (exit 1); fix them, then "
                   "`resolve` each file and `commit`\n");
            return 1;
        }
        printf("projeny: set up '%s' from '%s'\n", rel_to_cwd(workdir).c_str(),
               setup_from_name(cur).c_str());
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
        // Conflict-markered fresh setup for a Projeny:-based project whose
        // patch no longer applies (same recovery as above, into an
        // existing directory).
        std::vector<std::string> fresh_conflicts;
        do_fresh_setup_into_existing(ctx, cur, &fresh_conflicts);
        StatusData sd;
        sd.status = "setup";
        sd.conflicts = fresh_conflicts;
        sort_unique(&sd.conflicts);
        sd.embedded = cur.raw;
        write_status(ctx, sd);
        if (!sd.conflicts.empty()) {
            printf("projeny: set up '%s' from '%s' (into existing directory) "
                   "with %zu conflict(s):\n",
                   rel_to_cwd(workdir).c_str(), setup_from_name(cur).c_str(),
                   sd.conflicts.size());
            for (auto& c : sd.conflicts)
                printf("  %s\n", c.c_str());
            printf("projeny: setup left conflicts (exit 1); fix them, then "
                   "`resolve` each file and `commit`\n");
            return 1;
        }
        printf("projeny: set up '%s' from '%s' (into existing directory)\n",
               rel_to_cwd(workdir).c_str(), setup_from_name(cur).c_str());
        return 0;
    }

    StatusData old = StatusData::parse(ctx.statusfile);
    ProjenyFile oldpf =
        ProjenyFile::parse_bytes(old.embedded, "embedded copy in '" + ctx.statusfile + "'");
    std::string old_workdir = join_path(ctx.pdir, oldpf.name);

    // Reconstruct the expected tree E from the statusfile's copy. The
    // archive comes from the status file, so it may have been deleted from
    // git since (upstream rebase): read the snapshot copy when it exists.
    // A URL-based status copy verifies/re-fetches its snapshot instead (no
    // network while the snapshot still matches a URL: hash). A
    // Projeny:-based status copy extracts its base's committed content from
    // the FROZEN snapshot the status records (its snapshot when it exists,
    // else the live base file named by the embedded copy, frozen into it on
    // first use) plus the embedded patch — this runs BEFORE any adoption
    // below, so the reconstruction still sees the snapshot's old bytes when
    // the hand-edited header names a different base file.
    TempDir tE(make_scratch(ctx.pdir, ctx.filebase, "E"));
    TempDir tN(make_scratch(ctx.pdir, ctx.filebase, "N"));
    std::string Etree;
    if (oldpf.is_projeny_based()) {
        // Tolerant: when the frozen chain no longer reconstructs the tree
        // this status records (a patch no longer applies onto the moved
        // chain), the merge cannot know the base — die with the recovery
        // guidance instead of the bare apply report (rebase from the
        // workdir, or erase-setup --force + setup with conflict markers).
        try {
            TolerantPatchApply tol;
            Etree = build_projeny_tree(
                ctx.projeny_arg, oldpf,
                resolve_committed_base_bytes(
                    ctx.projeny_arg, oldpf,
                    "reconstruct the tree recorded by '" + ctx.statusfile +
                        "'"),
                oldpf.patch, "embedded patch in '" + ctx.statusfile + "'",
                tE.path,
                "reconstruct the tree recorded by '" + ctx.statusfile + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("set up", ctx.projeny_arg);
        }
    } else {
        Etree = build_tree_from_patch(
            tE, resolve_status_archive(ctx.pdir, oldpf,
                                       "reconstruct the tree recorded by '" +
                                           ctx.statusfile + "'"),
            oldpf.origname, oldpf.name, oldpf.patch,
            "embedded patch in '" + ctx.statusfile + "'");
    }

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

    // Classify U's blocks: which are real local changes, and how many are
    // just untracked files that will ride along. A pure addition whose path
    // the pending ops never registered (never `projeny add`ed, not a rename
    // destination) is untracked; everything else — a pending add, a
    // deletion, a rename, a modification — is a real local change.
    std::vector<std::string> add_keep;
    for (const auto& a : old.added)
        add_keep.push_back(a);
    for (const auto& rn : old.renamed)
        add_keep.push_back(rn.second);
    std::vector<FileDiff> ubs = split_file_diffs(U);
    size_t untracked = 0;
    bool have_changes = false;
    for (const auto& b : ubs) {
        if (block_is_pure_add(b) &&
            !vcs_covers_keep_path(add_keep, wid_rel(b.b_path, oldpf.name))) {
            ++untracked;
            continue;
        }
        have_changes = true;
        break;
    }

    if (ubs.empty()) {
        // No local changes at all (a pristine checkout of base+patch is
        // empty against itself, whether or not the patch is empty): plain
        // fresh setup from CURRENT .projeny. Keep pending add/rm/mv ops
        // (documented choice); previously recorded conflicts are unioned,
        // never silently dropped (like the conflicted-.projeny path
        // below). A Projeny:-based file whose own patch no longer applies
        // onto the adopted chain (a git pull repointed the base at another
        // .projeny whose committed changes collide with the patch) sets up
        // WITH CONFLICT MARKERS here — the same recovery as the
        // missing-workdir and missing-status fresh paths above: the
        // checkout carries <<<<<<< current / >>>>>>> patched regions the
        // user resolves, then commits. Classic Archive:/URL: files keep
        // the hard die, byte-identically.
        std::vector<std::string> fresh_conflicts;
        do_fresh_setup(ctx, cur, &fresh_conflicts);
        StatusData sd;
        sd.status = "setup";
        sd.added = old.added;
        sd.removed = old.removed;
        sd.renamed = old.renamed;
        sd.conflicts = fresh_conflicts;
        for (auto& c : old.conflicts) {
            if (std::find(sd.conflicts.begin(), sd.conflicts.end(), c) ==
                sd.conflicts.end())
                sd.conflicts.push_back(c);
        }
        sort_unique(&sd.conflicts);
        sd.embedded = cur.raw;
        write_status(ctx, sd);
        if (!fresh_conflicts.empty()) {
            printf("projeny: re-set up '%s' from '%s' (no local changes) "
                   "with %zu conflict(s):\n",
                   rel_to_cwd(workdir).c_str(), setup_from_name(cur).c_str(),
                   sd.conflicts.size());
            for (auto& c : sd.conflicts)
                printf("  %s\n", c.c_str());
            printf("projeny: setup left conflicts (exit 1); fix them, then "
                   "`resolve` each file and `commit`\n");
            return 1;
        }
        if (!sd.conflicts.empty()) {
            printf("projeny: re-set up '%s' from '%s' (no local changes) but "
                   "%zu conflict(s) are still unresolved:\n",
                   rel_to_cwd(workdir).c_str(), setup_from_name(cur).c_str(),
                   sd.conflicts.size());
            for (auto& c : sd.conflicts)
                printf("  %s\n", c.c_str());
            return 1;
        }
        printf("projeny: re-set up '%s' from '%s' (no local changes)\n",
               rel_to_cwd(workdir).c_str(), setup_from_name(cur).c_str());
        return 0;
    }

    // MERGE: fresh setup from current .projeny into a temp tree N, then apply
    // U onto N file-by-file, then move N into place. U carries the OLD wid
    // (oldpf.name); N carries the CURRENT wid (cur.name). A Projeny:-based
    // current file builds N from its frozen base's committed extraction,
    // under the status-aware law: a hand-edited Projeny: header naming a
    // different base file than the status records is ADOPTED here (after E
    // above, so E still reconstructed the old snapshot), and the newly
    // named base's live bytes overwrite the frozen snapshot and become N's
    // base — likewise a status-recorded base of ANOTHER mode (a
    // hand-conversion into the Projeny: form): without the adoption, the
    // lingering snapshot of the old era would silently supply a stale base.
    std::string Ntree;
    if (cur.is_projeny_based()) {
        StatusEmbeddedBase emb;
        emb.has = true;
        emb.projeny = oldpf.is_projeny_based();
        emb.base = oldpf.base;
        // Tolerant with the same recovery as the E build above: the
        // current patch failing onto the moved chain extraction is the
        // same frozen-chain-moved state.
        try {
            TolerantPatchApply tol;
            Ntree = build_setup_projeny_tree(
                ctx, cur, emb, cur.patch,
                "patch in '" + ctx.projeny_arg + "' (base '" + cur.base +
                    "')",
                tN.path, "set up '" + ctx.projeny_arg + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("set up", ctx.projeny_arg);
        }
    } else {
        Ntree = build_tree_from_patch(
            tN,
            materialize_archive(ctx.pdir, cur,
                                "set up '" + ctx.projeny_arg + "'"),
            cur.origname, cur.name, cur.patch,
            "patch in '" + ctx.projeny_arg + "'");
    }
    std::vector<std::string> conflicts;
    merge_user_diff_onto(Etree, Ntree, actual_workdir, Ntree, cur.name,
                         oldpf.name, U, ctx.pdir, ctx.filebase, &conflicts);
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
    if (!have_changes) {
        // The diff held only untracked files: the merge above carried them
        // into the new tree, but there was nothing to merge — say so
        // instead of claiming a merge.
        if (!sd.conflicts.empty()) {
            printf("projeny: re-set up '%s' from '%s' (no local changes; "
                   "kept %zu untracked file(s)) but %zu conflict(s) are "
                   "still unresolved:\n",
                   rel_to_cwd(workdir).c_str(), setup_from_name(cur).c_str(), untracked,
                   sd.conflicts.size());
            for (auto& c : sd.conflicts)
                printf("  %s\n", c.c_str());
            return 1;
        }
        printf("projeny: re-set up '%s' from '%s' (no local changes; kept "
               "%zu untracked file(s))\n",
               rel_to_cwd(workdir).c_str(), setup_from_name(cur).c_str(), untracked);
        return 0;
    }
    // Real local changes were merged: report what happened per file. Every
    // conflict in the final status list appears as a "conflict:" line even
    // when no diff block names it (e.g. one carried over from an earlier
    // setup). U was diffed without forced-rename pairing, so the pending
    // renames are handed in to keep a divergent mv reporting as renamed.
    std::vector<std::string> report =
        merge_report_lines(ubs, oldpf.name, &add_keep, sd.conflicts,
                           &old.renamed);
    printf("projeny: merged local changes onto '%s':\n",
           rel_to_cwd(workdir).c_str());
    for (const auto& l : report)
        printf("  %s\n", l.c_str());
    if (!sd.conflicts.empty()) {
        printf("projeny: setup left conflicts (exit 1); fix them, then "
               "`resolve` each file and `commit`\n");
        return 1;
    }
    return 0;
}

} // namespace

int cmd_commit(const std::string& projeny_arg)
{
    std::string pj = resolve_projeny_path(projeny_arg, "commit");
    Ctx ctx = resolve_ctx(pj);
    StatusData st = require_status_matches(ctx);
    require_no_conflicts(st, "cannot commit with unresolved conflicts");
    ProjenyFile cur =
        ProjenyFile::parse_bytes(st.embedded, "'" + ctx.projeny_arg + "'");
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then keep the hard
        // error — there is nothing to diff against.
        disregard_stale_state_for(ctx, cur, {cur.archive});
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

    TempDir tmp(make_scratch(ctx.pdir, ctx.filebase, "commit"));
    std::string base;
    if (cur.is_projeny_based()) {
        // bar's patch is relative to the base file's FULL committed
        // extraction, not to a raw tarball: diff against the extraction of
        // the status-frozen base (its snapshot, else the live base file
        // frozen into it — require_status_matches above guarantees the
        // embedded copy and the live file agree, so either names the same
        // base). No patch on top: the patch is what this commit
        // regenerates. The extraction tree's own file mtimes are the
        // frozen-mtime refresh source below (they stand in for the archive
        // members). Tolerant: a chain whose patch no longer applies (an
        // upstream link was rebased) cannot yield a diff base — die with
        // the recovery guidance instead of the bare apply report.
        try {
            TolerantPatchApply tol;
            base = build_projeny_tree(
                ctx.projeny_arg, cur,
                resolve_committed_base_bytes(
                    ctx.projeny_arg, cur, "commit '" + ctx.projeny_arg + "'"),
                "", "", tmp.path, "commit '" + ctx.projeny_arg + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("commit", ctx.projeny_arg);
        }
    } else {
        unpack_single_top(
            materialize_archive(ctx.pdir, cur, "commit '" + ctx.projeny_arg + "'"),
            tmp.path, cur.origname);
        base = join_path(tmp.path, cur.origname);
    }
    // Binary files travel in the patch as base64 blocks (add/delete/modify),
    // so tracked-binary changes and explicitly added binaries commit like
    // text. Untracked binaries (never committed, never `add`ed — like a
    // package tarball written into the workdir) are filtered back out of the
    // patch, the way git leaves untracked files out of commits.
    reconcile_binaries(workdir, base, false);
    // The differ returns canonical "a/<wid>/..." labels, so no second
    // canonicalization pass is needed (the old code double-wrapped
    // absolute paths and produced garbage labels). normalize_patch_text
    // already ends the patch with exactly one newline.
    // Commit filtering: only explicitly marked files change tracking state.
    //   - a file that disappeared without `projeny rm` (or a `projeny mv`
    //     source) is an error: refuse instead of silently deleting.
    //   - a new file that appeared without `projeny add` (or a `projeny mv`
    //     destination) is untracked: leave it out of the patch, like git.
    // Previously committed adds/deletes are tracked too: the diff is
    // regenerated from scratch on every commit, so without them a recommit
    // would silently drop every committed addition or re-error on every
    // committed deletion. Keep entries may name directories (add/rm take
    // dirs): files under them match via prefix.
    {
        std::vector<std::string> del_keep;
        for (const auto& r : st.removed)
            del_keep.push_back(r);
        for (const auto& rn : st.renamed)
            del_keep.push_back(rn.first);
        // The committed patch is projeny-generated (a/-prefixed), so no
        // target tree is needed to resolve its labels.
        for (const auto& p : vcs_deleted_paths(cur.patch, cur.name, "")) {
            if (std::find(del_keep.begin(), del_keep.end(), p) == del_keep.end())
                del_keep.push_back(p);
        }
        // Coverage for the disappeared check below (the shared rule lives
        // in vcs_delete_covered). Commit's counterpart probe is narrower
        // than the diff's: a moved file counts as covered only when its
        // counterpart exists in the workdir or was itself registered as
        // removed (`projeny rm` after the move records the moved-to path).
        // It deliberately does not consult the full keep list — a further
        // pending rename of the moved-to path leaves the moved-away file
        // unregistered, so the disappeared check still refuses instead of
        // folding the deletion into the move.
        auto covered = [&](const std::string& rel) -> bool {
            return vcs_delete_covered(del_keep, &st.renamed, rel,
                                      [&](const std::string& counterpart) {
                                          return path_exists(join_path(
                                                     workdir, counterpart)) ||
                                                 vcs_covers_keep_path(
                                                     st.removed, counterpart);
                                      });
        };
        // Authoritative disappeared check: walk the expected tree E (base
        // archive plus the current patch) and require every file missing
        // from the workdir to be covered above. This runs on the trees
        // themselves rather than the raw diff blocks, so content-based
        // rename pairing can never attribute a disappearance to the wrong
        // source when several same-content files changed hands at once.
        TempDir tmpE(make_scratch(ctx.pdir, ctx.filebase, "commit-exp"));
        std::string Etree;
        if (cur.is_projeny_based()) {
            // Expected tree for the disappeared check: the same frozen-base
            // extraction as the diff base above, plus the current patch.
            // Only the tree's PATHS feed the check (content is never
            // compared), so when the frozen chain no longer reconstructs a
            // clean base (an upstream link was rebased) the patch is
            // re-applied under conflict markers instead: the path set is
            // unchanged (failed blocks keep their files) and the commit
            // still works, while the merge base above already carried the
            // recovery.
            try {
                TolerantPatchApply tol;
                Etree = build_projeny_tree(
                    ctx.projeny_arg, cur,
                    resolve_committed_base_bytes(
                        ctx.projeny_arg, cur,
                        "commit '" + ctx.projeny_arg + "'"),
                    cur.patch,
                    "patch in '" + ctx.projeny_arg + "' (base '" + cur.base +
                        "')",
                    tmpE.path, "commit '" + ctx.projeny_arg + "'");
            } catch (const PatchApplyError&) {
                std::string bytes = resolve_committed_base_bytes(
                    ctx.projeny_arg, cur, "commit '" + ctx.projeny_arg + "'");
                std::vector<std::string> ignored;
                Etree = extract_projeny_base_tree(
                    ctx.projeny_arg, cur, bytes, tmpE.path,
                    "commit '" + ctx.projeny_arg + "'");
                if (!normalize_patch_text(cur.patch).empty())
                    vcs_apply_with_conflicts(Etree, cur.patch, cur.name,
                                             &ignored,
                                             VcsApplyMode::kFreshApply);
            }
        } else {
            Etree = build_tree_from_patch(
                tmpE,
                materialize_archive(ctx.pdir, cur,
                                    "commit '" + ctx.projeny_arg + "'"),
                cur.origname, cur.name, cur.patch,
                "patch in '" + ctx.projeny_arg + "'");
        }
        std::vector<std::string> disappeared;
        std::vector<std::string> stack;
        stack.push_back("");
        while (!stack.empty()) {
            std::string d = stack.back();
            stack.pop_back();
            std::string abs = d.empty() ? Etree : join_path(Etree, d);
            for (const std::string& name : list_dir_names(abs)) {
                std::string rel = d.empty() ? name : d + "/" + name;
                if (vcs_is_scratch_rel(rel))
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
            die("cannot commit with disappeared files (deleted on disk but not "
                "marked with 'projeny rm'; restore them or run 'projeny rm' "
                "first)",
                bullet_list(disappeared));
        }
    }
    std::vector<std::string> keep;
    for (const auto& a : st.added)
        keep.push_back(a);
    for (const auto& rn : st.renamed)
        keep.push_back(rn.second);
    // Previously committed adds are tracked too: the diff is
    // regenerated from scratch on every commit, so without them a
    // recommit would silently drop every committed addition.
    // The committed patch is projeny-generated (a/-prefixed), so no
    // target tree is needed to resolve its labels.
    for (const auto& p : vcs_add_paths(cur.patch, cur.name, "")) {
        if (std::find(keep.begin(), keep.end(), p) == keep.end())
            keep.push_back(p);
    }
    for (const auto& p : vcs_binary_add_paths(cur.patch, cur.name)) {
        if (std::find(keep.begin(), keep.end(), p) == keep.end())
            keep.push_back(p);
    }
    // The patch is generated through the same pending-aware differ that
    // `projeny diff <f.projeny>` prints, so what a diff shows is exactly
    // what commit stores. forced_renames makes `projeny mv` ALWAYS render
    // as a rename block — even when the moved file's content diverged
    // beyond the similarity threshold, and even when the moved file is
    // itself the product of an earlier committed rename (the pending
    // source resolves back through the committed patch to the original
    // archive path, which is what the base tree here — the raw archive —
    // still names). committed_patch supplies those committed renames.
    // add_keep leaves untracked adds out; delete_keep stays null because
    // the disappeared check above is the sole deletion authority: no
    // delete block is ever dropped here, and a resolved rename's delete
    // side may name an archive path that no pending op mentions.
    // frozen_mtimes keeps every frozen-mtime attribute in the regenerated
    // patch (attribute-only blocks for unchanged frozen files, the header
    // on modified ones), with values refreshed from this archive unpack so
    // the stored value always equals the archive's member mtime.
    std::map<std::string, uint64_t> frozen = refresh_frozen_from_tree(
        vcs_frozen_mtimes(cur.patch, cur.name), base);
    // A rename moves the file, so it must move the pin: the frozen map is
    // keyed by the path the user froze (the rename SOURCE), but diff blocks
    // carry the attribute on their live path (the DESTINATION — see
    // emit_block). Remap each pending rename's entry forward, in order so
    // chains (a->b, b->c) compose; the moved value overwrites any
    // destination entry, and the source entry dies with the path.
    for (const auto& rn : st.renamed) {
        auto it = frozen.find(rn.first);
        if (it == frozen.end())
            continue;
        uint64_t ts = it->second;
        frozen.erase(it);
        frozen[rn.second] = ts;
    }
    VcsDiffOpts dopts;
    dopts.forced_renames = &st.renamed;
    dopts.committed_patch = &cur.patch;
    dopts.add_keep = &keep;
    dopts.frozen_mtimes = &frozen;
    dopts.frozen_attribute_blocks = true;
    std::string raw_patch = vcs_diff_trees_ex(base, workdir, cur.name, dopts);
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
    std::string pj = resolve_projeny_path(projeny_arg, "add");
    Ctx ctx = resolve_ctx(pj);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error —
        // there is no workdir to add anything to.
        disregard_stale_state_for(ctx, cur, {cur.archive});
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
    // Absolutize before any workdir mutation (like cmd_setup): this command
    // can move/delete the directory the process's CWD sits in, and every
    // later path — the status file included — must not need the CWD again.
    std::string pj = absolutize(resolve_projeny_path(projeny_arg, "rm"));
    Ctx ctx = resolve_ctx(pj);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error —
        // there is no workdir to remove anything from.
        disregard_stale_state_for(ctx, cur, {cur.archive});
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
    // Absolutize before any workdir mutation (like cmd_setup): the move
    // itself can relocate the directory the process's CWD sits in, which
    // would silently re-point every relative path — the status file write
    // included — so the rename would be applied but never recorded.
    std::string pj = absolutize(resolve_projeny_path(projeny_arg, "mv"));
    Ctx ctx = resolve_ctx(pj);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error —
        // there is no workdir to rename anything in.
        disregard_stale_state_for(ctx, cur, {cur.archive});
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
    std::string pj = resolve_projeny_path(projeny_arg, "resolve");
    Ctx ctx = resolve_ctx(pj);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error — a
        // conflict list about files that no longer exist is meaningless.
        disregard_stale_state_for(ctx, cur, {cur.archive});
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
        // The precise marker grammar (marker at column 0, the full seven
        // characters): a line like "=======x" is content, not a marker.
        if (projeny_has_conflict_markers(content))
            warn("'" + rel + "' still contains conflict markers");
    }
    write_status(ctx, st);
    printf("projeny: resolved '%s'\n", rel.c_str());
    return 0;
}

// Replay recorded pending ops (adds, renames, removals) onto `root` so it
// carries the workdir's pending intent: the replayed result is what the
// workdir should hold given base+patch+pending, so a diff against it
// exposes exactly the uncommitted residue. Dies when a pending add/rename's
// workdir file has vanished mid-flight (`why` names what the command was
// about to do). The workdir itself is never mutated.
void replay_pending_ops(const StatusData& st, const std::string& workdir,
                        const std::string& root, const char* why)
{
    for (const auto& a : st.added) {
        std::string wf = join_path(workdir, a);
        if (!path_exists(wf))
            die(std::string("pending add '") + a + "' vanished from '" +
                workdir + "'; cannot " + why);
        make_dirs(dirname_of(join_path(root, a)));
        copy_path_preserving(wf, join_path(root, a));
    }
    for (const auto& rn : st.renamed) {
        std::string wf = join_path(workdir, rn.second);
        if (!path_exists(wf))
            die("pending rename '" + rn.first + " -> " + rn.second +
                "' vanished from '" + workdir + "'; cannot " + why);
        remove_recursive(join_path(root, rn.first));
        make_dirs(dirname_of(join_path(root, rn.second)));
        copy_path_preserving(wf, join_path(root, rn.second));
    }
    for (const auto& r : st.removed)
        remove_recursive(join_path(root, r));
}

namespace {

// The new-archive argument grammar shared by `rebase` and `create`: the
// FIRST argument decides the mode. A string curl's URL parser accepts (see
// arg_is_url) starts a URL: header list — one or more <url> [<blake3-hash>]
// pairs. Anything else is the legacy form: exactly one local tarball path.
// Every message words the command's own name, so rebase and create refuse
// identical shapes identically.
// The base-argument grammar shared by `rebase` and `create`: the FIRST
// argument decides the mode. A name ending in ".projeny" selects the
// projeny-base form (checked BEFORE the URL test — a URL can end in
// ".projeny" too, and the projeny form wins): exactly one such argument,
// with no hash pairs after it. Otherwise a string libcurl's URL parser
// accepts (http://, https://, ftp://, ftps://, file://) starts URL mode,
// and anything else is exactly one local tarball path. An EMPTY argument
// list is the zero-arg form, which only `rebase` has (allow_empty: a
// Projeny:-based project re-extracting its committed base — the caller
// decides whether the project's mode allows it). The projeny form is
// only parsed here: *projeny_base receives the RAW argument, and the
// callers validate it against the target file (existence, the
// same-directory law, the parse, and the live-chain cycle pre-check — see
// validate_projeny_base_arg) once they know the target's path.
void parse_archive_args(const std::vector<std::string>& archive_args,
                        std::vector<ProjenyUrl>* urls, std::string* tarball,
                        const char* cmd, std::string* projeny_base,
                        bool allow_empty)
{
    urls->clear();
    tarball->clear();
    projeny_base->clear();
    if (archive_args.empty()) {
        // The zero-arg form: `rebase` of a Projeny:-based project (the
        // mode check lives with the caller's project parse). `create`
        // has no zero-arg form: it must name a base.
        if (!allow_empty)
            die(std::string(cmd) +
                " takes exactly one new tarball path (or URL(s): 'projeny " +
                cmd + " <f.projeny> <url> [<blake3-hash>] [<url> "
                      "[<blake3-hash>]...])'");
        return;
    }
    if (ends_with(archive_args[0], ".projeny")) {
        if (archive_args.size() != 1)
            die(std::string(cmd) +
                " takes exactly one .projeny base file (no hash pairs); '" +
                archive_args[1] + "' is extra");
        *projeny_base = archive_args[0];
        return;
    }
    bool url_mode = arg_is_url(archive_args[0]);
    if (!url_mode) {
        if (archive_args.size() != 1)
            die(std::string(cmd) +
                " takes exactly one new tarball path (or URL(s): 'projeny " +
                cmd + " " +
                (std::string(cmd) == "create" ? "<f.projeny>"
                                              : "<f.projeny|dir>") +
                " <url> [<blake3-hash>] [<url> [<blake3-hash>]...])");
        *tarball = archive_args[0];
        return;
    }
    for (size_t i = 0; i < archive_args.size(); ++i) {
        const std::string& a = archive_args[i];
        // The first argument's URL-ness is already settled — it chose
        // URL mode above — so only the later arguments pay for a parse.
        if (i == 0 || arg_is_url(a)) {
            ProjenyUrl u;
            u.url = a;
            urls->push_back(u);
            continue;
        }
        // A non-URL argument is the blake3 hash of the URL before it:
        // the same 64-hex rule a .projeny file's "URL: <url> <hash>"
        // header and `projeny download` apply, normalized to lowercase.
        if (urls->empty() || !urls->back().hash.empty())
            die(std::string(cmd) +
                " URL arguments come as <url> [<blake3-hash>] pairs; '" + a +
                "' does not follow a hash-less URL");
        std::string lower;
        bool ok = a.size() == 64;
        for (char c : a) {
            if (!isxdigit(static_cast<unsigned char>(c)))
                ok = false;
            lower.push_back(
                static_cast<char>(tolower(static_cast<unsigned char>(c))));
        }
        if (!ok)
            die("invalid blake3 hash '" + a + "' for " + urls->back().url);
        urls->back().hash = lower;
    }
}

// Download and verify EVERY listed URL argument (the parse_archive_args
// URL form), shared by rebase and create: each distinct URL downloads
// once, a hashless URL's hash is computed from its download and reported,
// an asserted hash that mismatches is a hard error, and a hashed URL that
// fails to download only warns (the mirror stays listed). On return every
// entry carries a hash, *verified_data holds the FIRST verified download's
// bytes, and *new_base is the first URL's archive basename (a URL that
// does not name a file dies here, mirroring the .projeny parse's rule).
// Every message is byte-identical to the inline loop this extracted, which
// the test suite asserts.
void verify_urls(std::vector<ProjenyUrl>* urls, std::string* verified_data,
                 std::string* new_base, const char* cmd,
                 const std::string& what)
{
    *new_base = archive_name_from_url((*urls)[0].url);
    if (new_base->empty())
        die("URL '" + (*urls)[0].url + "' does not name a file");
    std::map<std::string, std::pair<bool, std::string>> fetched;
    bool have_verified = false;
    for (ProjenyUrl& u : *urls) {
        bool ok;
        std::string data, err;
        auto it = fetched.find(u.url);
        if (it == fetched.end()) {
            // try_download announces the attempt itself ("projeny:
            // downloading '<url>'", then '\r'-terminated progress).
            ok = try_download(u.url, &data, &err);
            fetched[u.url] = {ok, ok ? data : err};
        } else {
            ok = it->second.first;
            if (ok)
                data = it->second.second;
            else
                err = it->second.second;
        }
        if (!ok) {
            if (u.hash.empty())
                // Without a hash there is nothing to record: a URL:
                // header always carries one, so this is a hard error.
                die("cannot compute the blake3 hash of '" + u.url +
                    "': " + err +
                    "; pass the hash explicitly after the URL "
                    "('projeny hash <file>' computes it) — a URL: "
                    "header always carries a hash");
            // A hashed mirror that fails to download only warns: the
            // line stays listed in the .projeny file, and a later
            // setup falls through it exactly the same way.
            warn("could not download '" + u.url + "': " + err +
                 "; trying the next URL");
            continue;
        }
        std::string have = blake3_hash_hex(data);
        if (!u.hash.empty() && have != u.hash)
            // The user asserted this hash: a mismatch is a hard error,
            // not a mirror fall-through — a bad pair must not be
            // silently written into the file.
            die("downloaded '" + u.url + "' but its blake3 hash is " +
                have + ", expected " + u.hash +
                "; the hash was given explicitly, so projeny stops "
                "instead of writing a mismatched URL: header");
        if (u.hash.empty()) {
            u.hash = have;
            note("computed blake3 hash " + have + " for '" + u.url + "'");
        }
        if (!have_verified) {
            // The FIRST verified entry's bytes are the new tarball
            // (the snapshot is named after the first URL regardless).
            have_verified = true;
            *verified_data = data;
        }
    }
    if (!have_verified) {
        // The refusal words the command's own action: "not rebasing"
        // for rebase, "not creating" for create (the gerund of both
        // command names drops a trailing 'e').
        std::string doing = cmd;
        if (!doing.empty() && doing.back() == 'e')
            doing.pop_back();
        doing += "ing";
        die("no URL argument produced a verified download; not " + doing +
            " '" + what + "' (nothing was changed)");
    }
}

} // namespace

// ---- reverse patch application (rebase's workdir fallback) ----
//
// When a Projeny: chain's frozen snapshots no longer reconstruct the base a
// project's patch was diffed against (an upstream link was rebased), the
// only local record of that base is the workdir itself: workdir = base +
// patch. Reversing the patch onto a copy of the workdir therefore
// reconstructs the base the merge engine needs. The transform below is a
// line-level swap of every block's old/new sides: the diff --git labels,
// the ---/+++ labels (with /dev/null sides and git-quoted paths), the @@
// hunk ranges, the -/+ hunk body lines (the \ No newline markers stay in
// place and flip sides with their lines), old/new mode, new/deleted file
// mode, rename from/to, and similarity/index/frozen-mtime pass through.

// Reverse one FileDiff block. Returns the block's reversed text, or "" when
// the block cannot be reversed safely (with *unreversed carrying the
// block's touched workdir-relative paths): a GIT binary patch literal (the
// one-directional payload the internal format defines), a combined-diff
// block (opaque), a payload-free binary stanza, or any block whose labels
// or hunk headers do not parse. Unreversible blocks are simply left out of
// the reversed patch text — the caller keeps those files at their workdir
// bytes and reconciles them after the merge (see cmd_rebase).
std::string reverse_file_diff(const FileDiff& b, const std::string& wid,
                              const std::string& old_tree,
                              std::vector<std::string>* unreversed)
{
    // Every failure exit funnels here: the block stays OUT of the reversed
    // text and its touched paths are recorded so the caller keeps those
    // files at their workdir bytes and reconciles them after the merge —
    // an unrecorded failure would silently lose the file's committed
    // change (the merge would see base == workdir content and take the
    // upstream side wholesale).
    std::string old_rel = wid_rel(b.a_path, wid);
    std::string new_rel = wid_rel(b.b_path, wid);
    auto fail = [&]() -> std::string {
        if (!old_rel.empty())
            unreversed->push_back(old_rel);
        if (!new_rel.empty())
            unreversed->push_back(new_rel);
        return "";
    };

    // Split into lines (block text ends with '\n').
    std::vector<std::string> lines;
    {
        size_t pos = 0;
        while (pos < b.text.size()) {
            size_t nl = b.text.find('\n', pos);
            if (nl == std::string::npos) {
                lines.push_back(b.text.substr(pos));
                break;
            }
            lines.push_back(b.text.substr(pos, nl - pos));
            pos = nl + 1;
        }
    }
    if (lines.empty())
        return fail();

    // Combined diffs and blocks without a git header are opaque.
    if (b.text.find("diff --git ") != 0)
        return fail();

    // The header line's two labels, parsed like the applier's: a quoted
    // first token (git C-style) or an unquoted " b/" split.
    std::string la, lb;
    {
        std::string rest = lines[0].substr(11); // after "diff --git "
        auto qend = [](const std::string& s, size_t i) -> size_t {
            size_t j = i + 1;
            while (j < s.size()) {
                if (s[j] == '\\') {
                    j += 2;
                    continue;
                }
                if (s[j] == '"')
                    return j;
                ++j;
            }
            return std::string::npos;
        };
        if (!rest.empty() && rest[0] == '"') {
            size_t q = qend(rest, 0);
            if (q == std::string::npos || q + 1 >= rest.size() ||
                rest[q + 1] != ' ')
                return fail();
            la = rest.substr(0, q + 1);
            lb = rest.substr(q + 2);
        } else {
            size_t pos = rest.find(" b/");
            if (pos == std::string::npos)
                return fail();
            la = rest.substr(0, pos);
            lb = rest.substr(pos + 1);
        }
    }
    std::string ua = unquote_git_path(la);
    std::string ub = unquote_git_path(lb);
    // The differ's own labels: a/<wid>/<rel> and b/<wid>/<rel> (both live
    // paths for adds/deletes, like git). Anything else is foreign.
    if (ua.compare(0, 2, "a/") != 0 || ub.compare(0, 2, "b/") != 0)
        return fail();
    old_rel = wid_rel(ua.substr(2), wid);
    new_rel = wid_rel(ub.substr(2), wid);

    // A payload-bearing binary block cannot be reversed (the literal
    // sections are one-directional per the internal format): the caller
    // keeps the file at its workdir bytes. Payload-free blocks (pure
    // renames, mode-only changes) are header-only and reverse fine.
    if (b.text.find("GIT binary patch") != std::string::npos ||
        b.text.find("Binary files ") != std::string::npos) {
        return fail();
    }

    std::string out;
    out += "diff --git " + quote_git_path("a/" + wid + "/" + new_rel) + " " +
           quote_git_path("b/" + wid + "/" + old_rel) + "\n";
    // Emit an already-key-swapped line, restoring git's canonical pair
    // order: the transform turns (old mode/new mode) into (new mode/old
    // mode) and (rename from/rename to) into (rename to/rename from), so a
    // line whose transformed key is the pair's SECOND member swaps with the
    // preceding counterpart line.
    auto emit_swapped = [&](const std::string& transformed) {
        if (out.size() >= 2 && out.back() == '\n') {
            size_t p1 = out.rfind('\n', out.size() - 2);
            if (p1 != std::string::npos) {
                std::string prev = out.substr(p1 + 1); // includes its '\n'
                if ((prev.compare(0, 9, "new mode ") == 0 &&
                     transformed.compare(0, 9, "old mode ") == 0) ||
                    (prev.compare(0, 10, "rename to ") == 0 &&
                     transformed.compare(0, 12, "rename from ") == 0)) {
                    out.resize(p1 + 1);
                    out += transformed + "\n";
                    out += prev;
                    return;
                }
            }
        }
        out += transformed + "\n";
    };
    // Strictness gate state: the reversed block's PRE-IMAGE items (context
    // and added lines of the ORIGINAL hunk bodies, in order) must exist
    // verbatim in the workdir file for the reversal to be entrusted to the
    // applier. The applier tolerates slid and fuzzed matches (by design, up
    // to two context lines each side), which here would bake the workdir's
    // uncommitted edits into the reconstructed base — and then silently
    // drop them in the merge (base == theirs at the edited line lets the
    // upstream side win). A block whose pre-image does not strictly match
    // stays unreversed instead: the post-merge fixup keeps the workdir's
    // bytes for it and flags the conflict.
    // The gate applies to MODIFY-shaped blocks only (both --- and +++ sides
    // live): the reversed application then must match a pre-image inside an
    // existing workdir file — the one at the block's NEW-side path (for a
    // rename-with-content that is where the post-rename content lives, and
    // for a plain modify it is the same file). Pure adds (reversed: deletes)
    // and pure deletes (reversed: re-creations of the file the committed
    // patch removed — exactly the base content the merge needs) carry no
    // pre-image to verify.
    bool saw_minus_live = false;
    bool saw_plus_live = false;
    std::vector<std::vector<std::string>> gate_hunks;
    std::vector<std::string> gate_cur;
    bool in_hunk = false;
    for (size_t i = 1; i < lines.size(); ++i) {
        const std::string& l = lines[i];
        if (l.compare(0, 10, "old mode ") == 0) {
            out += "new mode " + l.substr(9) + "\n";
            continue;
        }
        if (l.compare(0, 10, "new mode ") == 0) {
            emit_swapped("old mode " + l.substr(9));
            continue;
        }
        if (l.compare(0, 14, "new file mode ") == 0) {
            out += "deleted file mode " + l.substr(14) + "\n";
            continue;
        }
        if (l.compare(0, 17, "deleted file mode ") == 0) {
            out += "new file mode " + l.substr(17) + "\n";
            continue;
        }
        if (l.compare(0, 12, "rename from ") == 0) {
            out += "rename to " + l.substr(12) + "\n";
            continue;
        }
        if (l.compare(0, 10, "rename to ") == 0) {
            emit_swapped("rename from " + l.substr(10));
            continue;
        }
        if (l.compare(0, 17, "similarity index ") == 0 ||
            l.compare(0, 21, "dissimilarity index ") == 0 ||
            l.compare(0, 6, "index ") == 0 ||
            l.compare(0, 13, "frozen-mtime ") == 0) {
            out += l + "\n";
            continue;
        }
        if (l.compare(0, 4, "--- ") == 0) {
            // The original --- line is the reversed block's +++ side: the
            // reversed NEW side is the original OLD side. A trailing
            // tab-stamp rides with the side it decorated (the applier
            // strips stamps, so this is cosmetic either way).
            std::string rest = l.substr(4);
            size_t tab = rest.find('\t');
            std::string stamp = tab == std::string::npos ? "" : rest.substr(tab);
            std::string body = tab == std::string::npos ? rest : rest.substr(0, tab);
            if (body == "/dev/null") {
                // The original was a pure add: the reversal deletes.
                out += "+++ /dev/null" + stamp + "\n";
                continue;
            }
            saw_minus_live = true;
            std::string u = unquote_git_path(body);
            if (u.compare(0, 2, "a/") != 0)
                return fail();
            out += "+++ " + quote_git_path("b/" + u.substr(2)) + stamp + "\n";
            continue;
        }
        if (l.compare(0, 4, "+++ ") == 0) {
            // The original +++ line is the reversed block's --- side: the
            // reversed OLD side is the original NEW side.
            std::string rest = l.substr(4);
            size_t tab = rest.find('\t');
            std::string stamp = tab == std::string::npos ? "" : rest.substr(tab);
            std::string body = tab == std::string::npos ? rest : rest.substr(0, tab);
            if (body == "/dev/null") {
                out += "--- /dev/null" + stamp + "\n";
                continue;
            }
            saw_plus_live = true;
            std::string u = unquote_git_path(body);
            if (u.compare(0, 2, "b/") != 0)
                return fail();
            out += "--- " + quote_git_path("a/" + u.substr(2)) + stamp + "\n";
            continue;
        }
        if (l.compare(0, 4, "@@ -") == 0) {
            if (in_hunk && !gate_cur.empty())
                gate_hunks.push_back(gate_cur);
            gate_cur.clear();
            // "@@ -os[,oc] +ns[,nc] @@ section" -> swap the two ranges.
            size_t sp1 = l.find(' ', 4);
            if (sp1 == std::string::npos)
                return fail();
            std::string r1 = l.substr(4, sp1 - 4); // "-os[,oc]"
            size_t sp2 = l.find(' ', sp1 + 1);
            if (sp2 == std::string::npos)
                return fail();
            std::string r2 = l.substr(sp1 + 1, sp2 - sp1 - 1); // "+ns[,nc]"
            if (r2.size() < 2 || r2[0] != '+')
                return fail();
            in_hunk = true;
            // Reversed old range = the original new range, reversed new
            // range = the original old range.
            out += "@@ -" + r2.substr(1) + " +" + r1.substr(1) + " " +
                   l.substr(sp2 + 1) + "\n";
            continue;
        }
        if (in_hunk) {
            if (l.empty()) {
                out += "\n"; // blank context line
                gate_cur.push_back("");
                continue;
            }
            char c = l[0];
            if (c == ' ') {
                out += l + "\n";
                gate_cur.push_back(l.substr(1));
                continue;
            }
            if (c == '\\') {
                out += l + "\n";
                continue;
            }
            if (c == '-') {
                out += "+" + l.substr(1) + "\n";
                continue;
            }
            if (c == '+') {
                out += "-" + l.substr(1) + "\n";
                gate_cur.push_back(l.substr(1));
                continue;
            }
            return fail(); // junk inside a hunk: refuse the block
        }
        // Anything else between the header and the hunks (or trailing
        // after them): refuse rather than emit a block the applier would
        // misread.
        return fail();
    }
    if (in_hunk && !gate_cur.empty())
        gate_hunks.push_back(gate_cur);
    if (saw_minus_live && saw_plus_live && !gate_hunks.empty()) {
        // The strict gate: every hunk's reversed pre-image (the original
        // block's context + added lines, in order) must appear verbatim in
        // the workdir file the reversal reads. No fuzz, no sliding past a
        // mismatch: any miss leaves the block unreversed (fail above
        // records the paths), so the fixup keeps the workdir's bytes.
        std::string content;
        if (!try_read_file_bytes(join_path(old_tree, new_rel), &content))
            return fail();
        std::vector<std::string> file_lines;
        {
            size_t pos = 0;
            while (pos < content.size()) {
                size_t nl = content.find('\n', pos);
                if (nl == std::string::npos) {
                    file_lines.push_back(content.substr(pos));
                    break;
                }
                file_lines.push_back(content.substr(pos, nl - pos));
                pos = nl + 1;
            }
        }
        for (const auto& items : gate_hunks) {
            if (items.empty())
                continue;
            bool found = false;
            if (items.size() <= file_lines.size()) {
                for (size_t p = 0; p + items.size() <= file_lines.size();
                     ++p) {
                    bool all = true;
                    for (size_t k = 0; k < items.size(); ++k) {
                        if (file_lines[p + k] != items[k]) {
                            all = false;
                            break;
                        }
                    }
                    if (all) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
                return fail();
        }
    }
    return out;
}

// Reverse `patch` (wid-label form): every block's old/new sides are swapped
// so applying the result to a base+patch tree reconstructs the base (see
// reverse_file_diff). Blocks that cannot be reversed safely are LEFT OUT
// and their touched workdir-relative paths are appended to *unreversed
// (the caller sorts/uniques it); the reversed result is normalized like
// every generated patch.
std::string reverse_patch_text(const std::string& patch, const std::string& wid,
                               const std::string& old_tree,
                               std::vector<std::string>* unreversed)
{
    std::string out;
    for (const FileDiff& b : split_file_diffs(patch)) {
        std::string rb = reverse_file_diff(b, wid, old_tree, unreversed);
        if (!rb.empty())
            out += rb;
    }
    return normalize_patch_text(out);
}

namespace {

// Whether the entry at workdir-relative path `rel` differs between the two
// trees in any way the tree differ can see: presence, regular-vs-symlink
// type, symlink target, exec bit (the differ's 100644/100755/120000 mode
// triple), or content bytes. Two directories compare equal (a patch block
// never names a directory as content; any file under it is its own touched
// path), and a special file never matches anything but itself. This is the
// state-(i) discriminator's atom: it compares the fallback's candidate old
// side against the frozen chain's patch-free extraction restricted to the
// files the embedded patch touches (see cmd_rebase).
bool touched_entry_differs(const std::string& tree_a, const std::string& tree_b,
                           const std::string& rel)
{
    std::string fa = join_path(tree_a, rel);
    std::string fb = join_path(tree_b, rel);
    struct stat sa, sb;
    bool ea = lstat(fa.c_str(), &sa) == 0;
    bool eb = lstat(fb.c_str(), &sb) == 0;
    if (ea != eb)
        return true; // present on one side only
    if (!ea)
        return false; // absent from both sides
    if (S_ISDIR(sa.st_mode) && S_ISDIR(sb.st_mode))
        return false; // both directories: nothing to compare here
    bool la = S_ISLNK(sa.st_mode) != 0;
    bool lb = S_ISLNK(sb.st_mode) != 0;
    if (la != lb)
        return true; // typechange
    if (la)
        return read_link_target(fa) != read_link_target(fb);
    if (!S_ISREG(sa.st_mode) || !S_ISREG(sb.st_mode))
        return true; // a special file matches no regular file
    if (((sa.st_mode & 0111) != 0) != ((sb.st_mode & 0111) != 0))
        return true; // exec-bit difference
    return !files_equal(fa, fb);
}

} // namespace

int cmd_rebase(const std::string& projeny_arg,
               const std::vector<std::string>& new_archive_args)
{
    // Argument parsing, before anything is touched: see
    // parse_archive_args — the FIRST argument decides the mode, URL list,
    // exactly one local tarball path, or exactly one .projeny base file.
    // An EMPTY argument list is the zero-arg form: valid only for a
    // Projeny:-based project (re-extract its committed base), refused
    // below once the project file's own parse is in hand.
    std::vector<ProjenyUrl> urls;
    std::string new_tarball;
    std::string projeny_base; // .projeny form: the RAW base argument
    parse_archive_args(new_archive_args, &urls, &new_tarball, "rebase",
                       &projeny_base, /* zero-arg form allowed */ true);
    bool url_mode = !urls.empty();
    bool projeny_mode = !projeny_base.empty();
    bool zero_arg_mode = new_archive_args.empty();

    // Absolutize up front: this command replaces the workdir, which can
    // delete the directory the process's CWD sits in (`projeny rebase ..`
    // from a workdir subdirectory); every later path must not need the
    // CWD again.
    std::string pj = absolutize(resolve_projeny_path(projeny_arg, "rebase"));
    Ctx ctx = resolve_ctx(pj);

    // The tarball form is refused for a URL-based project BEFORE the
    // setup-first fallback below: that fallback runs a full setup, which for
    // a URL project means downloading the archive — a refusal must never
    // cost a download, and must never create the checkout it refuses to
    // serve. The on-disk file decides: when the status file exists,
    // require_status_matches below enforces that it is byte-identical to the
    // status copy this same file would be re-parsed from.
    ProjenyFile cur0 = ProjenyFile::parse(ctx.projeny_arg);
    if (zero_arg_mode) {
        if (!cur0.is_projeny_based())
            die("cannot rebase '" + ctx.projeny_arg +
                "' without a new base argument: it is not a Projeny-based "
                "project (pass a tarball path or URL(s), or, for a "
                "Projeny-based project, no argument at all to re-extract "
                "its committed base)");
    } else if (cur0.is_url_based() && !url_mode)
        die("cannot rebase '" + ctx.projeny_arg +
            "' with a tarball path: it is a URL-based project (URL: headers; "
            "no Archive: tarball is checked into git); rebase it with URL(s) "
            "instead: projeny rebase " + ctx.projeny_arg +
            " <url> [<blake3-hash>] ... (when the hash is omitted, projeny "
            "downloads the tarball and computes the hash; 'projeny hash "
            "<file>' computes it for a local file)");

    // The .projeny base form validates against the target file before
    // anything is touched: the same-directory law, the base's existence and
    // parse, and the live-chain cycle pre-check (a refusal must never cost
    // a setup, let alone a merge).
    ProjenyFile base_pf;
    if (projeny_mode)
        projeny_base = validate_projeny_base_arg(projeny_base,
                                                 ctx.projeny_arg, "rebase",
                                                 &base_pf);

    // If status/workdir are missing, do a setup first.
    std::string workdir0 = join_path(ctx.pdir, cur0.name);
    if (!path_exists(ctx.statusfile) || !is_dir(workdir0)) {
        printf("projeny: no setup yet; running setup first\n");
        cmd_setup(pj);
    }

    StatusData st = require_status_matches(ctx);
    ProjenyFile cur =
        ProjenyFile::parse_bytes(st.embedded, "'" + ctx.projeny_arg + "'");
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir))
        die("workdir '" + workdir + "' is missing; run setup first");

    // Require a clean tree: the workdir must match the expected tree — i.e.
    // diffing it against base+patch using the stored patch's wid must come
    // back empty. Also refuse when conflicts are pending: conflict markers
    // in the tree would otherwise be diffed as content.
    // Pending add/rm/mv ops are honored: the tree is "clean" when it matches
    // base+patch modulo exactly the recorded pending ops (verified below by
    // replaying the ops onto a fresh base+patch tree and diffing).
    //
    // FROZEN-CHAIN FALLBACK: when the Projeny:-based expected-tree build
    // fails because a patch no longer applies onto the chain-composed
    // extraction (an upstream link was rebased and the frozen snapshots now
    // walk into the new content — every consumer of this tree bricks the
    // same way), and the workdir is intact, the rebase proceeds FROM THE
    // WORKDIR AS-IS instead of refusing: the old merge base is reconstructed
    // by reverse-applying the embedded patch onto a copy of the workdir (see
    // reverse_patch_text), the merge engine runs unchanged against it, and
    // the uncommitted-changes check is intentionally skipped (local changes
    // ride along — that is what "from the workdir as-is" means; the note
    // below covers it). The preservation boundary that follows from the
    // reconstruction: uncommitted edits INSIDE a hunk pre-image make the
    // reverse gate refuse, and the post-merge fixup keeps that file's
    // workdir bytes wholesale; edits OUTSIDE the pre-images — on touched or
    // untouched files alike — are NOT distinguishable from stale committed
    // content (the true old base is unreconstructable there), so the moved
    // chain's content stands and the file is flagged as a conflict for the
    // user to inspect (see vcs_detect_displaced_regions and the passes
    // below). A build failure with NO
    // workdir keeps the bare behavior: the setup-first fallback above has
    // already tried to make the checkout, and it either died (nothing to
    // rebase) or recorded conflicts (refused above).
    //
    // The fallback has a SECOND trigger: the expected tree may BUILD while
    // the workdir still mismatches it (state (i) — the moved chain does not
    // collide with the patch, so the mismatch is the upstream delta itself
    // rather than a broken hunk). Refusing there would be wrong twice over
    // — the decision tree at the mismatch site below explains — so the
    // mismatch is classified first: when the fallback's candidate old side
    // differs from the frozen chain's patch-free extraction on ANY file the
    // patch touches, the recorded base moved under the patch and the
    // fallback engages (identical machinery, so a clean workdir converges
    // exactly where erase-setup --force + setup would have converged,
    // without destroying anything); when every touched file matches, the
    // mismatch lies entirely outside the patch's file set and the classic
    // refusal stands (reworded, since neither half of the old advice held).
    require_no_conflicts(
        st, "workdir '" + workdir +
                "' has unresolved conflicts; resolve them before rebasing");
    bool workdir_fallback = false;
    std::unique_ptr<TempDir> tFallback; // owns the fallback old tree
    std::string fallback_old_tree;
    std::vector<std::string> reverse_failed;
    // The workdir-as-is note, printed once per fallback engagement (both
    // triggers — the failed expected-tree build below, and the state-(i)
    // decision tree — use the same machinery, and the note's clauses hold
    // verbatim for both: the frozen chain no longer reconstructs the
    // recorded base either way, and files the patch does not touch take the
    // new base's content, flagged, either way).
    auto note_workdir_as_is = [&ctx]() {
        printf("projeny: the frozen base chain of '%s' no longer "
               "reconstructs the recorded base (an upstream Projeny: "
               "link was rebased); rebasing from the workdir as-is; "
               "files the patch does not touch take the new base's "
               "content (flagged as conflicts)\n",
               ctx.projeny_arg.c_str());
    };
    // The fallback's old merge base, shared verbatim by both triggers: a
    // copy of the workdir with the embedded patch reverse-applied, strictly
    // per file (the gate in reverse_file_diff refuses a block whose
    // pre-image the workdir no longer holds verbatim — uncommitted
    // edits near a hunk region make it refuse, and the fixup below
    // then keeps that file's workdir bytes wholesale). The
    // reconstruction is byte-exact base content only INSIDE the
    // patch's hunk pre-images; outside them the old side IS the
    // workdir, so uncommitted edits there are indistinguishable
    // from stale committed content — the true old base is
    // unreconstructable there. The post-merge pass
    // (vcs_detect_displaced_regions) therefore keeps the merged
    // (moved-chain) content everywhere on such files and only REPORTS
    // whether any outside-pre-image region was displaced, flagging the
    // file for the user to inspect (see the passes below), and files
    // the patch never mentions take the new base's content with a
    // Conflict entry the same way.
    auto build_fallback_old_side = [&]() {
        tFallback.reset(
            new TempDir(make_scratch(ctx.pdir, ctx.filebase,
                                     "rebase-fallback")));
        fallback_old_tree = join_path(tFallback->path, cur.name);
        copy_recursive(workdir, fallback_old_tree);
        std::string reversed =
            reverse_patch_text(cur.patch, cur.name, fallback_old_tree,
                               &reverse_failed);
        if (!normalize_patch_text(reversed).empty()) {
            // kAllowAlready: a file the user locally reverted to its
            // base content is correctly skipped as already-reversed (it
            // IS the base), not failed and flagged.
            for (const VcsFailure& f :
                 apply_patch_per_file(fallback_old_tree, reversed,
                                      cur.name,
                                      VcsApplyMode::kAllowAlready)) {
                for (const std::string& p : f.paths)
                    if (!p.empty())
                        reverse_failed.push_back(p);
            }
        }
        sort_unique(&reverse_failed);
    };
    {
        TempDir tC(make_scratch(ctx.pdir, ctx.filebase, "rebase-clean"));
        std::string expect;
        bool built = false;
        if (cur.is_projeny_based()) {
            // Expected tree for a Projeny:-based project: the frozen base's
            // committed extraction plus the current patch — the same source
            // commit's diff base uses (the status-embedded == live
            // guarantee above makes the simple frozen-snapshot law apply).
            // Tolerant: a patch that no longer applies onto the moved chain
            // extraction is the fallback trigger below, not a refusal.
            try {
                TolerantPatchApply tol;
                expect = build_projeny_tree(
                    ctx.projeny_arg, cur,
                    resolve_committed_base_bytes(
                        ctx.projeny_arg, cur,
                        "rebase '" + ctx.projeny_arg + "'"),
                    cur.patch,
                    "patch in '" + ctx.projeny_arg + "' (base '" + cur.base +
                        "')",
                    tC.path, "rebase '" + ctx.projeny_arg + "'");
                built = true;
            } catch (const PatchApplyError& e) {
                if (!is_dir(workdir))
                    rethrow_patch_apply(e); // today's behavior, verbatim
                workdir_fallback = true;
            }
        } else {
            unpack_single_top(
                materialize_archive(ctx.pdir, cur,
                                    "rebase '" + ctx.projeny_arg + "'"),
                tC.path, cur.origname);
            expect = join_path(tC.path, cur.origname);
            if (!normalize_patch_text(cur.patch).empty() &&
                !apply_patch_whole(expect, cur.patch, cur.name,
                                   VcsApplyMode::kFreshApply))
                die("patch in '" + ctx.projeny_arg +
                    "' does not apply to its own base archive '" + cur.archive +
                    "'; run setup to repair first");
            built = true;
        }
        if (built) {
            // Replay recorded pending ops onto the expected tree, then
            // require the workdir to match exactly: any other delta is
            // uncommitted work.
            replay_pending_ops(st, workdir, expect, "check for uncommitted work");
            std::string U = diff_trees(expect, workdir, cur.name);
            if (!normalize_patch_text(U).empty()) {
                if (!cur.is_projeny_based()) {
                    // Archive- and URL-based projects keep the classic
                    // clean-tree law: their base bytes are frozen (the
                    // archive is checked in, the URL snapshot is
                    // hash-pinned), so the base cannot have moved under the
                    // checkout and every mismatch is uncommitted work.
                    die("workdir '" + workdir +
                        "' has uncommitted changes; commit or revert before "
                        "rebasing");
                }
                // STATE-(i) DECISION TREE: the expected tree BUILDS — the
                // frozen chain is self-consistent and the patch applies
                // onto it — yet the workdir mismatches it. Two shapes hide
                // in that mismatch, and they need opposite handling:
                //
                //   - The recorded base MOVED UNDER THE PATCH'S FILES: an
                //     upstream Projeny: link was rebased and its deltas
                //     touch files this patch also touches (without
                //     colliding — the build above succeeded, so this is not
                //     the failed-build trigger). Refusing here would be
                //     wrong twice over: `commit` would fold the upstream
                //     deltas into this project's committed patch (the
                //     README-documented corruption), and the refusal's only
                //     other recovery — erase-setup --force + setup —
                //     DESTROYS the uncommitted work. So the rebase takes
                //     the workdir-as-is fallback: the candidate old side
                //     reverses to exactly the recorded base, the merge
                //     converges onto the moved chain, and the conflict
                //     passes surface every region the fallback could not
                //     reconstruct (touched files flagged when their
                //     outside-pre-image bytes were displaced, untouched
                //     files flagged when the new base replaced them). With
                //     a clean workdir the reversal
                //     is exact, so the outcome equals the erase-setup +
                //     setup recovery's — without destroying anything.
                //
                //   - The mismatch lies entirely OUTSIDE the patch's file
                //     set: uncommitted work on files the patch does not
                //     touch, and/or upstream deltas on such files. The base
                //     the patch was diffed against is intact exactly where
                //     the patch needs it, so the classic refusal stands —
                //     worded below so it does not misdiagnose: the mismatch
                //     may be uncommitted work OR an upstream move under
                //     files the patch does not touch, and the primary
                //     recovery named is the one that works for both
                //     (erase-setup --force + setup); committing is called
                //     out only as the action that would fold upstream
                //     deltas into the patch when a link did move, never as
                //     the primary advice.
                //
                // The discriminator builds the fallback candidate old side
                // (the exact construction the fallback uses, so a reverse
                // failure here feeds the same conservative handling there)
                // and compares it against the frozen chain's PATCH-FREE
                // extraction — the base side the normal-path merge would
                // use — restricted to the files the patch touches. The
                // comparison counts presence, type, mode, and content (see
                // touched_entry_differs): any touched-file difference means
                // the recorded base moved under the patch.
                build_fallback_old_side();
                TempDir tFrozen(
                    make_scratch(ctx.pdir, ctx.filebase, "rebase-frozen"));
                std::string frozen_bytes = resolve_committed_base_bytes(
                    ctx.projeny_arg, cur,
                    "rebase '" + ctx.projeny_arg + "'");
                std::vector<std::string> fchain;
                std::set<std::string> fvisited;
                fvisited.insert(physical_or_lexical(ctx.projeny_arg));
                fchain.push_back(ctx.projeny_arg);
                std::string frozen_base = extract_committed_projeny_tree(
                    frozen_bytes, join_path(ctx.pdir, cur.base), fchain,
                    fvisited, "rebase '" + ctx.projeny_arg + "'",
                    tFrozen.path);
                // The patch's touched-path set, keyed exactly like the
                // fallback passes below key their per-file shapes:
                // modifies (and renames) under every side they name, adds
                // under the new path, deletes under the old path.
                std::set<std::string> touched;
                for (const FileDiff& b :
                     split_file_diffs(normalize_patch_text(cur.patch))) {
                    std::string orel = wid_rel(b.a_path, cur.name);
                    std::string nrel = wid_rel(b.b_path, cur.name);
                    if (block_is_pure_delete(b)) {
                        if (!orel.empty())
                            touched.insert(orel);
                    } else if (block_is_pure_add(b)) {
                        if (!nrel.empty())
                            touched.insert(nrel);
                    } else {
                        if (!orel.empty())
                            touched.insert(orel);
                        if (!nrel.empty() && nrel != orel)
                            touched.insert(nrel);
                    }
                }
                bool base_moved = false;
                for (const std::string& rel : touched) {
                    if (touched_entry_differs(fallback_old_tree, frozen_base,
                                              rel)) {
                        base_moved = true;
                        break;
                    }
                }
                if (base_moved) {
                    // The recorded base demonstrably moved under the patch's
                    // files: take the workdir as-is.
                    note_workdir_as_is();
                    workdir_fallback = true;
                } else {
                    // The Projeny:-chain residual refusal. Unlike the
                    // classic Archive:/URL: refusal above (whose base bytes
                    // are frozen, so every mismatch there really is
                    // uncommitted work), a Projeny: chain's base can move
                    // under files this patch does not touch — and then
                    // NOTHING is uncommitted. The message keeps both
                    // hypotheses, refuses to prescribe "commit them" (the
                    // fold the parenthetical warns against when a link did
                    // move), names no `revert` (there is no such command),
                    // and gives the recovery that works for both hypotheses.
                    die("workdir '" + workdir +
                        "' does not match the recorded base outside the "
                        "files the patch touches: either it has uncommitted "
                        "changes, or an upstream Projeny: link was rebased "
                        "and the base chain moved under files the patch "
                        "does not touch (committing would fold those "
                        "upstream deltas into the patch); recover with "
                        "'projeny erase-setup --force " +
                        ctx.projeny_arg + "' followed by 'projeny setup " +
                        ctx.projeny_arg + "'");
                }
            }
        } else {
            // The expected-tree build failed (state (ii)): the frozen chain
            // is unusable, so the rebase proceeds from the workdir as-is.
            note_workdir_as_is();
            build_fallback_old_side();
        }
    }

    // The new base: either the copied-in tarball (legacy), the verified
    // download (URL mode — its bytes stay in memory and are unpacked from a
    // scratch copy; the snapshot itself is written only at the very end, just
    // before write_status), or the .projeny form's LIVE base bytes (zero-arg
    // and base-switch: what the base file holds NOW, not the frozen snapshot
    // — picking up the base file's committed changes is the whole point of
    // the projeny rebase forms).
    std::string dest_archive;
    std::string new_base;      // the new archive's basename (report + headers)
    std::string verified_data; // URL mode: the verified new tarball's bytes
    std::string new_origname;  // the new base's single top-level directory
    // .projeny mode: the new base file's LIVE bytes and its real path (the
    // chain walk's second step, and the cycle identities' anchor).
    std::string new_projeny_bytes;
    std::string new_projeny_path;
    // URL mode's scratch home for those bytes (see below). It is created
    // only when URL mode actually runs — legacy rebases never write into
    // it — and lives to the end of the function: dest_archive names a file
    // INSIDE it, and that file is unpacked twice below, well after this
    // branch is over. (A unique_ptr keeps the lazy holder explicit: the
    // scratch dir exists only while URL mode does, and no other member
    // state has to track "created or not".)
    std::unique_ptr<TempDir> tURL;
    if (projeny_mode || zero_arg_mode) {
        // Zero-arg and base-switch: read the base file's LIVE bytes now.
        // The base-switch path was validated above; the zero-arg path names
        // cur.base and dies here when the file is gone — the frozen
        // snapshot cannot serve: this form exists to pick up what the base
        // file holds NOW.
        if (zero_arg_mode) {
            new_projeny_path = join_path(ctx.pdir, cur.base);
            if (!path_exists(new_projeny_path))
                die("base file '" + new_projeny_path +
                    "' does not exist; it is needed to rebase '" +
                    ctx.projeny_arg + "'");
        } else {
            new_projeny_path = projeny_base;
        }
        new_projeny_bytes = read_file_bytes(new_projeny_path);
        // The new base's own parse names the extraction's top directory
        // (and the rewritten Origname: below); the base's own
        // Origname-vs-extraction consistency is enforced by the extraction
        // itself, exactly like every chain link's.
        ProjenyFile newpf = ProjenyFile::parse_bytes(
            new_projeny_bytes, "'" + new_projeny_path + "'");
        new_origname = newpf.origname;
        new_base = basename_of(new_projeny_path);
    } else if (url_mode) {
        // URL mode: download+verify EVERY listed URL before anything is
        // written, so a hard error below leaves the .projeny file, the
        // status file, the snapshot, and the workdir untouched (see
        // verify_urls for the exact per-URL semantics).
        tURL.reset(new TempDir(make_scratch(ctx.pdir, ctx.filebase,
                                            "rebase-url")));
        verify_urls(&urls, &verified_data, &new_base, "rebase",
                    ctx.projeny_arg);
        // The verified bytes become the rebase's working archive as a scratch
        // file in tURL — deliberately NOT the snapshot: the snapshot is the
        // checkout's only local copy of the base the .projeny file still
        // records, so clobbering it here would brick the checkout when
        // anything below died (a garbage tarball, a merge-time error) with
        // new_base == cur.archive, and would send the conflict path's
        // old-tree materialization back to the old URL mid-rebase even when
        // that URL is alive. It is also not tmp.path below:
        // unpack_single_top requires its destination to end up with exactly
        // one top-level entry, which the scratch file would spoil. The real
        // snapshot — named after the FIRST URL's basename, the same
        // derivation ProjenyFile::parse applies to the rewritten file — is
        // written late, right before write_status (see below), once the
        // .projeny file and the workdir are final.
        dest_archive = join_path(tURL->path, new_base);
        write_file_bytes(dest_archive, verified_data);
        new_origname = archive_single_top_name(dest_archive);
    } else if (!zero_arg_mode) {
        if (!path_exists(new_tarball))
            die("new tarball '" + new_tarball + "' does not exist");
        new_base = basename_of(new_tarball);
        if (new_base.empty() || new_base.find('/') != std::string::npos)
            die("bad tarball path '" + new_tarball + "'");

        // Copy the tarball into pdir if it isn't already there. When the new
        // tarball's basename matches the current Archive but its bytes differ,
        // warn (content changed under a familiar name) and continue with the new
        // file — never silently keep the old bytes.
        dest_archive = join_path(ctx.pdir, new_base);
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
        new_origname = archive_single_top_name(dest_archive);
    }

    // Build the new tree: unpack the new base (or extract the .projeny
    // form's committed base content), apply current patch, then
    // replay pending add/rm/mv ops so the result includes the pending intent
    // (the ops were validated against base+patch by the clean-check above).
    TempDir tmp(make_scratch(ctx.pdir, ctx.filebase, "rebase"));
    std::string tree;
    if (projeny_mode || zero_arg_mode) {
        // The new base's COMMITTED EXTRACTION (its chain resolved through
        // each link's OWN frozen snapshot), deliberately NOT routed through
        // the project's own Origname: check — the header may still carry the
        // old top dir, and the rebase self-heals it below. The chain walk is
        // seeded with the project itself, so a chain that reaches back dies.
        std::vector<std::string> nchain;
        std::set<std::string> nvisited;
        nvisited.insert(physical_or_lexical(ctx.projeny_arg));
        nchain.push_back(ctx.projeny_arg);
        tree = extract_committed_projeny_tree(
            new_projeny_bytes, new_projeny_path, nchain, nvisited,
            "rebase '" + ctx.projeny_arg + "'", tmp.path);
    } else {
        unpack_single_top(dest_archive, tmp.path, new_origname);
        tree = join_path(tmp.path, new_origname);
    }
    // Frozen mtimes must be REFRESHED from the new archive before the patch
    // is applied (application rewrites files with a "now" timestamp): the
    // frozen set of the current patch is re-valued against the freshly
    // unpacked members, so the regenerated patch carries the new archive's
    // mtimes and the stamp pass at the end pins the workdir to them.
    std::map<std::string, uint64_t> frozen =
        refresh_frozen_from_tree(vcs_frozen_mtimes(cur.patch, cur.name), tree);
    std::vector<std::string> conflicts;
    // Workdir-relative paths the three-way merge below decided on (the files
    // whose hunks failed onto the new base). The fallback preservation passes
    // read this to tell merge-decided content from freshly applied content.
    std::set<std::string> merged_rels;
    // kAllowAlready (not kFreshApply) for the application onto the NEW base:
    // the new tarball is pristine, but it is a DIFFERENT upstream snapshot
    // that may already contain the user's change (upstream incorporated it —
    // the entire reason rebases exist). There the forward application fails
    // and the Already path is what recognizes the incorporated change and
    // still repairs mode/newline drift; kFreshApply here would drop that
    // repair (and the blank-adjacent deletion class this mode exists for is
    // closed by the pre-image veto in hunks_match_all instead). The
    // freshly-unpacked-own-base check above keeps kFreshApply: the patch
    // must apply forward to the very tarball it was diffed against.
    if (!normalize_patch_text(cur.patch).empty() &&
        !apply_patch_whole(tree, cur.patch, cur.name,
                           VcsApplyMode::kAllowAlready)) {
        std::vector<VcsFailure> bad = apply_patch_per_file(
            tree, cur.patch, cur.name, VcsApplyMode::kAllowAlready);
        // 3-way merge each failed file: base = OLD tree file, ours = new
        // tree file (patched except failed parts), theirs = old tree file.
        TempDir tO(make_scratch(ctx.pdir, ctx.filebase, "rebase-old"));
        std::string old_tree;
        if (workdir_fallback) {
            // The frozen chain is unusable: the fallback old tree built
            // during the clean-check phase above (the workdir copy with the
            // embedded patch reverse-applied). tO stays unused.
            old_tree = fallback_old_tree;
        } else if (cur.is_projeny_based()) {
            // The OLD base is the FROZEN snapshot's committed extraction —
            // the FULL extraction (the base's own committed patch included),
            // because the project's patch is relative to it. Like the new
            // tree above, deliberately NOT routed through the project's own
            // Origname: check (the header may carry the new base's top dir
            // already when the rewrite below has not happened yet — the
            // merge only needs the trees). Unreachable in fallback mode
            // with this failure class: the fallback exists precisely
            // because this extraction dies.
            std::string frozen_bytes = resolve_committed_base_bytes(
                ctx.projeny_arg, cur, "rebase '" + ctx.projeny_arg + "'");
            std::vector<std::string> ochain;
            std::set<std::string> ovisited;
            ovisited.insert(physical_or_lexical(ctx.projeny_arg));
            ochain.push_back(ctx.projeny_arg);
            old_tree = extract_committed_projeny_tree(
                frozen_bytes, join_path(ctx.pdir, cur.base), ochain, ovisited,
                "rebase '" + ctx.projeny_arg + "'", tO.path);
        } else {
            unpack_single_top(
                materialize_archive(ctx.pdir, cur,
                                    "rebase '" + ctx.projeny_arg + "'"),
                tO.path, cur.origname);
            old_tree = join_path(tO.path, cur.origname);
        }
        // Apply the old patch to the old tree to get the "theirs" content?
        // The old tree unpacked is the base; workdir == base+patch (clean
        // check above), so theirs-file == workdir file.
        for (const auto& f : bad) {
            std::vector<std::string> touched = failure_paths_or_die(f, "rebase");
            sort_unique(&touched);
            for (const std::string& rel : touched) {
                if (rel.empty())
                    continue;
                bool clean = merge_one_file(join_path(old_tree, rel),
                                            join_path(tree, rel),
                                            join_path(workdir, rel),
                                            join_path(tree, rel), tree);
                merged_rels.insert(rel);
                if (!clean)
                    conflicts.push_back(rel);
            }
        }
    }
    // Fallback preservation passes. The merge above three-ways every file
    // whose hunks failed onto the new base — but its base is the
    // RECONSTRUCTED old side: byte-exact base content inside the patch's
    // hunk pre-images, and the workdir's own bytes outside them, where
    // uncommitted edits are indistinguishable from stale committed content.
    // Two consequences, closed by two passes (both run before the
    // reverse-failed fixup below, which owns the files the gate refused):
    //
    // 1. DISPLACEMENT PASS — hunk-covered files. The merge sees uncommitted
    //    edits outside the pre-images as base content, so the new side's
    //    content there wins silently (feeding the workdir as the explicit
    //    other side would not help: against the edit-bearing base the new
    //    content reads as a deliberate one-sided edit and wins the same
    //    way). Those outside-pre-image bytes cannot be told apart from
    //    stale committed content — the true old base is unreconstructable
    //    there — so the moved chain's content STANDS (symmetric with
    //    untouched files, which take the new side's content wholesale) and
    //    the file is FLAGGED when any changed old-side range lies outside
    //    the pre-images (vcs_detect_displaced_regions): the displacement
    //    must be visible, never silent. Restoring the workdir's bytes there
    //    instead would revert the upstream move as often as it preserved
    //    real uncommitted work — and the reversion would fold into the
    //    regenerated committed patch through the resolve + commit flow,
    //    silently and durably. Users who want uncommitted outside-pre-image
    //    work kept must COMMIT it first: commit folds it into the project's
    //    patch, where the strict reverse gate (it refuses a pre-image the
    //    workdir no longer holds verbatim) and the three-way protect it.
    //    Regions inside the pre-images keep
    //    the merge's output (resolutions and conflict markers), so genuine
    //    overlaps still conflict and clean resolutions still ride: the
    //    reverse's success there is positive evidence the workdir matched
    //    base+patch exactly, so the three-way is trusted. The classification
    //    is anchored in old-side coordinates, so it runs identically for
    //    files whose hunks failed onto the new base (merged_rels) and for
    //    files whose hunks applied cleanly (the tree holds the applied patch;
    //    the same classification covers the outside-hunk regions around it —
    //    a changed base range covering the applied pre-image core always
    //    intersects the pre-image ranges, so the committed change itself is
    //    never a displacement candidate).
    //
    // 2. NEW-CONTENT PASS — files the patch does not touch (no hunk in the
    //    project's patch names them). The merge never runs for them and the
    //    tree holds the new base's content wholesale; the reconstructed old
    //    side equals the workdir there, so any uncommitted edit is
    //    indistinguishable from stale committed content and CANNOT be
    //    separated from an upstream move. Convergence wins (the typical
    //    bricked state's divergent untouched files are exactly the
    //    upstream-moved ones), so the tree's content stays — but the
    //    replacement is flagged with a Conflict entry so it is visible and
    //    resolvable, mirroring the divergent-binary rule (the new side
    //    wins, conflict recorded). This is the same policy pass 1 applies
    //    to touched files outside their pre-images.
    //
    // Both passes skip files the reverse gate refused (reverse_failed: the
    // fixup below keeps those at workdir bytes), rename-shaped and binary
    // blocks (the merge and fixups above own them; no old-side anchor to
    // classify against), and files absent from the workdir. Untracked workdir
    // files are out of scope: they are not in the new tree, so the
    // new-content pass never visits them.
    if (workdir_fallback && !normalize_patch_text(cur.patch).empty()) {
        // Per-file shape of the project's patch, aggregated over its blocks.
        struct FallbackFile {
            bool has_hunks = false;  // at least one text hunk names the file
            bool text_modify = true; // every block is a same-path text
                                     // modify: the shape the displacement
                                     // analysis can classify
            bool pure_add = true;    // every block is a pure add of the file
            bool pure_delete = true; // every block is a pure delete
            bool rename = false;     // any block moves the file
            bool binary = false;     // any block is binary
            std::vector<std::pair<size_t, size_t>> pre_ranges;
        };
        std::map<std::string, FallbackFile> ff;
        for (const FileDiff& b :
             split_file_diffs(normalize_patch_text(cur.patch))) {
            std::string orel = wid_rel(b.a_path, cur.name);
            std::string nrel = wid_rel(b.b_path, cur.name);
            bool binary = b.text.find("GIT binary patch") != std::string::npos ||
                          b.text.find("Binary files ") != std::string::npos;
            bool add = block_is_pure_add(b);
            bool del = block_is_pure_delete(b);
            bool rename = !add && !del && !orel.empty() && !nrel.empty() &&
                          orel != nrel;
            std::vector<std::pair<size_t, size_t>> pre;
            bool hunks_ok = !binary;
            if (!binary) {
                for (const std::string& l : split_lines(b.text)) {
                    if (l.compare(0, 4, "@@ -") != 0)
                        continue;
                    // "@@ -os[,oc] +ns[,nc] @@ section": unified-diff
                    // grammar, 1-based lines; a missing ,count means 1, and
                    // count 0 puts the empty range AFTER the named line.
                    // Only the OLD-side ranges are kept: the displacement
                    // analysis is anchored in old-side coordinates (base =
                    // the reconstructed old side).
                    size_t os = 0, oc = 1, ns = 0, nc = 1;
                    size_t i = 4; // after "@@ -"
                    auto num = [&](size_t* v) {
                        size_t r = 0;
                        bool any = false;
                        while (i < l.size() && l[i] >= '0' && l[i] <= '9') {
                            r = r * 10 + (l[i] - '0');
                            ++i;
                            any = true;
                        }
                        if (any)
                            *v = r;
                        return any;
                    };
                    if (!num(&os)) {
                        hunks_ok = false; // malformed header: do not trust
                        break;
                    }
                    if (i < l.size() && l[i] == ',') {
                        ++i;
                        if (!num(&oc))
                            hunks_ok = false;
                    }
                    if (i < l.size() && l[i] == ' ')
                        ++i;
                    if (i < l.size() && l[i] == '+')
                        ++i;
                    if (!num(&ns)) {
                        hunks_ok = false;
                        break;
                    }
                    if (i < l.size() && l[i] == ',') {
                        ++i;
                        if (!num(&nc))
                            hunks_ok = false;
                    }
                    if (!hunks_ok)
                        break;
                    size_t ps = oc == 0 ? os : (os > 0 ? os - 1 : 0);
                    pre.push_back({ps, ps + oc});
                }
            }
            // Key the shapes: modifies under their (shared) path, adds under
            // the new path, deletes under the old path, renames under both.
            std::vector<std::string> keys;
            if (del && !orel.empty())
                keys.push_back(orel);
            else if (add && !nrel.empty())
                keys.push_back(nrel);
            else {
                if (!orel.empty())
                    keys.push_back(orel);
                if (!nrel.empty() && nrel != orel)
                    keys.push_back(nrel);
            }
            for (const std::string& rel : keys) {
                FallbackFile& info = ff[rel];
                info.has_hunks =
                    info.has_hunks || (!pre.empty() && hunks_ok);
                info.text_modify = info.text_modify && hunks_ok && !binary &&
                                  !rename && !add && !del && !orel.empty() &&
                                  orel == nrel;
                info.pure_add = info.pure_add && add && hunks_ok;
                info.pure_delete = info.pure_delete && del && hunks_ok;
                info.rename = info.rename || rename;
                info.binary = info.binary || binary;
                if (hunks_ok)
                    info.pre_ranges.insert(info.pre_ranges.end(), pre.begin(),
                                           pre.end());
            }
        }
        auto in_reverse_failed = [&reverse_failed](const std::string& rel) {
            return std::binary_search(reverse_failed.begin(),
                                      reverse_failed.end(), rel);
        };
        auto flag_conflict = [&conflicts](const std::string& rel) {
            if (std::find(conflicts.begin(), conflicts.end(), rel) ==
                conflicts.end())
                conflicts.push_back(rel);
        };
        // Pass 1: surface the outside-pre-image displacement of hunk-covered
        // files (the merged content itself always stands — nothing is
        // written back here; the structural add/delete/tree-missing shapes
        // below are the only writers).
        for (const auto& kv : ff) {
            const std::string& rel = kv.first;
            const FallbackFile& info = kv.second;
            if (in_reverse_failed(rel))
                continue; // the fixup below keeps the workdir bytes
            std::string wf = join_path(workdir, rel);
            struct stat ws;
            if (lstat(wf.c_str(), &ws) != 0 || !S_ISREG(ws.st_mode))
                continue; // nothing local to preserve (or a non-file)
            if (info.has_hunks && info.pure_add && !info.rename &&
                !info.binary) {
                // The patch created the file; the workdir's bytes are the
                // patch's content plus any uncommitted edit. When the add
                // applied cleanly (the merge never ran for it) the tree
                // holds the bare patch content — take the workdir's bytes so
                // the edit rides. When the merge ran, its base-missing
                // three-way already keeps or conflicts divergent content.
                if (merged_rels.count(rel) != 0)
                    continue;
                std::string tf = join_path(tree, rel);
                struct stat ts;
                if (lstat(tf.c_str(), &ts) != 0 || !S_ISREG(ts.st_mode))
                    continue;
                if (files_equal(wf, tf))
                    continue;
                copy_path_preserving(wf, tf);
                continue; // nothing was displaced and nothing staled: no flag
            }
            if (info.has_hunks && info.pure_delete && !info.rename &&
                !info.binary) {
                // The patch deleted the file; workdir bytes mean the user
                // recreated it locally — uncommitted work the tree would
                // otherwise silently drop. Keep it, flagged.
                std::string tf = join_path(tree, rel);
                if (path_exists(tf))
                    continue; // the merge already placed content for it
                make_dirs(dirname_of(tf));
                copy_path_preserving(wf, tf);
                flag_conflict(rel);
                continue;
            }
            if (!info.has_hunks || !info.text_modify)
                continue; // mode-only, rename, binary or mixed: no analysis
            std::string of = join_path(fallback_old_tree, rel);
            struct stat osd;
            if (lstat(of.c_str(), &osd) != 0 || !S_ISREG(osd.st_mode))
                continue; // no old-side anchor: nothing to classify against
            std::string tf = join_path(tree, rel);
            struct stat ts;
            if (lstat(tf.c_str(), &ts) != 0 || !S_ISREG(ts.st_mode)) {
                // The patch modifies the file but the merged tree lacks it,
                // while the workdir holds content: keep the workdir's bytes,
                // flagged, like the reverse-failed fixup.
                make_dirs(dirname_of(tf));
                copy_path_preserving(wf, tf);
                flag_conflict(rel);
                continue;
            }
            // The modify-shaped case, reverse-succeeded: the workdir's
            // outside-pre-image bytes are indistinguishable from stale
            // committed content (the true old base is unreconstructable
            // there), so the merged (moved-chain) content STANDS as the
            // file's content — nothing is written back — and the file is
            // flagged when the merged content displaced any of those bytes
            // (a changed old-side range outside the hunk pre-images). The
            // flag is markerless, like pass 2's: the user inspects and
            // either accepts the converged content or restores their own
            // bytes via resolve before the commit folds the outcome into
            // the regenerated patch. Inside the pre-images the merge's
            // output (resolutions and markers) is trusted as produced: the
            // strict reverse gate succeeded there, which is positive
            // evidence the workdir matched base+patch exactly.
            std::string old_bytes = read_file_bytes(of);
            std::string tree_bytes = read_file_bytes(tf);
            if (vcs_detect_displaced_regions(old_bytes, tree_bytes,
                                             info.pre_ranges))
                flag_conflict(rel);
        }
        // Pass 2: flag untouched files whose workdir content the new base's
        // content replaced (the tree already holds the new side).
        {
            std::vector<std::string> stack;
            stack.push_back("");
            while (!stack.empty()) {
                std::string d = stack.back();
                stack.pop_back();
                std::string abs = d.empty() ? tree : join_path(tree, d);
                for (const std::string& name : list_dir_names(abs)) {
                    std::string rel = d.empty() ? name : d + "/" + name;
                    std::string tfull = join_path(tree, rel);
                    struct stat tst;
                    if (lstat(tfull.c_str(), &tst) != 0)
                        continue;
                    if (S_ISDIR(tst.st_mode)) {
                        stack.push_back(rel);
                        continue;
                    }
                    auto it = ff.find(rel);
                    if (it != ff.end() &&
                        (it->second.has_hunks || it->second.rename ||
                         it->second.binary))
                        continue; // patch-covered: pass 1 / the fixup own it
                    if (in_reverse_failed(rel))
                        continue; // the fixup below owns it
                    std::string wfull = join_path(workdir, rel);
                    struct stat wst;
                    if (lstat(wfull.c_str(), &wst) != 0)
                        continue; // not in the workdir: nothing was replaced
                    bool t_link = S_ISLNK(tst.st_mode) != 0;
                    bool w_link = S_ISLNK(wst.st_mode) != 0;
                    if (!S_ISREG(tst.st_mode) && !t_link)
                        continue;
                    if (!S_ISREG(wst.st_mode) && !w_link)
                        continue;
                    bool same;
                    if (t_link != w_link) {
                        same = false;
                    } else if (t_link) {
                        same = read_link_target(tfull) ==
                               read_link_target(wfull);
                    } else {
                        same = files_equal(wfull, tfull);
                    }
                    if (!same)
                        flag_conflict(rel);
                }
            }
        }
    }
    // Fallback fixup: files whose blocks could not be reversed (binary
    // literals, opaque or unparseable blocks) keep their WORKDIR bytes —
    // conservative, no markers: the merge above cannot know their true
    // base, so where the merged content differs from what the workdir
    // holds, the workdir wins and the file is flagged conflicted for the
    // usual resolve + commit flow. (In fallback mode this runs even without
    // merge failures: the fixup compares against the merged tree, whatever
    // produced it.)
    if (workdir_fallback) {
        for (const std::string& rel : reverse_failed) {
            std::string wf = join_path(workdir, rel);
            struct stat ws;
            if (lstat(wf.c_str(), &ws) != 0 || !S_ISREG(ws.st_mode))
                continue; // nothing local to preserve (a rename source)
            std::string tf = join_path(tree, rel);
            struct stat ts;
            if (lstat(tf.c_str(), &ts) == 0 && files_equal(wf, tf))
                continue; // already matches: leave it
            make_dirs(dirname_of(tf));
            copy_path_preserving(wf, tf);
            conflicts.push_back(rel);
        }
    }

    // Replay pending ops onto the rebased tree: adds/renames copy the user's
    // on-disk content forward (the new tree may differ textually from the old
    // base), removals delete, and renames remove the source. The regenerated
    // patch below is diffed against this final tree, so pending intent is
    // folded into the stored patch while the ops ALSO stay listed as pending
    // (commit validates and formally folds them).
    replay_pending_ops(st, workdir, tree, "rebase");

    // Same rename-forwarding as commit: a frozen pin follows the file it
    // was frozen under, so a pending mv re-keys the entry to the
    // destination before the regenerated patch is built (emit_block stamps
    // the live path). Order matters so chains compose.
    for (const auto& rn : st.renamed) {
        auto it = frozen.find(rn.first);
        if (it == frozen.end())
            continue;
        uint64_t ts = it->second;
        frozen.erase(it);
        frozen[rn.second] = ts;
    }

    // Update headers: the archive location (the Projeny: base file for a
    // base-switch; Archive: for a checked-in tarball; the URL: list for URL
    // mode — replace_archive_url_headers swaps every Archive:/URL:/Projeny:
    // line for the new URL: lines, so any mode converts to URL:-based here;
    // the Archive:-tarball conversion away from the Projeny: form goes
    // through replace_base_headers for the same reason — replace_header_
    // value could not remove the Projeny: line it replaces) plus the new top
    // dir. The zero-arg form changes NO base header: it re-extracts the
    // base the Projeny: header already names. The patch body's wid labels
    // already use Name (unchanged).
    if (projeny_mode && !zero_arg_mode) {
        // Base-switch: the header stores a plain filename, and the
        // same-directory law validated above guarantees the base lives next
        // to the target — so the new line carries the bare basename.
        cur.head = replace_base_headers(
            cur.head, "Projeny: " + basename_of(new_projeny_path) + "\n");
        cur.base = basename_of(new_projeny_path);
    } else if (url_mode) {
        cur.head = replace_archive_url_headers(cur.head, urls);
        cur.urls = urls;
        cur.archive = new_base;
    } else if (!projeny_mode && !zero_arg_mode && cur.is_projeny_based()) {
        // Mode conversion away from the Projeny: form via a tarball: the
        // Projeny: line is replaced by the new Archive: line (the old
        // .<stem>.snapshot simply lingers as a stale file — stale snapshots
        // are never auto-cleaned).
        cur.head = replace_base_headers(cur.head,
                                        "Archive: " + new_base + "\n");
        cur.archive = new_base;
    } else if (!projeny_mode && !zero_arg_mode) {
        cur.head = replace_header_value(cur.head, "Archive", new_base);
        cur.archive = new_base;
    }
    cur.head = replace_header_value(cur.head, "Origname", new_origname);
    cur.origname = new_origname;
    // Regenerate the patch against the new base so hunk positions/counts are
    // exact (content is base+patch by construction). The regenerated patch
    // carries the refreshed frozen-mtime headers, including attribute-only
    // blocks for frozen files the new base and the patched tree agree on.
    {
        TempDir tB(make_scratch(ctx.pdir, ctx.filebase, "rebase-base"));
        std::string base_only;
        if (projeny_mode || zero_arg_mode) {
            // The pristine committed extraction of the new base again (the
            // working tree above was patched): the same bytes, resolved
            // through the same snapshots, so the regenerated patch's hunk
            // positions are exact.
            std::vector<std::string> bchain;
            std::set<std::string> bvisited;
            bvisited.insert(physical_or_lexical(ctx.projeny_arg));
            bchain.push_back(ctx.projeny_arg);
            base_only = extract_committed_projeny_tree(
                new_projeny_bytes, new_projeny_path, bchain, bvisited,
                "rebase '" + ctx.projeny_arg + "'", tB.path);
        } else {
            unpack_single_top(dest_archive, tB.path, new_origname);
            base_only = join_path(tB.path, new_origname);
        }
        VcsDiffOpts ropts;
        ropts.frozen_mtimes = &frozen;
        ropts.frozen_attribute_blocks = true;
        // vcs_diff_trees_ex already returns canonical labels; normalize
        // ends with exactly one newline.
        std::string regen = normalize_patch_text(vcs_diff_trees_ex(
            base_only, tree, cur.name, ropts));
        cur.rebuild(regen);
    }
    write_file_bytes(ctx.projeny_arg, cur.raw);

    reconcile_binaries(workdir, tree, true);
    if (path_exists(workdir) && !remove_recursive(workdir))
        die("cannot remove existing workdir '" + workdir + "'");
    move_path(tree, workdir);

    // NOW the snapshots: the .projeny file has been rewritten and the
    // workdir swapped, so every die before this point left the .projeny
    // file, the status file, the snapshot, and the workdir exactly as they
    // were. They must be in place before write_status, whose
    // ensure_url_snapshot / resolve_committed_base_bytes re-check takes a
    // snapshot already matching as the archive/base (one written here would
    // otherwise be re-downloaded or re-frozen); both merge outcomes — a
    // clean tree and conflicts left as markers — reach this line.
    // write_file_bytes is temp file + rename, so the snapshot switches
    // atomically. The URL snapshot holds the verified download; the
    // Projeny: form's frozen snapshot is OVERWRITTEN with the new base's
    // live bytes (the sanctioned re-freeze: the zero-arg form re-extracts
    // what the base file holds now, the base-switch form switches the freeze
    // to the newly named base). A conversion away from the Projeny: form
    // leaves the old snapshot in place: stale snapshots are never
    // auto-cleaned, and the new form's snapshot is managed by the existing
    // flows.
    if (url_mode)
        write_file_bytes(snapshot_path_for(join_path(ctx.pdir, new_base)),
                         verified_data);
    if (projeny_mode || zero_arg_mode)
        write_file_bytes(projeny_snapshot_path_for(ctx.projeny_arg),
                         new_projeny_bytes);

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
    // The rebased tree's frozen files may have been rewritten by the applier
    // ("now" timestamps): re-stamp them to the refreshed values the new
    // patch carries (the new archive's member mtimes).
    stamp_frozen_mtimes(workdir, cur.patch, cur.name);
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

namespace {

// What a `projeny status`-equivalent computation reports for one project,
// factored into the two halves cmd_status prints so the erase-setup check
// phase (cmd_erase_setup_multi) can reuse exactly the same semantics — its
// definition of "would a commit have anything to do" is "status reports
// anything other than untracked files":
//
//   read_recorded_status — the status file's recorded state (the
//                          Conflict:/Added:/Removed:/Renamed: entries) plus
//                          the workdir the status records, with status's
//                          exact Name-fallback rule. PURE: it only parses
//                          files and stats directories; nothing on disk is
//                          renamed, written, or downloaded.
//
//   compute_live_diff    — status's live-diff section: materialize the
//                          archive the status records (snapshot first,
//                          exactly like status), build the expected tree,
//                          and diff it against the workdir. This half has
//                          the side effects: a legacy undotted snapshot is
//                          renamed into its dotted form, a classic archive
//                          is copied into a missing snapshot, and a URL
//                          snapshot that is missing or matches no URL:
//                          hash is re-downloaded. It also prints status's
//                          "(continuing ...)" warnings when the snapshot
//                          cannot be settled. `cmd_verb` words the
//                          frozen-chain-moved guidance die (status says
//                          "cannot status", the erase-setup check phase
//                          says "cannot erase-setup") — unless the caller
//                          passes `tolerate_chain_moved` (only cmd_status's
//                          conflicted-interim case does), which reports
//                          that one failure class through
//                          LiveDiff::chain_moved instead of dying.
//
// What is deliberately NOT shared: cmd_status's workdir-missing branch.
// It calls disregard_stale_state, which RENAMES the status file and the
// snapshots to '<name>.stale' names — a mutation that only makes sense for
// the informational command, and one the check phase must never perform
// (erase-setup reports a missing workdir as "did not exist" and would
// otherwise see its own warning change). cmd_status therefore runs that
// branch itself; the check phase treats a missing workdir as clean (there
// is nothing to destroy) without touching the status machinery at all.

struct RecordedStatus {
    StatusData st;           // the recorded entries, from the status file
    ProjenyFile emb;         // the embedded .projeny copy the status records
    std::string workdir;     // the workdir the status records (with the
                             // current-Name fallback applied)
    std::string cur_archive; // the CURRENT .projeny file's Archive: (empty
                             // when that file is unreadable, conflicted, or
                             // invalid) — used only by status's stale-state
                             // bookkeeping
    bool cur_projeny = false; // the CURRENT .projeny file is Projeny:-based
                              // (used only by status's stale-state
                              // bookkeeping, next to cur_archive)
};

RecordedStatus read_recorded_status(const std::string& pdir,
                                    const std::string& projeny_arg,
                                    const std::string& statusfile)
{
    RecordedStatus rec;
    rec.st = StatusData::parse(statusfile);

    // The embedded .projeny copy names the tree the status file records; the
    // workdir lives at its Name, falling back to the current .projeny Name's
    // dir when only that one exists (Name may have changed since the last
    // setup/commit).
    rec.emb = ProjenyFile::parse_bytes(
        rec.st.embedded, "embedded copy in '" + statusfile + "'");
    rec.workdir = join_path(pdir, rec.emb.name);
    {
        std::string cur_raw;
        if (try_read_file_bytes(projeny_arg, &cur_raw) &&
            !projeny_has_conflict_markers(cur_raw) &&
            validate_projeny_bytes(cur_raw).empty()) {
            ProjenyFile curpf =
                ProjenyFile::parse_bytes(cur_raw, "'" + projeny_arg + "'");
            rec.cur_archive = curpf.archive;
            rec.cur_projeny = curpf.is_projeny_based();
            std::string curdir = join_path(pdir, curpf.name);
            if (!is_dir(rec.workdir) && is_dir(curdir))
                rec.workdir = curdir;
        }
    }
    return rec;
}

// The live half: modified files, disappeared files (in the expected tree
// but missing on disk and not marked removed/rename-source), and untracked
// files (on disk but in neither the expected tree nor the pending
// added/rename-destination sets). Pending ops themselves stay on their
// Added:/Removed:/Renamed: lines (the recorded half) and are not repeated
// here. `ran` is false — and every list empty — when the diff could not
// run: a missing workdir, no usable archive, or (with `tolerate_chain_moved`)
// the chain-moved build failure reported through `chain_moved` instead of
// the guidance die. `filebase` is the .projeny
// file's basename (Ctx::filebase), so the expected tree is built in a
// scratch dir named after the file (see make_scratch).
struct LiveDiff {
    bool ran = false;
    // Set only when `tolerate_chain_moved` was passed: the expected-tree
    // build failed with the chain-moved failure class (the recorded patch
    // no longer applies onto the frozen base chain — an upstream Projeny:
    // link was rebased). The caller owns the wording of the response;
    // cmd_status's conflicted-interim case reports the recorded state and
    // a live-diff-unavailable note instead of the guidance die.
    bool chain_moved = false;
    std::vector<std::string> modified, disappeared, untracked;
};

LiveDiff compute_live_diff(const std::string& pdir, const StatusData& st,
                           const ProjenyFile& emb,
                           const std::string& workdir,
                           const std::string& statusfile,
                           const std::string& filebase,
                           const std::string& cmd_verb,
                           bool tolerate_chain_moved = false)
{
    LiveDiff live;
    if (!is_dir(workdir))
        return live; // nothing to diff; cmd_status runs its stale branch

    // Snapshot-aware and fully tolerant: prefer the snapshot (the
    // byte-exact copy of what the last setup actually used, which
    // survives git deleting the archive), fall back to the archive
    // itself (checkouts set up before snapshots existed — and copy it
    // into the snapshot, best effort, so the next run finds one), and
    // skip the live-diff section entirely when neither exists.
    // URL-based projects never have a checked-in archive: their
    // snapshot is the verified download cache. Always route it through
    // try_ensure_url_snapshot — the spec gives status no exception, so
    // an existing snapshot is hash-verified too (no network while it
    // matches any URL: hash), and a missing one is fetched. Status
    // stays informational, so a failed download or verification only
    // warns and skips the live-diff section (exactly like the classic
    // missing-archive case below).
    std::string archive_path = join_path(pdir, emb.archive);
    // The snapshot-materialization step is the only part of the live diff
    // that mutates shared state, so it alone runs under
    // g_fallback_download_mu (see its comment): two concurrent checkers
    // sharing an archive whose snapshot is missing must never materialize
    // (download/copy) it twice, and the loser of the race re-checks the
    // snapshot the winner just wrote — under the same mutex. The heavy rest
    // of the diff (untar + patch + the recursive byte-compare) only reads
    // the workdir and the checker's own scratch tree, and for projects that
    // AGREE on the archive's content a snapshot that has settled is never
    // rewritten (materialization writes one only when it is missing or
    // matches no URL: hash, and a hash one worker verified another worker
    // verifies identically), so it runs in parallel. Projects that DISAGREE
    // — two checked projects sharing an archive basename while expecting
    // different hashes for it — share one snapshot path, and a losing
    // worker's post-settlement re-materialization (write_file_bytes'
    // atomic temp+rename, outside the winner's critical section) can swap
    // the snapshot bytes under the winner's unlocked build_tree_from_patch/
    // tar read. That race is fail-closed: the winner's diff then reports
    // spurious modifications (a dirty refusal) or its tar/patch dies on the
    // unexpected bytes — never a wrong erase, never silent data loss — so
    // it is tolerated rather than locked out. The lock is never nested with
    // try_ensure_url_snapshot's own multi-mode locking: that only engages
    // when g_multi_plan is set, which neither caller (cmd_status, the
    // erase-setup check phase) ever is.
    //
    // A Projeny:-based status copy has no archive at all: its expected tree
    // is the committed extraction of the FROZEN base (the status's
    // snapshot, else the live base file frozen into it — see
    // resolve_committed_base_bytes) plus the embedded patch. Resolving the
    // base bytes is the one mutating step that branch adds (a missing
    // snapshot is frozen here), so it runs under the same mutex as the
    // archive materializations below; unlike them it DIES when neither the
    // snapshot nor the live base file exists — the adapted missing-archive
    // error of resolve_status_archive — so a checker never reports a
    // Projeny:-based checkout clean when it could not know.
    std::string projeny_bytes;
    if (emb.is_projeny_based()) {
        std::lock_guard<std::mutex> lk(g_fallback_download_mu);
        projeny_bytes = resolve_committed_base_bytes(
            join_path(pdir, filebase), emb,
            "reconstruct the tree recorded by '" + statusfile + "'");
    } else if (emb.is_url_based()) {
        std::string got, err;
        std::lock_guard<std::mutex> lk(g_fallback_download_mu);
        // announce_skip = false: status is informational and must print
        // nothing extra on a healthy (snapshot-matching) checkout.
        if (try_ensure_url_snapshot(pdir, emb, &got, &err, false))
            archive_path = got;
        else {
            warn(err + " (continuing without the live diff)");
            return live;
        }
    } else {
        std::lock_guard<std::mutex> lk(g_fallback_download_mu);
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
                warn("could not create snapshot '" + snap +
                     "' from archive '" + archive_path + "': " + err +
                     " (continuing)");
        } else {
            return live;
        }
    }
    TempDir tmp(make_scratch(pdir, filebase, "status"));
    std::string Etree;
    if (emb.is_projeny_based()) {
        // Tolerant: a patch that no longer applies onto the moved chain
        // extraction means every read of this checkout is bricked the same
        // way — die with the recovery guidance (worded for the calling
        // command) instead of the bare apply report. `tolerate_chain_moved`
        // (cmd_status's conflicted-interim case: conflicts are recorded and
        // the recovery the guidance names — rebase, commit — is itself
        // refused while conflicts are unresolved, so the guidance is
        // circular there) reports the failure through `chain_moved`
        // instead, leaving the response to the caller.
        try {
            TolerantPatchApply tol;
            Etree = build_projeny_tree(join_path(pdir, filebase), emb,
                                       projeny_bytes, emb.patch,
                                       "embedded patch in '" + statusfile +
                                           "'",
                                       tmp.path,
                                       "reconstruct the tree recorded by '" +
                                           statusfile + "'");
        } catch (const PatchApplyError&) {
            if (tolerate_chain_moved) {
                live.chain_moved = true;
                return live;
            }
            die_projeny_chain_moved(cmd_verb, join_path(pdir, filebase));
        }
    } else {
        Etree = build_tree_from_patch(tmp, archive_path, emb.origname, emb.name,
                                      emb.patch,
                                      "embedded patch in '" + statusfile +
                                          "'");
    }

    struct Entry {
        int kind = 0; // 0=regular, 1=symlink, 2=other
        std::string content; // regular: bytes; symlink: target
        bool exec = false;
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
                    if (vcs_is_scratch_rel(child))
                        continue;
                    collect(root, child, out);
                }
                return;
            }
            if (vcs_is_scratch_rel(rel))
                return;
            Entry e;
            if (S_ISLNK(lst.st_mode)) {
                e.kind = 1;
                e.content = read_link_target(full);
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
    for (auto& kv : emap) {
        auto it = wmap.find(kv.first);
        if (it == wmap.end()) {
            if (!covered_by(rm_cover, kv.first))
                live.disappeared.push_back(kv.first);
        } else {
            const Entry& a = kv.second;
            const Entry& b = it->second;
            bool same = (a.kind == b.kind && a.content == b.content &&
                         a.exec == b.exec);
            if (!same && !covered_by(rm_cover, kv.first))
                live.modified.push_back(kv.first);
        }
    }
    for (auto& kv : wmap) {
        if (emap.find(kv.first) == emap.end() &&
            !covered_by(add_cover, kv.first))
            live.untracked.push_back(kv.first);
    }
    sort_unique(&live.modified);
    sort_unique(&live.disappeared);
    sort_unique(&live.untracked);
    live.ran = true;
    return live;
}

} // namespace

int cmd_status(const std::string& projeny_arg)
{
    std::string pj = resolve_projeny_path(projeny_arg, "status");
    Ctx ctx = resolve_ctx(pj);
    if (!path_exists(ctx.statusfile)) {
        printf("projeny: '%s' is not set up (no status file)\n",
               ctx.projeny_arg.c_str());
        return 1;
    }
    RecordedStatus rec =
        read_recorded_status(ctx.pdir, ctx.projeny_arg, ctx.statusfile);

    if (!is_dir(rec.workdir)) {
        // The checkout directory is gone: the status file and the archive
        // snapshots are stale state from the removed checkout. Disregard
        // them (warn + rename to '<name>.stale', '.stale2', ...), report the
        // recorded state below, and skip the live diff (nothing to diff).
        // A Projeny:-based file — the status copy's or the current file's —
        // adds its frozen base snapshot to the list. This renames files on
        // purpose — which is exactly why it lives
        // here and not in the shared computation (see RecordedStatus
        // above): erase-setup's check phase reuses the computation without
        // ever staling anything.
        std::vector<std::string> archives;
        archives.push_back(rec.emb.archive);
        if (!rec.cur_archive.empty() && rec.cur_archive != rec.emb.archive)
            archives.push_back(rec.cur_archive);
        {
            std::vector<std::string> extra;
            if (rec.emb.is_projeny_based() || rec.cur_projeny)
                extra.push_back(projeny_snapshot_path_for(ctx.projeny_arg));
            disregard_stale_state(ctx, archives, nullptr,
                                  extra.empty() ? nullptr : &extra);
        }
    }

    printf("Status: %s\n", rec.st.status.c_str());
    for (auto& c : rec.st.conflicts)
        printf("Conflict: %s\n", c.c_str());
    for (auto& a : rec.st.added)
        printf("Added: %s\n", a.c_str());
    for (auto& r : rec.st.removed)
        printf("Removed: %s\n", r.c_str());
    for (auto& rn : rec.st.renamed)
        printf("Renamed: %s -> %s\n", rn.first.c_str(), rn.second.c_str());

    // Live workdir state vs the expected tree (base archive + embedded
    // patch): modified files, disappeared files, and untracked files.
    // compute_live_diff is the exact section status has always run here
    // (see the comment above it), including its snapshot materialization
    // and its "(continuing ...)" warnings.
    //
    // CONFLICTED-INTERIM TOLERANCE: when the recorded state carries
    // unresolved conflicts AND the expected-tree build fails with the
    // chain-moved failure class, the guidance the failure dies with is
    // circular — it names `rebase` and `commit`, both of which refuse
    // while conflicts are unresolved. So in that one state status prints
    // the recorded state (already above, exactly like the classic
    // conflicted state) plus a note that the live diff is unavailable,
    // and exits 0: the recorded entries are the truth the user needs to
    // resolve against. Every other failure class — and the chain-moved
    // class with no conflicts recorded — keeps the guidance die, as do
    // `diff` and every other compute_live_diff consumer (the erase-setup
    // check phase keeps the guidance die too, and fail-closes on the
    // no-diff classes it tolerates instead of reporting anything).
    LiveDiff live = compute_live_diff(
        ctx.pdir, rec.st, rec.emb, rec.workdir, ctx.statusfile, ctx.filebase,
        "status", /*tolerate_chain_moved=*/!rec.st.conflicts.empty());
    for (auto& m : live.modified)
        printf("Modified: %s\n", m.c_str());
    for (auto& d : live.disappeared)
        printf("Disappeared: %s\n", d.c_str());
    for (auto& u : live.untracked)
        printf("Untracked: %s\n", u.c_str());
    if (live.chain_moved)
        printf("projeny: the live diff is unavailable while the recorded "
               "patch does not apply onto the frozen base chain; resolve "
               "the conflicts and commit\n");
    return 0;
}

int cmd_diff_projeny(const std::string& projeny_arg)
{
    std::string pj = resolve_projeny_path(projeny_arg, "diff");
    Ctx ctx = resolve_ctx(pj);
    StatusData st = require_status_matches(ctx);
    require_no_conflicts(st, "cannot diff with unresolved conflicts");
    ProjenyFile cur = ProjenyFile::parse_bytes(st.embedded,
                                               "'" + ctx.projeny_arg + "'");
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then keep the hard
        // error — there is nothing to diff against.
        disregard_stale_state_for(ctx, cur, {cur.archive});
        die("workdir '" + workdir + "' is missing; run setup first");
    }

    // Validate pending ops exactly like commit: this diff must describe a
    // state commit would accept, so pending bookkeeping must match reality.
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

    // Baseline: what a FRESH `projeny setup` of the CURRENT .projeny file
    // would check out (the archive plus its patch). Like status, prefer the
    // snapshot — the byte-exact copy of what the last setup actually used,
    // which survives git deleting the archive — and fall back to the
    // archive itself, snapshotting it on the way. resolve_status_archive
    // dies with recovery guidance when neither file exists. A
    // Projeny:-based project has no archive: its baseline is the committed
    // extraction of the frozen base (its snapshot, else the live base file
    // frozen into it) plus the patch.
    TempDir tmp(make_scratch(ctx.pdir, ctx.filebase, "diff"));
    std::string Etree;
    if (cur.is_projeny_based()) {
        // Tolerant: the frozen-chain-moved state bricks the diff exactly
        // like status — die with the recovery guidance instead of the bare
        // apply report.
        try {
            TolerantPatchApply tol;
            Etree = build_projeny_tree(
                ctx.projeny_arg, cur,
                resolve_committed_base_bytes(ctx.projeny_arg, cur,
                                             "diff '" + ctx.projeny_arg + "'"),
                cur.patch,
                "patch in '" + ctx.projeny_arg + "' (base '" + cur.base + "')",
                tmp.path, "diff '" + ctx.projeny_arg + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("diff", ctx.projeny_arg);
        }
    } else {
        std::string archive_path = resolve_status_archive(
            ctx.pdir, cur, "diff '" + ctx.projeny_arg + "'");
        Etree = build_tree_from_patch(
            tmp, archive_path, cur.origname, cur.name, cur.patch,
            "patch in '" + ctx.projeny_arg + "'");
    }

    // Pending ops decide which changes count: adds (and move destinations)
    // on the add side, removals (and move sources) on the delete side.
    // committed_patch stays null: the expected tree already contains the
    // committed renames, so every pending rename source names an E path
    // directly and needs no unwinding.
    std::vector<std::string> add_keep, del_keep;
    for (const auto& a : st.added)
        add_keep.push_back(a);
    for (const auto& r : st.removed)
        del_keep.push_back(r);
    for (const auto& rn : st.renamed) {
        del_keep.push_back(rn.first);
        add_keep.push_back(rn.second);
    }
    std::vector<std::string> disappeared;
    // Frozen-mtime headers ride on modified frozen files so the printed
    // diff matches what commit would store for them. No attribute-only
    // blocks here (frozen_attribute_blocks stays false): a frozen mtime is
    // not a local change, so a clean checkout of a frozen project must
    // still diff empty.
    std::map<std::string, uint64_t> frozen =
        vcs_frozen_mtimes(cur.patch, cur.name);
    VcsDiffOpts dopts;
    dopts.forced_renames = &st.renamed;
    dopts.add_keep = &add_keep;
    dopts.delete_keep = &del_keep;
    dopts.disappeared = &disappeared;
    dopts.frozen_mtimes = &frozen;
    std::string raw = vcs_diff_trees_ex(Etree, workdir, cur.name, dopts);

    // Files that vanished without `projeny rm` are unregistered deletions:
    // they never reach the diff (the next setup would restore them), but
    // staying silent would make an empty output look like "no local
    // changes" instead of "some changes were ignored", so warn with the
    // exact command that registers each one. The path is spelled
    // wid-prefixed ("<name>/<rel>"): add/rm/mv accept that form from any
    // CWD, while the bare workdir-relative spelling only resolves when the
    // CWD is the workdir itself (everywhere else it dies with "outside the
    // workdir"). These go to stderr; stdout carries only the patch.
    for (const auto& rel : disappeared) {
        warn("'" + rel + "' was removed locally but is not marked with "
             "'projeny rm " +
             ctx.projeny_arg + " " + cur.name + "/" + rel +
             "'; it will not appear in the diff");
    }

    std::string patch = normalize_patch_text(raw);
    if (!patch.empty())
        fwrite(patch.data(), 1, patch.size(), stdout);
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

// Pick the wid to apply `patch` with inside `dir`: candidates are (in
// order) `prefer_base` ? {base_wid, patch_wid_hint(patch), ""} :
// {patch_wid_hint(patch), base_wid, ""} (deduped, an empty hint skipped),
// scored by how many patch_touched_paths exist under `dir`; strictly
// greater count wins, ties keep the earlier candidate. An empty patch
// keeps the first candidate.
std::string pick_patch_wid(const std::string& dir, const std::string& patch,
                           const std::string& base_wid, bool prefer_base)
{
    std::vector<std::string> candidates;
    auto push = [&candidates](const std::string& c) {
        if (std::find(candidates.begin(), candidates.end(), c) ==
            candidates.end())
            candidates.push_back(c);
    };
    std::string hint = patch_wid_hint(patch);
    if (prefer_base) {
        push(base_wid);
        if (!hint.empty())
            push(hint);
    } else {
        if (!hint.empty())
            push(hint);
        push(base_wid);
    }
    push("");
    std::string use_wid = candidates[0];
    if (!normalize_patch_text(patch).empty()) {
        size_t best = 0;
        bool have = false;
        for (auto& cand : candidates) {
            size_t hits = 0;
            for (auto& rel : patch_touched_paths(patch, cand, dir)) {
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
    return use_wid;
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
        // The same block shapes the parser recognizes count here
        // (git-style headers and plain unified ---/+++ blocks alike), so
        // the gate cannot pass something parse_patch would no-op.
        if (!vcs_patch_has_diff_blocks(patch))
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
    std::string use_wid = pick_patch_wid(dir, patch, wid, false);
    std::vector<std::string> conflicts;
    // kAllowAlready: an arbitrary target directory may genuinely already
    // hold (part of) the patch, and skipping those blocks is what makes
    // re-running `projeny patch` idempotent.
    bool clean = apply_patch_with_conflicts(dir, patch, use_wid, &conflicts,
                                            VcsApplyMode::kAllowAlready);
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

int cmd_apply(const std::string& projeny_arg, const std::string& patch_file)
{
    // Absolutize before any workdir mutation (like cmd_rm/cmd_mv): applying
    // a patch can create and delete directories, possibly including one the
    // process's CWD sits under, and every later path — the status file
    // write included — must not need the CWD again.
    std::string pj = absolutize(resolve_projeny_path(projeny_arg, "apply"));
    Ctx ctx = resolve_ctx(pj);
    if (!path_exists(ctx.statusfile))
        die("status file '" + ctx.statusfile + "' is missing; run setup first");
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    StatusData st = StatusData::parse(ctx.statusfile);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir)) {
        // The checkout directory is gone: the status and snapshot files are
        // stale state. Disregard them (warn + rename), then hard-error —
        // there is no workdir to apply anything to.
        disregard_stale_state_for(ctx, cur, {cur.archive});
        die("workdir '" + workdir + "' is missing; run setup first");
    }
    std::string patch = read_file_bytes(patch_file);
    if (!normalize_patch_text(patch).empty()) {
        // Refuse garbage input early: a non-empty patch file must hold at
        // least one diff block, otherwise a typo'd path would "succeed".
        // The same block shapes the parser recognizes count here
        // (git-style headers and plain unified ---/+++ blocks alike), so
        // the gate cannot pass something parse_patch would no-op.
        if (!vcs_patch_has_diff_blocks(patch))
            die("patch file '" + patch_file + "' contains no diff blocks");
    } else {
        // An empty (or whitespace-only) patch file changes nothing: report
        // success without touching the status file.
        printf("projeny: applied '%s' to '%s'\n", patch_file.c_str(),
               workdir.c_str());
        return 0;
    }
    // The patch may label files with any wid (its source checkout's name)
    // or with none (a plain git diff); prefer the checkout's own name on
    // ties, since the patch is being applied inside this checkout.
    std::string use_wid = pick_patch_wid(workdir, patch, cur.name, true);

    // Pre-flight, before anything is mutated: refuse when the patch would
    // touch a file that is already marked conflicted. Those need hand
    // resolution first (re-applying over half-resolved markers would bake
    // them into the result), so the whole command does nothing at all —
    // no filesystem changes and no status write — when any touched path is
    // conflicted.
    {
        std::vector<std::string> hit;
        for (auto& rel : patch_touched_paths(patch, use_wid, workdir)) {
            if (std::find(st.conflicts.begin(), st.conflicts.end(), rel) !=
                st.conflicts.end())
                hit.push_back(rel);
        }
        if (!hit.empty()) {
            sort_unique(&hit);
            die("cannot apply '" + patch_file +
                    "': the patch touches files that are already marked "
                    "conflicted",
                bullet_list(hit));
        }
    }

    // Classify the patch so the status file can record what it does. The
    // rename pairs mirror what the applier moves; the pure adds are the
    // add-paths list minus rename destinations; the deletes exclude
    // renames.
    // The tree is threaded so prefix-free plain-diff labels resolve with
    // the GNU patch -p1 fallback: the status file must record the paths
    // the applier actually touches (e.g. `sub/new.txt`, not
    // `oldtree/sub/new.txt`).
    std::vector<std::pair<std::string, std::string>> rens =
        vcs_rename_pairs(patch, use_wid, workdir);
    std::vector<std::string> adds;
    for (auto& a : vcs_add_paths(patch, use_wid, workdir)) {
        bool rename_dst = false;
        for (auto& r : rens) {
            if (r.second == a) {
                rename_dst = true;
                break;
            }
        }
        if (!rename_dst)
            adds.push_back(a);
    }
    std::vector<std::string> dels = vcs_deleted_paths(patch, use_wid, workdir);

    std::vector<std::string> conflicts;
    // kAllowAlready: the checkout may genuinely already hold (part of) the
    // patch, and skipping those blocks is what makes re-running
    // `projeny apply` idempotent.
    bool clean = apply_patch_with_conflicts(workdir, patch, use_wid,
                                            &conflicts,
                                            VcsApplyMode::kAllowAlready);
    sort_unique(&conflicts);

    // Post-apply bookkeeping, mirroring what add/rm/mv record by hand:
    // pending added/removed/renamed entries make the next `commit` fold
    // the patch into the .projeny file. Every step checks the post-apply
    // workdir, so failed (conflicted) blocks keep their old marks.
    std::vector<std::string> newly_added, newly_removed;
    std::vector<std::pair<std::string, std::string>> newly_renamed;
    auto in_added = [&st](const std::string& p) {
        return std::find(st.added.begin(), st.added.end(), p) != st.added.end();
    };
    auto in_removed = [&st](const std::string& p) {
        return std::find(st.removed.begin(), st.removed.end(), p) !=
               st.removed.end();
    };
    // Pure adds: the file exists again, so record it as added (un-removing
    // it first, like `projeny add` does).
    for (auto& a : adds) {
        if (!path_exists(join_path(workdir, a)))
            continue; // paranoia: the add failed as a conflict
        st.removed.erase(std::remove(st.removed.begin(), st.removed.end(), a),
                         st.removed.end());
        if (!in_added(a)) {
            st.added.push_back(a);
            newly_added.push_back(a);
        }
    }
    // Pure deletes: a failed delete keeps the file on disk (it is already
    // recorded as a conflict above), so only really-gone paths are marked.
    for (auto& d : dels) {
        if (path_exists(join_path(workdir, d)))
            continue; // conflicted delete: the file survived
        bool was_added = in_added(d);
        st.added.erase(std::remove(st.added.begin(), st.added.end(), d),
                       st.added.end());
        // Rename hygiene: a pending rename whose destination is now gone
        // collapses — the renamed-away source is truly gone, so it takes
        // the removal mark; a pending rename whose source is d is dropped
        // too (its destination's disappearance is the delete).
        std::vector<std::pair<std::string, std::string>> kept;
        for (auto& rn : st.renamed) {
            if (rn.second == d) {
                if (!in_added(rn.first) && !in_removed(rn.first)) {
                    st.removed.push_back(rn.first);
                    newly_removed.push_back(rn.first);
                }
            } else if (rn.first != d) {
                kept.push_back(rn);
            }
        }
        st.renamed = kept;
        if (!was_added && !in_removed(d)) {
            st.removed.push_back(d);
            newly_removed.push_back(d);
        }
    }
    // Renames: record only ones that actually happened (the source is gone
    // and the destination exists); a failed rename is a conflict, already
    // recorded. Chaining mirrors `projeny mv` onto a pending rename.
    for (auto& r : rens) {
        const std::string& s = r.first;
        const std::string& t = r.second;
        if (path_exists(join_path(workdir, s)) ||
            !path_exists(join_path(workdir, t)))
            continue;
        // A patch may rename through a path it creates and consumes
        // within the same run (blocks "a -> b" then "b -> c"): by
        // bookkeeping time both moves have happened, so the intermediate
        // pair fails the liveness check above and only the last block's
        // destination is alive. Follow the patch's own pairs backwards
        // from the live destination to the true origin. An earlier pair
        // re-keys the source only when its own source is gone too — the
        // fingerprint of a move that actually happened; a blocked or
        // already rename keeps its source in place and must not be
        // re-keyed (the liveness test would then pin a rename onto a
        // file that is still there).
        std::string src = s;
        {
            std::vector<std::string> seen{src};
            bool moved = true;
            while (moved) {
                moved = false;
                for (auto& q : rens) {
                    if (q.second != src || q.first == src)
                        continue;
                    if (std::find(seen.begin(), seen.end(), q.first) !=
                        seen.end())
                        continue;
                    if (path_exists(join_path(workdir, q.first)))
                        continue;
                    src = q.first;
                    seen.push_back(src);
                    moved = true;
                    break;
                }
            }
        }
        bool was_added = in_added(src);
        if (was_added) {
            st.added.erase(std::remove(st.added.begin(), st.added.end(), src),
                           st.added.end());
            if (!in_added(t)) {
                st.added.push_back(t);
                newly_added.push_back(t);
            }
        } else {
            // If src was itself a rename destination, chain to the original.
            std::string orig = src;
            std::vector<std::pair<std::string, std::string>> kept;
            for (auto& rn : st.renamed) {
                if (rn.second == src)
                    orig = rn.first;
                else
                    kept.push_back(rn);
            }
            st.renamed = kept;
            // Re-applying an already-recorded rename must not duplicate
            // the pair (idempotence): when the destination is already the
            // destination of a pending rename, this patch's rename either
            // is that exact pair or was folded into it by a previous
            // apply (the chain above re-keyed it), so there is nothing
            // left to record.
            bool recorded = false;
            for (auto& rn : st.renamed) {
                if (rn.second == t) {
                    recorded = true;
                    break;
                }
            }
            if (orig != t && !recorded) {
                st.renamed.push_back({orig, t});
                newly_renamed.push_back({orig, t});
            }
        }
        // Moving onto a pending-removed path un-removes it.
        st.removed.erase(std::remove(st.removed.begin(), st.removed.end(), t),
                         st.removed.end());
    }
    // Conflicts: union in and keep the canonical sorted form.
    for (auto& c : conflicts) {
        if (std::find(st.conflicts.begin(), st.conflicts.end(), c) ==
            st.conflicts.end())
            st.conflicts.push_back(c);
    }
    sort_unique(&st.conflicts);
    write_status(ctx, st);
    for (auto& a : newly_added)
        printf("projeny: marked '%s' as added\n", a.c_str());
    for (auto& r : newly_removed)
        printf("projeny: marked '%s' as removed\n", r.c_str());
    for (auto& r : newly_renamed)
        printf("projeny: marked '%s' -> '%s' as renamed\n", r.first.c_str(),
               r.second.c_str());
    if (clean) {
        printf("projeny: applied '%s' to '%s'\n", patch_file.c_str(),
               workdir.c_str());
        return 0;
    }
    printf("projeny: applied '%s' to '%s' with %zu conflict(s):\n",
           patch_file.c_str(), workdir.c_str(), conflicts.size());
    for (auto& c : conflicts)
        printf("  %s\n", c.c_str());
    return 1;
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
// The project argument goes through resolve_projeny_path (above), like
// every other project-taking command.

namespace {

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
            if (prefix.empty() || prefix == "." || prefix == "..")
                die("bad package output name '" + output + "'");
            return ArchiveKind{e.comp, prefix};
        }
    }
    die("cannot determine archive format from '" + output +
        "': expected one of .tar, .tar.gz/.tgz, .tar.bz2/.tbz2/.tbz, "
        ".tar.xz/.txz, .tar.zst/.tzst");
    return ArchiveKind{};
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
                if (is_tracked_rel(rel, fresh_root, st))
                    die("cannot package '" + rel +
                        "': unsupported file type; only regular files, "
                        "symlinks and directories are supported");
                continue; // untracked special file: leave it out
            }
            if (!is_tracked_rel(rel, fresh_root, st))
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

// Shared package/extract preamble: refuse upfront when a previous setup
// left unresolved conflicts (re-running setup here would otherwise re-merge
// the marker lines as ordinary content and silently clear the conflict
// list), run setup, refuse again when the setup itself ended conflicted,
// and require the .projeny/status match in between. `verb`/`verb_ing` spell
// the command for the messages ("package"/"packaging"). Returns 0 and fills
// *st_out with the post-setup status when the command may proceed; otherwise
// returns the rc the caller should propagate.
int package_extract_preamble(const std::string& pj, const char* verb,
                             const char* verb_ing, StatusData* st_out)
{
    std::string display = rel_to_cwd(pj);
    {
        Ctx pre_ctx = resolve_ctx(pj);
        if (path_exists(pre_ctx.statusfile)) {
            StatusData pre = StatusData::parse(pre_ctx.statusfile);
            require_no_conflicts(
                pre, std::string("cannot ") + verb + " '" + display +
                         "' with unresolved conflicts from a previous setup; "
                         "fix them and `projeny resolve` each file first");
        }
    }
    int rc = cmd_setup(pj);
    if (rc != 0) {
        warn(std::string("not ") + verb_ing + " '" + display +
             "': setup reported conflicts");
        return rc;
    }
    Ctx ctx = resolve_ctx(pj);
    *st_out = StatusData::parse(ctx.statusfile);
    require_no_conflicts(*st_out, std::string("cannot ") + verb + " '" +
                                      display +
                                      "' with unresolved conflicts");
    return 0;
}

int cmd_package(const std::string& projeny_arg, const std::string& output)
{
    // Absolutize up front: this command replaces the workdir, which can
    // delete the directory the process's CWD sits in (`projeny package ..`
    // from a workdir subdirectory); every later path must not need the
    // CWD again.
    std::string pj = absolutize(resolve_projeny_path(projeny_arg, "package"));
    ArchiveKind kind = classify_package_output(output); // fail fast on bad names
    StatusData st;
    int rc = package_extract_preamble(pj, "package", "packaging", &st);
    if (rc != 0)
        return rc;
    Ctx ctx = resolve_ctx(pj);
    ProjenyFile cur = ProjenyFile::parse(pj);
    std::string workdir = join_path(ctx.pdir, cur.name);

    // Fresh reference tree: what "tracked" means right now. For a
    // Projeny:-based project this is the committed extraction of the frozen
    // base (the setup that just ran froze/re-asserted it) plus the patch.
    TempDir tref(make_scratch(ctx.pdir, ctx.filebase, "pkg-ref"));
    std::string ref;
    if (cur.is_projeny_based()) {
        // Tolerant with recovery guidance, like every frozen-chain build.
        try {
            TolerantPatchApply tol;
            ref = build_projeny_tree(
                ctx.projeny_arg, cur,
                resolve_committed_base_bytes(ctx.projeny_arg, cur,
                                             "package '" + pj + "'"),
                cur.patch, "patch in '" + pj + "' (base '" + cur.base + "')",
                tref.path, "package '" + pj + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("package", ctx.projeny_arg);
        }
    } else {
        ref = build_tree_from_patch(
            tref, materialize_archive(ctx.pdir, cur, "package '" + pj + "'"),
            cur.origname, cur.name, cur.patch, "patch in '" + pj + "'");
    }

    TempDir tstage(make_scratch(ctx.pdir, ctx.filebase, "pkg"));
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
    printf("projeny: packaged %zu file(s) from '%s' into '%s' (%s/)\n",
           count, rel_to_cwd(workdir).c_str(), output.c_str(),
           kind.prefix.c_str());
    return 0;
}

int cmd_extract(const std::string& projeny_arg, const std::string& dest_dir)
{
    // Absolutize up front: this command replaces the workdir, which can
    // delete the directory the process's CWD sits in (`projeny extract ..`
    // from a workdir subdirectory); every later path must not need the
    // CWD again.
    std::string pj = absolutize(resolve_projeny_path(projeny_arg, "extract"));
    StatusData st;
    int rc = package_extract_preamble(pj, "extract", "extracting", &st);
    if (rc != 0)
        return rc;
    Ctx ctx = resolve_ctx(pj);
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

    TempDir tref(make_scratch(ctx.pdir, ctx.filebase, "ext-ref"));
    std::string ref;
    if (cur.is_projeny_based()) {
        // Tolerant with recovery guidance, like every frozen-chain build.
        try {
            TolerantPatchApply tol;
            ref = build_projeny_tree(
                ctx.projeny_arg, cur,
                resolve_committed_base_bytes(ctx.projeny_arg, cur,
                                             "extract '" + pj + "'"),
                cur.patch, "patch in '" + pj + "' (base '" + cur.base + "')",
                tref.path, "extract '" + pj + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("extract", ctx.projeny_arg);
        }
    } else {
        ref = build_tree_from_patch(
            tref, materialize_archive(ctx.pdir, cur, "extract '" + pj + "'"),
            cur.origname, cur.name, cur.patch, "patch in '" + pj + "'");
    }

    size_t count = 0;
    stage_tracked(workdir, ref, st, "", dest, &count);
    printf("projeny: extracted %zu file(s) from '%s' to '%s'\n", count,
           rel_to_cwd(workdir).c_str(), dest.c_str());
    return 0;
}

// ---- parallel multi-project commands ----
//
// setup/package/extract accept several projects and download/download
// accepts URL HASH pairs. The multi-project commands run in two phases
// (see parallel-projeny.txt): first every .projeny file is read to collect
// its URL: headers, shared archive basenames download ONCE as one batch
// (curl multi, -c/--curl-jobs transfers in flight; blake3 checks on
// -j/--jobs threads; one retry pass), then the per-project work runs on at
// most -j/--jobs threads. A single-project invocation takes the legacy
// fast path: the plain single-project command, byte-identically.

namespace {

// One source of "URL: <url> <hash>" lines for one archive package: a
// contributing .projeny file (the multi-command planning phase) or one
// command-line URL HASH pair (`projeny download`). `name` is the display
// name for the loud shared-package warnings: the .projeny path, or the URL.
struct PkgUrlSource {
    std::string name;
    std::vector<ProjenyUrl> urls;
};

// Candidate URL order for one package: the URLs every source lists first
// (first-seen order), then the remaining URLs (first-seen order). Common
// URLs try first so mirrors of the same archive are preferred — the batch
// scheduler walks candidates in order.
std::vector<std::string> candidate_urls(const std::vector<PkgUrlSource>& sources)
{
    std::vector<std::string> order;        // first-seen URL order
    std::map<std::string, size_t> per_url; // url -> sources listing it
    std::set<std::string> seen;
    for (const PkgUrlSource& s : sources) {
        std::set<std::string> mine;
        for (const ProjenyUrl& u : s.urls) {
            if (!mine.insert(u.url).second)
                continue; // a repeated URL line counts once per source
            if (seen.insert(u.url).second)
                order.push_back(u.url);
            ++per_url[u.url];
        }
    }
    std::vector<std::string> out;
    for (const std::string& u : order)
        if (per_url[u] == sources.size())
            out.push_back(u);
    for (const std::string& u : order)
        if (per_url[u] != sources.size())
            out.push_back(u);
    return out;
}

// The two loud shared-package warnings (all caps, per the spec), shared by
// the planning phase and `projeny download` so the wording stays identical:
// one when the sources of one package name different URL sets, and a louder
// one — per affected URL — when the same URL is listed with different
// blake3 hashes. Both only warn: the download is deduplicated by archive
// name either way and every source receives the same archive.
void warn_shared_package(const std::string& pkg,
                         const std::vector<PkgUrlSource>& sources)
{
    if (sources.size() < 2)
        return;
    // Different URL sets among the contributors of one package?
    bool differ = false;
    {
        std::set<std::string> first;
        for (const ProjenyUrl& u : sources[0].urls)
            first.insert(u.url);
        for (size_t i = 1; i < sources.size() && !differ; ++i) {
            std::set<std::string> mine;
            for (const ProjenyUrl& u : sources[i].urls)
                mine.insert(u.url);
            differ = mine != first;
        }
    }
    if (differ) {
        std::string files;
        for (size_t i = 0; i < sources.size(); ++i) {
            if (i > 0)
                files += "; ";
            files += sources[i].name;
        }
        warn("WARNING: " + std::to_string(sources.size()) +
             " PROJENY FILES DOWNLOAD ARCHIVES WITH THE SAME NAME '" + pkg +
             "' BUT WITH DIFFERENT URL SETS: " + files +
             ". DOWNLOADS ARE DEDUPLICATED BY ARCHIVE NAME, SO EVERY ONE OF "
             "THEM WILL GET THE SAME ARCHIVE FILE.");
    }
    // Same URL listed with different hashes across the contributors? One
    // line per affected URL, hashes (and the files listing them) in
    // first-seen order.
    std::vector<std::string> url_order;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>
        sightings; // url -> (hash, source name) in first-seen order
    {
        std::set<std::string> seen_urls;
        for (const PkgUrlSource& s : sources) {
            std::set<std::string> mine;
            for (const ProjenyUrl& u : s.urls) {
                if (seen_urls.insert(u.url).second)
                    url_order.push_back(u.url);
                if (!mine.insert(u.url).second)
                    continue; // a repeated URL line counts once per source
                sightings[u.url].emplace_back(u.hash, s.name);
            }
        }
    }
    for (const std::string& url : url_order) {
        const auto& seen = sightings[url];
        std::vector<std::string> hashes;
        std::string hash_list, files;
        for (const auto& sh : seen) {
            if (std::find(hashes.begin(), hashes.end(), sh.first) ==
                hashes.end()) {
                hashes.push_back(sh.first);
                if (!hash_list.empty())
                    hash_list += ", ";
                hash_list += sh.first;
            }
            if (!files.empty())
                files += "; ";
            files += sh.second;
        }
        if (hashes.size() < 2)
            continue;
        warn("WARNING: URL '" + url +
             "' IS LISTED WITH DIFFERENT BLAKE3 HASHES (" + hash_list +
             ") IN " + files +
             ". THIS IS ALMOST CERTAINLY A MISTAKE. ALL OF THESE FILES WILL "
             "RECEIVE THE SAME DOWNLOADED ARCHIVE.");
    }
}

// One requested per-project operation: the .projeny argument exactly as
// given on the command line, plus the package output / extract destination
// ("" for setup).
struct MultiJob {
    std::string projeny_arg;
    std::string extra;
};

// The shared multi-project body. `verb` names the command for the summary
// line ("setup(s) failed"), `verb_phrase` spells the dedupe warning
// ("setting it up"), `extra_noun` names the dropped duplicate's second
// argument ("output"/"destination", "" for setup), and run_one runs the
// single-project command: cmd_setup(arg) / cmd_package(arg, extra) /
// cmd_extract(arg, extra).
int run_multi(const char* verb, const char* verb_phrase,
              const char* extra_noun,
              const std::function<int(const std::string&, const std::string&)>&
                  run_one,
              const std::vector<MultiJob>& requested, int jobs, int curl_jobs)
{
    // Legacy fast path: exactly one project means exactly the single-project
    // command — byte-identical output, no planning phase, no labels, and no
    // resolution duplication (the command resolves the argument itself).
    if (requested.size() == 1)
        return run_one(requested[0].projeny_arg, requested[0].extra);

    // 1. Resolve + dedupe: two spellings of one .projeny file must never run
    // concurrently (and never twice). The FIRST occurrence wins.
    struct Resolved {
        std::string arg;  // as given
        std::string extra;
        std::string abs;  // absolutized .projeny path
    };
    std::vector<Resolved> projects;
    std::set<std::string> seen;
    projects.reserve(requested.size());
    for (const MultiJob& j : requested) {
        std::string abs = absolutize(resolve_projeny_path(j.projeny_arg, verb));
        if (!seen.insert(abs).second) {
            std::string dropped =
                extra_noun[0] ? " (the " + std::string(extra_noun) + " '" +
                                    j.extra + "' is ignored)"
                              : "";
            warn("'" + j.projeny_arg + "' is listed more than once; " +
                 verb_phrase + " only once" + dropped);
            continue;
        }
        projects.push_back({j.projeny_arg, j.extra, abs});
    }

    // 1b. Parallel package/extract only: two DIFFERENT projects whose
    // outputs (package) or destinations (extract) resolve to the same path
    // would race on one file — each worker writes or renames the full
    // result, so which bytes survive depends on scheduling. Absolutize each
    // output/destination (lexical absolutize() is fine for
    // not-yet-existing paths), group by the absolute path, and warn once
    // per colliding group. Setup has no second argument and cannot collide.
    // The runs still proceed: this is a loud heads-up, not a refusal.
    if (extra_noun[0] != '\0') {
        std::map<std::string, std::vector<size_t>> collisions;
        for (size_t i = 0; i < projects.size(); ++i)
            collisions[absolutize(projects[i].extra)].push_back(i);
        for (const auto& kv : collisions) {
            if (kv.second.size() < 2)
                continue;
            std::string listed;
            for (size_t k = 0; k < kv.second.size(); ++k) {
                if (!listed.empty())
                    listed +=
                        (k + 1 == kv.second.size()) ? " and " : ", ";
                listed += "'" + projects[kv.second[k]].extra + "'";
            }
            warn(std::string(verb) + " " + extra_noun + "s " + listed +
                 (kv.second.size() == 2 ? " both" : " all") +
                 " resolve to '" + kv.first +
                 "'; the parallel runs write the same file");
        }
    }

    // 2. Collect URLs (the download-planning phase: read every projeny file
    // up front). A file that cannot be read or parsed, or one with git
    // conflict markers, contributes nothing here: its per-project task dies
    // with the canonical message or handles the conflicts itself — and as a
    // deferred file it may fall back to a serialized download in the guard.
    std::map<std::string, std::vector<PkgUrlSource>> collect;
    for (const Resolved& r : projects) {
        std::string raw;
        if (!try_read_file_bytes(r.abs, &raw))
            continue;
        if (projeny_has_conflict_markers(raw))
            continue;
        ProjenyFile pf;
        try {
            pf = ProjenyFile::parse_bytes(raw, "'" + r.abs + "'");
        } catch (const ProjenyFatalError&) {
            continue; // the per-project task reproduces this error
        }
        if (!pf.is_url_based() || pf.archive.empty())
            continue; // classic Archive: and Projeny:-based projects have
                      // nothing to download here: a Projeny:-based file's
                      // base file is fetched on demand during its OWN setup
                      // (or is already frozen in its snapshot), never by
                      // the batch that merely names it
        PkgUrlSource src;
        src.name = r.abs;
        src.urls = pf.urls;
        collect[pf.archive].push_back(std::move(src));
    }

    // 3-5. Build the plan: loud warnings for shared packages, candidate URL
    // order, contributing pdirs, and the existing-snapshot check (a pdir
    // whose .<archive>.snapshot already verifies against any accepted hash
    // needs no download at all).
    std::map<std::string, PlannedPackage> plan;
    std::vector<BatchPackageSpec> specs;
    for (const auto& kv : collect) {
        const std::string& pkg = kv.first;
        const std::vector<PkgUrlSource>& sources = kv.second;
        warn_shared_package(pkg, sources);
        PlannedPackage p;
        p.urls = candidate_urls(sources);
        for (const PkgUrlSource& s : sources)
            for (const ProjenyUrl& u : s.urls)
                p.accepted_hashes.insert(u.hash);
        std::set<std::string> dirs;
        for (const PkgUrlSource& s : sources)
            dirs.insert(dirname_of(s.name));
        p.pdirs.assign(dirs.begin(), dirs.end());
        bool all_satisfied = true;
        for (const std::string& dir : p.pdirs) {
            // Same construction try_ensure_url_snapshot uses:
            // <pdir>/.<archive>.snapshot.
            std::string snap = snapshot_path_for(join_path(dir, pkg));
            bool satisfied = false;
            if (is_readable_file(snap)) {
                std::string h = blake3_file_hash_hex(snap);
                if (p.accepted_hashes.count(h) > 0) {
                    satisfied = true;
                    p.verified_hashes.insert(h);
                }
            }
            p.pdir_satisfied.push_back(satisfied);
            all_satisfied = all_satisfied && satisfied;
        }
        plan[pkg] = std::move(p);
        if (!all_satisfied) {
            BatchPackageSpec spec;
            spec.name = pkg;
            spec.urls = plan[pkg].urls;
            spec.accepted_hashes = plan[pkg].accepted_hashes;
            specs.push_back(std::move(spec));
        }
    }

    // 6. Download: one batch for every package that still needs it (never
    // two concurrent downloads of the same package — download_batch
    // guarantees one handle per package), then the verified bytes go into
    // each unsatisfied pdir's snapshot.
    if (!specs.empty()) {
        std::vector<BatchPackageResult> results =
            download_batch(specs, curl_jobs, jobs);
        for (size_t i = 0; i < specs.size(); ++i) {
            PlannedPackage& p = plan[specs[i].name];
            p.attempted = true;
            const BatchPackageResult& r = results[i];
            if (!r.ok) {
                p.errors = r.errors;
                std::string joined;
                for (size_t k = 0; k < r.errors.size(); ++k) {
                    if (k > 0)
                        joined += "; ";
                    joined += r.errors[k];
                }
                warn("failed to obtain '" + r.name + "': " + joined);
                continue;
            }
            p.ok = true;
            p.verified_hashes.insert(r.hash);
            for (size_t k = 0; k < p.pdirs.size(); ++k) {
                if (p.pdir_satisfied[k])
                    continue;
                write_file_bytes(snapshot_path_for(join_path(p.pdirs[k],
                                                             r.name)),
                                 r.data);
            }
        }
    }

    // 7. Per-project phase: every project on its own thread (at most `jobs`
    // at a time), labeled so its note/warn/die lines name the project. The
    // guard in try_ensure_url_snapshot reads the plan while this runs.
    std::vector<int> rcs(projects.size(), 0);
    std::vector<std::string> labels(projects.size());
    for (size_t i = 0; i < projects.size(); ++i)
        labels[i] = basename_of(projects[i].abs);
    g_multi_plan = &plan;
    struct PlanUninstall {
        ~PlanUninstall() { g_multi_plan = nullptr; }
    } plan_uninstall;
    run_parallel(jobs, projects.size(), [&](size_t i) {
        set_output_label(labels[i]);
        try {
            // The absolutized path: resolution happened once, above, so no
            // worker re-resolves a spelling after another worker may have
            // removed the directory the process's CWD sits in.
            rcs[i] = run_one(projects[i].abs, projects[i].extra);
        } catch (const ProjenyFatalError&) {
            // die() already printed the labeled report; do not repeat it.
            rcs[i] = 1;
        }
        set_output_label("");
    });

    // 8. Exit code: nonzero when any project failed (nonzero return or
    // death), with one summary line. Success prints nothing extra.
    size_t failed = 0;
    std::string failed_labels;
    for (size_t i = 0; i < projects.size(); ++i) {
        if (rcs[i] == 0)
            continue;
        ++failed;
        if (!failed_labels.empty())
            failed_labels += ", ";
        failed_labels += labels[i];
    }
    if (failed > 0)
        fprintf(stderr, "projeny: %zu of %zu %s(s) failed: %s\n", failed,
                projects.size(), verb, failed_labels.c_str());
    return failed > 0 ? 1 : 0;
}

} // namespace

int cmd_setup_multi(const std::vector<std::string>& projeny_args, int jobs,
                    int curl_jobs)
{
    std::vector<MultiJob> requested;
    requested.reserve(projeny_args.size());
    for (const std::string& a : projeny_args)
        requested.push_back({a, ""});
    return run_multi(
        "setup", "setting it up", "",
        [](const std::string& arg, const std::string&) {
            return cmd_setup(arg);
        },
        requested, jobs, curl_jobs);
}

int cmd_package_multi(
    const std::vector<std::pair<std::string, std::string>>& pairs, int jobs,
    int curl_jobs)
{
    std::vector<MultiJob> requested;
    requested.reserve(pairs.size());
    for (const auto& p : pairs)
        requested.push_back({p.first, p.second});
    return run_multi(
        "package", "packaging it", "output",
        [](const std::string& arg, const std::string& extra) {
            return cmd_package(arg, extra);
        },
        requested, jobs, curl_jobs);
}

int cmd_extract_multi(
    const std::vector<std::pair<std::string, std::string>>& pairs, int jobs,
    int curl_jobs)
{
    std::vector<MultiJob> requested;
    requested.reserve(pairs.size());
    for (const auto& p : pairs)
        requested.push_back({p.first, p.second});
    return run_multi(
        "extract", "extracting it", "destination",
        [](const std::string& arg, const std::string& extra) {
            return cmd_extract(arg, extra);
        },
        requested, jobs, curl_jobs);
}

// ---- erase-setup ----
//
// Delete what a `setup` created, for any number of projects in parallel
// (always through the parallel machinery — a new command has no legacy
// single-project fast path to preserve, and there is no download phase, so
// g_multi_plan stays untouched):
//
//   checkout dir   pdir/<Name>, removed recursively (rm -rf): uncommitted
//                  changes are discarded with it — that is the point
//   status file    .<f>.projeny.status (exactly resolve_ctx's canonical
//                  dotted form) plus the legacy undotted <f>.projeny.status
//                  when it exists — the same two-name pair the stale-state
//                  handling covers, so nothing survives under either name
//   setup journal  <f>.projeny.setup-journal, deleted silently: it is not
//                  worth a warning when absent, and leaving it would make
//                  the next setup run crash recovery against a checkout
//                  that no longer exists
//   snapshot       .<archive>.snapshot plus its legacy undotted form —
//                  only with --erase-snapshots, and ONLY the exact
//                  snapshot the next setup would pick up (the URL-derived
//                  basename for a URL: project, the Archive: value for a
//                  classic one): never a similarly named snapshot for a
//                  different version, and never the checked-in archive
//                  tarball itself, which is git-tracked
//
// A missing thing warns ("did not exist; nothing to erase") and counts as
// success; a deletion that cannot be completed prints an error naming the
// path, fails that project, and the remaining deletions still run (spec:
// "try to finish the rest of the deletion"). A run with failures ends with
// the standard one-line summary and exit 1, after every project finished.
//
// Without --force, NOTHING is erased before every project has passed a
// check phase: in parallel (subject to -j), each project is asked whether a
// commit would have anything to do — i.e. whether `projeny status` reports
// anything other than untracked files (recorded Conflict:/Added:/Removed:/
// Renamed: entries, or live-diff Modified:/Disappeared: files; untracked
// files alone are fine to blow away). The check reuses the exact machinery
// status uses, factored into read_recorded_status (pure) and
// compute_live_diff (the snapshot-materializing half; see the comment above
// them) — with two guards the informational command does not need:
//
//   - a missing workdir (or a status file under neither name) is clean and
//     never reaches the status machinery at all: status's workdir-missing
//     branch renames files (disregard_stale_state), which an erase check
//     must not do;
//   - compute_live_diff materializes its snapshot under
//     g_fallback_download_mu, so several checked projects sharing an
//     archive whose snapshot is missing can never materialize (download/
//     copy) it twice concurrently — only that step serializes; the untar,
//     patch, and byte-compare run in parallel subject to -j. Serialization
//     covers materialization only: it does not pin the snapshot's bytes.
//     Checked projects sharing an archive basename while disagreeing on
//     its expected hashes re-materialize the same snapshot path in turn,
//     and a loser's post-settlement write (atomic temp+rename) can swap
//     the bytes under a winner's unlocked tree build — fail-closed (see
//     the comment in compute_live_diff), never a wrong erase.
//
// All-clean runs proceed to the erase phase byte-identically to --force.
// Any dirty project refuses the WHOLE invocation — one die() report, exit
// 1, nothing erased (not even the clean projects' state). A project whose
// state cannot even be assessed (unreadable or unparseable .projeny —
// caught by the main-thread parse — a status computation that dies, e.g.
// a corrupt status file, or a live diff that cannot run because the
// archive and its snapshot are both missing or unusable) refuses the whole
// invocation the same way: the .projeny file names what would be deleted,
// and the status machinery names what would be lost, so a check that
// cannot run means erase-setup cannot know what it is about to destroy.
// A project that cannot be checked is never judged clean: fail closed,
// with --force as the only override.

namespace {

// Outcome of one erase-setup deletion.
enum class EraseOutcome {
    Missing, // nothing was there: warn (or stay silent) and succeed
    Erased,  // removed
    Failed   // could not be removed; *err names the path and the reason
};

// Delete one file-shaped path (a status file, snapshot, or setup journal)
// with unlink(2). These paths are expected to be plain files, so a
// directory sitting at one is NOT recursed into: the unlink fails (EISDIR,
// even for root) and the failure is reported — silently rm -rf'ing a
// directory someone put at a status-file path could destroy unrelated
// files. A missing path is Missing, and so is one a parallel sibling
// removed first (two projects sharing one snapshot): gone is gone.
EraseOutcome erase_file_item(const std::string& path, std::string* err)
{
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        if (errno == ENOENT)
            return EraseOutcome::Missing;
        *err = "cannot inspect '" + path + "': " + strerror(errno);
        return EraseOutcome::Failed;
    }
    if (unlink(path.c_str()) != 0) {
        if (errno == ENOENT)
            return EraseOutcome::Missing;
        *err = std::string("cannot remove '") + path + "': " +
               strerror(errno);
        return EraseOutcome::Failed;
    }
    return EraseOutcome::Erased;
}

// Delete one path the rm -rf way (the checkout: a directory tree, or
// whatever else sits at its Name). remove_recursive reports the first
// failing syscall's errno into *err while removing the rest.
EraseOutcome erase_tree_item(const std::string& path, std::string* err)
{
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) {
        if (errno == ENOENT)
            return EraseOutcome::Missing;
        *err = "cannot inspect '" + path + "': " + strerror(errno);
        return EraseOutcome::Failed;
    }
    std::string derr;
    if (!remove_recursive(path, &derr)) {
        *err = "cannot remove '" + path + "'" +
               (derr.empty() ? "" : ": " + derr);
        return EraseOutcome::Failed;
    }
    return EraseOutcome::Erased;
}

// Print one error line in die()'s exact format ("projeny: [<label>] error:
// <msg>") WITHOUT throwing: erase-setup reports each failed deletion and
// keeps deleting, and the failed project is only counted once its task is
// done. output_label() is this worker's project; the main thread (never a
// caller today) would print unlabeled, like die() there.
void print_erase_error(const std::string& msg)
{
    std::string prefix =
        output_label().empty() ? "" : "[" + output_label() + "] ";
    std::lock_guard<std::mutex> lk(g_output_mutex);
    fprintf(stderr, "projeny: %serror: %s\n", prefix.c_str(), msg.c_str());
}

// One resolved erase-setup request: the argument as given plus its
// absolutized .projeny path. Resolution happens once, on the main thread,
// so no worker re-resolves a spelling after another worker's deletion may
// have removed the directory the process's CWD sits in.
struct EraseJob {
    std::string arg;
    std::string abs;
};

// What one project erases, computed on the main thread from the parsed
// .projeny file. Empty path fields name nothing to do; a non-empty `error`
// fails the project before anything is deleted (the per-project task
// reports it under the project's label).
struct ErasePlan {
    std::string name;            // Name: header, for the notes
    std::string workdir;         // pdir/<Name>
    std::string status;          // .<f>.projeny.status (the canonical form)
    std::string legacy_status;   // <f>.projeny.status (deleted when present)
    std::string journal;         // <f>.projeny.setup-journal (silent)
    std::string snapshot;        // .<archive>.snapshot, or the frozen
                                 // .<stem>.snapshot of a Projeny:-based
                                 // project (--erase-snapshots)
    std::string legacy_snapshot; // <archive>.snapshot (when present; never
                                 // for the Projeny: form, which has no
                                 // legacy undotted snapshot)
    std::string error;           // non-empty: fail the project with this
};

// Erase one project's setup state. Returns 0, or 1 when any deletion
// failed — every remaining deletion still ran.
int erase_one_project(const ErasePlan& p)
{
    std::vector<std::string> erased; // "role 'path'" entries for the note
    bool failed = false;

    // The checkout, rm -rf.
    auto erase_tree_at = [&](const std::string& path) {
        std::string err;
        EraseOutcome o = erase_tree_item(path, &err);
        if (o == EraseOutcome::Missing) {
            warn("'" + rel_to_cwd(path) + "' did not exist; nothing to erase");
        } else if (o == EraseOutcome::Failed) {
            print_erase_error(err);
            failed = true;
        } else {
            erased.push_back("checkout '" + rel_to_cwd(path) + "'");
        }
    };

    // One file-shaped item. Missing paths warn (unless the item is deleted
    // silently) and count as success; failures report and fail the project
    // without stopping the remaining items.
    auto erase_file_at = [&](const std::string& path, const char* role,
                             bool warn_missing, bool in_note) {
        std::string err;
        EraseOutcome o = erase_file_item(path, &err);
        if (o == EraseOutcome::Missing) {
            if (warn_missing)
                warn("'" + rel_to_cwd(path) +
                     "' did not exist; nothing to erase");
        } else if (o == EraseOutcome::Failed) {
            print_erase_error(err);
            failed = true;
        } else if (in_note) {
            erased.push_back(std::string(role) + " '" + rel_to_cwd(path) +
                             "'");
        }
    };

    erase_tree_at(p.workdir);

    // The status file under both of its names: when both are gone, ONE
    // warning names the canonical dotted form (the same shape the stale
    // pair's warnings take).
    if (path_exists(p.status) || path_exists(p.legacy_status)) {
        erase_file_at(p.status, "status", false, true);
        erase_file_at(p.legacy_status, "status", false, true);
    } else {
        erase_file_at(p.status, "status", true, true);
    }

    // The crash-recovery setup journal: always attempted, never warned
    // about when absent, never in the note.
    erase_file_at(p.journal, "", false, false);

    // With --erase-snapshots: the exact snapshot the next setup would use,
    // under both of its names (migrate_snapshot would otherwise rename the
    // legacy form back into use). Same one-warning rule as the status.
    if (!p.snapshot.empty()) {
        if (path_exists(p.snapshot) || path_exists(p.legacy_snapshot)) {
            erase_file_at(p.snapshot, "snapshot", false, true);
            erase_file_at(p.legacy_snapshot, "snapshot", false, true);
        } else {
            erase_file_at(p.snapshot, "snapshot", true, true);
        }
    }

    if (failed)
        return 1;
    if (erased.empty()) {
        note("nothing to erase for '" + p.name + "'");
        return 0;
    }
    std::string list;
    for (size_t k = 0; k < erased.size(); ++k) {
        if (k > 0)
            list += ", ";
        list += erased[k];
    }
    note("erased setup state for '" + p.name + "' (" + list + ")");
    return 0;
}

// One project's no-force dirtiness assessment.
struct EraseCheck {
    bool failed = false; // the status machinery died, or the live diff could
                         // not run: nothing about the checkout can be assumed
    bool dirty = false;  // status reports something a commit would fold in
    std::string error;   // failed: the canonical error text. For a check that
                         // died this was already printed by die() under this
                         // project's label; for a live diff that could not
                         // run nothing was printed and this text is the only
                         // explanation (it appears in the refusal bullet).
    // dirty: every change a commit would fold in, as "kind: 'path'"
    // fragments in status's own vocabulary and print order (Conflict:,
    // Added:, Removed:, Renamed:, Modified:, Disappeared: — untracked files
    // are deliberately absent: they are not changes a commit would make).
    std::vector<std::string> changes;
};

// Assess one project for the check phase: would a commit have anything to
// do, i.e. does `projeny status` report anything other than untracked
// files? Read-only for the project's bookkeeping (the live-diff half may
// materialize a missing snapshot, exactly like status), and it never runs
// status's stale-state renaming. Dies (like status does) on a corrupt or
// unreadable status file, embedded copy, or patch — and records a check
// failure when the live diff cannot run at all (the snapshot the status
// records is missing or unusable, so the expected tree cannot be built) —
// the caller turns both into a whole-invocation refusal.
void assess_erase_check(const ErasePlan& p, const std::string& abs,
                        EraseCheck* out)
{
    // No status file under either name: nothing is tracked, so there is
    // nothing a commit could fold in — clean, without running the status
    // machinery. (resolve_ctx would rename a legacy form into place here;
    // the check only READS whichever form exists, leaving both for the
    // erase phase to delete under both names, as it always has.)
    std::string pdir = dirname_of(abs);
    std::string statusfile = p.status;
    if (!path_exists(statusfile)) {
        if (!path_exists(p.legacy_status))
            return;
        statusfile = p.legacy_status;
    }
    // Recorded state: pure (parse + stat only).
    RecordedStatus rec = read_recorded_status(pdir, abs, statusfile);
    // The checkout is gone: there is nothing to destroy, so there is
    // nothing to protect — clean (the erase phase will warn the workdir
    // missing). This deliberately skips the recorded entries too: status's
    // own missing-workdir branch treats them as stale state, and the erase
    // phase never runs the status machinery on a missing workdir.
    if (!is_dir(rec.workdir))
        return;
    // The live-diff half: materializes the snapshot the status records and
    // diffs the expected tree against the workdir. The checks run in
    // parallel subject to -j: compute_live_diff serializes only its
    // snapshot-materialization step internally (see the comment there) —
    // the one part that must never race across projects sharing an
    // archive — while the untar/patch/byte-compare runs concurrently. That
    // serialization does not pin a shared snapshot's bytes for projects
    // that disagree on the archive's hashes; the worst case is fail-closed
    // (see the comment in compute_live_diff).
    LiveDiff live = compute_live_diff(pdir, rec.st, rec.emb, rec.workdir,
                                      statusfile, basename_of(abs),
                                      "erase-setup");
    // The workdir is there, but the expected tree could not be built: the
    // snapshot the status records is missing or unusable (for a classic
    // project the checked-in archive is gone too, so it cannot be copied
    // back; for a URL project no URL could be downloaded or verified).
    // NOTHING is known about the checkout — the unassessable dirtiness the
    // compute above hides behind empty lists — so this project is never
    // clean: fail closed, like a check that died. The recorded entries
    // would still describe dirt, but the refusal needs none of it: the
    // unverifiable refusal (below) fires before the dirty one and erases
    // just as nothing.
    if (!live.ran) {
        out->failed = true;
        out->error =
            "cannot verify the checkout against its archive (the archive "
            "and its snapshot are missing or unusable); it cannot be "
            "checked for uncommitted changes";
        return;
    }
    // Dirtiness: any recorded pending op or conflict, or any live-diff
    // entry that is not "untracked".
    if (rec.st.conflicts.empty() && rec.st.added.empty() &&
        rec.st.removed.empty() && rec.st.renamed.empty() &&
        live.modified.empty() && live.disappeared.empty())
        return;
    out->dirty = true;
    for (const std::string& c : rec.st.conflicts)
        out->changes.push_back("conflict: '" + c + "'");
    for (const std::string& a : rec.st.added)
        out->changes.push_back("added: '" + a + "'");
    for (const std::string& r : rec.st.removed)
        out->changes.push_back("removed: '" + r + "'");
    for (const auto& rn : rec.st.renamed)
        out->changes.push_back("renamed: '" + rn.first + "' -> '" +
                               rn.second + "'");
    for (const std::string& m : live.modified)
        out->changes.push_back("modified: '" + m + "'");
    for (const std::string& d : live.disappeared)
        out->changes.push_back("disappeared: '" + d + "'");
}

// Build one project's erase plan from its (absolutized) .projeny path:
// read + parse the file, validate its Name: header, and fill in every path
// the erase deletes. On failure *error carries the canonical error text and
// the returned plan is empty (the caller refuses the project with it —
// die() has already printed the canonical report once, unlabeled, so the
// per-project reporting can reproduce it under the label).
ErasePlan make_erase_plan(const std::string& abs_file, bool erase_snapshots,
                          std::string* error)
{
    ErasePlan p;
    error->clear();
    std::string raw;
    if (!try_read_file_bytes(abs_file, &raw)) {
        p.error = "cannot read '" + abs_file +
                  "' (missing?); refusing to erase anything for it (the "
                  ".projeny file names what would be deleted)";
        *error = p.error;
        return p;
    }
    ProjenyFile pf;
    try {
        pf = ProjenyFile::parse_bytes(raw, "'" + abs_file + "'");
    } catch (const ProjenyFatalError& e) {
        // die() already printed the canonical report (unlabeled, like
        // the other parallel commands' planning phase); the per-project
        // task reproduces it under the label.
        p.error = e.message();
        *error = p.error;
        return p;
    }
    // Belt-and-suspenders (parse_bytes already rejects every one of
    // these with "bad Name: header"): never let a Name: that does not
    // name a single directory inside pdir turn into a deletion.
    if (pf.name.empty() || pf.name == "." || pf.name == ".." ||
        pf.name.find('/') != std::string::npos) {
        p.error = "cannot erase '" + abs_file +
                  "': its Name: header does not name a checkout "
                  "directory; refusing to delete anything";
        *error = p.error;
        return p;
    }
    std::string pdir = dirname_of(abs_file);
    p.name = pf.name;
    p.workdir = join_path(pdir, pf.name);
    p.status = dotname(abs_file) + ".status";
    p.legacy_status = abs_file + ".status";
    p.journal = abs_file + ".setup-journal";
    if (erase_snapshots) {
        // The exact snapshot the next setup would use: for a URL
        // project url_snapshot_path's dotted form, for a classic one
        // snapshot_path_for(pdir/<archive>) — the same formula, since
        // url_snapshot_path is snapshot_path_for(join_path(pdir,
        // pf.archive)) — and for a Projeny:-based project the frozen base
        // snapshot .<stem>.snapshot next to the .projeny file (the file
        // resolve_committed_base_bytes reads first), which has no legacy
        // undotted form. The checked-in archive tarball itself
        // (pdir/<archive>) is never touched, and neither is any
        // similarly named snapshot for a different version.
        if (pf.is_projeny_based()) {
            p.snapshot = projeny_snapshot_path_for(abs_file);
            p.legacy_snapshot = "";
        } else {
            std::string archive = join_path(pdir, pf.archive);
            p.snapshot = snapshot_path_for(archive);
            p.legacy_snapshot = legacy_snapshot_path_for(archive);
        }
    }
    return p;
}

// The single-project erase-setup core, shared with `create` (--force over
// an existing .projeny file, and the --origname corner guard's attributed
// sibling): build the plan, run the no-force check when force_level < 2
// (refusing with erase-setup's own wording — a dirty or uncheckable
// project erases nothing), and erase. force_level >= 2 skips the check,
// exactly like `erase-setup --force`. `cmd` names the calling command; the
// wording is erase-setup's own either way, so it is only carried for
// future diagnostics.
void erase_project_state(const std::string& abs_file, bool erase_snapshots,
                         int force_level, const char* cmd)
{
    (void)cmd;
    std::string error;
    ErasePlan p = make_erase_plan(abs_file, erase_snapshots, &error);
    std::string label = basename_of(abs_file);

    if (force_level < 2) {
        // A project whose .projeny cannot be read or parsed cannot be
        // assessed at all — refuse before any check runs, with the same
        // whole-invocation refusal one project's erase-setup prints.
        if (!error.empty())
            die("cannot check 1 of 1 project(s) for uncommitted changes; "
                "refusing to erase anything (use --force to erase anyway)",
                bullet_list({("'" + label + "': " + error)}));
        // The check: one labeled worker's worth of assess_erase_check, run
        // inline (there is exactly one project). A check that dies records
        // the failure instead of escaping: the refusal below names the
        // project again, exactly as the parallel form's does.
        EraseCheck check;
        set_output_label(label);
        try {
            assess_erase_check(p, abs_file, &check);
        } catch (const ProjenyFatalError& e) {
            check.failed = true;
            check.error = e.message();
        }
        set_output_label("");
        if (check.failed)
            die("cannot check 1 of 1 project(s) for uncommitted changes; "
                "refusing to erase anything (use --force to erase anyway)",
                bullet_list({("'" + label + "': " + check.error)}));
        if (check.dirty) {
            std::string line = "'" + label + "': ";
            for (size_t k = 0; k < check.changes.size(); ++k) {
                if (k > 0)
                    line += "; ";
                line += check.changes[k];
            }
            die("refusing to erase 1 of 1 project(s) with uncommitted "
                "changes (use --force to erase anyway)",
                bullet_list({line}));
        }
    } else if (!error.empty()) {
        // With --force the per-project phase dies under the label (the
        // canonical report already printed once, unlabeled, during the
        // planning above).
        set_output_label(label);
        try {
            die(error);
        } catch (...) {
            set_output_label("");
            throw;
        }
    }

    // The erase itself, labeled like the per-project phase: its warnings,
    // errors, and notes name the project.
    int rc = 0;
    set_output_label(label);
    try {
        rc = erase_one_project(p);
    } catch (const ProjenyFatalError&) {
        rc = 1;
    }
    set_output_label("");
    if (rc != 0) {
        // A deletion failed: erase-setup's own summary line, then stop —
        // a half-erased project is no base to create on top of. The
        // thrown message is never printed (die() printed the per-path
        // errors already); main() turns it into exit status 1.
        fprintf(stderr, "projeny: 1 of 1 erase-setup(s) failed: %s\n",
                label.c_str());
        throw ProjenyFatalError("erase-setup failed");
    }
}

} // namespace

int cmd_erase_setup_multi(const std::vector<std::string>& projeny_args,
                          int jobs, bool erase_snapshots, bool force)
{
    // 1. Resolve + dedupe, exactly like the other parallel commands: two
    // spellings of one .projeny file must never erase twice (and never
    // concurrently). The FIRST occurrence wins.
    std::vector<EraseJob> projects;
    std::set<std::string> seen;
    projects.reserve(projeny_args.size());
    for (const std::string& a : projeny_args) {
        std::string abs = absolutize(resolve_projeny_path(a, "erase-setup"));
        if (!seen.insert(abs).second) {
            warn("'" + a + "' is listed more than once; erasing it only "
                       "once");
            continue;
        }
        projects.push_back({a, abs});
    }

    // 2. Parse every unique .projeny on the main thread (make_erase_plan):
    // the Name: header names the checkout to delete and, with
    // --erase-snapshots, the Archive:/URL archive names the snapshot. A
    // file that cannot be read or parsed fails its own project — the
    // per-project task reports the canonical error under the project's
    // label — and the other projects still erase.
    std::vector<ErasePlan> plans(projects.size());
    std::vector<std::string> labels(projects.size());
    for (size_t i = 0; i < projects.size(); ++i) {
        labels[i] = basename_of(projects[i].abs);
        std::string error;
        plans[i] = make_erase_plan(projects[i].abs, erase_snapshots, &error);
    }

    // 3. Check phase (only without --force): assess every project in
    // parallel (subject to -j) BEFORE anything is erased, so a dirty or
    // unverifiable project refuses the whole invocation. With --force the
    // check is skipped entirely: the erase below is then the historical
    // unconditional behavior.
    if (!force) {
        // 3a. A project whose .projeny cannot be read or parsed (the
        // main-thread parse above recorded the canonical error) cannot be
        // assessed at all — refuse before any check runs. The refusal
        // carries each failure verbatim; nothing has been erased and
        // nothing will be.
        std::vector<std::string> broken;
        for (size_t i = 0; i < projects.size(); ++i) {
            if (plans[i].error.empty())
                continue;
            broken.push_back("'" + labels[i] + "': " + plans[i].error);
        }
        if (!broken.empty())
            die("cannot check " + std::to_string(broken.size()) + " of " +
                    std::to_string(projects.size()) +
                    " project(s) for uncommitted changes; refusing to erase "
                    "anything (use --force to erase anyway)",
                bullet_list(broken));

        // 3b. The parallel check: one worker per project, labeled so a
        // failure's canonical die() report names the project. A check that
        // dies records the failure instead of failing the pool: erase-setup
        // refuses as a whole after every check finished (the checks are
        // read-only apart from status's snapshot materialization, so
        // letting them all run to completion is safe).
        std::vector<EraseCheck> checks(projects.size());
        run_parallel(jobs, projects.size(), [&](size_t i) {
            set_output_label(labels[i]);
            try {
                assess_erase_check(plans[i], projects[i].abs, &checks[i]);
            } catch (const ProjenyFatalError& e) {
                // die() already printed the labeled report; do not repeat
                // it here — the refusal below names the project again.
                checks[i].failed = true;
                checks[i].error = e.message();
            }
            set_output_label("");
        });

        // 3c. A check that died (corrupt status file, unbuildable patch,
        // ...) or could not run at all (the archive and its snapshot are
        // missing or unusable, so the expected tree cannot be built) means
        // erase-setup cannot know what it would destroy: refuse the whole
        // invocation, listing every such project.
        std::vector<std::string> unverifiable;
        for (size_t i = 0; i < projects.size(); ++i) {
            if (!checks[i].failed)
                continue;
            unverifiable.push_back("'" + labels[i] + "': " + checks[i].error);
        }
        if (!unverifiable.empty())
            die("cannot check " + std::to_string(unverifiable.size()) +
                    " of " + std::to_string(projects.size()) +
                    " project(s) for uncommitted changes; refusing to erase "
                    "anything (use --force to erase anyway)",
                bullet_list(unverifiable));

        // 3d. Any dirty project refuses everything: erase NOTHING (not
        // even the clean projects' state), after every check finished, so
        // the all-or-nothing guarantee holds no matter which worker finished
        // first. The bullet list names each dirty project in argument order
        // with its changes in status's own vocabulary.
        size_t ndirty = 0;
        std::vector<std::string> dirty;
        for (size_t i = 0; i < projects.size(); ++i) {
            if (!checks[i].dirty)
                continue;
            ++ndirty;
            std::string line = "'" + labels[i] + "': ";
            for (size_t k = 0; k < checks[i].changes.size(); ++k) {
                if (k > 0)
                    line += "; ";
                line += checks[i].changes[k];
            }
            dirty.push_back(line);
        }
        if (ndirty > 0)
            die("refusing to erase " + std::to_string(ndirty) + " of " +
                    std::to_string(projects.size()) +
                    " project(s) with uncommitted changes (use --force to "
                    "erase anyway)",
                bullet_list(dirty));
    }

    // 4. Per-project phase: every project on its own thread (at most
    // `jobs` at a time), labeled so its warnings, errors, and notes name
    // the project. Without --force this only runs when EVERY project
    // passed the check phase above.
    std::vector<int> rcs(projects.size(), 0);
    run_parallel(jobs, projects.size(), [&](size_t i) {
        set_output_label(labels[i]);
        try {
            const ErasePlan& p = plans[i];
            if (!p.error.empty())
                die(p.error);
            rcs[i] = erase_one_project(p);
        } catch (const ProjenyFatalError&) {
            // die() already printed the labeled report; do not repeat it.
            rcs[i] = 1;
        }
        set_output_label("");
    });

    // 5. Exit code: nonzero when any project failed, with one summary line
    // after every worker joined. Success prints nothing extra.
    size_t failed = 0;
    std::string failed_labels;
    for (size_t i = 0; i < projects.size(); ++i) {
        if (rcs[i] == 0)
            continue;
        ++failed;
        if (!failed_labels.empty())
            failed_labels += ", ";
        failed_labels += labels[i];
    }
    if (failed > 0)
        fprintf(stderr, "projeny: %zu of %zu erase-setup(s) failed: %s\n",
                failed, projects.size(), failed_labels.c_str());
    return failed > 0 ? 1 : 0;
}

// Create a brand-new .projeny file pointing at an archive, then set it up.
//
// `file_arg` is the .projeny file to create: it must end in ".projeny" and
// may carry a directory path (its pdir — the directory that will hold the
// file, the archive, and the checkout — must exist). The file's stem is the
// default Name:; `origname_opt` (when given) overrides the checkout name —
// the file gets Name: <name> AND Origname: <name>, so the checkout
// directory becomes <pdir>/<name> while the .projeny file stays put.
// `archive_args` is exactly rebase's grammar (parse_archive_args): either
// one local tarball path (copied into the pdir, like rebase does) or one or
// more <url> [<blake3-hash>] pairs (downloaded + verified via verify_urls;
// the first verified download's bytes become the snapshot). `comment` is
// the file's prose (--comment; empty when absent). `force_level` counts
// --force occurrences: 0 refuses to overwrite an existing file; 1 erases
// the existing project's setup state WITH the no-force check; 2+ erases
// unconditionally (erase_project_state). With --origname, a pre-existing
// <pdir>/<name> checkout directory is refused at force 0 and — at force
// >= 1 — erased only when it is attributable, and attribution consults
// two sources in order. First the file being replaced: when <f> itself
// exists, parses, and carries Name: <name>, the directory is ITS checkout
// — its own erase below cleans it up, so no eviction is armed. Only then
// is the sibling <pdir>/<name>.projeny consulted (it must exist, parse,
// and carry Name: <name>), which arms the eviction. On any attribution
// failure the guard refuses regardless of force level: an unattributable
// directory is NEVER removed, no matter how many --force flags are given.
// `erase_snapshots` passes through to every erase. The composed file is
// parsed back and its fields validated before anything is written. Every
// fallible read-only step — the archive-argument grammar, the URL
// downloads (or the tarball's existence check), the Origname inference
// with its --origname match, and the corner guard's attribution — runs
// before any erase, so a refusal leaves the replaced project's checkout,
// status file, and .projeny bytes (and the pdir at large) untouched.
int cmd_create(const std::string& file_arg,
               const std::vector<std::string>& archive_args,
               const std::string& comment,
               const std::optional<std::string>& origname_opt,
               int force_level, bool erase_snapshots)
{
    // ---- argument validation, before anything is touched ----
    if (!ends_with(file_arg, ".projeny"))
        die("'" + file_arg + "' does not end in '.projeny'");
    std::string base = basename_of(file_arg);
    std::string stem = base.substr(0, base.size() - strlen(".projeny"));
    if (stem.empty())
        die("cannot create '" + file_arg +
            "': the project name before '.projeny' is empty");
    if (stem == "." || stem == ".." || stem.find('/') != std::string::npos)
        die("cannot create '" + file_arg + "': '" + stem +
            "' is not a valid project name (it must be a plain name: no "
            "'/', not '.' or '..')");
    std::string pdir_arg = dirname_of(file_arg);
    if (!is_dir(pdir_arg))
        die("'" + pdir_arg + "' does not exist");
    if (origname_opt &&
        (origname_opt->empty() || *origname_opt == "." ||
         *origname_opt == ".." ||
         origname_opt->find('/') != std::string::npos))
        die("bad --origname value '" + *origname_opt +
            "': it must be a plain directory name (no '/', not '.' or "
            "'..')");

    // The checkout name: --origname names the checkout directory (the
    // workdir is always named by the Name: header), defaulting to the
    // .projeny file's stem.
    std::string name = origname_opt ? *origname_opt : stem;

    // Absolutize up front: the erases below can remove the directory the
    // process's CWD sits in, and every later path must not need the CWD.
    std::string f = absolutize(file_arg);
    std::string pdir = dirname_of(f);

    // ---- the archive grammar: parse it before anything can fail ----
    // A grammar error (two tarball paths, a hash before any URL, ...) must
    // die before any erase runs, so an existing project outlives a typo.
    std::vector<ProjenyUrl> urls;
    std::string tarball;
    std::string projeny_base; // .projeny form: the RAW base argument
    parse_archive_args(archive_args, &urls, &tarball, "create", &projeny_base,
                        /* no zero-arg form */ false);
    bool url_mode = !urls.empty();
    bool projeny_mode = !projeny_base.empty();

    std::string new_base;      // the archive's basename (headers + snapshot)
    std::string verified_data; // URL mode: the verified tarball's bytes
    std::string origname;      // the archive's single top-level directory
    ProjenyFile base_pf;       // .projeny form: the base file's parse
    std::string base_path;     // .projeny form: the resolved base file path

    // ---- the fallible read-only phase ----
    // Everything from here through the corner guard below can refuse, and
    // none of it writes anything the caller would miss: only after ALL of
    // it passed do the erases (and the tarball copy) run. A plain refusal
    // dies before the download, so no bandwidth is wasted on it; a failed
    // download or a missing tarball dies before anything is erased, so
    // verify_urls' "(nothing was changed)" is literally true.
    if (path_exists(f) && force_level == 0)
        die("cannot create '" + f +
            "': it already exists (pass --force to overwrite it and "
            "erase its setup state; pass --force twice to erase "
            "unconditionally)");

    if (projeny_mode) {
        // The .projeny base form validates against the target file: the
        // same-directory law, the base's existence and parse, and the
        // live-chain cycle pre-check — all before any erase, so a refusal
        // leaves the replaced project's checkout, status file, and .projeny
        // bytes untouched. The base's own Origname: is the created file's
        // Origname: (the created file's base is the base file's COMMITTED
        // EXTRACTION, whose top-level directory the base's header names).
        base_path = validate_projeny_base_arg(projeny_base, f, "create",
                                              &base_pf);
        origname = base_pf.origname;
    } else if (url_mode) {
        // Download+verify every URL BEFORE any erase: a failed download
        // must leave the replaced project's checkout, status file, and
        // .projeny file untouched.
        //
        // The verified bytes' scratch home lives next to the .projeny file
        // being created (see make_scratch): the archive is inspected (and
        // Origname inferred) BEFORE anything is written into the pdir, so a
        // refusal here leaves nothing behind. The dir dies with this scope;
        // only the snapshot written near the end outlives it. Tarball-mode
        // creates (and every refusal before this branch) allocate none.
        TempDir tURL(make_scratch(pdir, basename_of(f), "create-url"));
        verify_urls(&urls, &verified_data, &new_base, "create", f);
        std::string archive_path = join_path(tURL.path, new_base);
        write_file_bytes(archive_path, verified_data);
        origname = archive_single_top_name(archive_path);
    } else {
        if (!path_exists(tarball))
            die("tarball '" + tarball + "' does not exist");
        new_base = basename_of(tarball);
        if (new_base.empty() || new_base.find('/') != std::string::npos)
            die("bad tarball path '" + tarball + "'");
        // The SOURCE tarball is the Origname source (archive_single_top_name
        // only reads it); the copy into the pdir waits for the destructive
        // phase below.
        origname = archive_single_top_name(tarball);
    }

    if (origname_opt && *origname_opt != origname)
        die(projeny_mode
                ? "the base file '" + base_path + "' extracts to '" +
                      origname + "', not '" + *origname_opt +
                      "' (setup requires Origname: to match the base "
                      "file's extraction)"
                : "the archive '" + (url_mode ? urls[0].url : tarball) +
                      "' unpacks to '" + origname + "', not '" +
                      *origname_opt + "' (setup requires them to match)");

    // ---- the --origname corner guard: checked read-only here, fired below ----
    // With --origname, a pre-existing <pdir>/<name> directory would be
    // clobbered by the checkout this command is about to create. At force
    // 0 that is a refusal. At force >= 1 the directory is erased ONLY when
    // it is attributable, and attribution consults two sources in order.
    // First the file being replaced: when <f> itself exists, softly parses,
    // and carries Name: <name>, the directory is ITS checkout — the
    // existing-file erase below removes <name>/ (and its status file), so
    // no eviction is armed and the guard has nothing left to do. Only when
    // <f> is no valid attribution source (missing, unparseable, or its
    // Name: differs) is the sibling <pdir>/<name>.projeny consulted: it
    // must exist (a plain file), parse cleanly, and carry Name: <name> —
    // that is what makes the directory that project's checkout (the same
    // rule resolve_projeny_path applies) — and arms the eviction below. On
    // ANY attribution failure the guard refuses regardless of force level:
    // --force --force never blind-rm's an unattributable directory
    // (erase-setup --force also only erases named projects). The check
    // runs BEFORE any erase; the eviction erase itself waits for the
    // destructive phase below.
    std::string guard_sib; // non-empty: the attributable sibling to evict
    if (origname_opt) {
        std::string workdir = join_path(pdir, name);
        if (path_exists(workdir)) {
            if (force_level == 0)
                die("cannot create '" + f + "': the checkout directory '" +
                    name + "' already exists at '" + workdir +
                    "'; remove it, or pass --force to erase the setup "
                    "state of the project that owns it");
            std::string sib = join_path(pdir, name + ".projeny");
            bool attributable = false;
            std::string raw;
            ProjenyFile cand_pf;
            // Source 1: the file being replaced. An <f> that softly parses
            // and carries Name: <name> owns the checkout; its erase below
            // cleans it up, so the guard stays unarmed.
            if (!is_dir(f) && try_read_file_bytes(f, &raw))
                attributable =
                    try_parse_projeny_softly(raw, "'" + f + "'", &cand_pf) &&
                    cand_pf.name == name;
            // Source 2: the sibling <pdir>/<name>.projeny (the same rule
            // resolve_projeny_path applies), consulted only when <f> did
            // not attribute the directory.
            if (!attributable) {
                if (!is_dir(sib) && try_read_file_bytes(sib, &raw))
                    attributable =
                        try_parse_projeny_softly(raw, "'" + sib + "'",
                                                 &cand_pf) &&
                        cand_pf.name == name;
                if (attributable)
                    guard_sib = sib;
            }
            if (!attributable)
                die("cannot create '" + f + "': '" + workdir +
                    "' exists but is not the checkout of '" + sib +
                    "' (or there is no such file); refusing to remove it");
        }
    }

    // ---- the destructive phase: every fallible check has passed ----
    if (path_exists(f))
        erase_project_state(f, erase_snapshots, force_level, "create");

    // Guard eviction: only the sibling-attributed path arms this (an <f>
    // that owned <name>/ attributed the directory to itself above and left
    // the guard unarmed; the erase above is what cleans its checkout up).
    // The re-check keeps the eviction honest: only when <name>/ still
    // stands does the remembered, attributable project's setup state go.
    if (!guard_sib.empty() && path_exists(join_path(pdir, name)))
        erase_project_state(guard_sib, erase_snapshots, force_level,
                            "create");

    // A leftover .projeny file that still claims the checkout name:
    // projeny never deletes .projeny files, so an evicted project's file
    // survives the guard's erase (and any earlier erase leaves a competing
    // sibling alone too) — it still carries Name: <name>, so both it and
    // the file this command is about to write claim <pdir>/<name> while
    // only the new one owns it. Warn loudly, naming the leftover and the
    // fix; never delete or modify the file.
    if (origname_opt) {
        std::string sib = join_path(pdir, name + ".projeny");
        if (sib != f && !is_dir(sib)) {
            std::string raw;
            ProjenyFile sib_pf;
            if (try_read_file_bytes(sib, &raw) &&
                try_parse_projeny_softly(raw, "'" + sib + "'", &sib_pf) &&
                sib_pf.name == name)
                warn("'" + sib + "' still names the checkout directory '" +
                     name + "', which now belongs to '" + f +
                     "'; remove or rename '" + sib +
                     "' if it is no longer wanted");
        }
    }

    // ---- tarball mode: copy the source into the pdir, after the erases ----
    if (!url_mode && !projeny_mode) {
        std::string dest_archive = join_path(pdir, new_base);
        std::string new_abs = absolutize(tarball);
        std::string dest_abs = absolutize(dest_archive);
        // Copy the tarball into the pdir if it isn't already there. When
        // the tarball's basename matches the replaced project's Archive
        // but its bytes differ, warn (content changed under a familiar
        // name) and continue with the new file — never silently keep the
        // old bytes. (The replaced file — if any — still holds its old
        // headers at this point; only its setup state was erased above.)
        std::string old_archive;
        std::string raw;
        ProjenyFile old_pf;
        if (path_exists(f) && try_read_file_bytes(f, &raw) &&
            try_parse_projeny_softly(raw, "'" + f + "'", &old_pf))
            old_archive = old_pf.archive;
        if (new_base == old_archive && path_exists(dest_archive) &&
            new_abs != dest_abs) {
            if (file_hash_hex(tarball) != file_hash_hex(dest_archive) ||
                file_size_bytes(tarball) != file_size_bytes(dest_archive))
                warn("tarball '" + new_base + "' differs from the current '" +
                     old_archive + "'; using the new file");
        }
        if (new_abs != dest_abs)
            copy_file_bytes(tarball, dest_archive);
    }

    // ---- compose the file ----
    // Header block in the conventional order: the base-location line (the
    // URL: lines per mirror, or Archive:, or Projeny:), then Origname:,
    // then Name: — the shape every tool-written .projeny file uses.
    ProjenyFile pf;
    std::string head;
    if (projeny_mode) {
        // The projeny form: the base is another .projeny file's committed
        // extraction, named as the plain filename the same-directory law
        // guarantees lives next to the new file.
        head += "Projeny: " + basename_of(base_path) + "\n";
    } else if (url_mode) {
        for (const ProjenyUrl& u : urls)
            head += "URL: " + u.url + " " + u.hash + "\n";
    } else {
        head += "Archive: " + new_base + "\n";
    }
    head += "Origname: " + origname + "\n";
    head += "Name: " + name + "\n";
    pf.head = head;
    if (!comment.empty()) {
        // Indent the prose like every tool-written .projeny file's free
        // text (rebuild keeps pre-indented lines verbatim;
        // normalize_prose_indent would otherwise guarantee only a single
        // leading space). Empty lines stay empty.
        std::string mid;
        size_t i = 0;
        while (i < comment.size()) {
            size_t j = comment.find('\n', i);
            if (j == std::string::npos)
                j = comment.size();
            std::string line = comment.substr(i, j - i);
            if (!line.empty())
                mid += "    ";
            mid += line;
            mid += "\n";
            i = j + 1;
        }
        pf.middle = mid;
    }
    pf.rebuild("");

    // Self-check: parse the composed bytes back and require every field to
    // round-trip before anything is written — a cheap guard against
    // composition bugs.
    {
        ProjenyFile check = ProjenyFile::parse_bytes(pf.raw, "'" + f + "'");
        bool ok = check.name == name && check.origname == origname &&
                  (projeny_mode ? check.base == basename_of(base_path)
                                : check.archive == new_base) &&
                  check.urls.size() == urls.size();
        for (size_t i = 0; url_mode && ok && i < urls.size(); ++i)
            ok = check.urls[i].url == urls[i].url &&
                 check.urls[i].hash == urls[i].hash;
        if (!ok)
            die("internal: the composed .projeny file for '" + f +
                "' does not round-trip; refusing to write it");
    }

    write_file_bytes(f, pf.raw);

    // NOW the URL snapshot: the .projeny file is in place, so the snapshot
    // name derived from the first URL's basename is exactly what the
    // file's parse will derive, and setup (called below) finds the archive
    // already cached — it downloads nothing. The projeny form needs no
    // snapshot write here: the setup below freezes the base file's current
    // bytes as the .<stem>.snapshot (the Phase 2A first-use law).
    if (url_mode)
        write_file_bytes(snapshot_path_for(join_path(pdir, new_base)),
                         verified_data);

    if (url_mode)
        printf("projeny: created '%s' (Name: '%s', Origname: '%s', archive "
               "'%s' from '%s')\n",
               f.c_str(), name.c_str(), origname.c_str(), new_base.c_str(),
               urls[0].url.c_str());
    else if (projeny_mode)
        printf("projeny: created '%s' (Name: '%s', Origname: '%s', base "
               "'%s')\n",
               f.c_str(), name.c_str(), origname.c_str(),
               basename_of(base_path).c_str());
    else
        printf("projeny: created '%s' (Name: '%s', Origname: '%s', archive "
               "'%s')\n",
               f.c_str(), name.c_str(), origname.c_str(), new_base.c_str());

    // The setup this command promises: a fresh checkout plus status file
    // (create just erased whatever setup state stood here), or setup's own
    // adopt-or-refuse rule for a pre-existing workdir the corner guard did
    // not attribute to anyone.
    return cmd_setup(f);
}

// Download URL HASH pairs into the current directory, as one parallel batch
// (the same machinery the multi-project commands use in their planning
// phase). Files are named after the URL's basename; a file already present
// with a matching hash is kept. Any failure is fatal — after every other
// package finished.
int cmd_download(const std::vector<std::string>& args, int jobs, int curl_jobs)
{
    if (args.size() < 2 || args.size() % 2 != 0)
        die("download takes URL HASH pairs; pass an even number of "
            "arguments");
    std::map<std::string, std::vector<PkgUrlSource>> by_pkg;
    std::set<std::pair<std::string, std::string>> exact;
    for (size_t i = 0; i + 1 < args.size(); i += 2) {
        const std::string& url = args[i];
        const std::string& hash = args[i + 1];
        // Same rules as a .projeny file's "URL: <url> <hash>" header
        // (parse_url_value): exactly 64 hex chars, normalized lowercase.
        std::string lower;
        bool ok = hash.size() == 64;
        for (char c : hash) {
            if (!isxdigit(static_cast<unsigned char>(c)))
                ok = false;
            lower.push_back(
                static_cast<char>(tolower(static_cast<unsigned char>(c))));
        }
        if (!ok)
            die("invalid blake3 hash '" + hash + "' for " + url);
        std::string pkg = archive_name_from_url(url);
        if (pkg.empty())
            die("URL '" + url + "' does not name a file");
        if (!exact.insert({url, lower}).second)
            continue; // exact duplicate (url,hash) pair: dedupe silently
        PkgUrlSource src;
        src.name = url;
        src.urls.push_back({url, lower});
        by_pkg[pkg].push_back(std::move(src));
    }

    std::vector<BatchPackageSpec> specs;
    for (const auto& kv : by_pkg) {
        const std::string& pkg = kv.first;
        const std::vector<PkgUrlSource>& sources = kv.second;
        warn_shared_package(pkg, sources);
        std::string local = "./" + pkg;
        // Pre-check: a local file that already verifies against one of the
        // listed hashes IS the download.
        if (is_readable_file(local)) {
            std::string h = blake3_file_hash_hex(local);
            bool have = false;
            for (const PkgUrlSource& s : sources)
                for (const ProjenyUrl& u : s.urls)
                    if (u.hash == h)
                        have = true;
            if (have) {
                note("already have " + pkg + " (blake3 hash verified)");
                continue;
            }
        }
        BatchPackageSpec spec;
        spec.name = pkg;
        spec.urls = candidate_urls(sources);
        for (const PkgUrlSource& s : sources)
            for (const ProjenyUrl& u : s.urls)
                spec.accepted_hashes.insert(u.hash);
        specs.push_back(std::move(spec));
    }

    if (specs.empty())
        return 0;
    std::vector<BatchPackageResult> results =
        download_batch(specs, curl_jobs, jobs);
    size_t failed = 0;
    std::vector<std::string> failed_lines;
    for (size_t i = 0; i < specs.size(); ++i) {
        const BatchPackageResult& r = results[i];
        if (!r.ok) {
            ++failed;
            failed_lines.insert(failed_lines.end(), r.errors.begin(),
                                r.errors.end());
            continue;
        }
        write_file_bytes("./" + r.name, r.data);
        note("wrote " + r.name + " (" + std::to_string(r.data.size()) +
             " bytes)");
    }
    if (failed > 0)
        die("failed to download " + std::to_string(failed) + " of " +
                std::to_string(specs.size()) + " package(s)",
            bullet_list(failed_lines));
    return 0;
}

// ---- frozen-mtime and attribute commands ----
//
// freeze-mtime pins a tracked file's checkout timestamp to what the archive
// wants (so timestamp-driven rebuild machinery — autoconf comparing
// configure.ac against tests/local.mk — keeps skipping its steps); the
// attribute is stored in the .projeny patch as a `frozen-mtime <ts>`
// extended header and re-stamped by every setup/rebase. unfreeze-mtime
// removes the attribute. list-frozen-mtimes and get-attributes report what
// is recorded.

namespace {

// Everything the mutating frozen-mtime commands need, with commit's guards:
// the project must be set up, the .projeny file must match the status copy
// exactly (both are rewritten together and must stay byte-identical), and
// no conflicts may be pending.
struct FreezeTarget {
    Ctx ctx;
    StatusData st;
    ProjenyFile cur;
    std::string workdir;
};

FreezeTarget resolve_frozen_target(const std::string& projeny_arg,
                                   const char* cmd)
{
    std::string pj = resolve_projeny_path(projeny_arg, cmd);
    FreezeTarget t;
    t.ctx = resolve_ctx(pj);
    t.st = require_status_matches(t.ctx);
    require_no_conflicts(t.st,
                         std::string("cannot ") + cmd +
                             " with unresolved conflicts");
    t.cur = ProjenyFile::parse_bytes(t.st.embedded,
                                     "'" + t.ctx.projeny_arg + "'");
    t.workdir = join_path(t.ctx.pdir, t.cur.name);
    if (!is_dir(t.workdir))
        die("workdir '" + t.workdir + "' is missing; run setup first");
    return t;
}

// Resolve a get-attributes path to a workdir-relative scope: "" means the
// whole workdir (the path named the workdir itself, e.g. '.'), otherwise a
// workdir-relative file or directory prefix. Same path forms as add/rm/mv.
std::string attr_scope(const std::string& workdir, const std::string& wid,
                       const std::string& user_path)
{
    std::string abs = normalize_lexical(absolutize(user_path));
    std::string wabs = normalize_lexical(absolutize(workdir));
    if (abs == wabs)
        return "";
    return normalize_workdir_rel(workdir, wid, user_path);
}

// True when workdir-relative `rel` is inside `scope` ("" covers everything).
bool in_scope(const std::string& rel, const std::string& scope)
{
    return scope.empty() || rel == scope || starts_with(rel, scope + "/");
}

// Collect the regular/symlink files under `prefix` in `root` (recursive),
// keeping those inside every scope, into `out`.
void attr_walk(const std::string& root, const std::string& prefix,
               const std::vector<std::string>& scopes,
               std::set<std::string>* out)
{
    std::string dir = prefix.empty() ? root : join_path(root, prefix);
    for (const std::string& name : list_dir_names(dir)) {
        std::string rel = prefix.empty() ? name : prefix + "/" + name;
        if (vcs_is_scratch_rel(rel))
            continue;
        std::string full = join_path(root, rel);
        struct stat st;
        if (lstat(full.c_str(), &st) != 0)
            continue; // raced deletion; nothing to report
        if (S_ISDIR(st.st_mode)) {
            attr_walk(root, rel, scopes, out);
            continue;
        }
        for (const auto& s : scopes) {
            if (in_scope(rel, s)) {
                out->insert(rel);
                break;
            }
        }
    }
}

} // namespace

int cmd_freeze_mtime(const std::string& projeny_arg,
                     const std::vector<std::string>& files)
{
    // main.cc's arity guard already rejects an empty list; dedupe repeated
    // arguments (stable) so the count it reports is accurate with duplicate
    // args (and so re-freezing the same file twice stays idempotent).
    std::vector<std::string> args;
    for (const std::string& f : files) {
        if (std::find(args.begin(), args.end(), f) == args.end())
            args.push_back(f);
    }
    FreezeTarget t = resolve_frozen_target(projeny_arg, "freeze-mtime");

    // Every requested path must name a tracked regular file of the workdir.
    // The frozen value is the mtime the BASE has for the file
    // (snapshot-aware unpack, like setup's tree reconstruction): unpack it
    // once and stat the members. A tracked file the base does not ship
    // (a committed patch-add) freezes at its current workdir mtime instead;
    // a pending (uncommitted) add is refused — the attribute needs a block
    // the committed patch can carry. For a Projeny:-based project the
    // member-mtime source is the FROZEN BASE EXTRACTION — the same source
    // commit's frozen-mtime refresh reads after the Phase 2A commit change
    // (the status-frozen base's committed content, no patch of this
    // project's applied; files the BASE's own committed patch rewrote carry
    // the applier's timestamps, exactly like commit's refresh source).
    std::vector<std::string> del;
    for (const auto& r : t.st.removed)
        del.push_back(r);
    for (const auto& rn : t.st.renamed)
        del.push_back(rn.first);
    // The committed patch is projeny-generated (a/-prefixed), so no
    // target tree is needed to resolve its labels.
    for (const auto& p : vcs_deleted_paths(t.cur.patch, t.cur.name, ""))
        del.push_back(p);
    std::vector<std::string> adds = vcs_add_paths(t.cur.patch, t.cur.name, "");
    std::vector<std::string> pending_adds = t.st.added;
    for (const auto& rn : t.st.renamed)
        pending_adds.push_back(rn.second);

    TempDir tmp(make_scratch(t.ctx.pdir, t.ctx.filebase, "freeze"));
    std::string rawtree;
    if (t.cur.is_projeny_based()) {
        // The frozen base extraction (no patch of this project's applied):
        // the committed content the frozen snapshot records, status-frozen
        // base — require_status_matches above guarantees the embedded copy
        // and the live file agree, so the simple snapshot law applies.
        // Tolerant with recovery guidance, like every frozen-chain build.
        try {
            TolerantPatchApply tol;
            rawtree = build_projeny_tree(
                t.ctx.projeny_arg, t.cur,
                resolve_committed_base_bytes(
                    t.ctx.projeny_arg, t.cur,
                    "freeze mtimes in '" + t.ctx.projeny_arg + "'"),
                "", "", tmp.path,
                "freeze mtimes in '" + t.ctx.projeny_arg + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("freeze mtimes in", t.ctx.projeny_arg);
        }
    } else {
        std::string archive_path =
            resolve_status_archive(t.ctx.pdir, t.cur,
                                   "freeze mtimes from the archive of '" +
                                       t.ctx.projeny_arg + "'");
        unpack_single_top(archive_path, tmp.path, t.cur.origname);
        rawtree = join_path(tmp.path, t.cur.origname);
    }

    std::map<std::string, uint64_t> frozen =
        vcs_frozen_mtimes(t.cur.patch, t.cur.name);
    for (const std::string& f : args) {
        std::string rel = normalize_workdir_rel(t.workdir, t.cur.name, f);
        std::string full = join_path(t.workdir, rel);
        struct stat st;
        if (lstat(full.c_str(), &st) != 0)
            die("cannot freeze '" + f + "': no such file in the workdir '" +
                rel_to_cwd(t.workdir) + "'");
        if (S_ISDIR(st.st_mode))
            die("cannot freeze '" + f +
                "': it is a directory; only regular files can be frozen");
        if (S_ISLNK(st.st_mode))
            die("cannot freeze '" + f +
                "': it is a symlink; only regular files can be frozen");
        if (!S_ISREG(st.st_mode))
            die("cannot freeze '" + f + "': not a regular file");
        if (vcs_covers_keep_path(del, rel))
            die("cannot freeze '" + f +
                "': it is not a tracked file in this project (it is removed "
                "or renamed away)");
        if (vcs_covers_keep_path(pending_adds, rel))
            die("cannot freeze '" + f +
                "': it is a pending (uncommitted) add; commit it first");
        std::string member = join_path(rawtree, rel);
        struct stat mst;
        if (lstat(member.c_str(), &mst) == 0 && S_ISREG(mst.st_mode)) {
            // The tarball's own mtime for the file (GNU tar preserves member
            // mtimes on unpack), refreshed even on a re-freeze: idempotent.
            frozen[rel] = (uint64_t)mst.st_mtim.tv_sec;
        } else if (vcs_covers_keep_path(adds, rel)) {
            // A committed patch-add has no archive member; freeze the mtime
            // the checkout currently has.
            frozen[rel] = (uint64_t)st.st_mtim.tv_sec;
        } else {
            die("cannot freeze '" + f +
                "': it is not a tracked file in this project (only files the "
                "archive ships or the committed patch adds can be frozen)");
        }
    }

    std::string new_patch = normalize_patch_text(
        vcs_set_frozen_mtimes(t.cur.patch, t.cur.name, frozen));
    t.cur.rebuild(new_patch);
    write_file_bytes(t.ctx.projeny_arg, t.cur.raw);
    StatusData sd = t.st;
    sd.embedded = t.cur.raw;
    write_status(t.ctx, sd);
    // "At the time when this command is issued, projeny should set the mtime
    // of the checked out file to be whatever the tarball wanted for that
    // file": stamp now (a no-op for files the archive already timestamps).
    stamp_frozen_mtimes(t.workdir, t.cur.patch, t.cur.name);
    printf("projeny: froze the mtime of %zu file(s) in '%s'\n", args.size(),
           t.ctx.projeny_arg.c_str());
    return 0;
}

int cmd_unfreeze_mtime(const std::string& projeny_arg,
                       const std::vector<std::string>& files)
{
    // main.cc's arity guard already rejects an empty list; dedupe repeated
    // arguments (stable) so the count is accurate with duplicate args (and
    // so unfreezing the same file twice does not die on the second one).
    std::vector<std::string> args;
    for (const std::string& f : files) {
        if (std::find(args.begin(), args.end(), f) == args.end())
            args.push_back(f);
    }
    FreezeTarget t = resolve_frozen_target(projeny_arg, "unfreeze-mtime");
    std::map<std::string, uint64_t> frozen =
        vcs_frozen_mtimes(t.cur.patch, t.cur.name);
    for (const std::string& f : args) {
        std::string rel = normalize_workdir_rel(t.workdir, t.cur.name, f);
        if (!frozen.count(rel))
            die("cannot unfreeze '" + f + "': its mtime is not frozen (see "
                "'projeny list-frozen-mtimes')");
        frozen.erase(rel);
    }
    // Dropping a header from an attribute-only block leaves a bare
    // `diff --git` husk; vcs_set_frozen_mtimes drops such blocks entirely.
    std::string new_patch = normalize_patch_text(
        vcs_set_frozen_mtimes(t.cur.patch, t.cur.name, frozen));
    t.cur.rebuild(new_patch);
    write_file_bytes(t.ctx.projeny_arg, t.cur.raw);
    StatusData sd = t.st;
    sd.embedded = t.cur.raw;
    write_status(t.ctx, sd);
    // The workdir file's mtime is deliberately left as it is: unfreezing
    // changes future setups only.
    printf("projeny: unfroze the mtime of %zu file(s) in '%s'\n", args.size(),
           t.ctx.projeny_arg.c_str());
    return 0;
}

int cmd_list_frozen_mtimes(const std::string& projeny_arg)
{
    std::string pj = resolve_projeny_path(projeny_arg, "list-frozen-mtimes");
    Ctx ctx = resolve_ctx(pj);
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    std::map<std::string, uint64_t> frozen =
        vcs_frozen_mtimes(cur.patch, cur.name);
    if (frozen.empty())
        die("no frozen mtimes in '" + ctx.projeny_arg + "' (freeze one with "
            "'projeny freeze-mtime " + ctx.projeny_arg + " <file>')");
    // One line per frozen file: "<workdir-relative path> <unix-epoch ts>".
    for (const auto& kv : frozen)
        printf("%s %llu\n", kv.first.c_str(), (unsigned long long)kv.second);
    return 0;
}

int cmd_get_attributes(const std::string& projeny_arg,
                       const std::vector<std::string>& paths)
{
    std::string pj = resolve_projeny_path(projeny_arg, "get-attributes");
    Ctx ctx = resolve_ctx(pj);
    ProjenyFile cur = ProjenyFile::parse(ctx.projeny_arg);
    std::string workdir = join_path(ctx.pdir, cur.name);
    if (!is_dir(workdir))
        die("workdir '" + workdir + "' is missing; run setup first");
    StatusData st;
    if (path_exists(ctx.statusfile))
        st = StatusData::parse(ctx.statusfile);

    // The tracking rule needs the fresh base+patch tree (one unpack, like
    // status's live diff): a file is tracked when the fresh tree, a pending
    // add, or a rename destination holds it and no removal or rename source
    // takes it away. For a Projeny:-based project the fresh tree is the
    // frozen base's committed extraction plus the patch (the same
    // reconstruction every other read of the project's base uses).
    TempDir tmp(make_scratch(ctx.pdir, ctx.filebase, "attrs"));
    std::string fresh;
    if (cur.is_projeny_based()) {
        // Tolerant: the frozen-chain-moved state bricks the tracking rule
        // exactly like status — die with the recovery guidance instead of
        // the bare apply report.
        try {
            TolerantPatchApply tol;
            fresh = build_projeny_tree(
                ctx.projeny_arg, cur,
                resolve_committed_base_bytes(
                    ctx.projeny_arg, cur, "get the attributes of '" + pj + "'"),
                cur.patch, "patch in '" + pj + "' (base '" + cur.base + "')",
                tmp.path, "get the attributes of '" + pj + "'");
        } catch (const PatchApplyError&) {
            die_projeny_chain_moved("get the attributes of", ctx.projeny_arg);
        }
    } else {
        fresh = build_tree_from_patch(
            tmp, resolve_status_archive(ctx.pdir, cur,
                                        "get the attributes of '" + pj + "'"),
            cur.origname, cur.name, cur.patch, "patch in '" + pj + "'");
    }

    // Scopes: "" for the whole workdir, else workdir-relative file or
    // directory prefixes. No paths means every tracked file of the project.
    // A requested file must exist and be tracked; a requested directory may
    // hold no tracked files at all.
    std::vector<std::string> scopes;
    if (paths.empty())
        scopes.push_back("");
    for (const std::string& p : paths) {
        std::string rel = attr_scope(workdir, cur.name, p);
        if (!rel.empty()) {
            std::string full = join_path(workdir, rel);
            struct stat pst;
            if (lstat(full.c_str(), &pst) != 0)
                die("cannot get attributes of '" + p +
                    "': no such file in the workdir '" + rel_to_cwd(workdir) +
                    "'");
            if (!S_ISDIR(pst.st_mode) && !is_tracked_rel(rel, fresh, st))
                die("'" + p +
                    "' is not a tracked file in '" + ctx.projeny_arg + "'");
        }
        scopes.push_back(rel);
    }

    std::map<std::string, uint64_t> frozen =
        vcs_frozen_mtimes(cur.patch, cur.name);
    std::set<std::string> cands;
    attr_walk(workdir, "", scopes, &cands);
    // The fresh tree can hold tracked files the workdir lost (disappeared):
    // they still carry their attributes, so walk it too.
    attr_walk(fresh, "", scopes, &cands);

    // One line per attribute, sorted by path: "<path>: frozen-mtime <ts>"
    // and "<path>: mode 100755" (a regular file with any exec bit; symlinks
    // are standard and never reported). Paths with no special attribute are
    // left out entirely.
    for (const std::string& rel : cands) {
        if (!is_tracked_rel(rel, fresh, st))
            continue; // a workdir file the fresh tree never tracked
        auto it = frozen.find(rel);
        if (it != frozen.end()) {
            printf("%s: frozen-mtime %llu\n", rel.c_str(),
                   (unsigned long long)it->second);
        }
        std::string full = join_path(workdir, rel);
        struct stat stt;
        if (lstat(full.c_str(), &stt) == 0 && S_ISREG(stt.st_mode) &&
            (stt.st_mode & 0111)) {
            printf("%s: mode 100755\n", rel.c_str());
        }
    }
    return 0;
}

int cmd_hash(const std::string& path)
{
    // The blake3 hash that a .projeny file's "URL: <url> <hash>" line wants:
    // hash the file's bytes and print the 64-char lowercase hex digest, and
    // nothing else, so the output can be pasted into the header verbatim.
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        die("cannot hash '" + path + "': " + strerror(errno));
    if (!S_ISREG(st.st_mode))
        die("cannot hash '" + path + "': it is not a regular file");
    printf("%s\n", blake3_file_hash_hex(path).c_str());
    return 0;
}

int cmd_help(const std::string& arg0)
{
    printf("usage: %s <command> [args]\n", arg0.c_str());
    printf("\n"
           "Manage \"project = release tarball + patch\" pairs.\n"
           "\n"
           "  setup <f.projeny|dir> [...]      unpack archive, apply patch\n"
           "  commit <f.projeny|dir>           fold workdir changes into the patch\n"
           "  add <f.projeny|dir> <path>       mark a file as added\n"
           "  rm <f.projeny|dir> <path>        delete a file, mark as removed\n"
           "  mv <f.projeny|dir> <src> <dst>   rename a file, mark as renamed\n"
           "  resolve <f.projeny|dir> <path>   clear a conflict marker entry\n"
           "  rebase <f.projeny|dir>           re-extract the committed Projeny: base\n"
           "  rebase <f.projeny|dir> <base.projeny>\n"
           "                                   ...or onto another .projeny base\n"
           "  rebase <f.projeny|dir> <tarball> point the project at a new tarball\n"
           "  rebase <f.projeny|dir> <url> [<hash>]...\n"
           "                                   ...or at new URL: header(s)\n"
           "  create <f.projeny> <base.projeny>\n"
           "                                   make a new .projeny based on another\n"
           "  create <f.projeny> <tarball>     make a new .projeny file and set it up\n"
           "  create <f.projeny> <url> [<hash>]...\n"
           "                                   ...or from new URL: header(s)\n"
           "  status <f.projeny|dir>           show setup/conflict/pending state\n"
           "  diff <f.projeny|dir>             print a checkout's uncommitted diff\n"
           "  diff <dir> <other-dir>           print the diff between two trees\n"
           "  patch <dir> <patch-file>         apply a patch file to a tree\n"
           "  apply <f.projeny|dir> <patch-file>\n"
           "                                   apply a patch inside a checkout\n"
           "  package <f.projeny|dir> <out> [...]\n"
           "                                   setup, then tar the tracked files\n"
           "  extract <f.projeny|dir> <dest> [...]\n"
           "                                   setup, then copy tracked files to a dir\n"
           "  download <url> <hash> [...]      download URL HASH pairs into the cwd\n"
           "  erase-setup <f.projeny|dir> [...]  delete a checkout and its status file\n"
           "  freeze-mtime <f.projeny|dir> <file>...\n"
           "                                   pin a file's mtime to the tarball's\n"
           "  unfreeze-mtime <f.projeny|dir> <file>...\n"
           "                                   drop the frozen mtime of a file\n"
           "  list-frozen-mtimes <f.projeny|dir>\n"
           "                                   list files with a frozen mtime\n"
           "  get-attributes <f.projeny|dir> [<paths>]\n"
           "                                   show special attributes of files\n"
           "  hash <file>                      print the blake3 hash of a file\n"
           "  help [command]                   show this message or command help\n"
           "\n"
           "  options for setup/package/extract/download: -j[--jobs] N, "
           "-c[--curl-jobs] N\n"
           "  options for erase-setup: -j[--jobs] N, --erase-snapshots, "
           "--force\n"
           "  options for create: --comment TEXT, --origname NAME,\n"
           "                      --force, --erase-snapshots\n"
           "  setup/package/extract/erase-setup take several projects (parallel);\n"
           "  download takes <url> <hash> pairs.\n"
           "\n"
           "Project arguments (<f.projeny|dir>) may be the .projeny file, the\n"
           "workdir or another directory holding exactly one .projeny file, or\n"
           "a path whose '<arg>.projeny' sibling exists — typically a missing\n"
           "checkout directory, or a bare name like 'foo' for 'foo.projeny'.\n"
           "A directory next to a '<dir>.projeny' sibling names it only when\n"
           "that file's Name: header equals the directory's name (the checkout\n"
           "is always named by Name:); on mismatch the command refuses, and a\n"
           "sibling that cannot be parsed keeps the old behavior so a\n"
           "git-conflicted .projeny still recovers via 'setup'. Relative\n"
           "arguments are lexically normalized first, so from inside\n"
           "the workdir '.' names the project and, from a workdir\n"
           "subdirectory, '..' does too.\n"
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
        printf("%s setup <f.projeny|dir>\n"
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
               "Instead of an Archive: header, a .projeny file may use one\n"
               "or more URL: headers to fetch the tarball from the network\n"
               "(no tarball is checked into git). Each line has the form\n"
               "\n"
               "  URL: <url> <blake3-hash>\n"
               "\n"
               "naming the tarball's URL and the blake3 hash of its bytes\n"
               "(compute the hash with `projeny hash <file>`). Archive: and\n"
               "URL: headers are mutually exclusive. The URL lines are\n"
               "mirrors: they are tried in the order listed, and a download\n"
               "that fails or does not match its hash only prints a warning\n"
               "before the next one is tried — it is a hard error only when\n"
               "no URL yields a download matching its recorded hash. The\n"
               "archive name (and the snapshot's name) is derived from the\n"
               "URL's basename, so the URL must name the tarball file\n"
               "itself. The download is cached — and verified against the\n"
               "URL hashes — as .<archive>.snapshot next to the .projeny\n"
               "file, exactly where a checked-in tarball's snapshot copy\n"
               "lives; while that snapshot matches a URL hash, it IS the\n"
               "archive and no network access happens. Only a missing\n"
               "snapshot (or one that no longer matches any hash, e.g. a\n"
               "tampered or truncated file) triggers a re-download.\n"
               "\n"
               "Instead of an Archive: or URL: header, a .projeny file may\n"
               "base itself on another .projeny file with a Projeny:\n"
               "header:\n"
               "\n"
               "  Projeny: <base.projeny>\n"
               "\n"
               "The value is a plain filename (no '/'), resolved in the same\n"
               "directory as the referencing file — the same search law the\n"
               "Archive: header uses — so a base always lives next to the\n"
               "file that names it. Exactly one of Archive:, URL:, and\n"
               "Projeny: may appear. The base is the COMMITTED EXTRACTION of\n"
               "the named file: its archive (snapshot-preferred) plus its\n"
               "committed patch — the base file's own workdir (its checkout\n"
               "with uncommitted changes) is ignored everywhere. Chains\n"
               "compose: a Projeny:-based base is itself extracted through\n"
               "its own chain, each link resolving ITS base through its own\n"
               "frozen snapshot. A chain that closes on itself is a hard\n"
               "error naming the whole walk; Projeny: chains must not\n"
               "contain cycles.\n"
               "\n"
               "A Projeny:-based project keeps a FROZEN snapshot of its base\n"
               "file's bytes as .<stem>.snapshot next to the .projeny file\n"
               "(bar.projeny keeps .bar.snapshot). Every setup, commit,\n"
               "status, diff, package, and extract of the project\n"
               "reconstructs exactly the base that snapshot records — so\n"
               "setup/commit keep using the old base even after the base\n"
               "file itself gains committed changes, until a `rebase`\n"
               "re-extracts it. Only a missing snapshot (first use: the\n"
               "base file's current bytes are frozen into it — not an error\n"
               "while the base file exists), a setup that adopts a\n"
               "hand-edited Projeny: header naming a DIFFERENT base file\n"
               "(the newly named base's current bytes overwrite the\n"
               "snapshot and become the base), or a `rebase` ever (re)\n"
               "writes it. `erase-setup --erase-snapshots` deletes exactly\n"
               "that snapshot file, and a workdir-gone setup stales it like\n"
               "any other snapshot bookkeeping.\n"
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
               "status file.\n"
               "\n"
               "Parallel mode: `%s setup <f.projeny|dir> [...]` sets up\n"
               "several projects in one run, in two phases. First every\n"
               ".projeny file named on the command line is read to collect\n"
               "its URL: headers, and everything that needs downloading runs\n"
               "as one batch: at most -c/--curl-jobs transfers in flight\n"
               "(default 8), blake3 hash checks on at most -j/--jobs threads\n"
               "(default the CPU count), one retry pass over the failures.\n"
               "Then the per-project setups run on at most -j/--jobs\n"
               "threads. A single-project setup runs exactly as it always\n"
               "has.\n"
               "\n"
               "Downloads are deduplicated by archive name (the URL's\n"
               "basename): when several named projects fetch archives with\n"
               "the same name, one download feeds all of them, and the URLs\n"
               "every file lists are tried first. projeny prints a loud\n"
               "(ALL-CAPS) warning when files name the same archive with\n"
               "different URL sets, and a louder one when the same URL is\n"
               "listed with different blake3 hashes — and proceeds anyway:\n"
               "every one of those files receives the same downloaded\n"
               "archive. Listing one project twice collapses into a single\n"
               "setup with a warning (never two setups of one .projeny, not\n"
               "even sequential ones). A project whose download fails dies\n"
               "with a labeled error line naming it; the other projects\n"
               "still set up, and the command exits nonzero with a one-line\n"
               "summary of what failed.\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t, t);
        return 0;
    }
    if (topic == "commit") {
        printf("%s commit <f.projeny|dir>\n"
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
               "while conflicts are pending (resolve them first).\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t);
        return 0;
    }
    if (topic == "add") {
        printf("%s add <f.projeny|dir> <path>\n"
               "\n"
               "Mark a file in the workdir as added-but-not-committed (stored\n"
               "in the status file; folded into the patch by the next\n"
               "`commit`). The path may be CWD-relative, absolute, or\n"
               "workdir-relative (<Name>/...); it is stored relative to the\n"
               "workdir. The file must exist. If a later `setup` also adds\n"
               "the same file, that is a merge conflict.\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t);
        return 0;
    }
    if (topic == "rm") {
        printf("%s rm <f.projeny|dir> <path>\n"
               "\n"
               "Delete a file from the workdir and mark it as\n"
               "removed-but-not-committed (folded into the patch by the next\n"
               "`commit`). Path forms are the same as for `add`.\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t);
        return 0;
    }
    if (topic == "mv") {
        printf("%s mv <f.projeny|dir> <src> <dst>\n"
               "\n"
               "Rename a file inside the workdir and record the rename as\n"
               "pending (folded into the patch by the next `commit`, which\n"
               "renders it as a rename diff — always, even when the moved\n"
               "file's content diverged beyond the rename similarity\n"
               "threshold). Moving a pending-added file keeps it added under\n"
               "the new name. Path forms are the same as for `add`.\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t);
        return 0;
    }
    if (topic == "resolve") {
        printf("%s resolve <f.projeny|dir> <path>\n"
               "\n"
               "Drop a file from the status conflict list after you fixed\n"
               "its conflict markers by hand. Accepts the stored\n"
               "workdir-relative form (src/a.c), the on-disk <Name>/.../\n"
               "form, a CWD-relative path, or an absolute path. If the file\n"
               "still contains conflict-marker lines, projeny warns on\n"
               "stderr but still resolves (committing markers would bake\n"
               "them into the patch).\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t);
        return 0;
    }
    if (topic == "rebase") {
        printf("%s rebase <f.projeny|dir>\n"
               "%s rebase <f.projeny|dir> <base.projeny>\n"
               "%s rebase <f.projeny|dir> <new-tarball>\n"
               "%s rebase <f.projeny|dir> <url> [<blake3-hash>] "
               "[<url> [<blake3-hash>]...]\n"
               "\n"
               "Point the project at a new base: apply the current patch\n"
               "onto it, rewrite the base-location and Origname:\n"
               "headers, regenerate the patch, and move the result into\n"
               "the workdir. The tree must be clean (no uncommitted\n"
               "changes) and have no pending conflicts; when never set\n"
               "up, runs `setup` first. Conflicts leave markers and are\n"
               "recorded in the status file. Pending add/rm/mv operations\n"
               "are preserved.\n"
               "\n"
               "Which form the new-archive arguments take decides the\n"
               "mode, and the FIRST argument decides that: a string\n"
               "libcurl's URL parser accepts (http://, https://, ftp://,\n"
               "ftps://, file://) starts URL mode, a name ending in\n"
               "'.projeny' starts the projeny-base mode, anything else is\n"
               "a local tarball path, and NO argument at all is the\n"
               "zero-arg form (a Projeny:-based project re-extracting its\n"
               "committed base; any other project refuses).\n"
               "\n"
               "With no argument (only a Projeny:-based project may do\n"
               "this), the project re-extracts its COMMITTED base: what\n"
               "the base file named by the Projeny: header holds NOW — its\n"
               "own archive plus its committed patch — not the frozen\n"
               "snapshot the last setup froze. This is how committed\n"
               "changes to the base file itself flow into the project: the\n"
               "patch moves onto the new extraction, Origname: is refreshed\n"
               "from the base (the header may have been hand-edited), and\n"
               "the frozen .<stem>.snapshot is overwritten with the base\n"
               "file's current bytes. The Projeny: header itself is\n"
               "unchanged, and the base file's own workdir is ignored\n"
               "(exactly like every other read of a base).\n"
               "\n"
               "With one <base.projeny> argument, the project switches to a\n"
               "different base file: it must live in the same directory as\n"
               "the project (the header stores a plain filename, so a\n"
               "cross-directory base cannot be expressed), must parse as a\n"
               ".projeny file, and must not close a cycle in the Projeny:\n"
               "chain — basing a file on something that transitively\n"
               "depends on it is refused before anything is touched. The\n"
               "Projeny: header is rewritten to the new base, Origname:\n"
               "comes from the new base's own parse, the patch moves onto\n"
               "the new base's committed extraction, and the frozen\n"
               "snapshot is overwritten with the new base's bytes.\n"
               "\n"
               "Giving tarball or URL arguments to a Projeny:-based project\n"
               "converts it away from the Projeny: form: the Projeny:\n"
               "header is replaced by the new Archive: or URL: header(s),\n"
               "the old .<stem>.snapshot is left in place (stale snapshots\n"
               "are never auto-cleaned), and everything else works exactly\n"
               "as described below. A URL:-based project accepts only URL\n"
               "arguments, unchanged.\n"
               "\n"
               "With one local tarball path, the project keeps its\n"
               "checked-in Archive: form: the tarball is copied next to\n"
               "the .projeny file, Archive:/Origname: are rewritten, and\n"
               "when the new tarball reuses the current Archive basename\n"
               "with different bytes, projeny warns on stderr and uses\n"
               "the new file. A URL:-based project has no checked-in\n"
               "tarball, so this form refuses.\n"
               "\n"
               "With URL arguments, the URLs (each optionally followed by\n"
               "its blake3 hash — the same 64-hex value `projeny hash`\n"
               "computes, uppercase accepted) become the file's new\n"
               "URL: headers, tried in order by later setups. An\n"
               "Archive:-based project converts to URL:-based (the\n"
               "Archive: header is replaced; remove the old checked-in\n"
               "tarball from git by hand); a URL:-based project gets new\n"
               "URL: headers. Every listed URL is downloaded and\n"
               "verified during the rebase, each distinct URL once: a\n"
               "hashless URL that cannot be downloaded is a hard error\n"
               "(no hash could be computed), a provided hash that\n"
               "mismatches is a hard error (the hash was asserted), and\n"
               "a hashed URL that fails to download only warns (the\n"
               "mirror stays listed, and a later setup falls through it\n"
               "the same way). An omitted hash is computed from the\n"
               "download and reported. If no URL verifies, nothing is\n"
               "rebased and every file is left untouched. The first\n"
               "verified download's bytes are written to the snapshot,\n"
               "named after the FIRST URL's basename; the rest of the\n"
               "rebase (clean tree, conflicts leave markers, resolve +\n"
               "commit) works exactly as with a tarball.\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t, t, t, t);
        return 0;
    }
    if (topic == "create") {
        printf("%s create <f.projeny> <base.projeny>\n"
               "%s create <f.projeny> <new-tarball>\n"
               "%s create <f.projeny> <url> [<blake3-hash>] "
               "[<url> [<blake3-hash>]...]\n"
               "       [--comment <text>] [--origname <name>] [--force]\n"
               "       [--erase-snapshots]\n"
               "\n"
               "Create a brand-new .projeny file naming an archive, then\n"
               "run `setup` on it: the checkout named by Name: is unpacked\n"
               "from the archive, the fresh (empty) patch applies, and a\n"
               "new status file records the setup. Options may appear\n"
               "anywhere among the arguments.\n"
               "\n"
               "The first argument is the .projeny file to create. It must\n"
               "end in '.projeny' and may carry a directory path (e.g.\n"
               "projects/foo.projeny): the directory that would hold it\n"
               "must exist. Its name — the basename minus '.projeny' — is\n"
               "the default Name: header, so the checkout directory\n"
               "defaults to <pdir>/<name> next to the new file.\n"
               "\n"
               "The rest of the arguments name the archive, with exactly\n"
               "the grammar `rebase` accepts, decided by the FIRST\n"
               "argument: a string libcurl's URL parser accepts (http://,\n"
               "https://, ftp://, ftps://, file://) starts URL mode, a name\n"
               "ending in '.projeny' starts the projeny-base mode, and\n"
               "anything else is one local tarball path.\n"
               "\n"
               "With one <base.projeny> argument, the new file is based on\n"
               "another .projeny file's committed extraction: the file gets\n"
               "a Projeny: header naming the base — a plain filename, so\n"
               "the base must live in the same directory as the new file —\n"
               "and the first setup below freezes the base file's current\n"
               "bytes as the .<stem>.snapshot. Origname: comes from the\n"
               "base file's own parse, so --origname must match the base\n"
               "file's extraction (its Origname: — the top-level directory\n"
               "its own committed content unpacks to), not an archive's.\n"
               "Basing the new file on a chain that would close on itself\n"
               "(the base transitively depends on the file being created)\n"
               "is refused before anything is written.\n"
               "\n"
               "With one local tarball path, the file gets an Archive:\n"
               "header and the tarball is copied next to the new .projeny\n"
               "file — exactly what `rebase` does, including the\n"
               "warn-and-use-the-new-file behavior when the copy's basename\n"
               "matches the Archive: of the project this command replaces\n"
               "with different bytes.\n"
               "\n"
               "With URL arguments, the URLs (each optionally followed by\n"
               "its blake3 hash — the same 64-hex value `projeny hash`\n"
               "computes, uppercase accepted) become the file's URL:\n"
               "headers, tried in order by later setups; the project is\n"
               "born URL:-based (no tarball is checked into git). Every\n"
               "listed URL is downloaded and verified during the create,\n"
               "each distinct URL once: an omitted hash is computed from\n"
               "the download and reported, a provided hash that mismatches\n"
               "is a hard error, a hashless URL that cannot be downloaded\n"
               "is a hard error, and a hashed URL that fails to download\n"
               "only warns (the mirror stays listed). If no URL verifies,\n"
               "nothing is created. The first verified download's bytes\n"
               "are written to the snapshot, named after the FIRST URL's\n"
               "basename — the same derivation the .projeny parse applies\n"
               "— so the setup below downloads nothing.\n"
               "\n"
               "Headers are composed in the conventional order: the\n"
               "base-location line (the URL: lines, or Archive:, or\n"
               "Projeny:), then Origname:, then Name:, followed\n"
               "by the prose. Origname: is inferred from the archive\n"
               "itself: its single top-level directory (an archive holding\n"
               "several is a hard error) — or, for the projeny form, taken\n"
               "from the base file's own Origname:. --origname <name> overrides the\n"
               "checkout name: the created file gets BOTH Name: <name> AND\n"
               "Origname: <name> — the workdir is always named by Name:,\n"
               "so the checkout directory becomes <pdir>/<name> while the\n"
               ".projeny file stays <f.projeny> — and <name> must match\n"
               "the archive's top directory, or the base file's extraction\n"
               "for the projeny form (otherwise the create refuses\n"
               "before anything is written).\n"
               "\n"
               "--comment <text> stores <text> as the file's free-text\n"
               "prose, indented like every tool-written .projeny file's\n"
               "comments; repeating the option keeps only the last text.\n"
               "\n"
               "When <f.projeny> already exists, --force decides what\n"
               "happens to the project it still names (the erase-setup\n"
               "semantics; --erase-snapshots passes through to the erase):\n"
               "\n"
               "  no --force     refuse: it already exists, nothing is\n"
               "                 touched\n"
               "  --force        erase the existing project's setup state,\n"
               "                 but only after the no-force check passes:\n"
               "                 a checkout with uncommitted changes or\n"
               "                 unresolved conflicts refuses\n"
               "  --force twice  erase unconditionally (uncommitted work\n"
               "                 is discarded)\n"
               "\n"
               "A pre-existing checkout directory named by --origname gets\n"
               "one more guard: <pdir>/<name> is erased (at --force or\n"
               "above, at the same force level) only when it is\n"
               "attributable. The file being replaced is considered\n"
               "first: when <f.projeny> itself exists, parses cleanly, and\n"
               "carries Name: <name>, the directory is its checkout and\n"
               "its own erase (above) removes it. Otherwise the sibling\n"
               "<pdir>/<name>.projeny must exist, parse cleanly, and\n"
               "carry Name: <name>, which is what makes the directory its\n"
               "checkout. A directory that cannot be attributed that way\n"
               "is NEVER removed, no matter how many --force flags are\n"
               "given (erase-setup --force also only erases named\n"
               "projects): the create refuses instead, naming the sibling\n"
               "it checked. An eviction never deletes the evicted\n"
               "project's .projeny file (projeny never deletes .projeny\n"
               "files), so the leftover file still carries Name: <name>\n"
               "and keeps claiming a checkout directory that now belongs\n"
               "to the new file: the create prints a warning naming the\n"
               "leftover and suggesting it be removed or renamed if it is\n"
               "no longer wanted. Without --origname, a pre-existing\n"
               "directory named by Name: is left to setup's own\n"
               "adopt-or-refuse rule.\n"
               "\n"
               "The command prints one `created` line naming the file, the\n"
               "checkout name, the top-level directory, and the archive or\n"
               "base file,\n"
               "then setup's usual line; the erases print their own\n"
               "erase-setup notes.\n",
               t, t, t);
        return 0;
    }
    if (topic == "status") {
        printf("%s status <f.projeny|dir>\n"
               "\n"
               "Show the setup state from the status file: the Status: line\n"
               "plus pending Conflict:/Added:/Removed:/Renamed: entries,\n"
               "plus the live workdir state versus the expected tree:\n"
               "Modified: (tracked files with uncommitted edits),\n"
               "Disappeared: (tracked files missing on disk and not marked\n"
               "removed/renamed), and Untracked: (new files not marked\n"
               "added/renamed) lines.\n"
               "Exits nonzero when never set up (no status file).\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t);
        return 0;
    }
    if (topic == "diff") {
        printf("%s diff <f.projeny|dir>\n"
               "%s diff <dir> <other-dir>\n"
               "\n"
               "With a .projeny file: print the checkout's uncommitted change\n"
               "to stdout — the diff of the workdir against what a FRESH\n"
               "`setup` of the current .projeny file would check out (the\n"
               "tarball plus the current patch). `commit` answers a\n"
               "different question about the same workdir: it diffs against\n"
               "the raw tarball. For ordinary edits the two patches agree;\n"
               "they differ only when a pending `mv` renames a file the\n"
               "last commit itself added or renamed — the diff describes\n"
               "the move against the checked-in paths, while `commit`\n"
               "re-derives it from the tarball's paths (a committed-added\n"
               "file commits as a plain add of its new name; a committed\n"
               "rename re-traces to the original tarball path). Each output\n"
               "is correct for its own baseline.\n"
               "\n"
               "Pending add/rm/mv operations are reported as add/delete/\n"
               "rename blocks: a `projeny mv` renders as a rename block even\n"
               "when the moved file's content diverged beyond the rename\n"
               "similarity threshold, and chained moves collapse into one\n"
               "rename from the original path to the final one (a directory\n"
               "move renames each contained file). Only registered changes\n"
               "count: a file added without `projeny add` or moved-to\n"
               "without `projeny mv` is untracked and stays out of the diff,\n"
               "like git leaves untracked files out (one exception: files\n"
               "created inside a directory that a pending `mv` moved — the\n"
               "move destination is registered, so new files under it ride\n"
               "along); a file deleted without `projeny rm` is left out\n"
               "too, and produces a warning on stderr naming the `projeny\n"
               "rm` command that would record it.\n"
               "The command refuses (hard error) when the .projeny file\n"
               "differs from the status copy (run `setup` to merge first),\n"
               "when conflicts are unresolved, or when pending ops do not\n"
               "match the workdir. The exit status is 0 whenever the diff\n"
               "itself succeeds — even when it prints changes or warnings.\n"
               "stdout carries only the patch; warnings go to stderr.\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically a missing checkout directory, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n"
               "\n"
               "With two directories: print the minimal unified diff between\n"
               "two on-disk trees to stdout (git-compatible: `diff --git\n"
               "a/... b/...`, ---/+++, @@ hunks, new/deleted/rename entries;\n"
               "text hunks are accepted by `git apply` and `patch -p1` as\n"
               "well as `projeny patch`). The diff is minimal: an internal\n"
               "Myers line diff with rename detection. NUL-bearing (binary)\n"
               "files travel as base64 `GIT binary patch` blocks\n"
               "(projeny-internal encoding); everything else roundtrips\n"
               "exactly, including trailing blank lines. Labels use the\n"
               "second directory's basename, so `projeny diff A B > p` plus\n"
               "`projeny patch C p` reproduces B from a copy C of A. Both\n"
               "arguments must be directories.\n",
               t, t);
        return 0;
    }
    if (topic == "patch") {
        printf("%s patch <dir> <patch-file>\n"
               "\n"
               "Apply a unified-diff patch file to a directory with -p1\n"
               "semantics and fuzz (git-style diffs: a/b prefixes,\n"
               "/dev/null sides, new/deleted/mode/rename lines). Plain\n"
               "unified diffs are recognized too: --- label / +++ label\n"
               "blocks with @@ hunks and no diff --git header, optionally\n"
               "preceded by prose (a cover note, mail headers, a\n"
               "diffstat), which is ignored. Prefix-free plain labels (the\n"
               "`diff -u old/f new/f` and recursive `diff -ruN oldtree\n"
               "newtree` shapes) resolve like `patch -p1` with the tree as\n"
               "the guide: the whole label path when it exists in <dir>,\n"
               "else one component stripped. GNU diff -N marks the\n"
               "nonexistent side of an add/delete block with the epoch\n"
               "timestamp 1970-01-01 00:00:00 +0000; such a side reads as\n"
               "/dev/null, so those blocks apply as creations/removals\n"
               "(a file the patch empties under an epoch-marked +++ side\n"
               "is removed, like GNU patch). GNU diff renders that stamp\n"
               "in the local zone, so west of UTC the epoch reads\n"
               "1969-12-31 16:00:00 -0800; any spelling naming the epoch\n"
               "instant is accepted. A plain block whose two\n"
               "sides name different paths applies to the old-side name,\n"
               "like GNU patch: plain diffs never rename — only explicit\n"
               "rename from/to headers do. The patch's workdir label (if\n"
               "any) is detected automatically,\n"
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
    if (topic == "apply") {
        printf("%s apply <f.projeny|dir> <patch-file>\n"
               "\n"
               "Apply a patch file inside a projeny checkout, on top of the\n"
               "uncommitted changes it already has. The patch may be any\n"
               "git diff, any projeny diff (including `projeny diff`\n"
               "output), or any plain unified diff (--- / +++ / @@ blocks\n"
               "without a diff --git header, optionally preceded by prose\n"
               "such as a cover note; the prose is ignored). Paths are\n"
               "interpreted like `patch -p1` run inside the checkout: one\n"
               "a/ or b/ component is stripped and the rest is relative to\n"
               "the checkout root; prefix-free plain labels (the `diff -u`\n"
               "and recursive `diff -ruN` shapes) fall back to a\n"
               "one-component strip guided by the checkout — the whole\n"
               "label path when it exists there, else one component\n"
               "stripped. GNU diff -N's epoch timestamps (1970-01-01\n"
               "00:00:00 +0000) mark the nonexistent side of an\n"
               "add/delete block: those blocks apply as creations and\n"
               "removals, like GNU patch. GNU diff renders those stamps\n"
               "in the local zone, so west of UTC the epoch reads\n"
               "1969-12-31 16:00:00 -0800; any spelling naming the epoch\n"
               "instant is accepted. A plain block whose two sides\n"
               "name different paths applies to the old-side name, like\n"
               "GNU patch: plain diffs never rename — only explicit\n"
               "rename from/to headers do. Text hunks apply with fuzz, and\n"
               "adds,\n"
               "deletes, renames, and mode changes (exec bit, symlinks)\n"
               "are applied to the files. The patch's workdir label (if\n"
               "any) is detected automatically.\n"
               "\n"
               "Unlike `patch`, this command also books the patch into the\n"
               "status file, so a later `commit` folds it into the\n"
               ".projeny patch: files the patch adds are marked added,\n"
               "files it deletes are marked removed, renames are recorded\n"
               "as pending renames (chained through existing pending\n"
               "renames exactly like `mv`), and a patch add un-removes a\n"
               "pending removal. Only blocks that really applied count: a\n"
               "failed (conflicted) add, delete, or rename leaves the old\n"
               "marks alone.\n"
               "Blocks that already applied are skipped, so re-applying is\n"
               "idempotent.\n"
               "Conflicted blocks get inline markers (<<<<<<< current /\n"
               "======= / >>>>>>> patched; matching hunks still apply,\n"
               "deletions/renames/binary blocks are left for you), the\n"
               "files are marked conflicted in the status file, and the\n"
               "command exits 1 — resolve with `projeny resolve` (fixing\n"
               "the markers by hand), then `commit`.\n"
               "When the patch touches a file that is already marked\n"
               "conflicted, nothing is applied at all — no filesystem\n"
               "change and no status write — and the command hard-errors;\n"
               "resolve those conflicts first. An empty patch file is a\n"
               "silent no-op; a non-empty patch with no diff blocks, or a\n"
               "missing patch file, is a hard error. git base85 binary\n"
               "payloads (and payload-less \"Binary files ... differ\"\n"
               "stanzas) cannot be applied and become conflicts, while\n"
               "projeny's own base64 binary sections apply.\n"
               "\n"
               "Like every project-taking command, the <f.projeny|dir>\n"
               "argument may also be the workdir or another directory\n"
               "holding exactly one .projeny file, or a path whose\n"
               "'<arg>.projeny' sibling exists.\n",
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
               "for a \"<dir>\" workdir) exactly one .projeny file, or a path\n"
               "whose '<arg>.projeny' sibling exists (a missing checkout\n"
               "directory also works then).\n"
               "The archive holds a single top-level directory named after\n"
               "the output file (pizlonated-libffi.tar.gz holds\n"
               "pizlonated-libffi/...). Compression is autodetected from the\n"
               "output extension: .tar (none), .tar.gz/.tgz (gzip),\n"
               ".tar.bz2/.tbz2/.tbz (bzip2), .tar.xz/.txz (xz),\n"
               ".tar.zst/.tzst (zstd). When `setup` reports conflicts the\n"
               "command prints them and exits nonzero without writing any\n"
               "archive.\n"
               "\n"
               "Parallel mode: `%s package <f.projeny|dir> <output-tarball>\n"
               "[...]` takes (project, output) pairs and packages them in\n"
               "parallel, with the same two-phase download batching as\n"
               "parallel `setup` (-j/--jobs bounds both the per-project\n"
               "parallelism and the hash-check parallelism, default the CPU\n"
               "count; -c/--curl-jobs bounds transfers, default 8). Listing\n"
               "one project twice collapses into one packaging with a\n"
               "warning — the first pair's output is the one produced — and\n"
               "projects sharing an archive basename download it once, with\n"
               "the same loud warnings as parallel `setup`.\n",
               t, t);
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
               ".projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (a missing checkout directory also works then). The\n"
               "destination must not exist or must be an empty directory\n"
               "(remove it first to redo an extraction).\n"
               "When `setup` reports conflicts the command prints them and\n"
               "exits nonzero without writing anything.\n"
               "\n"
               "Parallel mode: `%s extract <f.projeny|dir> <dest-dir>\n"
               "[...]` takes (project, destination) pairs and extracts them\n"
               "in parallel, with the same two-phase download batching as\n"
               "parallel `setup` (-j/--jobs bounds both the per-project\n"
               "parallelism and the hash-check parallelism, default the CPU\n"
               "count; -c/--curl-jobs bounds transfers, default 8). Listing\n"
               "one project twice collapses into one extraction with a\n"
               "warning — the first pair's destination is the one filled —\n"
               "and projects sharing an archive basename download it once,\n"
               "with the same loud warnings as parallel `setup`.\n",
               t, t);
        return 0;
    }
    if (topic == "freeze-mtime") {
        printf("%s freeze-mtime <f.projeny|dir> <filenames...>\n"
               "\n"
               "Pin the mtime of tracked regular files to what the tarball\n"
               "wants them to be. At the time the command runs, the checked\n"
               "out file's mtime is set to the archive member's mtime, and\n"
               "the attribute is recorded in the .projeny patch (and the\n"
               "status copy) as an extended header `frozen-mtime <ts>`\n"
               "(unix-epoch seconds) inside the file's `diff --git` block,\n"
               "right where the old/new mode lines live — so it survives\n"
               "every patch regeneration and merges like any other patch\n"
               "metadata.\n"
               "\n"
               "Every `setup` (and every `rebase`, and the setup that\n"
               "`package`/`extract` run) re-stamps frozen files afterwards:\n"
               "a file the patch rewrote gets a fresh timestamp from the\n"
               "applier, and the stamp pass puts the archive's mtime back.\n"
               "This keeps timestamp-driven build machinery (autoconf\n"
               "comparing configure.ac against a generated local.mk, make\n"
               "comparing sources against generated files) from seeing a\n"
               "patched file as newer than its inputs. `commit` and\n"
               "`rebase` refresh the stored values from the archive (after\n"
               "a rebase: the NEW archive's members), so the invariant\n"
               "holds that a frozen value is always the archive's member\n"
               "mtime for that file. Every setup that can parse the\n"
               ".projeny file stamps frozen files — including setups that\n"
               "end in conflicts — so a frozen file keeps the archive's\n"
               "mtime even when its content ends up with conflict markers;\n"
               "only a .projeny file that itself contains git conflict\n"
               "markers defers stamping to the next clean setup. Freezing\n"
               "itself re-stamps ALL frozen files of the project, not just\n"
               "the ones this command names.\n"
               "\n"
               "A file may be frozen without any content or mode change:\n"
               "that stores an attribute-only block (`diff --git a/X b/X`\n"
               "plus the `frozen-mtime` line, no hunks — the analogue of a\n"
               "mode-only block). Re-freezing a file updates its value.\n"
               "\n"
               "The file arguments use the same forms as `add`/`rm`/`mv`:\n"
               "CWD-relative, absolute, or workdir-relative (<Name>/...).\n"
               "Each must name a tracked regular file: directories,\n"
               "symlinks, untracked files, and pending (uncommitted) adds\n"
               "are refused; a file the committed patch adds (no archive\n"
               "member) freezes at its current workdir mtime. The command\n"
               "requires the .projeny file to match the status copy (else\n"
               "hard error: run `setup` to merge first) and refuses while\n"
               "conflicts are pending.\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists — including `.` from inside the workdir and `..` from\n"
               "a workdir subdirectory.\n",
               t);
        return 0;
    }
    if (topic == "unfreeze-mtime") {
        printf("%s unfreeze-mtime <f.projeny|dir> <filenames...>\n"
               "\n"
               "Drop the frozen-mtime attribute of each named file: future\n"
               "setups stop re-stamping it (patch-rewritten files then keep\n"
               "the fresh timestamp the applier gives them). The workdir\n"
               "file's current mtime is left as it is — unfreezing changes\n"
               "future setups only. When the attribute was the only content\n"
               "of a block (an attribute-only block), the block is dropped\n"
               "entirely rather than left as a bare `diff --git` husk.\n"
               "Refuses a file whose mtime is not frozen. The .projeny file\n"
               "and the status copy are updated together (the .projeny file\n"
               "must match the status copy first: run `setup` to merge).\n",
               t);
        return 0;
    }
    if (topic == "list-frozen-mtimes") {
        printf("%s list-frozen-mtimes <f.projeny|dir>\n"
               "\n"
               "List every file with a frozen mtime, one per line, as\n"
               "`<workdir-relative path> <unix-epoch timestamp>` — the\n"
               "mtime the archive has for that file and that every setup\n"
               "restores. Hard-errors when nothing is frozen. Reads the\n"
               "current .projeny patch, so it works without a setup too.\n",
               t);
        return 0;
    }
    if (topic == "get-attributes") {
        printf("%s get-attributes <f.projeny|dir> [<path>...]\n"
               "\n"
               "Print the special attributes of tracked files, one line per\n"
               "attribute, sorted by path:\n"
               "\n"
               "  <path>: frozen-mtime <ts>   the file's mtime is frozen\n"
               "  <path>: mode 100755         the workdir file currently\n"
               "                              has an exec bit\n"
               "\n"
               "The mode line reports the CURRENT workdir mode (what\n"
               "`commit` would store), not a stored attribute. Files with\n"
               "no special attribute are left out entirely (100644 regular\n"
               "files and symlinks are standard and never reported), so a\n"
               "project with nothing to report prints nothing: this\n"
               "command is silent by design, unlike `list-frozen-mtimes`,\n"
               "which hard-errors when nothing is frozen. With no paths,\n"
               "all tracked files of the project are considered; a path\n"
               "that names a directory considers everything tracked under\n"
               "it, recursively; a path that names a file considers just\n"
               "that file. Path forms are the same as for `add`\n"
               "(CWD-relative, absolute, or workdir-relative), and a path\n"
               "naming the workdir itself means the whole tree. A\n"
               "requested file that does not exist in the workdir, or is\n"
               "not tracked in the project, is a hard error naming it. A\n"
               "tracked file that has disappeared from the workdir only\n"
               "ever reports frozen-mtime (its mode is gone with it).\n",
               t);
        return 0;
    }
    if (topic == "hash") {
        printf("%s hash <file>\n"
               "\n"
               "Print the blake3 hash of a file as 64 lowercase hex chars,\n"
               "followed by a newline, and nothing else — so the output can\n"
               "be pasted straight into a .projeny file's\n"
               "`URL: <url> <blake3-hash>` header. The URL:-based form\n"
               "records the hash of the tarball's bytes so every download\n"
               "can be verified; `projeny hash <tarball>` is how that hash\n"
               "is computed. The file must exist and be a regular file.\n",
               t);
        return 0;
    }
    if (topic == "download") {
        printf("%s download <url> <blake3-hash> [<url> <blake3-hash>...]\n"
               "\n"
               "Download every URL into the current directory, named after\n"
               "the URL's basename (https://foo.dev/foo-1.2.3.tar.gz is\n"
               "saved as foo-1.2.3.tar.gz), and verify each download\n"
               "against its blake3 hash — the same hash `projeny hash`\n"
               "computes, and the same 64-hex-char form a .projeny file's\n"
               "`URL: <url> <blake3-hash>` header wants. Each URL must name\n"
               "the archive file itself (the basename names the output).\n"
               "\n"
               "The downloads run as one parallel batch, exactly like the\n"
               "download phase of parallel setup/package/extract: at most\n"
               "-c/--curl-jobs transfers in flight (default 8) and blake3\n"
               "hash checks on at most -j/--jobs threads (default the CPU\n"
               "count), with one retry pass over the failures. Packages are\n"
               "deduplicated by archive basename: two URLs sharing a\n"
               "basename download once (the URLs every pair lists are tried\n"
               "first), with a loud (ALL-CAPS) warning when their URL sets\n"
               "differ and a louder one when the same URL is listed with\n"
               "different hashes. Exact duplicate URL HASH pairs are\n"
               "collapsed silently. A file that already exists in the\n"
               "current directory with a matching hash is kept (\"already\n"
               "have <name>\") and not re-downloaded; an existing file that\n"
               "matches no listed hash is replaced by the download.\n"
               "\n"
               "The command exits nonzero — after finishing every other\n"
               "package — when any download fails or no URL yields bytes\n"
               "matching a listed hash; otherwise it exits 0.\n",
               t);
        return 0;
    }
    if (topic == "erase-setup") {
        printf("%s erase-setup <f.projeny|dir> [...] [--erase-snapshots] "
               "[--force]\n"
               "\n"
               "Delete everything a `setup` created for the named projects:\n"
               "the checkout directory (the workdir named by the Name:\n"
               "header — removed recursively, like `rm -rf`, uncommitted\n"
               "changes included), and the .<f>.projeny.status status file\n"
               "(under both of its names: the dotted form and the legacy\n"
               "undotted one). This is DESTRUCTIVE and cannot be undone:\n"
               "any uncommitted changes in the checkout are discarded along\n"
               "with it. The .projeny file itself, the checked-in Archive:\n"
               "tarball, and everything else next to them are left alone.\n"
               "\n"
               "Without --force, nothing is erased until every named\n"
               "project has passed a check: in parallel (subject to\n"
               "-j/--jobs), each project is asked whether a commit would\n"
               "have anything to do — that is, whether `projeny status`\n"
               "reports anything other than untracked files (Conflict:,\n"
               "Added:, Removed:, Renamed:, Modified:, or Disappeared:\n"
               "entries; untracked files alone are fine to blow away). If\n"
               "ANY project is dirty, the whole invocation refuses with one\n"
               "error and exit status 1, naming each dirty project and its\n"
               "changes — and NOTHING is erased, not even the clean\n"
               "projects' setup state (`refusing to erase 1 of 2\n"
               "project(s) with uncommitted changes (use --force to erase\n"
               "anyway)`). A project whose state cannot even be assessed\n"
               "— a missing or unparseable .projeny file, a status file\n"
               "that cannot be read, or a checkout whose archive and its\n"
               "snapshot are both missing or unusable, so the live diff\n"
               "that would compare the checkout against the recorded tree\n"
               "cannot run — refuses the whole invocation the same way\n"
               "(a checkout that happens to be clean refuses too: erase-\n"
               "setup cannot know it is clean; --force is the override).\n"
               "A project whose checkout directory is already gone, or\n"
               "that has no status file, is clean by definition: there is\n"
               "nothing there to destroy, so it erases normally (with the\n"
               "usual did-not-exist warnings).\n"
               "\n"
               "With --force, the check is skipped and erase-setup behaves\n"
               "unconditionally: every named project is erased no matter\n"
               "what its status reports — that is the escape hatch for\n"
               "deliberately discarding uncommitted work.\n"
               "\n"
               "Each named project is erased independently, on at most\n"
               "-j/--jobs threads (default the CPU count; -j0 and non-\n"
               "numeric values are refused). A checkout that is already\n"
               "gone, or a status file that does not exist, only prints a\n"
               "warning ('<path>' did not exist; nothing to erase) — a\n"
               "missing thing is never an error. Anything that cannot be\n"
               "deleted prints an error naming the path and fails that\n"
               "project, but erase-setup still tries to finish the rest of\n"
               "that project's deletions and the other projects': a failed\n"
               "run ends with one summary line (`projeny: 1 of 2\n"
               "erase-setup(s) failed: f.projeny`) and exit status 1 after\n"
               "everything else finished. Each successful project reports\n"
               "one line naming what it erased (`erased setup state for\n"
               "'<Name>' (checkout '<dir>', status '<statusfile>'[, snapshot\n"
               "'<snap>'])`); a project with nothing left reports `nothing\n"
               "to erase for '<Name>'`.\n"
               "\n"
               "The crash-recovery setup journal (<f>.projeny.setup-journal)\n"
               "is deleted too, silently: it is not worth a warning when\n"
               "absent, and leaving it behind would make the next `setup`\n"
               "run crash recovery against a checkout that no longer\n"
               "exists.\n"
               "\n"
               "With --erase-snapshots, the .<archive>.snapshot file setup\n"
               "would have used is deleted as well — for a URL:-based\n"
               "project the snapshot named after the first URL's basename,\n"
               "for a classic project the snapshot of the Archive: tarball\n"
               "(the legacy undotted <Archive>.snapshot form goes too, since\n"
               "setup would migrate it back into use), and for a\n"
               "Projeny:-based project the frozen base snapshot\n"
               ".<stem>.snapshot next to the .projeny file (bar.projeny\n"
               "keeps .bar.snapshot), which has no legacy undotted form.\n"
               "ONLY that exact\n"
               "snapshot is deleted: a similarly named snapshot for a\n"
               "different version of the archive (the .<old-archive>.snapshot\n"
               "a rebase left behind, say) survives, and the checked-in\n"
               "tarball itself is never touched. The next setup then\n"
               "re-downloads (URL: projects), unpacks from the checked-in\n"
               "tarball (Archive: projects), or re-freezes from the live\n"
               "base file (Projeny: projects). --force combines freely with\n"
               "--erase-snapshots.\n"
               "\n"
               "The .projeny file must be readable and parseable — it names\n"
               "what would be deleted, so a missing, garbage, or\n"
               "git-conflicted file fails its own project without anything\n"
               "being erased for it (the other projects still erase). With\n"
               "--force that means the other projects erase; without it,\n"
               "the whole invocation refuses before erasing anything.\n"
               "Naming one project twice collapses into a single erase with\n"
               "a warning, keyed on the resolved .projeny path.\n"
               "\n"
               "Like every project-taking command, the <f.projeny> argument\n"
               "may also be the workdir or another directory holding exactly\n"
               "one .projeny file, or a path whose '<arg>.projeny' sibling\n"
               "exists (typically the checkout directory itself, or a bare\n"
               "name like 'fake' for 'fake.projeny').\n",
               t);
        return 0;
    }
    if (topic == "help") {
        printf("%s help [command]\n"
               "\n"
               "With no arguments, list all commands. With a command name\n"
               "(setup, commit, add, rm, mv, resolve, rebase, create,\n"
               "status, diff, patch, apply, package, extract, download,\n"
               "erase-setup, freeze-mtime, unfreeze-mtime,\n"
               "list-frozen-mtimes, get-attributes, hash, help), print a\n"
               "detailed explanation of that command.\n",
               t);
        return 0;
    }
    fprintf(stderr, "projeny: error: unknown help topic '%s'\n",
            topic.c_str());
    return 1;
}
