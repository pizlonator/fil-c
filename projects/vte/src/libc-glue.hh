/*
 * Copyright © 2015, 2020 Christian Persch
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

#pragma once

#include <cassert>
#include <cerrno>
#include <memory>

#include <unistd.h>
#include <fcntl.h>

namespace vte::libc {

class ErrnoSaver {
public:
        ErrnoSaver() noexcept : m_errsv{errno} { }
        ~ErrnoSaver() noexcept { errno = m_errsv; }

        ErrnoSaver(ErrnoSaver const&) = delete;
        ErrnoSaver(ErrnoSaver&&) = delete;
        ErrnoSaver& operator=(ErrnoSaver const&) = delete;
        ErrnoSaver& operator=(ErrnoSaver&&) = delete;

        inline constexpr operator int () const noexcept { return m_errsv; }

        inline void reset() noexcept { m_errsv = 0; }

private:
        int m_errsv;
}; // class ErrnoSaver

class FD {
public:
        constexpr FD() noexcept = default;
        explicit constexpr FD(int fd) noexcept : m_fd{fd} { } // adopts the FD
        constexpr FD(FD const&) = delete;
        constexpr FD(FD&& rhs) noexcept : m_fd{rhs.release()} { }

        ~FD() noexcept { reset(); }

        // adopt the file descriptor
        FD& operator=(int rhs) noexcept
        {
                reset();
                m_fd = rhs;
                return *this;
        }

        FD& operator=(FD& rhs) = delete;

        FD& operator=(FD&& rhs) noexcept
        {
                if (this != std::addressof(rhs)) {
                        reset();
                        m_fd = rhs.release();
                }
                return *this;
        }

        explicit constexpr operator bool() const noexcept { return m_fd != -1; }

        constexpr int get() const noexcept { return m_fd; }

        constexpr int release() noexcept
        {
                auto fd = m_fd;
                m_fd = -1;
                return fd;
        }

        void reset()
        {
                if (m_fd != -1) {
                        auto errsv = ErrnoSaver{};
                        close(m_fd);
                        m_fd = -1;
                }
        }

        /* C++20 constexpr */ void swap(FD& other)
        {
                using std::swap;
                swap(m_fd, other.m_fd);
        }

private:
        int m_fd{-1};

}; // class FD

constexpr bool operator==(FD const& lhs, FD const& rhs) noexcept { return lhs.get() == rhs.get(); }
constexpr bool operator==(FD const& lhs, int rhs) noexcept { return lhs.get() == rhs; }
constexpr bool operator!=(FD const& lhs, FD const& rhs) noexcept { return !(lhs == rhs); }
constexpr bool operator!=(FD const& lhs, int rhs) noexcept { return !(lhs == rhs); }
/* C++20 constexpr */ inline void swap(FD& lhs, FD& rhs) noexcept { lhs.swap(rhs); }

/* FD convenience functions */

static inline int
fd_get_descriptor_flags(int fd)
{
        auto flags = int{};
        do {
                flags = fcntl(fd, F_GETFD);
        } while (flags == -1 && errno == EINTR);

        return flags;
}

static inline int
fd_set_descriptor_flags(int fd,
                        int flags)
{
        auto r = int{};
        do {
                r = fcntl(fd, F_SETFD, flags);
        } while (r == -1 && errno == EINTR);

        return r;
}

static inline int
fd_change_descriptor_flags(int fd,
                           int set_flags,
                           int unset_flags)
{
        auto const flags = fd_get_descriptor_flags(fd);
        if (flags == -1)
                return -1;

        auto const new_flags = (flags | set_flags) & ~unset_flags;
        if (new_flags == flags)
                return 0;

        return fd_set_descriptor_flags(fd, new_flags);
}

static inline int
fd_get_status_flags(int fd)
{
        auto flags = int{};
        do {
                flags = fcntl(fd, F_GETFL, 0);
        } while (flags == -1 && errno == EINTR);

        return flags;
}

static inline int
fd_set_status_flags(int fd,
                    int flags)
{
        auto r = int{};
        do {
                r = fcntl(fd, F_SETFL, flags);
        } while (r == -1 && errno == EINTR);

        return r;
}

static inline int
fd_change_status_flags(int fd,
                       int set_flags,
                       int unset_flags)
{
        auto const flags = fd_get_status_flags(fd);
        if (flags == -1)
                return -1;

        auto const new_flags = (flags | set_flags) & ~unset_flags;
        if (new_flags == flags)
                return 0;

        return fd_set_status_flags(fd, new_flags);
}

static inline bool
fd_get_cloexec(int fd)
{
        auto const r = fd_get_descriptor_flags(fd);
        return r != -1 && (r & FD_CLOEXEC) != 0;
}

static inline int
fd_set_cloexec(int fd)
{
        return fd_change_descriptor_flags(fd, FD_CLOEXEC, 0);
}

static inline int
fd_unset_cloexec(int fd)
{
        return fd_change_descriptor_flags(fd, 0, FD_CLOEXEC);
}

static inline int
fd_set_nonblock(int fd)
{
        return fd_change_status_flags(fd, O_NONBLOCK, 0);
}

static inline int
fd_dup_cloexec(int oldfd,
               int newfd)
{
        auto r = int{};
        do {
                r = fcntl(oldfd, F_DUPFD_CLOEXEC, newfd);
        } while (r == -1 && errno == EINTR);
        return r;
}

static inline int
fd_dup2(int oldfd,
        int newfd)
{
        auto r = int{};
        do {
                r = dup2(oldfd, newfd);
        } while (r == -1 && errno == EINTR);
        return r;
}

} // namespace vte::libc
