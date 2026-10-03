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

#include <string>
#include <utility>
#include <vector>

// Resolved locations for "<pdir>/<archive>", "<pdir>/<name>/", etc.
// `projeny_arg` is the .projeny path exactly as given on the command line
// (relative paths resolve against the caller's CWD).
struct Ctx {
    std::string projeny_arg;
    std::string pdir;       // directory containing the .projeny file
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
int cmd_rebase(const std::string& projeny_arg, const std::string& new_tarball);
int cmd_status(const std::string& projeny_arg);
int cmd_diff_projeny(const std::string& projeny_arg);
int cmd_diff(const std::string& dir, const std::string& other_dir);
int cmd_patch(const std::string& dir, const std::string& patch_file);
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
