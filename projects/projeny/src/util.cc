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
#include "util.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace {
std::vector<std::string> g_tempdirs;
}

void register_tempdir(const std::string& path)
{
    g_tempdirs.push_back(path);
}

void unregister_tempdir(const std::string& path)
{
    g_tempdirs.erase(std::remove(g_tempdirs.begin(), g_tempdirs.end(), path),
                     g_tempdirs.end());
}

void die(const std::string& msg, const std::string& detail)
{
    for (auto it = g_tempdirs.rbegin(); it != g_tempdirs.rend(); ++it)
        remove_recursive(*it);
    fprintf(stderr, "projeny: error: %s\n", msg.c_str());
    if (!detail.empty())
        fprintf(stderr, "%s", detail.c_str());
    exit(1);
}

void warn(const std::string& msg)
{
    fprintf(stderr, "projeny: warning: %s\n", msg.c_str());
}

namespace {
// (child-side errors are reported via the CmdResult output string.)
}

CmdResult run_cmd(const std::vector<std::string>& argv, const std::string& cwd,
                  const std::string& stdin_data, const std::string& stdin_file,
                  const std::vector<std::string>& extra_env)
{
    CmdResult out;
    if (argv.empty()) {
        out.code = 127;
        return out;
    }
    if (!stdin_data.empty() && !stdin_file.empty())
        die("internal error: stdin given twice");

    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    bool use_stdin_data = !stdin_data.empty();
    if (use_stdin_data) {
        if (pipe(in_pipe) != 0)
            die("pipe() failed: " + std::string(strerror(errno)));
    }
    if (pipe(out_pipe) != 0) {
        if (use_stdin_data) {
            close(in_pipe[0]);
            close(in_pipe[1]);
        }
        die("pipe() failed: " + std::string(strerror(errno)));
    }
    // Avoid leaking pipe fds into the child for unrelated fds; the two we
    // dup2 in the child are handled explicitly below.
    fcntl(out_pipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(out_pipe[1], F_SETFD, FD_CLOEXEC);
    if (use_stdin_data) {
        fcntl(in_pipe[0], F_SETFD, FD_CLOEXEC);
        fcntl(in_pipe[1], F_SETFD, FD_CLOEXEC);
    }

    std::vector<char*> c_argv;
    c_argv.reserve(argv.size() + 1);
    for (const auto& a : argv)
        c_argv.push_back(const_cast<char*>(a.c_str()));
    c_argv.push_back(nullptr);

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    int devnull = -1;
    if (!cwd.empty())
        posix_spawn_file_actions_addchdir_np(&fa, cwd.c_str());
    if (use_stdin_data) {
        posix_spawn_file_actions_adddup2(&fa, in_pipe[0], STDIN_FILENO);
        posix_spawn_file_actions_addclose(&fa, in_pipe[0]);
        posix_spawn_file_actions_addclose(&fa, in_pipe[1]);
    } else if (!stdin_file.empty()) {
        posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, stdin_file.c_str(),
                                         O_RDONLY, 0);
    } else {
        devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            posix_spawn_file_actions_adddup2(&fa, devnull, STDIN_FILENO);
            posix_spawn_file_actions_addclose(&fa, devnull);
        }
    }
    posix_spawn_file_actions_adddup2(&fa, out_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&fa, out_pipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&fa, out_pipe[0]);
    posix_spawn_file_actions_addclose(&fa, out_pipe[1]);

    std::vector<std::string> env_strings;
    std::vector<char*> c_env;
    char** use_environ = environ;
    if (!extra_env.empty()) {
        for (char** e = environ; *e; ++e)
            env_strings.push_back(*e);
        for (const auto& kv : extra_env) {
            size_t eq = kv.find('=');
            std::string key = eq == std::string::npos ? kv : kv.substr(0, eq);
            env_strings.erase(
                std::remove_if(env_strings.begin(), env_strings.end(),
                               [&](const std::string& e) {
                                   return e.size() > key.size() && e[key.size()] == '=' &&
                                          e.compare(0, key.size(), key) == 0;
                               }),
                env_strings.end());
            env_strings.push_back(kv);
        }
        c_env.reserve(env_strings.size() + 1);
        for (auto& e : env_strings)
            c_env.push_back(const_cast<char*>(e.c_str()));
        c_env.push_back(nullptr);
        use_environ = c_env.data();
    }

    pid_t pid = -1;
    int sr = posix_spawnp(&pid, c_argv[0], &fa, nullptr, c_argv.data(), use_environ);
    posix_spawn_file_actions_destroy(&fa);
    if (devnull >= 0)
        close(devnull);
    if (sr != 0) {
        close(out_pipe[0]);
        close(out_pipe[1]);
        if (use_stdin_data) {
            close(in_pipe[0]);
            close(in_pipe[1]);
        }
        if (sr == ENOENT) {
            out.code = 127;
            out.output =
                "projeny: error: required helper '" + argv[0] + "' not found in PATH\n";
            return out;
        }
        out.code = 127;
        out.output =
            "projeny: error: failed to spawn '" + argv[0] + "': " + strerror(sr) + "\n";
        return out;
    }
    close(out_pipe[1]);
    if (use_stdin_data)
        close(in_pipe[0]);

    if (use_stdin_data) {
        size_t off = 0;
        while (off < stdin_data.size()) {
            ssize_t w = write(in_pipe[1], stdin_data.data() + off,
                              stdin_data.size() - off);
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EPIPE)
                    break; // child exited early; collect output below
                break;
            }
            off += (size_t)w;
        }
        close(in_pipe[1]);
    }

    std::string output;
    char buf[65536];
    for (;;) {
        ssize_t r = read(out_pipe[0], buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (r == 0)
            break;
        output.append(buf, (size_t)r);
    }
    close(out_pipe[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        out.code = 127;
        out.output = output;
        return out;
    }
    if (WIFEXITED(status))
        out.code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        out.code = 128 + WTERMSIG(status);
    else
        out.code = 127;
    out.output = output;
    return out;
}

std::string read_file_bytes(const std::string& path)
{
    std::string out;
    if (!try_read_file_bytes(path, &out))
        die("cannot read file '" + path + "': " + strerror(errno));
    return out;
}

bool try_read_file_bytes(const std::string& path, std::string* out)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    std::string data;
    char buf[65536];
    for (;;) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR)
                continue;
            close(fd);
            return false;
        }
        if (r == 0)
            break;
        data.append(buf, (size_t)r);
    }
    close(fd);
    *out = data;
    return true;
}

namespace {

// Try-variant of write_file_bytes: the same write-temp + fsync + rename
// protocol (a crash never leaves a half-written file), but reports failure
// via the return value (strerror reason in *err when non-null) instead of
// dying. Used by write_file_bytes and the best-effort try_copy_file_bytes.
bool write_file_bytes_try(const std::string& path, const std::string& data,
                          std::string* err)
{
    auto fail = [&err](int e) {
        if (err != nullptr)
            *err = strerror(e);
        return false;
    };
    // Write-then-rename so a crash never leaves a half-written file behind.
    std::string tmp = path + ".tmp";
    int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return fail(errno);
    size_t off = 0;
    while (off < data.size()) {
        ssize_t w = write(fd, data.data() + off, data.size() - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            int e = errno;
            close(fd);
            unlink(tmp.c_str());
            return fail(e);
        }
        off += (size_t)w;
    }
    // fsync before rename so the data is durable on disk (not just in the
    // page cache) when the rename makes it visible; crash recovery then
    // only ever sees complete files.
    if (fsync(fd) != 0) {
        int e = errno;
        close(fd);
        unlink(tmp.c_str());
        return fail(e);
    }
    if (close(fd) != 0) {
        int e = errno;
        unlink(tmp.c_str());
        return fail(e);
    }
    if (rename(tmp.c_str(), path.c_str()) != 0) {
        int e = errno;
        unlink(tmp.c_str());
        return fail(e);
    }
    return true;
}

} // namespace

void write_file_bytes(const std::string& path, const std::string& data)
{
    std::string err;
    if (!write_file_bytes_try(path, data, &err))
        die("cannot write file '" + path + "': " + err);
}

void fsync_dir(const std::string& path)
{
    // Persist a directory entry itself (fsync the dir fd) so renames into
    // it survive a crash. Best-effort on filesystems that reject dir fsync
    // (EINVAL): the rename itself is still ordered after the fsynced file
    // data by write_file_bytes, so recovery only ever sees complete files.
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        die("cannot open directory '" + path + "': " + strerror(errno));
    if (fsync(fd) != 0 && errno != EINVAL) {
        int e = errno;
        close(fd);
        die("cannot fsync directory '" + path + "': " + strerror(e));
    }
    close(fd);
}

void copy_file_bytes(const std::string& src, const std::string& dst)
{
    write_file_bytes(dst, read_file_bytes(src));
}

bool try_copy_file_bytes(const std::string& src, const std::string& dst,
                         std::string* err)
{
    std::string data;
    if (!try_read_file_bytes(src, &data)) {
        if (err != nullptr)
            *err = strerror(errno);
        return false;
    }
    return write_file_bytes_try(dst, data, err);
}

// FNV-1a 64-bit content hash (streamed, binary-safe). Used only to detect
// "same basename, different content" tarballs in rebase — not cryptographic.
std::string file_hash_hex(const std::string& path)
{
    uint64_t h = 1469598103934665603ULL;
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        die("cannot read file '" + path + "': " + strerror(errno));
    char buf[65536];
    for (;;) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR)
                continue;
            int e = errno;
            close(fd);
            die("cannot read file '" + path + "': " + strerror(e));
        }
        if (r == 0)
            break;
        for (ssize_t i = 0; i < r; ++i) {
            h ^= (uint64_t)(unsigned char)buf[i];
            h *= 1099511628211ULL;
        }
    }
    close(fd);
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%016llx", (unsigned long long)h);
    return tmp;
}

uint64_t file_size_bytes(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        die("cannot stat file '" + path + "': " + strerror(errno));
    return (uint64_t)st.st_size;
}

bool path_exists(const std::string& p)
{
    struct stat st;
    return lstat(p.c_str(), &st) == 0;
}

bool is_dir(const std::string& p)
{
    struct stat st;
    if (lstat(p.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

std::string join_path(const std::string& a, const std::string& b)
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

std::string basename_of(const std::string& p)
{
    std::string s = strip_trailing_slashes(p);
    size_t i = s.rfind('/');
    if (i == std::string::npos)
        return s;
    return s.substr(i + 1);
}

std::string dirname_of(const std::string& p)
{
    std::string s = strip_trailing_slashes(p);
    size_t i = s.rfind('/');
    if (i == std::string::npos)
        return ".";
    if (i == 0)
        return "/";
    return s.substr(0, i);
}

std::string strip_trailing_slashes(const std::string& p)
{
    size_t n = p.size();
    while (n > 1 && p[n - 1] == '/')
        --n;
    return p.substr(0, n);
}

std::string dotname(const std::string& p)
{
    // Same directory, basename prefixed with '.': "dir/f.projeny" ->
    // "dir/.f.projeny"; a bare "f.projeny" has no directory component and
    // becomes ".f.projeny". Degenerate paths (/, ., ..) come back unchanged
    // — callers only pass real file names.
    std::string s = strip_trailing_slashes(p);
    std::string base = basename_of(s);
    if (base.empty() || base == "." || base == "..")
        return s;
    std::string dir = dirname_of(s);
    if (dir == "." || dir.empty())
        return "." + base;
    return join_path(dir, "." + base);
}

std::vector<std::string> split_lines(const std::string& s)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find('\n', i);
        if (j == std::string::npos) {
            out.push_back(s.substr(i));
            i = s.size();
        } else {
            out.push_back(s.substr(i, j - i));
            i = j + 1;
        }
    }
    for (auto& l : out) {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
    }
    return out;
}

std::string join_lines(const std::vector<std::string>& lines)
{
    std::string out;
    for (const auto& l : lines) {
        out += l;
        out += '\n';
    }
    return out;
}

std::string ltrim(const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
    return s.substr(i);
}

std::string rtrim(const std::string& s)
{
    size_t n = s.size();
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
        --n;
    return s.substr(0, n);
}

std::string trim(const std::string& s)
{
    return rtrim(ltrim(s));
}

bool starts_with(const std::string& s, const std::string& pfx)
{
    return s.size() >= pfx.size() && s.compare(0, pfx.size(), pfx) == 0;
}

bool ends_with(const std::string& s, const std::string& sfx)
{
    return s.size() >= sfx.size() &&
           s.compare(s.size() - sfx.size(), sfx.size(), sfx) == 0;
}

std::string get_cwd()
{
    char buf[8192];
    if (!getcwd(buf, sizeof(buf)))
        die("cannot determine current directory: " + std::string(strerror(errno)));
    return buf;
}

std::string absolutize(const std::string& p)
{
    if (!p.empty() && p[0] == '/')
        return normalize_lexical(p);
    return normalize_lexical(join_path(get_cwd(), p));
}

std::string normalize_lexical(const std::string& p)
{
    if (p.empty())
        return ".";
    bool absolute = p[0] == '/';
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        std::string comp = (j == std::string::npos) ? p.substr(i)
                                                    : p.substr(i, j - i);
        if (j == std::string::npos)
            i = p.size() + 1;
        else
            i = j + 1;
        if (comp.empty() || comp == ".")
            continue;
        if (comp == "..") {
            if (!parts.empty() && parts.back() != "..") {
                // For absolute paths, ".." at root is a no-op (stays at /).
                parts.pop_back();
            } else if (!absolute) {
                parts.push_back("..");
            }
            continue;
        }
        parts.push_back(comp);
    }
    std::string out;
    if (absolute)
        out = "/";
    for (size_t k = 0; k < parts.size(); ++k) {
        if (k > 0)
            out += "/";
        out += parts[k];
    }
    if (out.empty())
        out = absolute ? "/" : ".";
    return out;
}

LinkResolve resolve_link_target(const std::string& base_dir,
                                const std::string& target,
                                std::string* resolved)
{
    resolved->clear();
    if (target.empty())
        return LinkResolve::Inside;
    if (target[0] == '/') {
        *resolved = target;
        return LinkResolve::Absolute;
    }
    // Normalize the member's directory first so the target is resolved
    // against a clean tree-relative base. Bases are validated upstream (no
    // "..", never absolute), but normalize them anyway so a malformed base
    // can never make a target look shallower than it is.
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= base_dir.size()) {
        size_t j = base_dir.find('/', i);
        std::string comp = (j == std::string::npos)
                               ? base_dir.substr(i)
                               : base_dir.substr(i, j - i);
        if (j == std::string::npos)
            i = base_dir.size() + 1;
        else
            i = j + 1;
        if (comp.empty() || comp == ".")
            continue;
        if (comp == "..") {
            if (!parts.empty())
                parts.pop_back();
            continue;
        }
        parts.push_back(comp);
    }
    // Now fold in the target: "." is skipped and ".." pops; popping past the
    // tree root is the escape we refuse.
    bool escapes = false;
    i = 0;
    while (i <= target.size()) {
        size_t j = target.find('/', i);
        std::string comp = (j == std::string::npos) ? target.substr(i)
                                                    : target.substr(i, j - i);
        if (j == std::string::npos)
            i = target.size() + 1;
        else
            i = j + 1;
        if (comp.empty() || comp == ".")
            continue;
        if (comp == "..") {
            if (!parts.empty() && parts.back() != "..") {
                parts.pop_back();
            } else {
                escapes = true;
                parts.push_back("..");
            }
            continue;
        }
        parts.push_back(comp);
    }
    std::string out;
    for (size_t k = 0; k < parts.size(); ++k) {
        if (k > 0)
            out += "/";
        out += parts[k];
    }
    if (out.empty())
        out = ".";
    *resolved = out;
    return escapes ? LinkResolve::Escapes : LinkResolve::Inside;
}

std::string system_scratch_parent()
{
    const char* tmp = getenv("TMPDIR");
    if (tmp && tmp[0] == '/' && is_dir(tmp))
        return tmp;
    return "/tmp";
}

std::string make_tempdir(const std::string& parent, const std::string& prefix) {
    make_dirs(parent);
    std::string tmpl = join_path(parent, prefix + "XXXXXX");
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    if (!mkdtemp(buf.data()))
        die("cannot create temp dir in '" + parent + "': " + strerror(errno));
    return buf.data();
}

std::string write_temp_input(const std::string& parent, const std::string& prefix,
                             const std::string& data)
{
    make_dirs(parent);
    std::string tmpl = join_path(parent, prefix + "XXXXXX");
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    if (fd < 0)
        die("cannot create temp file in '" + parent + "': " + strerror(errno));
    size_t off = 0;
    while (off < data.size()) {
        ssize_t w = write(fd, data.data() + off, data.size() - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            int e = errno;
            close(fd);
            die("cannot write temp file: " + std::string(strerror(e)));
        }
        off += (size_t)w;
    }
    close(fd);
    // Return an absolute path: callers pass this to children that run with a
    // different cwd (git apply runs with cwd=treedir), where a relative
    // path would resolve to the wrong place.
    return absolutize(buf.data());
}

TempDir::TempDir(const std::string& parent, const std::string& prefix)
    : path(make_tempdir(parent, prefix)), owned_(true)
{
    register_tempdir(path);
}

TempDir::~TempDir()
{
    if (owned_)
        remove_recursive(path);
}

void TempDir::release()
{
    if (owned_) {
        unregister_tempdir(path);
        owned_ = false;
    }
}

bool remove_recursive(const std::string& path)
{
    struct stat st;
    if (lstat(path.c_str(), &st) != 0)
        return errno == ENOENT;
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(path.c_str()) != 0)
            return false;
        return true;
    }
    DIR* d = opendir(path.c_str());
    if (!d)
        return false;
    bool ok = true;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n == "." || n == "..")
            continue;
        if (!remove_recursive(join_path(path, n)))
            ok = false;
    }
    closedir(d);
    if (rmdir(path.c_str()) != 0)
        ok = false;
    return ok;
}

void make_dirs(const std::string& path)
{
    if (path.empty() || path == "." || path_exists(path))
        return;
    std::string cur;
    bool absolute = !path.empty() && path[0] == '/';
    if (absolute)
        cur = "/";
    size_t i = absolute ? 1 : 0;
    std::string acc = cur;
    while (i <= path.size()) {
        size_t j = path.find('/', i);
        std::string comp;
        if (j == std::string::npos) {
            comp = path.substr(i);
            i = path.size() + 1;
        } else {
            comp = path.substr(i, j - i);
            i = j + 1;
        }
        if (comp.empty() || comp == ".")
            continue;
        if (comp == "..") {
            if (!acc.empty() && acc != "/") {
                size_t k = acc.find_last_of('/', acc.size() - 2);
                acc = (k == std::string::npos) ? "" : acc.substr(0, k + 1);
            }
            continue;
        }
        if (!acc.empty() && acc.back() != '/')
            acc += '/';
        acc += comp;
        if (mkdir(acc.c_str(), 0777) != 0 && errno != EEXIST)
            die("cannot create directory '" + acc + "': " + strerror(errno));
    }
}

std::vector<std::string> list_dir_names(const std::string& path)
{
    DIR* d = opendir(path.c_str());
    if (!d)
        die("cannot list directory '" + path + "': " + strerror(errno));
    std::vector<std::string> out;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n == "." || n == "..")
            continue;
        out.push_back(n);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

void move_path(const std::string& src, const std::string& dst)
{
    if (rename(src.c_str(), dst.c_str()) == 0)
        return;
    if (errno == EXDEV) {
        // rename(2) cannot cross filesystems (e.g. scratch in /tmp, workdir
        // elsewhere): fall back to copy + remove.
        copy_recursive(src, dst);
        if (!remove_recursive(src))
            die("cannot remove '" + src + "' after cross-device move");
        return;
    }
    die("cannot rename '" + src + "' to '" + dst + "': " + strerror(errno));
}

void copy_recursive(const std::string& src, const std::string& dst)
{
    CmdResult r = run_cmd({"cp", "-a", "--", src, dst});
    if (r.code != 0)
        die("failed to copy '" + src + "' to '" + dst + "'", r.output);
}

void copy_path_preserving(const std::string& src, const std::string& dst)
{
    struct stat st;
    if (lstat(src.c_str(), &st) != 0)
        die("cannot stat '" + src + "': " + strerror(errno));
    if (S_ISLNK(st.st_mode)) {
        std::vector<char> buf(st.st_size > 0 ? (size_t)st.st_size + 1 : 4096);
        ssize_t r = readlink(src.c_str(), buf.data(), buf.size());
        if (r < 0)
            die("cannot read link '" + src + "': " + strerror(errno));
        std::string target(buf.data(), (size_t)r);
        make_dirs(dirname_of(dst));
        // Never follow: replace whatever sits at dst (file or link).
        if (path_exists(dst) && !remove_recursive(dst))
            die("cannot remove '" + dst + "' before recreating symlink");
        if (symlink(target.c_str(), dst.c_str()) != 0)
            die("cannot create symlink '" + dst + "': " + strerror(errno));
        preserve_file_times(src, dst);
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        copy_recursive(src, dst);
        return;
    }
    if (S_ISREG(st.st_mode)) {
        copy_file_bytes(src, dst);
        // write_file_bytes creates the destination with 0666 & ~umask, so
        // restore the source permission bits (notably the executable bit)
        // that `cp -a` would have kept. Without this, package/extract and
        // other staged copies silently drop +x (e.g. configure). Mask with
        // 0777: the full rwx bits are preserved (0700 stays 0700, 0640 stays
        // 0640) but setuid/setgid/sticky are never propagated into staged
        // package/extract payloads.
        if (chmod(dst.c_str(), (mode_t)(st.st_mode & 0777)) != 0)
            die("cannot set permissions on '" + dst + "': " + strerror(errno));
        // Keep the source timestamps so staged copies (package/extract) and
        // pending-op replays don't stamp "now" onto unmodified files (which
        // would trigger spurious rebuilds, e.g. libffi's doc step).
        preserve_file_times(src, dst);
        return;
    }
    die("cannot copy '" + src + "': unsupported file type; only regular files, "
        "symlinks and directories are supported");
}

bool contains_nul(const std::string& s)
{
    return s.find(char(0)) != std::string::npos;
}

std::string base64_encode(const std::string& data)
{
    static const char* tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        unsigned v = (unsigned char)data[i] << 16;
        size_t n = 1;
        if (i + 1 < data.size()) {
            v |= (unsigned char)data[i + 1] << 8;
            n = 2;
        }
        if (i + 2 < data.size()) {
            v |= (unsigned char)data[i + 2];
            n = 3;
        }
        out += tab[(v >> 18) & 63];
        out += tab[(v >> 12) & 63];
        out += (n >= 2) ? tab[(v >> 6) & 63] : '=';
        out += (n >= 3) ? tab[v & 63] : '=';
    }
    return out;
}

bool base64_decode(const std::string& s, std::string* out)
{
    static signed char rev[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i)
            rev[i] = -1;
        const char* tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; tab[i]; ++i)
            rev[(unsigned char)tab[i]] = (signed char)i;
        init = true;
    }
    out->clear();
    if (s.empty())
        return true;
    if (s.size() % 4 != 0)
        return false;
    for (size_t i = 0; i < s.size(); i += 4) {
        int vals[4];
        int pad = 0;
        for (int k = 0; k < 4; ++k) {
            unsigned char c = (unsigned char)s[i + (size_t)k];
            if (c == '=') {
                vals[k] = 0;
                ++pad;
            } else {
                if (rev[c] < 0)
                    return false;
                vals[k] = rev[c];
                if (pad > 0)
                    return false; // padding must be trailing
            }
        }
        if (pad > 2)
            return false;
        unsigned v = (unsigned)vals[0] << 18 | (unsigned)vals[1] << 12 |
                     (unsigned)vals[2] << 6 | (unsigned)vals[3];
        out->push_back((char)((v >> 16) & 0xff));
        if (pad < 2)
            out->push_back((char)((v >> 8) & 0xff));
        if (pad < 1)
            out->push_back((char)(v & 0xff));
    }
    return true;
}

void preserve_file_times(const std::string& src, const std::string& dst)
{
    struct stat sst;
    if (lstat(src.c_str(), &sst) != 0)
        die("cannot stat '" + src + "': " + strerror(errno));
    struct timespec ts[2];
#if defined(__APPLE__)
    ts[0] = sst.st_atimespec;
    ts[1] = sst.st_mtimespec;
#else
    ts[0] = sst.st_atim;
    ts[1] = sst.st_mtim;
#endif
    int flags = S_ISLNK(sst.st_mode) ? AT_SYMLINK_NOFOLLOW : 0;
    if (utimensat(AT_FDCWD, dst.c_str(), ts, flags) != 0) {
        if (S_ISLNK(sst.st_mode))
            return; // best-effort for links (timestamps rarely matter)
        die("cannot set timestamps on '" + dst + "': " + strerror(errno));
    }
}

std::string escape_status_path(const std::string& p)
{
    std::string out;
    for (char c : p) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '>': out += "\\>"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string unescape_status_path(const std::string& e)
{
    std::string out;
    for (size_t i = 0; i < e.size(); ++i) {
        if (e[i] == '\\' && i + 1 < e.size()) {
            char n = e[i + 1];
            if (n == '\\') {
                out += '\\';
                ++i;
            } else if (n == 'n') {
                out += '\n';
                ++i;
            } else if (n == 'r') {
                out += '\r';
                ++i;
            } else if (n == '>') {
                out += '>';
                ++i;
            } else {
                // Unknown escape: treat the backslash literally so the
                // mapping stays total (encoders never emit this form).
                out += '\\';
            }
        } else {
            out += e[i];
        }
    }
    return out;
}

std::string unquote_git_path(const std::string& p)
{
    if (p.size() >= 2 && p.front() == '"' && p.back() == '"') {
        std::string out;
        for (size_t i = 1; i + 1 < p.size(); ++i) {
            if (p[i] == '\\' && i + 1 < p.size() - 1) {
                ++i;
                switch (p[i]) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                default:
                    if (p[i] >= '0' && p[i] <= '7' && i + 2 < p.size() - 1 &&
                        p[i + 1] >= '0' && p[i + 1] <= '7' &&
                        p[i + 2] >= '0' && p[i + 2] <= '7') {
                        int v = (p[i] - '0') * 64 + (p[i + 1] - '0') * 8 +
                                (p[i + 2] - '0');
                        out += (char)v;
                        i += 2;
                    } else {
                        out += p[i];
                    }
                    break;
                }
            } else {
                out += p[i];
            }
        }
        return out;
    }
    return p;
}

std::string quote_git_path(const std::string& p)
{
    // Quote (git C-style) when the path would otherwise be ambiguous in a
    // patch: spaces split "diff --git" tokens, tabs collide with the ---/+++
    // timestamp separator, and "->" collides with rename separators, so our
    // own parsers need those quoted even though git itself would leave spaces
    // and "->" unquoted. The remaining triggers (quotes, backslashes,
    // newlines, other controls, non-ASCII bytes) match git's core.quotePath.
    bool need = p.find("->") != std::string::npos;
    if (!need) {
        for (char c : p) {
            unsigned char u = (unsigned char)c;
            if (u >= 0x80 || u == '"' || u == '\\' || u == '\n' || u == '\t' ||
                u < 0x20 || u == ' ') {
                need = true;
                break;
            }
        }
    }
    if (!need)
        return p;
    std::string out = "\"";
    char tmp[8];
    for (char c : p) {
        unsigned char u = (unsigned char)c;
        switch (u) {
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        default:
            if (u >= 0x20 && u < 0x7f) {
                // Printable ASCII (spaces, '-', '>', letters, ...) stays
                // literal inside the quotes, exactly like git emits it.
                out += (char)u;
            } else {
                snprintf(tmp, sizeof(tmp), "\\%03o", u);
                out += tmp;
            }
            break;
        }
    }
    out += '"';
    return out;
}

std::string replace_header_value(const std::string& head, const std::string& key,
                                 const std::string& value)
{
    std::vector<std::string> lines = split_lines(head);
    bool found = false;
    for (auto& l : lines) {
        if (l.size() > key.size() && l.compare(0, key.size(), key) == 0 &&
            l[key.size()] == ':') {
            l = key + ": " + value;
            found = true;
        }
    }
    if (!found)
        lines.insert(lines.begin(), key + ": " + value);
    return join_lines(lines);
}
