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
#include "projeny_file.h"

#include "util.h"

#include <cctype>
#include <cstring>

const char* kStatusDelim = "--- projeny content ---";

namespace {
// Status value lines look like "Key: <escaped-path>". Only the single
// delimiter space after the colon is stripped; any further leading spaces
// (and all trailing spaces) belong to the filename and are preserved.
std::string strip_one_space(const std::string& v)
{
    if (!v.empty() && v[0] == ' ')
        return v.substr(1);
    return v;
}
// Normalize free-text prose so tool-written files never contain a
// column-0 line: every non-empty line is made to start with a space or tab
// (a single space is prepended when missing). Git conflict markers always
// sit at column 0 unprefixed, so after this normalization any column-0
// marker-shaped line in a tool-written file can only be a genuine git
// conflict — detection is exact, with no false positives from prose.
// Idempotent: already-indented lines are kept verbatim, so existing
// indented prose roundtrips byte-for-byte; hand-written prose migrates on
// the next commit/rebase. Empty lines stay empty.
std::string normalize_prose_indent(const std::string& middle)
{
    if (middle.empty())
        return middle;
    bool ends_nl = middle.back() == '\n';
    std::vector<std::string> lines;
    size_t i = 0;
    while (i < middle.size()) {
        size_t j = middle.find('\n', i);
        if (j == std::string::npos) {
            lines.push_back(middle.substr(i));
            break;
        }
        lines.push_back(middle.substr(i, j - i));
        i = j + 1;
    }
    // A trailing '\n' terminates the last line rather than starting an
    // empty one (matches split_lines).
    if (ends_nl && !lines.empty() && lines.back().empty())
        lines.pop_back();
    for (auto& l : lines) {
        if (!l.empty() && l[0] != ' ' && l[0] != '\t')
            l = " " + l;
    }
    std::string out;
    for (auto& l : lines) {
        out += l;
        out += '\n';
    }
    if (!ends_nl && !out.empty())
        out.pop_back();
    return out;
}

// A header line is "Key: value" where Key holds no spaces/tabs/colon and the
// line is not indented (indented lines start the free-text section, per the
// spec example which shows 4-space-indented text after the blank line).
bool is_header_line(const std::string& line)
{
    if (line.empty() || line[0] == ' ' || line[0] == '\t')
        return false;
    size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0)
        return false;
    for (size_t i = 0; i < colon; ++i) {
        char c = line[i];
        if (c == ' ' || c == '\t')
            return false;
    }
    return true;
}
}

static bool is_marker_prefix(const std::string& line, const char* mark)
{
    size_t n = strlen(mark);
    if (line.compare(0, n, mark) != 0)
        return false;
    // Extended (nested-conflict) markers run longer than 7: git adds one
    // character per nesting level (e.g. "<<<<<<<<" / "========" for the
    // outer block of a nested conflict). Accept a run of 7 or more marker
    // characters, then require EOL or a label separator exactly as for the
    // plain 7-char form; anything else (e.g. "<<<<<<<<?") is not a marker.
    size_t i = n;
    while (i < line.size() && line[i] == mark[0])
        ++i;
    return line.size() == i || line[i] == ' ' || line[i] == '\t';
}

bool is_projeny_marker_opener(const std::string& line)
{
    return is_marker_prefix(line, "<<<<<<<");
}

bool is_projeny_marker_base(const std::string& line)
{
    return is_marker_prefix(line, "|||||||");
}

bool is_projeny_marker_divider(const std::string& line)
{
    return is_marker_prefix(line, "=======");
}

bool is_projeny_marker_closer(const std::string& line)
{
    return is_marker_prefix(line, ">>>>>>>");
}

bool is_projeny_marker_line(const std::string& line)
{
    return is_projeny_marker_opener(line) || is_projeny_marker_base(line) ||
           is_projeny_marker_divider(line) || is_projeny_marker_closer(line);
}

bool projeny_has_conflict_markers(const std::string& data)
{
    // Any marker line at column 0 means the file went through a conflicted
    // merge (git never emits partial markers on success, and a truncation
    // that eats the rest of a block must not look clean). split() then
    // validates the block structure and dies on malformed markers.
    // (split_lines strips one trailing '\r' per line, so CRLF checkouts
    // are detected too.)
    for (const std::string& line : split_lines(data)) {
        if (is_projeny_marker_line(line))
            return true;
    }
    return false;
}

ProjenyConflictSplit split_projeny_conflicts(const std::string& data,
                                             const std::string& what_for_errors)
{
    ProjenyConflictSplit out;
    if (!projeny_has_conflict_markers(data))
        return out;
    std::vector<std::string> lines = split_lines(data);
    std::vector<std::string> ours, theirs;
    enum State { Normal, InOurs, InBase, InTheirs };
    State st = Normal;
    bool saw_opener = false, saw_closer = false;
    auto marker_label = [](const std::string& l) -> std::string {
        // l starts with a run of 7+ marker characters plus EOL/space/tab
        // (see is_marker_prefix); the label is whatever follows the run,
        // trimmed. (Skipping the whole run matters for extended markers:
        // the label of "<<<<<<<< my branch" is "my branch", not "< my
        // branch".)
        size_t i = 0;
        char m = l.empty() ? '\x00' : l[0];
        while (i < l.size() && l[i] == m)
            ++i;
        return trim(l.substr(i));
    };
    for (size_t k = 0; k < lines.size(); ++k) {
        const std::string& l = lines[k];
        switch (st) {
        case Normal:
            if (is_projeny_marker_opener(l)) {
                st = InOurs;
                if (!saw_opener) {
                    out.opener_label = marker_label(l);
                    saw_opener = true;
                }
            } else if (is_projeny_marker_line(l)) {
                die("file " + what_for_errors +
                    " has malformed git conflict markers (stray '" + l +
                    "' outside a conflict block); if this line is free-text "
                    "prose rather than a git conflict, indent it with a "
                    "leading space (prose must not contain column-0 marker "
                    "lines). Otherwise resolve the .projeny file with 'git "
                    "checkout --ours/--theirs' (or edit it by hand) and run "
                    "'projeny setup' again");
            } else {
                ours.push_back(l);
                theirs.push_back(l);
            }
            break;
        case InOurs:
            if (is_projeny_marker_opener(l)) {
                die("file " + what_for_errors +
                    " has malformed git conflict markers (nested '<<<<<<<'); "
                    "resolve the .projeny file with "
                    "'git checkout --ours/--theirs' (or edit it by hand) and "
                    "run 'projeny setup' again");
            } else if (is_projeny_marker_base(l))
                st = InBase;
            else if (is_projeny_marker_divider(l))
                st = InTheirs;
            else if (is_projeny_marker_closer(l)) {
                die("file " + what_for_errors +
                    " has malformed git conflict markers (unexpected "
                    "'>>>>>>>'); resolve the .projeny file with "
                    "'git checkout --ours/--theirs' (or edit it by hand) and "
                    "run 'projeny setup' again");
            } else
                ours.push_back(l);
            break;
        case InBase:
            if (is_projeny_marker_opener(l) || is_projeny_marker_base(l) ||
                is_projeny_marker_closer(l)) {
                die("file " + what_for_errors +
                    " has malformed git conflict markers inside the base "
                    "('|||||||') section; resolve the .projeny file with "
                    "'git checkout --ours/--theirs' (or edit it by hand) and "
                    "run 'projeny setup' again");
            } else if (is_projeny_marker_divider(l))
                st = InTheirs;
            break; // base lines belong to neither side
        case InTheirs:
            if (is_projeny_marker_opener(l) || is_projeny_marker_base(l) ||
                is_projeny_marker_divider(l)) {
                die("file " + what_for_errors +
                    " has malformed git conflict markers inside the upstream "
                    "section; resolve the .projeny file with "
                    "'git checkout --ours/--theirs' (or edit it by hand) and "
                    "run 'projeny setup' again");
            } else if (is_projeny_marker_closer(l)) {
                st = Normal;
                if (!saw_closer) {
                    // Label after the ">>>>>>>" run; captured for direction
                    // detection (rebase/stash swap the sides). The label
                    // skips the whole marker run (see marker_label), so an
                    // extended closer like ">>>>>>>> branch" still yields
                    // "branch".
                    size_t ci = 0;
                    while (ci < l.size() && l[ci] == '>')
                        ++ci;
                    out.closer_label = trim(l.substr(ci));
                    saw_closer = true;
                }
            } else
                theirs.push_back(l);
            break;
        }
    }
    if (st != Normal) {
        die("file " + what_for_errors +
            " has malformed git conflict markers (block never closed with "
            "'>>>>>>>'); if these lines are free-text prose rather than a "
            "git conflict, indent them with a leading space (prose must not "
            "contain column-0 marker lines). Otherwise resolve the .projeny "
            "file with 'git checkout --ours/--theirs' (or edit it by hand) "
            "and run 'projeny setup' again");
    }
    // Rejoin with '\n', preserving the trailing-newline state. (CRLF input
    // was stripped per line by split_lines, so this normalizes to LF.)
    bool ends_nl = !data.empty() && data.back() == '\n';
    out.ours = join_lines(ours);
    out.theirs = join_lines(theirs);
    if (!ends_nl) {
        if (!out.ours.empty() && out.ours.back() == '\n')
            out.ours.pop_back();
        if (!out.theirs.empty() && out.theirs.back() == '\n')
            out.theirs.pop_back();
    }
    out.conflicted = true;
    return out;
}

std::string archive_name_from_url(const std::string& url)
{
    // Strip the scheme (everything up to and including "://"), if present.
    std::string rest = url;
    size_t scheme = rest.find("://");
    if (scheme != std::string::npos)
        rest = rest.substr(scheme + 3);
    // Strip any ?query or #fragment.
    size_t cut = rest.find_first_of("?#");
    if (cut != std::string::npos)
        rest = rest.substr(0, cut);
    // The archive name is the last path component.
    size_t slash = rest.find_last_of('/');
    std::string name =
        slash == std::string::npos ? rest : rest.substr(slash + 1);
    // The name becomes a filename (the snapshot's), so it must be usable as
    // one: never empty, ".", or "..".
    if (name.empty() || name == "." || name == "..")
        return "";
    return name;
}

namespace {

// A URL: header value must be exactly "<url> <blake3-hash>" (two
// whitespace-separated tokens; the hash is 64 hex chars, stored lowercase).
// Returns false when `value` is malformed (the caller words the error).
bool parse_url_value(const std::string& value, ProjenyUrl* out)
{
    std::string v = trim(value);
    size_t sp = v.find_first_of(" \t");
    if (sp == std::string::npos)
        return false;
    std::string url = v.substr(0, sp);
    std::string hash = trim(v.substr(sp + 1));
    if (url.empty() || hash.size() != 64)
        return false;
    for (char c : hash) {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            return false;
    }
    for (char& c : hash)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    out->url = url;
    out->hash = hash;
    return true;
}

}

std::string validate_projeny_bytes(const std::string& data)
{
    if (contains_nul(data))
        return "contains NUL bytes (binary merge garbage?)";
    if (projeny_has_conflict_markers(data))
        return "contains git conflict markers";
    // Header sanity without dying: required Origname:/Name: plus exactly one
    // of Archive: (checked-in tarball) or at least one URL: line (download).
    bool archive = false, origname = false, name = false, url = false;
    for (const std::string& line : split_lines(data)) {
        if (line.empty())
            break;
        if (line[0] == ' ' || line[0] == '\t')
            break;
        size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0)
            break;
        bool key_ok = true;
        for (size_t i = 0; i < colon; ++i) {
            if (line[i] == ' ' || line[i] == '\t') {
                key_ok = false;
                break;
            }
        }
        if (!key_ok)
            break;
        std::string key = line.substr(0, colon);
        if (key == "Archive")
            archive = true;
        else if (key == "Origname")
            origname = true;
        else if (key == "Name")
            name = true;
        else if (key == "URL")
            url = true;
    }
    if (!origname || !name || (!archive && !url))
        return "is missing a required header (need Archive: or at least one "
               "URL: line, plus Origname: and Name:)";
    return "";
}

std::string ProjenyFile::header_value(const std::string& key) const
{
    for (const std::string& line : split_lines(head)) {
        if (line.size() > key.size() && line.compare(0, key.size(), key) == 0 &&
            line[key.size()] == ':') {
            std::string v = line.substr(key.size() + 1);
            return trim(v);
        }
    }
    return "";
}

ProjenyFile ProjenyFile::parse(const std::string& path)
{
    return parse_bytes(read_file_bytes(path), "'" + path + "'");
}

ProjenyFile ProjenyFile::parse_bytes(const std::string& data,
                                     const std::string& what_for_errors)
{
    ProjenyFile pf;
    pf.raw = data;
    if (contains_nul(data))
        die("file " + what_for_errors + " contains NUL bytes");
    if (projeny_has_conflict_markers(data)) {
        die("file " + what_for_errors +
            " contains git conflict markers; refusing (only 'projeny setup' "
            "can resolve a conflicted .projeny file)");
    }
    // Raw lines (split on '\n', interior '\r' preserved) with byte offsets,
    // so middle/patch extraction below is byte-exact (CRLF content inside
    // hunk bodies survives). Structural tests strip one trailing '\r'.
    std::vector<std::string> lines;
    std::vector<size_t> starts;
    {
        size_t pos = 0;
        starts.push_back(0);
        while (pos < data.size()) {
            size_t nl = data.find('\n', pos);
            if (nl == std::string::npos) {
                lines.push_back(data.substr(pos));
                pos = data.size();
            } else {
                lines.push_back(data.substr(pos, nl - pos));
                pos = nl + 1;
                if (pos < data.size())
                    starts.push_back(pos);
                else {
                    // Trailing newline: no further line (matches split_lines:
                    // a trailing '\n' does not produce a final empty element).
                }
            }
        }
        if (data.empty()) {
            lines.clear();
            starts.clear();
        }
    }
    auto no_cr = [](const std::string& l) -> std::string {
        if (!l.empty() && l.back() == '\r')
            return l.substr(0, l.size() - 1);
        return l;
    };
    auto line_start = [&](size_t k) -> size_t {
        return k < starts.size() ? starts[k] : data.size();
    };

    // Headers: leading Key: lines; terminated by the first blank line or the
    // first non-header line. Extra/unknown headers are preserved verbatim.
    size_t i = 0;
    std::vector<std::string> head_lines;
    while (i < lines.size()) {
        std::string t = no_cr(lines[i]);
        if (t.empty()) {
            ++i; // consume the blank-line terminator
            break;
        }
        if (!is_header_line(t))
            break; // no headers at all; whole file is free text
        head_lines.push_back(t);
        ++i;
        // A blank line ends the header block.
        if (i < lines.size() && no_cr(lines[i]).empty()) {
            ++i;
            break;
        }
    }
    pf.head = join_lines(head_lines);

    // The patch starts at the first "diff --git "/"diff --cc "/"diff
    // --combined " line; tolerate leading whitespace before it. Everything
    // between the headers and the patch is free text (it may itself contain
    // blank lines). Combined diffs are recognized as patch (not free text)
    // so the applier can reject them with a precise error instead of
    // silently ignoring them.
    size_t diff_at = lines.size();
    for (size_t k = i; k < lines.size(); ++k) {
        std::string t = ltrim(no_cr(lines[k]));
        if (starts_with(t, "diff --git ") || starts_with(t, "diff --cc ") ||
            starts_with(t, "diff --combined ")) {
            diff_at = k;
            break;
        }
    }
    size_t mid_start = line_start(i);
    size_t patch_start =
        diff_at < lines.size() ? line_start(diff_at) : data.size();
    pf.middle = data.substr(mid_start, patch_start - mid_start);
    pf.patch = data.substr(patch_start);

    pf.archive = pf.header_value("Archive");
    pf.origname = pf.header_value("Origname");
    pf.name = pf.header_value("Name");

    // Collect every URL: header from the head (kept verbatim there; the
    // parsed copies drive URL-based setups). Each must be exactly
    // "URL: <url> <blake3-hash>".
    for (const std::string& line : split_lines(pf.head)) {
        if (line.size() < 4 || line.compare(0, 4, "URL:") != 0)
            continue; // not a URL: header line
        ProjenyUrl u;
        if (!parse_url_value(line.substr(4), &u)) {
            die("file " + what_for_errors +
                " has a malformed URL: header (want 'URL: <url> "
                "<blake3-hash>'): '" + line + "'");
        }
        pf.urls.push_back(u);
    }
    if (!pf.urls.empty() && !pf.archive.empty()) {
        die("file " + what_for_errors +
            " has both Archive: and URL: headers; they are mutually "
            "exclusive (either check the tarball into git next to the "
            ".projeny file and name it with Archive:, or fetch it with one "
            "or more URL: lines)");
    }
    if (!pf.urls.empty()) {
        // URL-based: the archive name comes from the first URL's basename,
        // and names the snapshot the download is cached as. Keep the
        // no-'/' invariant the classic Archive: form enforces (a derived
        // basename can never contain one).
        pf.archive = archive_name_from_url(pf.urls[0].url);
        if (pf.archive.empty()) {
            die("file " + what_for_errors + ": cannot derive the archive "
                "name from the URL '" + pf.urls[0].url +
                "' (the URL must name the archive file itself, so the "
                ".<archive>.snapshot download can be named after it)");
        }
    }
    if (pf.archive.empty() || pf.origname.empty() || pf.name.empty()) {
        die("file " + what_for_errors +
            " is missing a required header (need Archive: or at least one "
            "URL: line, plus Origname: and Name:)");
    }
    if (pf.archive.find('/') != std::string::npos)
        die("file " + what_for_errors + ": Archive: must be a plain filename");
    if (pf.origname.find('/') != std::string::npos ||
        pf.origname == "." || pf.origname == "..")
        die("file " + what_for_errors + ": bad Origname: header");
    if (pf.name.find('/') != std::string::npos || pf.name == "." || pf.name == "..")
        die("file " + what_for_errors + ": bad Name: header");
    return pf;
}

void ProjenyFile::rebuild(const std::string& new_patch)
{
    // Normalize prose first: every non-empty middle line is made to start
    // with a space/tab, so tool-written files never contain a column-0
    // line and column-0 marker-shaped lines stay unambiguously git
    // conflicts (see normalize_prose_indent).
    middle = normalize_prose_indent(middle);
    // Enforce the invariant on write: prose must never contain a column-0
    // marker line. (Unreachable after the normalization above, but kept as
    // a guard so no future writer can silently reintroduce the ambiguity:
    // such prose would false-positive as a git conflict on the next read.)
    for (const std::string& l : split_lines(middle)) {
        if (is_projeny_marker_line(l))
            die("cannot write .projeny file: free-text prose contains a "
                "column-0 marker line ('" + l + "'); indent the prose line "
                "with a leading space");
    }
    // raw = head + "\n" + middle + patch. `middle` is a verbatim byte slice
    // of the file (see parse_bytes), so it ends in '\n' only when the file's
    // prose did — and a .projeny file without a trailing newline is legal.
    // That makes the concatenation a trap: appending a patch to prose that
    // lacks its final newline GLUES the first "diff --git" line onto the
    // last prose line ("    mg 4.1 unmodifieddiff --git a/..."), and the
    // next parse then finds no "diff --git " line at all — silently turning
    // the committed patch into prose. So: a non-empty new_patch always
    // starts on its own line (exactly one '\n' is added when the middle
    // lacks one), while an empty new_patch preserves the middle
    // byte-exactly (a patch-less .projeny file must round-trip without
    // gaining a newline, so commit/rebase no-ops stay byte-identical).
    // The patch body itself is replaced verbatim, so trailing whitespace
    // inside the new patch is preserved byte-for-byte. Trailing-newline
    // shape: a non-empty patch always ends with '\n' (one is added when
    // missing), and a patch whose content ends with a blank line ends with
    // '\n\n' — that blank line is part of the patch and is preserved (e.g.
    // the blank line after a binary block). All rebuild callers pass
    // normalize_patch_text output, so stored patches already observe this.
    std::string out = head;
    out += "\n";
    out += middle;
    std::string np = new_patch;
    if (!np.empty()) {
        if (!middle.empty() && middle.back() != '\n')
            out += "\n";
        if (np.back() != '\n')
            np += "\n";
    }
    out += np;
    raw = out;
    patch = np;
}

// ---- status file ----
//
// Text format, chosen for debuggability:
//
//   Status: setup
//   Conflict: <workdir-relative path>     (repeatable, optional)
//   Added: <path>                         (repeatable, optional)
//   Removed: <path>                       (repeatable, optional)
//   Renamed: <src> -> <dst>               (repeatable, optional)
//   --- projeny content ---
//   <verbatim .projeny bytes to EOF>
//
// Paths are backslash-escaped (escape_status_path: "\" -> "\\",
// newline -> "\n", CR -> "\r", ">" -> "\>") so filenames containing
// newlines roundtrip (escaped values never contain a raw newline, so one
// path per line holds) and the "Renamed: <src> -> <dst>" separator stays
// unambiguous: ">" is always escaped inside values, so a raw " -> " in the
// line can only be the real field separator (split on the first one, then
// unescape each side). Only the single delimiter space after "Key:" is
// stripped; leading/trailing spaces inside filenames are preserved.
//
// The "Status:" line says whether the workdir was set up. The embedded copy
// is the .projeny file exactly as it was at the last setup/commit, so setup
// can reconstruct the expected tree even after the user edits .projeny.
// Conflict lines list files with conflict markers. Added/Removed/Renamed
// lines are pending operations not yet folded into the patch by commit.

StatusData StatusData::parse(const std::string& path)
{
    return parse_bytes(read_file_bytes(path), "'" + path + "'");
}

StatusData StatusData::parse_bytes(const std::string& data,
                                   const std::string& what_for_errors)
{
    StatusData sd;
    std::vector<std::string> lines = split_lines(data);
    size_t i = 0;
    bool saw_status = false;
    for (; i < lines.size(); ++i) {
        const std::string& l = lines[i];
        if (l == kStatusDelim) {
            ++i;
            break;
        }
        if (l.empty())
            continue;
        if (starts_with(l, "Status:")) {
            sd.status = trim(l.substr(7));
            saw_status = true;
        } else if (starts_with(l, "Conflict:")) {
            sd.conflicts.push_back(unescape_status_path(strip_one_space(l.substr(9))));
        } else if (starts_with(l, "Added:")) {
            sd.added.push_back(unescape_status_path(strip_one_space(l.substr(6))));
        } else if (starts_with(l, "Removed:")) {
            sd.removed.push_back(unescape_status_path(strip_one_space(l.substr(8))));
        } else if (starts_with(l, "Renamed:")) {
            std::string rest = strip_one_space(l.substr(8));
            size_t arrow = rest.find(" -> ");
            if (arrow == std::string::npos)
                die("file " + what_for_errors + " has a malformed Renamed: line");
            // '>' is escaped inside values, so the first raw " -> " is
            // always the real separator, even when a filename contains it.
            sd.renamed.push_back({unescape_status_path(rest.substr(0, arrow)),
                                  unescape_status_path(rest.substr(arrow + 4))});
        } else {
            die("file " + what_for_errors + " has an unrecognized status line: '" +
                l + "'");
        }
    }
    if (!saw_status)
        die("file " + what_for_errors + " is missing the Status: line");
    // The embedded .projeny copy is byte-exact: the raw bytes after the
    // delimiter line through EOF belong to the copy verbatim (no re-join,
    // so interior CR bytes from CRLF patch content and the trailing-newline
    // state both survive exactly).
    size_t emb_off = data.size();
    {
        size_t pos = 0;
        for (size_t k = 0; k < i; ++k) {
            size_t nl = data.find('\n', pos);
            if (nl == std::string::npos) {
                pos = data.size();
                break;
            }
            pos = nl + 1;
        }
        emb_off = pos;
    }
    sd.embedded = data.substr(emb_off);
    return sd;
}

std::string StatusData::serialize() const
{
    std::string out = "Status: " + status + "\n";
    for (const auto& c : conflicts)
        out += "Conflict: " + escape_status_path(c) + "\n";
    for (const auto& a : added)
        out += "Added: " + escape_status_path(a) + "\n";
    for (const auto& r : removed)
        out += "Removed: " + escape_status_path(r) + "\n";
    for (const auto& rn : renamed)
        out += "Renamed: " + escape_status_path(rn.first) + " -> " +
               escape_status_path(rn.second) + "\n";
    out += std::string(kStatusDelim) + "\n";
    // The embedded .projeny copy is byte-exact: stored verbatim (even
    // without a trailing newline) and compared as raw bytes on commit.
    // NOTE: no terminating newline is added when the copy lacks one, so the
    // status file itself simply doesn't end with '\n' in that case — which is
    // exactly how parse_bytes() tells "copy ends with '\n'" apart from "copy
    // has no trailing newline" on the way back in. Adding a terminator here
    // would make the two cases indistinguishable and break commit's raw-bytes
    // comparison for newline-less .projeny files.
    out += embedded;
    return out;
}
