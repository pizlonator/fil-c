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
#include "tree.h"

#include "util.h"
#include "vcs.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

void require_text_patch(const std::string& patch, const std::string& what)
{
    if (contains_nul(patch))
        die(what + " contains NUL bytes; binary patches are not supported");
}

std::string normalize_patch_text(const std::string& patch)
{
    if (patch.empty())
        return "";
    require_text_patch(patch, "patch");
    // Raw split on '\n' only: interior '\r' bytes (CRLF file content inside
    // hunk bodies) are preserved byte-for-byte. Only the blank-context-line
    // form is normalized (" " -> ""); every other line keeps its bytes
    // exactly (trailing spaces/tabs/\r are significant file content).
    std::vector<std::string> lines;
    size_t i = 0;
    while (i < patch.size()) {
        size_t j = patch.find('\n', i);
        if (j == std::string::npos) {
            lines.push_back(patch.substr(i));
            break;
        }
        lines.push_back(patch.substr(i, j - i));
        i = j + 1;
    }
    for (auto& l : lines) {
        if (l == " ")
            l = "";
    }
    if (lines.empty())
        return "";
    return join_lines(lines);
}

namespace {
// "diff --git <a> <b>" -> (a, b), honoring git's C-style quoting.
//
// Quoting reality: git quotes a path only when it must (tabs, quotes,
// backslashes, newlines, other controls, non-ASCII bytes); spaces and "->"
// are left UNQUOTED. So the unquoted form is ambiguous in general. Handled:
//   - quoted pairs (each side is quoted independently based on its own need,
//     so mixed quoted/unquoted pairs occur);
//   - unquoted pairs: every " <b-side>" boundary is a candidate split; the
//     split whose a/b-stripped remainders are equal wins (non-renames, the
//     common case); a single candidate is used as-is (covers renames and
//     fresh diffs whose paths hold no spaces). Anything still ambiguous
//     returns false; the caller then derives the paths from the block's
//     ---/+++ (or rename from/to) lines, which carry one path each and parse
//     unambiguously.
bool parse_diff_git_line(const std::string& line, std::string* a, std::string* b)
{
    const char* pfx = "diff --git ";
    if (!starts_with(line, pfx))
        return false;
    std::string rest = line.substr(strlen(pfx));
    // NOTE: no trailing-whitespace trimming here: filenames may END in
    // spaces, and git emits those raw on this line, so trimming would eat
    // real filename content (only CRLF handling in split_lines applies).
    if (rest.empty())
        return false;
    // End index (inclusive) of the quoted token starting at s[i] == '"',
    // or npos when unterminated.
    auto quoted_end = [](const std::string& s, size_t i) -> size_t {
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
    // A quoted b-side candidate must be exactly one token (closing quote at
    // end of string).
    auto quoted_single = [&](const std::string& t) -> bool {
        if (t.empty() || t[0] != '"')
            return true; // unquoted: whole remainder is the path
        size_t q = quoted_end(t, 0);
        return q != std::string::npos && q + 1 == t.size();
    };
    auto strip_sides = [](const std::string& p) -> std::string {
        std::string u = unquote_git_path(p);
        if (starts_with(u, "a/"))
            return u.substr(2);
        if (starts_with(u, "b/"))
            return u.substr(2);
        return u;
    };
    if (rest[0] == '"') {
        size_t q = quoted_end(rest, 0);
        if (q == std::string::npos || q + 1 >= rest.size() || rest[q + 1] != ' ')
            return false;
        std::string ta = rest.substr(0, q + 1);
        std::string tb = rest.substr(q + 2);
        if (tb.empty() || !quoted_single(tb))
            return false;
        *a = unquote_git_path(ta);
        *b = unquote_git_path(tb);
        return true;
    }
    // Unquoted a-side: gather candidate splits. The b-side starts at a space
    // followed by "b/", "a/" (defensive) or a quoted path.
    struct Cand {
        std::string l, r;
    };
    std::vector<Cand> cands;
    for (size_t i = 0; i < rest.size(); ++i) {
        if (rest[i] != ' ' || i + 1 >= rest.size())
            continue;
        char c = rest[i + 1];
        bool bstart = c == 'b' && rest.compare(i + 1, 2, "b/") == 0;
        bool astart = c == 'a' && rest.compare(i + 1, 2, "a/") == 0;
        if (bstart || astart || c == '"')
            cands.push_back({rest.substr(0, i), rest.substr(i + 1)});
    }
    if (cands.empty())
        return false;
    if (cands.size() == 1) {
        if (cands[0].l.empty() || cands[0].r.empty() ||
            !quoted_single(cands[0].r))
            return false;
        *a = unquote_git_path(cands[0].l);
        *b = unquote_git_path(cands[0].r);
        return true;
    }
    // Ambiguous: take the split whose stripped remainders agree (non-rename).
    for (const auto& c : cands) {
        if (c.l.empty() || c.r.empty() || !quoted_single(c.r))
            continue;
        std::string rl = strip_sides(c.l);
        std::string rr = strip_sides(c.r);
        if (!rl.empty() && rl == rr) {
            *a = unquote_git_path(c.l);
            *b = unquote_git_path(c.r);
            return true;
        }
    }
    return false;
}

}

std::vector<FileDiff> split_file_diffs(const std::string& patch)
{
    std::vector<FileDiff> out;
    if (patch.empty())
        return out;
    require_text_patch(patch, "patch");
    std::vector<std::string> lines = split_lines(patch);
    std::vector<size_t> starts;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (starts_with(lines[i], "diff --git ") || starts_with(lines[i], "diff --cc ") ||
            starts_with(lines[i], "diff --combined ")) {
            // "diff --cc"/"diff --combined" are merge-diff headers git emits
            // for conflicts; treat like a file boundary.
            starts.push_back(i);
        }
    }
    for (size_t s = 0; s < starts.size(); ++s) {
        size_t e = (s + 1 < starts.size()) ? starts[s + 1] : lines.size();
        std::vector<std::string> blk(lines.begin() + starts[s], lines.begin() + e);
        FileDiff fd;
        fd.text = join_lines(blk);
        std::string la, lb;
        if (parse_diff_git_line(lines[starts[s]], &la, &lb)) {
            // Strip the a/ b/ prefixes to get repo paths.
            auto strip = [](const std::string& p) -> std::string {
                if (starts_with(p, "a/"))
                    return p.substr(2);
                if (starts_with(p, "b/"))
                    return p.substr(2);
                return p;
            };
            std::string ra = strip(la), rb = strip(lb);
            if (ra == "/dev/null" || la == "/dev/null")
                ra = "";
            if (rb == "/dev/null" || lb == "/dev/null")
                rb = "";
            fd.a_path = ra;
            fd.b_path = rb;
        }
        out.push_back(fd);
    }
    return out;
}

namespace {

// True for tar metadata entries that carry no payload and must not count as
// top-level content: GNU tar's verbatim pax global header, the "@PaxHeader"
// extended-attribute directories POSIX/pax writers emit, SCHILY.* extended
// attributes, and the "." / "./" self-dir spellings.
bool is_archive_metadata_entry(const std::string& name)
{
    if (name == "pax_global_header" || name == "./pax_global_header")
        return true;
    if (name == "@PaxHeader" || name == "./@PaxHeader")
        return true;
    if (starts_with(name, "@PaxHeader/") || starts_with(name, "./@PaxHeader/"))
        return true;
    if (name == "SCHILY" || name == "./SCHILY")
        return true;
    if (starts_with(name, "SCHILY/") || starts_with(name, "./SCHILY/"))
        return true;
    // Strip all leading "./" components, then look for SCHILY.* as the first
    // real component (e.g. "./SCHILY.foobar" from ustar writers).
    std::string e = name;
    while (starts_with(e, "./"))
        e = e.substr(2);
    if (starts_with(e, "SCHILY."))
        return true;
    if (e.empty() || e == ".")
        return true;
    return false;
}

// Check a tar listing entry's symlink/hardlink target: it must be relative
// and must lexically resolve to a path inside the tree. A symlink's target
// is relative to the member's directory; a hardlink's target is the linked
// member's name, which tar resolves against the extraction root (it strips
// leading ".." before linking). In-tree ".." spellings (e.g.
// "b3sum/LICENSE_A2 -> ../LICENSE_A2") resolve back inside the tree and are
// fine; only absolute targets and true escapes are refused.
void check_tar_link_target(const std::string& member, const std::string& target,
                           bool hardlink)
{
    if (target.empty())
        return;
    if (target[0] == '/')
        die("archive member '" + member + "' links to absolute target '" +
            target + "'; refusing (symlink/hardlink escape)");
    // Strip the "./" spellings tar emits so the member's directory is
    // relative to the tree root.
    std::string m = member;
    while (starts_with(m, "./"))
        m = m.substr(2);
    std::string resolved;
    if (resolve_link_target(hardlink ? std::string(".") : dirname_of(m),
                            target, &resolved) == LinkResolve::Escapes)
        die("archive member '" + member + "' links to '" + target +
            "' (resolves to '" + resolved +
            "'); refusing (link target escapes the tree)");
}

// `tar -tv` long-listing line -> (member name, link target or "", hardlink).
// Symlink ("l") entries print as "name -> target", hardlink ("h") entries as
// "name link to target". Anything else has no link target to check.
bool parse_tar_verbose_line(const std::string& line, std::string* member,
                            std::string* target, bool* hardlink)
{
    if (line.empty())
        return false;
    // Long listing starts with a mode field like "-rw-r--r--" / "lrwxrwxrwx"
    // / "drwxr-xr-x" / "hrw-r--r--".
    char kind = line[0];
    const char* sep = nullptr;
    if (kind == 'l') {
        sep = " -> ";
        *hardlink = false;
    } else if (kind == 'h') {
        sep = " link to ";
        *hardlink = true;
    } else {
        return false; // regular file/dir/etc: no link target to check
    }
    size_t arrow = line.rfind(sep);
    if (arrow == std::string::npos)
        return false;
    std::string tail = line.substr(arrow + 4);
    // The member name is the last whitespace-separated field before " -> ".
    std::string head = line.substr(0, arrow);
    size_t sp = head.find_last_of(" \t");
    if (sp == std::string::npos)
        return false;
    *member = head.substr(sp + 1);
    *target = tail;
    // Trim trailing whitespace/CR from the target.
    while (!target->empty() &&
           (target->back() == ' ' || target->back() == '\t' ||
            target->back() == '\r'))
        target->pop_back();
    return true;
}

// Parse the member name out of any `tar -tv` line: the last
// whitespace-separated field, minus any " -> target" / " link to target"
// suffix. Returns "" when the line is not a listing entry.
std::string parse_tar_member_name(const std::string& line)
{
    if (line.empty())
        return "";
    // Listing lines start with the mode field (e.g. "-rw-r--r--",
    // "lrwxrwxrwx", "drwxr-xr-x", "hrw-r--r--").
    char kind = line[0];
    if (kind != 'l' && kind != 'h' && kind != '-' && kind != 'd' &&
        kind != 'c' && kind != 'b' && kind != 'p')
        return "";
    std::string head = line;
    size_t arrow = head.rfind(" -> ");
    size_t linkto = head.rfind(" link to ");
    size_t cut = std::string::npos;
    if (arrow != std::string::npos)
        cut = arrow;
    if (linkto != std::string::npos &&
        (cut == std::string::npos || linkto < cut))
        cut = linkto;
    if (cut != std::string::npos)
        head = head.substr(0, cut);
    size_t sp = head.find_last_of(" \t");
    if (sp == std::string::npos)
        return "";
    return head.substr(sp + 1);
}

// Reject members whose own path is absolute or escapes the top dir, and
// symlink/hardlink members whose target escapes. Member-path checks run on
// every listing line (the plain `tar -tf` listing below, which is also the
// source of truth for .. spellings); link-target checks run on the verbose
// listing where targets are visible.
void check_archive_members_safe(const std::string& archive)
{
    {
        CmdResult r = run_cmd({"tar", "-tf", absolutize(archive)});
        if (r.code != 0)
            die("cannot list archive '" + archive + "'", r.output);
        for (const std::string& line : split_lines(r.output)) {
            if (line.empty())
                continue;
            // tar prints diagnostics (e.g. "tar: Removing leading ...")
            // into the merged output; those are not member names.
            if (starts_with(line, "tar: "))
                continue;
            std::string member = line;
            if (!member.empty() && member[0] == '/')
                die("archive member '" + member + "' is absolute; refusing");
            size_t i = 0;
            bool escapes = false;
            // Strip leading "./" repetitions, then check each component.
            std::string m = member;
            while (starts_with(m, "./"))
                m = m.substr(2);
            while (i <= m.size()) {
                size_t j = m.find('/', i);
                std::string comp = (j == std::string::npos)
                                       ? m.substr(i)
                                       : m.substr(i, j - i);
                if (comp == "..")
                    escapes = true;
                if (j == std::string::npos)
                    break;
                i = j + 1;
            }
            if (escapes)
                die("archive member '" + member + "' contains '..'; refusing");
        }
    }
    CmdResult r = run_cmd({"tar", "-tvf", absolutize(archive)});
    if (r.code != 0)
        die("cannot list archive '" + archive + "'", r.output);
    for (const std::string& line : split_lines(r.output)) {
        if (line.empty())
            continue;
        // Character/block device nodes must never be unpacked (we run as
        // whatever user invoked us, possibly root): refuse up front.
        if (line[0] == 'c' || line[0] == 'b') {
            std::string member = parse_tar_member_name(line);
            if (member.empty())
                member = line;
            die("archive member '" + member +
                "' is a device node; refusing");
        }
        std::string member = parse_tar_member_name(line);
        if (member.empty())
            continue;
        std::string link_member, target;
        bool hardlink = false;
        if (parse_tar_verbose_line(line, &link_member, &target, &hardlink))
            check_tar_link_target(link_member.empty() ? member : link_member,
                                  target, hardlink);
    }
}

} // namespace

namespace {
void check_tool(const std::string& tool)
{
    CmdResult r = run_cmd({tool, "--version"});
    if (r.code != 0 && r.code != 1) // some tools exit 1 on --version; fine
        die("required helper '" + tool + "' is not available", r.output);
}
}

void unpack_single_top(const std::string& archive, const std::string& destdir,
                       const std::string& expect_top)
{
    check_tool("tar");
    if (!path_exists(archive))
        die("archive '" + archive + "' does not exist");
    // Reject symlink/hardlink escapes and absolute/".." members before
    // unpacking, then extract without letting the archive's ownership or
    // permission bits leak onto the workdir.
    check_archive_members_safe(archive);
    make_dirs(destdir);
    CmdResult r = run_cmd({"tar", "-xf", absolutize(archive), "-C",
                           absolutize(destdir), "--no-same-owner",
                           "--no-same-permissions"});
    if (r.code != 0)
        die("failed to unpack archive '" + archive + "'", r.output);
    std::vector<std::string> tops;
    for (const std::string& t : list_dir_names(destdir)) {
        if (is_archive_metadata_entry(t)) {
            // Some writers (e.g. pax/SCHILY entries stored as regular
            // files) materialize metadata as on-disk files; drop them so
            // the exactly-one-top-dir check below sees only real content.
            if (!remove_recursive(join_path(destdir, t)))
                die("cannot remove archive metadata entry '" + t + "'");
            continue;
        }
        tops.push_back(t);
    }
    if (tops.size() != 1 || tops[0] != expect_top) {
        std::string detail = "archive produced " + std::to_string(tops.size()) +
                             " top-level entries:";
        for (auto& t : tops)
            detail += " '" + t + "'";
        detail += "\n";
        die("archive '" + archive + "' must unpack to exactly one top-level "
            "directory named '" +
                expect_top + "'",
            detail);
    }
}

std::string archive_single_top_name(const std::string& archive)
{
    check_tool("tar");
    if (!path_exists(archive))
        die("archive '" + archive + "' does not exist");
    CmdResult r = run_cmd({"tar", "-tf", absolutize(archive)});
    if (r.code != 0)
        die("cannot list archive '" + archive + "'", r.output);
    std::string top;
    for (const std::string& line : split_lines(r.output)) {
        if (line.empty())
            continue;
        if (starts_with(line, "tar: "))
            continue; // tar diagnostic, not a member name
        if (is_archive_metadata_entry(line))
            continue;
        std::string e = line;
        if (starts_with(e, "./"))
            e = e.substr(2);
        if (e.empty() || e == ".")
            continue;
        size_t slash = e.find('/');
        std::string first = slash == std::string::npos ? e : e.substr(0, slash);
        if (is_archive_metadata_entry(first))
            continue;
        if (top.empty())
            top = first;
        else if (first != top)
            die("archive '" + archive +
                "' has multiple top-level entries (e.g. '" + top + "' and '" +
                first + "'); refusing");
    }
    if (top.empty())
        die("archive '" + archive + "' appears to be empty");
    return top;
}

std::string diff_trees(const std::string& base_tree, const std::string& workdir,
                       const std::string& wid)
{
    // Internal unified diff with git-compatible output (diff --git labels,
    // ---/+++, @@ hunks, new/deleted/rename entries). No git invocation.
    return vcs_diff_trees(base_tree, workdir, wid);
}

bool apply_patch_whole(const std::string& treedir, const std::string& patch,
                       const std::string& wid, const std::string& scratch_parent)
{
    (void)scratch_parent; // no temp files needed; kept for call compatibility
    if (normalize_patch_text(patch).empty())
        return true;
    return vcs_apply_whole(treedir, patch, wid);
}

std::vector<VcsFailure> apply_patch_per_file(const std::string& workdir,
                                              const std::string& patch,
                                              const std::string& wid)
{
    // Internal per-file application with -p1 semantics. Blocks already
    // applied are detected (reverse-match) and skipped; they are not
    // reported as failures. Single-parser source of truth: failures carry
    // workdir-relative paths, never indices into another parser's blocks.
    return vcs_apply_per_file(workdir, patch, wid);
}

bool apply_patch_with_conflicts(const std::string& treedir,
                                const std::string& patch, const std::string& wid,
                                const std::string& scratch_parent,
                                std::vector<std::string>* conflicts)
{
    (void)scratch_parent; // no temp files needed; kept for call compatibility
    if (normalize_patch_text(patch).empty())
        return true;
    return vcs_apply_with_conflicts(treedir, patch, wid, conflicts);
}

std::vector<std::string> patch_touched_paths(const std::string& patch,
                                             const std::string& wid)
{
    return vcs_touched_paths(patch, wid);
}
bool merge_one_file(const std::string& base_file, const std::string& ours_file,
                    const std::string& theirs_file, const std::string& dst_path,
                    const std::string& dst_root)
{
    // Internal three-way merge (no git merge-file invocation).
    return vcs_merge_one_file(base_file, ours_file, theirs_file, dst_path,
                              dst_root);
}
