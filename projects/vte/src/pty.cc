/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 * Copyright © 2009, 2010, 2019 Christian Persch
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

/**
 * SECTION: vte-pty
 * @short_description: Functions for starting a new process on a new pseudo-terminal and for
 * manipulating pseudo-terminals
 *
 * The terminal widget uses these functions to start commands with new controlling
 * pseudo-terminals and to resize pseudo-terminals.
 */

#include "config.h"

#include "pty.hh"

#include "libc-glue.hh"

#include <vte/vte.h>
#include "vteptyinternal.hh"

#include <assert.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#if __has_include(<sys/syslimits.h>)
#include <sys/syslimits.h>
#endif
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#if __has_include(<util.h>)
#include <util.h>
#endif
#if __has_include(<pty.h>)
#include <pty.h>
#endif
#if defined(__sun) && __has_include(<stropts.h>)
#include <stropts.h>
#define HAVE_STROPTS_H
#endif
#ifdef __NetBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#endif
#include <glib.h>
#include <gio/gio.h>
#include "debug.h"

#include <glib/gi18n-lib.h>

#include "glib-glue.hh"

#include "vtedefines.hh"

#include "missing.hh"

namespace vte::base {

Pty*
Pty::ref() noexcept
{
        g_atomic_int_inc(&m_refcount);
        return this;
}

void
Pty::unref() noexcept
{
        if (g_atomic_int_dec_and_test(&m_refcount))
                delete this;
}

int
Pty::get_peer(bool cloexec) const noexcept
{
        if (!m_pty_fd)
                return -1;

        /* FIXME? else if (m_flags & VTE_PTY_NO_CTTTY)
         * No session and no controlling TTY wanted, do we need to lose our controlling TTY,
         * perhaps by open("/dev/tty") + ioctl(TIOCNOTTY) ?
         */

        /* Now open the PTY peer. Note that this also makes the PTY our controlling TTY. */
        auto const fd_flags = int{O_RDWR |
                                  ((m_flags & VTE_PTY_NO_CTTY) ? O_NOCTTY : 0) |
                                  (cloexec ? O_CLOEXEC : 0)};

        auto peer_fd = vte::libc::FD{};

#ifdef __linux__
        peer_fd = ioctl(m_pty_fd.get(), TIOCGPTPEER, fd_flags);
        /* Note: According to the kernel's own tests (tools/testing/selftests/filesystems/devpts_pts.c),
         * the error returned when the running kernel does not support this ioctl should be EINVAL.
         * However it appears that the actual error returned is ENOTTY. So we check for both of them.
         * See issue#182.
         */
        if (!peer_fd &&
            errno != EINVAL &&
            errno != ENOTTY) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                 "ioctl(TIOCGPTPEER)", g_strerror(errsv));
                return -1;
        }

        /* Fall back to ptsname + open */
#endif

        if (!peer_fd) {
                auto const name = ptsname(m_pty_fd.get());
                if (name == nullptr) {
                        auto errsv = vte::libc::ErrnoSaver{};
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                         "ptsname", g_strerror(errsv));
                        return -1;
                }

                _vte_debug_print (VTE_DEBUG_PTY,
                                  "Setting up child pty: master FD = %d name = %s\n",
                                  m_pty_fd.get(), name);

                peer_fd = ::open(name, fd_flags);
                if (!peer_fd) {
                        auto errsv = vte::libc::ErrnoSaver{};
                        _vte_debug_print (VTE_DEBUG_PTY, "Failed to open PTY: %s\n",
                                          g_strerror(errsv));
                        return -1;
                }
        }

        assert(bool(peer_fd));

#if defined(__sun) && defined(HAVE_STROPTS_H)
        /* See https://illumos.org/man/7i/streamio */
        if (isastream (peer_fd.get()) == 1) {
                /* https://illumos.org/man/7m/ptem */
                if ((ioctl(peer_fd.get(), I_FIND, "ptem") == 0) &&
                    (ioctl(peer_fd.get(), I_PUSH, "ptem") == -1)) {
                        return -1;
                }
                /* https://illumos.org/man/7m/ldterm */
                if ((ioctl(peer_fd.get(), I_FIND, "ldterm") == 0) &&
                    (ioctl(peer_fd.get(), I_PUSH, "ldterm") == -1)) {
                        return -1;
                }
                /* https://illumos.org/man/7m/ttcompat */
                if ((ioctl(peer_fd.get(), I_FIND, "ttcompat") == 0) &&
                    (ioctl(peer_fd.get(), I_PUSH, "ttcompat") == -1)) {
                        return -1;
                }
        }
#endif

        return peer_fd.release();
}

void
Pty::child_setup() const noexcept
{
        /* Unblock all signals */
        sigset_t set;
        sigemptyset(&set);
        if (pthread_sigmask(SIG_SETMASK, &set, nullptr) == -1) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                 "pthread_sigmask", g_strerror(errsv));
                _exit(127);
        }

        /* Reset the handlers for all signals to their defaults.  The parent
         * (or one of the libraries it links to) may have changed one to be ignored.
         */
        for (int n = 1; n < NSIG; n++) {
                if (n == SIGSTOP || n == SIGKILL)
                        continue;

                signal(n, SIG_DFL);
        }

        if (!(m_flags & VTE_PTY_NO_SESSION)) {
                /* This starts a new session; we become its process-group leader,
                 * and lose our controlling TTY.
                 */
                _vte_debug_print (VTE_DEBUG_PTY, "Starting new session\n");
                if (setsid() == -1) {
                        auto errsv = vte::libc::ErrnoSaver{};
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                         "setsid", g_strerror(errsv));
                        _exit(127);
                }
        }

        auto peer_fd = get_peer();
        if (peer_fd == -1)
                _exit(127);

#ifdef TIOCSCTTY
        /* On linux, opening the PTY peer above already made it our controlling TTY (since
         * previously there was none, after the setsid() call). However, it appears that e.g.
         * on *BSD, that doesn't happen, so we need this explicit ioctl here.
         */
        if (!(m_flags & VTE_PTY_NO_CTTY)) {
                if (ioctl(peer_fd, TIOCSCTTY, peer_fd) != 0) {
                        auto errsv = vte::libc::ErrnoSaver{};
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                         "ioctl(TIOCSCTTY)", g_strerror(errsv));
                        _exit(127);
                }
        }
#endif

	/* now setup child I/O through the tty */
	if (peer_fd != STDIN_FILENO) {
		if (dup2(peer_fd, STDIN_FILENO) != STDIN_FILENO){
			_exit (127);
		}
	}
	if (peer_fd != STDOUT_FILENO) {
		if (dup2(peer_fd, STDOUT_FILENO) != STDOUT_FILENO){
			_exit (127);
		}
	}
	if (peer_fd != STDERR_FILENO) {
		if (dup2(peer_fd, STDERR_FILENO) != STDERR_FILENO){
			_exit (127);
		}
	}

	/* If the peer FD has not been consumed above as one of the stdio descriptors,
         * need to close it now so that it doesn't leak to the child.
         */
	if (peer_fd != STDIN_FILENO  &&
            peer_fd != STDOUT_FILENO &&
            peer_fd != STDERR_FILENO) {
                close(peer_fd);
	}
}

/*
 * Pty::set_size:
 * @rows: the desired number of rows
 * @columns: the desired number of columns
 * @cell_height_px: the height of a cell in px, or 0 for undetermined
 * @cell_width_px: the width of a cell in px, or 0 for undetermined
 *
 * Attempts to resize the pseudo terminal's window size.  If successful, the
 * OS kernel will send #SIGWINCH to the child process group.
 *
 * Returns: %true on success, or %false on error with errno set
 */
bool
Pty::set_size(int rows,
              int columns,
              int cell_height_px,
              int cell_width_px) const noexcept
{
        auto master = fd();

	struct winsize size;
	memset(&size, 0, sizeof(size));
	size.ws_row = rows > 0 ? rows : 24;
	size.ws_col = columns > 0 ? columns : 80;
	_vte_debug_print(VTE_DEBUG_PTY,
			"Setting size on fd %d to (%d,%d).\n",
			master, columns, rows);
        auto ret = ioctl(master, TIOCSWINSZ, &size);

        if (ret != 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "Failed to set size on %d: %s\n",
                                 master, g_strerror(errsv));
        }

        return ret == 0;
}

/*
 * Pty::get_size:
 * @rows: (out) (allow-none): a location to store the number of rows, or %NULL
 * @columns: (out) (allow-none): a location to store the number of columns, or %NULL
 *
 * Reads the pseudo terminal's window size.
 *
 * If getting the window size failed, @error will be set to a #GIOError.
 *
 * Returns: %true on success, or %false on error with errno set
 */
bool
Pty::get_size(int* rows,
              int* columns) const noexcept
{
        auto master = fd();

	struct winsize size;
	memset(&size, 0, sizeof(size));
        auto ret = ioctl(master, TIOCGWINSZ, &size);
        if (ret == 0) {
		if (columns != nullptr) {
			*columns = size.ws_col;
		}
		if (rows != nullptr) {
			*rows = size.ws_row;
		}
		_vte_debug_print(VTE_DEBUG_PTY,
				"Size on fd %d is (%d,%d).\n",
				master, size.ws_col, size.ws_row);
                return true;
	}

        auto errsv = vte::libc::ErrnoSaver{};
        _vte_debug_print(VTE_DEBUG_PTY,
                         "Failed to read size from fd %d: %s\n",
                         master, g_strerror(errsv));

        return false;
}

static int
fd_set_cpkt(vte::libc::FD& fd)
{
        auto ret = 0;
#if defined(TIOCPKT)
        /* tty_ioctl(4) -> every read() gives an extra byte at the beginning
         * notifying us of stop/start (^S/^Q) events. */
        int one = 1;
        ret = ioctl(fd.get(), TIOCPKT, &one);
#elif defined(__sun) && defined(HAVE_STROPTS_H)
        if (isastream(fd.get()) == 1 &&
            ioctl(fd.get(), I_FIND, "pckt") == 0)
                ret = ioctl(fd.get(), I_PUSH, "pckt");
#endif
        return ret;
}

static int
fd_setup(vte::libc::FD& fd)
{
        if (grantpt(fd.get()) != 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                 "grantpt", g_strerror(errsv));
                return -1;
        }

        if (unlockpt(fd.get()) != 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                 "unlockpt", g_strerror(errsv));
                return -1;
        }

        if (vte::libc::fd_set_cloexec(fd.get()) < 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s",
                                 "Setting CLOEXEC flag", g_strerror(errsv));
                return -1;
        }

        if (vte::libc::fd_set_nonblock(fd.get()) < 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s",
                                 "Setting O_NONBLOCK flag", g_strerror(errsv));
                return -1;
        }

        if (fd_set_cpkt(fd) < 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s",
                                 "Setting packet mode", g_strerror(errsv));
                return -1;
        }

        return 0;
}

/*
 * _vte_pty_open_posix:
 * @pty: a #VtePty
 * @error: a location to store a #GError, or %NULL
 *
 * Opens a new file descriptor to a new PTY master.
 *
 * Returns: the new PTY's master FD, or -1
 */
static vte::libc::FD
_vte_pty_open_posix(void)
{
	/* Attempt to open the master. */
        auto fd = vte::libc::FD{posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC)};
#ifndef __linux__
        /* Other kernels may not support CLOEXEC or NONBLOCK above, so try to fall back */
        bool need_cloexec = false, need_nonblocking = false;

#ifdef __NetBSD__
        // NetBSD is a special case: prior to 9.99.101, posix_openpt() will not return
        // EINVAL for unknown/unsupported flags but instead silently ignore these flags
        // and just return a valid PTY but without the NONBLOCK | CLOEXEC flags set.
        // So we need to manually apply these flags there. See issue #2575.
        int mib[2], osrev;
        size_t len;

        mib[0] = CTL_KERN;
        mib[1] = KERN_OSREV;
        len = sizeof(osrev);
        sysctl(mib, 2, &osrev, &len, NULL, 0);
        if (osrev < 999010100) {
                need_cloexec = need_nonblocking = true;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "NetBSD < 9.99.101, forcing fallback "
                                 "for NONBLOCK and CLOEXEC.\n");
        }
#else

        if (!fd && errno == EINVAL) {
                /* Try without NONBLOCK and apply the flag afterward */
                need_nonblocking = true;
                fd = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
                if (!fd && errno == EINVAL) {
                        /* Try without CLOEXEC and apply the flag afterwards */
                        need_cloexec = true;
                        fd = posix_openpt(O_RDWR | O_NOCTTY);
                }
        }
#endif /* __NetBSD__ */
#endif /* !linux */

        if (!fd) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s",
                                 "posix_openpt", g_strerror(errsv));
                return {};
        }

#ifndef __linux__
        if (need_cloexec && vte::libc::fd_set_cloexec(fd.get()) < 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s",
                                 "Setting CLOEXEC flag", g_strerror(errsv));
                return {};
        }

        if (need_nonblocking && vte::libc::fd_set_nonblock(fd.get()) < 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s",
                                 "Setting NONBLOCK flag", g_strerror(errsv));
                return {};
        }
#endif /* !linux */

        if (fd_set_cpkt(fd) < 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s",
                                 "Setting packet mode", g_strerror(errsv));
                return {};
        }

        if (grantpt(fd.get()) != 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                 "grantpt", g_strerror(errsv));
                return {};
        }

        if (unlockpt(fd.get()) != 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s\n",
                                 "unlockpt", g_strerror(errsv));
                return {};
        }

	_vte_debug_print(VTE_DEBUG_PTY, "Allocated pty on fd %d.\n", fd.get());

        return fd;
}

static vte::libc::FD
_vte_pty_open_foreign(int masterfd /* consumed */)
{
        auto fd = vte::libc::FD{masterfd};
        if (!fd) {
                errno = EBADF;
                return {};
        }

        if (fd_setup(fd) < 0)
                return {};

        return fd;
}

/*
 * Pty::set_utf8:
 * @utf8: whether or not the pty is in UTF-8 mode
 *
 * Tells the kernel whether the terminal is UTF-8 or not, in case it can make
 * use of the info.  Linux 2.6.5 or so defines IUTF8 to make the line
 * discipline do multibyte backspace correctly.
 *
 * Returns: %true on success, or %false on error with errno set
 */
bool
Pty::set_utf8(bool utf8) const noexcept
{
#ifdef IUTF8
	struct termios tio;
        if (tcgetattr(fd(), &tio) == -1) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s",
                                 "tcgetattr", g_strerror(errsv));
                return false;
        }

        auto saved_cflag = tio.c_iflag;
        if (utf8) {
                tio.c_iflag |= IUTF8;
        } else {
                tio.c_iflag &= ~IUTF8;
        }

        /* Only set the flag if it changes */
        if (saved_cflag != tio.c_iflag &&
            tcsetattr(fd(), TCSANOW, &tio) == -1) {
                auto errsv = vte::libc::ErrnoSaver{};
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %s",
                                 "tcsetattr", g_strerror(errsv));
                return false;
	}
#endif

        return true;
}

Pty*
Pty::create(VtePtyFlags flags)
{
        auto fd = _vte_pty_open_posix();
        if (!fd)
                return nullptr;

        return new Pty{std::move(fd), flags};
}

Pty*
Pty::create_foreign(int foreign_fd,
                    VtePtyFlags flags)
{
        auto fd = _vte_pty_open_foreign(foreign_fd);
        if (!fd)
                return nullptr;

        return new Pty{std::move(fd), flags};
}

} // namespace vte::base
