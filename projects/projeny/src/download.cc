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
// Downloading and hashing for URL:-based .projeny files. Links the curl
// library directly (curl_easy API) and compiles in the blake3 library
// vendored in src/blake3/ (blake3_hasher API) — projeny never shells out
// to `curl` or `b3sum`.
#include "download.h"

#include "util.h"

#include "blake3/blake3.h"
#include <curl/curl.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace {

// Lowercase hex of `n` bytes (blake3 outputs raw digests; the .projeny
// "URL: <url> <hash>" form and `projeny hash` both spell them lowercase hex).
std::string hex_lowercase(const unsigned char* bytes, size_t n)
{
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        out.push_back(digits[bytes[i] >> 4]);
        out.push_back(digits[bytes[i] & 0x0f]);
    }
    return out;
}

// curl_easy_writecallback: append the received bytes to the caller's
// std::string (passed via CURLOPT_WRITEDATA).
size_t append_to_string(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// Progress state for one download: what was already printed, so the
// callback can decide when the next line is due.
struct DownloadProgress {
    curl_off_t printed_bytes = 0;  // received count at the last printed line
    int printed_pct = -1;          // whole-percent at the last printed line
    curl_off_t last_total = 0;     // the current transfer's dltotal (0 = unknown)
    bool printed_any = false;      // whether any progress line printed at all
};

// One progress line, in a deliberately short form: the announcement
// ("downloading '<url>'") already names the URL, so the progress line does
// not repeat it, and the short prefix keeps the line within an 80-column
// terminal. The line is bare '\r'-terminated (no ANSI escapes, no padding,
// no isatty tricks), so a terminal redraws the line in place while a log
// file keeps every line.
void print_progress_line(curl_off_t now, curl_off_t total)
{
    // Serialized so a try_download running inside a parallel worker (the
    // multi-mode fallback download) can never interleave a partial line
    // with another thread's output.
    std::lock_guard<std::mutex> lk(g_output_mutex);
    if (total > 0)
        fprintf(stderr, "projeny: download progress: %lld/%lld bytes (%d%%)\r",
                (long long)now, (long long)total,
                (int)((100 * now) / total));
    else
        fprintf(stderr, "projeny: download progress: %lld bytes\r",
                (long long)now);
}

// curl xferinfo callback (CURLOPT_XFERINFOFUNCTION; needs
// CURLOPT_NOPROGRESS set to 0 to fire at all). Throttled by two gates that
// BOTH must pass before a line prints (see download.h for the rationale):
// at least 64 KiB since the last line, and — only when the total is known —
// a whole-percent boundary crossed since the last line.
int download_progress_cb(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                         curl_off_t ultotal, curl_off_t ulnow)
{
    (void)ultotal; (void)ulnow;
    DownloadProgress* st = static_cast<DownloadProgress*>(clientp);
    if (dlnow < 0)
        return 0;
    if (dlnow < st->printed_bytes) {
        // The body restarted (curl retried or rewound): reset the throttle
        // so the new run's progress is reported from scratch.
        st->printed_bytes = 0;
        st->printed_pct = -1;
    }
    // Mirror, never latch: one progress struct spans the whole redirect
    // chain (CURLOPT_FOLLOWLOCATION), so a redirect hop's Content-Length
    // must not survive into the closing line of a target that sends none —
    // a latched total printed bogus "N/M bytes (P%)" closers (P far past
    // 100) for unknown-length final bodies.
    st->last_total = dltotal;
    bool have_total = dltotal > 0;
    bool bytes_step = dlnow - st->printed_bytes >= 65536;
    int pct = have_total ? (int)((100 * dlnow) / dltotal) : -1;
    bool pct_step = !have_total || pct > st->printed_pct;
    if (!bytes_step || !pct_step)
        return 0;
    print_progress_line(dlnow, dltotal);
    st->printed_bytes = dlnow;
    st->printed_pct = pct;
    st->printed_any = true;
    return 0;
}

// Whole-percent (0..100) of a batch transfer whose total size is known.
// Clamped: only a server lying about its Content-Length can push the raw
// ratio past 100, and a clamped entry keeps every progress line honest.
int batch_whole_percent(curl_off_t now, curl_off_t total)
{
    int pct = (int)((100 * now) / total);
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return pct;
}

// Compact human byte count for one batch progress entry whose total size is
// unknown: "<n>B" below 1 KiB, else the count in KiB/MiB/GiB with one
// decimal, dropping a zero fraction ("1.4KiB", "37KiB", "1.2MiB"). Pure
// integer math (the decimal truncates, never rounds), so the token is
// monotone in the byte count and deterministic on every platform. A
// transfer that has received nothing yet renders "0B". Never "?": an
// unknown total still leaves an honest bytes-so-far to report, and a single
// greppable token per entry keeps the combined line grep-friendly.
std::string batch_byte_token(curl_off_t now)
{
    unsigned long long v = now > 0 ? (unsigned long long)now : 0ULL;
    if (v < 1024)
        return std::to_string(v) + "B";
    static const char* const units[] = {"KiB", "MiB", "GiB"};
    unsigned long long unit = 1024;
    int u = 0;
    while (u < 2 && v >= unit * 1024) {
        unit *= 1024;
        ++u;
    }
    std::string out = std::to_string(v / unit);
    unsigned long long tenths = (v % unit) * 10 / unit;
    if (tenths > 0) {
        out += '.';
        out += static_cast<char>('0' + tenths);
    }
    return out + units[u];
}

// One combined batch progress line, printed exactly like try_download's
// single-transfer lines (stderr, the "projeny: download progress: " prefix,
// bare '\r' termination — no ANSI escapes, no backspaces, no padding, no
// isatty checks — so a terminal redraws the line in place while a log keeps
// every line) and serialized by g_output_mutex like every other output.
// `entries` holds one single-token entry per package of the pass's roster
// in first-announcement order: the package's whole-percent ("N%") when its
// transfer knows its total size, its compact byte count ("37KiB") when it
// does not, or "0B" while it holds neither a live transfer nor a completed
// one — never "?", so every entry stays a single greppable token that
// correlates 1:1 with the "downloading '<name>' from '<url>'" announcements
// (entry N is the Nth announcement). The round's closing line is printed
// with closing=true: it ends the progress sequence with a newline after
// the '\r', so a terminal keeps the final state visible on its own line
// instead of letting the hash-phase reports overwrite it, and a log's line
// structure stays intact (the next "downloaded"/"failed to
// obtain"/"retrying" line starts at column 0).
void print_batch_progress_line(const std::vector<std::string>& entries,
                               bool closing)
{
    std::string line = "projeny: download progress:";
    for (const std::string& e : entries) {
        line += ' ';
        line += e;
    }
    std::lock_guard<std::mutex> lk(g_output_mutex);
    if (closing)
        fprintf(stderr, "%s\r\n", line.c_str());
    else
        fprintf(stderr, "%s\r", line.c_str());
}

// One-time global setup shared by try_download and download_batch. The
// function-local static's once-guard runs the init exactly once per process;
// both call sites run it on the calling thread before any other libcurl call
// (download_batch performs it before starting its transfer loop and its hash
// workers).
bool ensure_curl_global()
{
    static const bool global_ok = curl_global_init(CURL_GLOBAL_ALL) == CURLE_OK;
    return global_ok;
}

// Shared easy-handle setup, factored out of try_download so the batch
// scheduler's handles match its options exactly: the URL, the
// append-to-string write callback, redirect following, HTTP >= 400 reported
// as an error, no signals, and the same hung-transfer timeouts. Deliberately
// does NOT set CURLOPT_ERRORBUFFER (each caller owns its error-buffer
// storage and reads it after the transfer) nor any progress wiring
// (try_download enables its own progress callback after this call; the
// batch scheduler wires its record-only xferinfo callback after it — the
// combined batch progress line is rendered by the scheduler loop, not by
// the callbacks).
void configure_easy(CURL* c, const std::string& url, std::string* data)
{
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, append_to_string);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, data);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    // Bound the transfer so a hung mirror cannot stall setup/commit
    // forever: give up on a connection that cannot be established within
    // 30 seconds, and on any transfer that stays below 1 byte/sec for 60
    // seconds. file:// transfers (the tests) are unaffected.
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 60L);
}

// Internal per-package state for one download_batch run. One instance per
// spec, constructed up front in spec order and never resized afterwards, so
// an in-flight easy handle can safely hold pointers into buffer, curl_err,
// and the xfer fields for the whole transfer pass.
struct BatchWork {
    const BatchPackageSpec* spec = nullptr;
    CURL* handle = nullptr;          // non-null while the package is in flight
    std::string buffer;              // bytes of the current/last transfer
    char curl_err[CURL_ERROR_SIZE];  // error-buffer storage for the handle
    size_t next_url = 0;             // next spec->urls index to try
    std::string attempt_url;         // URL of the current/last attempt
    bool transferred = false;        // buffer holds an unverified transfer
    bool verified = false;           // hash matched; buffer/url are final
    std::string hash;                // blake3 hex of buffer (once verified)
    // Latest counts the xferinfo callback reported for the in-flight (or
    // last) attempt: written during curl_multi_perform on the scheduler's
    // thread and read by that same thread's combined progress-line render.
    // xfer_total <= 0 means the total size is unknown (no Content-Length);
    // it is mirrored, never latched, so a redirect hop's Content-Length
    // cannot survive into a target that sends none.
    curl_off_t xfer_now = 0;
    curl_off_t xfer_total = -1;
};

// The batch scheduler's xferinfo callback (CURLOPT_XFERINFOFUNCTION, with
// CURLOPT_NOPROGRESS lifted per handle). Unlike try_download's callback it
// prints nothing: one combined line for the whole batch is rendered by the
// scheduler loop instead, so N concurrent transfers share one line rather
// than spamming N '\r' lines each. It only records the latest counts into
// the owning package's BatchWork, and it always runs on the scheduler's
// thread (libcurl fires it inside curl_multi_perform, which only the
// scheduler calls), so no locking is needed here.
int batch_xferinfo_cb(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                      curl_off_t ultotal, curl_off_t ulnow)
{
    (void)ultotal; (void)ulnow;
    BatchWork* w = static_cast<BatchWork*>(clientp);
    if (dlnow >= 0)
        w->xfer_now = dlnow;
    w->xfer_total = dltotal > 0 ? dltotal : -1;
    return 0;
}

// The single-token progress entry for one package of the pass's roster.
// Every entry is honest — a percent or a byte count, never "?":
//   - a package holding a completed transfer reads "100%" (even when that
//     transfer's total size was unknown: completing is what the entry
//     reports, and the byte count is in the "downloaded '<name>' (<N>
//     bytes)" note);
//   - a package with a live transfer reads its whole-percent ("N%") when
//     the transfer reports a total size, else the compact byte count of
//     what has arrived so far ("0B" until the first bytes land);
//   - a package with neither — a failed attempt awaiting its next
//     candidate URL, or a package whose candidate URLs are exhausted —
//     reads "0B": attempts start from zero bytes and nothing of a failed
//     attempt is kept, so there are no bytes of this package to report.
std::string batch_entry_token(const BatchWork& w)
{
    if (w.transferred)
        return "100%";
    if (w.handle != nullptr) {
        if (w.xfer_total > 0)
            return std::to_string(batch_whole_percent(w.xfer_now,
                                                      w.xfer_total)) + "%";
        return batch_byte_token(w.xfer_now);
    }
    return "0B";
}

// One phase-1 scheduler run over work: fills the curl_jobs budget with at
// most one easy handle per package (packages with transferred-but-unverified
// bytes, verified packages, and exhausted packages are skipped), pumping the
// multi handle until every eligible package has either transferred or run
// out of candidate URLs. Every curl call happens here, on the caller's
// thread.
void batch_transfer_pass(std::vector<BatchWork>& work,
                         std::vector<BatchPackageResult>& results,
                         int curl_jobs)
{
    CURLM* multi = curl_multi_init();
    if (!multi)
        die("curl_multi_init failed");
    size_t in_flight = 0;
    std::unordered_map<CURL*, size_t> handle_owner;

    // Combined progress-line bookkeeping, all touched on this (calling)
    // thread: the xferinfo callbacks write the per-package counts (fired
    // inside curl_multi_perform below, still on this thread) and the loop
    // renders from them under g_output_mutex. announce_order lists each
    // package the first time this pass announces it — the pass's ROSTER,
    // which only ever grows and whose positions are the announcement
    // positions: entry N of every line (in-flight and closing alike) is
    // the Nth "downloading" announcement. rendered_entry holds the last
    // rendered entry token per package (the re-render throttle's
    // baseline); start_transfer resets it per attempt, alongside the
    // transfer state itself, because every attempt starts from zero bytes
    // and must never be gated by what a previous attempt last rendered.
    // last_render_ns is the steady-clock time (ns) of the last render
    // (0 = never, so the first due render is not delayed).
    std::vector<bool> announced_once(work.size(), false);
    std::vector<size_t> announce_order;
    std::vector<std::string> rendered_entry(work.size());
    long long last_render_ns = 0;

    // Internal-error escape: detach and free everything still in flight so
    // the die() below (which throws, not exits) doesn't leak handles.
    auto abort_all = [&]() {
        for (const auto& kv : handle_owner) {
            curl_multi_remove_handle(multi, kv.first);
            curl_easy_cleanup(kv.first);
            work[kv.second].handle = nullptr;
        }
        handle_owner.clear();
        in_flight = 0;
        curl_multi_cleanup(multi);
    };

    auto start_transfer = [&](size_t i) {
        BatchWork& w = work[i];
        const std::string& url = w.spec->urls[w.next_url];
        ++w.next_url;
        w.attempt_url = url;
        w.buffer.clear(); // a fresh attempt never appends to old bytes
        w.curl_err[0] = 0;
        w.xfer_now = 0;
        w.xfer_total = -1;
        // The re-render baseline resets with the transfer state: a new
        // attempt starts from zero bytes with an unknown total, so its
        // baseline is the fresh-attempt entry ("0B"). The previous
        // attempt's last-rendered percent (or byte count) must not gate
        // this one's renders — otherwise the combined progress line would
        // keep the dead attempt's stale, higher-than-actual percentage on
        // display until the new attempt climbed past it — and the restart
        // itself is not a visible change: the entry moves off "0B" only
        // when the new attempt actually receives bytes (or learns its
        // total), so the line re-renders from the new attempt's own early
        // percentages.
        rendered_entry[i] = batch_entry_token(w);
        if (!announced_once[i]) {
            announced_once[i] = true;
            // First announcement of this pass: the package joins the
            // roster here and stays in it until the pass ends.
            announce_order.push_back(i);
        }
        note("downloading '" + w.spec->name + "' from '" + url + "'");
        CURL* c = curl_easy_init();
        if (!c) {
            abort_all();
            die("curl_easy_init failed");
        }
        configure_easy(c, url, &w.buffer);
        curl_easy_setopt(c, CURLOPT_ERRORBUFFER, w.curl_err);
        // Lift NOPROGRESS and record the transfer's counts into w: the
        // combined progress line for the whole batch is rendered by the
        // scheduler loop below (one throttled line, never one line per
        // transfer).
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, batch_xferinfo_cb);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, &w);
        CURLMcode mrc = curl_multi_add_handle(multi, c);
        if (mrc != CURLM_OK) {
            curl_easy_cleanup(c);
            abort_all();
            die(std::string("curl_multi_add_handle failed: ") +
                curl_multi_strerror(mrc));
        }
        w.handle = c;
        handle_owner[c] = i;
        ++in_flight;
    };

    // A package needs a handle when it holds no bytes awaiting a hash
    // check, was not verified in an earlier round, owns no in-flight
    // handle, and still has candidate URLs left.
    auto eligible = [&](size_t i) {
        const BatchWork& w = work[i];
        return !w.verified && !w.transferred && w.handle == nullptr &&
               w.next_url < w.spec->urls.size();
    };

    for (;;) {
        // Fill the in-flight budget, lowest spec index first (deterministic
        // scheduling: spec order is stable across passes and runs).
        while (in_flight < (size_t)curl_jobs) {
            size_t pick = work.size();
            for (size_t i = 0; i < work.size(); ++i) {
                if (eligible(i)) {
                    pick = i;
                    break;
                }
            }
            if (pick == work.size())
                break;
            start_transfer(pick);
        }
        if (in_flight == 0)
            break; // nothing in flight and nothing eligible: pass complete
        int still_running = 0;
        CURLMcode mrc = curl_multi_perform(multi, &still_running);
        if (mrc != CURLM_OK) {
            abort_all();
            die(std::string("curl_multi_perform failed: ") +
                curl_multi_strerror(mrc));
        }
        // Drain completions: mark the transfer done, or record the failure
        // (which frees the package to try its next candidate URL on a later
        // fill).
        CURLMsg* msg;
        int msgs_left = 0;
        while ((msg = curl_multi_info_read(multi, &msgs_left)) != nullptr) {
            if (msg->msg != CURLMSG_DONE)
                continue;
            auto it = handle_owner.find(msg->easy_handle);
            if (it == handle_owner.end())
                continue; // cannot happen; never kill a handle we do not own
            BatchWork& w = work[it->second];
            BatchPackageResult& r = results[it->second];
            // Read the transfer result NOW: the message lives in the multi
            // handle's queue and removing/cleaning up the easy handle it
            // refers to frees that memory (TSAN caught the read below
            // landing on freed memory). Nothing after this line may touch
            // `msg`.
            const CURLcode xfer_result = msg->data.result;
            handle_owner.erase(it);
            curl_multi_remove_handle(multi, msg->easy_handle);
            curl_easy_cleanup(msg->easy_handle);
            w.handle = nullptr;
            --in_flight;
            if (xfer_result == CURLE_OK) {
                // FAILONERROR makes any HTTP >= 400 an error, so reaching
                // CURLE_OK means the body transferred.
                w.transferred = true;
            } else {
                // curl_easy_strerror names the failure class; the error
                // buffer usually holds the specific cause (HTTP status,
                // file:// error, ...). Both, when available.
                std::string err = curl_easy_strerror(xfer_result);
                if (w.curl_err[0] != 0)
                    err += std::string(": ") + w.curl_err;
                std::string line = "failed to download '" + w.spec->name +
                                   "' from '" + w.attempt_url + "': " + err;
                warn(line);
                r.errors.push_back(line);
            }
        }
        // Combined progress line: at most one per scheduler loop iteration,
        // and only when BOTH re-render gates pass — at least 200ms since
        // the last render, and some entry's rendered string changed. The
        // entries cover the ROSTER — every package this pass has announced
        // so far, in first-announcement order — so the entry count only
        // ever grows (a queued package joins when its announcement prints)
        // and a completed transfer stays listed at "100%" instead of
        // disappearing and shifting the entries left; a shrinking
        // '\r'-redrawn line would leave stale residue on the terminal. The
        // closing line after the loop reports the round's final state
        // deterministically.
        if (in_flight > 0) {
            long long now_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
            bool changed = false;
            for (size_t i : announce_order) {
                if (batch_entry_token(work[i]) != rendered_entry[i]) {
                    changed = true;
                    break;
                }
            }
            if (changed && now_ns - last_render_ns >= 200000000LL) {
                std::vector<std::string> entries;
                entries.reserve(announce_order.size());
                for (size_t i : announce_order) {
                    std::string tok = batch_entry_token(work[i]);
                    rendered_entry[i] = tok;
                    entries.push_back(std::move(tok));
                }
                print_batch_progress_line(entries, /*closing=*/false);
                last_render_ns = now_ns;
            }
        }
        if (still_running > 0) {
            // Sleep until something socket-ish happens (or 200ms, so a
            // wedged transfer still cycles the loop); file:// transfers
            // complete inside curl_multi_perform and never wait here.
            mrc = curl_multi_wait(multi, nullptr, 0, 200, nullptr);
            if (mrc != CURLM_OK) {
                abort_all();
                die(std::string("curl_multi_wait failed: ") +
                    curl_multi_strerror(mrc));
            }
        }
    }

    // The round's closing progress line: exactly one per pass, listing
    // EVERY package this pass announced — the same roster the in-flight
    // lines rendered, in first-announcement order — with its final state:
    // "100%" for a package holding a completed transfer (including one
    // whose total size was unknown: completing is what the closing line
    // reports), "0B" for one that never finished one (its attempts failed
    // before or mid-transfer, and nothing of a failed attempt is kept).
    // Unlike the throttled in-flight lines this is deterministic, so tests
    // can pin the entry count and values; a hash mismatch does not change
    // the entry (the transfer completed — the mismatch is the hash pass's
    // report). The retry pass runs this function again and prints its own
    // closing line for its own round. Silent only when the pass announced
    // nothing at all.
    if (!announce_order.empty()) {
        std::vector<std::string> entries;
        entries.reserve(announce_order.size());
        for (size_t i : announce_order)
            entries.push_back(batch_entry_token(work[i]));
        print_batch_progress_line(entries, /*closing=*/true);
    }
    curl_multi_cleanup(multi);
}

// One phase-2 run: blake3-check every package holding transferred-but-
// unverified bytes on hash_jobs threads. Verified packages keep their bytes
// (the result assembles from them at the end); mismatched ones record the
// failure, drop the bytes, and become eligible for the retry pass. The
// workers touch only their own package's state (and warn() serializes the
// mismatch lines), so no locking beyond g_output_mutex is needed — the
// vendored blake3 keeps all its state in the caller's hasher.
void batch_hash_pass(std::vector<BatchWork>& work,
                     std::vector<BatchPackageResult>& results,
                     int hash_jobs)
{
    std::vector<size_t> pending;
    for (size_t i = 0; i < work.size(); ++i) {
        // Only bytes that are still unverified: a package verified in an
        // earlier round keeps its result and is never re-hashed (and its
        // verified line never prints twice).
        if (work[i].transferred && !work[i].verified)
            pending.push_back(i);
    }
    if (pending.empty())
        return;
    run_parallel(hash_jobs, pending.size(), [&](size_t k) {
        BatchWork& w = work[pending[k]];
        BatchPackageResult& r = results[pending[k]];
        std::string got = blake3_hash_hex(w.buffer);
        if (w.spec->accepted_hashes.count(got) > 0) {
            w.verified = true;
            w.hash = std::move(got);
            note("downloaded '" + w.spec->name + "' (" +
                 std::to_string(w.buffer.size()) + " bytes); blake3 hash "
                 "verified");
            return;
        }
        std::string line = "downloaded '" + w.spec->name + "' from '" +
                           w.attempt_url + "' but blake3 hash mismatch (got " +
                           got + ")";
        warn(line);
        r.errors.push_back(line);
        w.buffer.clear();
        w.transferred = false;
    });
}

} // namespace

std::string blake3_hash_hex(const std::string& data)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    if (!data.empty())
        blake3_hasher_update(&hasher, data.data(), data.size());
    unsigned char out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
    return hex_lowercase(out, BLAKE3_OUT_LEN);
}

std::string blake3_file_hash_hex(const std::string& path)
{
    // Same read loop as read_file_bytes (64 KiB chunks, EINTR-tolerant), but
    // streaming the bytes into the hasher instead of a string: tarballs can
    // be tens of megabytes.
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        die("cannot read file '" + path + "': " + strerror(errno));
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    char buf[65536];
    for (;;) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR)
                continue;
            int saved = errno;
            close(fd);
            die("cannot read file '" + path + "': " + strerror(saved));
        }
        if (r == 0)
            break;
        blake3_hasher_update(&hasher, buf, (size_t)r);
    }
    close(fd);
    unsigned char out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
    return hex_lowercase(out, BLAKE3_OUT_LEN);
}

bool try_download(const std::string& url, std::string* data, std::string* err)
{
    if (!ensure_curl_global()) {
        *err = "curl_global_init failed";
        return false;
    }
    data->clear();
    CURL* c = curl_easy_init();
    if (!c) {
        *err = "curl_easy_init failed";
        return false;
    }
    std::string body;
    char curl_err[CURL_ERROR_SIZE];
    curl_err[0] = 0;
    configure_easy(c, url, &body);
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, curl_err);
    // Announce the attempt before it starts, then wire up the progress
    // callback. libcurl suppresses progress callbacks by default
    // (CURLOPT_NOPROGRESS defaults to 1), so lifting that is required for
    // download_progress_cb to fire at all. (The announcement and every
    // progress line print under g_output_mutex — note() and
    // print_progress_line lock it — so this stays line-atomic even when a
    // fallback download runs inside a parallel worker.)
    note("downloading '" + url + "'");
    DownloadProgress progress;
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, download_progress_cb);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &progress);
    CURLcode rc = curl_easy_perform(c);
    if (rc == CURLE_OK) {
        // curl does not promise a progress tick landing exactly on the
        // received count, so close the report explicitly — unless the
        // callback already printed exactly that state.
        if (!progress.printed_any ||
            progress.printed_bytes != (curl_off_t)body.size())
            print_progress_line((curl_off_t)body.size(),
                                progress.last_total);
    }
    curl_easy_cleanup(c);
    if (rc != CURLE_OK) {
        // curl_easy_strerror names the failure class; the error buffer (set
        // above) usually holds the specific cause (HTTP status, file://
        // error, ...). Both, when available.
        std::string msg = curl_easy_strerror(rc);
        if (curl_err[0] != 0)
            msg += std::string(": ") + curl_err;
        *err = msg;
        return false;
    }
    *data = body;
    return true;
}

std::vector<BatchPackageResult> download_batch(
    const std::vector<BatchPackageSpec>& specs, int curl_jobs, int hash_jobs)
{
    if (curl_jobs < 1)
        curl_jobs = 1;
    if (hash_jobs < 1)
        hash_jobs = 1;
    std::vector<BatchPackageResult> results(specs.size());
    std::vector<BatchWork> work(specs.size());
    for (size_t i = 0; i < specs.size(); ++i) {
        results[i].name = specs[i].name;
        work[i].spec = &specs[i];
        work[i].curl_err[0] = 0;
    }
    if (!ensure_curl_global()) {
        // curl itself unavailable: every package fails, and no retry changes
        // that. Reported in the results, not thrown — this is a download
        // failure, not an internal error.
        for (auto& r : results)
            r.errors.push_back("curl_global_init failed");
        return results;
    }

    // Pass 0, then — exactly once, and only when something failed — one full
    // retry pass over the failures (candidates restart from the first URL:
    // a transient failure of an early mirror should not promote a later
    // mirror permanently). Each pass is a fresh phase-1 schedule followed by
    // a parallel phase-2 hash round.
    batch_transfer_pass(work, results, curl_jobs);
    batch_hash_pass(work, results, hash_jobs);
    std::vector<std::string> failed_names;
    for (size_t i = 0; i < work.size(); ++i) {
        if (!work[i].verified) {
            work[i].next_url = 0;
            failed_names.push_back(results[i].name);
        }
    }
    if (!failed_names.empty()) {
        std::string list;
        for (size_t i = 0; i < failed_names.size(); ++i) {
            if (i > 0)
                list += ", ";
            list += failed_names[i];
        }
        note("retrying " + std::to_string(failed_names.size()) +
             " failed download(s): " + list);
        batch_transfer_pass(work, results, curl_jobs);
        batch_hash_pass(work, results, hash_jobs);
    }

    // Assemble the results in spec order. Verified packages move their bytes
    // out; failed ones keep only their per-attempt error lines.
    for (size_t i = 0; i < work.size(); ++i) {
        BatchWork& w = work[i];
        BatchPackageResult& r = results[i];
        if (w.verified) {
            r.ok = true;
            r.data = std::move(w.buffer);
            r.hash = std::move(w.hash);
            r.url = w.attempt_url;
        } else {
            r.ok = false;
            r.data.clear();
            r.hash.clear();
            r.url.clear();
            if (r.errors.empty()) {
                // Degenerate spec (no candidate URLs): still report a
                // failure reason instead of a silently empty error list.
                r.errors.push_back("download of '" + r.name +
                                   "' has no candidate URLs");
            }
        }
    }
    return results;
}
