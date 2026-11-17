/*
 * Copyright © 2001,2002 Red Hat, Inc.
 * Copyright © 2014, 2017 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <glib-unix.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <cairo/cairo-gobject.h>
#include <vte/vte.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "std-glue.hh"
#include "cairo-glue.hh"
#include "glib-glue.hh"
#include "gtk-glue.hh"
#include "libc-glue.hh"
#include "pango-glue.hh"
#include "pcre2-glue.hh"
#include "refptr.hh"
#include "vte-glue.hh"

/* options */

static void G_GNUC_PRINTF(2, 3)
verbose_fprintf(FILE* fp,
                char const* format,
                ...);

#define verbose_print(...) verbose_fprintf(stdout, __VA_ARGS__)
#define verbose_printerr(...) verbose_fprintf(stderr, __VA_ARGS__)

#define CONFIG_GROUP "VteApp Configuration"

static consteval auto
gtk_if(auto v3,
       auto v4) noexcept -> auto {
        if constexpr (VTE_GTK == 3)
                return v3;
        else if constexpr (VTE_GTK == 4)
                return v4;
        else
                __builtin_unreachable();
}

class Options {
public:
        gboolean allow_window_ops{false};
        gboolean audible_bell{false};
        gboolean a11y{gtk_if(true, false)};
        gboolean backdrop{false};
        gboolean bidi{true};
        gboolean bold_is_bright{false};
        gboolean bold{true};
        gboolean builtin_dingus{true};
        gboolean console{false};
        gboolean context_menu{true};
        gboolean debug{false};
        gboolean decorations{true};
        gboolean fallback_scrolling{true};
        gboolean feed_stdin{false};
        gboolean geometry_hints{true};
        gboolean hyperlink{true};
        gboolean icon_title{false};
        gboolean keep{false};
        gboolean kinetic_scrolling{true};
        gboolean legacy_osc777{false};
        gboolean object_notifications{false};
        gboolean overlay_scrollbar{false};
        gboolean pty{true};
        gboolean require_systemd_scope{false};
        gboolean reverse{false};
        gboolean rewrap{true};
        gboolean scroll_on_insert{false};
        gboolean scroll_on_keystroke{true};
        gboolean scroll_on_output{false};
        gboolean scroll_unit_is_pixels{false};
        gboolean scrollbar{true};
        gboolean shaping{true};
        gboolean shell{true};
        gboolean systemd_scope{true};
        gboolean test_mode{false};
        gboolean track_clipboard_targets{false};
        gboolean use_scrolled_window{false};
        gboolean use_theme_colors{false};
        gboolean version{false};
        gboolean whole_window_transparent{false};
        gboolean xfill{true};
        gboolean yfill{true};
        bool bg_color_set{false};
        bool fg_color_set{false};
        bool cursor_bg_color_set{false};
        bool cursor_fg_color_set{false};
        bool hl_bg_color_set{false};
        bool hl_fg_color_set{false};
        cairo_extend_t background_extend{CAIRO_EXTEND_NONE};
        char* command{nullptr};
        char* encoding{nullptr};
        char* font_string{nullptr};
        char* geometry{nullptr};
        char* output_filename{nullptr};
        char* title{nullptr};
        char* word_char_exceptions{nullptr};
        char* working_directory{nullptr};
        char** dingus{nullptr};
        char** exec_argv{nullptr};
        char** environment{nullptr};
        GdkPixbuf* background_pixbuf{nullptr};
        GdkRGBA bg_color{1.0, 1.0, 1.0, 1.0};
        GdkRGBA fg_color{0.0, 0.0, 0.0, 1.0};
        GdkRGBA cursor_bg_color{};
        GdkRGBA cursor_fg_color{};
        GdkRGBA hl_bg_color{};
        GdkRGBA hl_fg_color{};
        int cjk_ambiguous_width{1};
        int extra_margin{-1};
        int scrollback_lines{-1 /* infinite */};
        int transparency_percent{-1};
        int verbosity{0};
        double cell_height_scale{1.0};
        double cell_width_scale{1.0};
        VteCursorBlinkMode cursor_blink_mode{VTE_CURSOR_BLINK_SYSTEM};
        VteCursorShape cursor_shape{VTE_CURSOR_SHAPE_BLOCK};
        VteTextBlinkMode text_blink_mode{VTE_TEXT_BLINK_ALWAYS};
        VteAlign xalign{VteAlign(-1)};
        VteAlign yalign{VteAlign(-1)};
        vte::glib::RefPtr<GtkCssProvider> css{};

#if VTE_GTK == 3
        gboolean argb_visual{true};
        gboolean double_buffer{true};
#endif /* VTE_GTK == 3 */

        ~Options() {
                g_clear_object(&background_pixbuf);
                g_free(command);
                g_free(encoding);
                g_free(font_string);
                g_free(geometry);
                g_free(output_filename);
                g_free(word_char_exceptions);
                g_free(working_directory);
                g_strfreev(dingus);
                g_strfreev(exec_argv);
                g_strfreev(environment);
        }

        auto fds()
        {
                auto fds = std::vector<int>{};
                fds.reserve(m_fds.size());
                for (auto& fd : m_fds)
                        fds.emplace_back(fd.get());

                return fds;
        }

        auto map_fds()
        {
                return m_map_fds;
        }

        auto environment_for_spawn() const noexcept
        {
                auto envv = g_get_environ();

                // Merge in extra variables
                if (environment) {
                        for (auto i = 0; environment[i]; ++i) {
                                auto const eq = strchr(environment[i], '=');
                                if (eq) {
                                        auto const var = vte::glib::take_string(g_strndup(environment[i], eq - environment[i]));
                                        envv = g_environ_setenv(envv, var.get(), eq + 1, true);
                                } else {
                                        envv = g_environ_unsetenv(envv, environment[i]);
                                }
                        }
                }

                // Cleanup environment
                // List of variables and prefixes copied from gnome-terminal.
                for (auto const& var : {"COLORTERM",
                                        "COLUMNS",
                                        "DESKTOP_STARTUP_ID",
                                        "EXIT_CODE",
                                        "EXIT_STATUS",
                                        "GIO_LAUNCHED_DESKTOP_FILE",
                                        "GIO_LAUNCHED_DESKTOP_FILE_PID",
                                        "GJS_DEBUG_OUTPUT",
                                        "GJS_DEBUG_TOPICS",
                                        "GNOME_DESKTOP_ICON",
                                        "INVOCATION_ID",
                                        "JOURNAL_STREAM",
                                        "LINES",
                                        "LISTEN_FDNAMES",
                                        "LISTEN_FDS",
                                        "LISTEN_PID",
                                        "MAINPID",
                                        "MANAGERPID",
                                        "NOTIFY_SOCKET",
                                        "NOTIFY_SOCKET",
                                        "PIDFILE",
                                        "PWD",
                                        "REMOTE_ADDR",
                                        "REMOTE_PORT",
                                        "SERVICE_RESULT",
                                        "SHLVL",
                                        "TERM",
                                        "VTE_VERSION",
                                        "WATCHDOG_PID",
                                        "WATCHDOG_USEC",
                                        "WINDOWID"}) {
                        envv = g_environ_unsetenv(envv, var);
                }

                for (auto const& prefix : {"GNOME_TERMINAL_",

                                           // other terminals
                                           "FOOT_",
                                           "ITERM2_",
                                           "MC_",
                                           "MINTTY_",
                                           "PUTTY_",
                                           "RXVT_",
                                           "TERM_",
                                           "URXVT_",
                                           "WEZTERM_",
                                           "XTERM_"}) {
                        for (auto i = 0; envv[i]; ++i) {
                                if (!g_str_has_prefix (envv[i], prefix))
                                        continue;

                                auto const eq = strchr(envv[i], '=');
                                g_assert(eq);
                                auto const var = vte::glib::take_string(g_strndup(envv[i], eq - envv[i]));
                                envv = g_environ_unsetenv(envv, var.get());
                        }
                }

                return vte::glib::take_strv(envv);
        }

private:

        std::vector<vte::libc::FD> m_fds{};
        std::vector<int> m_map_fds{};

        bool parse_enum(GType type,
                        char const* str,
                        int& value,
                        GError** error)
        {
                GEnumClass* enum_klass = reinterpret_cast<GEnumClass*>(g_type_class_ref(type));
                GEnumValue* enum_value = g_enum_get_value_by_nick(enum_klass, str);

                if (enum_value == nullptr) {
                        g_type_class_unref(enum_klass);
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse enum value \"%s\" as type \"%s\"",
                                    str, g_quark_to_string(g_type_qname(type)));
                        return false;
                }

                value = enum_value->value;
                g_type_class_unref(enum_klass);
                return true;
        }

        bool parse_width_enum(char const* str,
                              int& value,
                              GError** error)
        {
                int v = 1;
                if (str == nullptr || g_str_equal(str, "narrow"))
                        v = 1;
                else if (g_str_equal(str, "wide"))
                        v = 2;
                else {
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse \"%s\" as width (allowed values are \"narrow\" or \"wide\")", str);
                        return false;
                }

                value = v;
                return true;
        }

        bool parse_color(char const* str,
                         GdkRGBA* value,
                         bool* value_set,
                         GError** error)
        {
                GdkRGBA color;
                if (!gdk_rgba_parse(&color, str)) {
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse \"%s\" as color", str);
                        if (value_set)
                                *value_set = false;
                        return false;
                }

                *value = color;
                if (value_set)
                        *value_set = true;
                return true;
        }

        int parse_fd_arg(char const* arg,
                         char** end_ptr,
                         GError** error)
        {
                errno = 0;
                char* end = nullptr;
                auto const v = g_ascii_strtoll(arg, &end, 10);
                if (errno || end == arg || v < G_MININT || v > G_MAXINT) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Failed to parse \"%s\" as file descriptor number", arg);
                        return -1;
                }
                if (v == -1) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "\"%s\" is not a valid file descriptor number", arg);
                        return -1;
                }

                if (end_ptr) {
                        *end_ptr = end;
                } else if (*end) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Extra characters after number in \"%s\"", arg);
                        return -1;
                }

                return int(v);
        }

        bool parse_fd_arg(char const* str,
                          GError** error)
        {
                char *end = nullptr;
                auto fd = parse_fd_arg(str, &end, error);
                if (fd == -1)
                        return FALSE;

                auto map_to = int{};
                if (*end == '=' || *end == ':') {
                        map_to = parse_fd_arg(end + 1, nullptr, error);
                        if (map_to == -1)
                                return false;

                        if (map_to == STDIN_FILENO ||
                            map_to == STDOUT_FILENO ||
                            map_to == STDERR_FILENO) {
                                g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                             "Cannot map file descriptor to %d (reserved)", map_to);
                                return false;
                        }
                } else if (*end) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Failed to parse \"%s\" as file descriptor assignment", str);
                        return false;
                } else {
                        map_to = fd;
                }

                /* N:M assigns, N=M assigns a dup of N. Always dup stdin/out/err since
                 * we need to output messages ourself there, too.
                 */
                auto new_fd = int{};
                if (*end == '=' || fd < 3) {
                        new_fd = vte::libc::fd_dup_cloexec(fd, 3);
                        if (new_fd == -1) {
                                auto errsv = vte::libc::ErrnoSaver{};
                                g_set_error (error, G_IO_ERROR, g_io_error_from_errno(errsv),
                                             "Failed to duplicate file descriptor %d: %s",
                                             fd, g_strerror(errsv));
                                return false;
                        }
                } else {
                        new_fd = fd;
                        if (vte::libc::fd_set_cloexec(fd) == -1) {
                                auto errsv = vte::libc::ErrnoSaver{};
                                g_set_error (error, G_IO_ERROR, g_io_error_from_errno(errsv),
                                             "Failed to set cloexec on file descriptor %d: %s",
                                             fd, g_strerror(errsv));
                                return false;
                        }
                }

                m_fds.emplace_back(new_fd);
                m_map_fds.emplace_back(map_to);
                return true;
        }

        bool parse_geometry(char const* str,
                            GError** error)
        {
                g_free(geometry);
                geometry = g_strdup(str);
                return true;
        }

#if VTE_GTK == 4
        static void
        parse_css_error_cb(GtkCssProvider* provider,
                           void* section,
                           GError* error,
                           GError** ret_error) noexcept
        {
                if (!error)
                        return;

                if (error->domain == GTK_CSS_PARSER_WARNING)
                        verbose_printerr("Warning parsing CSS: %s", error->message);
                else
                        *ret_error = g_error_copy(error);
        }
#endif /* VTE_GTK == 4 */

        bool
        parse_css(char const* value,
                  GError** error)
        {
                auto provider = vte::glib::take_ref(gtk_css_provider_new());
#if VTE_GTK == 3
                if (!gtk_css_provider_load_from_path(provider.get(), value, error))
                    return false;

                g_object_set_data_full(G_OBJECT(provider.get()), "VTEAPP_PATH",
                                       g_strdup(value), GDestroyNotify(g_free));

#elif VTE_GTK == 4
                GError* err = nullptr;
                auto const id = g_signal_connect(provider.get(), "parsing-error",
                                                 G_CALLBACK(parse_css_error_cb), &err);

                gtk_css_provider_load_from_path(provider.get(), value);
                g_signal_handler_disconnect(provider.get(), id);
                if (err) {
                        g_propagate_prefixed_error(error, err,
                                                   "Error parsing CSS file \"%s\": ",
                                                   value);
                        return false;
                }
#endif /* VTE_GTK */

                css = std::move(provider);
                return true;
        }

        static gboolean
        parse_background_image(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                g_clear_object(&that->background_pixbuf);
                that->background_pixbuf = gdk_pixbuf_new_from_file(value, error);
                if (that->background_pixbuf)
                        g_object_set_data_full(G_OBJECT(that->background_pixbuf), "VTEAPP_PATH",
                                               g_strdup(value), GDestroyNotify(g_free));

                return that->background_pixbuf != nullptr;
        }

        static gboolean
        parse_background_extend(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(CAIRO_GOBJECT_TYPE_EXTEND, value, v, error);
                if (rv)
                        that->background_extend = cairo_extend_t(v);
                return rv;
        }

        static gboolean
        parse_cjk_width(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_width_enum(value, that->cjk_ambiguous_width, error);
        };

        static gboolean
        parse_cursor_blink(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(VTE_TYPE_CURSOR_BLINK_MODE, value, v, error);
                if (rv)
                        that->cursor_blink_mode = VteCursorBlinkMode(v);
                return rv;
        }

        static gboolean
        parse_cursor_shape(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(VTE_TYPE_CURSOR_SHAPE, value, v, error);
                if (rv)
                        that->cursor_shape = VteCursorShape(v);
                return rv;
        }

        static gboolean
        parse_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                bool set;
                return that->parse_color(value, &that->bg_color, &set, error);
        }

        static gboolean
        parse_css_file(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_css(value, error);
        }

        static gboolean
        parse_fd(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_fd_arg(value, error);
        }

        static gboolean
        parse_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                bool set;
                return that->parse_color(value, &that->fg_color, &set, error);
        }

        static gboolean
        parse_cursor_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->cursor_bg_color, &that->cursor_bg_color_set, error);
        }

        static gboolean
        parse_cursor_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->cursor_fg_color, &that->cursor_fg_color_set, error);
        }

        static gboolean
        parse_hl_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->hl_bg_color, &that->hl_bg_color_set, error);
        }

        static gboolean
        parse_hl_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->hl_fg_color, &that->hl_fg_color_set, error);
        }

        static gboolean
        parse_text_blink(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(VTE_TYPE_TEXT_BLINK_MODE, value, v, error);
                if (rv)
                        that->text_blink_mode = VteTextBlinkMode(v);
                return rv;
        }

        static gboolean
        parse_verbosity(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                that->verbosity++;
                return true;
        }

        static gboolean
        parse_xalign(char const* option, char const* value, void* data, GError** error)
        {
                auto const that = static_cast<Options*>(data);
                auto v = int{};
                auto const rv = that->parse_enum(VTE_TYPE_ALIGN, value, v, error);
                if (rv)
                        that->xalign = VteAlign(v);
                return rv;
        }

        static gboolean
        parse_yalign(char const* option, char const* value, void* data, GError** error)
        {
                auto const that = static_cast<Options*>(data);
                auto v = int{};
                auto const rv = that->parse_enum(VTE_TYPE_ALIGN, value, v, error);
                if (rv)
                        that->yalign = VteAlign(v);
                return rv;
        }

        char const*
        default_config_path() {
                auto const dh = g_get_user_config_dir();
                auto ini_path = vte::glib::take_string(g_build_filename(dh, "vteapp.ini", nullptr));
                return g_intern_string(ini_path.get());
        }

        bool
        load_config(GKeyFile* keyfile,
                    char const* path,
                    GError** error)
        {
                if (!g_key_file_load_from_file(keyfile,
                                               path ? path : default_config_path(),
                                               GKeyFileFlags(G_KEY_FILE_KEEP_COMMENTS |
                                                             G_KEY_FILE_KEEP_TRANSLATIONS),
                                               error))
                        return false;

                // Load the config from the keyfile. Don't error out on invalid
                // data, just ignore it.

                auto load_bool_option = [&](char const* key,
                                            gboolean* ptr) noexcept -> void
                {
                        auto err = vte::glib::Error{};
                        auto const v = g_key_file_get_boolean(keyfile, CONFIG_GROUP, key, err);
                        if (!err.error())
                                *ptr = v != false;
                };

                load_bool_option("AllowWindowOps", &allow_window_ops);
                load_bool_option("AudibleBell", &audible_bell);
                load_bool_option("Accessibility", &a11y);
                load_bool_option("Backdrop", &backdrop);
                load_bool_option("BboldIsBright", &bold_is_bright);
                load_bool_option("BiDi", &bidi);
                load_bool_option("Bold", &bold);
                load_bool_option("BuiltinDingus", &builtin_dingus);
                load_bool_option("ContextMenu", &context_menu);
                load_bool_option("Debug", &debug);
                load_bool_option("Decorations", &decorations);
                load_bool_option("FallbackScrolling", &fallback_scrolling);
                load_bool_option("GeometryHints", &geometry_hints);
                load_bool_option("Hyperlink", &hyperlink);
                load_bool_option("Keep", &keep);
                load_bool_option("KineticScrolling", &kinetic_scrolling);
                load_bool_option("ObjectNotifications", &object_notifications);
                load_bool_option("OverlayScrollbar", &overlay_scrollbar);
                load_bool_option("Pty", &pty);
                load_bool_option("RequireSystemdScope", &require_systemd_scope);
                load_bool_option("Reverse", &reverse);
                load_bool_option("Rewrap", &rewrap);
                load_bool_option("ScrollUnitIsPixels", &scroll_unit_is_pixels);
                load_bool_option("Scrollbar", &scrollbar);
                load_bool_option("ScrolledWindow", &use_scrolled_window);
                load_bool_option("ScrollOnInsert", &scroll_on_insert);
                load_bool_option("ScrollOnKeystroke", &scroll_on_keystroke);
                load_bool_option("ScrollOnOutput", &scroll_on_output);
                load_bool_option("Shaping", &shaping);
                load_bool_option("Shell", &shell);
                load_bool_option("SystemdScope", &systemd_scope);
                load_bool_option("TestMode", &test_mode);
                load_bool_option("TrackClipboardTargets", &track_clipboard_targets);
                load_bool_option("UseThemeColors", &use_theme_colors);
                load_bool_option("WholeWindowTransparent", &whole_window_transparent);
                load_bool_option("XFill", &xfill);
                load_bool_option("YFill", &yfill);
#if VTE_GTK == 3
                load_bool_option("ArgbVisual", &argb_visual);
                load_bool_option("DoubleBuffer", &double_buffer);
#endif /* VTE_GTK == 3 */

                auto load_color_option = [&](char const* key,
                                             GdkRGBA* ptr,
                                             bool* setptr) noexcept -> void
                {
                        auto str = vte::glib::take_string
                                (g_key_file_get_string(keyfile, CONFIG_GROUP, key, nullptr));
                        if (!str)
                                return;

                        parse_color(str.get(), ptr, setptr, nullptr);
                };

                load_color_option("BackgroundColor", &bg_color, &bg_color_set);
                load_color_option("CursorBackgroundColor", &cursor_bg_color, &cursor_bg_color_set);
                load_color_option("CursorForegroundColor", &cursor_fg_color, &cursor_fg_color_set);
                load_color_option("ForegroundColor", &fg_color, &fg_color_set);
                load_color_option("HighlightBackgroundColor", &hl_bg_color, &hl_bg_color_set);
                load_color_option("HighlightForegroundColor", &hl_fg_color, &hl_fg_color_set);

                auto load_double_option = [&](char const* key,
                                              double* ptr) noexcept -> void
                {
                        auto err = vte::glib::Error{};
                        auto const v = g_key_file_get_double(keyfile, CONFIG_GROUP, key, err);
                        if (!err.error() &&
                            std::isfinite(v))
                                *ptr = v;
                };

                load_double_option("CellHeightScale", &cell_height_scale);
                load_double_option("CellWidthScale", &cell_width_scale);

                auto load_enum_option = [&](char const* key,
                                            GType enum_type,
                                            auto* ptr) noexcept -> void
                {
                        auto str = vte::glib::take_string
                                (g_key_file_get_string(keyfile, CONFIG_GROUP, key, nullptr));
                        if (!str)
                                return;

                        auto v = 0;
                        if (parse_enum(enum_type, str.get(), v, nullptr))
                                *ptr = (decltype(*ptr))v;
                };

                load_enum_option("BackgroundExtend", CAIRO_GOBJECT_TYPE_EXTEND, &background_extend);
                load_enum_option("TextBlink", VTE_TYPE_TEXT_BLINK_MODE, &text_blink_mode);
                load_enum_option("CursorBlink", VTE_TYPE_CURSOR_BLINK_MODE, &cursor_blink_mode);
                load_enum_option("CursorShape", VTE_TYPE_CURSOR_SHAPE, &cursor_shape);
                load_enum_option("XAlign", VTE_TYPE_ALIGN, &xalign);
                load_enum_option("YAlign", VTE_TYPE_ALIGN, &yalign);

                auto load_int_option = [&](char const* key,
                                           int* ptr) noexcept -> void
                {
                        auto err = vte::glib::Error{};
                        auto v = g_key_file_get_integer(keyfile, CONFIG_GROUP, key, err);
                        if (!err.error())
                                *ptr = v;
                };

                load_int_option("ExtraMargin", &extra_margin);
                load_int_option("ScrollbackLines", &scrollback_lines);
                load_int_option("Transparent", &transparency_percent); // 0..100
                load_int_option("Verbosity", &verbosity);

                auto load_string_option = [&](char const* key,
                                              char** ptr,
                                              bool raw = false) noexcept -> void
                {
                        auto str = vte::glib::take_string
                                (g_key_file_get_string(keyfile, CONFIG_GROUP, key, nullptr));

                        // FIXME: decode to allow non-UTF-8 if @raw
                        if (*ptr)
                                g_free(*ptr);
                        *ptr = str.release();
                };

                load_string_option("Encoding", &encoding);
                load_string_option("Font", &font_string);
                load_string_option("Geometry", &geometry);
                load_string_option("Title", &title);
                load_string_option("WordCharExceptions", &word_char_exceptions);
                load_string_option("WorkingDirectory", &working_directory, true);

                auto load_strv_option = [&](char const* key,
                                            char*** ptr,
                                            bool raw = false) noexcept -> void
                {
                        auto len = gsize{0};
                        auto strv = vte::glib::take_strv
                                (g_key_file_get_string_list(keyfile, CONFIG_GROUP, key, &len, nullptr));
                        // FIXME: decode to allow non-UTF-8 if @raw
                        if (*ptr)
                                g_strfreev(*ptr);
                        *ptr = strv.release();
                };

                load_strv_option("Dingu", &dingus, false);
                load_strv_option("Env", &environment, true);

                // load bgimage option
                {
                        auto str = vte::glib::take_string
                                (g_key_file_get_string(keyfile, CONFIG_GROUP, "BackgroundImage", nullptr));
                        if (str) {
                                g_clear_object(&background_pixbuf);
                                background_pixbuf = gdk_pixbuf_new_from_file(str.get(), nullptr);
                                if (background_pixbuf)
                                        g_object_set_data_full(G_OBJECT(background_pixbuf), "VTEAPP_PATH",
                                                               g_strdup(str.get()), GDestroyNotify(g_free));

                        }
                }

                // load CJK width option
                {
                        auto str = vte::glib::take_string
                                (g_key_file_get_string(keyfile, CONFIG_GROUP, "CJKWidth", nullptr));
                        if (str) {
                                auto v = 1;
                                if (parse_width_enum(str.get(), v, nullptr))
                                        cjk_ambiguous_width = v;
                        }
                };

                // load css option
                {
                        auto str = vte::glib::take_string
                                (g_key_file_get_string(keyfile, CONFIG_GROUP, "CssFile", nullptr));
                        if (str)
                                parse_css(str.get(), nullptr);
                };

                // Note that the following command line options don't
                // have a config file equivalent:
                // --fd, --feed-stdin, --output-file, --version

                return true;
        }

        bool
        save_config(GKeyFile* keyfile,
                    char const* save_path,
                    GError** error)
        {
                // Store the config into the keyfile.

                auto const defopt = Options{}; // for default values

                auto save_bool_option = [&](char const* key,
                                            bool v,
                                            bool dv) noexcept -> void
                {
                        if (v == dv)
                                return;

                        g_key_file_set_boolean(keyfile, CONFIG_GROUP, key, v);
                };

                save_bool_option("AllowWindowOps" , allow_window_ops, defopt.allow_window_ops);
                save_bool_option("AudibleBell" , audible_bell, defopt.audible_bell);
                save_bool_option("Accessibility", a11y, defopt.a11y);
                save_bool_option("Backdrop" , backdrop, defopt.backdrop);
                save_bool_option("BboldIsBright" , bold_is_bright, defopt.bold_is_bright);
                save_bool_option("BiDi" , bidi, defopt.bidi);
                save_bool_option("Bold" , bold, defopt.bold);
                save_bool_option("BuiltinDingus" , builtin_dingus, defopt.builtin_dingus);
                save_bool_option("ContextMenu" , context_menu, defopt.context_menu);
                save_bool_option("Debug" , debug, defopt.debug);
                save_bool_option("Decorations" , decorations, defopt.decorations);
                save_bool_option("FallbackScrolling" , fallback_scrolling, defopt.fallback_scrolling);
                save_bool_option("GeometryHints" , geometry_hints, defopt.geometry_hints);
                save_bool_option("Hyperlink" , hyperlink, defopt.hyperlink);
                save_bool_option("Keep" , keep, defopt.keep);
                save_bool_option("KineticScrolling" , kinetic_scrolling, defopt.kinetic_scrolling);
                save_bool_option("ObjectNotifications" , object_notifications, defopt.object_notifications);
                save_bool_option("OverlayScrollbar" , overlay_scrollbar, defopt.overlay_scrollbar);
                save_bool_option("Pty" , pty, defopt.pty);
                save_bool_option("RequireSystemdScope" , require_systemd_scope, defopt.require_systemd_scope);
                save_bool_option("Reverse" , reverse, defopt.reverse);
                save_bool_option("Rewrap" , rewrap, defopt.rewrap);
                save_bool_option("ScrollUnitIsPixels" , scroll_unit_is_pixels, defopt.scroll_unit_is_pixels);
                save_bool_option("Scrollbar" , scrollbar, defopt.scrollbar);
                save_bool_option("ScrolledWindow" , use_scrolled_window, defopt.use_scrolled_window);
                save_bool_option("ScrollOnInsert", scroll_on_insert, defopt.scroll_on_insert);
                save_bool_option("ScrollOnKeystroke", scroll_on_keystroke, defopt.scroll_on_keystroke);
                save_bool_option("ScrollOnOutput", scroll_on_output, defopt.scroll_on_output);
                save_bool_option("Shaping" , shaping, defopt.shaping);
                save_bool_option("Shell" , shell, defopt.shell);
                save_bool_option("SystemdScope" , systemd_scope, defopt.systemd_scope);
                save_bool_option("TestMode" , test_mode, defopt.test_mode);
                save_bool_option("TrackClipboardTargets" , track_clipboard_targets, defopt.track_clipboard_targets);
                save_bool_option("UseThemeColors" , use_theme_colors, defopt.use_theme_colors);
                save_bool_option("WholeWindowTransparent" , whole_window_transparent, defopt.whole_window_transparent);
                save_bool_option("XFill" , xfill, defopt.xfill);
                save_bool_option("YFill" , yfill, defopt.yfill);
#if VTE_GTK == 3
                save_bool_option("ArgbVisual" , argb_visual, defopt.argb_visual);
                save_bool_option("DoubleBuffer" , double_buffer, defopt.double_buffer);
#endif /* VTE_GTK == 3 */

                auto save_color_option = [&](char const* key,
                                             GdkRGBA const& color,
                                             bool set) noexcept -> void
                {
                        if (!set)
                                return;

                        auto str = vte::glib::take_string(gdk_rgba_to_string(&color));
                        g_key_file_set_string(keyfile, CONFIG_GROUP, key, str.get());
                };

                save_color_option("BackgroundColor", bg_color, bg_color_set);
                save_color_option("CursorBackgroundColor", cursor_bg_color, cursor_bg_color_set);
                save_color_option("CursorForegroundColor", cursor_fg_color, cursor_fg_color_set);
                save_color_option("ForegroundColor", fg_color, fg_color_set);
                save_color_option("HighlightBackgroundColor", hl_bg_color, hl_bg_color_set);
                save_color_option("HighlightForegroundColor", hl_fg_color, hl_fg_color_set);

                auto save_double_option = [&](char const* key,
                                              double v,
                                              double dv) noexcept -> void
                {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
                        if (v == dv)
                                return;
#pragma GCC diagnostic pop

                        g_key_file_set_double(keyfile, CONFIG_GROUP, key, v);
                };

                save_double_option("CellHeightScale" , cell_height_scale, defopt.cell_height_scale);
                save_double_option("CellWidthScale" , cell_width_scale, defopt.cell_width_scale);

                auto save_enum_option = [&](char const* key,
                                            GType enum_type,
                                            auto v,
                                            auto dv) noexcept -> void
                {
                        if (v == dv)
                                return;

                        auto enum_klass = reinterpret_cast<GEnumClass*>(g_type_class_ref(enum_type));
                        if (auto const ev = g_enum_get_value(enum_klass, int(v))) {
                                g_key_file_set_string(keyfile, CONFIG_GROUP, key, ev->value_nick);
                        }
                        g_type_class_unref(enum_klass);
                };

                save_enum_option("BackgroundExtend" , CAIRO_GOBJECT_TYPE_EXTEND, background_extend, defopt.background_extend);
                save_enum_option("TextBlink" , VTE_TYPE_TEXT_BLINK_MODE, text_blink_mode, defopt.text_blink_mode);
                save_enum_option("CursorBlink" , VTE_TYPE_CURSOR_BLINK_MODE, cursor_blink_mode, defopt.cursor_blink_mode);
                save_enum_option("CursorShape" , VTE_TYPE_CURSOR_SHAPE, cursor_shape, defopt.cursor_shape);
                save_enum_option("XAlign" , VTE_TYPE_ALIGN, xalign, defopt.xalign);
                save_enum_option("YAlign" , VTE_TYPE_ALIGN, yalign, defopt.yalign);

                auto save_int_option = [&](char const* key,
                                           int v,
                                           int dv) noexcept -> void
                {
                        if (v == dv)
                                return;

                        g_key_file_set_integer(keyfile, CONFIG_GROUP, key, v);
                };

                save_int_option("ExtraMargin" , extra_margin, defopt.extra_margin);
                save_int_option("ScrollbackLines" , scrollback_lines, defopt.scrollback_lines);
                save_int_option("Transparent" , transparency_percent, defopt.transparency_percent); // 0..100
                save_int_option("Verbosity" , verbosity, defopt.verbosity);

                auto save_string_option = [&](char const* key,
                                              char const* v,
                                              char const* dv,
                                              bool raw = false) noexcept -> void
                {
                        if (!v || g_strcmp0(v, dv) == 0)
                                return;

                        // FIXME: encode to allow non-UTF-8 if @raw
                        if (raw && !g_utf8_validate(v, -1, nullptr))
                                return;

                        g_key_file_set_string(keyfile, CONFIG_GROUP, key, v);
                };

                save_string_option("Encoding" , encoding, defopt.encoding);
                save_string_option("Font" , font_string, defopt.font_string);
                save_string_option("Geometry" , geometry, defopt.geometry);
                save_string_option("Title" , title, defopt.title);
                save_string_option("WordCharExceptions" , word_char_exceptions, defopt.word_char_exceptions);
                save_string_option("WorkingDirectory" , working_directory, defopt.working_directory, true);

                auto save_strv_option = [&](char const* key,
                                            char const* const* v,
                                            char const* const* dv,
                                            bool raw = false) noexcept -> void
                {
                        if (!v || (v && dv && g_strv_equal(v, dv)))
                                return;

                        // FIXME: encode to allow non-UTF-8 if @raw
                        if (raw) {
                                for (auto i = 0; v[i]; ++i) {
                                        if (!g_utf8_validate(v[i], -1, nullptr))
                                                return;
                                }
                        }

                        g_key_file_set_string_list(keyfile, CONFIG_GROUP, key,
                                                   v, g_strv_length((char**)v));
                };

                save_strv_option("Dingu" , dingus, defopt.dingus, false);
                save_strv_option("Env" , environment, defopt.environment, true);

                // save bgimage option
                {
                        if (background_pixbuf) {
                                auto const path = (char const*)g_object_get_data(G_OBJECT(background_pixbuf), "VTEAPP_PATH");
                                if (path) {
                                        // FIXME: encode to allow non-UTF-8
                                        g_key_file_set_string(keyfile, CONFIG_GROUP, "BackgroundImage", path);
                                }
                        }
                }

                // save CJK width option
                {
                        if (cjk_ambiguous_width != defopt.cjk_ambiguous_width) {
                                g_key_file_set_string(keyfile, CONFIG_GROUP, "CJKWidth",
                                                      cjk_ambiguous_width == 2 ? "wide" : "narrow");
                        }
                };

                // save css option
                {
                        if (css) {
                                auto const path = (char const*)g_object_get_data(G_OBJECT(css.get()), "VTEAPP_PATH");
                                if (path) {
                                        // FIXME: encode to allow non-UTF-8
                                        g_key_file_set_string(keyfile, CONFIG_GROUP, "CssFile", path);
                                }
                        }
                };

                // Now save to keyfile
                return g_key_file_save_to_file(keyfile,
                                               save_path ? save_path : default_config_path(),
                                               error);
        }

public:

        double get_alpha() const
        {
                return double(100 - CLAMP(transparency_percent, 0, 100)) / 100.0;
        }

        double get_alpha_bg() const
        {
                double alpha;
                if (background_pixbuf != nullptr)
                        alpha = 0.0;
                else if (whole_window_transparent)
                        alpha = 1.0;
                else
                        alpha = get_alpha();

                return alpha;
        }

        double get_alpha_bg_for_draw() const
        {
                double alpha;
                if (whole_window_transparent)
                        alpha = 1.0;
                else
                        alpha = get_alpha();

                return alpha;
        }

        GdkRGBA get_color_bg() const
        {
                GdkRGBA color{bg_color};
                color.alpha = get_alpha_bg();
                return color;
        }

        GdkRGBA get_color_fg() const
        {
                return fg_color;
        }

        bool parse_argv(int argc,
                        char* argv[],
                        GError** error)
        {
                auto entries = std::vector<GOptionEntry>{};

                auto add_bool_option = [&](char const* option,
                                           char const short_option,
                                           char const* negated_option,
                                           char const negated_short_option,
                                           int flags,
                                           gboolean* arg_data,
                                           char const* desc,
                                           char const* negated_desc) constexpr noexcept -> void
                {
                        entries.push_back({option,
                                           short_option,
                                           // hide this option if it's the default anyway
                                           flags | (*arg_data ? G_OPTION_FLAG_HIDDEN : 0),
                                           G_OPTION_ARG_NONE,
                                           arg_data,
                                           desc,
                                           nullptr});
                        entries.push_back({negated_option,
                                           negated_short_option,
                                           // hide this option if it's the default anyway
                                           flags | (*arg_data ? 0 : G_OPTION_FLAG_HIDDEN) | G_OPTION_FLAG_REVERSE,
                                           G_OPTION_ARG_NONE,
                                           arg_data,
                                           negated_desc,
                                           nullptr});
                };

                add_bool_option("allow-window-ops", 0, "no-allow-window-ops", 0,
                                0, &allow_window_ops,
                                "Allow window operations (resize, move, raise/lower, (de)iconify)",
                                "Disallow window operations (resize, move, raise/lower, (de)iconify)");
                add_bool_option("audible-bell", 'a', "no-audible-bell", 0,
                                0, &audible_bell,
                                "Enable audible terminal bell",
                                "Disable audible terminal bell");
                add_bool_option("a11y", 0, "no-a11y", 0,
                                0, &a11y,
                                "Enable acessibility",
                                "Disable accessibility");
                add_bool_option("backdrop", 0, "no-backdrop", 0,
                                0, &backdrop,
                                "Enable dimming when toplevel unfocused",
                                "Disable dimming when toplevel unfocused");
                add_bool_option("bold-is-bright", 'B', "no-bold-is-bright", 0,
                                0, &bold_is_bright,
                                "Bold to also brightens colors",
                                "Bold does not also brightens colors");
                add_bool_option("debug", 'd', "no-debug", 0,
                                0, &debug,
                                "Enable various debugging checks",
                                "Disable various debugging checks");
                add_bool_option("keep", 'k', "no-keep", 0,
                                0, &keep,
                                "Live on after the command exits",
                                "Exit after the command exits");
                add_bool_option("bidi", 0, "no-bidi", 0,
                                0, &bidi,
                                "Enable BiDi",
                                "Disable BiDi");
                add_bool_option("bold", 0, "no-bold", 0,
                                0, &bold,
                                "Enable bold",
                                "Disable bold");
                add_bool_option("builtin-dingus", 0, "no-builtin-dingus", 0,
                                0, &builtin_dingus,
                                "Highlight URLs inside the terminal",
                                "Don't highlight URLs inside the terminal");
                add_bool_option("context-menu", 0, "no-context-menu", 0,
                                0, &context_menu,
                                "Enable context menu",
                                "Disable context menu");
                add_bool_option("decorations", 0, "no-decorations", 0,
                                0, &decorations,
                                "Enable window decorations",
                                "Disable window decorations");
                add_bool_option("fallback-scrolling", 0, "no-fallback-scrolling", 0,
                                0, &fallback_scrolling,
                                "Enable fallback scrolling",
                                "Disable fallback scrolling");
                add_bool_option("geometry-hints", 0, "no-geometry-hints", 'G',
                                0,&geometry_hints,
                                "Allow the terminal to be resized to any dimension, not constrained to fit to an integer multiple of characters",
                                "Disallow the terminal to be resized to any dimension, not constrained to fit to an integer multiple of characters");
                add_bool_option("hyperlink", 0, "no-hyperlink", 'H',
                                0, &hyperlink,
                                "Enable hyperlinks",
                                "Disable hyperlinks");
                add_bool_option("kinetic-scrolling", 0, "no-kinetic-scrolling", 0,
                                0, &kinetic_scrolling,
                                "Enable kinetic scrolling",
                                "Disable kinetic scrolling");
                add_bool_option("legacy-osc777", 0, "no-legacy-osc777", 0,
                                0, &legacy_osc777,
                                "Enable legacy OSC 777 sequences",
                                "Disable legacy OSC 777 sequences");
                add_bool_option("pty", 0, "no-pty", 0,
                                0, &pty,
                                "Enable PTY creation with --no-shell",
                                "Disable PTY creation with --no-shell");
                add_bool_option("rewrap", 0, "no-rewrap", 'R',
                                0, &rewrap,
                                "Enable rewrapping on resize",
                                "Disable rewrapping on resize");
                add_bool_option("scrollbar", 0, "no-scrollbar", 0,
                                0, &scrollbar,
                                "Enable scrollbar",
                                "Disable scrollbar");
                add_bool_option("shaping", 0, "no-shaping", 0,
                                0, &shaping,
                                "Enable Arabic shaping",
                                "Disable Arabic shaping");
                add_bool_option("shell", 0, "no-shell", 'S',
                                0, &shell,
                                "Enable spawning a shell inside the terminal",
                                "Disable spawning a shell inside the terminal");
                add_bool_option("systemd-scope", 0, "no-systemd-scope", 0,
                                0, &systemd_scope,
                                "Enable using systemd user scope",
                                "Disble using systemd user scope");
                add_bool_option("overlay-scrollbar", 'N', "no-overlay-scrollbar", 0,
                                0, &overlay_scrollbar,
                                "Use overlay scrollbar",
                                "Use regular scrollbar");
                add_bool_option("reverse", 0, "no-reverse", 0,
                                0, &reverse,
                                "Reverse foreground/background colors",
                                "Don't reverse foreground/background colors");
                add_bool_option("require-systemd-scope", 0, "no-require-systemd-scope", 0,
                                0, &require_systemd_scope,
                                "Require use of a systemd user scope",
                                "Don't require use of a systemd user scope");
                add_bool_option("scroll-on-insert", 0, "no-scroll-on-insert", 0,
                                0, &scroll_on_insert,
                                "Scroll to bottom when text is pasted",
                                "Don't scroll to bottom when text is pasted");
                add_bool_option("scroll-on-keystroke", 0, "no-scroll-on-keystroke", 0,
                                0, &scroll_on_keystroke,
                                "Scroll to bottom when a key is pressed",
                                "Don't scroll to bottom when a key is pressed");
                add_bool_option("scroll-on-output", 0, "no-scroll-on-output", 0,
                                0, &scroll_on_output,
                                "Scroll to bottom when new output is received",
                                "Don't scroll to bottom when new output is received");
                add_bool_option("scroll-unit-is-pixels", 0, "no-scroll-unit-is-pixels", 0,
                                0, &scroll_unit_is_pixels,
                                "Use pixels as scroll unit",
                                "Use lines as scroll unit");
                add_bool_option("track-clipboard-targets", 0, "no-track-clipboard-targets", 0,
                                G_OPTION_FLAG_HIDDEN, &track_clipboard_targets,
                                "Track clipboard targets",
                                "Don't track clipboard targets");
                add_bool_option("whole-window-transparent", 0, "no-whole-window-transparent", 0,
                                0, &whole_window_transparent,
                                "Make the whole window transparent",
                                "Don't make the whole window transparent");
                add_bool_option("scrolled-window", 0, "no-scrolled-window", 0,
                                0, &use_scrolled_window,
                                "Use a GtkScrolledWindow",
                                "Don't use a GtkScrolledWindow");
                add_bool_option("use-theme-colors", 0, "no-use-theme-colors", 0,
                                0, &use_theme_colors,
                                "Use foreground and background colors from the gtk+ theme",
                                "Don't use foreground and background colors from the gtk+ theme");

                add_bool_option("xfill", 0, "no-xfill", 0,
                                0, &xfill,
                                "Fill horizontally",
                                "Don't fill horizontally");
                add_bool_option("yfill", 0, "no-yfill", 0,
                                0, &yfill,
                                "Fill vertically",
                                "Don't fill vertically");
                add_bool_option("object-notifications", 'N', "no-object-notifications", 0,
                                0, &object_notifications,
                                "Print VteTerminal object notifications",
                                "Don't print VteTerminal object notifications");

#if VTE_GTK == 3
                add_bool_option("argb-visual", 0, "no-argb-visual", 0,
                                0, &argb_visual,
                                "Use an ARGB visual",
                                "Don't use an ARGB visual");
                add_bool_option("double-buffer", 0, "no-double-buffer", '2',
                                0, &double_buffer,
                                "Enable double-buffering",
                                "Disable double-buffering");
#endif /* VTE_GTK == 3 */

                entries.push_back({}); // terminate

                char* dummy_string = nullptr;
                GOptionEntry const more_entries[] = {
                        { "background-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_bg_color,
                          "Set default background color", "COLOR" },
                        { "background-image", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_background_image,
                          "Set background image from file", "FILE" },
                        { "background-extend", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_background_extend,
                          "Set background image extend", "EXTEND" },
                        { "blink", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_text_blink,
                          "Text blink mode (never|focused|unfocused|always)", "MODE" },
                        { "cell-height-scale", 0, 0, G_OPTION_ARG_DOUBLE, &cell_height_scale,
                          "Add extra line spacing", "1.0..2.0" },
                        { "cell-width-scale", 0, 0, G_OPTION_ARG_DOUBLE, &cell_width_scale,
                          "Add extra letter spacing", "1.0..2.0" },
                        { "cjk-width", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cjk_width,
                          "Specify the cjk ambiguous width to use for UTF-8 encoding", "NARROW|WIDE" },
                        { "cursor-blink", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cursor_blink,
                          "Cursor blink mode (system|on|off)", "MODE" },
                        { "cursor-background-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cursor_bg_color,
                          "Enable a colored cursor background", "COLOR" },
                        { "cursor-foreground-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cursor_fg_color,
                          "Enable a colored cursor foreground", "COLOR" },
                        { "cursor-shape", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_cursor_shape,
                          "Set cursor shape (block|underline|ibeam)", nullptr },
                        { "css-file", 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, (void*)parse_css_file,
                          "Load CSS from FILE", "FILE" },
                        { "dingu", 'D', 0, G_OPTION_ARG_STRING_ARRAY, &dingus,
                          "Add regex highlight", nullptr },
                        { "encoding", 0, 0, G_OPTION_ARG_STRING, &encoding,
                          "Specify the terminal encoding to use", "ENCODING" },
                        { "env", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &environment,
                          "Add environment variable to the child\'s environment", "VAR=VALUE" },
                        { "extra-margin", 0, 0, G_OPTION_ARG_INT, &extra_margin,
                          "Add extra margin around the terminal widget", "MARGIN" },
                        { "fd", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_fd,
                          "Pass file descriptor N (as M) to the child process", "N[:M]|N[=M]" },
                        { "feed-stdin", 'B', 0, G_OPTION_ARG_NONE, &feed_stdin,
                          "Feed input to the terminal", nullptr },
                        { "font", 'f', 0, G_OPTION_ARG_STRING, &font_string,
                          "Specify a font to use", nullptr },
                        { "foreground-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_fg_color,
                          "Set default foreground color", "COLOR" },
                        { "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
                          "Set the size (in characters) and position", "GEOMETRY" },
                        { "highlight-background-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_hl_bg_color,
                          "Enable distinct highlight background color for selection", "COLOR" },
                        { "highlight-foreground-color", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_hl_fg_color,
                          "Enable distinct highlight foreground color for selection", "COLOR" },
                        { "icon-title", 'i', 0, G_OPTION_ARG_NONE, &icon_title,
                          "Enable the setting of the icon title", nullptr },
                        { "output-file", 0, 0, G_OPTION_ARG_FILENAME, &output_filename,
                          "Save terminal contents to file at exit", nullptr },
                        { "scrollback-lines", 'n', 0, G_OPTION_ARG_INT, &scrollback_lines,
                          "Specify the number of scrollback-lines (-1 for infinite)", nullptr },
                        { "title", 0, 0, G_OPTION_ARG_STRING, &title, "Set the initial title of the window", "TITLE" },
                        { "transparent", 'T', 0, G_OPTION_ARG_INT, &transparency_percent,
                          "Enable the use of a transparent background", "0..100" },
                        { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
                          (void*)parse_verbosity,
                          "Enable verbose debug output", nullptr },
                        { "version", 0, 0, G_OPTION_ARG_NONE, &version,
                          "Show version", nullptr },
                        { "word-char-exceptions", 0, 0, G_OPTION_ARG_STRING, &word_char_exceptions,
                          "Specify the word char exceptions", "CHARS" },
                        { "working-directory", 'w', 0, G_OPTION_ARG_FILENAME, &working_directory,
                          "Specify the initial working directory of the terminal", nullptr },
                        { "xalign", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_xalign,
                          "Horizontal alignment (start|end|center)", "ALIGN" },
                        { "yalign", 0, 0, G_OPTION_ARG_CALLBACK, (void*)parse_yalign,
                          "Vertical alignment (fill|start|end|center)", "ALIGN" },

                        { "test-mode", 0, 0, G_OPTION_ARG_NONE, &test_mode,
                          "Enable test mode", nullptr },

                        /* Options for compatibility with the old vteapp test application */
                        { "border-width", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &extra_margin,
                          nullptr, nullptr },
                        { "command", 'c', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_FILENAME, &command,
                          nullptr, nullptr },
                        { "console", 'C', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &console,
                          nullptr, nullptr },
                        { "pty-flags", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &dummy_string,
                          nullptr, nullptr },
                        { "scrollbar-policy", 'P', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,
                          &dummy_string, nullptr, nullptr },

                        { nullptr }
                };

                gboolean no_load_config = false;
                char* load_config_path = nullptr;
                char* save_config_path = nullptr;
                GOptionEntry const config_entries[] = {
                        { "load-config",
                          0,
                          0,
                          G_OPTION_ARG_FILENAME,
                          &load_config_path,
                          "Load configuration from file",
                          "FILE" },
                        { "save-config",
                          0,
                          0,
                          G_OPTION_ARG_FILENAME,
                          &save_config_path,
                          "Save configuration to file",
                          "FILE" },
                        { "no-load-config",
                          0,
                          0,
                          G_OPTION_ARG_NONE,
                          &no_load_config,
                          "Don't load default configuration",
                          nullptr },
                        { nullptr },
                };

                /* Look for '--' */
                for (int i = 0; i < argc; i++) {
                        if (!g_str_equal(argv[i], "--"))
                                continue;

                        i++; /* skip it */
                        if (i == argc) {
                                g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                                            "No command specified after -- terminator");
                                return false;
                        }

                        exec_argv = g_new(char*, argc - i + 1);
                        int j;
                        for (j = 0; i < argc; i++, j++)
                                exec_argv[j] = g_strdup(argv[i]);
                        exec_argv[j] = nullptr;

                        argc = i;
                        break;
                }

                auto config_ini = vte::take_freeable(g_key_file_new());

                do {
                        // First, parse the --no-load-config and --load-config
                        // options
                        auto context = vte::take_freeable(g_option_context_new(nullptr));
                        g_option_context_set_help_enabled(context.get(), false);
                        g_option_context_set_ignore_unknown_options(context.get(), true);
                        g_option_context_set_translation_domain(context.get(), GETTEXT_PACKAGE);

                        auto err = vte::glib::Error{};
                        auto group = g_option_group_new(nullptr, nullptr, nullptr, this, nullptr);
                        g_option_group_set_translation_domain(group, GETTEXT_PACKAGE);
                        g_option_group_add_entries(group, config_entries);
                        g_option_context_set_main_group(context.get(), group);

                        // This will remove parsed options from @argv and
                        // leave unrecognised options to be parsed again below.
                        if (!g_option_context_parse(context.get(), &argc, &argv, err))
                                break;

                        if (!no_load_config) {
                                // Allow default load to fail
                                if (!load_config(config_ini.get(), nullptr, err) &&
                                    !err.matches(G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
                                        verbose_printerr("Failed to load default configuration: %s\n", err.message());
                                }
                        }

                        if (load_config_path && !load_config(config_ini.get(), load_config_path, error)) {
                                    g_free(load_config_path);
                                    g_free(save_config_path);
                                    break; // don't bail out
                        }
                } while (false);

                {
                        // Now parse all options. (We add the above-handled options too,
                        // so that --help can output them; but since they are already
                        // removed from argv they won't be processed again.
                        auto context = vte::take_freeable
                                (g_option_context_new("[-- COMMAND …] — VTE test application"));
                        g_option_context_set_help_enabled(context.get(), true);
                        g_option_context_set_translation_domain(context.get(), GETTEXT_PACKAGE);

                        auto group = g_option_group_new(nullptr, nullptr, nullptr, this, nullptr);
                        g_option_group_set_translation_domain(group, GETTEXT_PACKAGE);
                        g_option_group_add_entries(group, entries.data());
                        g_option_group_add_entries(group, more_entries);
                        g_option_context_set_main_group(context.get(), group);

#if VTE_GTK == 3
                        g_option_context_add_group(context.get(), gtk_get_option_group(true));
#endif

                        if (!g_option_context_parse(context.get(), &argc, &argv, error))
                                return false;
                }

                g_free(dummy_string);

                // Now save the combined config, if requested
                if (save_config_path &&
                    !save_config(config_ini.get(), save_config_path, error)) {
                    return false;
                }

                if (reverse) {
                        using std::swap;
                        swap(fg_color, bg_color);
                }

                if (use_scrolled_window) {
                        geometry_hints = false;
                }

#if VTE_GTK == 4
                if (!gtk_init_check()) {
                        g_set_error_literal(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                                            "Failed to initialise gtk+");
                        return false;
                }
#endif /* VTE_GTK == 4 */

                return true;
        }
}; // class Options

Options options{}; /* global */

/* debug output */

static void G_GNUC_PRINTF(2, 3)
verbose_fprintf(FILE* fp,
                char const* format,
                ...)
{
        if (options.verbosity == 0)
                return;

        va_list args;
        va_start(args, format);
        g_vfprintf(fp, format, args);
        va_end(args);
}

/* regex */

static void
jit_regex(VteRegex* regex,
          char const* pattern)
{
        auto error = vte::glib::Error{};
        if (!vte_regex_jit(regex, PCRE2_JIT_COMPLETE, error) ||
            !vte_regex_jit(regex, PCRE2_JIT_PARTIAL_SOFT, error)) {
                if (!error.matches(VTE_REGEX_ERROR, -45 /* PCRE2_ERROR_JIT_BADOPTION: JIT not supported */))
                        verbose_printerr("JITing regex \"%s\" failed: %s\n", pattern, error.message());
        }
}

static vte::Freeable<VteRegex>
compile_regex_for_search(char const* pattern,
                         bool caseless,
                         GError** error)
{
        uint32_t flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_MULTILINE;
        if (caseless)
                flags |= PCRE2_CASELESS;

        auto regex = vte_regex_new_for_search(pattern, strlen(pattern), flags, error);
        if (regex != nullptr)
                jit_regex(regex, pattern);

        return vte::take_freeable(regex);
}

static vte::Freeable<VteRegex>
compile_regex_for_match(char const* pattern,
                        bool caseless,
                        GError** error)
{
        uint32_t flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_MULTILINE;
        if (caseless)
                flags |= PCRE2_CASELESS;

        auto regex = vte_regex_new_for_match(pattern, strlen(pattern), flags, error);
        if (regex != nullptr)
                jit_regex(regex, pattern);

        return vte::take_freeable(regex);
}

/* search popover */

#define VTEAPP_TYPE_SEARCH_POPOVER         (vteapp_search_popover_get_type())
#define VTEAPP_SEARCH_POPOVER(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), VTEAPP_TYPE_SEARCH_POPOVER, VteappSearchPopover))
#define VTEAPP_SEARCH_POPOVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VTEAPP_TYPE_SEARCH_POPOVER, VteappSearchPopoverClass))
#define VTEAPP_IS_SEARCH_POPOVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), VTEAPP_TYPE_SEARCH_POPOVER))
#define VTEAPP_IS_SEARCH_POPOVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), VTEAPP_TYPE_SEARCH_POPOVER))
#define VTEAPP_SEARCH_POPOVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), VTEAPP_TYPE_SEARCH_POPOVER, VteappSearchPopoverClass))

typedef struct _VteappSearchPopover        VteappSearchPopover;
typedef struct _VteappSearchPopoverClass   VteappSearchPopoverClass;

struct _VteappSearchPopover {
        GtkPopover parent;

        /* from GtkWidget template */
        GtkWidget* search_entry;
        GtkWidget* search_prev_button;
        GtkWidget* search_next_button;
        GtkWidget* close_button;
        GtkToggleButton* match_case_checkbutton;
        GtkToggleButton* entire_word_checkbutton;
        GtkToggleButton* regex_checkbutton;
        GtkToggleButton* wrap_around_checkbutton;
        GtkWidget* reveal_button;
        GtkWidget* revealer;
        /* end */

        VteTerminal *terminal;
        bool regex_caseless;
        bool has_regex;
        char* regex_pattern;
};

struct _VteappSearchPopoverClass {
        GtkPopoverClass parent;
};

static GType vteapp_search_popover_get_type(void);

enum {
        SEARCH_POPOVER_PROP0,
        SEARCH_POPOVER_PROP_TERMINAL,
        SEARCH_POPOVER_N_PROPS
};

static GParamSpec* search_popover_pspecs[SEARCH_POPOVER_N_PROPS];

static void
vteapp_search_popover_update_sensitivity(VteappSearchPopover* popover)
{
        bool can_search = popover->has_regex;

        gtk_widget_set_sensitive(popover->search_next_button, can_search);
        gtk_widget_set_sensitive(popover->search_prev_button, can_search);
}

static void
vteapp_search_popover_update_regex(VteappSearchPopover* popover)
{
#if VTE_GTK == 3
        auto search_text = gtk_entry_get_text(GTK_ENTRY(popover->search_entry));
#elif VTE_GTK == 4
        auto search_text = gtk_editable_get_text(GTK_EDITABLE(popover->search_entry));
#endif /* VTE_GTK */
        bool caseless = gtk_toggle_button_get_active(popover->match_case_checkbutton) == FALSE;

        char* pattern;
        if (gtk_toggle_button_get_active(popover->regex_checkbutton))
                pattern = g_strdup(search_text);
        else
                pattern = g_regex_escape_string(search_text, -1);

        if (gtk_toggle_button_get_active(popover->regex_checkbutton)) {
                char* tmp = g_strdup_printf("\\b%s\\b", pattern);
                g_free(pattern);
                pattern = tmp;
        }

        if (caseless == popover->regex_caseless &&
            g_strcmp0(pattern, popover->regex_pattern) == 0)
                return;

        popover->regex_caseless = caseless;
        g_free(popover->regex_pattern);
        popover->regex_pattern = nullptr;

        if (search_text[0] != '\0') {
                auto error = vte::glib::Error{};
                auto regex = compile_regex_for_search(pattern, caseless, error);
                vte_terminal_search_set_regex(popover->terminal, regex.get(), 0);

                if (error.error()) {
                        popover->has_regex = false;
                        popover->regex_pattern = pattern; /* adopt */
                        pattern = nullptr; /* adopted */
                        gtk_widget_set_tooltip_text(popover->search_entry, nullptr);
                } else {
                        popover->has_regex = true;
                        gtk_widget_set_tooltip_text(popover->search_entry, error.message());
                }
        }

        g_free(pattern);

        vteapp_search_popover_update_sensitivity(popover);
}

static void
vteapp_search_popover_wrap_around_toggled(GtkToggleButton* button,
                                          VteappSearchPopover* popover)
{
        vte_terminal_search_set_wrap_around(popover->terminal, gtk_toggle_button_get_active(button));
}

static void
vteapp_search_popover_search_forward(VteappSearchPopover* popover)
{
        if (!popover->has_regex)
                return;
        vte_terminal_search_find_next(popover->terminal);
}

static void
vteapp_search_popover_search_backward(VteappSearchPopover* popover)
{
        if (!popover->has_regex)
                return;
        vte_terminal_search_find_previous(popover->terminal);
}

G_DEFINE_TYPE(VteappSearchPopover, vteapp_search_popover, GTK_TYPE_POPOVER)

static void
vteapp_search_popover_init(VteappSearchPopover* popover)
{
        gtk_widget_init_template(GTK_WIDGET(popover));

        popover->regex_caseless = false;
        popover->has_regex = false;
        popover->regex_pattern = nullptr;

        g_signal_connect_swapped(popover->close_button, "clicked", G_CALLBACK(gtk_widget_hide), popover);

        g_object_bind_property(popover->reveal_button, "active",
                               popover->revealer, "reveal-child",
                               GBindingFlags(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE));

        g_signal_connect_swapped(popover->search_entry, "next-match", G_CALLBACK(vteapp_search_popover_search_forward), popover);
        g_signal_connect_swapped(popover->search_entry, "previous-match", G_CALLBACK(vteapp_search_popover_search_backward), popover);
        g_signal_connect_swapped(popover->search_entry, "search-changed", G_CALLBACK(vteapp_search_popover_update_regex), popover);

        g_signal_connect_swapped(popover->search_next_button,"clicked", G_CALLBACK(vteapp_search_popover_search_forward), popover);
        g_signal_connect_swapped(popover->search_prev_button,"clicked", G_CALLBACK(vteapp_search_popover_search_backward), popover);

        g_signal_connect_swapped(popover->match_case_checkbutton,"toggled", G_CALLBACK(vteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->entire_word_checkbutton,"toggled", G_CALLBACK(vteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->regex_checkbutton,"toggled", G_CALLBACK(vteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->match_case_checkbutton, "toggled", G_CALLBACK(vteapp_search_popover_update_regex), popover);

        g_signal_connect(popover->wrap_around_checkbutton, "toggled", G_CALLBACK(vteapp_search_popover_wrap_around_toggled), popover);

        vteapp_search_popover_update_sensitivity(popover);
}

static void
vteapp_search_popover_finalize(GObject *object)
{
        VteappSearchPopover* popover = VTEAPP_SEARCH_POPOVER(object);

        g_free(popover->regex_pattern);

        G_OBJECT_CLASS(vteapp_search_popover_parent_class)->finalize(object);
}

static void
vteapp_search_popover_set_property(GObject* object,
                                   guint prop_id,
                                   const GValue* value,
                                   GParamSpec* pspec)
{
        VteappSearchPopover* popover = VTEAPP_SEARCH_POPOVER(object);

        switch (prop_id) {
        case SEARCH_POPOVER_PROP_TERMINAL:
                popover->terminal = reinterpret_cast<VteTerminal*>(g_value_get_object(value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
                break;
        }
}

static void
vteapp_search_popover_class_init(VteappSearchPopoverClass* klass)
{
        GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->finalize = vteapp_search_popover_finalize;
        gobject_class->set_property = vteapp_search_popover_set_property;

        search_popover_pspecs[SEARCH_POPOVER_PROP_TERMINAL] =
                g_param_spec_object("terminal", nullptr, nullptr,
                                    VTE_TYPE_TERMINAL,
                                    GParamFlags(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_properties(gobject_class, G_N_ELEMENTS(search_popover_pspecs), search_popover_pspecs);

        GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
        gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/vte/app/ui/search-popover.ui");
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, search_entry);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, search_prev_button);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, search_next_button);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, reveal_button);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, close_button);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, revealer);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, match_case_checkbutton);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, entire_word_checkbutton);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, regex_checkbutton);
        gtk_widget_class_bind_template_child(widget_class, VteappSearchPopover, wrap_around_checkbutton);
}

static GtkWidget*
vteapp_search_popover_new(VteTerminal* terminal,
                          GtkWidget* relative_to)
{
        auto popover = reinterpret_cast<GtkWidget*>(g_object_new(VTEAPP_TYPE_SEARCH_POPOVER,
                                                                 "terminal", terminal,
#if VTE_GTK == 3
                                                                 "relative-to", relative_to,
#endif
                                                                 nullptr));

#if VTE_GTK == 4
        gtk_widget_set_parent(popover, relative_to);
#endif

        return popover;
}

/* terminal */

#define VTEAPP_TYPE_TERMINAL         (vteapp_terminal_get_type())
#define VTEAPP_TERMINAL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), VTEAPP_TYPE_TERMINAL, VteappTerminal))
#define VTEAPP_TERMINAL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VTEAPP_TYPE_TERMINAL, VteappTerminalClass))
#define VTEAPP_IS_TERMINAL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), VTEAPP_TYPE_TERMINAL))
#define VTEAPP_IS_TERMINAL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), VTEAPP_TYPE_TERMINAL))
#define VTEAPP_TERMINAL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), VTEAPP_TYPE_TERMINAL, VteappTerminalClass))

typedef struct _VteappTerminal       VteappTerminal;
typedef struct _VteappTerminalClass  VteappTerminalClass;

struct _VteappTerminal {
        VteTerminal parent;

        cairo_pattern_t* background_pattern;
        bool has_backdrop;
        bool use_backdrop;
};

struct _VteappTerminalClass {
        VteTerminalClass parent;
};

static GType vteapp_terminal_get_type(void);

G_DEFINE_TYPE(VteappTerminal, vteapp_terminal, VTE_TYPE_TERMINAL)

#define BACKDROP_ALPHA (0.2)

static void
vteapp_terminal_realize(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->realize(widget);

        if (!options.background_pixbuf)
                return;

        auto terminal = VTEAPP_TERMINAL(widget);

#if VTE_GTK == 3
        auto surface = vte::take_freeable
                (gdk_cairo_surface_create_from_pixbuf(options.background_pixbuf,
                                                      0 /* take scale from window */,
                                                      gtk_widget_get_window(widget)));
#elif VTE_GTK == 4
        auto const width = gdk_pixbuf_get_width(options.background_pixbuf);
        auto const height = gdk_pixbuf_get_height(options.background_pixbuf);
        auto surface = vte::take_freeable(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                                     width, height));
        auto cr = vte::take_freeable(cairo_create(surface.get()));
        gdk_cairo_set_source_pixbuf(cr.get(), options.background_pixbuf, 0, 0);
        cairo_paint(cr.get());
        cairo_surface_flush(surface.get()); // FIXME necessary?
#endif
        terminal->background_pattern = cairo_pattern_create_for_surface(surface.get());

        cairo_pattern_set_extend(terminal->background_pattern, options.background_extend);
}

static void
vteapp_terminal_unrealize(GtkWidget* widget)
{
#if VTE_GTK == 3
        auto terminal = VTEAPP_TERMINAL(widget);

        if (terminal->background_pattern != nullptr) {
                cairo_pattern_destroy(terminal->background_pattern);
                terminal->background_pattern = nullptr;
        }
#endif /* VTE_GTK */

        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->unrealize(widget);
}

#if VTE_GTK == 3
static void
vteapp_terminal_draw_background(GtkWidget* widget,
                                cairo_t* cr)
{
        auto terminal = VTEAPP_TERMINAL(widget);

        if (terminal->background_pattern != nullptr) {
                cairo_push_group(cr);

                /* Draw background colour */
                cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
                cairo_rectangle(cr, 0.0, 0.0,
                                gtk_widget_get_allocated_width(widget),
                                gtk_widget_get_allocated_height(widget));
                GdkRGBA bg;
                vte_terminal_get_color_background_for_draw(VTE_TERMINAL(terminal), &bg);
                cairo_set_source_rgba(cr, bg.red, bg.green, bg.blue, 1.0);
                cairo_paint(cr);

                /* Draw background image */
                cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
                cairo_set_source(cr, terminal->background_pattern);
                cairo_paint(cr);

                cairo_pop_group_to_source(cr);
                cairo_paint_with_alpha(cr, options.get_alpha_bg_for_draw());

        }
}
#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4
static void
vteapp_terminal_draw_background(GtkWidget* widget,
                                GtkSnapshot* snapshot)
{
}
#endif /* VTE_GTK  == 4 */

#if VTE_GTK == 3
static void
vteapp_terminal_draw_backdrop(GtkWidget* widget,
                              cairo_t* cr)
{
        auto terminal = VTEAPP_TERMINAL(widget);

        if (terminal->use_backdrop && terminal->has_backdrop) {
                cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
                cairo_set_source_rgba(cr, 0, 0, 0, BACKDROP_ALPHA);
                cairo_rectangle(cr, 0.0, 0.0,
                                gtk_widget_get_allocated_width(widget),
                                gtk_widget_get_allocated_height(widget));
                cairo_paint(cr);
        }
}
#elif VTE_GTK == 4
static void
vteapp_terminal_draw_backdrop(GtkWidget* widget,
                              GtkSnapshot* snapshot)
{
        static const GdkRGBA rgba = {0, 0, 0, BACKDROP_ALPHA};
        auto terminal = VTEAPP_TERMINAL(widget);

        if (terminal->use_backdrop && terminal->has_backdrop) {
                auto const rect = GRAPHENE_RECT_INIT(.0f,
                                                     .0f,
                                                     float(gtk_widget_get_allocated_width(widget)),
                                                     float(gtk_widget_get_allocated_height(widget)));
                gtk_snapshot_append_color(snapshot, &rgba, &rect);
        }
}
#endif

#if VTE_GTK == 3

static gboolean
vteapp_terminal_draw(GtkWidget* widget,
                     cairo_t* cr)
{
        vteapp_terminal_draw_background(widget, cr);

        auto const rv = GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->draw(widget, cr);

        vteapp_terminal_draw_backdrop(widget, cr);

        return rv;
}

#endif /* VTE_GTK == 3 */

static void
vteapp_terminal_update_theme_colors(GtkWidget* widget)
{
        if (!options.use_theme_colors)
                return;

        auto terminal = VTEAPP_TERMINAL(widget);
        auto context = gtk_widget_get_style_context(widget);

#if VTE_GTK == 3
        auto const flags = gtk_style_context_get_state(context);
#endif

        auto theme_fg = GdkRGBA{};
        gtk_style_context_get_color(context,
#if VTE_GTK == 3
                                    flags,
#endif
                                    &theme_fg);

        auto theme_bg = GdkRGBA{};
#if VTE_GTK == 3
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        gtk_style_context_get_background_color(context, flags, &theme_bg);
        G_GNUC_END_IGNORE_DEPRECATIONS;
#elif VTE_GTK == 4
        // FIXMEgtk4 "background-color" lookup always fails
        if (!gtk_style_context_lookup_color(context, "text_view_bg", &theme_bg)) {
                verbose_print("Failed to get theme background color\n");
                return;
        }
#endif

        auto dti = [](double d) -> unsigned { return std::clamp(unsigned(d*255), 0u, 255u); };

        verbose_print("Theme colors: foreground is #%02X%02X%02X, background is #%02X%02X%02X\n",
                      dti(theme_fg.red), dti(theme_fg.green), dti(theme_fg.blue),
                      dti(theme_bg.red), dti(theme_bg.green), dti(theme_bg.blue));

        theme_fg.alpha = 1.;
        theme_bg.alpha = options.get_alpha_bg();
        vte_terminal_set_colors(VTE_TERMINAL(terminal), &theme_fg, &theme_bg, nullptr, 0);
}

#if VTE_GTK == 3

static void
vteapp_terminal_style_updated(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->style_updated(widget);

        auto terminal = VTEAPP_TERMINAL(widget);

        auto context = gtk_widget_get_style_context(widget);
        auto const flags = gtk_style_context_get_state(context);
        terminal->has_backdrop = (flags & GTK_STATE_FLAG_BACKDROP) != 0;

        vteapp_terminal_update_theme_colors(widget);
}

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4

static void
vteapp_terminal_snapshot(GtkWidget* widget,
                         GtkSnapshot* snapshot_object)
{
        vteapp_terminal_draw_background(widget, snapshot_object);

        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->snapshot(widget, snapshot_object);

        vteapp_terminal_draw_backdrop(widget, snapshot_object);
}

static void
vteapp_terminal_css_changed(GtkWidget* widget,
                            GtkCssStyleChange* change)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->css_changed(widget, change);

        vteapp_terminal_update_theme_colors(widget);
}

static void
vteapp_terminal_state_flags_changed(GtkWidget* widget,
                                    GtkStateFlags old_flags)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->state_flags_changed(widget, old_flags);

        auto terminal = VTEAPP_TERMINAL(widget);
        auto const flags = gtk_widget_get_state_flags(widget);
        terminal->has_backdrop = (flags & GTK_STATE_FLAG_BACKDROP) != 0;
}

static void
vteapp_terminal_system_setting_changed(GtkWidget* widget,
                                       GtkSystemSetting setting)
{
        GTK_WIDGET_CLASS(vteapp_terminal_parent_class)->system_setting_changed(widget, setting);

        // FIXMEgtk4 find a way to update colours on theme change like gtk3 above
}

#endif /* VTE_GTK == 4 */

static void
vteapp_terminal_class_init(VteappTerminalClass *klass)
{
        auto widget_class = GTK_WIDGET_CLASS(klass);
        widget_class->realize = vteapp_terminal_realize;
        widget_class->unrealize = vteapp_terminal_unrealize;

#if VTE_GTK == 3
        widget_class->draw = vteapp_terminal_draw;
        widget_class->style_updated = vteapp_terminal_style_updated;
#elif VTE_GTK == 4
        widget_class->snapshot = vteapp_terminal_snapshot;
        widget_class->css_changed = vteapp_terminal_css_changed;
        widget_class->state_flags_changed = vteapp_terminal_state_flags_changed;
        widget_class->system_setting_changed = vteapp_terminal_system_setting_changed;
#endif

        // Test termprops
        if (options.test_mode) {
                vte_install_termprop("vte.ext.vteapp.test.valueless",
                                     VTE_PROPERTY_VALUELESS,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.bool",
                                     VTE_PROPERTY_BOOL,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.int",
                                     VTE_PROPERTY_INT,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.uint",
                                     VTE_PROPERTY_UINT,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.double",
                                     VTE_PROPERTY_DOUBLE,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.rgb",
                                     VTE_PROPERTY_RGB,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.rgba",
                                     VTE_PROPERTY_RGBA,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.string",
                                     VTE_PROPERTY_STRING,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.data",
                                     VTE_PROPERTY_DATA,
                                     VTE_PROPERTY_FLAG_NONE);
                vte_install_termprop("vte.ext.vteapp.test.uuid",
                                     VTE_PROPERTY_UUID,
                                     VTE_PROPERTY_FLAG_NONE);

                vte_install_termprop_alias("vte.ext.vteapp.test.alias",
                                           "vte.ext.vteapp.test.bool");
        }

        { // BEGIN distro patches adding termprops

        } // END distro patches adding termprops

        if (options.verbosity > 1) {
                auto n_termprops = gsize{0};
                auto termprops = vte::glib::take_free_ptr(vte_get_termprops(&n_termprops));
                verbose_print("Installed termprops are:\n");
                for (auto i = gsize{0}; i < n_termprops; ++i) {
                        verbose_print("  %s\n", termprops.get()[i]);
                }
        }
}

static void
vteapp_terminal_init(VteappTerminal *terminal)
{
        terminal->background_pattern = nullptr;
        terminal->has_backdrop = false;
        terminal->use_backdrop = options.backdrop;

        vte_terminal_set_suppress_legacy_signals(VTE_TERMINAL(terminal));

#if VTE_GTK == 3
        if (options.background_pixbuf != nullptr)
                vte_terminal_set_clear_background(VTE_TERMINAL(terminal), false);
#endif /* VTE_GTK == 3 */
}

static GtkWidget *
vteapp_terminal_new(void)
{
        return reinterpret_cast<GtkWidget*>(g_object_new(VTEAPP_TYPE_TERMINAL, nullptr));
}

/* terminal window */

#define VTEAPP_TYPE_WINDOW         (vteapp_window_get_type())
#define VTEAPP_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), VTEAPP_TYPE_WINDOW, VteappWindow))
#define VTEAPP_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VTEAPP_TYPE_WINDOW, VteappWindowClass))
#define VTEAPP_IS_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), VTEAPP_TYPE_WINDOW))
#define VTEAPP_IS_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), VTEAPP_TYPE_WINDOW))
#define VTEAPP_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), VTEAPP_TYPE_WINDOW, VteappWindowClass))

typedef struct _VteappWindow       VteappWindow;
typedef struct _VteappWindowClass  VteappWindowClass;

struct _VteappWindow {
        GtkApplicationWindow parent;

        /* from GtkWidget template */
        GtkWidget* window_grid;
        GtkScrollbar* scrollbar;
        /* GtkGrid* notifications_grid; */
        GtkWidget* readonly_emblem;
        /* GtkButton* copy_button; */
        /* GtkButton* paste_button; */
        GtkToggleButton* find_button;
        GtkMenuButton* gear_button;
        /* end */

        VteTerminal* terminal;
        GPid child_pid;
        GtkWidget* search_popover;

        /* used for updating the geometry hints */
        int cached_cell_width{0};
        int cached_cell_height{0};
        int cached_chrome_width{0};
        int cached_chrome_height{0};
        int cached_csd_width{0};
        int cached_csd_height{0};

#if VTE_GTK == 3
        GtkClipboard* clipboard;
        GdkWindowState window_state{GdkWindowState(0)};
#endif
#if VTE_GTK == 4
        GdkClipboard* clipboard;
        GdkToplevelState toplevel_state{GdkToplevelState(0)};
#endif
};

struct _VteappWindowClass {
        GtkApplicationWindowClass parent;
};

static GType vteapp_window_get_type(void);

static char const* const builtin_dingus[] = {
        "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?",
        "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
        nullptr,
};

/* Just some arbitrary minimum values */
#define MIN_COLUMNS (16)
#define MIN_ROWS    (2)

static void
vteapp_window_add_dingus(VteappWindow* window,
                         char const* const* dingus)
{
        for (auto i = 0; dingus[i] != nullptr; i++) {
                auto tag = -1;
                auto error = vte::glib::Error{};
                auto regex = compile_regex_for_match(dingus[i], true, error);
                if (regex) {
                        tag = vte_terminal_match_add_regex(window->terminal, regex.get(), 0);
                }

                if (error.error()) {
                        verbose_printerr("Failed to compile regex \"%s\": %s\n",
                                         dingus[i], error.message());
                }

                if (tag != -1)
                        vte_terminal_match_set_cursor_name(window->terminal, tag, "pointer");
        }
}

static void
vteapp_window_update_geometry(VteappWindow* window)
{
        if (!options.geometry_hints)
                return;

#if VTE_GTK == 3
        GtkWidget* window_widget = GTK_WIDGET(window);
        GtkWidget* terminal_widget = GTK_WIDGET(window->terminal);

        int columns = vte_terminal_get_column_count(window->terminal);
        int rows = vte_terminal_get_row_count(window->terminal);
        int cell_width = vte_terminal_get_char_width(window->terminal);
        int cell_height = vte_terminal_get_char_height(window->terminal);

        /* Calculate the chrome size as difference between the content's
         * natural size requisition and the terminal grid's size.
         * This includes the terminal's padding in the chrome.
         */
        GtkRequisition contents_req;
        gtk_widget_get_preferred_size(window->window_grid, nullptr, &contents_req);
        int chrome_width = contents_req.width - cell_width * columns;
        int chrome_height = contents_req.height - cell_height * rows;
        g_assert_cmpint(chrome_width, >=, 0);
        g_assert_cmpint(chrome_height, >=, 0);

        int csd_width = 0;
        int csd_height = 0;
        if (gtk_widget_get_realized(terminal_widget)) {
                /* Calculate the CSD size as difference between the toplevel's
                 * and content's allocation.
                 */
                GtkAllocation toplevel, contents;
                gtk_widget_get_allocation(window_widget, &toplevel);
                gtk_widget_get_allocation(window->window_grid, &contents);

                csd_width = toplevel.width - contents.width;
                csd_height = toplevel.height - contents.height;
                g_assert_cmpint(csd_width, >=, 0);
                g_assert_cmpint(csd_height, >=, 0);

                /* Only actually set the geometry hints once the window is realized,
                 * since only then we know the CSD size. Only set the geometry when
                 * anything has changed.
                 */
                if (options.geometry_hints &&
                    (cell_height != window->cached_cell_height ||
                     cell_width != window->cached_cell_width ||
                     chrome_width != window->cached_chrome_width ||
                     chrome_height != window->cached_chrome_height ||
                     csd_width != window->cached_csd_width ||
                     csd_width != window->cached_csd_height)) {
                        GdkGeometry geometry;

                        geometry.base_width = csd_width + chrome_width;
                        geometry.base_height = csd_height + chrome_height;
                        geometry.width_inc = cell_width;
                        geometry.height_inc = cell_height;
                        geometry.min_width = geometry.base_width + cell_width * MIN_COLUMNS;
                        geometry.min_height = geometry.base_height + cell_height * MIN_ROWS;

                        gtk_window_set_geometry_hints(GTK_WINDOW(window),
                                                      nullptr,
                                                      &geometry,
                                                      GdkWindowHints(GDK_HINT_RESIZE_INC |
                                                                     GDK_HINT_MIN_SIZE |
                                                                     GDK_HINT_BASE_SIZE));

                        verbose_print("Updating geometry hints base %dx%d inc %dx%d min %dx%d\n",
                                      geometry.base_width, geometry.base_height,
                                      geometry.width_inc, geometry.height_inc,
                                      geometry.min_width, geometry.min_height);
                }
        }

        window->cached_csd_width = csd_width;
        window->cached_csd_height = csd_height;
        window->cached_cell_width = cell_width;
        window->cached_cell_height = cell_height;
        window->cached_chrome_width = chrome_width;
        window->cached_chrome_height = chrome_height;

        verbose_print("Cached grid %dx%d cell-size %dx%d chrome %dx%d csd %dx%d\n",
                      columns, rows,
                      window->cached_cell_width, window->cached_cell_height,
                      window->cached_chrome_width, window->cached_chrome_height,
                      window->cached_csd_width, window->cached_csd_height);
#elif VTE_GTK == 4
        // FIXMEgtk4 there appears to be no way to do this with gtk4 ? maybe go to X/wayland
        // directly to set the geometry hints?
#endif
}

#include <gdk/gdk.h>

static void
vteapp_window_resize(VteappWindow* window)
{
        /* Can't do this when not using geometry hints */
        if (!options.geometry_hints)
                return;

        /* Don't do this for fullscreened, maximised, or tiled windows. */
#if VTE_GTK == 3
        if (window->window_state & (GDK_WINDOW_STATE_MAXIMIZED |
                                    GDK_WINDOW_STATE_FULLSCREEN |
                                    GDK_WINDOW_STATE_TILED |
#if GTK_CHECK_VERSION(3,22,23)
                                    GDK_WINDOW_STATE_TOP_TILED |
                                    GDK_WINDOW_STATE_BOTTOM_TILED |
                                    GDK_WINDOW_STATE_LEFT_TILED |
                                    GDK_WINDOW_STATE_RIGHT_TILED |
#endif
                                    0))
                return;
#elif VTE_GTK == 4
        if (window->toplevel_state & (GDK_TOPLEVEL_STATE_MAXIMIZED |
                                      GDK_TOPLEVEL_STATE_FULLSCREEN |
                                      GDK_TOPLEVEL_STATE_TILED |
                                      GDK_TOPLEVEL_STATE_TOP_TILED |
                                      GDK_TOPLEVEL_STATE_BOTTOM_TILED |
                                      GDK_TOPLEVEL_STATE_LEFT_TILED |
                                      GDK_TOPLEVEL_STATE_RIGHT_TILED))
                return;
#endif /* VTE_GTK */

#if VTE_GTK == 3
        // FIXMEgtk4

        /* First, update the geometry hints, so that the cached_* members are up-to-date */
        vteapp_window_update_geometry(window);

        /* Calculate the window's pixel size corresponding to the terminal's grid size */
        int columns = vte_terminal_get_column_count(window->terminal);
        int rows = vte_terminal_get_row_count(window->terminal);
        int pixel_width = window->cached_chrome_width + window->cached_cell_width * columns;
        int pixel_height = window->cached_chrome_height + window->cached_cell_height * rows;

        verbose_print("VteappWindow resize grid %dx%d pixel %dx%d\n",
                      columns, rows, pixel_width, pixel_height);

        gtk_window_resize(GTK_WINDOW(window), pixel_width, pixel_height);
#endif /* VTE_GTK == 3 FIXMEgtk4 */
}

static void
vteapp_window_parse_geometry(VteappWindow* window)
{
#if VTE_GTK == 3
        /* First update the geometry hints, so that gtk_window_parse_geometry()
         * knows the char width/height and base size increments.
         */
        vteapp_window_update_geometry(window);

        if (options.geometry != nullptr) {
                auto rv = gtk_window_parse_geometry(GTK_WINDOW(window), options.geometry);

                if (!rv)
                        verbose_printerr("Failed to parse geometry spec \"%s\"\n", options.geometry);
                else if (options.geometry_hints) {
                        /* After parse_geometry(), we can get the default size in
                         * width/height increments, i.e. in grid size.
                         */
                        int columns, rows;
                        gtk_window_get_default_size(GTK_WINDOW(window), &columns, &rows);
                        vte_terminal_set_size(window->terminal, columns, rows);
                        gtk_window_resize_to_geometry(GTK_WINDOW(window), columns, rows);
                } else {
                        /* Approximate the grid width from the passed pixel size. */
                        int width, height;
                        gtk_window_get_default_size(GTK_WINDOW(window), &width, &height);
                        width -= window->cached_csd_width + window->cached_chrome_width;
                        height -= window->cached_csd_height + window->cached_chrome_height;
                        int columns = width / window->cached_cell_width;
                        int rows = height / window->cached_cell_height;
                        vte_terminal_set_size(window->terminal,
                                              MAX(columns, MIN_COLUMNS),
                                              MAX(rows, MIN_ROWS));
                }
        } else {
                /* In GTK+ 3.0, the default size of a window comes from its minimum
                 * size not its natural size, so we need to set the right default size
                 * explicitly */
                if (options.geometry_hints) {
                        /* Grid based */
                        gtk_window_set_default_geometry(GTK_WINDOW(window),
                                                        vte_terminal_get_column_count(window->terminal),
                                                        vte_terminal_get_row_count(window->terminal));
                } else {
                        /* Pixel based */
                        vteapp_window_resize(window);
                }
        }
#elif VTE_GTK == 4
        // FIXMEgtk4 ????
#endif /* VTE_GTK */
}

static void
vteapp_window_adjust_font_size(VteappWindow* window,
                               double factor)
{
        vte_terminal_set_font_scale(window->terminal,
                                    vte_terminal_get_font_scale(window->terminal) * factor);

        vteapp_window_resize(window);
}

static void
window_spawn_cb(VteTerminal* terminal,
                GPid child_pid,
                GError* error,
                void* data)
{
        if (terminal == nullptr) /* terminal destroyed while spawning */
                return;

        VteappWindow* window = VTEAPP_WINDOW(data);
        window->child_pid = child_pid;

        if (child_pid != -1)
                verbose_printerr("Spawning succeded, PID=%ld\n", (long)child_pid);

        if (error != nullptr) {
                verbose_printerr("Spawning failed: %s\n", error->message);

                auto msg = vte::glib::take_string(g_strdup_printf("Spawning failed: %s", error->message));
                if (options.keep)
                        vte_terminal_feed(window->terminal, msg.get(), -1);
                else {
#if VTE_GTK == 3
                        gtk_widget_destroy(GTK_WIDGET(window));
#elif VTE_GTK == 4
                        gtk_window_destroy(GTK_WINDOW(window));
#endif
                }
        }
}

static bool
vteapp_window_launch_argv(VteappWindow* window,
                          char** argv,
                          GError** error)
{
        auto const spawn_flags = GSpawnFlags(G_SPAWN_SEARCH_PATH_FROM_ENVP |
                                             VTE_SPAWN_NO_PARENT_ENVV |
                                             (options.systemd_scope ? 0 : VTE_SPAWN_NO_SYSTEMD_SCOPE) |
                                             (options.require_systemd_scope ? VTE_SPAWN_REQUIRE_SYSTEMD_SCOPE : 0));
        auto fds = options.fds();
        auto map_fds = options.map_fds();
        vte_terminal_spawn_with_fds_async(window->terminal,
                                          VTE_PTY_DEFAULT,
                                          options.working_directory,
                                          argv,
                                          options.environment_for_spawn().get(),
                                          fds.data(), fds.size(),
                                          map_fds.data(), map_fds.size(),
                                          spawn_flags,
                                          nullptr, nullptr, nullptr, /* child setup, data and destroy */
                                          -1 /* default timeout of 30s */,
                                          nullptr /* cancellable */,
                                          window_spawn_cb, window);
        return true;
}

static bool
vteapp_window_launch_commandline(VteappWindow* window,
                                 char* commandline,
                                 GError** error)
{
        int argc;
        char** argv;
        if (!g_shell_parse_argv(commandline, &argc, &argv, error))
            return false;

        bool rv = vteapp_window_launch_argv(window, argv, error);

        g_strfreev(argv);
        return rv;
}

static bool
vteapp_window_launch_shell(VteappWindow* window,
                           GError** error)
{
        char* shell = vte_get_user_shell();
        if (shell == nullptr || shell[0] == '\0') {
                g_free(shell);
                shell = g_strdup(g_getenv("SHELL"));
        }
        if (shell == nullptr || shell[0] == '\0') {
                g_free(shell);
                shell = g_strdup("/bin/sh");
        }

        char* argv[2] = { shell, nullptr };

        bool rv = vteapp_window_launch_argv(window, argv, error);

        g_free(shell);
        return rv;
}

static bool
vteapp_window_fork(VteappWindow* window,
                   GError** error)
{
        auto pty = vte::glib::take_ref(vte_pty_new_sync(VTE_PTY_DEFAULT, nullptr, error));
        if (!pty)
                return false;

        auto pid = fork();
        switch (pid) {
        case -1: { /* error */
                auto errsv = vte::libc::ErrnoSaver{};
                g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                            "Error forking: %s", g_strerror(errsv));
                return false;
        }

        case 0: /* child */ {
                vte_pty_child_setup(pty.get());

                for (auto i = 0; ; i++) {
                        switch (i % 3) {
                        case 0:
                        case 1:
                                g_print("%d\n", i);
                                break;
                        case 2:
                                g_printerr("%d\n", i);
                                break;
                        }
                        g_usleep(G_USEC_PER_SEC);
                }
        }
        default: /* parent */
                vte_terminal_set_pty(window->terminal, pty.get());
                vte_terminal_watch_child(window->terminal, pid);
                verbose_print("Child PID is %d (mine is %d).\n", (int)pid, (int)getpid());
                break;
        }

        return true;
}

static gboolean
tick_cb(VteappWindow* window)
{
        static int i = 0;
        char buf[256];
        auto s = g_snprintf(buf, sizeof(buf), "%d\r\n", i++);
        vte_terminal_feed(window->terminal, buf, s);
        return G_SOURCE_CONTINUE;
}

static bool
vteapp_window_tick(VteappWindow* window,
                   GError** error)
{
        g_timeout_add_seconds(1, (GSourceFunc) tick_cb, window);
        return true;
}

static void
vteapp_window_launch(VteappWindow* window)
{
        auto rv = bool{};
        auto error = vte::glib::Error{};

        if (options.exec_argv != nullptr)
                rv = vteapp_window_launch_argv(window, options.exec_argv, error);
        else if (options.command != nullptr)
                rv = vteapp_window_launch_commandline(window, options.command, error);
        else if (options.shell)
                rv = vteapp_window_launch_shell(window, error);
        else if (options.pty)
                rv = vteapp_window_fork(window, error);
        else
                rv = vteapp_window_tick(window, error);

        if (!rv)
                verbose_printerr("Error launching: %s\n", error.message());
}

static void
window_update_copy_sensitivity(VteappWindow* window)
{
        auto action = g_action_map_lookup_action(G_ACTION_MAP(window), "copy");
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
                                    vte_terminal_get_has_selection(window->terminal));
}

static void
window_update_fullscreen_state(VteappWindow* window)
{
#if VTE_GTK == 3
        auto const fullscreen = (window->window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
#elif VTE_GTK == 4
        auto const fullscreen = (window->toplevel_state & GDK_TOPLEVEL_STATE_FULLSCREEN) != 0;
#endif
        auto action = g_action_map_lookup_action(G_ACTION_MAP(window), "fullscreen");
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean (fullscreen));
}

static void
window_update_paste_sensitivity(VteappWindow* window)
{
        bool can_paste = false;

#if VTE_GTK == 3
        GdkAtom* targets;
        int n_targets;

        if (gtk_clipboard_wait_for_targets(window->clipboard, &targets, &n_targets)) {
                can_paste = gtk_targets_include_text(targets, n_targets);
                g_free(targets);
        }
#elif VTE_GTK == 4
        auto formats = gdk_clipboard_get_formats(window->clipboard);
        can_paste = gdk_content_formats_contain_gtype(formats, G_TYPE_STRING);
#endif /* VTE_GTK */

        auto action = g_action_map_lookup_action(G_ACTION_MAP(window), "paste");
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), can_paste);
}

/* Callbacks */

static void
window_action_copy_cb(GSimpleAction* action,
                      GVariant* parameter,
                      void* data)
{
        VteappWindow* window = VTEAPP_WINDOW(data);
        char const* str = g_variant_get_string(parameter, nullptr);

        vte_terminal_copy_clipboard_format(window->terminal,
                                           g_str_equal(str, "html") ? VTE_FORMAT_HTML : VTE_FORMAT_TEXT);

}

static void
window_action_copy_match_cb(GSimpleAction* action,
                            GVariant* parameter,
                            void* data)
{
        auto window = VTEAPP_WINDOW(data);

        auto len = size_t{};
        auto str = g_variant_get_string(parameter, &len);
#if VTE_GTK == 3
        gtk_clipboard_set_text(window->clipboard, str, len);
#elif VTE_GTK == 4
        gdk_clipboard_set_text(window->clipboard, str);
#endif
}

static void
window_action_paste_cb(GSimpleAction* action,
                       GVariant* parameter,
                       void* data)
{
        VteappWindow* window = VTEAPP_WINDOW(data);
        vte_terminal_paste_clipboard(window->terminal);
}

static void
window_action_reset_cb(GSimpleAction* action,
                       GVariant* parameter,
                       void* data)
{
        auto window = VTEAPP_WINDOW(data);
        auto clear = false;

        if (parameter != nullptr)
                clear = g_variant_get_boolean(parameter);
        else {
                auto modifiers = GdkModifierType{};
#if VTE_GTK == 3
                if (!gtk_get_current_event_state(&modifiers))
                        modifiers = GdkModifierType(0);
#elif VTE_GTK == 4
                // FIXMEgtk4!
                modifiers = GdkModifierType(0);
#endif

                clear = (modifiers & GDK_CONTROL_MASK) != 0;
        }

        vte_terminal_reset(window->terminal, true, clear);
}

static void
window_action_find_cb(GSimpleAction* action,
                      GVariant* parameter,
                      void* data)
{
        VteappWindow* window = VTEAPP_WINDOW(data);
        gtk_toggle_button_set_active(window->find_button, true);
}

static void
window_action_fullscreen_state_cb (GSimpleAction *action,
                                   GVariant *state,
                                   void* data)
{
        VteappWindow* window = VTEAPP_WINDOW(data);

        if (!gtk_widget_get_realized(GTK_WIDGET(window)))
                return;

        if (g_variant_get_boolean(state))
                gtk_window_fullscreen(GTK_WINDOW(window));
        else
                gtk_window_unfullscreen(GTK_WINDOW(window));

        /* The window-state-changed callback will update the action's actual state */
}

static void
window_setup_context_menu_cb(VteTerminal* terminal,
                             VteEventContext const* context,
                             VteappWindow* window)
{
        if (!options.context_menu)
                return;

        // context == nullptr when the menu is dismissed after being shown
        if (!context) {
                verbose_print("setup-context-menu reset\n");
                vte_terminal_set_context_menu_model(terminal, nullptr);
                return;
        }

        verbose_print("setup-context-menu\n");

        auto menu = vte::glib::take_ref(g_menu_new());
        g_menu_append(menu.get(), "_Copy", "win.copy::text");
        g_menu_append(menu.get(), "Copy As _HTML", "win.copy::html");

#if VTE_GTK == 4
        double x, y;
#endif
        if (
#if VTE_GTK == 3
            auto const event = vte_event_context_get_event(context)
#elif VTE_GTK == 4
            vte_event_context_get_coordinates(context, &x, &y)
#endif // VTE_GTK
            ) {
#if VTE_GTK == 3
                auto hyperlink = vte::glib::take_string(vte_terminal_hyperlink_check_event(window->terminal, event));
#elif VTE_GTK == 4
                auto hyperlink = vte::glib::take_string(vte_terminal_check_hyperlink_at(window->terminal, x, y));
#endif
                if (hyperlink) {
                        verbose_print("Hyperlink: %s\n", hyperlink.get());
                        auto target = g_variant_new_string(hyperlink.get()); /* floating */
                        auto item = vte::glib::take_ref(g_menu_item_new("Copy _Hyperlink", nullptr));
                        g_menu_item_set_action_and_target_value(item.get(), "win.copy-match", target);
                        g_menu_append_item(menu.get(), item.get());
                }

#if VTE_GTK == 3
                auto match = vte::glib::take_string(vte_terminal_match_check_event(window->terminal, event, nullptr));
#elif VTE_GTK == 4
                auto match = vte::glib::take_string(vte_terminal_check_match_at(window->terminal, x, y, nullptr));
#endif // VTE_GTK
                if (match) {
                        verbose_print("Match: %s\n", match.get());
                        auto target = g_variant_new_string(match.get()); /* floating */
                        auto item = vte::glib::take_ref(g_menu_item_new("Copy _Match", nullptr));
                        g_menu_item_set_action_and_target_value(item.get(), "win.copy-match", target);
                        g_menu_append_item(menu.get(), item.get());
                }

                /* Test extra match API */
                static const char extra_pattern[] = "(\\d+)\\s*(\\w+)";
                char* extra_match = nullptr;
                char *extra_subst = nullptr;
                auto error = vte::glib::Error{};
                auto regex = compile_regex_for_match(extra_pattern, false, error);
                error.assert_no_error();

                VteRegex* regexes[1] = {regex.get()};
#if VTE_GTK == 3
                vte_terminal_event_check_regex_simple(window->terminal, event,
                                                      regexes, G_N_ELEMENTS(regexes),
                                                      0,
                                                      &extra_match);
#elif VTE_GTK == 4
                vte_terminal_check_regex_simple_at(window->terminal, x, y,
                                                   regexes, G_N_ELEMENTS(regexes),
                                                   0,
                                                   &extra_match);

#endif // VTE_GTK
                if (extra_match != nullptr &&
                    (extra_subst = vte_regex_substitute(regex.get(), extra_match, "$2 $1",
                                                        PCRE2_SUBSTITUTE_EXTENDED |
                                                        PCRE2_SUBSTITUTE_GLOBAL,
                                                        error)) == nullptr) {
                        verbose_printerr("Substitution failed: %s\n", error.message());
                }

                if (extra_match != nullptr) {
                        if (extra_subst != nullptr)
                                verbose_print("%s match: %s => %s\n", extra_pattern, extra_match, extra_subst);
                        else
                                verbose_print("%s match: %s\n", extra_pattern, extra_match);
                }
                g_free(extra_match);
                g_free(extra_subst);
        }

        g_menu_append(menu.get(), "_Paste", "win.paste");

#if VTE_GTK == 3
        auto const fullscreen = (window->window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
#elif VTE_GTK == 4
        auto const fullscreen = gtk_window_is_fullscreen(GTK_WINDOW(window));
#endif // VTE_GTK
        if (fullscreen)
                g_menu_append(menu.get(), "_Fullscreen", "win.fullscreen");

        vte_terminal_set_context_menu_model(terminal, G_MENU_MODEL(menu.get()));
}

static void
window_cell_size_changed_cb(VteTerminal* term,
                            guint width,
                            guint height,
                            VteappWindow* window)
{
        vteapp_window_update_geometry(window);
}

static void
window_child_exited_cb(VteTerminal* term,
                       int status,
                       VteappWindow* window)
{
        if (WIFEXITED(status))
                verbose_printerr("Child exited with status %x\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
                verbose_printerr("Child terminated by signal %d\n", WTERMSIG(status));
        else
                verbose_printerr("Child terminated\n");

        if (options.output_filename != nullptr) {
                auto file = vte::glib::take_ref
                        (g_file_new_for_commandline_arg(options.output_filename));
                auto error = vte::glib::Error{};
                auto stream = vte::glib::take_ref(g_file_replace(file.get(),
                                                                 nullptr,
                                                                 false,
                                                                 G_FILE_CREATE_NONE,
                                                                 nullptr,
                                                                 error));

                if (stream) {
                        vte_terminal_write_contents_sync(window->terminal,
                                                         G_OUTPUT_STREAM(stream.get()),
                                                         VTE_WRITE_DEFAULT,
                                                         nullptr,
                                                         error);
                }

                if (error.error()) {
                        verbose_printerr("Failed to write output to \"%s\": %s\n",
                                         options.output_filename, error.message());
                }
        }

        window->child_pid = -1;

        if (options.keep)
                return;

#if VTE_GTK == 3
        gtk_widget_destroy(GTK_WIDGET(window));
#elif VTE_GTK == 4
        gtk_window_destroy(GTK_WINDOW(window));
#endif
}

#if VTE_GTK == 3

static void
window_clipboard_owner_change_cb(GtkClipboard* clipboard,
                                 GdkEvent* event,
                                 VteappWindow* window)
{
        window_update_paste_sensitivity(window);
}

#elif VTE_GTK == 4

static void
window_clipboard_formats_notify_cb(GdkClipboard* clipboard,
                                   GParamSpec* pspec,
                                   VteappWindow* window)
{
        window_update_paste_sensitivity(window);
}

#endif /* VTE_GTK */

static void
window_decrease_font_size_cb(VteTerminal* terminal,
                             VteappWindow* window)
{
        vteapp_window_adjust_font_size(window, 1.0 / 1.2);
}

static void
window_increase_font_size_cb(VteTerminal* terminal,
                             VteappWindow* window)
{
        vteapp_window_adjust_font_size(window, 1.2);
}

static void
window_deiconify_window_cb(VteTerminal* terminal,
                           VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

#if VTE_GTK == 3
        gtk_window_deiconify(GTK_WINDOW(window));
#elif VTE_GTK == 4
        auto toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(window)));
        gdk_toplevel_present(toplevel, nullptr); // FIXMEgtk4 nullptr not allowed
#endif
}

static void
window_iconify_window_cb(VteTerminal* terminal,
                         VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

#if VTE_GTK == 3
        gtk_window_iconify(GTK_WINDOW(window));
#elif VTE_GTK == 4
        auto toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(window)));
        gdk_toplevel_minimize(toplevel);
#endif
}

static void
window_window_title_changed_cb(VteTerminal* terminal,
                               char const* prop,
                               VteappWindow* window)
{
        auto const title = vte_terminal_get_termprop_string_by_id(window->terminal,
                                                                  VTE_PROPERTY_ID_XTERM_TITLE,
                                                                  nullptr);
        gtk_window_set_title(GTK_WINDOW(window), title && title[0] ? title : "Terminal");
}

static void
window_lower_window_cb(VteTerminal* terminal,
                       VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (!gtk_widget_get_realized(GTK_WIDGET(window)))
                return;

#if VTE_GTK == 3
        gdk_window_lower(gtk_widget_get_window(GTK_WIDGET(window)));
#elif VTE_GTK == 4
        auto toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(window)));
        gdk_toplevel_lower(toplevel);
#endif
}

static void
window_raise_window_cb(VteTerminal* terminal,
                       VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (!gtk_widget_get_realized(GTK_WIDGET(window)))
                return;

#if VTE_GTK == 3
        gdk_window_raise(gtk_widget_get_window(GTK_WIDGET(window)));
#elif VTE_GTK == 4
        auto toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(window)));
        gdk_toplevel_present(toplevel, nullptr); // FIXMEgtk4 gdk_toplevel_raise() doesn't exist??
#endif
}

static void
window_maximize_window_cb(VteTerminal* terminal,
                          VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        gtk_window_maximize(GTK_WINDOW(window));
}

static void
window_restore_window_cb(VteTerminal* terminal,
                         VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        gtk_window_unmaximize(GTK_WINDOW(window));
}

static void
window_move_window_cb(VteTerminal* terminal,
                      guint x,
                      guint y,
                      VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

#if VTE_GTK == 3
        gtk_window_move(GTK_WINDOW(window), x, y);
#elif VTE_GTK == 4
        // FIXMEgtk4
#endif
}

#if VTE_GTK == 4

static void
window_toplevel_notify_state_cb(GdkToplevel* toplevel,
                                GParamSpec* pspec,
                                VteappWindow* window)
{
        window->toplevel_state = gdk_toplevel_get_state(toplevel);
        window_update_fullscreen_state(window);
}

#endif /* VTE_GTK == 4 */

static void
window_notify_cb(GObject* object,
                 GParamSpec* pspec,
                 VteappWindow* window)
{
        if (pspec->owner_type != VTE_TYPE_TERMINAL)
                return;

        GValue value G_VALUE_INIT;
        g_object_get_property(object, pspec->name, &value);
        auto str = g_strdup_value_contents(&value);
        g_value_unset(&value);

        verbose_print("NOTIFY property \"%s\" value %s\n", pspec->name, str);
        g_free(str);
}

static void
window_refresh_window_cb(VteTerminal* terminal,
                         VteappWindow* window)
{
        gtk_widget_queue_draw(GTK_WIDGET(window));
}

static void
window_resize_window_cb(VteTerminal* terminal,
                        guint columns,
                        guint rows,
                        VteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (columns < MIN_COLUMNS || rows < MIN_ROWS)
                return;

        vte_terminal_set_size(window->terminal, columns, rows);
        vteapp_window_resize(window);
}

static void
window_selection_changed_cb(VteTerminal* terminal,
                            VteappWindow* window)
{
        window_update_copy_sensitivity(window);
}

static void
window_termprop_changed_cb(VteTerminal* terminal,
                           char const* prop,
                           VteappWindow* window)
{
        if (auto const value = vte::take_freeable
            (vte_terminal_ref_termprop_variant(terminal, prop))) {
                auto str = vte::glib::take_string(g_variant_print(value.get(), true));
                verbose_print("Termprop '%s' changed to '%s'\n",
                              prop, str.get());
        } else {
                verbose_print("Termprop '%s' changed to no-value\n",
                              prop);
        }
}

static void
window_input_enabled_state_cb(GAction* action,
                              GParamSpec* pspec,
                              VteappWindow* window)
{
        gtk_widget_set_visible(window->readonly_emblem,
                               !g_variant_get_boolean(g_action_get_state(action)));
}

static void
window_search_popover_closed_cb(GtkPopover* popover,
                                VteappWindow* window)
{
        if (gtk_toggle_button_get_active(window->find_button))
                gtk_toggle_button_set_active(window->find_button, false);
}

static void
window_find_button_toggled_cb(GtkToggleButton* button,
                              VteappWindow* window)
{
        auto active = gtk_toggle_button_get_active(button);

        if (gtk_widget_get_visible(GTK_WIDGET(window->search_popover)) != active)
                gtk_widget_set_visible(GTK_WIDGET(window->search_popover), active);
}

G_DEFINE_TYPE(VteappWindow, vteapp_window, GTK_TYPE_APPLICATION_WINDOW)

static void
vteapp_window_init(VteappWindow* window)
{
        gtk_widget_init_template(GTK_WIDGET(window));

        window->child_pid = -1;
}

static void
vteapp_window_constructed(GObject *object)
{
        VteappWindow* window = VTEAPP_WINDOW(object);

        G_OBJECT_CLASS(vteapp_window_parent_class)->constructed(object);

        gtk_window_set_title(GTK_WINDOW(window),
                             options.title && options.title[0] ? options.title : "Terminal");

        if (!options.decorations)
                gtk_window_set_decorated(GTK_WINDOW(window), false);

        /* Create terminal and connect scrollbar */
        window->terminal = reinterpret_cast<VteTerminal*>(vteapp_terminal_new());

        gtk_widget_set_hexpand(GTK_WIDGET(window->terminal), true);
        gtk_widget_set_vexpand(GTK_WIDGET(window->terminal), true);

        auto margin = options.extra_margin;
        if (margin >= 0) {
                gtk_widget_set_margin_start(GTK_WIDGET(window->terminal), margin);
                gtk_widget_set_margin_end(GTK_WIDGET(window->terminal), margin);
                gtk_widget_set_margin_top(GTK_WIDGET(window->terminal), margin);
                gtk_widget_set_margin_bottom(GTK_WIDGET(window->terminal), margin);
        }

        if (options.scrollbar && !options.use_scrolled_window) {
                auto vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(window->terminal));
#if VTE_GTK == 3
                gtk_range_set_adjustment(GTK_RANGE(window->scrollbar), vadj);
#elif VTE_GTK == 4
                gtk_scrollbar_set_adjustment(GTK_SCROLLBAR(window->scrollbar), vadj);
#endif
        }

        if (!options.scrollbar || options.use_scrolled_window) {
#if VTE_GTK == 3
                gtk_widget_destroy(GTK_WIDGET(window->scrollbar));
#elif VTE_GTK == 4
                gtk_grid_remove(GTK_GRID(window->window_grid), GTK_WIDGET(window->scrollbar));
#endif
                window->scrollbar = nullptr;
        }

        /* Create actions */
        GActionEntry const entries[] = {
                { "copy",        window_action_copy_cb,       "s", nullptr, nullptr },
                { "copy-match",  window_action_copy_match_cb, "s", nullptr, nullptr },
                { "paste",       window_action_paste_cb,  nullptr, nullptr, nullptr },
                { "reset",       window_action_reset_cb,      "b", nullptr, nullptr },
                { "find",        window_action_find_cb,   nullptr, nullptr, nullptr },
                { "fullscreen",  nullptr,                 nullptr, "false", window_action_fullscreen_state_cb },
        };

        GActionMap* map = G_ACTION_MAP(window);
        g_action_map_add_action_entries(map, entries, G_N_ELEMENTS(entries), window);

        /* Property actions */
        auto action = vte::glib::take_ref
                (g_property_action_new("input-enabled", window->terminal, "input-enabled"));
        g_action_map_add_action(map, G_ACTION(action.get()));
        g_signal_connect(action.get(), "notify::state", G_CALLBACK(window_input_enabled_state_cb), window);

#if VTE_GTK == 4
        auto gear_popover = gtk_menu_button_get_popover(GTK_MENU_BUTTON(window->gear_button));
        gtk_widget_set_halign(GTK_WIDGET(gear_popover), GTK_ALIGN_END);
#endif

        /* Find */
        window->search_popover = vteapp_search_popover_new(window->terminal,
                                                           GTK_WIDGET(window->find_button));

        g_signal_connect(window->search_popover, "closed",
                         G_CALLBACK(window_search_popover_closed_cb), window);
        g_signal_connect(window->find_button, "toggled",
                         G_CALLBACK(window_find_button_toggled_cb), window);

        /* Clipboard */
#if VTE_GTK == 3
        window->clipboard = gtk_widget_get_clipboard(GTK_WIDGET(window), GDK_SELECTION_CLIPBOARD);
        g_signal_connect(window->clipboard, "owner-change", G_CALLBACK(window_clipboard_owner_change_cb), window);
#elif VTE_GTK == 4
        window->clipboard = gtk_widget_get_clipboard(GTK_WIDGET(window));
        g_signal_connect(window->clipboard, "notify::formats", G_CALLBACK(window_clipboard_formats_notify_cb), window);
#endif /* VTE_GTK */

        /* Set ARGB visual */
        if (options.transparency_percent >= 0) {
#if VTE_GTK == 3
                if (options.argb_visual) {
                        auto screen = gtk_widget_get_screen(GTK_WIDGET(window));
                        auto visual = gdk_screen_get_rgba_visual(screen);
                        if (visual != nullptr)
                                gtk_widget_set_visual(GTK_WIDGET(window), visual);
                }

                /* Without this transparency doesn't work; see bug #729884. */
                gtk_widget_set_app_paintable(GTK_WIDGET(window), true);

#elif VTE_GTK == 4
                // FIXMEgtk4
#endif /* VTE_GTK == 3 */
        }

        /* Signals */
        g_signal_connect(window->terminal, "setup-context-menu", G_CALLBACK(window_setup_context_menu_cb), window);
        g_signal_connect(window->terminal, "char-size-changed", G_CALLBACK(window_cell_size_changed_cb), window);
        g_signal_connect(window->terminal, "child-exited", G_CALLBACK(window_child_exited_cb), window);
        g_signal_connect(window->terminal, "decrease-font-size", G_CALLBACK(window_decrease_font_size_cb), window);
        g_signal_connect(window->terminal, "deiconify-window", G_CALLBACK(window_deiconify_window_cb), window);
        g_signal_connect(window->terminal, "iconify-window", G_CALLBACK(window_iconify_window_cb), window);
        g_signal_connect(window->terminal, "increase-font-size", G_CALLBACK(window_increase_font_size_cb), window);
        g_signal_connect(window->terminal, "lower-window", G_CALLBACK(window_lower_window_cb), window);
        g_signal_connect(window->terminal, "maximize-window", G_CALLBACK(window_maximize_window_cb), window);
        g_signal_connect(window->terminal, "move-window", G_CALLBACK(window_move_window_cb), window);
        g_signal_connect(window->terminal, "raise-window", G_CALLBACK(window_raise_window_cb), window);
        g_signal_connect(window->terminal, "refresh-window", G_CALLBACK(window_refresh_window_cb), window);
        g_signal_connect(window->terminal, "resize-window", G_CALLBACK(window_resize_window_cb), window);
        g_signal_connect(window->terminal, "restore-window", G_CALLBACK(window_restore_window_cb), window);
        g_signal_connect(window->terminal, "selection-changed", G_CALLBACK(window_selection_changed_cb), window);
        g_signal_connect(window->terminal, "termprop-changed", G_CALLBACK(window_termprop_changed_cb), window);
        g_signal_connect(window->terminal, "termprop-changed::" VTE_TERMPROP_XTERM_TITLE, G_CALLBACK(window_window_title_changed_cb), window);
        if (options.object_notifications)
                g_signal_connect(window->terminal, "notify", G_CALLBACK(window_notify_cb), window);

        /* Settings */
#if VTE_GTK == 3
        if (!options.double_buffer) {
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
                gtk_widget_set_double_buffered(GTK_WIDGET(window->terminal), false);
                G_GNUC_END_IGNORE_DEPRECATIONS;
        }
#endif /* VTE_GTK == 3 */

        if (options.encoding != nullptr) {
                auto error = vte::glib::Error{};
                if (!vte_terminal_set_encoding(window->terminal, options.encoding, error))
                        g_printerr("Failed to set encoding: %s\n", error.message());
        }

        if (options.word_char_exceptions != nullptr)
                vte_terminal_set_word_char_exceptions(window->terminal, options.word_char_exceptions);

        vte_terminal_set_allow_hyperlink(window->terminal, options.hyperlink);
        vte_terminal_set_audible_bell(window->terminal, options.audible_bell);
        vte_terminal_set_allow_bold(window->terminal, options.bold);
        vte_terminal_set_bold_is_bright(window->terminal, options.bold_is_bright);
        vte_terminal_set_cell_height_scale(window->terminal, options.cell_height_scale);
        vte_terminal_set_cell_width_scale(window->terminal, options.cell_width_scale);
        vte_terminal_set_cjk_ambiguous_width(window->terminal, options.cjk_ambiguous_width);
        vte_terminal_set_cursor_blink_mode(window->terminal, options.cursor_blink_mode);
        vte_terminal_set_cursor_shape(window->terminal, options.cursor_shape);
        vte_terminal_set_enable_a11y(window->terminal, options.a11y);
        vte_terminal_set_enable_bidi(window->terminal, options.bidi);
        vte_terminal_set_enable_shaping(window->terminal, options.shaping);
        vte_terminal_set_enable_fallback_scrolling(window->terminal, options.fallback_scrolling);
        vte_terminal_set_enable_legacy_osc777(window->terminal, options.legacy_osc777);
        vte_terminal_set_mouse_autohide(window->terminal, true);
        vte_terminal_set_rewrap_on_resize(window->terminal, options.rewrap);
        vte_terminal_set_scroll_on_insert(window->terminal, options.scroll_on_insert);
        vte_terminal_set_scroll_on_output(window->terminal, options.scroll_on_output);
        vte_terminal_set_scroll_on_keystroke(window->terminal, options.scroll_on_keystroke);
        vte_terminal_set_scroll_unit_is_pixels(window->terminal, options.scroll_unit_is_pixels);
        vte_terminal_set_scrollback_lines(window->terminal, options.scrollback_lines);
        vte_terminal_set_text_blink_mode(window->terminal, options.text_blink_mode);
        if (options.xalign != VteAlign(-1))
                vte_terminal_set_xalign(window->terminal, options.xalign);
        if (options.yalign != VteAlign(-1))
                vte_terminal_set_yalign(window->terminal, options.yalign);
        vte_terminal_set_xfill(window->terminal, options.xfill);
        vte_terminal_set_yfill(window->terminal, options.yfill);

        /* Style */
        if (options.font_string != nullptr) {
                auto desc = vte::take_freeable(pango_font_description_from_string(options.font_string));
                vte_terminal_set_font(window->terminal, desc.get());
        }

        auto fg = options.get_color_fg();
        auto bg = options.get_color_bg();
        vte_terminal_set_colors(window->terminal, &fg, &bg, nullptr, 0);
        if (options.cursor_bg_color_set)
                vte_terminal_set_color_cursor(window->terminal, &options.cursor_bg_color);
        if (options.cursor_fg_color_set)
                vte_terminal_set_color_cursor_foreground(window->terminal, &options.cursor_fg_color);
        if (options.hl_bg_color_set)
                vte_terminal_set_color_highlight(window->terminal, &options.hl_bg_color);
        if (options.hl_fg_color_set)
                vte_terminal_set_color_highlight_foreground(window->terminal, &options.hl_fg_color);

        if (options.whole_window_transparent)
                gtk_widget_set_opacity (GTK_WIDGET (window), options.get_alpha());

        /* Dingus */
        if (options.builtin_dingus)
                vteapp_window_add_dingus(window, builtin_dingus);
        if (options.dingus != nullptr)
                vteapp_window_add_dingus(window, options.dingus);

        /* Done! */
        if (options.use_scrolled_window) {
#if VTE_GTK == 3
                auto sw = gtk_scrolled_window_new(nullptr, nullptr);
                gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(window->terminal));
#elif VTE_GTK == 4
                auto sw = gtk_scrolled_window_new();
                gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), GTK_WIDGET(window->terminal));
#endif /* VTE_GTK */
                gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(sw),
                                                          options.kinetic_scrolling);

                auto vpolicy = GTK_POLICY_ALWAYS;
                if (!options.scrollbar)
                        vpolicy = GTK_POLICY_EXTERNAL;
                else if (options.overlay_scrollbar)
                        vpolicy = GTK_POLICY_AUTOMATIC;

                gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                               GTK_POLICY_NEVER, // hpolicy
                                               vpolicy);
                gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(sw),
                                                          options.overlay_scrollbar);

                gtk_grid_attach(GTK_GRID(window->window_grid), sw,
                                0, 0, 1, 1);
                gtk_widget_set_halign(GTK_WIDGET(sw), GTK_ALIGN_FILL);
                gtk_widget_set_valign(GTK_WIDGET(sw), GTK_ALIGN_FILL);
                gtk_widget_show(sw);
        } else {
                gtk_grid_attach(GTK_GRID(window->window_grid), GTK_WIDGET(window->terminal),
                                0, 0, 1, 1);
        }

        gtk_widget_set_halign(GTK_WIDGET(window->terminal), GTK_ALIGN_FILL);
        gtk_widget_set_valign(GTK_WIDGET(window->terminal), GTK_ALIGN_FILL);
        gtk_widget_show(GTK_WIDGET(window->terminal));

        window_update_paste_sensitivity(window);
        window_update_copy_sensitivity(window);

        gtk_widget_grab_focus(GTK_WIDGET(window->terminal));

        /* Sanity check */
        g_assert(!gtk_widget_get_realized(GTK_WIDGET(window)));
}

static void
vteapp_window_dispose(GObject *object)
{
        VteappWindow* window = VTEAPP_WINDOW(object);

        if (window->clipboard != nullptr) {
                g_signal_handlers_disconnect_by_func(window->clipboard,
#if VTE_GTK == 3
                                                     (void*)window_clipboard_owner_change_cb,
#elif VTE_GTK == 4
                                                     (void*)window_clipboard_formats_notify_cb,
#endif
                                                     window);
                window->clipboard = nullptr;
        }

        if (window->search_popover != nullptr) {
#if VTE_GTK == 3
                gtk_widget_destroy(window->search_popover);
#elif VTE_GTK == 4
                gtk_widget_unparent(window->search_popover); // this destroys the popover
#endif /* VTE_GTK */
                window->search_popover = nullptr;
        }

        // Disconnect all signal handlers from the terminal
        g_signal_handlers_disconnect_matched(window->terminal,
                                             GSignalMatchType(G_SIGNAL_MATCH_DATA),
                                             0, // signal id
                                             0, // detail quark
                                             nullptr, // closure
                                             nullptr, // func
                                             window);

        G_OBJECT_CLASS(vteapp_window_parent_class)->dispose(object);
}

static void
vteapp_window_realize(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_window_parent_class)->realize(widget);

        /* Now we can know the CSD size, and thus apply the geometry. */
        VteappWindow* window = VTEAPP_WINDOW(widget);
        verbose_print("VteappWindow::realize\n");

#if VTE_GTK == 3
        auto win = gtk_widget_get_window(GTK_WIDGET(window));
        window->window_state = gdk_window_get_state(win);
#elif VTE_GTK == 4
        auto surface = gtk_native_get_surface(GTK_NATIVE(widget));
        window->toplevel_state = gdk_toplevel_get_state(GDK_TOPLEVEL(surface));
        g_signal_connect(surface, "notify::state",
                         G_CALLBACK(window_toplevel_notify_state_cb), window);
#endif

        window_update_fullscreen_state(window);

        vteapp_window_resize(window);
}

static void
vteapp_window_unrealize(GtkWidget* widget)
{
#if VTE_GTK == 4
        auto window = VTEAPP_WINDOW(widget);
        auto toplevel = gtk_native_get_surface(GTK_NATIVE(widget));
        g_signal_handlers_disconnect_by_func(toplevel,
                                             (void*)window_toplevel_notify_state_cb,
                                             window);
#endif

        GTK_WIDGET_CLASS(vteapp_window_parent_class)->unrealize(widget);
}

static void
vteapp_window_show(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_window_parent_class)->show(widget);

        /* Re-apply the geometry. */
        VteappWindow* window = VTEAPP_WINDOW(widget);
        verbose_print("VteappWindow::show\n");
        vteapp_window_resize(window);
}

#if VTE_GTK == 3

static void
vteapp_window_style_updated(GtkWidget* widget)
{
        GTK_WIDGET_CLASS(vteapp_window_parent_class)->style_updated(widget);

        /* Re-apply the geometry. */
        VteappWindow* window = VTEAPP_WINDOW(widget);
        verbose_print("VteappWindow::style-update\n");
        vteapp_window_resize(window);
}

static gboolean
vteapp_window_state_event (GtkWidget* widget,
                           GdkEventWindowState* event)
{
        auto window = VTEAPP_WINDOW(widget);
        window->window_state = event->new_window_state;

        if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
                window_update_fullscreen_state(window);

        return GTK_WIDGET_CLASS(vteapp_window_parent_class)->window_state_event(widget, event);
}

#endif /* VTE_GTK == 3 */

static void
vteapp_window_class_init(VteappWindowClass* klass)
{
        GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->constructed  = vteapp_window_constructed;
        gobject_class->dispose  = vteapp_window_dispose;

        GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
        widget_class->realize = vteapp_window_realize;
        widget_class->unrealize = vteapp_window_unrealize;
        widget_class->show = vteapp_window_show;

#if VTE_GTK == 3
        widget_class->style_updated = vteapp_window_style_updated;
        widget_class->window_state_event = vteapp_window_state_event;
#elif VTE_GTK == 4
        // FIXMEgtk4 window state event
#endif

        gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/vte/app/ui/window.ui");

        gtk_widget_class_bind_template_child(widget_class, VteappWindow, window_grid);
        gtk_widget_class_bind_template_child(widget_class, VteappWindow, scrollbar);
        /* gtk_widget_class_bind_template_child(widget_class, VteappWindow, notification_grid); */
        gtk_widget_class_bind_template_child(widget_class, VteappWindow, readonly_emblem);
        /* gtk_widget_class_bind_template_child(widget_class, VteappWindow, copy_button); */
        /* gtk_widget_class_bind_template_child(widget_class, VteappWindow, paste_button); */
        gtk_widget_class_bind_template_child(widget_class, VteappWindow, find_button);
        gtk_widget_class_bind_template_child(widget_class, VteappWindow, gear_button);
}

static VteappWindow*
vteapp_window_new(GApplication* app)
{
        return reinterpret_cast<VteappWindow*>(g_object_new(VTEAPP_TYPE_WINDOW,
                                                            "application", app,
#if VTE_GTK == 4
                                                            "handle-menubar-accel", false,
#endif
                                                            nullptr));
}

/* application */

#define VTEAPP_TYPE_APPLICATION         (vteapp_application_get_type())
#define VTEAPP_APPLICATION(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), VTEAPP_TYPE_APPLICATION, VteappApplication))
#define VTEAPP_APPLICATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), VTEAPP_TYPE_APPLICATION, VteappApplicationClass))
#define VTEAPP_IS_APPLICATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), VTEAPP_TYPE_APPLICATION))
#define VTEAPP_IS_APPLICATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), VTEAPP_TYPE_APPLICATION))
#define VTEAPP_APPLICATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), VTEAPP_TYPE_APPLICATION, VteappApplicationClass))

typedef struct _VteappApplication       VteappApplication;
typedef struct _VteappApplicationClass  VteappApplicationClass;

struct _VteappApplication {
        GtkApplication parent;

        guint input_source;
};

struct _VteappApplicationClass {
        GtkApplicationClass parent;
};

static GType vteapp_application_get_type(void);

static void
app_action_new_cb(GSimpleAction* action,
                  GVariant* parameter,
                  void* data)
{
        GApplication* application = G_APPLICATION(data);
        auto window = vteapp_window_new(application);
        vteapp_window_parse_geometry(window);
        gtk_window_present(GTK_WINDOW(window));
        vteapp_window_launch(window);
}

static void
app_action_close_cb(GSimpleAction* action,
                    GVariant* parameter,
                    void* data)
{
        GtkApplication* application = GTK_APPLICATION(data);
        auto window = gtk_application_get_active_window(application);
        if (window == nullptr)
                return;

#if VTE_GTK == 3
        gtk_widget_destroy(GTK_WIDGET(window));
#elif VTE_GTK == 4
        gtk_window_destroy(GTK_WINDOW(window));
#endif
}

static gboolean
app_stdin_readable_cb(int fd,
                      GIOCondition condition,
                      VteappApplication* application)
{
        auto eos = bool{false};
        if (condition & G_IO_IN) {
                auto window = gtk_application_get_active_window(GTK_APPLICATION(application));
                auto terminal = VTEAPP_IS_WINDOW(window) ? VTEAPP_WINDOW(window)->terminal : nullptr;

                char buf[4096];
                auto r = int{0};
                do {
                        errno = 0;
                        r = read(fd, buf, sizeof(buf));
                        if (r > 0 && terminal != nullptr)
                                vte_terminal_feed(terminal, buf, r);
                } while (r > 0 || errno == EINTR);

                if (r == 0)
                        eos = true;
        }

        if (eos) {
                application->input_source = 0;
                return G_SOURCE_REMOVE;
        }

        return G_SOURCE_CONTINUE;
}

#if VTE_GTK == 3

static void
app_clipboard_targets_received_cb(GtkClipboard* clipboard,
                                  GdkAtom* targets,
                                  int n_targets,
                                  VteappApplication* application)
{
        verbose_print("Clipboard has %d targets:", n_targets);

        for (auto i = 0; i < n_targets; ++i) {
                auto atom_name = vte::glib::take_string(gdk_atom_name(targets[i]));
                verbose_print(" %s", atom_name.get());
        }
        verbose_print("\n");
}

static void
app_clipboard_owner_change_cb(GtkClipboard* clipboard,
                              GdkEvent* event G_GNUC_UNUSED,
                              VteappApplication* application)
{
        verbose_print("Clipboard owner-change, requesting targets\n");

        /* We can do this without holding a reference to @application since
         * the application lives as long as the process.
         */
        gtk_clipboard_request_targets(clipboard,
                                      (GtkClipboardTargetsReceivedFunc)app_clipboard_targets_received_cb,
                                      application);
}

#elif VTE_GTK == 4

static void
app_clipboard_changed_cb(GdkClipboard* clipboard,
                         VteappApplication* application)
{
        auto formats = gdk_clipboard_get_formats(clipboard);
        auto str = vte::glib::take_string(gdk_content_formats_to_string(formats));

        verbose_print("Clipboard owner changed, targets now %s\n", str.get());
}

#endif /* VTE_GTK */

static gboolean
app_load_css_from_resource(GApplication *application,
                           GtkCssProvider *provider,
                           bool theme)
{
        auto const base_path = g_application_get_resource_base_path(application);

        char* uri = nullptr;
        if (theme) {
                char *str = nullptr;
                g_object_get(gtk_settings_get_default(), "gtk-theme-name", &str, nullptr);
                auto theme_name = g_ascii_strdown (str, -1);
                uri = g_strdup_printf("resource://%s/css/%s/app.css", base_path, theme_name);
                g_free(theme_name);
                g_free(str);
        } else {
                uri = g_strdup_printf("resource://%s/css/app.css", base_path);
        }

        auto file = vte::glib::take_ref(g_file_new_for_uri(uri));
        g_free(uri);

        if (!g_file_query_exists(file.get(), nullptr /* cancellable */))
                return false;

#if VTE_GTK == 3
        gtk_css_provider_load_from_file(provider, file.get(), nullptr);
#elif VTE_GTK == 4
        gtk_css_provider_load_from_file(provider, file.get());
#endif

        return true;
}

static void
app_load_css(GApplication *application,
             bool theme)
{
        auto provider = vte::glib::take_ref(gtk_css_provider_new());
        if (!app_load_css_from_resource(application, provider.get(), theme))
                return;

#if VTE_GTK == 3
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                  GTK_STYLE_PROVIDER(provider.get()),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#elif VTE_GTK == 4
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                                   GTK_STYLE_PROVIDER(provider.get()),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#endif
}

G_DEFINE_TYPE(VteappApplication, vteapp_application, GTK_TYPE_APPLICATION)

static void
vteapp_application_init(VteappApplication* application)
{
        g_object_set(gtk_settings_get_default(),
                     "gtk-enable-accels", FALSE,
#if VTE_GTK == 3
                     "gtk-enable-mnemonics", FALSE,
                     /* Make gtk+ CSD not steal F10 from the terminal */
                     "gtk-menu-bar-accel", nullptr,
#endif
                     nullptr);

        if (options.css) {
#if VTE_GTK == 3
                gtk_style_context_add_provider_for_screen(gdk_screen_get_default (),
                                                          GTK_STYLE_PROVIDER(options.css.get()),
                                                          GTK_STYLE_PROVIDER_PRIORITY_USER);
#elif VTE_GTK == 4
                gtk_style_context_add_provider_for_display(gdk_display_get_default (),
                                                          GTK_STYLE_PROVIDER(options.css.get()),
                                                          GTK_STYLE_PROVIDER_PRIORITY_USER);
#endif
        }

        if (options.feed_stdin) {
                g_unix_set_fd_nonblocking(STDIN_FILENO, true, nullptr);
                application->input_source = g_unix_fd_add(STDIN_FILENO,
                                                          GIOCondition(G_IO_IN | G_IO_HUP | G_IO_ERR),
                                                          (GUnixFDSourceFunc)app_stdin_readable_cb,
                                                          application);
        }

        if (options.track_clipboard_targets) {
#if VTE_GTK == 3
                auto clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(),
                                                               GDK_SELECTION_CLIPBOARD);
                app_clipboard_owner_change_cb(clipboard, nullptr, application);
                g_signal_connect(clipboard, "owner-change",
                                 G_CALLBACK(app_clipboard_owner_change_cb), application);

#elif VTE_GTK == 4
                auto clipboard = gdk_display_get_clipboard(gdk_display_get_default());
                app_clipboard_changed_cb(clipboard, application);
                g_signal_connect(clipboard, "changed",
                                 G_CALLBACK(app_clipboard_changed_cb), application);
#endif /* VTE_GTK */
        }
}

static void
vteapp_application_dispose(GObject* object)
{
        VteappApplication* application = VTEAPP_APPLICATION(object);

        if (application->input_source != 0) {
                g_source_remove(application->input_source);
                application->input_source = 0;
        }

        G_OBJECT_CLASS(vteapp_application_parent_class)->dispose(object);
}

static void
vteapp_application_startup(GApplication* application)
{
        // Load built-in CSS
        app_load_css(application, true);
        app_load_css(application, false);

        /* Create actions */
        GActionEntry const entries[] = {
                { "new",   app_action_new_cb,   nullptr, nullptr, nullptr },
                { "close", app_action_close_cb, nullptr, nullptr, nullptr },
        };

        GActionMap* map = G_ACTION_MAP(application);
        g_action_map_add_action_entries(map, entries, G_N_ELEMENTS(entries), application);

        g_application_set_resource_base_path (application, "/org/gnome/vte/app");

        G_APPLICATION_CLASS(vteapp_application_parent_class)->startup(application);
}

static void
vteapp_application_activate(GApplication* application)
{
        auto action = g_action_map_lookup_action(G_ACTION_MAP(application), "new");
        g_action_activate(action, nullptr);
}

static void
vteapp_application_class_init(VteappApplicationClass* klass)
{
        GObjectClass* object_class = G_OBJECT_CLASS(klass);
        object_class->dispose = vteapp_application_dispose;

        GApplicationClass* application_class = G_APPLICATION_CLASS(klass);
        application_class->startup = vteapp_application_startup;
        application_class->activate = vteapp_application_activate;
}

static GApplication*
vteapp_application_new(void)
{
        return reinterpret_cast<GApplication*>(g_object_new(VTEAPP_TYPE_APPLICATION,
                                                            "application-id", "org.gnome.Vte.Application",
                                                            "flags", guint(G_APPLICATION_NON_UNIQUE),
                                                            nullptr));
}

/* main */

int
main(int argc,
     char *argv[])
{
        setlocale(LC_ALL, "");

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

        /* Not interested in silly debug spew, bug #749195 */
        if (g_getenv("G_ENABLE_DIAGNOSTIC") == nullptr)
                g_setenv("G_ENABLE_DIAGNOSTIC", "0", true);

       g_set_prgname("Terminal");
       g_set_application_name("Terminal");

       auto error = vte::glib::Error{};
       if (!options.parse_argv(argc, argv, error)) {
               g_printerr("Error parsing arguments: %s\n", error.message());
               return EXIT_FAILURE;
       }

        if (g_getenv("VTE_CJK_WIDTH") != nullptr)
                verbose_printerr("VTE_CJK_WIDTH is not supported anymore, use --cjk-width instead\n");

       if (options.version) {
               g_print("VTE Application %s %s\n", VERSION, vte_get_features());
               return EXIT_SUCCESS;
       }

#if VTE_GTK == 3
       if (options.debug)
               gdk_window_set_debug_updates(true);
#endif /* VTE_GTK == 3 */

#if VTE_DEBUG
       if (options.test_mode) {
               vte_set_test_flags(VTE_TEST_FLAGS_ALL);
               options.allow_window_ops = true;
       }
#else
       options.test_mode = false;
#endif

       auto reset_termios = bool{false};
       struct termios saved_tcattr;
       if (options.feed_stdin && isatty(STDIN_FILENO)) {
               /* Put terminal in raw mode */

               struct termios tcattr;
               if (tcgetattr(STDIN_FILENO, &tcattr) == 0) {
                       saved_tcattr = tcattr;
                       cfmakeraw(&tcattr);
                       if (tcsetattr(STDIN_FILENO, TCSANOW, &tcattr) == 0)
                               reset_termios = true;
               }
       }

       auto app = vte::glib::take_ref(vteapp_application_new());
       auto rv = g_application_run(app.get(), 0, nullptr);

       if (reset_termios)
               tcsetattr(STDIN_FILENO, TCSANOW, &saved_tcattr);

       return rv;
}
