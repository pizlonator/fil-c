/*
 * Copyright © 2019 Christian Persch
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

#include <functional>
#include <memory>

#include <glib.h>

#include "cxx-utils.hh"
#include "std-glue.hh"

namespace vte::glib {

template<typename T>
using FreePtr = vte::FreeablePtr<T, decltype(&g_free), &g_free>;

template<typename T>
FreePtr<T>
take_free_ptr(T* ptr)
{
        return FreePtr<T>{ptr};
}

using StringPtr = FreePtr<char>;
using StringGetter = vte::ValueGetter<StringPtr, char*, nullptr>;

inline StringPtr
take_string(char* str)
{
        return take_free_ptr(str);
}

inline StringPtr
dup_string(char const* str)
{
        return take_string(g_strdup(str));
}

using StrvPtr = vte::FreeablePtr<char*, decltype(&g_strfreev), &g_strfreev>;
using StrvGetter = vte::ValueGetter<StrvPtr, char**, nullptr>;

inline StrvPtr
take_strv(char** strv)
{
        return StrvPtr{strv};
}

inline StrvPtr
dup_strv(char const* const* strv)
{
        return take_strv(g_strdupv(const_cast<char**>(strv)));
}

class Error {
public:
        Error() noexcept = default;
        ~Error() noexcept { reset(); }

        Error(Error const&) = delete;
        Error(Error&&) = delete;
        Error& operator=(Error const&) = delete;
        Error& operator=(Error&&) = delete;

        operator GError* ()  noexcept { return m_error; }
        operator GError** () noexcept { return &m_error; }

        auto error()   const noexcept { return m_error != nullptr; }
        auto domain()  const noexcept { return error() ? m_error->domain : 0; }
        auto code()    const noexcept { return error() ? m_error->code : -1; }
        auto message() const noexcept { return error() ? m_error->message : nullptr; }

        void assert_no_error() const noexcept { g_assert_no_error(m_error); }

        G_GNUC_PRINTF(4, 5)
        void set(GQuark domain,
                 int code,
                 char const* format,
                 ...)
        {
                va_list args;
                va_start(args, format);
                g_propagate_error(&m_error, g_error_new_valist(domain, code, format, args));
                va_end(args);
        }

        void set_literal(GQuark domain,
                         int code,
                         char const* msg)
        {
                g_propagate_error(&m_error, g_error_new_literal(domain, code, msg));
        }

        bool matches(GQuark domain, int code) const noexcept
        {
                return error() && g_error_matches(m_error, domain, code);
        }

        void reset() noexcept { g_clear_error(&m_error); }

        GError* release() noexcept { auto err = m_error; m_error = nullptr; return err; }

        bool propagate(GError** error) noexcept { g_propagate_error(error, release()); return false; }

private:
        GError* m_error{nullptr};
};

class Timer {
public:
        /* If the callback returns true, the timer is repeated; if the
         * callback returns false, the timer is removed.
         */
        using callback_type = std::function<bool()>;

        Timer(callback_type callback,
              char const* name)
                : m_callback(callback)
#if VTE_DEBUG
                , m_name(name)
#endif
        {
        }

        ~Timer() noexcept
        {
                abort();
        }

        Timer(Timer const&) = delete;
        Timer(Timer&&) = delete;
        Timer& operator=(Timer const&) = delete;
        Timer& operator=(Timer&&) = delete;

        constexpr operator bool() const noexcept { return m_source_id != 0; }

        class Priority {
        public:
                enum {
                      eHIGH = G_PRIORITY_HIGH,
                      eDEFAULT = G_PRIORITY_DEFAULT,
                      eHIGH_IDLE = G_PRIORITY_HIGH_IDLE,
                      eDEFAULT_IDLE = G_PRIORITY_DEFAULT_IDLE,
                      eLOW = G_PRIORITY_LOW,
                };
        };

        void schedule(unsigned int timeout,
                      int priority = Priority::eDEFAULT) noexcept
        {
                abort();
                m_source_id = g_timeout_add_full(priority,
                                                 timeout,
                                                 s_dispatch_timer_cb,
                                                 this,
                                                 s_destroy_timer_cb);
                set_source_name();
        }

        void schedule_seconds(unsigned int timeout,
                              int priority = Priority::eDEFAULT) noexcept
        {
                abort();
                m_source_id = g_timeout_add_seconds_full(priority,
                                                         timeout,
                                                         s_dispatch_timer_cb,
                                                         this,
                                                         s_destroy_timer_cb);
                set_source_name();
        }

        void schedule_idle(int priority = Priority::eDEFAULT) noexcept
        {
                abort();
                m_source_id = g_idle_add_full(priority,
                                              s_dispatch_timer_cb,
                                              this,
                                              s_destroy_timer_cb);
                set_source_name();
        }

        void abort() noexcept
        {
                if (m_source_id != 0) {
                        g_source_remove(m_source_id);
                        m_source_id = 0;
                }

                m_rescheduled = false;
        }

private:
        callback_type m_callback{};
#if VTE_DEBUG
        char const* m_name{nullptr};
#endif
        guint m_source_id{0};
        bool m_rescheduled{false};

        bool dispatch() noexcept
        {
                auto const id = m_source_id;

                auto rv = false;
                try {
                        rv = m_callback();
                } catch (...) {
                        vte::log_exception();
                }

                /* The Timer may have been re-scheduled or removed from within
                 * the callback. In this case, the callback must return false!
                 * m_source_id is now different (since the old source
                 * ID is still associated with the main context until we return from
                 * this function), after which invalidate_source() will be called,
                 * but must not overwrite m_source_id.
                 * In the non-rescheduled case, invalidate_source() must set
                 * m_source_id to 0.
                 */
                m_rescheduled = id != m_source_id;
                assert(!m_rescheduled || rv == false);
                return rv;
        }

        inline void set_source_name() const noexcept
        {
                #if VTE_DEBUG
                g_source_set_name_by_id(m_source_id, m_name);
                #endif
        }

        static gboolean s_dispatch_timer_cb(void* data) noexcept
        {
                auto timer = reinterpret_cast<Timer*>(data);
                return timer->dispatch();
        }

        void invalidate_source() noexcept
        {
                if (!m_rescheduled)
                        m_source_id = 0;
                m_rescheduled = false;
        }

        static void s_destroy_timer_cb(void* data) noexcept
        {
                auto timer = reinterpret_cast<Timer*>(data);
                timer->invalidate_source();
        }
};

bool set_error_from_exception(GError** error
#if VTE_DEBUG
                              , char const* func = __builtin_FUNCTION()
                              , char const* filename = __builtin_FILE()
                              , int const line = __builtin_LINE()
#endif
                              ) noexcept;

} // namespace vte::glib

namespace vte {

VTE_DECLARE_FREEABLE(GArray, g_array_unref);
VTE_DECLARE_FREEABLE(GBytes, g_bytes_unref);
VTE_DECLARE_FREEABLE(GChecksum, g_checksum_free);
VTE_DECLARE_FREEABLE(GKeyFile, g_key_file_unref);
VTE_DECLARE_FREEABLE(GOptionContext, g_option_context_free);
VTE_DECLARE_FREEABLE(GString, g_autoptr_cleanup_gstring_free);
VTE_DECLARE_FREEABLE(GUri, g_uri_unref);
VTE_DECLARE_FREEABLE(GVariant, g_variant_unref);

} // namespace vte

namespace vte::glib {

inline char*
release_to_string(vte::Freeable<GString> str,
                  gsize* length = nullptr) noexcept
{
        if (length)
                *length = str.get()->len;

        return g_string_free(str.release(), false);
}

} // namespace vte::glib
