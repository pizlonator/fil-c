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
// Tree unpack/diff/apply helpers shared by the ops.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "vcs.h"

// Unpack `archive` (any tar format tar auto-detects) into `destdir` and
// require that it produce exactly one top-level entry named `expect_top`.
// Hard error otherwise. Dies with a clear message if tar is missing.
void unpack_single_top(const std::string& archive, const std::string& destdir,
                       const std::string& expect_top);

// Return names (no . / ..) of the single top-level entry of the archive, or
// die. Used by rebase to discover the new Origname.
std::string archive_single_top_name(const std::string& archive);

// One parsed "diff --git" block.
struct FileDiff {
    std::string text; // raw block text including trailing newline
    std::string a_path; // old-side repo path ("" for pure additions)
    std::string b_path; // new-side repo path ("" for pure deletions)
};

// Split a unified diff into FileDiff blocks. Handles "diff --git" plus the
// "diff --cc"/"diff --combined" variants git merge produces (folded into the
// git form where possible); anything else starts a new opaque block so we
// never silently merge two files' hunks.
std::vector<FileDiff> split_file_diffs(const std::string& patch);

// Rewrite a WID-label diff (labels "a/<wid>/..." / "b/<wid>/...") into
// plain a//b/ label form relative to the workdir root, for internal -p1
// application with cwd=workdir (strip "a"/"b" and <wid>; tolerate an absent
// wid). keep_wid=true keeps the wid component. Returns the rewritten patch.
std::string relabel_wid_patch(const std::string& patch, const std::string& wid,
                              bool keep_wid);

// Labels in freshly generated patches are canonical: "a/<wid>/..." /
// "b/<wid>/..." where wid is the workdir basename, rename lines are bare
// workdir-relative paths, and no absolute paths appear anywhere.
std::string canonicalize_git_diff(const std::string& patch, const std::string& wid,
                                  const std::string& base_abs,
                                  const std::string& work_abs);

// Unified-diff hunk headers carry stale line counts after we split/rewrite
// patches; recount them so application never trips on count mismatches.
std::string recount_patch(const std::string& patch);

// Wrap binary-safe invariant: these helpers operate on text patches. A patch
// containing NUL bytes is a hard error (it came from a text file anyway).
void require_text_patch(const std::string& patch, const std::string& what);

// Normalize for patch comparison and storage: unified diffs encode a blank
// context line as "" (empty) while some producers write it as " " (one
// space); map the latter to the former. All other lines (including context/added lines with
// significant trailing spaces or tabs) are preserved EXACTLY. Only the final
// line terminator is normalized: the result ends with exactly one '\n' when
// non-empty ("" stays ""). In particular, NO blanket right-trimming is done.
std::string normalize_patch_text(const std::string& patch);

// Compute the user diff: unpack `archive` (expecting top dir `origname`) to a
// temp dir and diff base-tree vs `workdir` internally. Labels are
// a/<wid>/... and b/<wid>/... where wid is the workdir basename.
std::string diff_workdir_vs_base(const std::string& archive,
                                 const std::string& origname,
                                 const std::string& workdir,
                                 const std::string& scratch_parent);

// Same but between two on-disk trees (used at commit). Includes rename
// detection so pending Renamed ops render as rename diffs.
std::string diff_trees(const std::string& base_tree, const std::string& workdir,
                       const std::string& wid);

// Apply `patch` (WID-label form, wid == `wid`) inside `treedir` with -p1
// semantics. `wid` is the patch's workdir name, which may differ
// from basename(treedir) for unpacked "origname" trees. Returns true on full
// success. On failure the tree may be partially patched; callers prefer the
// per-file path below for merge.
bool apply_patch_whole(const std::string& treedir, const std::string& patch,
                       const std::string& wid, const std::string& scratch_parent);

// Apply each file block of `patch` individually with -p1 semantics.
// Returns per-block failures with workdir-relative paths (single-parser
// source of truth; no indices into any other parser's output).
// Blocks already applied are detected (reverse-match) and skipped.
std::vector<VcsFailure> apply_patch_per_file(const std::string& workdir,
                                              const std::string& patch,
                                              const std::string& wid);

// Apply `patch` (WID-label form, wid == `wid`) inside `treedir`, writing
// git-style conflict markers ("<<<<<<< current" / "=======" /
// ">>>>>>> patched") inline for blocks that do not apply cleanly and
// appending their workdir-relative paths to `conflicts`. Returns true when
// everything applied cleanly. Used by `projeny patch`.
bool apply_patch_with_conflicts(const std::string& treedir,
                                const std::string& patch, const std::string& wid,
                                const std::string& scratch_parent,
                                std::vector<std::string>* conflicts);

// Workdir-relative paths `patch` touches under `wid` (see vcs_touched_paths).
std::vector<std::string> patch_touched_paths(const std::string& patch,
                                             const std::string& wid);

// Three-way merge one file: base (may be missing), ours (fresh setup file,
// may be missing), theirs (user file, may be missing) -> write merged result
// with conflict markers into dst_path. dst_root is the tree dst_path lives
// in (dst_path is join_path(dst_root, rel)); it names the member in
// diagnostics and bounds symlink targets the merge may create. Returns true
// if clean.
bool merge_one_file(const std::string& base_file, const std::string& ours_file,
                    const std::string& theirs_file, const std::string& dst_path,
                    const std::string& dst_root);
