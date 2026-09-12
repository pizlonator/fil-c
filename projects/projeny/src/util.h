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
// projeny - project tarball+patch manager.
// Uses only the C++ standard library and POSIX. No third-party dependencies.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include <cstdint>

// Result of running a child process. `output` merges stdout and stderr.
struct CmdResult {
    int code = -1;
    std::string output;
};

// Run argv[0] with args (no shell). cwd "" means inherit. Exactly one of
// stdin_data / stdin_file may be provided to feed the child's stdin.
// extra_env entries look like "KEY=VALUE" and are set in the child.
CmdResult run_cmd(const std::vector<std::string>& argv, const std::string& cwd = "",
                  const std::string& stdin_data = "", const std::string& stdin_file = "",
                  const std::vector<std::string>& extra_env = std::vector<std::string>());

// Print "projeny: error: <msg>" (plus optional detail) and exit(1).
// Also removes any registered temp dirs first.
[[noreturn]] void die(const std::string& msg, const std::string& detail = "");
void warn(const std::string& msg);

// Whole-file binary-safe IO. Reading a missing file dies.
std::string read_file_bytes(const std::string& path);
bool try_read_file_bytes(const std::string& path, std::string* out);
void write_file_bytes(const std::string& path, const std::string& data);
void copy_file_bytes(const std::string& src, const std::string& dst);
// Try-variant of copy_file_bytes for best-effort callers: same
// temp-file+fsync+rename protocol as write_file_bytes, but returns false
// instead of dying (*err, when non-null, receives a strerror-style reason).
bool try_copy_file_bytes(const std::string& src, const std::string& dst,
                         std::string* err = nullptr);
// FNV-1a 64-bit content hash (hex) and byte size of a file. Used to detect
// "same basename, different content" tarballs in rebase; dies on IO errors.
std::string file_hash_hex(const std::string& path);
uint64_t file_size_bytes(const std::string& path);
bool path_exists(const std::string& p);
bool is_dir(const std::string& p);

std::string join_path(const std::string& a, const std::string& b);
std::string basename_of(const std::string& p);
std::string dirname_of(const std::string& p); // "" and bare names -> "."
std::string strip_trailing_slashes(const std::string& p);
// Same directory, basename prefixed with '.': "dir/f.projeny" ->
// "dir/.f.projeny"; a bare "f.projeny" becomes ".f.projeny". Used for the
// canonical dot-prefixed status-file and snapshot names (hidden files that
// never clutter directory listings or accidental checkins).
std::string dotname(const std::string& p);

// Split on '\n'. A trailing '\n' does not produce a final empty element.
// Strips a trailing '\r' from each line (tolerates CRLF input).
std::vector<std::string> split_lines(const std::string& s);
std::string join_lines(const std::vector<std::string>& lines); // adds '\n' after each

std::string ltrim(const std::string& s);
std::string rtrim(const std::string& s);
std::string trim(const std::string& s);
bool starts_with(const std::string& s, const std::string& pfx);
bool ends_with(const std::string& s, const std::string& sfx);

std::string get_cwd();
std::string absolutize(const std::string& p); // lexical, based on get_cwd()
// Lexically normalize: collapse ".", duplicate slashes; ".." pops textually.
std::string normalize_lexical(const std::string& p);

// Outcome of lexically resolving a symlink/hardlink target against a tree
// root.
enum class LinkResolve {
    Inside,   // resolves to a path inside the tree
    Absolute, // the target itself is absolute
    Escapes,  // normalization climbs above the tree root
};

// Lexically resolve a link target for a member whose directory (relative to
// the tree root) is base_dir ("" or "." for the root itself). Absolute
// targets report Absolute. Otherwise base_dir/target is normalized
// component-wise: empty and "." components are dropped and ".." pops the
// component stack; popping an empty stack means the target climbs above the
// tree root and reports Escapes. *resolved always receives the target's path
// relative to the tree root (with leading ".." components when it escapes),
// for use in diagnostics.
LinkResolve resolve_link_target(const std::string& base_dir,
                                const std::string& target,
                                std::string* resolved);

// Create a unique temp dir parent/prefixXXXXXX (mkdtemp). Dies on failure.
std::string make_tempdir(const std::string& parent, const std::string& prefix);
// System scratch parent for temp dirs/files that must never live inside a
// workdir (crashed runs would otherwise pollute the next diff): $TMPDIR when
// it names an existing absolute directory, else /tmp.
std::string system_scratch_parent();
// Create a temp file containing data; returns path (caller removes it).
std::string write_temp_input(const std::string& parent, const std::string& prefix,
                             const std::string& data);

// Temp dir tracking so die() can clean up even on hard-error paths.
void register_tempdir(const std::string& path);
void unregister_tempdir(const std::string& path);

// RAII temp dir. Removes the tree on destruction unless released().
class TempDir {
  public:
    TempDir(const std::string& parent, const std::string& prefix);
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    void release();
    std::string path;
  private:
    bool owned_ = false;
};

bool remove_recursive(const std::string& path); // true on success; missing -> true
void fsync_dir(const std::string& path); // persist dir entries, dies on failure
void make_dirs(const std::string& path);        // mkdir -p, dies on failure
std::vector<std::string> list_dir_names(const std::string& path); // sorted, no . / ..
void move_path(const std::string& src, const std::string& dst);   // rename(2), dies
void copy_recursive(const std::string& src, const std::string& dst); // cp -a, dies
// Copy one path preserving its kind: symlinks are recreated (never
// followed), directories copy recursively, regular files copy bytes.
// Anything else (FIFOs, sockets, devices) is a hard error. Dies on failure.
void copy_path_preserving(const std::string& src, const std::string& dst);

bool contains_nul(const std::string& s);

// Standard MIME base64 (A-Za-z0-9+/ with = padding) for binary patch
// payloads. encode splits nothing (callers wrap at 76 columns); decode
// rejects any character outside the alphabet/padding (whitespace included:
// callers concatenate lines first). Returns false on invalid input.
std::string base64_encode(const std::string& data);
bool base64_decode(const std::string& s, std::string* out);

// Copy the atime/mtime from `src` onto `dst` (best-effort for symlinks,
// which use AT_SYMLINK_NOFOLLOW and ignore failures; dies for regular
// files). Used so staged copies (package/extract) and clean merges keep
// the source timestamps instead of stamping "now", which would trigger
// spurious rebuilds (e.g. libffi's doc step).
void preserve_file_times(const std::string& src, const std::string& dst);

// Status-file path escaping: workdir-relative paths are stored one-per-line,
// so backslash, newline and carriage return are backslash-escaped; unescape()
// reverses it. '>' is also escaped, so the "Renamed: <src> -> <dst>" split
// (first raw " -> ") can never hit separator text inside a filename.
// Escaped paths never contain a raw newline.
std::string escape_status_path(const std::string& p);
std::string unescape_status_path(const std::string& e);

// git-path quoting helpers for rewriting diff labels.
std::string unquote_git_path(const std::string& p);
std::string quote_git_path(const std::string& p);

// Replace the value of a "Key: ..." line inside a header block, or prepend
// "Key: value\n" if the key is absent. Matches lines starting with "Key:".
std::string replace_header_value(const std::string& head, const std::string& key,
                                 const std::string& value);
