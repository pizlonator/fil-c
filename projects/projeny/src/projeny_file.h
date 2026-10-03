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

// One "URL: <url> <blake3-hash>" header line (see ProjenyFile::urls). The
// hash is stored lowercase; the URL is stored exactly as written.
struct ProjenyUrl {
    std::string url;
    std::string hash;
};

// A parsed .projeny file.
//
// Layout: "Key: value" headers at the top, terminated by a blank line,
// then free text and a git-style patch. The required keys are Origname:,
// Name:, and either Archive: (a tarball checked into git next to the
// .projeny file) or one or more URL: lines (the tarball is downloaded and
// verified); everything else is preserved verbatim:
//   head    = the header block (without the trailing blank line)
//   patch   = everything from the "diff --git" block onward (may be empty)
//   middle  = free text between the blank line and the first "diff --git"
struct ProjenyFile {
    std::string raw;    // full verbatim bytes
    std::string head;   // headers, no trailing blank line
    std::string middle; // free text between headers and patch
    std::string patch;  // from first "diff --git " line to EOF (or "")
    std::string archive;
    std::string origname;
    std::string name;
    // The "URL: <url> <blake3-hash>" headers, in file order. Empty for the
    // classic Archive:-based form. When non-empty, Archive: is absent and
    // `archive` holds the name derived from the FIRST URL's basename: the
    // downloaded tarball is cached (and verified against the hashes) as the
    // .<archive>.snapshot next to the .projeny file, exactly where a classic
    // project's snapshot copy lives.
    std::vector<ProjenyUrl> urls;

    // True when the tarball is fetched from URL: lines instead of a checked-in
    // Archive: tarball.
    bool is_url_based() const { return !urls.empty(); }

    static ProjenyFile parse(const std::string& path);
    static ProjenyFile parse_bytes(const std::string& data,
                                   const std::string& what_for_errors);
    std::string header_value(const std::string& key) const; // "" if absent

    // Rebuild raw from head/middle/patch (headers kept verbatim, patch body
    // replaced). Used by commit/rebase/freeze-mtime/unfreeze-mtime to
    // preserve the user's headers.
    void rebuild(const std::string& new_patch);
};

// The archive filename a URL implies (the URL's basename): everything up to
// and including "://" is stripped (any scheme), then any "?query" or
// "#fragment", then everything up to the last '/'. Returns "" when the
// result would be empty, ".", or ".." — i.e. when the URL does not name the
// archive file itself (projeny needs the basename to name the snapshot).
// Also used best-effort when extracting the archive name from a status
// file's embedded .projeny copy.
std::string archive_name_from_url(const std::string& url);

// Git conflict-marker helpers for .projeny files.
//
// A marker line is one of <<<<<<< / ||||||| / ======= / >>>>>>> at column 0,
// each possibly extended with extra marker characters for nested conflicts
// (git adds one character per nesting level, e.g. "<<<<<<<<" / "========"),
// with trailing label text allowed (e.g. "<<<<<<< HEAD"). This can never occur
// in a well-formed .projeny file: headers are "Key: value" lines, the patch
// section's structural lines all start with other characters (d/-/+/@@/i/n/
// o/s/r/B/\), and every hunk-body line carries a ' '/'+'/'-' prefix (or is
// a blank context line), so marker-shaped content always has a prefix byte.
// Free-text prose in the middle section is kept unambiguous the same way:
// tool-written files never contain a column-0 prose line (rebuild() prepends
// a single space to any non-empty line missing its indent, idempotently),
// so a column-0 marker-shaped line is always a genuine git conflict marker.
// Hand-written prose must follow the same rule — indent every non-empty
// line with a leading space — or the file is refused as conflicted
// (documented in help).
bool is_projeny_marker_opener(const std::string& line);
bool is_projeny_marker_base(const std::string& line);
bool is_projeny_marker_divider(const std::string& line);
bool is_projeny_marker_closer(const std::string& line);
bool is_projeny_marker_line(const std::string& line);

// True when `data` contains any git conflict-marker line at column 0
// (opener, base, divider, or closer). Partial markers (e.g. a truncation
// that ate the rest of the block) still count; split_projeny_conflicts()
// then rejects them as malformed.
bool projeny_has_conflict_markers(const std::string& data);

// Split a conflicted .projeny file into its two sides by force-resolving
// every conflict block. Dies when the markers are malformed (unbalanced
// opener/divider/closer, nested opener, EOF inside a block). Both sides are
// rejoined with '\n' (CRLF input is normalized to LF; callers warn about
// that separately). The sides are the raw first/second halves: which one is
// local vs upstream depends on the git operation (merge keeps ours=local,
// rebase/stash-pop swap them), so callers must resolve the direction from
// opener_label/closer_label and the status copy — see setup_conflicted.
struct ProjenyConflictSplit {
    bool conflicted = false;
    std::string ours;
    std::string theirs;
    // Branch labels from the first conflict block ("<<<<<<< <opener>" and
    // ">>>>>>> <closer>", e.g. "HEAD" / "branch", "Updated upstream" /
    // "Stashed changes"). Empty when the markers carry no labels.
    std::string opener_label;
    std::string closer_label;
};
ProjenyConflictSplit split_projeny_conflicts(const std::string& data,
                                             const std::string& what_for_errors);

// Non-dying validation used by `setup` to give git-recovery guidance:
// returns "" when `data` is a usable .projeny file, else a short reason
// ("conflict markers", "NUL bytes", "missing headers", ...).
std::string validate_projeny_bytes(const std::string& data);

// Status file bookkeeping (see projeny_file.cc for the format).
struct StatusData {
    std::string status; // e.g. "setup"
    std::vector<std::string> conflicts;
    std::vector<std::string> added;
    std::vector<std::string> removed;
    // Renamed pairs are (src, dst), workdir-relative paths.
    std::vector<std::pair<std::string, std::string>> renamed;
    std::string embedded; // verbatim .projeny copy, exactly as stored

    static StatusData parse(const std::string& path);
    static StatusData parse_bytes(const std::string& data,
                                  const std::string& what_for_errors);
    std::string serialize() const;
};

extern const char* kStatusDelim;
