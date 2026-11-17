/*
 * Copyright © 2018, 2019 Christian Persch
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

#include <memory>

#include <gio/gio.h>

#include "vte/vteenums.h"

#include "libc-glue.hh"

namespace vte::base {

class Pty {
private:
        mutable volatile int m_refcount{1};
        vte::libc::FD m_pty_fd{};
        VtePtyFlags m_flags{VTE_PTY_DEFAULT};

public:
        constexpr Pty(vte::libc::FD&& fd,
                      VtePtyFlags flags = VTE_PTY_DEFAULT) noexcept
                : m_pty_fd{std::move(fd)},
                  m_flags{flags}
        {
        }

        Pty(Pty const&) = delete;
        Pty(Pty&&) = delete;
        Pty operator=(Pty const&) = delete;
        Pty operator=(Pty&&) = delete;

        Pty* ref() noexcept;
        void unref() noexcept;

        inline constexpr int fd() const noexcept { return m_pty_fd.get(); }
        inline constexpr auto flags() const noexcept { return m_flags; }

        int get_peer(bool cloexec = false) const noexcept;

        void child_setup() const noexcept;

        bool set_size(int rows,
                      int columns,
                      int cell_height_px,
                      int cell_width_px) const noexcept;
        bool get_size(int* rows,
                      int* columns) const noexcept;
        bool set_utf8(bool utf8) const noexcept;

        static Pty* create(VtePtyFlags flags);
        static Pty* create_foreign(int fd,
                                   VtePtyFlags flags);
};

} // namespace vte::base
