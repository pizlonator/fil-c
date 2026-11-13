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
#include "ops.h"
#include "util.h"

#include <cctype>
#include <cstdio>
#include <optional>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
int usage(const char* arg0, bool err)
{
    FILE* f = err ? stderr : stdout;
    fprintf(f,
            "usage: %s "
            "<setup|commit|add|rm|mv|resolve|rebase|create|status|diff|patch|"
            "apply|"
            "package|extract|"
            "download|erase-setup|freeze-mtime|unfreeze-mtime|list-frozen-mtimes|get-attributes|hash|"
            "help> "
            "[args]\n"
            "  setup <f.projeny|dir> [...]\n"
            "  commit <f.projeny|dir>\n"
            "  add <f.projeny|dir> <path>\n"
            "  rm <f.projeny|dir> <path>\n"
            "  mv <f.projeny|dir> <src> <dst>\n"
            "  resolve <f.projeny|dir> <path>\n"
            "  rebase <f.projeny|dir> <new-tarball>\n"
            "  rebase <f.projeny|dir> <url> [<hash>] [<url> [<hash>]...]\n"
            "  create <f.projeny> <new-tarball> [--comment <text>]\n"
            "         [--origname <name>] [--force] [--erase-snapshots]\n"
            "  create <f.projeny> <url> [<hash>] [<url> [<hash>]...]\n"
            "  status <f.projeny|dir>\n"
            "  diff <f.projeny|dir>\n"
            "  diff <dir> <other-dir>\n"
            "  patch <dir> <patch-file>\n"
            "  apply <f.projeny|dir> <patch-file>\n"
            "  package <f.projeny|dir> <output> [...]\n"
            "  extract <f.projeny|dir> <dest> [...]\n"
            "  download <url> <hash> [<url> <hash>...]\n"
            "  erase-setup <f.projeny|dir> [...] [--erase-snapshots] "
            "[--force]\n"
            "  freeze-mtime <f.projeny|dir> <filenames...>\n"
            "  unfreeze-mtime <f.projeny|dir> <filenames...>\n"
            "  list-frozen-mtimes <f.projeny|dir>\n"
            "  get-attributes <f.projeny|dir> [<path>...]\n"
            "  hash <file>\n"
            "  help [command]\n"
            "  options for setup/package/extract/download: -j[--jobs] N, "
            "-c[--curl-jobs] N\n"
            "  options for erase-setup: -j[--jobs] N, --erase-snapshots, "
            "--force\n"
            "  options for create: --comment TEXT, --origname NAME,\n"
            "                      --force, --erase-snapshots\n",
            arg0);
    return err ? 1 : 0;
}

// Default per-project / hash-check parallelism: one thread per CPU (never
// fewer than one; hardware_concurrency may report 0). -j/--jobs overrides
// it, including upward on purpose (make-style -j100 for testing).
int default_jobs()
{
    int n = (int)std::thread::hardware_concurrency();
    return n < 1 ? 1 : n;
}

// Parse the value of a -j/--jobs/-c/--curl-jobs option: a decimal integer
// >= 1. `opt` is the spelling the user used, so the error names it. An
// absurdly large value clamps to INT_MAX (the thread pool is capped by the
// task count anyway), which keeps `-j 999999999999` from being an error.
int parse_jobs_value(const std::string& opt, const std::string& value)
{
    std::string digits = value;
    bool negative = false;
    if (!digits.empty() && digits[0] == '-') {
        negative = true;
        digits = digits.substr(1);
    }
    bool numeric = !digits.empty();
    for (char c : digits) {
        if (!isdigit(static_cast<unsigned char>(c))) {
            numeric = false;
            break;
        }
    }
    if (!numeric)
        die("invalid value for " + opt + ": '" + value + "'");
    if (negative)
        die(opt + " must be >= 1");
    unsigned long long v = 0;
    for (char c : digits) {
        if (v > 1000000000ull) // clamp early: the number only gets bigger
            return 2147483647;
        v = v * 10 + (unsigned long long)(c - '0');
    }
    if (v < 1)
        die(opt + " must be >= 1");
    if (v > 2147483647ull)
        return 2147483647;
    return (int)v;
}

// True when `rest` (the tail of an attached option like "-j8") spells a
// valid value for a jobs-style option, so "-j8" parses as -j 8 while
// "-json" stays an unknown option.
bool looks_like_jobs_value(const std::string& rest)
{
    if (rest.empty())
        return false;
    size_t i = (rest[0] == '-') ? 1 : 0;
    if (i >= rest.size())
        return false;
    for (; i < rest.size(); ++i) {
        if (!isdigit(static_cast<unsigned char>(rest[i])))
            return false;
    }
    return true;
}

// The four signals whose default dispositions kill the process outright,
// with no cleanup: RAII never runs when a signal's default disposition
// terminates the process, so a Ctrl-C during a slow download would leak the
// scratch dir it just created (and any other TempDir still alive). Instead:
//
//   1. Block SIGINT/SIGTERM/SIGHUP/SIGQUIT process-wide BEFORE any thread
//      exists, so every later thread (the run_parallel workers, the ones
//      libcurl spawns) inherits the mask and none of them can be killed by
//      one of these signals mid-operation.
//   2. One dedicated detached thread loops on sigwait for exactly that set;
//      on a wake it sweeps the still-registered temp dirs and then dies by
//      the signal itself.
//
// SIGPIPE is deliberately NOT in the set: at its default disposition it is
// what makes `projeny diff f.projeny | head` (or any piped projeny output)
// die promptly and conventionally (exit 141). With SIGPIPE blocked and
// sigwait'ed, writes to a closed pipe merely return EPIPE — which projeny's
// output paths do not check — so a piped projeny would pointlessly run to
// completion and exit 0 instead of dying at the pipe. SIGPIPE deaths are
// also extremely narrow (the moment of a failed write), and any scratch dir
// alive at that instant is cleaned by the startup reapers on the next run
// (the flock is kernel-released on death), so not handling SIGPIPE does not
// reintroduce a lasting leak.
//
// The sigwait-thread pattern is used so the cleanup can call ordinary (non-
// async-signal-safe) functions — opendir/unlink/flock and the rest of
// remove_recursive — in a normal thread context; a signal handler may call
// none of them. Signals arriving while the sweep runs stay pending (every
// thread still blocks them), so a second Ctrl-C cannot interrupt the
// cleanup mid-removal. The exit stays shell-correct: the default
// disposition is restored for the caught signal, which is then unblocked on
// the sigwait thread alone and raised — the process dies with 128+sig (143
// for SIGTERM, 130 for SIGINT, ...) exactly as it would have without any of
// this; the _exit below is only the fallback if the raise is somehow lost.
void install_signal_cleanup()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGQUIT);
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0)
        return; // no mask, no sigwait: keep the default-disposition behavior
    std::thread([set]() {
        for (;;) {
            int sig = 0;
            if (sigwait(&set, &sig) != 0)
                continue; // not a member / spurious: wait again
            cleanup_tempdirs();
            // Die by the signal (see the function comment): restore SIG_DFL,
            // unblock just this signal on this thread — the other threads
            // keep it blocked, so the raise is delivered here — then raise.
            signal(sig, SIG_DFL);
            sigset_t one;
            sigemptyset(&one);
            sigaddset(&one, sig);
            pthread_sigmask(SIG_UNBLOCK, &one, nullptr);
            raise(sig);
            _exit(128 + sig); // fallback: reached only if the raise is lost
        }
    }).detach();
}

} // namespace

int main(int argc, char** argv)
{
    // Signals first: every thread created below (workers, curl's own) must
    // inherit the blocked mask (see install_signal_cleanup above).
    install_signal_cleanup();

    // Belt-and-suspenders for every exit path of main — the normal returns
    // and the catch clauses alike (those also sweep themselves): remove
    // anything still registered. Every TempDir has destroyed itself by now
    // on every path, so this is a no-op unless something registered a dir
    // it never cleaned up; cheap insurance either way, and it is a no-op
    // when the registry is empty.
    struct FinalTempdirSweep {
        ~FinalTempdirSweep()
        {
            cleanup_tempdirs();
        }
    } final_tempdir_sweep;

    std::string arg0 = argc > 0 ? argv[0] : "projeny";
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.push_back(argv[i]);
    if (args.empty())
        return usage(arg0.c_str(), true);

    const std::string& cmd = args[0];
    // die() no longer exits: it prints the error report and throws
    // ProjenyFatalError, so the hard-error path funnels through here.
    // Catching it restores the single-project exit(1) behavior: the report
    // is already on stderr (print nothing extra), every worker thread has
    // joined by now, so sweeping the still-registered temp dirs is safe.
    // The std::exception net catches anything else that escapes (e.g.
    // bad_alloc) so the process still fails with exit 1 and a diagnostic
    // instead of an abort.
    try {
        // Startup janitor (once per process, before any command runs):
        // reap the legacy "projeny-<role>-XXXXXX" scratch dirs the old
        // binaries scattered into $TMPDIR//tmp. Current scratch dirs live
        // next to the .projeny file and are reaped by make_scratch, one
        // directory at a time. Never dies, prints one line only when it
        // actually removed something (see reap_legacy_tmp_scratch).
        reap_legacy_tmp_scratch();
        if (cmd == "help" || cmd == "--help" || cmd == "-h") {
            if (args.size() == 1)
                return cmd_help(arg0);
            if (args.size() == 2)
                return cmd_help_topic(arg0, args[1]);
            return usage(arg0.c_str(), true);
        }
        // setup/package/extract/download are the parallel-mode commands:
        // they accept -j/--jobs and -c/--curl-jobs anywhere among their
        // arguments. Every other command keeps its exact argument shape.
        if (cmd == "setup" || cmd == "package" || cmd == "extract" ||
            cmd == "download") {
            int jobs = default_jobs();
            int curl_jobs = 8;
            std::vector<std::string> rest;
            for (size_t i = 1; i < args.size(); ++i) {
                const std::string& tok = args[i];
                if (tok == "-j" || tok == "--jobs") {
                    if (i + 1 >= args.size())
                        die("option " + tok + " requires a value");
                    jobs = parse_jobs_value(tok, args[++i]);
                } else if (tok == "-c" || tok == "--curl-jobs") {
                    if (i + 1 >= args.size())
                        die("option " + tok + " requires a value");
                    curl_jobs = parse_jobs_value(tok, args[++i]);
                } else if (starts_with(tok, "-j") && tok.size() > 2 &&
                           looks_like_jobs_value(tok.substr(2))) {
                    jobs = parse_jobs_value("-j", tok.substr(2));
                } else if (starts_with(tok, "-c") && tok.size() > 2 &&
                           looks_like_jobs_value(tok.substr(2))) {
                    curl_jobs = parse_jobs_value("-c", tok.substr(2));
                } else if (starts_with(tok, "--jobs=")) {
                    jobs = parse_jobs_value("--jobs", tok.substr(7));
                } else if (starts_with(tok, "--curl-jobs=")) {
                    curl_jobs = parse_jobs_value("--curl-jobs", tok.substr(12));
                } else if (starts_with(tok, "-")) {
                    die("unknown option '" + tok + "'");
                } else {
                    rest.push_back(tok);
                }
            }
            if (cmd == "setup") {
                if (rest.empty())
                    return usage(arg0.c_str(), true);
                return cmd_setup_multi(rest, jobs, curl_jobs);
            }
            if (cmd == "package" || cmd == "extract") {
                // Paired parallel form: (project, output/dest) pairs.
                if (rest.size() < 2 || rest.size() % 2 != 0)
                    return usage(arg0.c_str(), true);
                std::vector<std::pair<std::string, std::string>> pairs;
                pairs.reserve(rest.size() / 2);
                for (size_t i = 0; i + 1 < rest.size(); i += 2)
                    pairs.emplace_back(rest[i], rest[i + 1]);
                if (cmd == "package")
                    return cmd_package_multi(pairs, jobs, curl_jobs);
                return cmd_extract_multi(pairs, jobs, curl_jobs);
            }
            // download: URL HASH pairs.
            if (rest.size() < 2 || rest.size() % 2 != 0)
                return usage(arg0.c_str(), true);
            return cmd_download(rest, jobs, curl_jobs);
        }
        // erase-setup is a parallel-mode command too: -j/--jobs anywhere,
        // plus the valueless --erase-snapshots and --force flags. It has NO
        // download phase, so there is nothing for -c/--curl-jobs to
        // control: those spellings are just unknown options here.
        if (cmd == "erase-setup") {
            int jobs = default_jobs();
            bool erase_snapshots = false;
            bool force = false;
            std::vector<std::string> rest;
            for (size_t i = 1; i < args.size(); ++i) {
                const std::string& tok = args[i];
                if (tok == "-j" || tok == "--jobs") {
                    if (i + 1 >= args.size())
                        die("option " + tok + " requires a value");
                    jobs = parse_jobs_value(tok, args[++i]);
                } else if (starts_with(tok, "-j") && tok.size() > 2 &&
                           looks_like_jobs_value(tok.substr(2))) {
                    jobs = parse_jobs_value("-j", tok.substr(2));
                } else if (starts_with(tok, "--jobs=")) {
                    jobs = parse_jobs_value("--jobs", tok.substr(7));
                } else if (tok == "--erase-snapshots") {
                    erase_snapshots = true;
                } else if (tok == "--force") {
                    force = true;
                } else if (tok.size() > 2 && starts_with(tok, "-c") &&
                           looks_like_jobs_value(tok.substr(2))) {
                    // The attached-value spelling of -c: name the option,
                    // not the spelling, so every -c form reports alike.
                    die("unknown option '-c'");
                } else if (starts_with(tok, "-")) {
                    die("unknown option '" + tok + "'");
                } else {
                    rest.push_back(tok);
                }
            }
            if (rest.empty())
                return usage(arg0.c_str(), true);
            return cmd_erase_setup_multi(rest, jobs, erase_snapshots, force);
        }
        // create parses its options anywhere among the arguments, exactly
        // like erase-setup: the valueless --force (countable) and
        // --erase-snapshots flags, plus the valued --comment/--origname.
        // It never downloads in parallel and has no -j/-c surface: those
        // spellings are unknown options here, named without their attached
        // value so every form reports alike.
        if (cmd == "create") {
            std::string comment;
            std::optional<std::string> origname;
            int force = 0;
            bool erase_snapshots = false;
            std::vector<std::string> rest;
            for (size_t i = 1; i < args.size(); ++i) {
                const std::string& tok = args[i];
                if (tok == "--comment") {
                    if (i + 1 >= args.size())
                        die("option " + tok + " requires a value");
                    comment = args[++i];
                } else if (starts_with(tok, "--comment=")) {
                    comment = tok.substr(10);
                } else if (tok == "--origname") {
                    if (i + 1 >= args.size())
                        die("option " + tok + " requires a value");
                    origname = args[++i];
                } else if (starts_with(tok, "--origname=")) {
                    origname = tok.substr(11);
                } else if (tok == "--force") {
                    ++force;
                } else if (tok == "--erase-snapshots") {
                    erase_snapshots = true;
                } else if (tok.size() > 2 && starts_with(tok, "-c") &&
                           looks_like_jobs_value(tok.substr(2))) {
                    die("unknown option '-c'");
                } else if (tok.size() > 2 && starts_with(tok, "-j") &&
                           looks_like_jobs_value(tok.substr(2))) {
                    die("unknown option '-j'");
                } else if (starts_with(tok, "-")) {
                    die("unknown option '" + tok + "'");
                } else {
                    rest.push_back(tok);
                }
            }
            if (rest.size() < 2)
                return usage(arg0.c_str(), true);
            return cmd_create(
                rest[0],
                std::vector<std::string>(rest.begin() + 1, rest.end()),
                comment, origname, force, erase_snapshots);
        }
        if (cmd == "commit") {
            if (args.size() != 2)
                return usage(arg0.c_str(), true);
            return cmd_commit(args[1]);
        }
        if (cmd == "add") {
            if (args.size() != 3)
                return usage(arg0.c_str(), true);
            return cmd_add(args[1], args[2]);
        }
        if (cmd == "rm") {
            if (args.size() != 3)
                return usage(arg0.c_str(), true);
            return cmd_rm(args[1], args[2]);
        }
        if (cmd == "mv") {
            if (args.size() != 4)
                return usage(arg0.c_str(), true);
            return cmd_mv(args[1], args[2], args[3]);
        }
        if (cmd == "resolve") {
            if (args.size() != 3)
                return usage(arg0.c_str(), true);
            return cmd_resolve(args[1], args[2]);
        }
        if (cmd == "rebase") {
            if (args.size() < 3)
                return usage(arg0.c_str(), true);
            return cmd_rebase(args[1],
                              std::vector<std::string>(args.begin() + 2,
                                                       args.end()));
        }
        if (cmd == "status") {
            if (args.size() != 2)
                return usage(arg0.c_str(), true);
            return cmd_status(args[1]);
        }
        if (cmd == "diff") {
            if (args.size() == 2)
                return cmd_diff_projeny(args[1]);
            if (args.size() == 3)
                return cmd_diff(args[1], args[2]);
            return usage(arg0.c_str(), true);
        }
        if (cmd == "patch") {
            if (args.size() != 3)
                return usage(arg0.c_str(), true);
            return cmd_patch(args[1], args[2]);
        }
        if (cmd == "apply") {
            if (args.size() != 3)
                return usage(arg0.c_str(), true);
            return cmd_apply(args[1], args[2]);
        }
        if (cmd == "freeze-mtime") {
            if (args.size() < 3)
                return usage(arg0.c_str(), true);
            return cmd_freeze_mtime(args[1],
                                    std::vector<std::string>(args.begin() + 2,
                                                             args.end()));
        }
        if (cmd == "unfreeze-mtime") {
            if (args.size() < 3)
                return usage(arg0.c_str(), true);
            return cmd_unfreeze_mtime(args[1],
                                      std::vector<std::string>(args.begin() + 2,
                                                               args.end()));
        }
        if (cmd == "list-frozen-mtimes") {
            if (args.size() != 2)
                return usage(arg0.c_str(), true);
            return cmd_list_frozen_mtimes(args[1]);
        }
        if (cmd == "get-attributes") {
            if (args.size() < 2)
                return usage(arg0.c_str(), true);
            return cmd_get_attributes(args[1],
                                      std::vector<std::string>(args.begin() + 2,
                                                               args.end()));
        }
        if (cmd == "hash") {
            if (args.size() != 2)
                return usage(arg0.c_str(), true);
            return cmd_hash(args[1]);
        }
        fprintf(stderr, "projeny: error: unknown command '%s'\n", cmd.c_str());
        return usage(arg0.c_str(), true);
    } catch (const ProjenyFatalError&) {
        // die() already printed its report to stderr under g_output_mutex;
        // every thread has joined, so the temp-dir sweep is safe here.
        cleanup_tempdirs();
        return 1;
    } catch (const std::exception& e) {
        fprintf(stderr, "projeny: error: internal: %s\n", e.what());
        cleanup_tempdirs();
        return 1;
    }
}
