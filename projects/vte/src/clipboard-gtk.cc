/*
 * Copyright © 2020, 2022 Christian Persch
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

#include "clipboard-gtk.hh"
#include "glib-glue.hh"
#include "gtk-glue.hh"
#include "widget.hh"
#include "vteinternal.hh"

#include <new>
#include <stdexcept>
#include <utility>

#define MIME_TYPE_TEXT_PLAIN_UTF8 "text/plain;charset=utf-8"
#define MIME_TYPE_TEXT_HTML_UTF8  "text/html;charset=utf-8"
#define MIME_TYPE_TEXT_HTML_UTF16 "text/html"

namespace vte::platform {

// Note:
// Each Clipboard is owned via std::shared_ptr by Widget, which drops that ref on unrealize.
// The Clipboard keeps a std::weak_ref back on Widget, and converts that to a std::shared_ptr
// via .lock() only when it wants to dispatch a callback.
// Clipboard::Offer and Clipboard::Request own their Clipboard as a std::shared_ptr.

Clipboard::Clipboard(Widget& delegate,
                     ClipboardType type) /* throws */
        : m_delegate{delegate.weak_from_this()},
          m_type{type}
{
        auto display = gtk_widget_get_display(delegate.gtk());

        switch (type) {
        case ClipboardType::PRIMARY:
                m_clipboard = vte::glib::make_ref
#if VTE_GTK == 3
                        (gtk_clipboard_get_for_display(display, GDK_SELECTION_PRIMARY));
#elif VTE_GTK == 4
                        (gdk_display_get_primary_clipboard(display));
#endif
                break;
        case ClipboardType::CLIPBOARD:
                m_clipboard = vte::glib::make_ref
#if VTE_GTK == 3
                        (gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD));
#elif VTE_GTK == 4
                        (gdk_display_get_clipboard(display));
#endif
                break;
        }

        if (!m_clipboard)
                throw std::runtime_error{"Failed to create clipboard"};
}

class Clipboard::Offer {
#if VTE_GTK == 4
        friend class ContentProvider;
#endif
public:
        Offer(Clipboard& clipboard,
              OfferGetCallback get_callback,
              OfferClearCallback clear_callback)
                : m_clipboard{clipboard.shared_from_this()},
                  m_get_callback{get_callback},
                  m_clear_callback{clear_callback}
        {
        }

        ~Offer() = default;

        auto& clipboard() const noexcept { return *m_clipboard; }

        static void run(std::unique_ptr<Offer> offer,
                        ClipboardFormat format) noexcept;

private:
        std::shared_ptr<Clipboard> m_clipboard;
        OfferGetCallback m_get_callback;
        OfferClearCallback m_clear_callback;

#if VTE_GTK == 3

        void dispatch_get(ClipboardFormat format,
                          GtkSelectionData* data) noexcept
        try
        {
                if (auto delegate = clipboard().m_delegate.lock()) {
                        auto str = (*delegate.*m_get_callback)(clipboard(), format);
                        if (!str)
                                return;

                        switch (format) {
                        case ClipboardFormat::TEXT:
                                // This makes yet another copy of the data... :(
                                gtk_selection_data_set_text(data, str->data(), str->size());
                                break;

                        case ClipboardFormat::HTML: {
                                auto const target = gtk_selection_data_get_target(data);

                                if (target == gdk_atom_intern_static_string(MIME_TYPE_TEXT_HTML_UTF8)) {
                                        // This makes yet another copy of the data... :(
                                        gtk_selection_data_set(data,
                                                               target,
                                                               8,
                                                               reinterpret_cast<guchar const*>(str->data()),
                                                               str->size());
                                } else if (target == gdk_atom_intern_static_string(MIME_TYPE_TEXT_HTML_UTF16)) {
                                        auto [html, len] = text_to_utf16_mozilla(*str);

                                        // This makes yet another copy of the data... :(
                                        if (html) {
                                                gtk_selection_data_set(data,
                                                                       target,
                                                                       16,
                                                                       reinterpret_cast<guchar const*>(html.get()),
                                                                       len);
                                        }
                                }
                                break;
                        }
                        }
                }
        }
        catch (...)
        {
                vte::log_exception();
        }

        void dispatch_clear() noexcept
        try
        {
                if (auto delegate = clipboard().m_delegate.lock()) {
                        (*delegate.*m_clear_callback)(clipboard());
                }
        }
        catch (...)
        {
                vte::log_exception();
        }

        static void
        clipboard_get_cb(GtkClipboard* clipboard,
                         GtkSelectionData* data,
                         guint info,
                         void* user_data) noexcept
        {
                if (int(info) != vte::to_integral(ClipboardFormat::TEXT) &&
                    int(info) != vte::to_integral(ClipboardFormat::HTML))
                        return;

                reinterpret_cast<Offer*>(user_data)->dispatch_get(ClipboardFormat(info), data);
        }

        static void
        clipboard_clear_cb(GtkClipboard* clipboard,
                           void* user_data) noexcept
        {
                // Assume ownership of the Offer, and delete it after dispatching the callback
                auto offer = std::unique_ptr<Offer>{reinterpret_cast<Offer*>(user_data)};
                offer->dispatch_clear();
        }

        static std::pair<GtkTargetEntry*, int>
        targets_for_format(ClipboardFormat format)
        {
                switch (format) {
                case vte::platform::ClipboardFormat::TEXT: {
                        static GtkTargetEntry *text_targets = nullptr;
                        static int n_text_targets;

                        if (text_targets == nullptr) {
                                auto list = vte::take_freeable(gtk_target_list_new(nullptr, 0));
                                gtk_target_list_add_text_targets(list.get(),
                                                                 vte::to_integral(ClipboardFormat::TEXT));

                                text_targets = gtk_target_table_new_from_list(list.get(), &n_text_targets);
                        }

                        return {text_targets, n_text_targets};
                }

                case vte::platform::ClipboardFormat::HTML: {
                        static GtkTargetEntry *html_targets = nullptr;
                        static int n_html_targets;

                        if (html_targets == nullptr) {
                                auto list = vte::take_freeable(gtk_target_list_new(nullptr, 0));
                                gtk_target_list_add_text_targets(list.get(),
                                                                 vte::to_integral(ClipboardFormat::TEXT));
                                gtk_target_list_add(list.get(),
                                                    gdk_atom_intern_static_string(MIME_TYPE_TEXT_HTML_UTF8),
                                                    0,
                                                    vte::to_integral(ClipboardFormat::HTML));
                                gtk_target_list_add(list.get(),
                                                    gdk_atom_intern_static_string(MIME_TYPE_TEXT_HTML_UTF16),
                                                    0,
                                                    vte::to_integral(ClipboardFormat::HTML));

                                html_targets = gtk_target_table_new_from_list(list.get(), &n_html_targets);
                        }

                        return {html_targets,  n_html_targets};
                }
                default:
                        g_assert_not_reached();
                }
        }

#endif /* VTE_GTK == 3 */

        static std::pair<vte::glib::StringPtr, size_t>
        text_to_utf16_mozilla(std::string_view const& str) noexcept
        {
                // Use g_convert() instead of g_utf8_to_utf16() since the former
                // adds a BOM which Mozilla requires for text/html format.
                auto len = size_t{};
                auto data = g_convert(str.data(), str.size(),
                                      "UTF-16", // conver to UTF-16
                                      "UTF-8", // convert from UTF-8
                                      nullptr, // out bytes_read
                                      &len,
                                      nullptr);
                return {vte::glib::take_string(data), len};
        }

}; // class Clipboard::Offer


#if VTE_GTK == 4

static void* task_tag;

using VteContentProvider = GdkContentProvider;

class ContentProvider {
public:
        ContentProvider(VteContentProvider* native)
                : m_native{native}
        {
        }

        ContentProvider() = default;
        ~ContentProvider() = default;

        ContentProvider(ContentProvider const&) = delete;
        ContentProvider(ContentProvider&&) = delete;

        ContentProvider& operator=(ContentProvider const&) = delete;
        ContentProvider& operator=(ContentProvider&&) = delete;

        void take_offer(std::unique_ptr<Clipboard::Offer> offer)
        {
                m_offer = std::move(offer);
        }

        void set_format(ClipboardFormat format)
        {
                m_format = format;
                m_content_formats = format_to_content_formats(format);
        }

        void content_changed()
        {
        }

        void attach_clipboard(GdkClipboard* gdk_clipboard)
        {
        }

        void
        detach_clipboard(GdkClipboard* gdk_clipboard)
        {
                if (auto const delegate = m_offer->clipboard().m_delegate.lock()) {
                        (*delegate.*m_offer->m_clear_callback)(m_offer->clipboard());
                }
        }

        GdkContentFormats*
        ref_formats()
        {
                return m_content_formats ? gdk_content_formats_ref(m_content_formats.get()) : nullptr;
        }

        GdkContentFormats*
        ref_storable_formats()
        {
                return format_to_content_formats(ClipboardFormat::TEXT).release();
        }

        void
        write_mime_type_async(char const* mime_type,
                              GOutputStream* stream,
                              int io_priority,
                              GCancellable* cancellable,
                              GAsyncReadyCallback callback,
                              void* user_data)
        {
                auto task = vte::glib::take_ref(g_task_new(m_native, cancellable, callback, user_data));
                g_task_set_priority(task.get(), io_priority);
                g_task_set_source_tag(task.get(), &task_tag);
#if GLIB_CHECK_VERSION(2, 60, 0)
                g_task_set_name(task.get(), "vte-content-provider-write-async");
#endif

                auto const format = format_from_mime_type(mime_type);
                if (format == ClipboardFormat::INVALID)
                        return g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                                                       "Unknown format");

                if (auto const delegate = m_offer->clipboard().m_delegate.lock()) {
                        auto str = (*delegate.*m_offer->m_get_callback)(m_offer->clipboard(), format);
                        if (!str)
                                return g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                                               "Nothing on offer");

                        auto bytes = vte::Freeable<GBytes>{};
                        switch (format_from_mime_type(mime_type)) {
                        case ClipboardFormat::TEXT: {
                                bytes = vte::take_freeable(g_bytes_new_with_free_func(g_strndup(str->data(), str->size()),
                                                                                      str->size(),
                                                                                      g_free, nullptr));
                                break;
                        }

                        case ClipboardFormat::HTML: {
                                auto const type = std::string_view{mime_type};
                                if (type == MIME_TYPE_TEXT_HTML_UTF8) {
                                        bytes = vte::take_freeable(g_bytes_new_with_free_func(g_strndup(str->data(), str->size()),
                                                                                              str->size(),
                                                                                              g_free, nullptr));
                                } else if (type == MIME_TYPE_TEXT_HTML_UTF16) {
                                        auto [html, len] = m_offer->text_to_utf16_mozilla(*str);

                                        if (html) {
                                                bytes = vte::take_freeable(g_bytes_new_with_free_func(html.release(), len, g_free, nullptr));
                                                break;
                                        } else {
                                                return g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                                                                               "Invalid data");
                                        }
                                }

                                break;
                        }
                        case ClipboardFormat::INVALID:
                        default:
                                break;
                        }

                        if (bytes) {
                                auto provider = vte::glib::take_ref(gdk_content_provider_new_for_bytes(mime_type, bytes.release()));
                                return gdk_content_provider_write_mime_type_async(provider.get(),
                                                                                  mime_type,
                                                                                  stream,
                                                                                  io_priority,
                                                                                  cancellable,
                                                                                  write_mime_type_async_done_cb,
                                                                                  task.release()); // transfer
                        }
                }

                return g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                               "Offer expired");
        }

        bool
        write_mime_type_finish(GAsyncResult* result,
                               GError** error)
        {
                auto const task = G_TASK(result);
                return g_task_propagate_boolean(task, error);
        }

        bool
        get_value(GValue* value,
                  GError** error)
        {
                if (G_VALUE_HOLDS(value, G_TYPE_STRING)) {
                        if (auto const delegate = m_offer->clipboard().m_delegate.lock()) {
                                auto const str = (*delegate.*m_offer->m_get_callback)(m_offer->clipboard(), ClipboardFormat::TEXT);
                                if (!str)
                                        return false;

                                g_value_take_string(value, g_strndup(str->data(), str->size()));
                                return true;
                        }
                }

                return false;
        }

        void
        offer() noexcept
        {
                gdk_clipboard_set_content(m_offer->clipboard().platform(), m_native);
        }


private:
        VteContentProvider* m_native{nullptr}; /* unowned */

        std::unique_ptr<Clipboard::Offer> m_offer;

        ClipboardFormat m_format{ClipboardFormat::INVALID};
        vte::Freeable<GdkContentFormats> m_content_formats;

        vte::Freeable<GdkContentFormats>
        format_to_content_formats(ClipboardFormat format) noexcept
        {
                auto builder = vte::take_freeable(gdk_content_formats_builder_new());

                switch (format) {
                case ClipboardFormat::TEXT:
                        gdk_content_formats_builder_add_mime_type(builder.get(),
                                                                  MIME_TYPE_TEXT_PLAIN_UTF8);
                        break;
                case ClipboardFormat::HTML:
                        gdk_content_formats_builder_add_mime_type(builder.get(),
                                                                  MIME_TYPE_TEXT_HTML_UTF8);
                        gdk_content_formats_builder_add_mime_type(builder.get(),
                                                                  MIME_TYPE_TEXT_HTML_UTF16);
                        break;
                case ClipboardFormat::INVALID:
                default:
                        __builtin_unreachable();
                }

                return vte::take_freeable(gdk_content_formats_builder_to_formats(builder.release()));
        }

        ClipboardFormat
        format_from_mime_type(std::string_view const& mime_type) noexcept
        {
                if (mime_type == MIME_TYPE_TEXT_PLAIN_UTF8)
                        return ClipboardFormat::TEXT;
                else if (mime_type == MIME_TYPE_TEXT_HTML_UTF8 ||
                         mime_type == MIME_TYPE_TEXT_HTML_UTF16)
                        return ClipboardFormat::HTML;
                else
                        return ClipboardFormat::INVALID;
        }

        static void
        write_mime_type_async_done_cb(GObject* source,
                                      GAsyncResult* result,
                                      void* user_data) noexcept
        try
        {
                auto const provider = GDK_CONTENT_PROVIDER(source);
                auto const task = vte::glib::take_ref(reinterpret_cast<GTask*>(user_data)); // ref added on ::write_mime_type_async

                auto error = vte::glib::Error{};
                if (!gdk_content_provider_write_mime_type_finish(provider, result, error)) {
                        return g_task_return_error(task.get(), error.release());
                }

                return g_task_return_boolean(task.get(), true);
        }
        catch (...)
        {
                vte::log_exception();
        }

}; // class ContentProvider

#define VTE_TYPE_CONTENT_PROVIDER            (vte_content_provider_get_type())
#define VTE_CONTENT_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), VTE_TYPE_CONTENT_PROVIDER, VteContentProvider))
#define VTE_CONTENT_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  VTE_TYPE_CONTENT_PROVIDER, VteContentProviderClass))
#define VTE_IS_CONTENT_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), VTE_TYPE_CONTENT_PROVIDER))
#define VTE_IS_CONTENT_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  VTE_TYPE_CONTENT_PROVIDER))
#define VTE_CONTENT_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  VTE_TYPE_CONTENT_PROVIDER, VteContentProviderClass))

using VteContentProviderClass = GdkContentProviderClass;

static GType vte_content_provider_get_type(void);

G_DEFINE_TYPE_WITH_CODE(VteContentProvider, vte_content_provider, GDK_TYPE_CONTENT_PROVIDER,
                        {
                                VteContentProvider_private_offset =
                                        g_type_add_instance_private(g_define_type_id, sizeof(ContentProvider));
                        });

template<typename T>
static inline auto
IMPL(T* that)
{
        auto const pthat = reinterpret_cast<VteContentProvider*>(that);
        return std::launder(reinterpret_cast<ContentProvider*>(vte_content_provider_get_instance_private(pthat)));
}

static void
vte_content_provider_content_changed(GdkContentProvider* provider) noexcept
try
{
        GDK_CONTENT_PROVIDER_CLASS(vte_content_provider_parent_class)->content_changed(provider);

        IMPL(provider)->content_changed();
}
catch (...)
{
        vte::log_exception();
}

static void
vte_content_provider_attach_clipboard(GdkContentProvider* provider,
                                      GdkClipboard* clipboard) noexcept
try
{
        GDK_CONTENT_PROVIDER_CLASS(vte_content_provider_parent_class)->attach_clipboard(provider,
                                                                                        clipboard);

        IMPL(provider)->attach_clipboard(clipboard);
}
catch (...)
{
        vte::log_exception();
}

static void
vte_content_provider_detach_clipboard(GdkContentProvider* provider,
                                      GdkClipboard* clipboard) noexcept
try
{
        GDK_CONTENT_PROVIDER_CLASS(vte_content_provider_parent_class)->detach_clipboard(provider,
                                                                                        clipboard);

        IMPL(provider)->detach_clipboard(clipboard);
}
catch (...)
{
        vte::log_exception();
}

static GdkContentFormats*
vte_content_provider_ref_formats(GdkContentProvider* provider) noexcept
try
{
        return IMPL(provider)->ref_formats();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

static GdkContentFormats*
vte_content_provider_ref_storable_formats(GdkContentProvider* provider) noexcept
try
{
        return IMPL(provider)->ref_storable_formats();
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

static void
vte_content_provider_write_mime_type_async(GdkContentProvider* provider,
                                           char const* mime_type,
                                           GOutputStream* stream,
                                           int io_priority,
                                           GCancellable* cancellable,
                                           GAsyncReadyCallback callback,
                                           void* user_data) noexcept
try
{
        return IMPL(provider)->write_mime_type_async(mime_type,
                                                     stream,
                                                     io_priority,
                                                     cancellable,
                                                     callback, user_data);
}
catch (...)
{
        vte::log_exception();
}

static gboolean
vte_content_provider_write_mime_type_finish(GdkContentProvider* provider,
                                            GAsyncResult* result,
                                            GError** error) noexcept
try
{
        assert(g_task_is_valid(result, provider));
        assert(g_task_get_source_tag(G_TASK(result)) == &task_tag);

        return IMPL(provider)->write_mime_type_finish(result, error);
}
catch (...)
{
        vte::glib::set_error_from_exception(error);
        return false;
}

static gboolean
vte_content_provider_get_value(GdkContentProvider* provider,
                               GValue* value,
                               GError** error) noexcept
try
{
        if (IMPL(provider)->get_value(value, error))
                return true;

        return GDK_CONTENT_PROVIDER_CLASS(vte_content_provider_parent_class)->get_value(provider,
                                                                                        value,
                                                                                        error);
}
catch (...)
{
        vte::glib::set_error_from_exception(error);
        return false;
}

static void
vte_content_provider_init(VteContentProvider* provider)
try
{
        auto place = vte_content_provider_get_instance_private(provider);
        new (place) ContentProvider{provider};
}
catch (...)
{
        vte::log_exception();
        g_error("Failed to create ContentProvider\n");
}

static void
vte_content_provider_finalize(GObject* object) noexcept
{
        IMPL(object)->~ContentProvider();

        G_OBJECT_CLASS(vte_content_provider_parent_class)->finalize(object);
}

static void
vte_content_provider_class_init(VteContentProviderClass *klass)
{
        auto gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->finalize = vte_content_provider_finalize;

        auto provider_class = GDK_CONTENT_PROVIDER_CLASS(klass);
        provider_class->content_changed = vte_content_provider_content_changed;
        provider_class->attach_clipboard = vte_content_provider_attach_clipboard;
        provider_class->detach_clipboard = vte_content_provider_detach_clipboard;
        provider_class->ref_formats = vte_content_provider_ref_formats;
        provider_class->ref_storable_formats = vte_content_provider_ref_storable_formats;
        provider_class->write_mime_type_async = vte_content_provider_write_mime_type_async;
        provider_class->write_mime_type_finish = vte_content_provider_write_mime_type_finish;
        provider_class->get_value = vte_content_provider_get_value;
}

static auto
vte_content_provider_new(void) noexcept
{
        return reinterpret_cast<VteContentProvider*>
                (g_object_new(VTE_TYPE_CONTENT_PROVIDER, nullptr));
}

#endif /* VTE_GTK == 4 */

void
Clipboard::Offer::run(std::unique_ptr<Offer> offer,
                      ClipboardFormat format) noexcept
{
#if VTE_GTK == 3
        auto [targets, n_targets] = targets_for_format(format);

        // Transfers ownership of *offer to the clipboard. If setting succeeds,
        // the clipboard will own *offer until the clipboard_data_clear_cb
        // callback is called.
        // If setting the clipboard fails, the clear callback will never be
        // called.
        if (gtk_clipboard_set_with_data(offer->clipboard().platform(),
                                        targets, n_targets,
                                        clipboard_get_cb,
                                        clipboard_clear_cb,
                                        offer.get())) {
                gtk_clipboard_set_can_store(offer->clipboard().platform(), targets, n_targets);
                offer.release(); // transferred to clipboard above
        }
#elif VTE_GTK == 4
        // It seems that to make the content available lazily (i.e. only
        // generate it when the clipboard contents are requested), or
        // receive a notification when said content no longer owns the
        // clipboard, one has to write a new GdkContentProvider implementation.
        auto const provider = vte::glib::take_ref(vte_content_provider_new());
        // Transfers ownership of *offer to the provider.
        auto const impl = IMPL(provider.get());
        impl->take_offer(std::move(offer));
        impl->set_format(format);
        impl->offer();
#endif /* VTE_GTK */
}

class Clipboard::Request {
public:
        Request(Clipboard& clipboard,
                RequestDoneCallback done_callback,
                RequestFailedCallback failed_callback)
                : m_clipboard{clipboard.shared_from_this()},
                  m_done_callback{done_callback},
                  m_failed_callback{failed_callback}
        {
        }

        ~Request() = default;

        auto& clipboard() const noexcept { return *m_clipboard; }

        static void run(std::unique_ptr<Request> request) noexcept
        {
                auto const platform = request->clipboard().platform();
#if VTE_GTK == 3
                gtk_clipboard_request_text(platform,
                                           text_received_cb,
                                           request.release());
#elif VTE_GTK == 4
                gdk_clipboard_read_text_async(platform,
                                              nullptr, // cancellable
                                              GAsyncReadyCallback(text_received_cb),
                                              request.release());
#endif /* VTE_GTK */
        }

private:
        std::shared_ptr<Clipboard> m_clipboard;
        RequestDoneCallback m_done_callback;
        RequestFailedCallback m_failed_callback;

#if VTE_GTK == 3
        void dispatch(char const *text) noexcept
        try
        {
                if (auto const delegate = clipboard().m_delegate.lock()) {
                        if (text)
                                (*delegate.*m_done_callback)(clipboard(), {text, strlen(text)});
                        else
                                (*delegate.*m_failed_callback)(clipboard());
                }
        }
        catch (...)
        {
                vte::log_exception();
        }

        static void text_received_cb(GtkClipboard *clipboard,
                                     char const* text,
                                     gpointer data) noexcept
        {
                auto const request = std::unique_ptr<Request>{reinterpret_cast<Request*>(data)};
                request->dispatch(text);
        }

#elif VTE_GTK == 4

        void dispatch(GObject* source,
                      GAsyncResult* result) noexcept
        try
        {
                // Well done gtk4 to not simply tell us also the length of the received text!
                auto const text = vte::glib::take_string
                        (gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source),
                                                        result,
                                                        nullptr));

                if (auto const delegate = clipboard().m_delegate.lock()) {
                        if (text)
                                (*delegate.*m_done_callback)(clipboard(), {text.get(), strlen(text.get())});
                        else
                                (*delegate.*m_failed_callback)(clipboard());
                }
        }
        catch (...)
        {
                vte::log_exception();
        }

        static void text_received_cb(GObject* source,
                                     GAsyncResult* result,
                                     gpointer data) noexcept
        {
                auto const request = std::unique_ptr<Request>{reinterpret_cast<Request*>(data)};
                request->dispatch(source, result);
        }

#endif /* VTE_GTK */

}; // class Clipboard::Request

void
Clipboard::offer_data(ClipboardFormat format,
                      OfferGetCallback get_callback,
                      OfferClearCallback clear_callback) /* throws */
{
        Offer::run(std::make_unique<Offer>(*this, get_callback, clear_callback), format);
}

void
Clipboard::set_text(char const* text,
                    size_t size) noexcept
{
#if VTE_GTK == 3
        gtk_clipboard_set_text(platform(), text, size);
#elif VTE_GTK == 4
        // This API sucks
        gdk_clipboard_set_text(platform(), text);
#endif /* VTE_GTK */
}

void
Clipboard::request_text(RequestDoneCallback done_callback,
                        RequestFailedCallback failed_callback) /* throws */
{
        Request::run(std::make_unique<Request>(*this, done_callback, failed_callback));
}

} // namespace vte::platform
