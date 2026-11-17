/*
 * Copyright © 2020, 2021 Christian Persch
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

#include "glib-glue.hh"

#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>

#include "debug.h"

#define VTE_EXCEPTION_ERROR g_quark_from_static_string("std::exception")

typedef enum {
        VTE_EXCEPTION_GENERIC,
} VteException;

namespace vte {

using namespace std::literals;

static void
exception_append_to_string(std::exception const& e,
                           std::string& what,
                           int level = 0)
{
        if (level > 0)
                what += ": "sv;
        what += e.what();

        try {
                std::rethrow_if_nested(e);
        } catch (std::bad_alloc const& en) {
                g_error("Allocation failure: %s\n", what.c_str());
        } catch (std::exception const& en) {
                exception_append_to_string(en, what, level + 1);
        } catch (...) {
                what += ": Unknown nested exception"sv;
        }
}

#if VTE_DEBUG

void log_exception(char const* func,
                   char const* filename,
                   int const line) noexcept
try
{
        auto what = std::string{};

        try {
                throw; // rethrow current exception
        } catch (std::bad_alloc const& e) {
                g_error("Allocation failure: %s\n", e.what());
        } catch (std::exception const& e) {
                exception_append_to_string(e, what);
        } catch (...) {
                what = "Unknown exception"sv;
        }

        _vte_debug_print(VTE_DEBUG_EXCEPTIONS,
                         "Caught exception in %s [%s:%d]: %s\n",
                         func, filename, line, what.c_str());
}
catch (...)
{
        _vte_debug_print(VTE_DEBUG_EXCEPTIONS,
                         "Caught exception while logging an exception in %s [%s:%d]\n",
                         func, filename, line);
}

#else

static void
log_exception(std::exception const& e)
{
        try {
                std::rethrow_if_nested(e);
        } catch (std::bad_alloc const& en) {
                g_error("Allocation failure: %s\n", e.what());
        } catch (std::exception const& en) {
                log_exception(en);
        } catch (...) {
        }
}

void
log_exception() noexcept
try
{
        try {
                throw; // rethrow current exception
        } catch (std::bad_alloc const& e) {
                g_error("Allocation failure: %s\n", e.what());
        } catch (std::exception const& e) {
                log_exception(e);
        } catch (...) {
        }
}
catch (...)
{
}

#endif /* VTE_DEBUG */

namespace glib {

bool set_error_from_exception(GError** error
#if VTE_DEBUG
                              , char const* func
                              , char const* filename
                              , int const line
#endif
                              ) noexcept
try
{
        auto what = std::string{};

        try {
                throw; // rethrow current exception
        } catch (std::bad_alloc const& e) {
                g_error("Allocation failure: %s\n", e.what());
        } catch (std::exception const& e) {
                exception_append_to_string(e, what);
        } catch (...) {
                what = "Unknown exception"sv;
        }

#if VTE_DEBUG
        auto msg = vte::glib::take_string(g_strdup_printf("Caught exception in %s [%s:%d]: %s",
                                                          func, filename, line,
                                                          what.c_str()));
#else
        auto msg = vte::glib::take_string(g_strdup_printf("Caught exception: %s",
                                                          what.c_str()));
#endif
        auto msg_str = vte::glib::take_string(g_utf8_make_valid(msg.get(), -1));
        g_set_error_literal(error,
                            VTE_EXCEPTION_ERROR,
                            VTE_EXCEPTION_GENERIC,
                            msg_str.get());
        _vte_debug_print(VTE_DEBUG_EXCEPTIONS, "%s", msg_str.get());

        return false;
}
catch (...)
{
        vte::log_exception();
#if VTE_DEBUG
        g_set_error(error,
                    VTE_EXCEPTION_ERROR,
                    VTE_EXCEPTION_GENERIC,
                    "Caught exception while logging an exception in %s [%s:%d]\n",
                    func, filename, line);
#else
        g_set_error_literal(error,
                            VTE_EXCEPTION_ERROR,
                            VTE_EXCEPTION_GENERIC,
                            "Caught exception while logging an exception");
#endif
        return false;
}

} // namespace glib
} // namespace vte
