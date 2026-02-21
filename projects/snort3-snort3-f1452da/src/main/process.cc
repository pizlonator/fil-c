//--------------------------------------------------------------------------
// Copyright (C) 2014-2025 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "process.h"

#include <fcntl.h>
#include <grp.h>
#include <lua.h>
#include <openssl/crypto.h>
#include <pcap.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <zlib.h>

#ifdef HAVE_HYPERSCAN
#include <hs_compile.h>
#endif

#ifdef HAVE_LZMA
#include <lzma.h>
#endif

#ifdef HAVE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

#ifdef HAVE_LIBML
#include <libml.h>
#endif

#ifdef HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <dlfcn.h>
#endif

#ifdef HAVE_MALLOC_TRIM
#include <malloc.h>
#endif

extern "C" {
#include <daq.h>
}

#include <csignal>
#include <fstream>
#include <iostream>

#include "log/messages.h"
#include "helpers/markup.h"
#include "helpers/ring.h"
#include "helpers/sigsafe.h"
#include "packet_io/sfdaq.h"
#include "protocols/packet.h"   // For NUM_IP_PROTOS
#include "utils/cpp_macros.h"
#include "utils/stats.h"
#include "utils/snort_pcre.h"
#include "utils/util.h"

#include "main.h"
#include "oops_handler.h"
#include "snort_config.h"

using namespace snort;

#ifndef SIGNAL_SNORT_RELOAD
#define SIGNAL_SNORT_RELOAD        SIGHUP
#endif

#ifndef SIGNAL_SNORT_DUMP_STATS
#define SIGNAL_SNORT_DUMP_STATS    SIGUSR1
#endif

#ifndef SIGNAL_SNORT_ROTATE_STATS
#define SIGNAL_SNORT_ROTATE_STATS  SIGUSR2
#endif

// this one should not be changed by user
#define SIGNAL_SNORT_CHILD_READY   SIGCHLD

#ifndef SIGNAL_SNORT_READ_ATTR_TBL
#define SIGNAL_SNORT_READ_ATTR_TBL SIGURG
#endif

static const char* const pig_sig_names[PIG_SIG_MAX] =
{
    "none", "quit", "term", "int",
    "reload-config", "reload-attributes",
    "dump-stats", "rotate-stats"
};

static Ring<PigSignal> sig_ring(4);
static volatile sig_atomic_t child_ready_signal = 0;
static THREAD_LOCAL bool is_main_thread = false;

// Backup copies of the original fatal signal actions.  Kept here because they must
//  be trivially accessible to signal handlers.
#ifdef HAVE_SIGACTION
static struct
{
    int signal;
    struct sigaction original_sigaction;
}
original_sigactions[] =
{
    { SIGILL, { } },
    { SIGABRT, { } },
    { SIGBUS,  { } },
    { SIGFPE,  { } },
    { SIGSEGV, { } },
    { 0, { } },
};
#else
static struct
{
    int signal;
    sig_t original_sighandler;
}
original_sighandlers[] =
{
    { SIGILL,  SIG_DFL },
    { SIGABRT, SIG_DFL },
    { SIGBUS,  SIG_DFL },
    { SIGFPE,  SIG_DFL },
    { SIGSEGV, SIG_DFL },
    { 0, SIG_DFL },
};
#endif

static bool add_signal(int sig, sig_t);
static bool restore_signal(int sig, bool silent = false);

static bool exit_pronto = true;

void set_quick_exit(bool b)
{ exit_pronto = b; }

void set_main_thread()
{ is_main_thread = true; }

static void exit_handler(int signal)
{
    PigSignal s;
    const char* t;

    switch ( signal )
    {
        case SIGTERM: s = PIG_SIG_TERM; t = "term"; break;
        case SIGQUIT: s = PIG_SIG_QUIT; t = "quit"; break;
        case SIGINT:  s = PIG_SIG_INT;  t = "int";  break;
        default: return;
    }

    if ( exit_pronto )
    {
        SigSafePrinter(STDOUT_FILENO).printf("%s caught %s signal, exiting\n", get_prompt(), t);
        _exit(0);
    }

    sig_ring.put(s);
}

static void dirty_handler(int signal)
{
    SnortConfig::get_main_conf()->dirty_pig = true;
    exit_handler(signal);
}

static void dump_stats_handler(int /*signal*/)
{
    sig_ring.put(PIG_SIG_DUMP_STATS);
}

static void rotate_stats_handler(int /*signal*/)
{
    sig_ring.put(PIG_SIG_ROTATE_STATS);
}

static void reload_config_handler(int /*signal*/)
{
    sig_ring.put(PIG_SIG_RELOAD_CONFIG);
}

static void reload_attrib_handler(int /*signal*/)
{
    sig_ring.put(PIG_SIG_RELOAD_HOSTS);
}

static void child_ready_handler(int /*signal*/)
{
    child_ready_signal = 1;
}

#ifdef HAVE_LIBUNWIND
static void print_backtrace(SigSafePrinter& ssp)
{
    int ret;

    // grab the machine context and initialize the cursor
    unw_context_t context;
    if ((ret = unw_getcontext(&context)) < 0)
    {
        ssp.printf("unw_getcontext failed: %s (%d)\n", unw_strerror(ret), ret);
        return;
    }

    unw_cursor_t cursor;
    if ((ret = unw_init_local(&cursor, &context)) < 0)
    {
        ssp.printf("unw_init_local failed: %s (%d)\n", unw_strerror(ret), ret);
        return;
    }

    ssp.printf("Backtrace:\n");

    // walk the stack frames
    unsigned frame_num = 0;
    bool signal_frame_found = false;

    while ((ret = unw_step(&cursor)) > 0)
    {
        if (!signal_frame_found)
        {
            if (unw_is_signal_frame(&cursor))
            {
                signal_frame_found = true;
#ifdef __aarch64__
                // skip __kernel_rt_sigreturn in vDSO for arm
                continue;
#endif
            }
            else
                // skip printing any frames until we've found the frame that received the signal
                continue;
        }

        unw_word_t pc;
        if ((ret = unw_get_reg(&cursor, UNW_REG_IP, &pc)) < 0)
        {
            ssp.printf("unw_get_reg failed for instruction pointer: %s (%d)\n",
                    unw_strerror(ret), ret);
            return;
        }


        ssp.printf("  #%u 0x%x", frame_num, pc);

        char sym[1024];
        unw_word_t offset;
        ret = unw_get_proc_name(&cursor, sym, sizeof(sym), &offset);
        switch (-ret)
        {
            case 0:
                // fallthrough
            case UNW_ENOMEM:
                ssp.printf(" in %s+0x%x", sym, offset);
                break;
            case UNW_ENOINFO:
                // fallthrough
            case UNW_EUNSPEC:
                // fallthrough
            default:
                ssp.printf(" unw_get_proc_name failed: %s (%d)", unw_strerror(ret), ret);
                break;
        }

        unw_proc_info_t pip;
        if ((ret = unw_get_proc_info(&cursor, &pip)) < 0)
        {
            ssp.printf(" unw_get_proc_info failed: %s (%d)", unw_strerror(ret), ret);
        }
        else
        {
            Dl_info dlinfo;
            if (dladdr((void*) (uintptr_t) (pip.start_ip + offset), &dlinfo)
                && dlinfo.dli_fname && *dlinfo.dli_fname)
            {
                ssp.printf(" (%s @0x%x)", dlinfo.dli_fname, dlinfo.dli_fbase);
            }
        }

        ssp.printf("\n");

        frame_num++;
    }
    if (ret < 0)
        ssp.printf("unw_step failed: %s (%d)\n", unw_strerror(ret), ret);

    ssp.printf("\n");
}
#endif

static void oops_handler(int signal)
{
    // First things first, restore the original signal handler.
    restore_signal(signal, true);

    // Log the Snort version and signal caught.
    const char* sigstr = "???\n";
    switch (signal)
    {
        case SIGILL:
            sigstr = STRINGIFY(SIGILL) " (" STRINGIFY_MX(SIGILL) ")";
            break;
        case SIGABRT:
            sigstr = STRINGIFY(SIGABRT) " (" STRINGIFY_MX(SIGABRT) ")";
            break;
        case SIGBUS:
            sigstr = STRINGIFY(SIGBUS) " (" STRINGIFY_MX(SIGBUS) ")";
            break;
        case SIGFPE:
            sigstr = STRINGIFY(SIGFPE) " (" STRINGIFY_MX(SIGFPE) ")";
            break;
        case SIGSEGV:
            sigstr = STRINGIFY(SIGSEGV) " (" STRINGIFY_MX(SIGSEGV) ")";
            break;
    }
    SigSafePrinter ssp(STDERR_FILENO);
    ssp.printf("\nSnort (PID %u) caught fatal signal: %s\n", getpid(), sigstr);
#ifdef BUILD
    ssp.printf("Version: " VERSION " Build " BUILD "\n\n");
#else
    ssp.printf("Version: " VERSION "\n\n");
#endif

#ifdef HAVE_LIBUNWIND
    // Try to pretty-print a stack trace using libunwind to traverse the stack.
    print_backtrace(ssp);
#endif

    // FIXIT-L what should we capture if this is the main thread?
    if ( !is_main_thread )
        OopsHandler::handle_crash(STDERR_FILENO);

    if (ssp.flush() != 0)
        ssp.printf("Failed to flush output. Error: %s\n", strerror(errno));

    // Finally, raise the signal so that the original handler can handle it.
    raise(signal);
}

//-------------------------------------------------------------------------
// access methods
//-------------------------------------------------------------------------

PigSignal get_pending_signal()
{
    return sig_ring.get(PIG_SIG_NONE);
}

const char* get_signal_name(PigSignal s)
{
    if ( s >= PIG_SIG_MAX )
        return "error";

    return pig_sig_names[s];
}

//-------------------------------------------------------------------------
// signal management
//-------------------------------------------------------------------------

static bool add_signal(int sig, sig_t signal_handler)
{
#ifdef HAVE_SIGACTION
    struct sigaction action;
    // Mask all other signals while in the signal handler
    sigfillset(&action.sa_mask);
    // Make compatible system calls restartable across signals
    action.sa_flags = SA_RESTART;
    action.sa_handler = signal_handler;

    struct sigaction* old_action = nullptr;
    for (unsigned i = 0; original_sigactions[i].signal; i++)
    {
        if (original_sigactions[i].signal == sig)
        {
            old_action = &original_sigactions[i].original_sigaction;
            break;
        }
    }

    if (sigaction(sig, &action, old_action) != 0)
    {
        ErrorMessage("Could not add handler for signal %d: %s (%d)\n", sig, get_error(errno), errno);
        return false;
    }
#else
    sig_t original_handler = signal(sig, signal_handler);
    if (original_handler == SIG_ERR)
    {
        ErrorMessage("Could not add handler for signal %d: %s (%d)\n", sig, get_error(errno), errno);
        return false;
    }

    for (unsigned i = 0; original_sighandlers[i].signal; i++)
    {
        if (original_sighandlers[i].signal == sig)
        {
            original_sighandlers[i].original_sighandler = original_handler;
            break;
        }
    }
#endif

    return true;
}

static bool restore_signal(int sig, bool silent)
{
#ifdef HAVE_SIGACTION
    struct sigaction* new_action = nullptr;
    struct sigaction action;

    for (unsigned i = 0; original_sigactions[i].signal; i++)
    {
        if (original_sigactions[i].signal == sig)
        {
            new_action = &original_sigactions[i].original_sigaction;
            break;
        }
    }

    if (!new_action)
    {
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        action.sa_handler = SIG_DFL;
        new_action = &action;
    }

    if (sigaction(sig, new_action, nullptr) != 0)
    {
        if (!silent)
            ErrorMessage("Could not restore handler for signal %d: %s (%d)\n", sig, get_error(errno), errno);
        return false;
    }
#else
    sig_t signal_handler = SIG_DFL;

    for (unsigned i = 0; original_sighandlers[i].signal; i++)
    {
        if (original_sighandlers[i].signal == sig)
        {
            signal_handler = original_sighandlers[i].original_sighandler;
            break;
        }
    }

    if (signal(sig, signal_handler) == SIG_ERR)
    {
        if (!silent)
            ErrorMessage("Could not restore handler for signal %d: %s (%d)\n", sig, get_error(errno), errno);
        return false;
    }
#endif

    return true;
}

void install_oops_handler()
{
    add_signal(SIGILL, oops_handler);
    add_signal(SIGABRT, oops_handler);
    add_signal(SIGBUS, oops_handler);
    add_signal(SIGFPE, oops_handler);
    add_signal(SIGSEGV, oops_handler);
}

void init_signals()
{
    sigset_t set;

    sigemptyset(&set);
    // FIXIT-L this is undefined for multithreaded apps
    sigprocmask(SIG_SETMASK, &set, nullptr);

    // First things first, install the crash handler
    install_oops_handler();

    // Ignore SIGPIPE for now (it's not particularly actionable in a multithreaded program)
    add_signal(SIGPIPE, SIG_IGN);

    // Set up a clean exit when expected shutdown signals come along
    add_signal(SIGTERM, exit_handler);
    add_signal(SIGINT, exit_handler);
    add_signal(SIGQUIT, dirty_handler);

    // Finally, set up signal handlers for custom Snort actions
    add_signal(SIGNAL_SNORT_DUMP_STATS, dump_stats_handler);
    add_signal(SIGNAL_SNORT_ROTATE_STATS, rotate_stats_handler);
    add_signal(SIGNAL_SNORT_RELOAD, reload_config_handler);
    add_signal(SIGNAL_SNORT_READ_ATTR_TBL, reload_attrib_handler);

    // Errno will have potentially been left set from a failed handler installation
    errno = 0;
}

void remove_oops_handler()
{
    restore_signal(SIGILL);
    restore_signal(SIGABRT);
    restore_signal(SIGBUS);
    restore_signal(SIGFPE);
    restore_signal(SIGSEGV);
}

void term_signals()
{
    restore_signal(SIGNAL_SNORT_DUMP_STATS);
    restore_signal(SIGNAL_SNORT_ROTATE_STATS);
    restore_signal(SIGNAL_SNORT_RELOAD);
    restore_signal(SIGNAL_SNORT_READ_ATTR_TBL);

    restore_signal(SIGTERM);
    restore_signal(SIGINT);
    restore_signal(SIGQUIT);

    restore_signal(SIGPIPE);

    remove_oops_handler();
}

static void help_signal(unsigned n, const char* name, const char* h)
{
    std::cout << Markup::item();

    std::cout << Markup::emphasis_on();
    std::cout << name;
    std::cout << Markup::emphasis_off();

    std::cout << "(" << n << "): " << h << std::endl;
}

void help_signals()
{
    help_signal(SIGTERM, "term", "shutdown normally");
    help_signal(SIGINT,  "int", "shutdown normally");
    help_signal(SIGQUIT, "quit", "shutdown as if started with --dirty-pig");

    help_signal(SIGNAL_SNORT_DUMP_STATS,  "stats", "dump stats to stdout");
    help_signal(SIGNAL_SNORT_ROTATE_STATS,  "rotate", "rotate stats files");
    help_signal(SIGNAL_SNORT_RELOAD,  "reload", "reload config file");
    help_signal(SIGNAL_SNORT_READ_ATTR_TBL,  "hosts", "reload hosts file");
}

//-------------------------------------------------------------------------
// daemonize
//-------------------------------------------------------------------------

static void snuff_stdio()
{
    // Redirect stdin to /dev/null
    if (freopen("/dev/null", "r", stdin) == nullptr)
        FatalError("failed to snuff stdin - %s", get_error(errno));

    // Redirect stdout to /dev/null
    if (freopen("/dev/null", "w", stdout) == nullptr)
        FatalError("failed to snuff stdout - %s", get_error(errno));
 
    // Redirect stderr to /dev/null
    if (freopen("/dev/null", "w", stderr) == nullptr)
        FatalError("failed to snuff stderr - %s", get_error(errno));
}

// All threads need to be created after daemonizing.  If created in the
// parent thread, when it goes away, so will all of the threads.  The child
// does not "inherit" threads created in the parent.

// Similar to daemon(1, 0).  We also add a signal to confirm child was at
// least started ok.  Should daemon() become posix compliant, we can switch
// to that.  We can also consider updating to have the child execve().
// Note that there are system tools for daemonization these days.  See
// daemon(3) and search deprecated daemonomicon for more.
void daemonize()
{
    // don't daemonize more than once
    if ( getppid() == 1 )
        return;

    LogMessage("initializing daemon mode\n");

    // register signal handler so that parent can trap signal
    add_signal(SIGNAL_SNORT_CHILD_READY, child_ready_handler);

    pid_t cpid = fork();

    if ( cpid < 0 )
        FatalError("fork failed - %s", get_error(errno));

    if ( cpid > 0 )
    {
        // parent
        int i = 0;

        while ( !child_ready_signal and i++ < 10 )
            sleep(1);

        if ( !child_ready_signal )
            FatalError("daemonization failed");
        else
            LogMessage("child process is %d\n", cpid);

        exit(0);
    }

    // child
    errno = 0;  // disregard any inherited issues

    setsid();

    if ( errno )
        FatalError("failed to setsid - %s", get_error(errno));

    if ( SnortConfig::log_quiet() or SnortConfig::log_syslog() )
        snuff_stdio();

    pid_t ppid = getppid();

    if ( kill(ppid, SIGNAL_SNORT_CHILD_READY) )
        LogMessage("daemonization incomplete, failed to signal parent "
            "%d: %s\n", ppid, get_error(errno));
}

//-------------------------------------------------------------------------
// heap stats
//-------------------------------------------------------------------------

void trim_heap()
{
#ifdef HAVE_MALLOC_TRIM
    malloc_trim(0);
#endif
}

//-------------------------------------------------------------------------
// other foo
//-------------------------------------------------------------------------

// Store interesting data in memory that would not otherwise be visible
// in a CORE(5) file

#ifdef BUILD
    #define SNORT_VERSION_STRING ("### Snort Version " VERSION " Build " BUILD "\n")
#else
    #define SNORT_VERSION_STRING ("### Snort Version " VERSION "\n")
#endif
#define SNORT_VERSION_STRLEN sizeof(SNORT_VERSION_STRING)
char __snort_version_string[SNORT_VERSION_STRLEN];

void StoreSnortInfoStrings()
{
    strncpy(__snort_version_string, SNORT_VERSION_STRING,
        sizeof(__snort_version_string));
}

#undef SNORT_VERSION_STRING
#undef SNORT_VERSION_STRLEN

int DisplayBanner()
{
    const char* ljv = LUA_VERSION;
    while ( *ljv && !isdigit(*ljv) )
        ++ljv;

    char pcre2_version[64] { };
    int pcre2_version_size = pcre2_config(PCRE2_CONFIG_VERSION, pcre2_version);

    if (pcre2_version_size <= 0 or static_cast<size_t>(pcre2_version_size) > sizeof(pcre2_version))
        abort();

    LogMessage("\n");
    LogMessage("   ,,_     -*> Snort++ <*-\n");
#ifdef BUILD
    LogMessage("  o\"  )~   Version %s (Build %s)\n", VERSION, BUILD);
#else
    LogMessage("  o\"  )~   Version %s\n", VERSION);
#endif
    LogMessage("   ''''    By Martin Roesch & The Snort Team\n");
    LogMessage("           http://snort.org/contact#team\n");
    LogMessage("           Copyright (C) 2014-2025 Cisco and/or its affiliates."
                           " All rights reserved.\n");
    LogMessage("           Copyright (C) 1998-2013 Sourcefire, Inc., et al.\n");
    LogMessage("           Using DAQ version %s\n", daq_version_string());
#ifdef HAVE_HYPERSCAN
    LogMessage("           Using Hyperscan version %s\n", hs_version());
#endif
#ifdef HAVE_JEMALLOC
    const char* jv;
    size_t sz = sizeof(jv);
    mallctl("version", &jv,  &sz, NULL, 0);
    LogMessage("           Using Jemalloc version %s\n", jv);
#endif
#ifdef HAVE_LIBML
    LogMessage("           Using LibML version %s\n", libml::version());
#endif
    LogMessage("           Using %s\n", pcap_lib_version());
    LogMessage("           Using Lua version %s\n", ljv);
#ifdef HAVE_LZMA
    LogMessage("           Using LZMA version %s\n", lzma_version_string());
#endif
    LogMessage("           Using %s\n", OpenSSL_version(SSLEAY_VERSION));
    LogMessage("           Using PCRE2 version %s\n", (char*) pcre2_version);
    LogMessage("           Using ZLIB version %s\n", zlib_version);

    LogMessage("\n");

    return 0;
}

// get offset seconds from GMT
int gmt2local(time_t t)
{
    if (t == 0)
        t = time(nullptr);

    struct tm gmt;
    struct tm* lt = gmtime_r(&t, &gmt);
    if (lt == nullptr)
        return 0;

    struct tm loc;
    localtime_r(&t, &loc);

    int dt = (loc.tm_hour - gmt.tm_hour) * 60 * 60 +
        (loc.tm_min - gmt.tm_min) * 60;

    int dir = loc.tm_year - gmt.tm_year;

    if (dir == 0)
        dir = loc.tm_yday - gmt.tm_yday;

    dt += dir * 24 * 60 * 60;

    return(dt);
}

static FILE* pid_lockfile = nullptr;
static FILE* pid_file = nullptr;

void CreatePidFile(pid_t pid)
{
    SnortConfig* sc = SnortConfig::get_main_conf();

    sc->pid_filename = sc->log_dir + '/' + sc->pid_filename;

    std::string pid_lockfilename;

    if ( !sc->no_lock_pid_file() )
    {
        pid_lockfilename = sc->pid_filename;
        pid_lockfilename += ".lck";

        /* First, lock the PID file */
        pid_lockfile = fopen(pid_lockfilename.c_str(), "w");

        if ( pid_lockfile )
        {
            struct flock lock;
            int lock_fd = fileno(pid_lockfile);

            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = 0;
            lock.l_len = 0;

            if (fcntl(lock_fd, F_SETLK, &lock) == -1)
            {
                ClosePidFile();
                ParseError("Failed to Lock PID File \"%s\" for PID \"%d\"",
                    sc->pid_filename.c_str(), (int)pid);
                return;
            }
        }
    }

    /* Okay, were able to lock PID file, now open and write PID */
    pid_file = fopen(sc->pid_filename.c_str(), "w");
    if (pid_file)
    {
        LogMessage("Writing PID \"%d\" to file \"%s\"\n", (int)pid,
            sc->pid_filename.c_str());
        fprintf(pid_file, "%d\n", (int)pid);
        fflush(pid_file);
    }
    else
    {
        if (pid_lockfile)
        {
            fclose(pid_lockfile);
            pid_lockfile = nullptr;
        }
        const char* error = get_error(errno);
        ErrorMessage("Failed to create pid file %s, Error: %s\n",
            sc->pid_filename.c_str(), error);
        sc->pid_filename.clear();
    }
    if ( !pid_lockfilename.empty() )
        unlink(pid_lockfilename.c_str());
}

void ClosePidFile()
{
    if (pid_file)
    {
        fclose(pid_file);
        pid_file = nullptr;
    }
    if (pid_lockfile)
    {
        fclose(pid_lockfile);
        pid_lockfile = nullptr;
    }
}

// set safe UserID and GroupID, if needed
bool SetUidGid(int user_id, int group_id)
{
    // Were any changes requested?
    if (group_id == -1 && user_id == -1)
        return true;

    if (group_id != -1)
    {
        if (setgid(group_id) < 0)
        {
            ParseError("Cannot set GID: %d", group_id);
            return false;
        }
        LogMessage("Set GID to %d\n", group_id);
    }

    if (user_id != -1)
    {
        if (setuid(user_id) < 0)
        {
            ParseError("Cannot set UID: %d", user_id);
            return false;
        }
        LogMessage("Set UID to %d\n", user_id);
    }

    return true;
}

// set the groups of the process based on the UserID with the GroupID added
void InitGroups(int user_id, int group_id)
{
    if ((user_id != -1) && (getuid() == 0))
    {
        struct passwd* pw = getpwuid(user_id);  // main thread only

        if (pw != nullptr)
        {
            /* getpwuid and initgroups may use the same static buffers */
            char* username = snort_strdup(pw->pw_name);

            if (initgroups(username, group_id) < 0)
                ParseError("Can not initgroups(%s,%d)", username, group_id);

            snort_free(username);
        }

        /** Just to be on the safe side... **/
        endgrent();
        endpwent();
    }
}

//-------------------------------------------------------------------------

// read the BPF filters in from a file, return the processed BPF string
std::string read_infile(const char* key, const char* fname)
{
    int fd = open(fname, O_RDONLY);
    struct stat buf;

    if (fd < 0)
    {
        ErrorMessage("Failed to open file: %s with error: %s", fname, get_error(errno));
        return "";
    }

    if (fstat(fd, &buf) < 0)
    {
        ParseError("can't stat %s: %s", fname, get_error(errno));
        close(fd);
        return "";
    }

    //check that its a regular file and not a directory or special file
    if (!S_ISREG(buf.st_mode) )
    {
        ParseError("not a regular file: %s", fname);
        close(fd);
        return "";
    }

    std::string line;
    std::ifstream bpf_file(fname);

    if (bpf_file.is_open())
    {
        std::stringstream file_content;
        file_content << bpf_file.rdbuf();
        line = file_content.str();

        bpf_file.close();
    }
    else
    {
        ParseError("can't open file %s = %s: %s", key, fname, get_error(errno));
        close(fd);
        return "";
    }
    close(fd);
    return line;
}

typedef char PathBuf[PATH_MAX+1];

static const char* CurrentWorkingDir(PathBuf& buf)
{
    if ( !getcwd(buf, sizeof(buf)-1) )
        return nullptr;

    buf[sizeof(buf)-1] = '\0';
    return buf;
}

static char* GetAbsolutePath(const char* dir, PathBuf& buf)
{
    assert(dir);
    errno = 0;

    if ( !realpath(dir, buf) )
    {
        LogMessage("Couldn't determine absolute path for '%s': %s\n", dir, get_error(errno));
        return nullptr;
    }

    return buf;
}

// Chroot and adjust the log_dir reference
bool EnterChroot(std::string& root_dir, std::string& log_dir)
{
    if (log_dir.empty())
    {
        ParseError("Log directory not specified");
        return false;
    }
    PathBuf pwd;
    PathBuf abs_log_dir;

    if ( !GetAbsolutePath(log_dir.c_str(), abs_log_dir) )
        return false;

    /* change to the desired root directory */
    if (chdir(root_dir.c_str()) != 0)
    {
        ParseError("EnterChroot: Can not chdir to \"%s\": %s", root_dir.c_str(),
            get_error(errno));
        return false;
    }

    /* always returns an absolute pathname */
    const char* abs_root_dir = CurrentWorkingDir(pwd);
    if (!abs_root_dir)
    {
        ParseError("Couldn't retrieve current working directory");
        return false;
    }
    size_t abs_root_dir_len = strlen(abs_root_dir);

    if (strncmp(abs_root_dir, abs_log_dir, abs_root_dir_len))
    {
        ParseError("Specified log directory is not contained with the chroot jail");
        return false;
    }

    if (chroot(abs_root_dir) < 0)
    {
        ParseError("Can not chroot to \"%s\": absolute: %s: %s",
            root_dir.c_str(), abs_root_dir, get_error(errno));
        return false;
    }


    /* Immediately change to the root directory of the jail. */
    if (chdir("/") < 0)
    {
        ParseError("Can not chdir to \"/\" after chroot: %s",
            get_error(errno));
        return false;
    }


    if (abs_root_dir_len >= strlen(abs_log_dir))
        log_dir = "/";
    else
        log_dir = abs_log_dir + abs_root_dir_len;


    LogMessage("Chroot directory = %s\n", root_dir.c_str());

    return true;
}

#if defined(NOCOREFILE)
void SetNoCores()
{
    struct rlimit rlim;

    getrlimit(RLIMIT_CORE, &rlim);
    rlim.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &rlim);
}
#endif

