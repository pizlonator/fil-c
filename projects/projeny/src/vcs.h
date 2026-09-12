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
// Internal git-compatible diff/patch/merge (no git binary invocation).
//
// diff output is unified with `diff --git a/<wid>/... b/<wid>/...` labels,
// `---`/`+++`, `@@` hunks, new/deleted file entries and rename entries, so
// `git apply` and `patch -p1` accept text hunks. Binary (NUL-bearing) files
// are carried as base64 `GIT binary patch` literal sections (projeny-internal
// encoding, documented in vcs.cc); the applier accepts them plus git-style
// diffs (a/b prefixes, /dev/null sides, new/deleted/old/new mode lines,
// rename from/to, `\ No newline at end of file`) with fuzz and -p1 semantics.
// A git `Binary files ... differ` stanza without a payload, or a foreign
// `GIT binary patch` section our base64 decoder rejects, parses as a binary
// block with no payload: it applies as a per-file failure (merge conflict),
// never as a hard error and never silently.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Compute the user diff between two on-disk trees. Labels are
// a/<wid>/... and b/<wid>/... where wid is the workdir basename.
// Includes rename detection. Binary files are emitted as base64 binary
// blocks (add/delete/modify/rename-with-content); pure renames and
// mode-only changes of binaries carry no payload.
std::string vcs_diff_trees(const std::string& base_tree, const std::string& workdir,
                           const std::string& wid);

// Drop pure-deletion blocks whose deleted path is not in `keep` (workdir-
// relative). Used by setup to restore files that vanished without an
// explicit `projeny rm` (or rename source): only deletions listed in `keep`
// (pending removals plus pending rename sources) survive; every other
// deletion block (text or binary) is removed so a fresh setup restores the
// file. Rename, modify, add, and mode-only blocks are always kept.
std::string vcs_drop_deletes_not_in(const std::string& patch,
                                    const std::string& wid,
                                    const std::vector<std::string>& keep);

// Workdir-relative new paths of binary-add blocks in `patch` (binary blocks
// with a payload that create a file). Used by commit to tell previously
// committed binary adds (tracked: keep them on every recommits) apart from
// truly untracked binaries (leave them out).
std::vector<std::string> vcs_binary_add_paths(const std::string& patch,
                                              const std::string& wid);

// Workdir-relative new paths of pure-add blocks in `patch` (text, symlink
// and binary adds that create a file, excluding renames). Used by commit to
// tell previously committed adds (tracked: keep them on every recommit)
// apart from truly untracked files (leave them out).
std::vector<std::string> vcs_add_paths(const std::string& patch,
                                       const std::string& wid);

// Workdir-relative deleted paths of pure-deletion blocks in `patch`
// (excluding renames). Used by commit to tell previously committed deletes
// (tracked: still deleted) apart from new disappearances.
std::vector<std::string> vcs_deleted_paths(const std::string& patch,
                                           const std::string& wid);

// Drop pure-add blocks whose new path is not in `keep` (workdir-relative,
// exact or under-a-kept-dir). Used by commit so untracked files (never
// committed, never `add`ed) stay out of the patch like git leaves untracked
// files out, while explicitly `add`ed files and pending rename destinations
// (listed in `keep`) are carried. Rename, modify, delete and mode-only
// blocks are always kept.
std::string vcs_drop_adds_not_in(const std::string& patch,
                                 const std::string& wid,
                                 const std::vector<std::string>& keep);

// Drop binary-add blocks whose new path is not in `keep` (workdir-relative).
// Used by commit so untracked binaries (build junk, package tarballs written
// into the workdir) stay out of the patch like git leaves untracked files
// out, while explicitly `add`ed binaries and pending rename destinations
// (listed in `keep`) are carried. Text adds are never dropped.
std::string vcs_drop_binary_adds_not_in(const std::string& patch,
                                        const std::string& wid,
                                        const std::vector<std::string>& keep);

// Apply `patch` (wid-label form, wid == `wid`) inside `treedir`.
// Returns true on full success; the tree may be partially patched on failure.
// Blocks already applied are skipped. Binary blocks compare and write raw
// bytes; unparseable binary blocks fail instead of dying.
bool vcs_apply_whole(const std::string& treedir, const std::string& patch,
                     const std::string& wid);

// Apply each file block of `patch` individually. Returns per-block failures
// with workdir-relative paths (no indices into any other parser's output).
// Blocks already applied are skipped. Each entry carries a display name for
// error messages plus the list of workdir-relative files it touches for
// three-way merging (empty when the block has no parseable path, e.g. an
// unsupported combined-diff block).
struct VcsFailure {
    std::string display;
    std::vector<std::string> paths;
};

std::vector<VcsFailure> vcs_apply_per_file(const std::string& workdir,
                                            const std::string& patch,
                                            const std::string& wid);

// Apply `patch` (wid-label form, wid == `wid`) inside `treedir`, writing
// git-style conflict markers inline for blocks that do not apply cleanly.
// Clean blocks (including already-applied ones) are applied as usual; each
// failed block keeps the successfully applied hunks (modify blocks) or the
// current file (add/delete/rename blocks, including binary blocks, which
// never get inline markers) and records its workdir-relative
// path(s) in `conflicts` (callers sort/unique the list). Markers use
// "<<<<<<< current" / "=======" / ">>>>>>> patched" labels. Returns true
// when everything applied cleanly.
bool vcs_apply_with_conflicts(const std::string& treedir,
                              const std::string& patch, const std::string& wid,
                              std::vector<std::string>* conflicts);

// All workdir-relative paths `patch` touches under `wid` (both sides of
// renames; display fallback when a block has no parseable path). Used to
// pick the right wid for `projeny patch` without trial application.
std::vector<std::string> vcs_touched_paths(const std::string& patch,
                                           const std::string& wid);

// Three-way merge one file: base (may be missing), ours (fresh setup file,
// may be missing), theirs (user file, may be missing) -> write merged result
// with conflict markers into dst_path. dst_root is the tree dst_path lives
// in (dst_path is join_path(dst_root, rel)); it names the member in
// diagnostics and bounds symlink targets the merge may create. Returns true
// if clean.
// Binary (NUL-bearing) sides merge byte-wise: agreement or a one-sided change
// wins cleanly, divergent binary changes keep the theirs bytes and report a
// conflict (markers would corrupt binary content).
bool vcs_merge_one_file(const std::string& base_file, const std::string& ours_file,
                        const std::string& theirs_file, const std::string& dst_path,
                        const std::string& dst_root);
