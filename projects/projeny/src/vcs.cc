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
// Internal git-compatible unified diff, patch application (with fuzz), and
// three-way merge. No git binary is invoked anywhere.
//
// Line model: files are split on '\n' only (a trailing '\n' does not produce
// a final empty element; interior '\r' bytes from CRLF files are preserved
// as content). `ends_nl` records whether the file ends with '\n' (empty
// files report true; it is irrelevant for them).
#include "vcs.h"

#include "util.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The frozen-mtime extended header prefix (`frozen-mtime <ts>`, emitted
// right after the `diff --git` line) and its length. File-scope static: the
// parse, emit, and edit paths all share it (a magic 13 invites typos).
static const char kFrozenMtimeHeader[] = "frozen-mtime ";
static const size_t kFrozenMtimeHeaderLen = sizeof(kFrozenMtimeHeader) - 1;

namespace {

// Split on '\n' without touching '\r'. A trailing '\n' does not produce a
// final empty element.
std::vector<std::string> split_raw(const std::string& s)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find('\n', i);
        if (j == std::string::npos) {
            out.push_back(s.substr(i));
            break;
        }
        out.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return out;
}

struct FileLines {
    std::vector<std::string> lines;
    bool ends_nl = true;
};

FileLines split_content(const std::string& data)
{
    FileLines fl;
    if (data.empty())
        return fl; // empty file: no lines, ends_nl irrelevant (true)
    fl.ends_nl = data.back() == '\n';
    fl.lines = split_raw(data);
    // NOTE: no pop_back here. split_raw already drops the artifact of the
    // final terminator (a trailing '\n' yields no extra element), so a
    // trailing "" element is a real blank line and must be kept: dropping
    // it loses blank lines at end of file on every commit roundtrip.
    return fl;
}

std::string join_content(const std::vector<std::string>& lines, bool ends_nl)
{
    std::string out;
    for (const auto& l : lines) {
        out += l;
        out += '\n';
    }
    if (!ends_nl && !lines.empty())
        out.pop_back(); // drop the final terminator
    return out;
}

struct Collected {
    bool is_symlink = false;
    bool is_exec = false; // regular files only
    bool is_binary = false; // regular files only: content holds NUL bytes.
    std::string content;  // regular: raw bytes; symlink: link target
};

void collect_into(const std::string& root, const std::string& rel,
                  std::map<std::string, Collected>& out, const std::string& what)
{
    std::string full = rel.empty() ? root : join_path(root, rel);
    struct stat st;
    if (lstat(full.c_str(), &st) != 0)
        die("cannot stat '" + full + "': " + strerror(errno));
    if (S_ISDIR(st.st_mode)) {
        for (const std::string& name : list_dir_names(full)) {
            std::string child = rel.empty() ? name : rel + "/" + name;
            collect_into(root, child, out, what);
        }
        return;
    }
    if (S_ISLNK(st.st_mode)) {
        Collected c;
        c.is_symlink = true;
        c.content = read_link_target(full);
        if (c.content.find('\0') != std::string::npos)
            die(what + ": '" + rel + "' is a binary file; binary files are not supported");
        // Fail fast on targets that do not stay inside the tree (absolute,
        // or resolving above the root), exactly like tar unpack and patch
        // application: refusing here keeps commit from storing a patch that
        // setup would later have to refuse. In-tree ".." spellings (e.g.
        // "sub/link -> ../file") resolve back inside and are fine.
        {
            std::string resolved;
            LinkResolve lr =
                resolve_link_target(dirname_of(rel), c.content, &resolved);
            if (lr == LinkResolve::Absolute)
                die(what + ": '" + rel + "' links to absolute target '" +
                    c.content + "'; refusing (symlink escape)");
            if (lr == LinkResolve::Escapes)
                die(what + ": '" + rel + "' links to '" + c.content +
                    "' (resolves to '" + resolved +
                    "'); refusing (link target escapes the tree)");
        }
        out[rel] = c;
        return;
    }
    if (S_ISREG(st.st_mode)) {
        std::string data = read_file_bytes(full);
        Collected c;
        c.is_symlink = false;
        c.is_exec = (st.st_mode & 0111) != 0;
        c.is_binary = data.find(char(0)) != std::string::npos;
        c.content = data;
        out[rel] = c;
        return;
    }
    die(what + ": '" + rel + "' has unsupported file type; only regular files, "
        "symlinks and directories are supported");
}

std::string mode_of(const Collected& c)
{
    if (c.is_symlink)
        return "120000";
    return c.is_exec ? "100755" : "100644";
}

// ---- Myers line diff ----

struct Op {
    char kind; // ' ', '-', '+'
    std::string line;
};

// Myers O(ND) greedy diff with prefix/suffix trim and fallbacks for huge or
// highly dissimilar inputs (emits wholesale replacement then).
std::vector<Op> myers_lines(const std::vector<std::string>& a,
                            const std::vector<std::string>& b)
{
    size_t pre = 0;
    while (pre < a.size() && pre < b.size() && a[pre] == b[pre])
        ++pre;
    size_t suf = 0;
    while (suf < a.size() - pre && suf < b.size() - pre &&
           a[a.size() - 1 - suf] == b[b.size() - 1 - suf])
        ++suf;
    std::vector<std::string> am(a.begin() + pre, a.end() - suf);
    std::vector<std::string> bm(b.begin() + pre, b.end() - suf);

    std::vector<Op> out;
    for (size_t i = 0; i < pre; ++i)
        out.push_back({' ', a[i]});

    bool done_middle = false;
    // Fast paths.
    if (am.empty() && bm.empty()) {
        done_middle = true;
    } else if (am.empty() || bm.empty()) {
        // Pure insertion/deletion: no need for Myers.
        for (auto& l : am)
            out.push_back({'-', l});
        for (auto& l : bm)
            out.push_back({'+', l});
        done_middle = true;
    } else {
        size_t n = am.size(), m = bm.size();
        bool wholesale = false;
        if (n + m > 10000 || n * m > 25000000)
            wholesale = true;
        if (!wholesale && n + m > 2000) {
            // Cheap similarity probe: fraction of am lines found in bm.
            std::unordered_multiset<std::string> bset(bm.begin(), bm.end());
            size_t common = 0;
            for (auto& l : am) {
                auto it = bset.find(l);
                if (it != bset.end()) {
                    ++common;
                    bset.erase(it);
                }
            }
            double ratio = (double)common / (double)(n > m ? n : m);
            if (ratio < 0.3)
                wholesale = true;
        }
        if (wholesale) {
            for (auto& l : am)
                out.push_back({'-', l});
            for (auto& l : bm)
                out.push_back({'+', l});
            done_middle = true;
        }
    }
    if (!done_middle) {
        size_t n = am.size(), m = bm.size();
        size_t maxd = n + m;
        // D-limit guard inside the loop aborts to wholesale past 5000
        // (bounds trace memory); small edits in big files stay exact.
        const size_t kDLimit = 5000;
        {
            int off = (int)maxd;
            std::vector<int> v(2 * maxd + 1, -1);
            v[off + 1] = 0;
            std::vector<std::vector<int>> trace;
            trace.reserve(64);
            int found_d = -1;
            for (size_t d = 0; d <= maxd; ++d) {
                if (d > kDLimit) {
                    found_d = -2; // give up
                    break;
                }
                for (int k = -(int)d; k <= (int)d; k += 2) {
                    int idx = off + k;
                    int x;
                    if (k == -(int)d || (k != (int)d && v[idx - 1] < v[idx + 1]))
                        x = v[idx + 1]; // down (insertion)
                    else
                        x = v[idx - 1] + 1; // right (deletion)
                    int y = x - k;
                    while ((size_t)x < n && (size_t)y < m && am[(size_t)x] == bm[(size_t)y]) {
                        ++x;
                        ++y;
                    }
                    v[idx] = x;
                    if ((size_t)x >= n && (size_t)y >= m) {
                        found_d = (int)d;
                        break;
                    }
                }
                trace.push_back(v);
                if (found_d >= 0)
                    break;
            }
            if (found_d == -2) {
                for (auto& l : am)
                    out.push_back({'-', l});
                for (auto& l : bm)
                    out.push_back({'+', l});
            } else {
                // Backtrack.
                std::vector<Op> rev;
                int x = (int)n, y = (int)m;
                for (int d = found_d; d > 0; --d) {
                    const std::vector<int>& vprev = trace[(size_t)d - 1];
                    int k = x - y;
                    int idx = off + k;
                    bool down = (k == -(int)d ||
                                 (k != (int)d && vprev[idx - 1] < vprev[idx + 1]));
                    int kprev = down ? k + 1 : k - 1;
                    int xprev = vprev[off + kprev];
                    int yprev = xprev - kprev;
                    while (x > xprev && y > yprev) {
                        --x;
                        --y;
                        rev.push_back({' ', am[(size_t)x]});
                    }
                    if (down) {
                        --y;
                        rev.push_back({'+', bm[(size_t)y]});
                    } else {
                        --x;
                        rev.push_back({'-', am[(size_t)x]});
                    }
                    x = xprev;
                    y = yprev;
                }
                while (x > 0 && y > 0) {
                    --x;
                    --y;
                    rev.push_back({' ', am[(size_t)x]});
                }
                while (x > 0) {
                    --x;
                    rev.push_back({'-', am[(size_t)x]});
                }
                while (y > 0) {
                    --y;
                    rev.push_back({'+', bm[(size_t)y]});
                }
                std::reverse(rev.begin(), rev.end());
                for (auto& o : rev)
                    out.push_back(o);
            }
        }
    }
    for (size_t i = 0; i < suf; ++i)
        out.push_back({' ', a[a.size() - suf + i]});
    return out;
}

struct Hunk {
    long old_start = 1;
    long old_count = 0;
    long new_start = 1;
    long new_count = 0;
    std::vector<Op> lines;
};

std::vector<Hunk> build_hunks(const std::vector<Op>& script, int context = 3)
{
    std::vector<Hunk> hunks;
    size_t n = script.size();
    // Change blocks: maximal runs containing '-' or '+'.
    struct Block {
        size_t s, e; // [s,e) in script
    };
    std::vector<Block> blocks;
    size_t i = 0;
    while (i < n) {
        if (script[i].kind == ' ') {
            ++i;
            continue;
        }
        size_t s = i;
        while (i < n && script[i].kind != ' ')
            ++i;
        blocks.push_back({s, i});
    }
    if (blocks.empty())
        return hunks;
    // Expand with context and merge overlaps (gap <= 2*context).
    struct Range {
        size_t s, e;
    };
    std::vector<Range> ranges;
    for (auto& b : blocks) {
        size_t s = b.s > (size_t)context ? b.s - (size_t)context : 0;
        size_t e = b.e + (size_t)context < n ? b.e + (size_t)context : n;
        if (!ranges.empty() && s <= ranges.back().e)
            ranges.back().e = e > ranges.back().e ? e : ranges.back().e;
        else
            ranges.push_back({s, e});
    }
    // Line numbers.
    for (auto& r : ranges) {
        long oc = 0, nc = 0;
        long ob = 0, nb = 0;
        for (size_t k = 0; k < r.s; ++k) {
            if (script[k].kind == ' ' || script[k].kind == '-')
                ++ob;
            if (script[k].kind == ' ' || script[k].kind == '+')
                ++nb;
        }
        for (size_t k = r.s; k < r.e; ++k) {
            if (script[k].kind == ' ' || script[k].kind == '-')
                ++oc;
            if (script[k].kind == ' ' || script[k].kind == '+')
                ++nc;
        }
        Hunk h;
        h.old_count = oc;
        h.new_count = nc;
        h.old_start = (oc == 0) ? ob : ob + 1;
        h.new_start = (nc == 0) ? nb : nb + 1;
        for (size_t k = r.s; k < r.e; ++k)
            h.lines.push_back(script[k]);
        hunks.push_back(h);
    }
    return hunks;
}

} // namespace

namespace {

// Lines of a collected entry for diffing: regular files split content;
// symlinks are a single line (the target) without trailing newline.
FileLines entry_lines(const Collected& c)
{
    if (c.is_symlink) {
        FileLines fl;
        fl.lines.push_back(c.content);
        fl.ends_nl = false;
        return fl;
    }
    return split_content(c.content);
}

std::string diff_label(const std::string& side, const std::string& wid,
                       const std::string& rel)
{
    return quote_git_path(side + "/" + wid + "/" + rel);
}

// Emit one file block. `old_rel`/`new_rel` are workdir-relative paths (one
// may be empty for add/delete but not both). `old_c`/`new_c` are null when
// the side is absent. `rename` marks a rename pair (old_rel != new_rel with
// shared content lineage). `frozen` (usually the diff options'
// frozen_mtimes map) adds the `frozen-mtime <ts>` extended header for a
// frozen path, immediately after the `diff --git` line and before any
// old/new mode lines — the same header region the parser scans for modes.
// Returns block text ending with '\n'. Callers never invoke this for a pair
// with identical content, mode, and path: those are skipped before dispatch.
std::string emit_block(const std::string& wid, const std::string& old_rel,
                       const std::string& new_rel, const Collected* old_c,
                       const Collected* new_c, bool rename, int similarity,
                       const std::map<std::string, uint64_t>* frozen)
{
    std::string out;
    // For adds and deletes the `diff --git` line repeats the live path on
    // both sides (like git); /dev/null appears only on the ---/+++ lines.
    std::string da = old_c ? diff_label("a", wid, old_rel) : "/dev/null";
    std::string db = new_c ? diff_label("b", wid, new_rel) : "/dev/null";
    if (!old_c)
        da = diff_label("a", wid, new_rel);
    if (!new_c)
        db = diff_label("b", wid, old_rel);
    out += "diff --git " + da + " " + db + "\n";
    // Frozen-mtime attribute header for the block's live path (the rename
    // destination when both sides exist, since that is the path the checkout
    // uses). Deleted blocks carry no attribute: the file is gone, and a
    // frozen entry for it dies with the block (deletion is an explicit op
    // that prunes the pin, so no warning there). Symlinks are skipped too —
    // freezing is a regular-file attribute — but a frozen path whose new
    // side is a symlink (a typechange in place, or a pending mv re-keyed to
    // a destination that is now a symlink; symlink flips never pair as
    // renames, so the add-block path covers that) loses its pin when this
    // patch replaces the stored one, so warn instead of dropping it
    // silently. Only callers that thread a frozen map (commit, rebase, the
    // uncommitted diff) can reach the warning.
    if (frozen && new_c) {
        std::string live = new_rel.empty() ? old_rel : new_rel;
        if (new_c->is_symlink) {
            if (frozen->count(live))
                warn("dropping the frozen mtime for '" + live +
                     "': the file is now a symlink");
        } else {
            auto it = frozen->find(live);
            if (it != frozen->end())
                out += kFrozenMtimeHeader + std::to_string(it->second) + "\n";
        }
    }
    std::string om = old_c ? mode_of(*old_c) : "";
    std::string nm = new_c ? mode_of(*new_c) : "";
    if (!old_c && new_c) {
        out += "new file mode " + nm + "\n";
    } else if (old_c && !new_c) {
        out += "deleted file mode " + om + "\n";
    } else if (om != nm) {
        out += "old mode " + om + "\n";
        out += "new mode " + nm + "\n";
    }
    if (rename) {
        out += "similarity index " + std::to_string(similarity) + "%\n";
        out += "rename from " + quote_git_path(old_rel) + "\n";
        out += "rename to " + quote_git_path(new_rel) + "\n";
    }
    // Binary (NUL-bearing) content travels as base64 `GIT binary patch`
    // literals, never as text hunks (unified diffs cannot carry NUL bytes).
    // Format (projeny-internal; git's own base85+deflate is not decoded):
    //   GIT binary patch
    //   literal <new-size>
    //   <base64 of the new bytes, 76 columns, zero lines when empty>
    //   <blank line>
    //   literal <old-size>
    //   <base64 of the old bytes, 76 columns, zero lines when empty>
    //   <blank line>
    // The new literal comes first (like git), so forward application writes
    // it and reverse detection compares against the old one. Base64 output
    // never contains spaces, so no content line collides with a `literal `,
    // `diff --git `, or marker line. Pure renames and mode-only changes of
    // binaries carry no payload (like git).
    {
        bool ob = old_c && !old_c->is_symlink && old_c->is_binary;
        bool nb = new_c && !new_c->is_symlink && new_c->is_binary;
        if (ob || nb) {
            bool same_bytes = old_c && new_c &&
                              old_c->is_symlink == new_c->is_symlink &&
                              old_c->content == new_c->content;
            if (same_bytes) {
                // Pure rename or mode-only change of a binary: the mode and
                // rename lines above already describe it (no ---/+++/payload,
                // like git).
                return out;
            }
            std::string old_bytes = old_c ? old_c->content : "";
            std::string new_bytes = new_c ? new_c->content : "";
            auto emit_literal = [&](const std::string& bytes) {
                out += "literal " + std::to_string(bytes.size()) + "\n";
                std::string b64 = base64_encode(bytes);
                for (size_t i = 0; i < b64.size(); i += 76)
                    out += b64.substr(i, 76) + "\n";
                out += "\n";
            };
            out += "GIT binary patch\n";
            emit_literal(new_bytes);
            emit_literal(old_bytes);
            return out;
        }
    }
    if (!old_c || !new_c) {
        // Add/delete of an empty file carries no ---/+++/hunks (like git).
        FileLines fl = new_c ? entry_lines(*new_c) : entry_lines(*old_c);
        if (fl.lines.empty())
            return out;
    }
    std::string minus = old_c ? quote_git_path("a/" + wid + "/" + old_rel) : "/dev/null";
    std::string plus = new_c ? quote_git_path("b/" + wid + "/" + new_rel) : "/dev/null";
    if (rename) {
        minus = quote_git_path("a/" + wid + "/" + old_rel);
        plus = quote_git_path("b/" + wid + "/" + new_rel);
    }
    // Mode-only change: no content hunks (like git).
    if (old_c && new_c && entry_lines(*old_c).lines == entry_lines(*new_c).lines &&
        ((old_c->is_symlink == new_c->is_symlink) &&
         entry_lines(*old_c).ends_nl == entry_lines(*new_c).ends_nl)) {
        // Content identical: mode-only changes and pure renames emit no
        // hunks (like git). A fully identical pair should never reach here
        // (the caller skips it), so a header-only block is the fallback.
        return out;
    }
    out += "--- " + minus + "\n";
    out += "+++ " + plus + "\n";
    FileLines ofl = old_c ? entry_lines(*old_c) : FileLines();
    FileLines nfl = new_c ? entry_lines(*new_c) : FileLines();
    std::vector<Op> script = myers_lines(ofl.lines, nfl.lines);
    std::vector<Hunk> hunks = build_hunks(script, 3);
    for (auto& h : hunks) {
        char buf[128];
        snprintf(buf, sizeof(buf), "@@ -%ld,%ld +%ld,%ld @@", h.old_start,
                 h.old_count, h.new_start, h.new_count);
        out += buf;
        out += "\n";
        // Locate last old-side / new-side line for no-newline markers.
        long last_old = -1, last_new = -1;
        for (size_t k = 0; k < h.lines.size(); ++k) {
            if (h.lines[k].kind == ' ' || h.lines[k].kind == '-')
                last_old = (long)k;
            if (h.lines[k].kind == ' ' || h.lines[k].kind == '+')
                last_new = (long)k;
        }
        bool touch_old = old_c && (h.old_start + h.old_count ==
                                   (long)ofl.lines.size() + 1) &&
                         !ofl.lines.empty();
        // Insertion-only hunk at end (old_count==0, at EOF): old marker only
        // if the old file lacks newline AND the hunk has no old lines — git
        // omits it there; skip.
        if (h.old_count == 0)
            touch_old = false;
        bool touch_new = new_c && (h.new_start + h.new_count ==
                                   (long)nfl.lines.size() + 1) &&
                         !nfl.lines.empty();
        if (h.new_count == 0)
            touch_new = false;
        bool old_mark = touch_old && !ofl.ends_nl;
        bool new_mark = touch_new && !nfl.ends_nl;
        bool shared_single = old_mark && new_mark && last_old == last_new &&
                             last_old >= 0 && h.lines[(size_t)last_old].kind == ' ';
        for (size_t k = 0; k < h.lines.size(); ++k) {
            const Op& o = h.lines[k];
            if (o.kind == ' ' && o.line.empty()) {
                out += "\n"; // blank context like git (no leading space)
            } else {
                out += std::string(1, o.kind) + o.line + "\n";
            }
            if (shared_single && (long)k == last_old) {
                out += "\\ No newline at end of file\n";
            } else {
                if (old_mark && (long)k == last_old && !shared_single &&
                    (o.kind == ' ' || o.kind == '-'))
                    out += "\\ No newline at end of file\n";
                if (new_mark && (long)k == last_new && !shared_single &&
                    (o.kind == ' ' || o.kind == '+'))
                    out += "\\ No newline at end of file\n";
            }
        }
    }
    return out;
}

// Multiset line similarity 0..100 between two regular contents.
int line_similarity(const std::string& a, const std::string& b)
{
    FileLines fa = split_content(a);
    FileLines fb = split_content(b);
    if (fa.lines.empty() && fb.lines.empty())
        return 100;
    if (fa.lines.empty() || fb.lines.empty())
        return 0;
    std::unordered_multiset<std::string> setb(fb.lines.begin(), fb.lines.end());
    size_t common = 0;
    for (auto& l : fa.lines) {
        auto it = setb.find(l);
        if (it != setb.end()) {
            ++common;
            setb.erase(it);
        }
    }
    size_t denom = fa.lines.size() + fb.lines.size();
    return (int)(200 * common / denom);
}

struct Pending {
    std::string rel;
    Collected c;
};

} // namespace

namespace {

// Forward declarations: the pending-aware differ below resolves pending
// rename sources through the committed patch, which needs the patch parser
// defined further down (same unnamed namespace, so these complete there).
struct PBlock;
std::vector<PBlock> parse_patch(const std::string& patch, const std::string& wid);

// Strictly parse a `frozen-mtime` header value: decimal digits only — no
// sign, no whitespace, no other characters, and no overflow. strtoull would
// silently clamp out-of-range values to ULLONG_MAX and map garbage to 0,
// which reads as "no header" and would silently disable the attribute.
// Dies otherwise.
uint64_t parse_frozen_mtime_value(const std::string& v)
{
    if (v.empty() || v.find_first_not_of("0123456789") != std::string::npos ||
        v.size() > 20)
        die("malformed frozen-mtime header '" + v +
            "' (expected a unix-epoch timestamp in seconds)");
    errno = 0;
    unsigned long long val = strtoull(v.c_str(), nullptr, 10);
    if (errno == ERANGE)
        die("malformed frozen-mtime header '" + v +
            "' (timestamp overflows 64 bits)");
    return (uint64_t)val;
}

std::vector<std::pair<std::string, std::string>> committed_rename_pairs(
    const std::string& patch, const std::string& wid);

} // namespace

// True when `rel` is `k` itself or lives under kept directory `k` — the
// same predicate commit and the pending-aware diff use for their keep
// lists (add/rm/mv take directories, so keep entries may name dirs).
// Exported (declared in vcs.h) so `commit`'s disappeared check shares it.
bool vcs_covers_keep_path(const std::vector<std::string>& keep,
                          const std::string& rel)
{
    for (const auto& k : keep) {
        if (k.empty())
            continue;
        if (rel == k)
            return true;
        if (rel.size() > k.size() && rel.compare(0, k.size(), k) == 0 &&
            rel[k.size()] == '/')
            return true;
    }
    return false;
}

bool vcs_delete_covered(
    const std::vector<std::string>& keep,
    const std::vector<std::pair<std::string, std::string>>* renames,
    const std::string& rel,
    const std::function<bool(const std::string&)>& counterpart_exists)
{
    for (const auto& k : keep) {
        if (k.empty())
            continue;
        if (rel == k)
            return true; // exact: a removal (or file rename source)
        if (!(rel.size() > k.size() &&
              rel.compare(0, k.size(), k) == 0 && rel[k.size()] == '/'))
            continue;
        // rel lives under k. Authoritative unless k is exactly a
        // pending rename source (a directory move).
        bool is_dir_move = false;
        if (renames) {
            for (const auto& rn : *renames) {
                if (rn.first != k)
                    continue;
                is_dir_move = true;
                std::string counterpart =
                    rn.second + rel.substr(k.size());
                if (counterpart_exists(counterpart))
                    return true; // moved with the directory
            }
        }
        if (!is_dir_move)
            return true;
    }
    // Only directory-move sources covered rel and none of them moved
    // this file: unregistered.
    return false;
}

// True for legacy scratch entries that must never be diffed or reported
// (left behind inside workdirs by older crashed runs; scratch now lives
// outside): ".projeny-tmp*" at the workdir root or under any directory.
bool vcs_is_scratch_rel(const std::string& rel)
{
    // The prefix compare covers ".projeny-tmp" itself and every suffix.
    if (rel.compare(0, 13, ".projeny-tmp") == 0)
        return true;
    if (rel.find("/.projeny-tmp") != std::string::npos)
        return true;
    return false;
}

std::string vcs_diff_trees_ex(const std::string& base_tree,
                              const std::string& workdir, const std::string& wid,
                              const VcsDiffOpts& opts)
{
    std::map<std::string, Collected> base, work;
    collect_into(base_tree, "", base, "base tree");
    collect_into(workdir, "", work, "workdir");
    bool warned_scratch = false;
    auto skip_scratch = [&](const std::string& rel) -> bool {
        if (vcs_is_scratch_rel(rel)) {
            if (!warned_scratch) {
                warn("ignoring stale '.projeny-tmp*' scratch entries inside the workdir");
                warned_scratch = true;
            }
            return true;
        }
        return false;
    };
    // Refined delete-side keep coverage. Callers pass a non-null
    // delete_keep (the null handling lives at the call sites: delete
    // blocks are kept, rename old sides are ok, and every missing base
    // file counts as disappeared). The rule lives in vcs_delete_covered;
    // this probe counts a moved file's counterpart as present when it
    // exists in the workdir OR is itself registered by the full keep list
    // (e.g. `projeny rm` after the move, or a further pending rename of
    // the moved-to path).
    auto delete_covered = [&](const std::string& rel) -> bool {
        return vcs_delete_covered(*opts.delete_keep, opts.forced_renames, rel,
                                  [&](const std::string& counterpart) {
                                      return work.count(counterpart) > 0 ||
                                             vcs_covers_keep_path(
                                                 *opts.delete_keep,
                                                 counterpart);
                                  });
    };
    // Partition.
    std::map<std::string, Collected> common_base, common_work;
    std::vector<Pending> deleted, added;
    for (auto& kv : base) {
        if (skip_scratch(kv.first))
            continue;
        auto it = work.find(kv.first);
        if (it == work.end())
            deleted.push_back({kv.first, kv.second});
        else {
            common_base[kv.first] = kv.second;
            common_work[kv.first] = it->second;
        }
    }
    for (auto& kv : work) {
        if (skip_scratch(kv.first))
            continue;
        if (base.find(kv.first) == base.end())
            added.push_back({kv.first, kv.second});
    }
    struct BlockJob {
        std::string sort_key;
        std::string text;
        char kind = 'm'; // 'm' modify/typechange, 'r' rename, 'd' delete, 'a' add
        std::string old_rel, new_rel; // rename sides (for keep filtering)
        const Collected* old_c = nullptr; // rename sides' content (conversions)
        const Collected* new_c = nullptr;
    };
    std::vector<BlockJob> jobs;
    // Modified in place.
    for (auto& kv : common_base) {
        const std::string& rel = kv.first;
        const Collected& ob = kv.second;
        const Collected& nw = common_work[rel];
        if (ob.is_symlink == nw.is_symlink && ob.content == nw.content &&
            mode_of(ob) == mode_of(nw)) {
            // Unchanged — except that a frozen path must still emit its
            // attribute: an attribute-only block (diff --git + frozen-mtime,
            // no ---/+++/hunks) keeps the header alive in a patch that commit
            // or rebase regenerates from scratch. Only the regenerate path
            // asks for this (frozen_attribute_blocks), so a clean checkout of
            // a frozen project still diffs empty.
            if (opts.frozen_attribute_blocks && opts.frozen_mtimes &&
                !nw.is_symlink && opts.frozen_mtimes->count(rel)) {
                BlockJob j;
                j.sort_key = rel;
                j.text = emit_block(wid, rel, rel, &ob, &nw, false, 0,
                                    opts.frozen_mtimes);
                jobs.push_back(std::move(j));
            }
            continue; // unchanged: identical pairs never reach emit_block
        }
        if (ob.is_symlink != nw.is_symlink) {
            // Typechange: emit as delete+add hunks in one block (mode lines
            // record the transition; hunks carry old->new content).
            BlockJob j;
            j.sort_key = rel;
            j.text = emit_block(wid, rel, rel, &ob, &nw, false, 0,
                                opts.frozen_mtimes);
            jobs.push_back(std::move(j));
            continue;
        }
        BlockJob j;
        j.sort_key = rel;
        j.text = emit_block(wid, rel, rel, &ob, &nw, false, 0,
                            opts.frozen_mtimes);
        jobs.push_back(std::move(j));
    }
    // Rename detection: forced (pending-op) pairs first, then exact
    // content matches, then similar pairs (>50%).
    std::vector<bool> used_d(deleted.size(), false), used_a(added.size(), false);
    struct Rename {
        size_t di, ai;
        int sim;
    };
    std::vector<Rename> chosen;
    if (opts.forced_renames && !opts.forced_renames->empty()) {
        // Pending `projeny mv` pairs pair up before any content guesswork:
        // a move must render as a rename even when the moved file's content
        // diverged beyond the similarity threshold, and it must win the
        // pairing when several same-content files changed hands at once.
        // Committed rename blocks, parsed once, for resolving a pending
        // source backwards to a base-tree path: commit diffs the raw
        // archive, which predates the committed renames, so a pending
        // rename of an already-committed rename names no base path until
        // unwound. Empty (the expected tree of a fresh setup already
        // contains the committed renames): sources resolve directly.
        std::vector<std::pair<std::string, std::string>> committed;
        if (opts.committed_patch && !opts.committed_patch->empty())
            committed = committed_rename_pairs(*opts.committed_patch, wid);
        // A map key names a directory when some file lives under it (the
        // maps hold file entries only; keys are sorted, so everything
        // under `rel/` is one contiguous range).
        auto is_dir_key = [](const std::map<std::string, Collected>& m,
                             const std::string& rel) -> bool {
            std::string pfx = rel + "/";
            auto it = m.lower_bound(pfx);
            return it != m.end() && it->first.compare(0, pfx.size(), pfx) == 0;
        };
        std::unordered_map<std::string, size_t> didx, aidx;
        for (size_t i = 0; i < deleted.size(); ++i)
            didx.emplace(deleted[i].rel, i);
        for (size_t j = 0; j < added.size(); ++j)
            aidx.emplace(added[j].rel, j);
        // Resolve `s` backwards through committed renames (a from -> to
        // block with to == s replaces s by from) until it names a
        // base-tree path; "" when unresolvable (e.g. the source was itself
        // committed-added, so no base path backs it and no delete side is
        // needed — the add side is settled by the keep filtering). Cycles
        // cannot occur in a projeny-generated patch; guarded anyway.
        auto resolve_to_base = [&](const std::string& start) -> std::string {
            std::string s = start;
            std::unordered_set<std::string> seen;
            seen.insert(s);
            while (!base.count(s) && !is_dir_key(base, s)) {
                bool advanced = false;
                for (const auto& cp : committed) {
                    if (cp.second == s) {
                        s = cp.first;
                        advanced = true;
                        break;
                    }
                }
                if (!advanced || !seen.insert(s).second)
                    return "";
            }
            return s;
        };
        // Pair one (source, destination): both sides must still be
        // pending, and a symlink flip is not a rename (typechanges are
        // their own thing, like the in-place case). Regular non-binary
        // pairs show their real similarity; everything else displays 100.
        auto try_pair = [&](const std::string& s, const std::string& d) {
            auto di = didx.find(s);
            if (di == didx.end())
                return;
            auto ai = aidx.find(d);
            if (ai == aidx.end())
                return;
            if (used_d[di->second] || used_a[ai->second])
                return;
            const Collected& oc = deleted[di->second].c;
            const Collected& nc = added[ai->second].c;
            if (oc.is_symlink != nc.is_symlink)
                return;
            int sim = 100;
            if (!oc.is_symlink && !oc.is_binary && !nc.is_binary)
                sim = line_similarity(oc.content, nc.content);
            used_d[di->second] = true;
            used_a[ai->second] = true;
            chosen.push_back({di->second, ai->second, sim});
        };
        for (const auto& fr : *opts.forced_renames) {
            std::string s = resolve_to_base(fr.first);
            if (s.empty())
                continue; // no base path: nothing to pair (no delete side)
            if (is_dir_key(base, s) && is_dir_key(work, fr.second)) {
                // Directory move: pair every base file under the source
                // dir with the same relative path under the destination
                // (files the move dropped or replaced stay unpaired).
                std::string pfx = s + "/";
                for (auto it = base.lower_bound(pfx);
                     it != base.end() &&
                     it->first.compare(0, pfx.size(), pfx) == 0; ++it) {
                    try_pair(it->first, fr.second + it->first.substr(s.size()));
                }
            } else {
                try_pair(s, fr.second);
            }
        }
    }
    std::vector<Rename> renames;
    for (size_t i = 0; i < deleted.size(); ++i) {
        for (size_t j = 0; j < added.size(); ++j) {
            if (deleted[i].c.is_symlink != added[j].c.is_symlink)
                continue;
            if (deleted[i].c.content == added[j].c.content) {
                renames.push_back({i, j, 100});
            }
        }
    }
    // Greedy exact pairing in sorted order for determinism.
    std::sort(renames.begin(), renames.end(), [&](const Rename& x, const Rename& y) {
        if (deleted[x.di].rel != deleted[y.di].rel)
            return deleted[x.di].rel < deleted[y.di].rel;
        return added[x.ai].rel < added[y.ai].rel;
    });
    for (auto& r : renames) {
        if (!used_d[r.di] && !used_a[r.ai]) {
            used_d[r.di] = true;
            used_a[r.ai] = true;
            chosen.push_back(r);
        }
    }
    // Similarity pairing for the rest (regular files only).
    struct Cand {
        size_t di, ai;
        int sim;
    };
    std::vector<Cand> cands;
    for (size_t i = 0; i < deleted.size(); ++i) {
        if (used_d[i] || deleted[i].c.is_symlink || deleted[i].c.is_binary)
            continue;
        for (size_t j = 0; j < added.size(); ++j) {
            if (used_a[j] || added[j].c.is_symlink || added[j].c.is_binary)
                continue;
            int sim = line_similarity(deleted[i].c.content, added[j].c.content);
            if (sim >= 50)
                cands.push_back({i, j, sim});
        }
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y) {
        return x.sim > y.sim;
    });
    for (auto& c : cands) {
        if (!used_d[c.di] && !used_a[c.ai]) {
            used_d[c.di] = true;
            used_a[c.ai] = true;
            chosen.push_back({c.di, c.ai, c.sim});
        }
    }
    for (auto& r : chosen) {
        const std::string& orel = deleted[r.di].rel;
        const std::string& nrel = added[r.ai].rel;
        const Collected& ob = deleted[r.di].c;
        const Collected& nw = added[r.ai].c;
        BlockJob j;
        j.sort_key = nrel;
        j.kind = 'r';
        j.old_rel = orel;
        j.new_rel = nrel;
        j.old_c = &ob;
        j.new_c = &nw;
        j.text = emit_block(wid, orel, nrel, &ob, &nw, true, r.sim,
                            opts.frozen_mtimes);
        jobs.push_back(std::move(j));
    }
    for (size_t i = 0; i < deleted.size(); ++i) {
        if (used_d[i])
            continue;
        BlockJob j;
        j.sort_key = deleted[i].rel;
        j.kind = 'd';
        j.old_rel = deleted[i].rel;
        j.old_c = &deleted[i].c;
        j.text = emit_block(wid, deleted[i].rel, "", &deleted[i].c, nullptr,
                            false, 0, opts.frozen_mtimes);
        jobs.push_back(std::move(j));
    }
    for (size_t j = 0; j < added.size(); ++j) {
        if (used_a[j])
            continue;
        BlockJob b;
        b.sort_key = added[j].rel;
        b.kind = 'a';
        b.new_rel = added[j].rel;
        b.new_c = &added[j].c;
        b.text = emit_block(wid, "", added[j].rel, nullptr, &added[j].c,
                            false, 0, opts.frozen_mtimes);
        jobs.push_back(std::move(b));
    }
    // Keep-list filtering (pending-aware callers only: both pointers null
    // keeps every block, which is what the plain tree diff must do). An
    // untracked anything (text, symlink, or binary) is dropped; an
    // unregistered deletion is dropped; a rename stands only when its
    // covered sides justify it, degrading to the surviving side's
    // delete/add so the tracked half of the move is still recorded.
    if (opts.add_keep || opts.delete_keep) {
        std::vector<BlockJob> kept;
        kept.reserve(jobs.size());
        for (BlockJob& j : jobs) {
            if (j.kind == 'a' && opts.add_keep &&
                !vcs_covers_keep_path(*opts.add_keep, j.new_rel))
                continue; // untracked add: leave it out, like git
            if (j.kind == 'd' && opts.delete_keep &&
                !delete_covered(j.old_rel))
                continue; // unregistered deletion: not part of this diff
            if (j.kind == 'r' && (opts.add_keep || opts.delete_keep)) {
                bool o_ok =
                    !opts.delete_keep || delete_covered(j.old_rel);
                bool n_ok =
                    !opts.add_keep ||
                    vcs_covers_keep_path(*opts.add_keep, j.new_rel);
                if (!o_ok && !n_ok)
                    continue; // neither side tracked: drop the move
                if (!n_ok) {
                    // Only the old side is tracked: render as a delete.
                    j.text = emit_block(wid, j.old_rel, "", j.old_c, nullptr,
                                        false, 0, opts.frozen_mtimes);
                    j.sort_key = j.old_rel;
                    j.kind = 'd';
                } else if (!o_ok) {
                    // Only the new side is tracked: render as an add.
                    j.text = emit_block(wid, "", j.new_rel, nullptr, j.new_c,
                                        false, 0, opts.frozen_mtimes);
                    j.kind = 'a';
                }
            }
            kept.push_back(std::move(j));
        }
        jobs.swap(kept);
    }
    // Authoritative disappearance list: base-tree files missing from the
    // workdir and not covered by the delete keep list. Collected from the
    // trees themselves rather than the blocks, so content-based rename
    // pairing can never attribute a disappearance to the wrong source.
    // With a null delete_keep, nothing counts as registered (callers that
    // ask for disappearances without filtering are telling the truth);
    // with one, the refined predicate decides (a directory move covers
    // its inner files only when they moved with it).
    if (opts.disappeared) {
        for (const auto& kv : base) {
            if (skip_scratch(kv.first))
                continue;
            if (work.count(kv.first))
                continue;
            if (opts.delete_keep && delete_covered(kv.first))
                continue;
            opts.disappeared->push_back(kv.first);
        }
        std::sort(opts.disappeared->begin(), opts.disappeared->end());
        opts.disappeared->erase(
            std::unique(opts.disappeared->begin(), opts.disappeared->end()),
            opts.disappeared->end());
    }
    std::sort(jobs.begin(), jobs.end(), [](const BlockJob& x, const BlockJob& y) {
        return x.sort_key < y.sort_key;
    });
    std::string out;
    for (auto& j : jobs)
        out += j.text;
    return out;
}

std::string vcs_diff_trees(const std::string& base_tree, const std::string& workdir,
                           const std::string& wid)
{
    // Plain tree diff: every pending-aware behavior is keyed off a null
    // option pointer, so this must stay byte-identical to the pre-ex
    // implementation (2-dir diff, setup's U-diff, and rebase rely on it).
    VcsDiffOpts opts;
    return vcs_diff_trees_ex(base_tree, workdir, wid, opts);
}

namespace {

// ---- Patch parsing (raw split; '\r' preserved in content) ----

struct PHunkLine {
    char op; // ' ', '-', '+'
    std::string text;
};

struct PHunk {
    long old_start = 1, old_count = 0, new_start = 1, new_count = 0;
    std::vector<PHunkLine> lines;
    bool old_no_nl = false;
    bool new_no_nl = false;
};

struct PBlock {
    // Workdir-relative paths (wid stripped); has_old/has_new false for /dev/null.
    std::string old_rel, new_rel;
    bool has_old = false, has_new = false;
    bool is_rename = false;
    std::string rename_from, rename_to;
    std::string old_mode, new_mode; // "" if absent
    // Frozen-mtime attribute (unix-epoch seconds) from the `frozen-mtime`
    // extended header. 0 means "no header" (the sentinel vcs_frozen_mtimes
    // and emit_block test for), so an archive mtime of epoch-0 can never be
    // frozen — practically impossible, and treated as absent if it ever
    // appears in a patch.
    uint64_t frozen_mtime = 0;
    bool is_new = false, is_deleted = false;
    bool is_binary = false;
    // Binary payload (projeny base64 `GIT binary patch` literals): the first
    // literal is the new bytes, the second the old bytes. binary_has_payload
    // is false for `Binary files ... differ` stanzas and foreign (git
    // base85/deflate) sections our decoder rejects: those blocks fail
    // cleanly at apply time (merge conflict), never die and never apply.
    bool binary_has_payload = false;
    std::string bin_old;
    std::string bin_new;
    bool is_combined = false; // "diff --cc"/"diff --combined" (unsupported)
    std::string header_line;  // raw first line of the block (for diagnostics)
    std::string raw;          // raw block text including trailing newline
    std::vector<PHunk> hunks;
    bool old_no_nl_file = false, new_no_nl_file = false;
    std::string git_a, git_b; // raw sides from diff --git (for fallback)
};

// Strip one trailing '\r' for metadata prefix tests (content untouched).
std::string no_cr(const std::string& l)
{
    if (!l.empty() && l.back() == '\r')
        return l.substr(0, l.size() - 1);
    return l;
}

// Reject patch member paths that would escape the tree, exactly like tar
// unpack validation (tree.cc check_archive_members_safe): absolute paths and
// any ".." component die outright. Called after a/b + wid stripping, so an
// attacker label like "a/<wid>/../../etc/evil" (which strips to
// "../../etc/evil") dies here before anything is written: join_path()
// concatenates blindly, so a ".." rel would resolve outside treedir at the
// OS level. Runs at parse time, i.e. before any block is applied, so a
// malicious patch is refused without touching the tree.
void check_patch_member_path(const std::string& orig_label,
                             const std::string& rel)
{
    // An empty rel (or ".") means the label stripped down to the workdir
    // root itself (e.g. "a/<wid>" or "a/<wid>/." with wid <wid>):
    // join_path(treedir, rel) would then target the tree directory, not a
    // member. Refuse it exactly like an escape — there is no legitimate
    // file patch against the root.
    if (rel.empty() || rel == ".")
        die("patch member path '" + orig_label +
            "' resolves to the tree root; refusing (path traversal)");
    if (rel[0] == '/')
        die("patch member path '" + orig_label +
            "' is absolute; refusing (path traversal)");
    size_t i = 0;
    while (i <= rel.size()) {
        size_t j = rel.find('/', i);
        std::string comp =
            (j == std::string::npos) ? rel.substr(i) : rel.substr(i, j - i);
        if (comp == "..")
            die("patch member path '" + orig_label +
                "' contains '..'; refusing (path traversal)");
        if (j == std::string::npos)
            break;
        i = j + 1;
    }
}

// Map a ---/+++/diff--git label to workdir-relative form. Sets *missing for
// /dev/null. Strips one a/ or b/ component, then the wid component.
// Dies on absolute or ".."-carrying results (see check_patch_member_path).
std::string label_to_rel(const std::string& lab, const std::string& wid,
                         bool* missing)
{
    std::string u = unquote_git_path(lab);
    if (u == "/dev/null") {
        *missing = true;
        return "";
    }
    *missing = false;
    if (u.compare(0, 2, "a/") == 0 || u.compare(0, 2, "b/") == 0)
        u = u.substr(2);
    else if (u == "a" || u == "b")
        u = "";
    if (!wid.empty()) {
        if (u == wid)
            u = "";
        else if (u.compare(0, wid.size() + 1, wid + "/") == 0)
            u = u.substr(wid.size() + 1);
    }
    check_patch_member_path(lab, u);
    return u;
}

void split_label_stamp_raw(const std::string& p, std::string* body,
                           std::string* stamp)
{
    size_t tab = p.find('\t');
    if (tab == std::string::npos) {
        *body = p;
        *stamp = "";
    } else {
        *body = p.substr(0, tab);
        *stamp = p.substr(tab);
    }
}

bool parse_hunk_header(const std::string& line, long* os, long* oc, long* ns,
                       long* nc)
{
    // "@@ -os[,oc] +ns[,nc] @@..."
    if (line.compare(0, 3, "@@ ") != 0)
        return false;
    size_t end = line.find(" @@", 3);
    if (end == std::string::npos)
        return false;
    std::string range = line.substr(3, end - 3);
    size_t sp = range.find(' ');
    if (sp == std::string::npos)
        return false;
    std::string ro = range.substr(0, sp), rn = range.substr(sp + 1);
    if (ro.empty() || ro[0] != '-' || rn.empty() || rn[0] != '+')
        return false;
    ro = ro.substr(1);
    rn = rn.substr(1);
    auto parse_one = [](const std::string& r, long* s, long* c) {
        size_t comma = r.find(',');
        if (comma == std::string::npos) {
            *s = strtol(r.c_str(), nullptr, 10);
            *c = 1;
        } else {
            *s = strtol(r.substr(0, comma).c_str(), nullptr, 10);
            *c = strtol(r.substr(comma + 1).c_str(), nullptr, 10);
        }
    };
    parse_one(ro, os, oc);
    parse_one(rn, ns, nc);
    return true;
}

// Tokenize a "diff --git" remainder honoring C-quotes (for fallback paths).
bool parse_git_sides(const std::string& rest, std::string* a, std::string* b)
{
    if (rest.empty())
        return false;
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
    if (rest[0] == '"') {
        size_t q = qend(rest, 0);
        if (q == std::string::npos || q + 1 >= rest.size() || rest[q + 1] != ' ')
            return false;
        *a = unquote_git_path(rest.substr(0, q + 1));
        *b = unquote_git_path(rest.substr(q + 2));
        return true;
    }
    // Unquoted: split on " b/" preferably, else last " a/" or quoted b-side.
    size_t pos = rest.find(" b/");
    if (pos != std::string::npos) {
        *a = unquote_git_path(rest.substr(0, pos));
        *b = unquote_git_path(rest.substr(pos + 1));
        return !a->empty() && !b->empty();
    }
    // Single candidate on " a/" or quote.
    size_t best = std::string::npos;
    for (size_t i = 0; i < rest.size(); ++i) {
        if (rest[i] == ' ' && i + 1 < rest.size() &&
            (rest.compare(i + 1, 2, "a/") == 0 || rest[i + 1] == '"'))
            best = i;
    }
    if (best == std::string::npos)
        return false;
    *a = unquote_git_path(rest.substr(0, best));
    *b = unquote_git_path(rest.substr(best + 1));
    return !a->empty() && !b->empty();
}

std::vector<PBlock> parse_patch(const std::string& patch, const std::string& wid)
{
    std::vector<PBlock> out;
    if (patch.empty())
        return out;
    if (patch.find('\0') != std::string::npos)
        die("patch contains NUL bytes; refusing (a well-formed patch is text: binary content travels base64-encoded)");
    std::vector<std::string> lines = split_raw(patch);
    // NOTE: no trailing-empty pop here. split_raw never yields a spurious
    // final element (a trailing '\n' produces no extra entry), so a trailing
    // "" is always a real blank line — typically trailing context of the
    // last hunk or a binary literal terminator — and dropping it would
    // corrupt raw roundtrips (the setup/commit filters rejoin blocks
    // byte-exact) and miscount the last hunk.
    std::vector<size_t> starts;
    std::vector<bool> is_cc;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string t = no_cr(lines[i]);
        if (t.compare(0, 11, "diff --git ") == 0) {
            starts.push_back(i);
            is_cc.push_back(false);
        } else if (t.compare(0, 10, "diff --cc ") == 0 ||
                   t.compare(0, 16, "diff --combined ") == 0) {
            // Combined merge diffs are not applyable; treat each as its own
            // block so per-file application reports it as a failure with a
            // usable path instead of silently ignoring it (which would
            // diverge from parsers that count every diff header).
            starts.push_back(i);
            is_cc.push_back(true);
        }
    }
    for (size_t s = 0; s < starts.size(); ++s) {
        size_t e = (s + 1 < starts.size()) ? starts[s + 1] : lines.size();
        PBlock blk;
        std::string first = no_cr(lines[starts[s]]);
        blk.header_line = first;
        if (is_cc[s]) {
            blk.is_combined = true;
            // Combined header carries a single path ("diff --cc <path>").
            std::string rest;
            if (first.compare(0, 10, "diff --cc ") == 0)
                rest = first.substr(10);
            else
                rest = first.substr(16);
            rest = unquote_git_path(rest);
            // Strip a/ b/ and wid prefixes the same way as normal sides.
            bool miss = false;
            std::string rel = label_to_rel(rest, wid, &miss);
            if (!miss && !rel.empty()) {
                blk.old_rel = blk.new_rel = rel;
                blk.has_old = blk.has_new = true;
            } else if (!rest.empty() && rest != "/dev/null") {
                // Fall back to the raw token so diagnostics still name it.
                // Still traversal-checked: a combined-diff path must never
                // escape the tree either.
                std::string r = rest;
                if (r.compare(0, 2, "a/") == 0 || r.compare(0, 2, "b/") == 0)
                    r = r.substr(2);
                check_patch_member_path(rest, r);
                blk.old_rel = blk.new_rel = r;
                blk.has_old = blk.has_new = true;
            }
            // Parse hunks below (they will not match; apply fails cleanly).
        } else {
            std::string rest = first.size() > 11 ? first.substr(11) : "";
            std::string ga, gb;
            if (parse_git_sides(rest, &ga, &gb)) {
                blk.git_a = ga;
                blk.git_b = gb;
            }
        }
        std::string minus_body, plus_body;
        bool have_minus = false, have_plus = false;
        for (size_t k = starts[s] + 1; k < e; ++k) {
            std::string l = lines[k];
            std::string t = no_cr(l);
            if (t.compare(0, 9, "old mode ") == 0)
                blk.old_mode = t.substr(9);
            else if (t.compare(0, 9, "new mode ") == 0)
                blk.new_mode = t.substr(9);
            else if (t.compare(0, kFrozenMtimeHeaderLen,
                               kFrozenMtimeHeader) == 0)
                blk.frozen_mtime = parse_frozen_mtime_value(
                    t.substr(kFrozenMtimeHeaderLen));
            else if (t.compare(0, 17, "deleted file mode") == 0) {
                blk.is_deleted = true;
                std::string v = t.size() > 18 ? ltrim(t.substr(17)) : "";
                blk.old_mode = v;
            } else if (t.compare(0, 13, "new file mode") == 0) {
                blk.is_new = true;
                std::string v = t.size() > 14 ? ltrim(t.substr(13)) : "";
                blk.new_mode = v;
            } else if (t.compare(0, 12, "rename from ") == 0) {
                blk.is_rename = true;
                std::string v = t.substr(12);
                if (!v.empty() && v[0] == ' ')
                    v = v.substr(1);
                blk.rename_from = unquote_git_path(v);
            } else if (t.compare(0, 10, "rename to ") == 0) {
                blk.is_rename = true;
                std::string v = t.substr(10);
                if (!v.empty() && v[0] == ' ')
                    v = v.substr(1);
                blk.rename_to = unquote_git_path(v);
            } else if (t.compare(0, 12, "Binary files") == 0 ||

                       t.compare(0, 16, "GIT binary patch") == 0) {
                blk.is_binary = true;
            } else if (t.compare(0, 4, "--- ") == 0) {
                std::string body, stamp;
                split_label_stamp_raw(l.substr(4), &body, &stamp);
                // Body may carry trailing '\r' as content only for weird
                // names; keep verbatim (labels never end with content '\r'
                // except CRLF-named files, which stay consistent).
                minus_body = body;
                have_minus = true;
            } else if (t.compare(0, 4, "+++ ") == 0) {
                std::string body, stamp;
                split_label_stamp_raw(l.substr(4), &body, &stamp);
                plus_body = body;
                have_plus = true;
            }
        }
        // Binary payload parsing happens after the ---/+++/rename fallback
        // below (it needs no paths): see the GIT binary patch decode after
        // the hunk loop.
        if (have_minus) {
            bool miss = false;
            blk.old_rel = label_to_rel(minus_body, wid, &miss);
            blk.has_old = !miss;
            if (miss)
                blk.is_new = true;
        }
        if (have_plus) {
            bool miss = false;
            blk.new_rel = label_to_rel(plus_body, wid, &miss);
            blk.has_new = !miss;
            if (miss)
                blk.is_deleted = true;
        }
        // Normalize rename paths (bare workdir-relative, maybe wid-prefixed).
        // Rename from/to lines carry one path each (git may even emit
        // absolute paths there), so validate exactly like patch labels:
        // absolute or ".."-carrying results are traversal attempts.
        auto norm_rename = [&](const std::string& p) -> std::string {
            std::string u = p;
            if (u.compare(0, 2, "a/") == 0 || u.compare(0, 2, "b/") == 0)
                u = u.substr(2);
            if (!wid.empty()) {
                if (u == wid)
                    u = "";
                else if (u.compare(0, wid.size() + 1, wid + "/") == 0)
                    u = u.substr(wid.size() + 1);
            }
            check_patch_member_path(p, u);
            return u;
        };
        if (blk.is_rename) {
            blk.rename_from = norm_rename(blk.rename_from);
            blk.rename_to = norm_rename(blk.rename_to);
            if (!have_minus) {
                blk.old_rel = blk.rename_from;
                blk.has_old = true;
            }
            if (!have_plus) {
                blk.new_rel = blk.rename_to;
                blk.has_new = true;
            }
        }
        if (!have_minus && !have_plus) {
            // Mode-only / empty-file / pure-rename: fall back to diff --git
            // sides.
            if (!blk.git_a.empty() || !blk.git_b.empty()) {
                bool ma = false, mb = false;
                std::string ra = blk.git_a.empty()
                                     ? ""
                                     : label_to_rel(blk.git_a, wid, &ma);
                std::string rb = blk.git_b.empty()
                                     ? ""
                                     : label_to_rel(blk.git_b, wid, &mb);
                if (blk.git_a == "/dev/null" || blk.git_b == "/dev/null") {
                    blk.has_old = blk.git_a != "/dev/null";
                    blk.has_new = blk.git_b != "/dev/null";
                    blk.old_rel = blk.has_old ? ra : "";
                    blk.new_rel = blk.has_new ? rb : "";
                } else if (!ma && !mb) {
                    blk.has_old = blk.has_new = true;
                    blk.old_rel = ra;
                    blk.new_rel = rb;
                } else {
                    // One side /dev/null without ---/+++: new/deleted empty.
                    blk.has_old = !ma;
                    blk.has_new = !mb;
                    blk.old_rel = ra;
                    blk.new_rel = rb;
                    if (ma)
                        blk.is_new = true;
                    if (mb)
                        blk.is_deleted = true;
                }
            }
        }
        // Parse hunks.
        PHunk* cur = nullptr;
        for (size_t k = starts[s] + 1; k < e; ++k) {
            std::string l = lines[k];
            std::string t = no_cr(l);
            long os = 0, oc = 0, ns = 0, nc = 0;
            if (t.compare(0, 3, "@@ ") == 0 && parse_hunk_header(t, &os, &oc,
                                                                &ns, &nc)) {
                PHunk h;
                h.old_start = os;
                h.old_count = oc;
                h.new_start = ns;
                h.new_count = nc;
                blk.hunks.push_back(h);
                cur = &blk.hunks.back();
                continue;
            }
            if (!cur)
                continue;
            if (t.empty()) {
                cur->lines.push_back({' ', ""}); // blank context
                continue;
            }
            char c = l[0];
            if (c == ' ' || c == '-' || c == '+') {
                // " " (lone space) is a blank context line.
                std::string text = (l.size() == 1 && c == ' ') ? "" : l.substr(1);
                cur->lines.push_back({c, text});
            } else if (c == '\\') {
                // "\ No newline at end of file": attach to previous line.
                if (!cur->lines.empty()) {
                    char p = cur->lines.back().op;
                    if (p == '-')
                        cur->old_no_nl = true;
                    else if (p == '+')
                        cur->new_no_nl = true;
                    else if (p == ' ') {
                        cur->old_no_nl = true;
                        cur->new_no_nl = true;
                    }
                }
            } else {
                cur = nullptr; // end of hunk body
            }
        }
        for (auto& h : blk.hunks) {
            if (h.old_no_nl)
                blk.old_no_nl_file = true;
            if (h.new_no_nl)
                blk.new_no_nl_file = true;
        }
        // Raw block text (for the setup/commit filters that drop blocks).
        {
            std::string raw;
            for (size_t k = starts[s]; k < e; ++k) {
                raw += lines[k];
                raw += '\n';
            }
            blk.raw = raw;
        }
        // Decode our base64 `GIT binary patch` literals (new first, then
        // old, each terminated by a blank line). Anything else claiming to
        // be binary (a bare `Binary files ... differ` stanza, or a foreign
        // base85/deflate section) keeps is_binary with no payload: it fails
        // cleanly at apply time instead of dying here.
        if (blk.is_binary) {
            size_t g = starts.size(); // line index of "GIT binary patch"
            for (size_t k = starts[s] + 1; k < e; ++k) {
                if (no_cr(lines[k]) == "GIT binary patch") {
                    g = k;
                    break;
                }
            }
            if (g < e) {
                std::vector<std::string> payloads;
                bool ok = true;
                size_t k = g + 1;
                for (int li = 0; li < 2 && ok; ++li) {
                    if (k >= e || no_cr(lines[k]).compare(0, 8, "literal ") != 0) {
                        ok = false;
                        break;
                    }
                    long want = strtol(no_cr(lines[k]).substr(8).c_str(),
                                       nullptr, 10);
                    if (want < 0) {
                        ok = false;
                        break;
                    }
                    ++k;
                    std::string b64;
                    while (k < e && !no_cr(lines[k]).empty() &&
                           no_cr(lines[k]).compare(0, 8, "literal ") != 0 &&
                           no_cr(lines[k]).compare(0, 6, "delta ") != 0) {
                        // Base64 lines carry no '\r' from our encoder, but
                        // tolerate CRLF patches by stripping one trailing CR.
                        b64 += no_cr(lines[k]);
                        ++k;
                    }
                    std::string bytes;
                    if (!base64_decode(b64, &bytes) ||
                        (long)bytes.size() != want) {
                        ok = false;
                        break;
                    }
                    payloads.push_back(bytes);
                    // Consume the blank terminator when present (tolerate a
                    // missing one at the end of the block).
                    if (k < e && no_cr(lines[k]).empty())
                        ++k;
                }
                if (ok && payloads.size() == 2) {
                    blk.bin_new = payloads[0];
                    blk.bin_old = payloads[1];
                    blk.binary_has_payload = true;
                }
            }
        }
        out.push_back(blk);
    }
    return out;
}

// (from, to) pairs of every rename block in `patch` (wid-label form, wid ==
// `wid`). Used by the pending-aware diff to follow a pending rename source
// backwards through committed renames until it names a base-tree path.
std::vector<std::pair<std::string, std::string>> committed_rename_pairs(
    const std::string& patch, const std::string& wid)
{
    std::vector<std::pair<std::string, std::string>> out;
    if (patch.empty())
        return out;
    for (const PBlock& b : parse_patch(patch, wid)) {
        if (b.is_rename && b.has_old && b.has_new && !b.rename_from.empty() &&
            !b.rename_to.empty())
            out.push_back({b.rename_from, b.rename_to});
    }
    return out;
}

// ---- frozen-mtime attribute editing ----
//
// The `frozen-mtime <ts>` extended header lives in the same region of a
// block as the old/new mode lines: immediately after the `diff --git` line,
// before any mode lines. Blocks are edited byte-exactly (hunk bodies and
// binary payloads are never re-encoded), so set/unfreeze only ever touch
// the header line itself.

// Rebuild one raw block with its frozen-mtime header set (set=true, value
// `ts`) or removed (set=false). The header is placed right after the block's
// first line (the `diff --git` line) and replaces any existing one.
std::string block_set_frozen(const std::string& raw, bool set, uint64_t ts)
{
    std::vector<std::string> lines = split_raw(raw);
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i == 0) {
            out += lines[0];
            out += "\n";
            if (set)
                out += kFrozenMtimeHeader + std::to_string(ts) + "\n";
            continue;
        }
        if (no_cr(lines[i]).compare(0, kFrozenMtimeHeaderLen,
                                    kFrozenMtimeHeader) == 0)
            continue; // an existing header: replaced (or dropped) above
        out += lines[i];
        out += "\n";
    }
    return out;
}

// True when `raw` is a bare `diff --git` header line with nothing else: the
// husk an attribute-only block leaves behind once its frozen-mtime header is
// removed. Such a block describes no change at all, so it is dropped rather
// than kept.
bool block_is_husk(const std::string& raw)
{
    std::vector<std::string> lines = split_raw(raw);
    if (lines.size() != 1)
        return false;
    std::string t = no_cr(lines[0]);
    return starts_with(t, "diff --git ") || starts_with(t, "diff --cc ") ||
           starts_with(t, "diff --combined ");
}

// The text of an attribute-only block: `diff --git a/X b/X` plus the
// frozen-mtime header, with no ---/+++/hunks (the analogue of a mode-only
// block, which carries no hunks either).
std::string frozen_attribute_block(const std::string& wid, const std::string& rel,
                                   uint64_t ts)
{
    std::string a = quote_git_path("a/" + wid + "/" + rel);
    std::string b = quote_git_path("b/" + wid + "/" + rel);
    return "diff --git " + a + " " + b + "\n" + kFrozenMtimeHeader +
           std::to_string(ts) + "\n";
}

} // namespace

std::map<std::string, uint64_t> vcs_frozen_mtimes(const std::string& patch,
                                                  const std::string& wid)
{
    std::map<std::string, uint64_t> out;
    if (patch.empty())
        return out;
    for (const PBlock& b : parse_patch(patch, wid)) {
        if (b.frozen_mtime == 0)
            continue;
        // A frozen attribute belongs to the block's live path: the rename
        // destination when both sides exist, else whichever side is named.
        std::string key = !b.new_rel.empty() ? b.new_rel : b.old_rel;
        if (key.empty())
            continue;
        out[key] = b.frozen_mtime;
    }
    return out;
}

std::string vcs_set_frozen_mtimes(const std::string& patch, const std::string& wid,
                                  const std::map<std::string, uint64_t>& frozen)
{
    if (patch.empty() && frozen.empty())
        return patch;
    std::vector<PBlock> blocks = parse_patch(patch, wid);
    std::string out;
    std::set<std::string> covered;
    for (const PBlock& b : blocks) {
        std::string key = !b.new_rel.empty() ? b.new_rel : b.old_rel;
        auto it = key.empty() ? frozen.end() : frozen.find(key);
        // A deletion never carries the header: a frozen mtime needs a live
        // file, and the attribute dies with the file (matching emit_block,
        // which skips frozen paths on delete blocks).
        bool keep_header = it != frozen.end() && !b.is_deleted;
        std::string raw = keep_header ? block_set_frozen(b.raw, true, it->second)
                                      : block_set_frozen(b.raw, false, 0);
        if (keep_header)
            covered.insert(key);
        if (block_is_husk(raw))
            continue; // attribute-only block whose freeze was removed
        out += raw;
    }
    // Frozen paths the patch does not mention yet (unchanged tarball files):
    // new attribute-only blocks so the attribute has a home in the patch.
    for (const auto& kv : frozen) {
        if (covered.count(kv.first))
            continue;
        out += frozen_attribute_block(wid, kv.first, kv.second);
    }
    return out;
}

namespace {

// ---- Hunk matching/application with fuzz ----

// Check hunk body lines against file lines at pos with fuzz f (ignores up to
// f leading/trailing context lines; '-' lines always checked).
bool hunk_matches_at(const std::vector<std::string>& file, size_t pos,
                     const PHunk& h, int fuzz, bool reverse)
{
    // Collect checkable (file-side) lines with their file offsets.
    struct Item {
        std::string text;
        long file_off; // offset from pos
    };
    std::vector<Item> items;
    long off = 0;
    for (auto& ln : h.lines) {
        char op = ln.op;
        if (reverse && (op == '-' || op == '+'))
            op = (op == '-') ? '+' : '-';
        if (op == ' ' || op == '-') {
            items.push_back({ln.text, off});
            ++off;
        } else if (op == '+') {
            // inserted: no file line consumed
        }
    }
    // Fuzz skips leading/trailing *context* items (not '-' items).
    size_t lo = 0, hi = items.size();
    // Map items back to hunk lines to know which are context.
    std::vector<char> kinds;
    for (auto& ln : h.lines) {
        char op = ln.op;
        if (reverse && (op == '-' || op == '+'))
            op = (op == '-') ? '+' : '-';
        if (op == ' ' || op == '-')
            kinds.push_back(op);
    }
    int skip_lo = fuzz, skip_hi = fuzz;
    while (lo < hi && skip_lo > 0 && kinds[lo] == ' ') {
        ++lo;
        --skip_lo;
    }
    while (hi > lo && skip_hi > 0 && kinds[hi - 1] == ' ') {
        --hi;
        --skip_hi;
    }
    for (size_t i = lo; i < hi; ++i) {
        size_t fp = pos + (size_t)items[i].file_off;
        if (fp >= file.size())
            return false;
        if (file[fp] != items[i].text)
            return false;
    }
    return true;
}

// Body counts (header counts ignored for tolerance).
void hunk_body_counts(const PHunk& h, long* rem, long* add, bool reverse)
{
    long r = 0, a = 0;
    for (auto& ln : h.lines) {
        char op = ln.op;
        if (reverse && (op == '-' || op == '+'))
            op = (op == '-') ? '+' : '-';
        if (op == '-')
            ++r;
        else if (op == '+')
            ++a;
    }
    *rem = r;
    *add = a;
}

long hunk_expected(const PHunk& h, bool reverse)
{
    // 0-based expected file index for the hunk start (before offset).
    if (!reverse)
        return h.old_count == 0 ? h.old_start : h.old_start - 1;
    return h.new_count == 0 ? h.new_start : h.new_start - 1;
}

// Find best position for hunk: minimal fuzz, then minimal |pos-expected|,
// ties prefer earlier. Searches the whole file. Returns -1 if none.
long find_hunk_pos(const std::vector<std::string>& file, const PHunk& h,
                   long expected, bool reverse)
{
    for (int f = 0; f <= 2; ++f) {
        long best = -1;
        long best_dist = -1;
        // Clamp expected into range.
        long lo = 0, hi = (long)file.size();
        for (long p = lo; p <= hi; ++p) {
            if (!hunk_matches_at(file, (size_t)p, h, f, reverse))
                continue;
            long dist = p >= expected ? p - expected : expected - p;
            if (best < 0 || dist < best_dist ||
                (dist == best_dist && p < best)) {
                best = p;
                best_dist = dist;
            }
            if (dist == 0)
                break; // cannot beat exact
        }
        if (best >= 0)
            return best;
    }
    return -1;
}

bool hunks_match_all(const std::vector<std::string>& file,
                     const std::vector<PHunk>& hunks, bool reverse)
{
    long offset = 0;
    for (auto& h : hunks) {
        long exp = hunk_expected(h, reverse) + offset;
        if (exp < 0)
            exp = 0;
        if (exp > (long)file.size())
            exp = (long)file.size();
        long pos = find_hunk_pos(file, h, exp, reverse);
        if (pos < 0)
            return false;
        long rem = 0, add = 0;
        hunk_body_counts(h, &rem, &add, reverse);
        offset += (reverse ? (rem - add) : (add - rem));
    }
    return true;
}

// Apply hunks to file lines. Returns false if any hunk fails to match.
// Sets ends_nl per new-file markers / preservation rules.
bool apply_hunks(const FileLines& file, const std::vector<PHunk>& hunks,
                 FileLines* result, bool new_no_nl_file)
{
    std::vector<std::string> cur = file.lines;
    long offset = 0;
    bool last_touches_eof = false;
    for (size_t hi = 0; hi < hunks.size(); ++hi) {
        const PHunk& h = hunks[hi];
        long exp = hunk_expected(h, false) + offset;
        if (exp < 0)
            exp = 0;
        if (exp > (long)cur.size())
            exp = (long)cur.size();
        long pos = find_hunk_pos(cur, h, exp, false);
        if (pos < 0)
            return false;
        std::vector<std::string> next;
        next.reserve(cur.size() + 8);
        for (long i = 0; i < pos; ++i)
            next.push_back(cur[(size_t)i]);
        long fp = pos;
        for (auto& ln : h.lines) {
            if (ln.op == ' ') {
                if (fp < (long)cur.size())
                    next.push_back(cur[(size_t)fp]);
                else
                    next.push_back(ln.text);
                ++fp;
            } else if (ln.op == '-') {
                ++fp; // drop
            } else if (ln.op == '+') {
                next.push_back(ln.text);
            }
        }
        for (size_t i = (size_t)fp; i < cur.size(); ++i)
            next.push_back(cur[i]);
        long rem = 0, add = 0;
        hunk_body_counts(h, &rem, &add, false);
        offset += (add - rem);
        cur.swap(next);
        if (hi + 1 == hunks.size()) {
            // Last hunk: does it touch new EOF?
            long new_end = pos;
            for (auto& ln : h.lines) {
                if (ln.op == ' ' || ln.op == '+')
                    ++new_end;
            }
            last_touches_eof = (new_end == (long)cur.size());
        }
    }
    result->lines = cur;
    if (hunks.empty()) {
        result->ends_nl = file.ends_nl;
    } else if (last_touches_eof) {
        result->ends_nl = !hunks.back().new_no_nl;
    } else {
        // If some hunk carried a new marker but did not touch EOF (should
        // not happen in well-formed diffs), honor it conservatively.
        result->ends_nl = new_no_nl_file ? false : file.ends_nl;
    }
    return true;
}

} // namespace

namespace {

enum class BlkStatus { Applied, Already, Failed };

bool is_exec_mode(const std::string& m)
{
    return m == "100755";
}

// The tree-relative spelling of an on-disk member path. `full` is always
// join_path(root, rel) with a validated rel at every call site, so a missing
// prefix means a caller bookkeeping bug: die loudly rather than resolve
// against a bogus base.
std::string rel_under(const std::string& root, const std::string& full)
{
    if (full == root)
        return "";
    if (starts_with(full, root + "/"))
        return full.substr(root.size() + 1);
    die("internal error: '" + full + "' is not inside '" + root + "'");
}

void check_patch_link_target(const std::string& root, const std::string& member,
                             const std::string& target)
{
    // Same validation as tar unpack (tree.cc check_tar_link_target): reject
    // absolute targets and targets that resolve outside the tree so a
    // malicious patch cannot create symlinks escaping the workdir. In-tree
    // ".." spellings (e.g. "sub/link -> ../file") resolve back inside the
    // tree and are fine. `member` is an on-disk path under `root`; the link
    // is named (and its target resolved) tree-relatively.
    if (target.empty())
        return;
    std::string rel = rel_under(root, member);
    if (target[0] == '/')
        die("patch creates symlink '" + rel + "' with absolute target '" +
            target + "'; refusing (symlink escape)");
    std::string resolved;
    if (resolve_link_target(dirname_of(rel), target, &resolved) ==
        LinkResolve::Escapes)
        die("patch creates symlink '" + rel + "' with target '" + target +
            "' (resolves to '" + resolved +
            "'); refusing (link target escapes the tree)");
}

// Binary-safe content probe: lstat first so symlinks compare by target
// string and regular files by raw bytes (NULs included). Directories and
// special files report Other (callers fail those cleanly); missing paths
// report Missing. Never dies on binary bytes.
enum class RawKind { Missing, Link, Regular, Other };

struct RawContent {
    RawKind kind = RawKind::Missing;
    std::string bytes; // link target for links, raw bytes for regular files
};

RawContent read_raw_content(const std::string& full)
{
    RawContent rc;
    struct stat st;
    if (lstat(full.c_str(), &st) != 0)
        return rc;
    if (S_ISLNK(st.st_mode)) {
        rc.kind = RawKind::Link;
        rc.bytes = read_link_target(full);
        return rc;
    }
    if (S_ISREG(st.st_mode)) {
        rc.kind = RawKind::Regular;
        rc.bytes = read_file_bytes(full);
        return rc;
    }
    rc.kind = RawKind::Other;
    return rc;
}

// Write raw bytes as a regular file (binary-safe) or a symlink when
// mode == "120000" (target = bytes, validated like every other link the
// patch creates). Regular writes set an explicit 0755/0644 when mode names
// one, else leave the fresh 0666&~umask bits for the caller to restore.
void write_raw_target(const std::string& root, const std::string& full,
                      const std::string& bytes,
                      const std::string& mode /* "" if keep */)
{
    if (mode == "120000") {
        check_patch_link_target(root, full, bytes);
        make_dirs(dirname_of(full));
        unlink(full.c_str());
        if (symlink(bytes.c_str(), full.c_str()) != 0)
            die("cannot create symlink '" + full + "': " + strerror(errno));
        return;
    }
    make_dirs(dirname_of(full));
    write_file_bytes(full, bytes);
    if (!mode.empty()) {
        mode_t m = is_exec_mode(mode) ? 0755 : 0644;
        if (chmod(full.c_str(), m) != 0)
            die("cannot set permissions on '" + full + "': " + strerror(errno));
    }
}

// True when `full` exists as a NUL-bearing regular file. Text patch paths
// probe this before read_target_lines (which dies on binary bytes): a text
// block meeting a binary file is a content mismatch, reported as a clean
// per-file failure (merge conflict) rather than a hard error. Symlinks,
// missing paths, and non-regular files are never "binary" here.
bool path_is_binary_file(const std::string& full)
{
    struct stat lst;
    if (lstat(full.c_str(), &lst) != 0 || !S_ISREG(lst.st_mode))
        return false;
    int fd = open(full.c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    char buf[65536];
    bool found = false;
    for (;;) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (r == 0)
            break;
        if (memchr(buf, 0, (size_t)r) != nullptr) {
            found = true;
            break;
        }
    }
    close(fd);
    return found;
}

void write_target(const std::string& root, const std::string& full,
                  const FileLines& fl,
                  const std::string& mode /* "" if keep */)
{
    if (mode == "120000") {
        // Symlink: content is the single target line. Validate exactly like
        // tar unpack so patch symlinks cannot escape the tree.
        std::string target = fl.lines.empty() ? "" : fl.lines[0];
        check_patch_link_target(root, full, target);
        make_dirs(dirname_of(full));
        unlink(full.c_str());
        if (symlink(target.c_str(), full.c_str()) != 0)
            die("cannot create symlink '" + full + "': " + strerror(errno));
        return;
    }
    make_dirs(dirname_of(full));
    write_file_bytes(full, join_content(fl.lines, fl.ends_nl));
    if (!mode.empty()) {
        mode_t m = is_exec_mode(mode) ? 0755 : 0644;
        if (chmod(full.c_str(), m) != 0)
            die("cannot set permissions on '" + full + "': " + strerror(errno));
    }
}

FileLines read_target_lines(const std::string& full, bool* is_link,
                            std::string* link_target)
{
    struct stat st;
    if (lstat(full.c_str(), &st) != 0) {
        *is_link = false;
        return FileLines();
    }
    if (S_ISLNK(st.st_mode)) {
        *is_link = true;
        std::string target = read_link_target(full);
        if (link_target)
            *link_target = target;
        FileLines fl;
        fl.lines.push_back(target);
        fl.ends_nl = false;
        return fl;
    }
    *is_link = false;
    std::string data = read_file_bytes(full);
    if (data.find('\0') != std::string::npos)
        die("file '" + full + "' is binary; binary files are not supported");
    return split_content(data);
}

// Expected new content of a new-file block (hunks applied to empty).
FileLines new_file_content(const PBlock& blk)
{
    FileLines empty;
    empty.lines.clear();
    empty.ends_nl = true;
    FileLines res;
    if (blk.hunks.empty())
        return empty;
    if (!apply_hunks(empty, blk.hunks, &res, blk.new_no_nl_file))
        res = empty; // unapplyable here; caller treats as failure later
    return res;
}

// Enforce explicit mode on an already-applied file (no-op when the patch
// carries no mode change or targets a symlink).
void enforce_already_mode(const std::string& path, const PBlock& blk)
{
    if (blk.new_mode.empty() || blk.new_mode == "120000")
        return;
    mode_t m = is_exec_mode(blk.new_mode) ? 0755 : 0644;
    if (chmod(path.c_str(), m) != 0)
        die("cannot set permissions on '" + path + "': " + strerror(errno));
}

// Does the patch's last hunk touch new EOF (header heuristic)? Used to decide
// whether trailing-newline enforcement applies. new_no_nl markers only ever
// appear on EOF-touching hunks, so a patch declaring missing newline always
// touches EOF regardless of shifted hunk offsets.
bool touches_new_eof(const FileLines& cur, const PBlock& blk)
{
    if (blk.hunks.empty())
        return false;
    if (blk.new_no_nl_file)
        return true;
    if (cur.lines.empty())
        return true;
    const PHunk& h = blk.hunks.back();
    long n = (long)cur.lines.size();
    long new_end = h.new_start + h.new_count - 1;
    // Tolerance for shifted hunk offsets (fuzz): within a few lines of EOF
    // still counts as touching.
    return new_end >= n - 3;
}

// Enforce trailing-newline state on an already-applied file when the patch
// touches EOF. Rewrites the file with corrected ends_nl, preserving mode.
void enforce_already_newline(const std::string& path, FileLines& cur,
                             const PBlock& blk, bool is_link)
{
    if (is_link || blk.hunks.empty() || cur.lines.empty())
        return;
    if (!touches_new_eof(cur, blk))
        return;
    bool expected = !blk.hunks.back().new_no_nl;
    // File-level flag agrees with the last hunk for well-formed patches;
    // prefer it when it declares missing newline (shifted-offset safety).
    if (blk.new_no_nl_file)
        expected = false;
    if (cur.ends_nl == expected)
        return;
    // Remember the full mode so the rename-based rewrite neither drops
    // +x nor widens restrictive modes (0700->0755) nor leaks rw (0640->0644).
    // An explicit patch mode still wins below; otherwise the exact bits stay.
    mode_t prev_mode = 0;
    bool have_prev_mode = false;
    struct stat sst;
    if (stat(path.c_str(), &sst) == 0 && S_ISREG(sst.st_mode)) {
        prev_mode = (mode_t)(sst.st_mode & 07777);
        have_prev_mode = true;
    }
    cur.ends_nl = expected;
    write_file_bytes(path, join_content(cur.lines, cur.ends_nl));
    if (!blk.new_mode.empty() && blk.new_mode != "120000") {
        mode_t m = is_exec_mode(blk.new_mode) ? 0755 : 0644;
        if (chmod(path.c_str(), m) != 0)
            die("cannot set permissions on '" + path + "': " + strerror(errno));
    } else if (have_prev_mode) {
        struct stat dst_st;
        if (lstat(path.c_str(), &dst_st) == 0 && S_ISREG(dst_st.st_mode) &&
            (mode_t)(dst_st.st_mode & 07777) != prev_mode) {
            if (chmod(path.c_str(), prev_mode) != 0)
                die("cannot set permissions on '" + path + "': " +
                    strerror(errno));
        }
    }
}

void ensure_already_state(const std::string& path, FileLines& cur,
                          const PBlock& blk, bool is_link)
{
    enforce_already_newline(path, cur, blk, is_link);
    enforce_already_mode(path, blk);
}

// Classify a path for patch application. Only symlinks and regular files
// carry readable file content. Directories, FIFOs, sockets and device nodes
// must report per-file failure (so merges conflict) rather than dying in
// read_file_bytes (EISDIR) or hanging forever (opening a FIFO blocks).
enum class PathKind { Missing, Link, Regular, Other };

PathKind path_kind(const std::string& full)
{
    struct stat st;
    if (lstat(full.c_str(), &st) != 0)
        return PathKind::Missing;
    if (S_ISLNK(st.st_mode))
        return PathKind::Link;
    if (S_ISREG(st.st_mode))
        return PathKind::Regular;
    return PathKind::Other;
}

// True when no existing ancestor of `full` strictly below `treedir` is
// anything but a real directory: an lstat (never stat, so nothing is
// followed) walk from the parent chain up to `treedir`. A planted symlink
// ancestor (e.g. "link -> /tmp" inside the target tree with a patch
// touching "link/evil") fails this check, as does a file/special node in
// an ancestor slot. Callers touching treedir/<rel> for reads, unlinks,
// moves, chmods, or conflict-marker writes must check this first and
// report per-file failure/conflict on false instead of following the link
// outside the tree. Missing ancestors are fine (they will be created).
bool ancestors_are_dirs(const std::string& treedir, const std::string& full)
{
    std::string d = dirname_of(full);
    while (d.size() > treedir.size() && starts_with(d, treedir + "/")) {
        struct stat st;
        if (lstat(d.c_str(), &st) == 0 && !S_ISDIR(st.st_mode))
            return false;
        d = dirname_of(d);
    }
    return true;
}

// True when every existing ancestor of `full` (strictly below `treedir`) is
// a directory, so creating `full` cannot fail with ENOTDIR inside make_dirs
// (which would die instead of reporting per-file failure). Also confines
// `full` to `treedir` lexically: even if a ".." rel ever slipped past the
// parse-time check_patch_member_path, the normalized target outside the
// tree is refused here (callers turn it into a per-file failure/conflict,
// never an escape).
//
// Note the lstat (not stat): a symlink ancestor is NOT a directory, so it
// is rejected. This matters because make_dirs() and write_file_bytes()
// both follow symlinks: with an attacker- or user-planted "link -> /tmp"
// inside the target tree, creating "treedir/link/evil" without this check
// would plant "evil" in /tmp. Every creation path under a patch-controlled
// rel must pass through here (directly or via confined_for_write below)
// and fail as a per-file conflict instead of following the link.
//
// Reads, unlinks, moves, and chmods need the same protection even though
// they do not create anything (lstat/read/unlink/chmod all follow
// through-link ancestors): they gate on ancestors_are_dirs above (same
// walk) before touching anything.
bool creatable_under(const std::string& treedir, const std::string& full)
{
    std::string nfull = normalize_lexical(full);
    std::string ntree = normalize_lexical(treedir);
    if (nfull != ntree && !starts_with(nfull, ntree + "/"))
        return false;
    return ancestors_are_dirs(treedir, full);
}

// True when `full` may be (over)written as a regular file under `treedir`:
// confined under `treedir` with no symlink ancestors (see
// creatable_under), and not itself a symlink (a regular write would
// follow it to wherever it points). Conflict-marker writers use this:
// they run after a block already failed, on user-controlled trees (e.g.
// `projeny patch <dir>`), where a planted "link -> /tmp" plus a failing
// block for "link/evil" would otherwise escape the tree through
// make_dirs()/write_file_bytes(), which both follow symlinks. A false
// return means "leave the file for the user"; the conflict itself is
// already recorded by the caller.
bool confined_for_write(const std::string& treedir, const std::string& full)
{
    if (!creatable_under(treedir, full))
        return false;
    struct stat st;
    if (lstat(full.c_str(), &st) == 0 && S_ISLNK(st.st_mode))
        return false;
    return true;
}

BlkStatus apply_block(const std::string& treedir, const PBlock& blk,
                      const std::string& wid)
{
    // Resolve target paths.
    std::string old_full, new_full;
    bool have_old_path = false, have_new_path = false;
    if (blk.is_rename) {
        if (blk.rename_from.empty() || blk.rename_to.empty())
            return BlkStatus::Failed;
        old_full = join_path(treedir, blk.rename_from);
        new_full = join_path(treedir, blk.rename_to);
        have_old_path = have_new_path = true;
    } else if (blk.has_old && blk.has_new) {
        if (blk.old_rel == blk.new_rel) {
            old_full = new_full = join_path(treedir, blk.old_rel);
            have_old_path = have_new_path = true;
        } else if (!blk.old_rel.empty() && !blk.new_rel.empty()) {
            // Non-rename with differing sides: treat as rename.
            old_full = join_path(treedir, blk.old_rel);
            new_full = join_path(treedir, blk.new_rel);
            have_old_path = have_new_path = true;
        } else {
            return BlkStatus::Failed;
        }
    } else if (blk.has_old && !blk.has_new) {
        old_full = join_path(treedir, blk.old_rel);
        have_old_path = true;
    } else if (!blk.has_old && blk.has_new) {
        new_full = join_path(treedir, blk.new_rel);
        have_new_path = true;
    } else {
        // No ---/+++: mode-only or empty-file block; path from diff sides.
        // The wid is stripped here with the block's own wid (parse stores
        // raw sides; the caller threads it through).
        std::string rel;
        if (!blk.git_a.empty() && blk.git_a != "/dev/null") {
            bool miss = false;
            rel = label_to_rel(blk.git_a, wid, &miss);
            if (miss)
                return BlkStatus::Failed;
        }
        if (rel.empty())
            return BlkStatus::Failed;
        old_full = new_full = join_path(treedir, rel);
        have_old_path = have_new_path = true;
    }

    // A binary block carries a `GIT binary patch` payload instead of text
    // hunks, so a rename whose content changed has empty hunks but real
    // bytes to write. Only payload-free renames (pure rename, or mode-only
    // rename of a binary — the emitter omits the payload when the bytes are
    // identical) may take the move-only fast path below; a payload-bearing
    // rename must fall through to the modify path, which verifies the old
    // bytes at the source and writes the new bytes to the destination.
    bool pure_rename = blk.is_rename && blk.hunks.empty() && !blk.is_new &&
                       !blk.is_deleted && !blk.binary_has_payload;
    if (pure_rename) {
        if (blk.is_combined)
            return BlkStatus::Failed;
        // Through-link safety: path_kind/read_target_lines below follow
        // ancestors, and move_path/chmod would mutate through them. A
        // "link -> /tmp" ancestor on either end fails as a conflict
        // without touching anything outside the tree.
        if (!ancestors_are_dirs(treedir, old_full) ||
            !ancestors_are_dirs(treedir, new_full))
            return BlkStatus::Failed;
        PathKind old_k = path_kind(old_full);
        PathKind new_k = path_kind(new_full);
        // Directories/special files at either end are not renameable
        // content: fail cleanly (merge conflicts) instead of dying while
        // reading them.
        if (old_k == PathKind::Other || new_k == PathKind::Other)
            return BlkStatus::Failed;
        bool old_e = old_k != PathKind::Missing;
        bool new_e = new_k != PathKind::Missing;
        if (!old_e && !new_e)
            return BlkStatus::Failed;
        if (!old_e && new_e) {
            // Already renamed: enforce carried mode, then report idempotent.
            // Gate the chmod like any other: through a symlink (ancestor
            // or the file itself) it would touch outside the tree.
            if (!blk.new_mode.empty() && blk.new_mode != "120000") {
                if (!confined_for_write(treedir, new_full))
                    return BlkStatus::Failed;
                mode_t m = is_exec_mode(blk.new_mode) ? 0755 : 0644;
                if (chmod(new_full.c_str(), m) != 0)
                    die("cannot set permissions on '" + new_full +
                        "': " + strerror(errno));
            }
            return BlkStatus::Already; // already renamed
        }
        if (old_e && new_e) {
            // Destination in the way: succeed idempotently only when it
            // holds the same content (same bytes and symlink-ness) as the
            // source; otherwise fail without overwriting. Binary-safe: raw
            // bytes compare (pure renames of binaries carry no payload, so
            // the bytes themselves are the lineage).
            RawContent ro = read_raw_content(old_full);
            RawContent rn = read_raw_content(new_full);
            bool same = ro.kind == rn.kind && ro.kind != RawKind::Other &&
                        ro.kind != RawKind::Missing && ro.bytes == rn.bytes;
            if (same) {
                // Same gate as above: never chmod through a link.
                if (!blk.new_mode.empty() && blk.new_mode != "120000" &&
                    !confined_for_write(treedir, new_full))
                    return BlkStatus::Failed;
                enforce_already_mode(new_full, blk);
                return BlkStatus::Already;
            }
            return BlkStatus::Failed; // destination differs: conflict
        }
        // old_e && !new_e: normal rename. No expected content is stored for
        // pure renames, so lineage is the source's current content itself.
        if (!creatable_under(treedir, new_full))
            return BlkStatus::Failed;
        // The post-move chmod would follow the file itself when the source
        // is a symlink, so refuse that combination before moving (moving
        // first and failing after would leave the tree half-mutated).
        if (!blk.new_mode.empty() && blk.new_mode != "120000" &&
            !confined_for_write(treedir, old_full))
            return BlkStatus::Failed;
        make_dirs(dirname_of(new_full));
        move_path(old_full, new_full);
        // Apply mode change if the rename carries one.
        if (!blk.new_mode.empty() && blk.new_mode != "120000") {
            mode_t m = is_exec_mode(blk.new_mode) ? 0755 : 0644;
            if (chmod(new_full.c_str(), m) != 0)
                die("cannot set permissions on '" + new_full +
                    "': " + strerror(errno));
        }
        return BlkStatus::Applied;
    }

    if (blk.is_combined)
        return BlkStatus::Failed;

    if (blk.is_new) {
        // Through-link safety first (see below).
        if (!ancestors_are_dirs(treedir, new_full))
            return BlkStatus::Failed;
        if (blk.is_binary) {
            // Binary create: the payload's new bytes are the expected
            // content. Without a payload (foreign stanza) there is nothing
            // verifiable to create: fail cleanly (merge conflict).
            if (!blk.binary_has_payload)
                return BlkStatus::Failed;
            PathKind nk = path_kind(new_full);
            if (nk == PathKind::Other)
                return BlkStatus::Failed;
            if (nk != PathKind::Missing) {
                RawContent cur = read_raw_content(new_full);
                bool same_link =
                    (cur.kind == RawKind::Link) == (blk.new_mode == "120000");
                if (same_link && cur.kind != RawKind::Other &&
                    cur.bytes == blk.bin_new) {
                    if (!blk.new_mode.empty() && blk.new_mode != "120000" &&
                        !confined_for_write(treedir, new_full))
                        return BlkStatus::Failed;
                    enforce_already_mode(new_full, blk);
                    return BlkStatus::Already;
                }
                return BlkStatus::Failed;
            }
            if (!creatable_under(treedir, new_full))
                return BlkStatus::Failed;
            write_raw_target(treedir, new_full, blk.bin_new, blk.new_mode);
            return BlkStatus::Applied;
        }
        // Create. Already-applied when the file exists with expected content.
        // Even then, enforce the expected mode so exec-bit drift is repaired.
        // A directory/special file in the way is a clean failure (merge
        // conflicts), never a die (EISDIR) or a hang (FIFO open).
        // Through-link safety first: the existence probe and the content
        // read below follow ancestors, so a "link -> /tmp" ancestor fails
        // here as a conflict without reading anything outside the tree.
        if (!ancestors_are_dirs(treedir, new_full))
            return BlkStatus::Failed;
        PathKind nk = path_kind(new_full);
        if (nk == PathKind::Other)
            return BlkStatus::Failed;
        if (nk != PathKind::Missing) {
            if (!blk.is_binary && path_is_binary_file(new_full))
                return BlkStatus::Failed; // text create vs binary file
            FileLines exp = new_file_content(blk);
            std::string want_mode = blk.new_mode;
            bool is_link = false;
            std::string tgt;
            FileLines cur = read_target_lines(new_full, &is_link, &tgt);
            // Compare bytes (and symlink-ness). Mode differences alone do not
            // count as already-applied; fall through to failure so callers
            // can report precisely... but a mode-only drift with identical
            // content is benign: treat content-equal as already applied
            // (and repair the mode below).
            bool same_link = is_link == (want_mode == "120000");
            if (same_link &&
                join_content(cur.lines, cur.ends_nl) ==
                    join_content(exp.lines, exp.ends_nl)) {
                // Never chmod through a link on the Already path either.
                if (!blk.new_mode.empty() && blk.new_mode != "120000" &&
                    !confined_for_write(treedir, new_full))
                    return BlkStatus::Failed;
                enforce_already_mode(new_full, blk);
                return BlkStatus::Already;
            }
            return BlkStatus::Failed;
        }
        FileLines exp = new_file_content(blk);
        if (!blk.hunks.empty()) {
            FileLines empty;
            if (!apply_hunks(empty, blk.hunks, &exp, blk.new_no_nl_file))
                return BlkStatus::Failed;
        } else {
            exp.lines.clear();
            exp.ends_nl = true;
        }
        if (!creatable_under(treedir, new_full))
            return BlkStatus::Failed;
        write_target(treedir, new_full, exp, blk.new_mode);
        return BlkStatus::Applied;
    }

    if (blk.is_deleted) {
        // Through-link safety: the probe, the content read, and the
        // unlink/remove_recursive below all follow ancestors. A "link ->
        // /tmp" ancestor fails here as a conflict without touching
        // anything outside the tree (unlinking through it would delete
        // outside files). Unlinking the link itself remains allowed: the
        // gate only inspects ancestors, and remove_recursive unlinks a
        // symlink target itself rather than following it.
        if (!ancestors_are_dirs(treedir, old_full))
            return BlkStatus::Failed;
        if (blk.is_binary) {
            // Binary delete: the payload's old bytes must match the file, so
            // deleting a file the user changed becomes a conflict instead of
            // data loss. Without a payload there is nothing to verify
            // against: fail cleanly unless the file is already gone.
            PathKind ok = path_kind(old_full);
            if (ok == PathKind::Missing)
                return BlkStatus::Already;
            if (ok == PathKind::Other)
                return BlkStatus::Failed;
            if (!blk.binary_has_payload)
                return BlkStatus::Failed;
            RawContent cur = read_raw_content(old_full);
            bool same_link =
                (cur.kind == RawKind::Link) == (blk.old_mode == "120000");
            if (!same_link || cur.kind == RawKind::Other ||
                cur.bytes != blk.bin_old)
                return BlkStatus::Failed;
            if (!remove_recursive(old_full))
                return BlkStatus::Failed;
            return BlkStatus::Applied;
        }
        PathKind ok = path_kind(old_full);
        if (ok == PathKind::Missing)
            return BlkStatus::Already;
        if (ok == PathKind::Other)
            return BlkStatus::Failed; // dir/special in the way: conflict
        if (!blk.is_binary && path_is_binary_file(old_full))
            return BlkStatus::Failed; // text delete vs binary file
        bool is_link = false;
        FileLines cur = read_target_lines(old_full, &is_link, nullptr);
        if (blk.hunks.empty()) {
            if (unlink(old_full.c_str()) != 0 && errno != ENOENT)
                return BlkStatus::Failed;
            if (path_exists(old_full))
                return BlkStatus::Failed; // e.g. became a dir: conflict
            return BlkStatus::Applied;
        }
        FileLines dummy;
        if (!apply_hunks(cur, blk.hunks, &dummy, blk.new_no_nl_file)) {
            // Maybe already deleted except leftover? File exists and hunks
            // do not match: fail. (Reverse-delete check: empty file with no
            // hunks handled above.)
            return BlkStatus::Failed;
        }
        if (!remove_recursive(old_full))
            return BlkStatus::Failed;
        return BlkStatus::Applied;
    }

    // Modify (possibly with rename and/or mode change).
    std::string src_full = have_old_path ? old_full : new_full;
    std::string dst_full = have_new_path ? new_full : old_full;
    // Through-link safety for every read/mutation below (path_kind,
    // read_target_lines, move_path, chmod, write_target, and the
    // Already-path enforce helpers all follow ancestors): a "link -> /tmp"
    // ancestor on either end fails as a conflict without touching anything
    // outside the tree.
    if (!ancestors_are_dirs(treedir, src_full) ||
        !ancestors_are_dirs(treedir, dst_full))
        return BlkStatus::Failed;
    if (blk.is_binary) {
        // Binary modify (possibly with rename and/or mode change, possibly a
        // symlink<->file typechange): the payload's old bytes must match the
        // source. Without a payload there is nothing to verify: fail cleanly.
        if (!blk.binary_has_payload)
            return BlkStatus::Failed;
        PathKind src_k = path_kind(src_full);
        if (src_k == PathKind::Other)
            return BlkStatus::Failed;
        if (src_k == PathKind::Missing) {
            // Already applied? The destination holds the new bytes.
            if (path_kind(dst_full) == PathKind::Other)
                return BlkStatus::Failed;
            if (path_exists(dst_full)) {
                RawContent dc = read_raw_content(dst_full);
                bool same_link =
                    (dc.kind == RawKind::Link) == (blk.new_mode == "120000");
                if (same_link && dc.kind != RawKind::Other &&
                    dc.bytes == blk.bin_new) {
                    if (!blk.new_mode.empty() && blk.new_mode != "120000" &&
                        !confined_for_write(treedir, dst_full))
                        return BlkStatus::Failed;
                    enforce_already_mode(dst_full, blk);
                    return BlkStatus::Already;
                }
            }
            return BlkStatus::Failed;
        }
        RawContent sc = read_raw_content(src_full);
        if (sc.kind == RawKind::Other)
            return BlkStatus::Failed;
        bool same_link_old =
            (sc.kind == RawKind::Link) == (blk.old_mode == "120000");
        // Old-side kind is only known when the patch carries mode lines; a
        // bare binary modify (no mode lines) accepts either kind as long as
        // the bytes match.
        bool old_mode_known = !blk.old_mode.empty() || !blk.new_mode.empty();
        if (sc.bytes != blk.bin_old ||
            (old_mode_known && !blk.old_mode.empty() && !same_link_old))
            {
            // Maybe already applied (source already holds the new bytes).
            bool same_link_new =
                (sc.kind == RawKind::Link) == (blk.new_mode == "120000");
            bool new_mode_known = !blk.new_mode.empty();
            if (sc.bytes == blk.bin_new &&
                (!new_mode_known || same_link_new)) {
                std::string dst =
                    blk.is_rename && src_full != dst_full ? dst_full : src_full;
                if (!confined_for_write(treedir, dst) &&
                    (!blk.new_mode.empty() && blk.new_mode != "120000"))
                    return BlkStatus::Failed;
                enforce_already_mode(dst, blk);
                return BlkStatus::Already;
            }
            return BlkStatus::Failed;
        }
        if (path_kind(dst_full) == PathKind::Other)
            return BlkStatus::Failed;
        if (!creatable_under(treedir, dst_full))
            return BlkStatus::Failed;
        std::string eff_mode = blk.new_mode;
        mode_t src_full_mode = 0;
        bool have_src_full_mode = false;
        if (eff_mode.empty()) {
            if (sc.kind == RawKind::Link) {
                eff_mode = "120000";
            } else {
                struct stat sst;
                if (stat(src_full.c_str(), &sst) == 0 &&
                    S_ISREG(sst.st_mode)) {
                    src_full_mode = (mode_t)(sst.st_mode & 07777);
                    have_src_full_mode = true;
                    if (sst.st_mode & 0111)
                        eff_mode = "100755";
                }
            }
        }
        if (eff_mode != "120000") {
            struct stat dst_st;
            if (lstat(dst_full.c_str(), &dst_st) == 0 &&
                S_ISLNK(dst_st.st_mode))
                return BlkStatus::Failed;
        }
        if (blk.is_rename || src_full != dst_full) {
            if (blk.is_rename && src_full != dst_full &&
                path_exists(dst_full)) {
                // Rename onto an existing path: idempotent only when it
                // already holds the new bytes of the expected kind (checked
                // above for src; check dst too when both exist).
                RawContent dc = read_raw_content(dst_full);
                if (dc.kind == RawKind::Other || dc.kind == RawKind::Missing ||
                    ((dc.kind == RawKind::Link) != (eff_mode == "120000")) ||
                    dc.bytes != blk.bin_new)
                    return BlkStatus::Failed;
                remove_recursive(src_full);
            } else {
                write_raw_target(treedir, dst_full, blk.bin_new, eff_mode);
                if (src_full != dst_full)
                    remove_recursive(src_full);
            }
        } else {
            write_raw_target(treedir, dst_full, blk.bin_new, eff_mode);
        }
        if (have_src_full_mode) {
            struct stat dst_st;
            if (lstat(dst_full.c_str(), &dst_st) == 0 &&
                S_ISREG(dst_st.st_mode) &&
                (mode_t)(dst_st.st_mode & 07777) != src_full_mode) {
                if (chmod(dst_full.c_str(), src_full_mode) != 0)
                    die("cannot set permissions on '" + dst_full +
                        "': " + strerror(errno));
            }
        }
        return BlkStatus::Applied;
    }
    PathKind src_k = path_kind(src_full);
    if (src_k == PathKind::Other)
        return BlkStatus::Failed; // dir/special: conflict, not die/hang
    if (src_k == PathKind::Missing) {
        // Already applied? Check the destination: reverse-match hunks there,
        // enforcing mode/newline even on the Already path.
        if (path_kind(dst_full) == PathKind::Other)
            return BlkStatus::Failed;
        // Reverse-match hunks against the destination (whether the block
        // renames or not — both shapes read the same way). A pure rename
        // (empty hunks) whose destination exists is already applied too.
        if (path_exists(dst_full)) {
            if (!blk.is_binary && path_is_binary_file(dst_full))
                return BlkStatus::Failed; // text block vs binary file
            bool is_link = false;
            FileLines cur = read_target_lines(dst_full, &is_link, nullptr);
            if ((!blk.hunks.empty() &&
                 hunks_match_all(cur.lines, blk.hunks, true)) ||
                (blk.is_rename && blk.hunks.empty())) {
                if (!confined_for_write(treedir, dst_full))
                    return BlkStatus::Failed;
                ensure_already_state(dst_full, cur, blk, is_link);
                return BlkStatus::Already;
            }
        }
        return BlkStatus::Failed;
    }
    if (blk.hunks.empty()) {
        // Mode-only change (no content hunks): no content read is needed, so
        // binaries (mode-only binary changes carry no payload) apply without
        // ever touching NUL bytes. The chmod below follows the
        // file itself when it is a symlink, so refuse that combination
        // before moving (moving first and failing after would leave the
        // tree half-mutated).
        if (!blk.new_mode.empty() && blk.new_mode != "120000" &&
            !confined_for_write(treedir, src_full))
            return BlkStatus::Failed;
        if (blk.is_rename) {
            if (path_exists(dst_full))
                return BlkStatus::Failed;
            if (!creatable_under(treedir, dst_full))
                return BlkStatus::Failed;
            make_dirs(dirname_of(dst_full));
            move_path(src_full, dst_full);
            src_full = dst_full;
        }
        if (!blk.new_mode.empty() && blk.new_mode != "120000") {
            mode_t m = is_exec_mode(blk.new_mode) ? 0755 : 0644;
            if (chmod(src_full.c_str(), m) != 0)
                die("cannot set permissions on '" + src_full +
                    "': " + strerror(errno));
        }
        return BlkStatus::Applied;
    }
    if (!blk.is_binary && path_is_binary_file(src_full))
        return BlkStatus::Failed; // text modify vs binary file
    bool is_link = false;
    FileLines cur = read_target_lines(src_full, &is_link, nullptr);
    // Already applied? Reverse-match first (like `apply -R --check`),
    // enforcing effective mode and trailing-newline state even when the
    // content already matches so retry/re-setup repairs drift.
    if (hunks_match_all(cur.lines, blk.hunks, true)) {
        // For renames the content lives at dst when src is gone; here src
        // exists, so the content is at src. Enforce there.
        // Gate the rewrite/chmod like any other: never write through a link.
        if (!confined_for_write(treedir, src_full))
            return BlkStatus::Failed;
        ensure_already_state(src_full, cur, blk, is_link);
        return BlkStatus::Already;
    }
    FileLines res;
    if (!apply_hunks(cur, blk.hunks, &res, blk.new_no_nl_file))
        return BlkStatus::Failed;
    // A directory/special file at the destination is a clean failure, not a
    // die inside write_target.
    if (path_kind(dst_full) == PathKind::Other)
        return BlkStatus::Failed;
    if (!creatable_under(treedir, dst_full))
        return BlkStatus::Failed;
    // Effective output mode: explicit new mode wins; otherwise a symlink
    // stays a symlink and a regular file keeps its executable bit, so
    // applying a content-only change never silently drops +x.
    std::string eff_mode = blk.new_mode;
    // Full permission snapshot for content-only rewrites (no explicit mode
    // in the patch): write_target is rename-based, so without a restore it
    // would reset restrictive modes (0700->0755 via the inferred 100755, or
    // 0640->0644 via 0666&~umask when non-exec). Restored below; explicit
    // mode changes keep the patch's declared 0755/0644.
    mode_t src_full_mode = 0;
    bool have_src_full_mode = false;
    if (eff_mode.empty()) {
        if (is_link) {
            eff_mode = "120000";
        } else {
            struct stat sst;
            if (stat(src_full.c_str(), &sst) == 0 && S_ISREG(sst.st_mode)) {
                src_full_mode = (mode_t)(sst.st_mode & 07777);
                have_src_full_mode = true;
                if (sst.st_mode & 0111)
                    eff_mode = "100755";
            }
        }
    }
    // A regular write follows the destination itself when it is a symlink:
    // replacing or overwriting through it would touch wherever it points.
    // Symlink outputs (eff_mode 120000) replace the link itself via
    // unlink+symlink and stay allowed; anything else fails as a conflict.
    // (Through-link ancestors were already gated above.)
    if (eff_mode != "120000") {
        struct stat dst_st;
        if (lstat(dst_full.c_str(), &dst_st) == 0 &&
            S_ISLNK(dst_st.st_mode))
            return BlkStatus::Failed;
    }
    if (blk.is_rename) {
        // Write to the new path, remove the old.
        write_target(treedir, dst_full, res, eff_mode);
        if (src_full != dst_full)
            remove_recursive(src_full);
    } else {
        write_target(treedir, dst_full, res, eff_mode);
    }
    if (have_src_full_mode) {
        struct stat dst_st;
        if (lstat(dst_full.c_str(), &dst_st) == 0 &&
            S_ISREG(dst_st.st_mode) &&
            (mode_t)(dst_st.st_mode & 07777) != src_full_mode) {
            if (chmod(dst_full.c_str(), src_full_mode) != 0)
                die("cannot set permissions on '" + dst_full +
                    "': " + strerror(errno));
        }
    }
    return BlkStatus::Applied;
}

VcsFailure failure_for_block(const PBlock& blk, const std::string& wid)
{
    VcsFailure f;
    auto uniq_push = [&](std::vector<std::string>& v, const std::string& p) {
        if (p.empty())
            return;
        if (std::find(v.begin(), v.end(), p) == v.end())
            v.push_back(p);
    };
    if (blk.is_combined) {
        std::string p;
        if (blk.has_old && blk.has_new && blk.old_rel == blk.new_rel)
            p = blk.old_rel;
        else if (!blk.new_rel.empty())
            p = blk.new_rel;
        else if (!blk.old_rel.empty())
            p = blk.old_rel;
        if (!p.empty()) {
            f.display = p;
            f.paths.push_back(p);
        } else if (!blk.header_line.empty()) {
            f.display = blk.header_line;
        } else {
            f.display = "<combined diff>";
        }
        return f;
    }
    if (blk.is_rename) {
        f.display = blk.rename_from + " -> " + blk.rename_to;
        uniq_push(f.paths, blk.rename_from);
        uniq_push(f.paths, blk.rename_to);
        if (f.display.empty() || f.display == " -> ")
            f.display = !blk.header_line.empty() ? blk.header_line : "<rename>";
        return f;
    }
    if (blk.has_old && blk.has_new) {
        if (blk.old_rel == blk.new_rel) {
            f.display = blk.old_rel;
            uniq_push(f.paths, blk.old_rel);
        } else if (!blk.old_rel.empty() && !blk.new_rel.empty()) {
            f.display = blk.old_rel + " -> " + blk.new_rel;
            uniq_push(f.paths, blk.old_rel);
            uniq_push(f.paths, blk.new_rel);
        } else if (!blk.old_rel.empty()) {
            f.display = blk.old_rel;
            uniq_push(f.paths, blk.old_rel);
        } else {
            f.display = blk.new_rel;
            uniq_push(f.paths, blk.new_rel);
        }
        if (f.display.empty())
            f.display = !blk.header_line.empty() ? blk.header_line : "<unknown file>";
        return f;
    }
    if (blk.has_old && !blk.has_new) {
        f.display = blk.old_rel.empty() && !blk.header_line.empty() ? blk.header_line
                                                                     : blk.old_rel;
        uniq_push(f.paths, blk.old_rel);
        if (f.display.empty())
            f.display = "<unknown file>";
        return f;
    }
    if (!blk.has_old && blk.has_new) {
        f.display = blk.new_rel.empty() && !blk.header_line.empty() ? blk.header_line
                                                                     : blk.new_rel;
        uniq_push(f.paths, blk.new_rel);
        if (f.display.empty())
            f.display = "<unknown file>";
        return f;
    }
    // No ---/+++: mode-only/empty fallback via diff sides. The wid must be
    // stripped with the block's own wid (callers thread it through); using
    // "" here would leak the wid prefix into merge paths.
    if (!blk.git_a.empty() && blk.git_a != "/dev/null") {
        bool miss = false;
        std::string rel = label_to_rel(blk.git_a, wid, &miss);
        if (!miss && !rel.empty()) {
            f.display = rel;
            f.paths.push_back(rel);
            return f;
        }
        f.display = blk.git_a;
        return f;
    }
    if (!blk.git_b.empty() && blk.git_b != "/dev/null") {
        bool miss = false;
        std::string rel = label_to_rel(blk.git_b, wid, &miss);
        if (!miss && !rel.empty()) {
            f.display = rel;
            f.paths.push_back(rel);
            return f;
        }
        f.display = blk.git_b;
        return f;
    }
    f.display = !blk.header_line.empty() ? blk.header_line : "<unknown file>";
    return f;
}

} // namespace

bool vcs_apply_whole(const std::string& treedir, const std::string& patch,
                     const std::string& wid)
{
    if (patch.empty())
        return true;
    if (patch.find('\0') != std::string::npos)
        die("patch contains NUL bytes; refusing (a well-formed patch is text: binary content travels base64-encoded)");
    // Empty modulo whitespace/comments counts as empty.
    bool any = false;
    for (auto& b : parse_patch(patch, wid)) {
        (void)b;
        any = true;
        break;
    }
    if (!any)
        return true;
    std::vector<PBlock> blocks = parse_patch(patch, wid);
    for (auto& b : blocks) {
        BlkStatus st = apply_block(treedir, b, wid);
        if (st == BlkStatus::Failed)
            return false;
    }
    return true;
}

std::vector<VcsFailure> vcs_apply_per_file(const std::string& workdir,
                                              const std::string& patch,
                                              const std::string& wid)
{
    std::vector<VcsFailure> failed;
    std::vector<PBlock> blocks = parse_patch(patch, wid);
    for (size_t i = 0; i < blocks.size(); ++i) {
        BlkStatus st = apply_block(workdir, blocks[i], wid);
        if (st == BlkStatus::Failed)
            failed.push_back(failure_for_block(blocks[i], wid));
    }
    return failed;
}

// Write a conflict-marker file: "<<<<<<< current" + current lines +
// "=======" + patched lines + ">>>>>>> patched", always newline-terminated
// (markers are line-based, so a missing trailing newline is normalized).
// Returns false (writing nothing) when the destination is not confined
// for writing (see confined_for_write): the caller has already recorded
// the conflict, so the file is left for the user instead of following a
// symlink out of the tree.
static bool write_patch_conflict(const std::string& treedir,
                                 const std::string& full,
                                 const std::vector<std::string>& current,
                                 const std::vector<std::string>& patched)
{
    if (!confined_for_write(treedir, full))
        return false;
    std::vector<std::string> out;
    out.push_back("<<<<<<< current");
    for (auto& l : current)
        out.push_back(l);
    out.push_back("=======");
    for (auto& l : patched)
        out.push_back(l);
    out.push_back(">>>>>>> patched");
    make_dirs(dirname_of(full));
    write_file_bytes(full, join_content(out, true));
    return true;
}

// Best-effort desired content for a failed block: hunks applied to empty,
// falling back to the added lines when they do not apply there either.
static std::vector<std::string> desired_from_hunks(const PBlock& blk)
{
    FileLines empty;
    empty.lines.clear();
    empty.ends_nl = true;
    FileLines res;
    if (!blk.hunks.empty() &&
        apply_hunks(empty, blk.hunks, &res, blk.new_no_nl_file))
        return res.lines;
    std::vector<std::string> out;
    for (auto& h : blk.hunks) {
        for (auto& ln : h.lines) {
            if (ln.op == '+')
                out.push_back(ln.text);
        }
    }
    return out;
}

// Turn one failed block into conflict markers inside `treedir`. The block's
// path(s) are already recorded by the caller; this only writes file content:
// modify blocks keep the hunks that match and embed per-hunk markers for the
// ones that do not, new-file blocks pit current bytes against desired bytes,
// deleted-file and rename blocks are left on disk untouched (the user
// resolves them by hand). Every write goes through confined_for_write, so a
// symlink ancestor planted in the target tree (e.g. "link -> /tmp" with a
// failing block for "link/evil" under `projeny patch`) fails as a recorded
// conflict instead of escaping the tree.
static void write_conflict_for_block(const std::string& treedir,
                                     const PBlock& blk)
{
    if (blk.is_combined || blk.is_rename)
        return;
    if (blk.is_binary)
        return; // binary content cannot carry inline markers: the file is
                // left on disk as-is and the conflict is recorded already.
    if (blk.is_new) {
        if (blk.new_rel.empty())
            return;
        std::string full = join_path(treedir, blk.new_rel);
        if (path_is_binary_file(full))
            return; // text conflict vs binary file: leave it
        // Do not even read through a symlink ancestor: the bytes would come
        // from outside the tree (the write below is already gated, but the
        // read must not follow either).
        if (!ancestors_are_dirs(treedir, full))
            return;
        if (path_kind(full) != PathKind::Regular)
            return; // missing (uncomputable), dir/special: leave it
        bool is_link = false;
        FileLines cur = read_target_lines(full, &is_link, nullptr);
        if (is_link)
            return; // symlinks get no inline markers; leave for the user
        write_patch_conflict(treedir, full, cur.lines,
                             desired_from_hunks(blk));
        return;
    }
    if (blk.is_deleted)
        return; // keep the file on disk; the user deletes it by hand
    // Modify. Only single-path content blocks get inline hunk markers;
    // anything rename-shaped is left alone.
    if (!blk.has_old || !blk.has_new || blk.old_rel != blk.new_rel ||
        blk.old_rel.empty() || blk.hunks.empty())
        return;
    std::string full = join_path(treedir, blk.old_rel);
    if (path_is_binary_file(full))
        return; // text conflict vs binary file: leave it
    // Same no-through-link-read rule as above (writes below are gated by
    // confined_for_write already).
    if (!ancestors_are_dirs(treedir, full))
        return;
    PathKind k = path_kind(full);
    if (k == PathKind::Other)
        return; // dir/special in the way: leave it
    if (k == PathKind::Missing) {
        write_patch_conflict(treedir, full, std::vector<std::string>(),
                             desired_from_hunks(blk));
        return;
    }
    bool is_link = false;
    FileLines cur = read_target_lines(full, &is_link, nullptr);
    if (is_link)
        return;
    if (cur.lines.empty() && cur.ends_nl) {
        // Empty file: nothing to match against; pit emptiness vs desired.
        write_patch_conflict(treedir, full, cur.lines,
                             desired_from_hunks(blk));
        return;
    }
    std::vector<std::string> out = cur.lines;
    long offset = 0;
    for (const PHunk& h : blk.hunks) {
        long exp = hunk_expected(h, false) + offset;
        if (exp < 0)
            exp = 0;
        if (exp > (long)out.size())
            exp = (long)out.size();
        long pos = find_hunk_pos(out, h, exp, false);
        if (pos >= 0) {
            // Splice this hunk in (same splice as apply_hunks, one hunk).
            std::vector<std::string> next;
            next.reserve(out.size() + 8);
            for (long i = 0; i < pos; ++i)
                next.push_back(out[(size_t)i]);
            long fp = pos;
            for (auto& ln : h.lines) {
                if (ln.op == ' ') {
                    if (fp < (long)out.size())
                        next.push_back(out[(size_t)fp]);
                    else
                        next.push_back(ln.text);
                    ++fp;
                } else if (ln.op == '-') {
                    ++fp;
                } else if (ln.op == '+') {
                    next.push_back(ln.text);
                }
            }
            for (size_t i = (size_t)fp; i < out.size(); ++i)
                next.push_back(out[i]);
            long rem = 0, add = 0;
            hunk_body_counts(h, &rem, &add, false);
            offset += (add - rem);
            out.swap(next);
            continue;
        }
        // Failed hunk: embed markers at the expected position. The current
        // side is the actual file slice the hunk would have replaced; the
        // patched side is what the hunk wanted (context + additions).
        long rem = 0, add = 0;
        hunk_body_counts(h, &rem, &add, false);
        long old_span = 0;
        for (auto& ln : h.lines) {
            if (ln.op == ' ' || ln.op == '-')
                ++old_span;
        }
        long e = exp;
        if (e > (long)out.size())
            e = (long)out.size();
        long slice_end = e + old_span;
        if (slice_end > (long)out.size())
            slice_end = (long)out.size();
        std::vector<std::string> current_side(out.begin() + (size_t)e,
                                              out.begin() + (size_t)slice_end);
        std::vector<std::string> patched_side;
        for (auto& ln : h.lines) {
            if (ln.op == '+' || ln.op == ' ')
                patched_side.push_back(ln.text);
        }
        std::vector<std::string> marker;
        marker.push_back("<<<<<<< current");
        for (auto& l : current_side)
            marker.push_back(l);
        marker.push_back("=======");
        for (auto& l : patched_side)
            marker.push_back(l);
        marker.push_back(">>>>>>> patched");
        // The marker block replaces the hunk region (no duplication).
        out.erase(out.begin() + (size_t)e,
                  out.begin() + (size_t)slice_end);
        out.insert(out.begin() + (size_t)e, marker.begin(), marker.end());
        offset += (long)marker.size() - (slice_end - e);
    }
    // Confined write (see confined_for_write): a symlink ancestor in the
    // target tree turns this into "leave the file for the user" (the
    // conflict is already recorded) instead of an escape.
    if (!confined_for_write(treedir, full))
        return;
    make_dirs(dirname_of(full));
    write_file_bytes(full, join_content(out, true));
}

bool vcs_apply_with_conflicts(const std::string& treedir,
                              const std::string& patch, const std::string& wid,
                              std::vector<std::string>* conflicts)
{
    if (patch.empty())
        return true;
    if (patch.find((char)0) != std::string::npos)
        die("patch contains NUL bytes; refusing (a well-formed patch is text: binary content travels base64-encoded)");
    std::vector<PBlock> blocks = parse_patch(patch, wid);
    if (blocks.empty())
        return true;
    bool all_clean = true;
    for (size_t i = 0; i < blocks.size(); ++i) {
        const PBlock& blk = blocks[i];
        BlkStatus st = apply_block(treedir, blk, wid);
        if (st != BlkStatus::Failed)
            continue;
        all_clean = false;
        VcsFailure f = failure_for_block(blk, wid);
        for (auto& p : f.paths) {
            if (p.empty())
                continue;
            if (std::find(conflicts->begin(), conflicts->end(), p) ==
                conflicts->end())
                conflicts->push_back(p);
        }
        if (f.paths.empty()) {
            // Opaque block (e.g. combined diff) with no parseable path:
            // still report the display name so the console list is complete.
            std::string d = f.display.empty() ? "<unknown file>" : f.display;
            if (std::find(conflicts->begin(), conflicts->end(), d) ==
                conflicts->end())
                conflicts->push_back(d);
        }
        write_conflict_for_block(treedir, blk);
    }
    return all_clean;
}

std::vector<std::string> vcs_touched_paths(const std::string& patch,
                                           const std::string& wid)
{
    std::vector<std::string> out;
    if (patch.empty())
        return out;
    if (patch.find((char)0) != std::string::npos)
        die("patch contains NUL bytes; refusing (a well-formed patch is text: binary content travels base64-encoded)");
    for (auto& b : parse_patch(patch, wid)) {
        VcsFailure f = failure_for_block(b, wid);
        for (auto& rel : f.paths) {
            if (!rel.empty() &&
                std::find(out.begin(), out.end(), rel) == out.end())
                out.push_back(rel);
        }
        if (f.paths.empty() && !f.display.empty() &&
            std::find(out.begin(), out.end(), f.display) == out.end())
            out.push_back(f.display);
    }
    return out;
}

std::string vcs_drop_deletes_not_in(const std::string& patch,
                                    const std::string& wid,
                                    const std::vector<std::string>& keep)
{
    if (patch.empty())
        return patch;
    std::vector<PBlock> blocks = parse_patch(patch, wid);
    // All touched paths in the patch (for file/dir swap detection below).
    std::vector<std::string> touched;
    for (auto& b : blocks) {
        VcsFailure f = failure_for_block(b, wid);
        for (auto& rel : f.paths) {
            if (!rel.empty() &&
                std::find(touched.begin(), touched.end(), rel) == touched.end())
                touched.push_back(rel);
        }
    }
    auto is_prefix_path = [](const std::string& pre, const std::string& full) {
        return full.size() > pre.size() &&
               full.compare(0, pre.size(), pre) == 0 &&
               full[pre.size()] == '/';
    };
    // `keep` may hold directories (projeny rm/add take dirs): a patch block
    // names files, so a deleted file under a pending-removed (or
    // rename-source) dir is kept via under_path semantics, as
    // is_tracked_path does.
    auto is_kept = [&keep, &is_prefix_path](const std::string& rel) {
        for (auto& k : keep) {
            if (k.empty())
                continue;
            if (rel == k || is_prefix_path(k, rel))
                return true;
        }
        return false;
    };
    std::string out;
    for (auto& b : blocks) {
        bool pure_delete = b.is_deleted && !b.is_rename && !b.is_new &&
                           !b.is_combined;
        if (pure_delete) {
            // Deleted path: old_rel for ---/+++-style blocks; for blocks
            // without ---/+++ (mode-only/empty fallbacks) the display path
            // doubles as the deleted side.
            std::string rel = b.old_rel;
            if (rel.empty())
                rel = failure_for_block(b, wid).display;
            if (!is_kept(rel)) {
                // Accidental loss: drop it so setup restores the file —
                // unless it participates in a file/dir swap visible in this
                // same patch (a tracked file replaced by a directory, or a
                // tracked directory replaced by a file). Swaps show as a
                // delete plus an add with a strict prefix relationship in
                // either direction; dropping the delete there would strand
                // the add behind an ENOTDIR failure instead of merging.
                bool swap = false;
                for (auto& t : touched) {
                    if (t != rel &&
                        (is_prefix_path(rel, t) || is_prefix_path(t, rel))) {
                        swap = true;
                        break;
                    }
                }
                if (!swap)
                    continue;
            }
        }
        out += b.raw;
    }
    return out;
}

std::vector<std::string> vcs_binary_add_paths(const std::string& patch,
                                              const std::string& wid)
{
    std::vector<std::string> out;
    if (patch.empty())
        return out;
    for (auto& b : parse_patch(patch, wid)) {
        if (b.is_combined || b.new_rel.empty())
            continue;
        // Tracked binaries: payload-carrying adds, and EVERY binary rename
        // destination (payload-free pure renames included). Commit re-derives
        // its diff against the raw archive, which predates every committed
        // rename, so a committed binary rename whose bytes changed re-derives
        // as delete+add (binaries never similarity-pair); without the
        // destination here that add is dropped as untracked and the committed
        // file silently vanishes on the next setup. Where pairing re-finds
        // the rename instead, the extra entry is harmless: it only re-covers
        // a path the stored patch already carries.
        bool tracked = (b.is_binary && b.binary_has_payload && b.is_new &&
                        !b.is_rename) ||
                       (b.is_binary && b.is_rename);
        if (tracked &&
            std::find(out.begin(), out.end(), b.new_rel) == out.end())
            out.push_back(b.new_rel);
    }
    return out;
}

// Shared implementation of vcs_add_paths / vcs_deleted_paths: collect the
// workdir-relative paths of the patch's pure adds (plus rename
// destinations, whose commit-derived diff needs them) or pure deletes
// respectively — deduplicated, in patch order.
static std::vector<std::string> patch_side_paths(const std::string& patch,
                                                 const std::string& wid,
                                                 bool adds)
{
    std::vector<std::string> out;
    if (patch.empty())
        return out;
    for (auto& b : parse_patch(patch, wid)) {
        bool hit;
        if (adds) {
            bool pure_add = b.is_new && !b.is_rename && !b.is_deleted &&
                            !b.is_combined;
            // Rename destinations are tracked too (see the header comment):
            // a committed rename's content can diverge beyond rename
            // detection, in which case commit's re-derived diff (against the
            // raw archive, which predates the rename) is a delete of the old
            // path plus an add of the new one — dropping that add loses the
            // committed file.
            bool rename_new = b.is_rename && !b.is_combined;
            hit = pure_add || rename_new;
        } else {
            hit = b.is_deleted && !b.is_rename && !b.is_new && !b.is_combined;
        }
        if (!hit)
            continue;
        std::string rel = adds ? b.new_rel : b.old_rel;
        if (rel.empty())
            rel = failure_for_block(b, wid).display;
        if (rel.empty() || rel == "<unknown file>" || rel == "<combined diff>" ||
            rel == "<rename>")
            continue;
        if (std::find(out.begin(), out.end(), rel) == out.end())
            out.push_back(rel);
    }
    return out;
}

std::vector<std::string> vcs_add_paths(const std::string& patch,
                                       const std::string& wid)
{
    return patch_side_paths(patch, wid, true);
}

std::vector<std::string> vcs_deleted_paths(const std::string& patch,
                                           const std::string& wid)
{
    return patch_side_paths(patch, wid, false);
}

namespace {
bool vcs_is_prefix_path(const std::string& pre, const std::string& full)
{
    return full.size() > pre.size() &&
           full.compare(0, pre.size(), pre) == 0 &&
           full[pre.size()] == '/';
}

bool vcs_is_kept_path(const std::vector<std::string>& keep,
                      const std::string& rel)
{
    for (auto& k : keep) {
        if (k.empty())
            continue;
        if (rel == k || vcs_is_prefix_path(k, rel))
            return true;
    }
    return false;
}
} // namespace

std::string vcs_drop_adds_not_in(const std::string& patch,
                                 const std::string& wid,
                                 const std::vector<std::string>& keep)
{
    if (patch.empty())
        return patch;
    std::vector<PBlock> blocks = parse_patch(patch, wid);
    // `keep` may hold directories (projeny add takes dirs): an add under a
    // pending-added (or rename-destination) dir is kept via under_path
    // semantics, as is_tracked_path does.
    std::string out;
    for (auto& b : blocks) {
        bool pure_add = b.is_new && !b.is_rename && !b.is_deleted &&
                        !b.is_combined;
        if (pure_add) {
            std::string rel = b.new_rel;
            if (rel.empty())
                rel = failure_for_block(b, wid).display;
            if (!vcs_is_kept_path(keep, rel))
                continue; // untracked file: leave it out of the patch
        }
        out += b.raw;
    }
    return out;
}

std::string vcs_drop_binary_adds_not_in(const std::string& patch,
                                        const std::string& wid,
                                        const std::vector<std::string>& keep)
{
    if (patch.empty())
        return patch;
    std::vector<PBlock> blocks = parse_patch(patch, wid);
    // `keep` may hold directories (projeny add takes dirs): a binary add
    // under a pending-added (or rename-destination) dir is kept via
    // under_path semantics, as is_tracked_path does.
    auto is_prefix_path = [](const std::string& pre, const std::string& full) {
        return full.size() > pre.size() &&
               full.compare(0, pre.size(), pre) == 0 &&
               full[pre.size()] == '/';
    };
    auto is_kept_binary = [&keep, &is_prefix_path](const std::string& rel) {
        for (auto& k : keep) {
            if (k.empty())
                continue;
            if (rel == k || is_prefix_path(k, rel))
                return true;
        }
        return false;
    };
    std::string out;
    for (auto& b : blocks) {
        if (b.is_binary && b.binary_has_payload && b.is_new && !b.is_rename) {
            if (!is_kept_binary(b.new_rel))
                continue; // untracked binary: leave it out of the patch
        }
        out += b.raw;
    }
    return out;
}

namespace {

struct Change {
    size_t base_start, base_end; // [start,end) in base lines
    std::vector<std::string> fresh; // replacement lines from this side
};

std::vector<Change> changes_from(const std::vector<std::string>& base,
                                 const std::vector<std::string>& side)
{
    std::vector<Op> script = myers_lines(base, side);
    std::vector<Change> out;
    size_t i = 0, bpos = 0;
    while (i < script.size()) {
        if (script[i].kind == ' ') {
            ++i;
            ++bpos;
            continue;
        }
        size_t s = bpos;
        std::vector<std::string> fresh;
        while (i < script.size() && script[i].kind != ' ') {
            if (script[i].kind == '-')
                ++bpos;
            else if (script[i].kind == '+')
                fresh.push_back(script[i].line);
            ++i;
        }
        out.push_back({s, bpos, fresh});
    }
    return out;
}

} // namespace

bool vcs_merge_one_file(const std::string& base_file, const std::string& ours_file,
                        const std::string& theirs_file, const std::string& dst_path,
                        const std::string& dst_root)
{
    // Symlink-ancestor safety (no check needed here, by construction):
    // every caller passes a dst inside a tool-created temp tree (setup and
    // rebase merges), never a user-controlled dir. Symlinks inside such a
    // tree can only come from tarballs and patches, both of which reject
    // absolute and ".."-carrying link targets up front, so every link
    // resolves inside the tree: writing through one cannot escape it.
    // (Direct patch application to user dirs goes through apply_block and
    // the conflict writers instead, which do check — see creatable_under
    // and confined_for_write.)
    bool base_e = path_exists(base_file);
    bool ours_e = path_exists(ours_file);
    bool theirs_e = path_exists(theirs_file);

    // Directories and special files (FIFOs, sockets, devices) cannot be read
    // as file content: read_file_bytes would die (EISDIR) or hang forever
    // (FIFO open blocks). Any side that is neither a symlink nor a regular
    // file becomes a clean conflict with a placeholder instead.
    auto side_kind = [](const std::string& f, bool exists) -> int {
        // 0 missing, 1 symlink, 2 regular, 3 dir/special.
        if (!exists)
            return 0;
        struct stat st;
        if (lstat(f.c_str(), &st) != 0)
            return 0;
        if (S_ISLNK(st.st_mode))
            return 1;
        if (S_ISREG(st.st_mode))
            return 2;
        return 3;
    };
    int bk = side_kind(base_file, base_e);
    int okind = side_kind(ours_file, ours_e);
    int tkind = side_kind(theirs_file, theirs_e);
    if (bk == 3 || okind == 3 || tkind == 3) {
        // File/directory (or special-file) swaps cannot be line-merged.
        // Like git's file/directory conflicts: keep the local (theirs) entry
        // when it exists so no uncommitted user data is silently dropped,
        // and always report a conflict for manual resolution. Only when
        // neither side exists (both deleted a former dir) is it clean.
        auto keep_side = [&](const std::string& src, int k) {
            make_dirs(dirname_of(dst_path));
            remove_recursive(dst_path);
            if (k == 1) {
                std::string target = read_link_target(src);
                check_patch_link_target(dst_root, dst_path, target);
                if (symlink(target.c_str(), dst_path.c_str()) != 0)
                    die("cannot create symlink '" + dst_path +
                        "': " + strerror(errno));
                return;
            }
            if (k == 2) {
                copy_file_bytes(src, dst_path);
                struct stat sst;
                if (lstat(src.c_str(), &sst) == 0 && S_ISREG(sst.st_mode)) {
                    if (chmod(dst_path.c_str(),
                              (mode_t)(sst.st_mode & 07777)) != 0)
                        die("cannot set permissions on '" + dst_path +
                            "': " + strerror(errno));
                }
                return;
            }
            // Directories (and FIFO/socket survivors) copy with cp -a, which
            // recreates them without following or blocking.
            copy_recursive(src, dst_path);
        };
        if (tkind != 0) {
            keep_side(theirs_file, tkind);
            return false;
        }
        if (okind != 0) {
            keep_side(ours_file, okind);
            return false;
        }
        remove_recursive(dst_path);
        return true;
    }

    // Symlink-aware helpers: lstat first so links are compared by target
    // string, never by dereferenced bytes. A link vs file (or differing
    // targets) is always a content mismatch.
    auto is_link = [](const std::string& f) -> bool {
        struct stat st;
        if (lstat(f.c_str(), &st) != 0)
            return false;
        return S_ISLNK(st.st_mode);
    };
    auto read_target = [](const std::string& f) -> std::string {
        return read_link_target(f);
    };
    // Copy src to dst preserving symlink-ness and permission bits, so a
    // clean merge never introduces spurious mode changes (which the next
    // commit would otherwise report as uncommitted work). Symlink targets
    // are validated exactly like patch/tar links so a malicious merge
    // cannot plant an escaping link. Timestamps come along too, so clean
    // merges of unmodified files don't stamp "now" and trigger rebuilds.
    auto place = [&](const std::string& src) {
        if (src.empty() || !path_exists(src)) {
            unlink(dst_path.c_str());
            return;
        }
        make_dirs(dirname_of(dst_path));
        struct stat sst;
        if (lstat(src.c_str(), &sst) == 0 && S_ISLNK(sst.st_mode)) {
            std::string target = read_target(src);
            check_patch_link_target(dst_root, dst_path, target);
            unlink(dst_path.c_str());
            if (symlink(target.c_str(), dst_path.c_str()) != 0)
                die("cannot create symlink '" + dst_path +
                    "': " + strerror(errno));
            preserve_file_times(src, dst_path);
            return;
        }
        copy_file_bytes(src, dst_path);
        if (lstat(src.c_str(), &sst) == 0 && S_ISREG(sst.st_mode)) {
            if (chmod(dst_path.c_str(), (mode_t)(sst.st_mode & 07777)) != 0)
                die("cannot set permissions on '" + dst_path +
                    "': " + strerror(errno));
        }
        preserve_file_times(src, dst_path);
    };
    auto exec_flag = [](const std::string& f) -> int {
        struct stat st;
        if (lstat(f.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            return -1;
        return (st.st_mode & 0111) ? 1 : 0;
    };
    // Desired executable bit for a freshly written merge result. Three-way
    // rule: agreement wins; a chmod on exactly one side wins; divergent
    // chmods keep ours. Missing/non-regular sides vote -1 and are ignored.
    // NOTE: sample this BEFORE writing dst: every in-place caller
    // (setup, conflicted setup, rebase) merges with ours_file == dst_path,
    // so voting after the write would read back the fresh 0666&~umask mode
    // and mistake it for an upstream chmod -x.
    auto merged_exec_want = [&]() -> int {
        int bo = exec_flag(base_file), oo = exec_flag(ours_file),
            to = exec_flag(theirs_file);
        if (oo == to && oo >= 0)
            return oo;
        if (bo == oo && to >= 0)
            return to;
        if (bo == to && oo >= 0)
            return oo;
        if (oo >= 0)
            return oo;
        if (to >= 0)
            return to;
        if (bo >= 0)
            return bo;
        return -1;
    };
    // Full permission template for a freshly written merge result. Prefers
    // ours (which aliases dst in every in-place caller), then theirs, then
    // base, so restrictive modes survive: 0700 stays 0700, 0640 stays 0640.
    // Masked with 07777 so suid/sgid/sticky survive local merges; staged
    // package/extract payloads strip them separately (copy_path_preserving
    // uses 0777). Sample BEFORE writing dst for the same aliasing reason
    // as merged_exec_want below. Falls back to 0644 for brand-new conflict
    // files with no regular side to sample.
    auto merged_mode_template = [&]() -> mode_t {
        struct stat st;
        if (stat(ours_file.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            return (mode_t)(st.st_mode & 07777);
        if (stat(theirs_file.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            return (mode_t)(st.st_mode & 07777);
        if (stat(base_file.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            return (mode_t)(st.st_mode & 07777);
        return (mode_t)0644;
    };
    // Restore the full mode on a freshly written merge result at `dst`
    // to the pre-sampled `want` exec vote applied onto the pre-sampled
    // `tmpl` permission template (-1 vote: leave alone). Line merges and
    // conflict-marker writes go through write_file_bytes (0666 & ~umask),
    // so without this a merge of an executable file silently drops +x and
    // the next commit would record a spurious mode change. Hardcoding
    // 0755/0644 here would widen 0700->0755, leak 0640->0644, and drop
    // suid/sgid/sticky; instead the rw (and special) bits come from the
    // template while only the exec bits follow the vote:
    //   want==1 and template already exec -> keep template exactly
    //     (0700 stays 0700);
    //   want==1 and template non-exec -> template | 0111
    //     (0640->0751, 0644->0755; rw bits preserved);
    //   want==0 -> template & ~0111
    //     (0755->0644, 0700->0600, 0750->0640; rw bits preserved).
    auto restore_merged_exec = [&](const std::string& dst, int want,
                                   mode_t tmpl) {
        if (want < 0)
            return;
        struct stat dst_st;
        if (lstat(dst.c_str(), &dst_st) != 0 || !S_ISREG(dst_st.st_mode))
            return;
        tmpl = (mode_t)(tmpl & 07777);
        mode_t m;
        if (want) {
            if (tmpl & 0111)
                m = tmpl;
            else
                m = (mode_t)(tmpl | 0111);
        } else {
            m = (mode_t)(tmpl & ~0111);
        }
        if ((dst_st.st_mode & 07777) != m) {
            if (chmod(dst.c_str(), m) != 0)
                die("cannot set permissions on '" + dst + "': " +
                    strerror(errno));
        }
    };
    auto conflict_text = [](const std::string& o, const std::string& t) {
        std::string out = "<<<<<<< projeny (new setup)\n" + o;
        if (!out.empty() && out.back() != '\n')
            out += "\n";
        out += "=======\n" + t;
        if (!out.empty() && out.back() != '\n')
            out += "\n";
        out += ">>>>>>> projeny (local changes)\n";
        return out;
    };

    // Binary (NUL-bearing) sides merge byte-wise, never line-wise: markers
    // would corrupt binary content. Agreement wins; a change on exactly one
    // side wins; divergent changes keep the theirs (local) bytes so no
    // uncommitted user data is silently dropped, and report a conflict for
    // manual resolution. Symlink-mixed cases (a link on any side while a
    // regular side carries NULs) also keep theirs and conflict: links
    // compare by target string and never line-merge against binary bytes.
    {
        auto has_nul = [](const std::string& f, int k) -> bool {
            if (k != 2)
                return false;
            std::string data = read_file_bytes(f);
            return data.find(char(0)) != std::string::npos;
        };
        bool any_binary =
            has_nul(base_file, bk) || has_nul(ours_file, okind) ||
            has_nul(theirs_file, tkind);
        if (any_binary) {
            bool any_link = (bk == 1 || okind == 1 || tkind == 1);
            int ew = merged_exec_want();
            mode_t tm = merged_mode_template();
            auto keep_theirs_conflict = [&]() -> bool {
                if (tkind != 0) {
                    place(theirs_file);
                } else if (okind != 0) {
                    place(ours_file);
                } else {
                    remove_recursive(dst_path);
                }
                return false;
            };
            if (any_link)
                return keep_theirs_conflict();
            auto read_opt = [](const std::string& f, int k,
                               std::string* out) -> bool {
                if (k == 0) {
                    out->clear();
                    return false;
                }
                *out = read_file_bytes(f);
                return true;
            };
            std::string b, o, t;
            bool be = read_opt(base_file, bk, &b);
            bool oe = read_opt(ours_file, okind, &o);
            bool te = read_opt(theirs_file, tkind, &t);
            bool clean = false;
            std::string winner;
            bool delete_winner = false;
            if (oe && te && o == t) {
                winner = ours_file;
                clean = true;
            } else if (be && oe && b == o) {
                // Base == ours: take theirs (may be a deletion).
                if (!te) {
                    delete_winner = true;
                } else {
                    winner = theirs_file;
                }
                clean = true;
            } else if (be && te && b == t) {
                // Base == theirs: take ours (may be a deletion).
                if (!oe) {
                    delete_winner = true;
                } else {
                    winner = ours_file;
                }
                clean = true;
            } else if (!be && !oe && !te) {
                return true; // nothing anywhere (defensive)
            } else if (!be) {
                // Added on one or both sides without a base.
                if (oe && !te) {
                    winner = ours_file;
                    clean = true;
                } else if (!oe && te) {
                    winner = theirs_file;
                    clean = true;
                } else if (oe && te) {
                    return keep_theirs_conflict();
                }
            } else if (!oe && !te) {
                // Deleted on both sides.
                remove_recursive(dst_path);
                return true;
            } else if (!oe || !te) {
                // Deleted on one side, changed on the other.
                const std::string& kept = oe ? o : t;
                if (kept == b) {
                    remove_recursive(dst_path);
                    return true;
                }
                return keep_theirs_conflict();
            } else {
                return keep_theirs_conflict();
            }
            if (clean) {
                if (delete_winner) {
                    remove_recursive(dst_path);
                    return true;
                }
                // place() preserves the winner's bytes, mode, and
                // timestamps, so clean binary merges never look modified.
                place(winner);
                // A fresh binary blob takes the merged exec vote (a chmod on
                // exactly one side still wins for binaries).
                restore_merged_exec(dst_path, ew, tm);
                return true;
            }
            return keep_theirs_conflict();
        }
    }

    if (!base_e) {
        // File is new on at least one side. Compare symlink-ness first:
        // links by target string, regular files by bytes. Link vs file is
        // always a mismatch.
        if (ours_e && theirs_e) {
            bool o_link = is_link(ours_file);
            bool t_link = is_link(theirs_file);
            if (o_link || t_link) {
                if (o_link && t_link) {
                    std::string ot = read_target(ours_file);
                    std::string tt = read_target(theirs_file);
                    if (ot == tt) {
                        place(ours_file);
                        return true;
                    }
                }
                std::string o = o_link ? read_target(ours_file)
                                       : read_file_bytes(ours_file);
                std::string t = t_link ? read_target(theirs_file)
                                       : read_file_bytes(theirs_file);
                if ((!o_link && o.find('\0') != std::string::npos) ||
                    (!t_link && t.find('\0') != std::string::npos))
                    die("cannot merge binary files; binary files are not supported");
                make_dirs(dirname_of(dst_path));
                // Conflict becomes a regular file with markers; any
                // pre-existing symlink at dst is replaced.
                // Sample mode/vote before the unlink+write: ours may alias
                // dst, so sampling after would read the fresh 0666 mode.
                int ew_link = merged_exec_want();
                mode_t tm_link = merged_mode_template();
                unlink(dst_path.c_str());
                write_file_bytes(dst_path, conflict_text(o, t));
                restore_merged_exec(dst_path, ew_link, tm_link);
                return false;
            }
            std::string o = read_file_bytes(ours_file);
            std::string t = read_file_bytes(theirs_file);
            if (o.find('\0') != std::string::npos ||
                t.find('\0') != std::string::npos)
                die("cannot merge binary files; binary files are not supported");
            if (o == t) {
                place(ours_file);
                return true;
            }
            make_dirs(dirname_of(dst_path));
            int ew_new = merged_exec_want();
            mode_t tm_new = merged_mode_template();
            unlink(dst_path.c_str());
            write_file_bytes(dst_path, conflict_text(o, t));
            restore_merged_exec(dst_path, ew_new, tm_new);
            return false;
        }
        place(ours_e ? ours_file : theirs_file);
        return true;
    }
    if (!ours_e && !theirs_e) {
        unlink(dst_path.c_str());
        return true;
    }
    if (!ours_e || !theirs_e) {
        std::string kept = ours_e ? ours_file : theirs_file;
        bool k_link = is_link(kept);
        bool b_link = is_link(base_file);
        if (k_link || b_link) {
            std::string kd = k_link ? read_target(kept) : read_file_bytes(kept);
            std::string bd = b_link ? read_target(base_file) : read_file_bytes(base_file);
            if ((!k_link && kd.find('\0') != std::string::npos) ||
                (!b_link && bd.find('\0') != std::string::npos))
                die("cannot merge binary files; binary files are not supported");
            bool same = (k_link == b_link) && (kd == bd);
            if (same) {
                unlink(dst_path.c_str());
                return true;
            }
            make_dirs(dirname_of(dst_path));
            std::string out;
            if (!ours_e) {
                out = "<<<<<<< projeny (new setup: file deleted)\n=======\n" + kd;
                if (!out.empty() && out.back() != '\n')
                    out += "\n";
                out += ">>>>>>> projeny (local changes)\n";
            } else {
                out = "<<<<<<< projeny (new setup)\n" + kd;
                if (!out.empty() && out.back() != '\n')
                    out += "\n";
                out += "=======\n>>>>>>> projeny (local changes: file deleted)\n";
            }
            int ew_dell = merged_exec_want();
            mode_t tm_dell = merged_mode_template();
            unlink(dst_path.c_str());
            write_file_bytes(dst_path, out);
            restore_merged_exec(dst_path, ew_dell, tm_dell);
            return false;
        }
        std::string k = read_file_bytes(kept);
        std::string b = read_file_bytes(base_file);
        if (k.find('\0') != std::string::npos ||
            b.find('\0') != std::string::npos)
            die("cannot merge binary files; binary files are not supported");
        if (k == b) {
            unlink(dst_path.c_str());
            return true;
        }
        make_dirs(dirname_of(dst_path));
        std::string out;
        if (!ours_e) {
            out = "<<<<<<< projeny (new setup: file deleted)\n=======\n" + k;
            if (!out.empty() && out.back() != '\n')
                out += "\n";
            out += ">>>>>>> projeny (local changes)\n";
        } else {
            out = "<<<<<<< projeny (new setup)\n" + k;
            if (!out.empty() && out.back() != '\n')
                out += "\n";
            out += "=======\n>>>>>>> projeny (local changes: file deleted)\n";
        }
        int ew_del = merged_exec_want();
        mode_t tm_del = merged_mode_template();
        unlink(dst_path.c_str());
        write_file_bytes(dst_path, out);
        restore_merged_exec(dst_path, ew_del, tm_del);
        return false;
    }
    // All three exist: lstat first. If any side is a symlink, compare by
    // link-target strings (link vs file is always a mismatch); only when
    // all three are regular files compare dereferenced bytes. Symlink
    // conflicts are reported with target strings as marker content and
    // never attempt line-level merging.
    {
        bool b_link = is_link(base_file);
        bool o_link = is_link(ours_file);
        bool t_link = is_link(theirs_file);
        if (b_link || o_link || t_link) {
            std::string b_s = b_link ? read_target(base_file) : read_file_bytes(base_file);
            std::string o_s = o_link ? read_target(ours_file) : read_file_bytes(ours_file);
            std::string t_s = t_link ? read_target(theirs_file) : read_file_bytes(theirs_file);
            if ((!b_link && b_s.find('\0') != std::string::npos) ||
                (!o_link && o_s.find('\0') != std::string::npos) ||
                (!t_link && t_s.find('\0') != std::string::npos))
                die("cannot merge binary files; binary files are not supported");
            bool ot_eq = (o_link == t_link) && (o_s == t_s);
            if (ot_eq) {
                if (!o_link && !t_link && !b_link) {
                    int bo = exec_flag(base_file), oo = exec_flag(ours_file),
                        to = exec_flag(theirs_file);
                    if (oo == to || to < 0)
                        place(ours_file);
                    else if (bo == oo)
                        place(theirs_file);
                    else
                        place(ours_file);
                } else {
                    place(ours_file);
                }
                return true;
            }
            bool bo_eq = (b_link == o_link) && (b_s == o_s);
            if (bo_eq) {
                place(theirs_file);
                return true;
            }
            bool bt_eq = (b_link == t_link) && (b_s == t_s);
            if (bt_eq) {
                place(ours_file);
                return true;
            }
            make_dirs(dirname_of(dst_path));
            int ew_sym = merged_exec_want();
            mode_t tm_sym = merged_mode_template();
            unlink(dst_path.c_str());
            write_file_bytes(dst_path, conflict_text(o_s, t_s));
            restore_merged_exec(dst_path, ew_sym, tm_sym);
            return false;
        }
    }
    std::string bdata = read_file_bytes(base_file);
    std::string odata = read_file_bytes(ours_file);
    std::string tdata = read_file_bytes(theirs_file);
    if (bdata.find('\0') != std::string::npos ||
        odata.find('\0') != std::string::npos ||
        tdata.find('\0') != std::string::npos)
        die("cannot merge binary files; binary files are not supported");
    if (odata == tdata) {
        // Same bytes: merge the executable bit three-way (a chmod on exactly
        // one side wins; agreement wins).
        int bo = exec_flag(base_file), oo = exec_flag(ours_file),
            to = exec_flag(theirs_file);
        if (oo == to || to < 0)
            place(ours_file);
        else if (bo == oo)
            place(theirs_file);
        else
            place(ours_file);
        return true;
    }
    if (bdata == odata) {
        place(theirs_file);
        return true;
    }
    if (bdata == tdata) {
        place(ours_file);
        return true;
    }
    FileLines base = split_content(bdata);
    FileLines ours = split_content(odata);
    FileLines theirs = split_content(tdata);
    std::vector<Change> co = changes_from(base.lines, ours.lines);
    std::vector<Change> ct = changes_from(base.lines, theirs.lines);
    std::vector<std::string> merged;
    bool clean = true;
    size_t i = 0, p = 0, q = 0;
    // Source of the trailing newline when clean (resolved after the loop).
    while (i < base.lines.size() || p < co.size() || q < ct.size()) {
        // Next change boundaries at/after i.
        Change* c1 = nullptr;
        Change* c2 = nullptr;
        if (p < co.size() && co[p].base_start <= i &&
            (co[p].base_end > i || co[p].base_start == i))
            c1 = &co[p];
        else if (p < co.size() && co[p].base_start > i &&
                 (q >= ct.size() || co[p].base_start <= ct[q].base_start))
            c1 = nullptr; // gap first
        if (q < ct.size() && ct[q].base_start <= i &&
            (ct[q].base_end > i || ct[q].base_start == i))
            c2 = &ct[q];
        // Determine next event position.
        size_t next = base.lines.size();
        if (p < co.size() && co[p].base_start > i)
            next = co[p].base_start < next ? co[p].base_start : next;
        if (q < ct.size() && ct[q].base_start > i)
            next = ct[q].base_start < next ? ct[q].base_start : next;
        if (!c1 && !c2) {
            // Gap: copy base lines up to next (or EOF), unless a change
            // starts exactly at i (handled below as insertion).
            bool ins1 = p < co.size() && co[p].base_start == i &&
                        co[p].base_end == i && !co[p].fresh.empty();
            bool ins2 = q < ct.size() && ct[q].base_start == i &&
                        ct[q].base_end == i && !ct[q].fresh.empty();
            if (ins1 || ins2) {
                // Insertion(s) at gap i.
                if (ins1 && ins2) {
                    if (co[p].fresh == ct[q].fresh) {
                        for (auto& l : co[p].fresh)
                            merged.push_back(l);
                    } else {
                        clean = false;
                        merged.push_back("<<<<<<< projeny (new setup)");
                        for (auto& l : co[p].fresh)
                            merged.push_back(l);
                        merged.push_back("=======");
                        for (auto& l : ct[q].fresh)
                            merged.push_back(l);
                        merged.push_back(">>>>>>> projeny (local changes)");
                    }
                    ++p;
                    ++q;
                    continue;
                } else if (ins1) {
                    for (auto& l : co[p].fresh)
                        merged.push_back(l);
                    ++p;
                    continue;
                } else {
                    for (auto& l : ct[q].fresh)
                        merged.push_back(l);
                    ++q;
                    continue;
                }
            }
            while (i < next) {
                merged.push_back(base.lines[i]);
                ++i;
            }
            // Changes starting at `next` (== i now) loop around.
            if (i >= base.lines.size() && p >= co.size() && q >= ct.size())
                break;
            // Avoid infinite loop when next == i but no insertion matched
            // (means a non-insertion change starts at i; fall through).
            if (next > i)
                continue;
        }
        // Refresh covering changes at i.
        c1 = nullptr;
        c2 = nullptr;
        if (p < co.size() && co[p].base_start <= i && co[p].base_end > i)
            c1 = &co[p];
        else if (p < co.size() && co[p].base_start == i && co[p].base_end == i) {
            // Insertion handled above; if we reach here one side had a
            // non-insertion at i and the other an insertion: merge as overlap.
            c1 = &co[p];
        }
        if (q < ct.size() && ct[q].base_start <= i && ct[q].base_end > i)
            c2 = &ct[q];
        else if (q < ct.size() && ct[q].base_start == i && ct[q].base_end == i)
            c2 = &ct[q];
        if (c1 && !c2) {
            for (auto& l : c1->fresh)
                merged.push_back(l);
            i = c1->base_end;
            ++p;
        } else if (!c1 && c2) {
            for (auto& l : c2->fresh)
                merged.push_back(l);
            i = c2->base_end;
            ++q;
        } else if (c1 && c2) {
            size_t u_end = c1->base_end > c2->base_end ? c1->base_end : c2->base_end;
            // Ours/theirs versions of the union range: each is the chunk's
            // fresh lines (plus base tail lines spliced in below).
            std::vector<std::string> ov = c1->fresh, tv = c2->fresh;
            // For overlapping ranges with different extents, splice base tail
            // lines the shorter side does not cover so both versions span the
            // union (diff3 semantics for adjacent-but-unequal edits).
            if (c1->base_end < u_end) {
                std::vector<std::string> ext(ov);
                for (size_t k = c1->base_end; k < u_end; ++k)
                    ext.push_back(base.lines[k]);
                ov.swap(ext);
            }
            if (c2->base_end < u_end) {
                std::vector<std::string> ext(tv);
                for (size_t k = c2->base_end; k < u_end; ++k)
                    ext.push_back(base.lines[k]);
                tv.swap(ext);
            }
            if (ov == tv) {
                for (auto& l : ov)
                    merged.push_back(l);
            } else {
                clean = false;
                merged.push_back("<<<<<<< projeny (new setup)");
                for (auto& l : ov)
                    merged.push_back(l);
                merged.push_back("=======");
                for (auto& l : tv)
                    merged.push_back(l);
                merged.push_back(">>>>>>> projeny (local changes)");
            }
            i = u_end;
            ++p;
            ++q;
        } else {
            // No covering change but loop did not advance (e.g. change
            // starts beyond i and next==i miscomputed): copy one line.
            if (i < base.lines.size()) {
                merged.push_back(base.lines[i]);
                ++i;
            } else {
                break;
            }
        }
    }
    make_dirs(dirname_of(dst_path));
    // Sample the exec vote before any write: ours may alias dst (all
    // in-place merges pass ours_file == dst_path).
    int exec_want = merged_exec_want();
    mode_t exec_tmpl = merged_mode_template();
    if (!clean) {
        std::string out;
        for (auto& l : merged) {
            out += l;
            out += '\n';
        }
        write_file_bytes(dst_path, out);
        restore_merged_exec(dst_path, exec_want, exec_tmpl);
        return false;
    }
    // Clean: resolve trailing-newline flag three-way on the flag alone.
    bool flag = base.ends_nl;
    if (ours.ends_nl == theirs.ends_nl)
        flag = ours.ends_nl;
    else if (ours.ends_nl == base.ends_nl)
        flag = theirs.ends_nl;
    else if (theirs.ends_nl == base.ends_nl)
        flag = ours.ends_nl;
    write_file_bytes(dst_path, join_content(merged, flag));
    restore_merged_exec(dst_path, exec_want, exec_tmpl);
    return true;
}
