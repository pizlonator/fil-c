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

#include <memory>
#include <optional>
#include <string>

#include <gtk/gtk.h>

#include "glib-glue.hh"
#include "refptr.hh"
#include "fwd.hh"

namespace vte::platform {

enum class ClipboardFormat {
        TEXT,
        HTML,
#if VTE_GTK == 4
        INVALID = -1
#endif
};

enum class ClipboardType {
        CLIPBOARD = 0,
        PRIMARY   = 1
};

#if VTE_GTK == 4
class ContentProvider;
#endif

class Clipboard : public std::enable_shared_from_this<Clipboard> {
#if VTE_GTK == 4
        friend class ContentProvider;
#endif
public:
        Clipboard(Widget& delegate,
                  ClipboardType type) /* throws */;
        ~Clipboard() = default;

        Clipboard(Clipboard const&) = delete;
        Clipboard(Clipboard&&) = delete;

        Clipboard& operator=(Clipboard const&) = delete;
        Clipboard& operator=(Clipboard&&) = delete;

        constexpr auto type() const noexcept { return m_type; }

        void disown() noexcept
        {
                m_delegate.reset();
        }

        using OfferGetCallback = std::optional<std::string_view>(Widget::*)(Clipboard const&,
                                                                            ClipboardFormat format);
        using OfferClearCallback = void (Widget::*)(Clipboard const&);
        using RequestDoneCallback = void (Widget::*)(Clipboard const&,
                                                     std::string_view const&);
        using RequestFailedCallback = void (Widget::*)(Clipboard const&);

        void offer_data(ClipboardFormat format,
                        OfferGetCallback get_callback,
                        OfferClearCallback clear_callback) /* throws */;

        void set_text(char const* text,
                      size_t size) noexcept;

        void request_text(RequestDoneCallback done_callback,
                          RequestFailedCallback failed_callback) /* throws */;

private:
#if VTE_GTK == 3
        vte::glib::RefPtr<GtkClipboard> m_clipboard;
#elif VTE_GTK == 4
        vte::glib::RefPtr<GdkClipboard> m_clipboard;
#endif
        std::weak_ptr<Widget> m_delegate;
        ClipboardType m_type;

        auto platform() const noexcept { return m_clipboard.get(); }

        class Offer;
        class Request;

}; // class Clipboard

} // namespace vte::platform
