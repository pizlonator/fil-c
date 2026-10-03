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
// Uses only the C++ standard library and POSIX for the utilities here.
// (The projeny binary as a whole links libcurl and compiles in the
// vendored blake3 (src/blake3/) for URL: archive downloads — see
// download.h/download.cc; this header's helpers remain third-party-free.)
#pragma once

#include <exception>
#include <functional>
#include <mutex>
#include <string>
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

// Thrown by die(). Lets parallel workers isolate per-project hard errors:
// worker threads catch it, record the failure, and keep other projects
// running; main() catches it for the single-project exit(1) behavior.
// message() is the error message itself (no "projeny: error: " prefix, no
// detail): die() has already printed the full report to stderr by the time
// it throws.
class ProjenyFatalError : public std::exception {
  public:
    explicit ProjenyFatalError(std::string message);
    const char* what() const noexcept override;
    const std::string& message() const noexcept;
  private:
    std::string message_;
};

// Serializes note/warn/die and download progress printing so parallel
// workers never interleave partial lines. Every message projeny prints to
// stderr goes through a fprintf under one lock of this mutex.
extern std::mutex g_output_mutex;

// Print "projeny: error: <msg>" (plus optional detail, verbatim) to stderr —
// exactly the bytes die() has always printed — and then throw
// ProjenyFatalError(msg) instead of exiting. Temp dirs are NOT removed here:
// in parallel mode other threads own their own temp dirs, so the sweep moves
// to cleanup_tempdirs(), which main() runs after catching (all threads have
// joined by then). Callers keep treating die() as noreturn.
[[noreturn]] void die(const std::string& msg, const std::string& detail = "");
// Both lock g_output_mutex for their whole line and prefix the thread's
// output label (see set_output_label) after the "projeny:" prefix:
// "projeny: [<label>] warning: <msg>" / "projeny: [<label>] <msg>".
void warn(const std::string& msg);
// Print "projeny: <msg>" to stderr: informational output that is neither an
// error nor a warning (download announcements, verification notes), so the
// "projeny:" prefix stays uniform across every message we print.
void note(const std::string& msg);

// When non-empty, note/warn/die prefix messages with "[<label>] " after the
// "projeny:" prefix — "projeny: [<label>] <msg>", "projeny: [<label>]
// warning: <msg>", "projeny: [<label>] error: <msg>" — so an error stays one
// greppable line while remaining attributable to its project. Set per-thread
// by parallel workers (each worker labels itself with the project it is
// running); empty on the main thread, whose output stays byte-identical to
// the single-project behavior. The label lives in thread_local storage: no
// locking, and a worker's label never leaks into another thread.
void set_output_label(const std::string& label);   // sets thread_local label
const std::string& output_label();                 // reads it

// Run `ntasks` work items across at most `nthreads` std::threads
// (nthreads >= 1; the actual thread count is min(nthreads, ntasks), and 0
// tasks means no threads at all). All threads are joined before return.
// Exceptions from tasks are captured and, after the join, the one from the
// lowest task index is rethrown. Used for parallel hash checks and
// per-project parallel work. Note a task that dies() is exactly this case:
// the ProjenyFatalError lands here, the remaining tasks still run to
// completion, and the throw surfaces once the pool has quiesced.
void run_parallel(int nthreads, size_t ntasks, const std::function<void(size_t)>& task);

// Whole-file binary-safe IO. Reading a missing file dies.
std::string read_file_bytes(const std::string& path);
bool try_read_file_bytes(const std::string& path, std::string* out);
void write_file_bytes(const std::string& path, const std::string& data);
void copy_file_bytes(const std::string& src, const std::string& dst);
// Try-variant of copy_file_bytes for best-effort callers: same
// temp-file+rename protocol as write_file_bytes, but returns false
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

// Split a path into its components, dropping empty and "." components;
// ".." components are kept verbatim when keep_dots is true. Used by the
// lexical path algebra (normalize_lexical, rel_to_cwd).
std::vector<std::string> split_path_components(const std::string& p,
                                               bool keep_dots);

// Read a symlink's target, retrying when the link grows between the lstat
// size hint and readlink (a single read would silently truncate). Dies on
// failure; callers invoke it for a path they just saw as a symlink, so an
// error here is a race.
std::string read_link_target(const std::string& path);

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

// Physical (symlink-resolved) form of `path` via realpath(3). Dies on
// failure: callers invoke it only for paths that already exist on disk, so
// an error here is a race (the path vanished) or an unreadable ancestor.
std::string physical_path(const std::string& path);

// System scratch parent for temp dirs/files that must never live inside a
// workdir (crashed runs would otherwise pollute the next diff): $TMPDIR when
// it names an existing absolute directory, else /tmp.
std::string system_scratch_parent();

// Temp dir tracking so hard-error cleanup can remove even still-registered
// dirs once every thread has joined. The registry is shared by all threads
// (parallel workers register their own temp dirs), so register/unregister/
// cleanup serialize internally.
void register_tempdir(const std::string& path);
void unregister_tempdir(const std::string& path);
// Remove every still-registered temp dir (deepest-first, so children go
// before parents) and forget the registry. Called by main() on the hard-
// error paths — after all worker threads have joined, so no other thread can
// be using (or removing) a registered dir concurrently. Missing paths are
// skipped silently: an unwinding TempDir may already have removed its own.
void cleanup_tempdirs();

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

// Remove `path` recursively (rm -rf): files, symlinks, and directory trees.
// True on success; a missing path is success. When `err` is non-null it
// receives the FIRST failing syscall's strerror-style reason (naming the
// path) while the removal of everything else continues — a caller that must
// press on after a failed deletion (erase-setup) uses it to report what
// went wrong without stopping.
bool remove_recursive(const std::string& path, std::string* err = nullptr);
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
