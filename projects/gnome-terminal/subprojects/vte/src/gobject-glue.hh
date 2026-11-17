/*
 * Copyright © 2020 Christian Persch
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

#include <glib-object.h>

namespace vte::glib {

class FreezeObjectNotify {
public:
        explicit FreezeObjectNotify(void* object) noexcept
                : m_object{G_OBJECT(object)}
        {
                g_object_freeze_notify(m_object);
        }

        ~FreezeObjectNotify() noexcept
        {
                g_object_thaw_notify(m_object);
        }

        FreezeObjectNotify() = delete;
        FreezeObjectNotify(FreezeObjectNotify const&) = delete;
        FreezeObjectNotify(FreezeObjectNotify&&) = delete;
        FreezeObjectNotify& operator=(FreezeObjectNotify const&) = delete;
        FreezeObjectNotify& operator=(FreezeObjectNotify&&) = delete;

        auto get() const noexcept { return m_object; }

private:
        GObject* m_object;
}; // class FreezeObjectNotify

} // namespace vte::glib
