/*
 * Copyright © 2001,2002 Red Hat, Inc.
 * Copyright © 2009, 2010, 2019, 2020 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "spawn.hh"

#include "libc-glue.hh"

#include <vte/vte.h>
#include "vteptyinternal.hh"
#include "vtespawn.hh"
#include "debug.h"
#include "reaper.hh"

#include <assert.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>

#include <glib/gi18n-lib.h>
#include <glib-unix.h>

#include "glib-glue.hh"

#if WITH_SYSTEMD
#include "systemd.hh"
#endif

#include "vtedefines.hh"

#include "missing.hh"

namespace vte::base {

static int
set_cloexec_cb(void* data,
               int fd)
{
        if (fd >= *reinterpret_cast<int*>(data)) {
                auto r = vte::libc::fd_set_cloexec(fd);
                /* Ignore EBADF because the libc or fallback implementation
                 * of fdwalk may call this function on invalid file descriptors.
                 */
                if (r < 0 && errno == EBADF)
                        r = 0;
                return r;
        }
        return 0;
}

static int
cloexec_from(int fd)
{
#ifdef CLOSE_RANGE_CLOEXEC
        /* First, try close_range(CLOEXEC) which is faster than the methods
         * below, and works even if /proc is not available.
         */
        auto const res = close_range(fd, -1, CLOSE_RANGE_CLOEXEC);
        if (res == 0)
                return 0;
        if (res == -1 &&
            errno != ENOSYS /* old kernel, or not supported on this platform */ &&
            errno != EINVAL /* flags not supported */)
                return res;
#endif /* CLOSE_RANGE_CLOEXEC */

        /* Fall back to fdwalk */
        return fdwalk(set_cloexec_cb, &fd);
}

static bool
make_pipe(int flags,
          vte::libc::FD& read_fd,
          vte::libc::FD& write_fd,
          vte::glib::Error& error)
{
#if !GLIB_CHECK_VERSION(2, 78, 0)
        if constexpr (O_CLOEXEC != FD_CLOEXEC) {
                if (flags & O_CLOEXEC) {
                        flags &= ~O_CLOEXEC;
                        flags |= FD_CLOEXEC;
                }
        }
#endif // !glib 2.78

        int fds[2] = { -1, -1 };
        if (!g_unix_open_pipe(fds, flags, error))
                return false;

        read_fd = fds[0];
        write_fd = fds[1];
        return true;
}

/* Code for read_ints copied from glib/glib/gspawn.c, there under LGPL2.1+,
 * and used here under LGPL3+.
 *
 * Copyright 2000 Red Hat, Inc.
 */
static bool
read_ints(int fd,
          int* buf,
          int n_ints_in_buf,
          int *n_ints_read,
          int timeout,
          GPollFD *cancellable_pollfd,
          vte::glib::Error& error)
{
        GPollFD pollfds[2];
        auto n_pollfds = unsigned{0};

        if (timeout >= 0 || cancellable_pollfd != nullptr) {
                if (vte::libc::fd_set_nonblock(fd) < 0) {
                        auto errsv = vte::libc::ErrnoSaver{};
                        error.set(G_IO_ERROR, g_io_error_from_errno(errsv),
                                  _("Failed to set pipe nonblocking: %s"),
                                  g_strerror(errsv));
                        return false;
                }

                pollfds[0].fd = fd;
                pollfds[0].events = G_IO_IN | G_IO_HUP | G_IO_ERR;
                n_pollfds = 1;

                if (cancellable_pollfd != nullptr) {
                        pollfds[1] = *cancellable_pollfd;
                        n_pollfds = 2;
                }
        } else
                n_pollfds = 0;

        auto start_time = int64_t{0};
        if (timeout >= 0)
                start_time = g_get_monotonic_time();

        auto bytes = size_t{0};
        while (true) {
                if (bytes >= sizeof(int)*2)
                        break; /* give up, who knows what happened, should not be
                                * possible.
                                */

        again:
                if (n_pollfds != 0) {
                        pollfds[0].revents = pollfds[1].revents = 0;

                        auto const r = g_poll(pollfds, n_pollfds, timeout);

                        /* Update timeout */
                        if (timeout >= 0) {
                                timeout -= (g_get_monotonic_time () - start_time) / 1000;
                                if (timeout < 0)
                                        timeout = 0;
                        }

                        if (r < 0 && errno == EINTR)
                                goto again;
                        if (r < 0) {
                                auto errsv = vte::libc::ErrnoSaver{};
                                error.set(G_IO_ERROR, g_io_error_from_errno(errsv),
                                          _("poll error: %s"),
                                          g_strerror(errsv));
                                return false;
                        }
                        if (r == 0) {
                                auto errsv = vte::libc::ErrnoSaver{};
                                error.set_literal(G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                                                  _("Operation timed out"));
                                return false;
                        }

                        /* If the passed-in poll FD becomes readable, that's the signal
                         * to cancel the operation. We do NOT actually read from its FD!
                         */
                        if (n_pollfds == 2 && pollfds[1].revents) {
                                auto errsv = vte::libc::ErrnoSaver{};
                                error.set_literal(G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                                  _("Operation was cancelled"));
                                return false;
                        }

                        /* Now we know we can try to read from the child */
                }

                auto const chunk = read(fd,
                                        ((char*)buf) + bytes,
                                        sizeof(int) * n_ints_in_buf - bytes);
                if (chunk < 0 && errno == EINTR)
                        goto again;

                if (chunk < 0) {
                        auto errsv = vte::libc::ErrnoSaver{};

                        /* Some weird shit happened, bail out */
                        error.set(G_IO_ERROR, g_io_error_from_errno(errsv),
                                  _("Failed to read from child pipe (%s)"),
                                  g_strerror(errsv));

                        return false;
                } else if (chunk == 0)
                        break; /* EOF */
                else /* chunk > 0 */
                        bytes += chunk;
        }

        *n_ints_read = int(bytes / sizeof(int));

        return true;
}

static char**
merge_environ(char** envp /* consumed */,
              char const* cwd,
              bool inherit)
{
        auto table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

        if (inherit) {
                auto parent_environ = vte::glib::take_strv(g_get_environ());
                if (parent_environ) {
                        auto penvv = parent_environ.get();
                        for (auto i = unsigned{0}; penvv[i] != NULL; ++i) {
                                auto name = g_strdup(penvv[i]);
                                auto value = strchr(name, '=');
                                if (value) {
                                        *value = '\0';
                                        value = g_strdup(value + 1);
                                }
                                g_hash_table_replace(table, name, value); /* takes ownership of name and value */
                        }
                }
        }

        /* Make sure the one in envp overrides the default. */
        g_hash_table_replace(table, g_strdup("TERM"), g_strdup(VTE_TERMINFO_NAME));

        if (envp) {
                for (auto i = unsigned{0}; envp[i] != nullptr; ++i) {
                        auto name = g_strdup(envp[i]);
                        auto value = strchr(name, '=');
                        if (value) {
                                *value = '\0';
                                value = g_strdup(value + 1);
                        }
                        g_hash_table_replace(table, name, value); /* takes ownership of name and value */
                }

                g_strfreev(envp);
        }

        /* Always set this ourself, not allowing replacing from envp */
        g_hash_table_replace(table, g_strdup("VTE_VERSION"), g_strdup_printf("%u", VTE_VERSION_NUMERIC));
        g_hash_table_replace(table, g_strdup("COLORTERM"), g_strdup("truecolor"));

        /* We need to put the working directory also in PWD, so that
         * e.g. bash starts in the right directory if @directory is a symlink.
         * See bug #502146 and #758452.
         *
         * If chdir to cwd fails, and we fall back to the fallback cwd, PWD will
         * be set to a directory != the actual working directory, but that's not
         * a problem since PWD is only used when it's equal to the actual working
         * directory.
         */
        if (cwd)
                g_hash_table_replace(table, g_strdup("PWD"), g_strdup(cwd));

        auto array = g_ptr_array_sized_new(g_hash_table_size(table) + 1);

        auto iter = GHashTableIter{};
        g_hash_table_iter_init(&iter, table);
        char *name, *value;
        while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value)) {
                if (value)
                        g_ptr_array_add(array, g_strconcat(name, "=", value, nullptr));
        }
        g_hash_table_destroy(table);
        g_ptr_array_add(array, nullptr);

        return reinterpret_cast<char**>(g_ptr_array_free(array, false));
}

void
SpawnContext::prepare_environ()
{
        m_envv = vte::glib::take_strv(merge_environ(m_envv.release(), m_cwd.get(), inherit_environ()));
}

char const*
SpawnContext::search_path() const noexcept
{
        auto const path = m_search_path ? g_environ_getenv(environ(), "PATH") : nullptr;
        return path ? : "/bin:/usr/bin";
}

size_t
SpawnContext::workbuf_size() const noexcept
{
        auto const path = search_path();
        return std::max(path ? strlen(path) + strlen(arg0()) + 2 /* leading '/' plus NUL terminator */ : 0,
                        (g_strv_length(argv()) + 2) * sizeof(char*));
}

/* This function is called between fork and execve/_exit and so must be
 * async-signal-safe; see man:signal-safety(7).
 */
SpawnContext::ExecError
SpawnContext::exec(vte::libc::FD& child_report_error_pipe_write,
                   void* workbuf,
                   size_t workbufsize) noexcept
{
        /* NOTE! This function must not rely on smart pointers to
         * release their object, since the destructors are NOT run
         * when the exec succeeds!
         */

        _VTE_DEBUG_IF(VTE_DEBUG_MISC) {
                g_printerr ("Spawning command:\n");
                auto argv = m_argv.get();
                for (auto i = 0; argv[i] != NULL; i++) {
                        g_printerr("    argv[%d] = %s\n", i, argv[i]);
                }
                if (m_envv) {
                        auto envv = m_envv.get();
                        for (auto i = 0; envv[i] != NULL; i++) {
                                g_printerr("    env[%d] = %s\n", i, envv[i]);
                        }
                }
                g_printerr("    directory: %s\n",
                           m_cwd ? m_cwd.get() : "(none)");
        }

        /* Unblock all signals */
        sigset_t set;
        sigemptyset(&set);
        if (pthread_sigmask(SIG_SETMASK, &set, nullptr) == -1) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                 "pthread_sigmask", g_strerror(errsv));
                return ExecError::SIGMASK;
        }

        /* Reset the handlers for all signals to their defaults.  The parent
         * (or one of the libraries it links to) may have changed one to be ignored.
         * Esp. SIGPIPE since it ensures this process terminates when we write
         * to child_err_report_pipe after the parent has exited.
         */
        for (auto n = int{1}; n < NSIG; ++n) {
                if (n == SIGSTOP || n == SIGKILL)
                        continue;

                signal(n, SIG_DFL);
        }

        /* Close all file descriptors on exec. Note that this includes
         * child_error_report_pipe_write, which keeps the parent from blocking
         * forever on the other end of that pipe.
         */
        if (cloexec_from(3) < 0)
                return ExecError::FDWALK;

        /* Working directory */
        if (m_cwd && chdir(m_cwd.get()) < 0) {
                /* If the fallback fails too, make sure to return the errno
                 * from the original cwd, not the fallback cwd.
                 */
                auto errsv = vte::libc::ErrnoSaver{};
                if (m_fallback_cwd && chdir(m_fallback_cwd.get()) < 0)
                        return ExecError::CHDIR;

                errsv.reset();
        }

        /* Session */
        if (!(pty()->flags() & VTE_PTY_NO_SESSION)) {
                /* This starts a new session; we become its process-group leader,
                 * and lose our controlling TTY.
                 */
                _vte_debug_print(VTE_DEBUG_PTY, "Starting new session\n");
                if (setsid() == -1) {
                        auto errsv = vte::libc::ErrnoSaver{};
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                         "setsid", g_strerror(errsv));
                        return ExecError::SETSID;
                }
        }

        auto peer_fd = pty()->get_peer(true /* cloexec */);
        if (peer_fd == -1)
                return ExecError::GETPTPEER;

#ifdef TIOCSCTTY
        /* On linux, opening the PTY peer above already made it our controlling TTY (since
         * previously there was none, after the setsid() call). However, it appears that e.g.
         * on *BSD, that doesn't happen, so we need this explicit ioctl here.
         */
        if (!(pty()->flags() & VTE_PTY_NO_CTTY)) {
                if (ioctl(peer_fd, TIOCSCTTY, peer_fd) != 0) {
                        auto errsv = vte::libc::ErrnoSaver{};
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                         "ioctl(TIOCSCTTY)", g_strerror(errsv));
                        return ExecError::SCTTY;
                }
        }
#endif

        /* Replace the placeholders with the FD assignment for the PTY */
        m_fd_map[0].first = peer_fd;
        m_fd_map[1].first = peer_fd;
        m_fd_map[2].first = peer_fd;

        /* Assign FDs */
        auto const n_fd_map = m_fd_map.size();
        for (auto i = size_t{0}; i < n_fd_map; ++i) {
                auto const [source_fd, target_fd] = m_fd_map[i];

                /* -1 means the source_fd is only in the map so that it can
                 * be checked for conflicts with other target FDs. It may be
                 * re-assigned while relocating other FDs.
                 */
                if (target_fd == -1)
                        continue;

                /* We want to move source_fd to target_fd */

                if (target_fd != source_fd) {

                        /* Need to check if target_fd is an FDs in the FD list.
                         * If so, need to re-assign the source FD(s) first.
                         */
                        for (auto j = size_t{0}; j < n_fd_map; ++j) {
                                auto const [from_fd, to_fd] = m_fd_map[j];

                                if (from_fd != target_fd)
                                        continue;

                                /* Duplicate from_fd to any free FD number, which will
                                 * be != from_fd/target_fd.
                                 */
                                auto new_from_fd = vte::libc::fd_dup_cloexec(from_fd, 3);
                                if (new_from_fd == -1)
                                        return ExecError::DUP;

                                for (auto k = j; k < n_fd_map; ++k) {
                                        if (m_fd_map[k].first == from_fd)
                                                m_fd_map[k].first = new_from_fd;
                                }

                                /* Now that we have updated all references to the old
                                 * source FD in the map, we can close the FD. (Not
                                 * strictly necessary since it'll be dup2'd over
                                 * anyway.)
                                 */
                                if (from_fd == child_report_error_pipe_write.get()) {
                                        /* Need to report the new pipe write FD back to the caller. */
                                        child_report_error_pipe_write = new_from_fd;
                                } else {
                                        (void)close(from_fd);
                                }

                                /* We have replaced *all* instances of target_fd as a
                                 * source with new_from_fd, so we don't need to continue
                                 * with the loop.
                                 */
                                break;
                        }

                        /* Now we know that target_fd can be safely overwritten. */
                        if (vte::libc::fd_dup2(source_fd, target_fd) == -1)
                                return ExecError::DUP2;
                } else {
                        /* Already assigned correctly, but need to remove FD_CLOEXEC */
                        if (vte::libc::fd_unset_cloexec(target_fd) == -1)
                                return ExecError::UNSET_CLOEXEC;

                }

                /* Mark source in the map as done */
                m_fd_map[i].first = -1;
        }

        /* Finally call an extra child setup */
        if (m_child_setup)
                m_child_setup(m_child_setup_data.get());

        /* exec */
        _vte_execute(arg0(),
                     argv(),
                     environ(),
                     search_path(),
                     workbuf,
                     workbufsize);

        /* If we get here, exec failed */
        return ExecError::EXEC;
}

SpawnOperation::~SpawnOperation()
{
        if (m_cancellable && m_cancellable_pollfd.fd != -1)
                g_cancellable_release_fd(m_cancellable.get());

        if (m_pid != -1) {
                /* Since we're not passing the PID back to the caller,
                 * we need to kill and reap it ourself.
                 */
                if (m_kill_pid) {
                        auto const pgrp = getpgid(m_pid);
                        /* Make sure not to kill ourself, if the child died before
                         * it could call setsid()!
                         */
                        if (pgrp != -1 && pgrp != getpgid(getpid()))
                                kill(-pgrp, SIGHUP);

                        kill(m_pid, SIGHUP);
                }

                vte_reaper_add_child(m_pid);
        }
}

bool
SpawnOperation::prepare(vte::glib::Error& error)
{
#if !WITH_SYSTEMD
        if (context().require_systemd_scope()) {
                error.set_literal(G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                  "systemd not available");
                return false;
        }
#endif

        if (m_cancellable &&
            !g_cancellable_make_pollfd(m_cancellable.get(), &m_cancellable_pollfd)) {
                auto errsv = vte::libc::ErrnoSaver{};
                error.set(G_IO_ERROR,
                          g_io_error_from_errno(errsv),
                          "Failed to make cancellable pollfd: %s",
                          g_strerror(errsv));
                return false;
        }

        auto child_report_error_pipe_read = vte::libc::FD{};
        auto child_report_error_pipe_write = vte::libc::FD{};
        if (!make_pipe(O_CLOEXEC,
                       child_report_error_pipe_read,
                       child_report_error_pipe_write,
                       error))
                return false;

        /* allocate workbuf for SpawnContext::Exec() */
        auto const workbufsize = context().workbuf_size();
        auto workbuf = vte::glib::take_free_ptr(g_try_malloc(workbufsize));
        if (!workbuf) {
                auto errsv = vte::libc::ErrnoSaver{};
                error.set(G_IO_ERROR,
                          g_io_error_from_errno(errsv),
                          "Failed to allocate workbuf: %s",
                          g_strerror(errsv));
                return false;
        }

        /* Need to add the write end of the pipe to the FD map, so
         * that the FD re-arranging code knows it needs to preserve
         * the FD and not dup2 over it.
         * Target -1 means that no actual re-assignment will take place.
         */
        context().add_map_fd(child_report_error_pipe_write.get(), -1);

        auto const pid = fork();
        if (pid < 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                error.set(G_IO_ERROR,
                          g_io_error_from_errno(errsv),
                          "Failed to fork: %s",
                          g_strerror(errsv));
                return false;
        }

        if (pid == 0) {
                /* Child */

                child_report_error_pipe_read.reset();

                auto const err = context().exec(child_report_error_pipe_write,
                                                workbuf.get(), workbufsize);

                /* Manually free the workbuf */
                g_free(workbuf.release());

                /* If we get here, exec failed. Write the error to the pipe and exit. */
                _vte_write_err(child_report_error_pipe_write.get(), int(err));
                _exit(127);
                return true;
        }

        /* Parent */
        m_pid = pid;
        m_child_report_error_pipe_read = std::move(child_report_error_pipe_read);

        return true;
}

bool
SpawnOperation::run(vte::glib::Error& error) noexcept
{
        int buf[2] = {G_SPAWN_ERROR_FAILED, ENOSYS};
        auto n_read = int{0};

        if (!read_ints(m_child_report_error_pipe_read.get(),
                       buf, 2,
                       &n_read,
                       m_timeout,
                       &m_cancellable_pollfd,
                       error))
                return false;

        if (n_read >= 2) {
                /* Spawn failed. buf[0] contains an error from
                 * SpawnContext::ExecError, and buf[1] contains errno.
                 */

                /* The process will have called _exit(127) already, no need to kill it */
                m_kill_pid = false;

                auto const err = buf[1];

                switch (SpawnContext::ExecError(buf[0])) {
                case SpawnContext::ExecError::CHDIR: {
                        auto cwd = vte::glib::take_string(context().cwd() ? g_utf8_make_valid(context().cwd(), -1) : nullptr);
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  _("Failed to change to directory “%s”: %s"),
                                  cwd.get(),
                                  g_strerror(err));
                        break;
                }

                case SpawnContext::ExecError::DUP:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to duplicate file descriptor: %s",
                                  g_strerror(err));
                        break;

                case SpawnContext::ExecError::DUP2:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to duplicate file descriptor (dup2): %s",
                                  g_strerror(err));
                        break;

                case SpawnContext::ExecError::EXEC:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to execve: %s",
                                  g_strerror(err));
                        break;

                case SpawnContext::ExecError::FDWALK:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to fdwalk: %s",
                                  g_strerror(err));
                        break;

                case SpawnContext::ExecError::GETPTPEER:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to open PTY peer: %s",
                                  g_strerror(err));
                        break;

                case SpawnContext::ExecError::SCTTY:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to set controlling TTY: %s",
                                  g_strerror(err));
                        break;

                case SpawnContext::ExecError::SETSID:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to start session: %s",
                                  g_strerror(err));
                        break;

                case SpawnContext::ExecError::SIGMASK:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to set signal mask: %s",
                                  g_strerror(err));
                        break;

                case SpawnContext::ExecError::UNSET_CLOEXEC:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Failed to make file descriptor not cloexec: %s",
                                  g_strerror(err));
                        break;

                default:
                        error.set(G_IO_ERROR, g_io_error_from_errno(err),
                                  "Unknown error: %s",
                                  g_strerror(err));
                        break;
                }

                auto arg0 = vte::glib::take_string(g_utf8_make_valid(context().argv()[0], -1));
                g_prefix_error(error,
                               _("Failed to execute child process “%s”: "),
                               arg0.get());

                return false;
        }

        /* Spawn succeeded */

#if WITH_SYSTEMD
        if (context().systemd_scope() &&
            !vte::systemd::create_scope_for_pid_sync(m_pid,
                                                     m_timeout, // FIXME: recalc timeout
                                                     m_cancellable.get(),
                                                     error)) {
                if (context().require_systemd_scope())
                        return false;

                _vte_debug_print(VTE_DEBUG_PTY,
                                 "Failed to create systemd scope: %s",
                                 error.message());
                error.reset();
        }
#endif // WITH_SYSTEMD

        return true;
}

void
SpawnOperation::run_in_thread(GTask* task) noexcept
{
        auto error = vte::glib::Error{};
        if (run(error))
                g_task_return_int(task, ssize_t{release_pid()});
        else
                g_task_return_error(task, error.release());
}

void
SpawnOperation::run_async(std::unique_ptr<SpawnOperation> op,
                          void* source_tag,
                          GAsyncReadyCallback callback,
                          void* user_data)
{
        /* Spawning is split into the fork() phase, and waiting for the child to
         * exec or report an error. This is done so that the fork is happening on
         * the main thread; see issue vte#118.
         */
        auto error = vte::glib::Error{};
        auto rv = op->prepare(error);

        /* Create a GTask to run the user-provided callback, and transfers
         * ownership of @op to the task.
         */
        auto task = vte::glib::take_ref(g_task_new(op->context().pty_wrapper(),
                                                   op->m_cancellable.get(),
                                                   callback,
                                                   user_data));
        g_task_set_source_tag(task.get(), source_tag);
        g_task_set_task_data(task.get(), op.release(), delete_cb);
        // g_task_set_name(task.get(), "vte-spawn-async");

        if (!rv)
                return g_task_return_error(task.get(), error.release());

        /* Async read from the child */
        g_task_run_in_thread(task.get(), run_in_thread_cb);
}

bool
SpawnOperation::run_sync(SpawnOperation& op,
                         GPid* pid,
                         vte::glib::Error& error)
{
        auto rv = op.prepare(error) && op.run(error);
        if (rv)
                *pid = op.release_pid();
        else
                *pid = -1;

        return rv;
}

} // namespace vte::base
