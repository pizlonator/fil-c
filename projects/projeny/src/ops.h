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
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

// Resolved locations for "<pdir>/<archive>", "<pdir>/<name>/", etc.
// `projeny_arg` is the .projeny path exactly as given on the command line
// (relative paths resolve against the caller's CWD).
struct Ctx {
    std::string projeny_arg;
    std::string pdir;       // directory containing the .projeny file
    std::string filebase;   // the .projeny file's basename ("dash.projeny"):
                            // scratch dirs are named ".<filebase>.<role>.X"
                            // next to the file (see make_scratch in ops.cc)
    std::string statusfile; // dot-prefixed ".<projeny path>.status" (a
                            // legacy undotted "<projeny path>.status" is
                            // renamed into place on first use)
};

Ctx resolve_ctx(const std::string& projeny_arg);

int cmd_setup(const std::string& projeny_arg);
// Parallel multi-project setup: one cmd_setup per argument, run on at most
// `jobs` threads after one batched download pass over every URL: header in
// every named project (at most `curl_jobs` transfers in flight). A single
// argument runs the plain single-project command, byte-identically.
int cmd_setup_multi(const std::vector<std::string>& projeny_args, int jobs,
                    int curl_jobs);
int cmd_commit(const std::string& projeny_arg);
int cmd_add(const std::string& projeny_arg, const std::string& path);
int cmd_rm(const std::string& projeny_arg, const std::string& path);
int cmd_mv(const std::string& projeny_arg, const std::string& src,
           const std::string& dst);
int cmd_resolve(const std::string& projeny_arg, const std::string& path);

// Move the project's patch onto a new base archive. `new_archive_args` is
// the tail after the project argument: either exactly one local tarball
// path (legacy rebase of an Archive:-based project), or one or more URLs
// each optionally followed by its blake3 hash (the new URL: headers; also
// converts an Archive:-based project to URL:-based). A URL:-based project
// accepts only URLs.
int cmd_rebase(const std::string& projeny_arg,
               const std::vector<std::string>& new_archive_args);

// Create a brand-new .projeny file at `file_arg` (which must end in
// ".projeny"; its pdir — the dirname — must exist) naming the archive in
// `archive_args` — rebase's exact grammar: one local tarball path (copied
// into the pdir) or one or more <url> [<blake3-hash>] pairs (downloaded,
// verified, and recorded as URL: headers with the first verified download
// cached as the snapshot) — and then run `setup` on it. `comment` is the
// file's prose (--comment, empty when absent). `origname_opt` (--origname)
// names the checkout directory: the file gets Name: <name> AND
// Origname: <name>, which must match the archive's top-level directory.
// `force_level` counts --force: 0 refuses an existing file; 1 erases the
// existing project's setup state with the no-force check; 2+ erases
// unconditionally. A pre-existing <pdir>/<origname> directory is erased at
// force >= 1 only when it is attributable, and attribution consults two
// sources in order: the file being replaced (when it exists, parses, and
// has Name: <name>, its own erase then cleans up the checkout) and then
// the <name>.projeny sibling, which is evicted; the guard refuses — at
// any force level — only when neither source attributes the directory.
// `erase_snapshots` passes through to every erase.
int cmd_create(const std::string& file_arg,
               const std::vector<std::string>& archive_args,
               const std::string& comment,
               const std::optional<std::string>& origname_opt,
               int force_level, bool erase_snapshots);
int cmd_status(const std::string& projeny_arg);
int cmd_diff_projeny(const std::string& projeny_arg);
int cmd_diff(const std::string& dir, const std::string& other_dir);
int cmd_patch(const std::string& dir, const std::string& patch_file);

// Apply a patch file (any git diff, any projeny diff — including `projeny
// diff` output) inside a projeny checkout, on top of the uncommitted
// changes it already has. Files the patch adds are marked added and files
// it deletes removed in the status file; renames are recorded as pending
// renames and mode changes (exec bit, symlinks) are applied to the files.
// Already-applied blocks are skipped (re-applying is safe). Blocks that do
// not apply get inline conflict markers (<<<<<<< current / ======= /
// >>>>>>> patched) and their files are marked conflicted; the command then
// exits 1. When the patch touches a file that is already marked conflicted
// nothing is applied at all and the command hard-errors.
int cmd_apply(const std::string& projeny_arg, const std::string& patch_file);
int cmd_package(const std::string& projeny_arg, const std::string& output);
int cmd_extract(const std::string& projeny_arg, const std::string& dest_dir);
// Parallel multi-project package/extract: (project, output/dest) pairs run
// on at most `jobs` threads after one batched download pass (see
// cmd_setup_multi). A single pair runs the plain single-project command,
// byte-identically.
int cmd_package_multi(const std::vector<std::pair<std::string, std::string>>& pairs,
                      int jobs, int curl_jobs);
int cmd_extract_multi(const std::vector<std::pair<std::string, std::string>>& pairs,
                      int jobs, int curl_jobs);
// Download URL HASH pairs (args is a flat url,hash,url,hash... list,
// validated here) into the current directory as one parallel batch: at
// most `curl_jobs` transfers in flight, blake3 hash checks on at most
// `jobs` threads. Files are named after the URL's basename.
int cmd_download(const std::vector<std::string>& args, int jobs, int curl_jobs);

// Erase every project's setup state (always the parallel machinery, even
// for one argument): the checkout (pdir/<Name>, removed recursively —
// uncommitted changes included), the dotted .<f>.projeny.status status
// file plus its legacy undotted form, and the <f>.projeny.setup-journal
// crash-recovery file (silently). With `erase_snapshots`, the exact
// .<archive>.snapshot the next setup would use goes too (never a
// similarly named snapshot, never the checked-in tarball). A missing
// thing warns and counts as success; a deletion that fails prints an
// error, fails its project, and the remaining deletions still run. At
// most `jobs` threads.
//
// Without `force`, nothing is erased until every project passes a parallel
// check phase (subject to `jobs`): each project must report nothing a
// commit would fold in — `projeny status` other than untracked files. Any
// dirty project, or any project whose state cannot be assessed (unreadable
// or unparseable .projeny, a status computation that dies, or a checkout
// whose archive and its snapshot are both missing or unusable, so the live
// diff cannot run — such a project is never assumed clean, even a clean
// one), refuses the whole invocation with one die() report and exit 1,
// erasing nothing. `force` skips the check and erases unconditionally.
int cmd_erase_setup_multi(const std::vector<std::string>& projeny_args,
                          int jobs, bool erase_snapshots, bool force);
int cmd_freeze_mtime(const std::string& projeny_arg,
                     const std::vector<std::string>& files);
int cmd_unfreeze_mtime(const std::string& projeny_arg,
                       const std::vector<std::string>& files);
int cmd_list_frozen_mtimes(const std::string& projeny_arg);
int cmd_get_attributes(const std::string& projeny_arg,
                       const std::vector<std::string>& paths);
// Print the blake3 hash (64 lowercase hex chars) of a regular file, for
// pasting into a .projeny file's "URL: <url> <hash>" header.
int cmd_hash(const std::string& path);
int cmd_help(const std::string& arg0);
int cmd_help_topic(const std::string& arg0, const std::string& topic);
