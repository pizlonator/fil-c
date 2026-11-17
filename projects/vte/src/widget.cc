/*
 * Copyright © 2008, 2009, 2010, 2018, 2019, 2020, 2021 Christian Persch
 * Copyright © 2001-2004,2009,2010 Red Hat, Inc.
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

#include "widget.hh"

#include <sys/wait.h> // for W_EXITCODE

#include <exception>
#include <new>
#include <stdexcept>
#include <string>

#include "vtegtk.hh"
#include "vteptyinternal.hh"
#include "debug.h"
#include "gobject-glue.hh"

#if VTE_GTK == 3
#define VTE_STYLE_CLASS_MONOSPACE GTK_STYLE_CLASS_MONOSPACE
#elif VTE_GTK == 4
#define VTE_STYLE_CLASS_MONOSPACE "monospace"
#endif

using namespace std::literals;

namespace vte {

namespace platform {

static void vadjustment_value_changed_cb(vte::platform::Widget* that) noexcept;

static void
im_commit_cb(GtkIMContext* im_context,
             char const* text,
             Widget* that) noexcept
try
{
        if (text == nullptr)
                return;

        that->terminal()->im_commit(text);
}
catch (...)
{
        vte::log_exception();
}

static void
im_preedit_start_cb(GtkIMContext* im_context,
                    Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit started.\n");
        that->terminal()->im_preedit_set_active(true);
}
catch (...)
{
        vte::log_exception();
}

static void
im_preedit_end_cb(GtkIMContext* im_context,
                  Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit ended.\n");
        that->terminal()->im_preedit_set_active(false);
}
catch (...)
{
        vte::log_exception();
}

static void
im_preedit_changed_cb(GtkIMContext* im_context,
                      Widget* that) noexcept
try
{
        that->im_preedit_changed();
}
catch (...)
{
        vte::log_exception();
}

static gboolean
im_retrieve_surrounding_cb(GtkIMContext* im_context,
                           Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method retrieve-surrounding.\n");
        return that->terminal()->im_retrieve_surrounding();
}
catch (...)
{
        vte::log_exception();
        return false;
}

static gboolean
im_delete_surrounding_cb(GtkIMContext* im_context,
                         int offset,
                         int n_chars,
                         Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS,
                         "Input method delete-surrounding offset %d n-chars %d.\n",
                         offset, n_chars);
        return that->terminal()->im_delete_surrounding(offset, n_chars);
}
catch (...)
{
        vte::log_exception();
        return false;
}

static void
settings_notify_cb(GtkSettings* settings,
                   GParamSpec* pspec,
                   vte::platform::Widget* that) noexcept
try
{
        that->settings_changed();
}
catch (...)
{
        vte::log_exception();
}

// Callbacks for context menu setup

void
Widget::unset_context_menu(GtkWidget* widget,
                           bool deactivate,
                           bool notify)
{
        if (!widget || widget != m_menu_showing.get())
                return;

#if VTE_GTK == 4
        // Cancel idle
        if (m_context_menu_unset_on_idle_source != 0) {
                g_source_remove(m_context_menu_unset_on_idle_source);
                m_context_menu_unset_on_idle_source = 0;
        }
#endif // VTE_GTK == 4

        // Take ownership of the menu
        auto menu = std::move(m_menu_showing);

        // Disconnect all signal handlers
        g_signal_handlers_disconnect_matched(menu.get(),
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0,
                                             nullptr,
                                             nullptr,
                                             this);

#if VTE_GTK == 3
        if (gtk_menu_get_attach_widget(GTK_MENU(menu.get())) || deactivate) {
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu.get()));
        }
        if (gtk_menu_get_attach_widget(GTK_MENU(menu.get()))) {
                // This will remove the ref from the attach widget,
                // and (potentially) destroy the menu
                gtk_menu_detach(GTK_MENU(menu.get()));
                menu.reset();
        }

#elif VTE_GTK == 4
        gtk_widget_unparent(menu.get());

        if (gtk_widget_get_visible(menu.get())) {
                gtk_popover_popdown(GTK_POPOVER(menu.get()));
                menu.reset();
        }

#endif // VTE_GTK

        if (notify)
                emit_setup_context_menu(nullptr);
}

#if VTE_GTK == 3

static void
context_menu_selection_done_cb(GtkWidget *menu,
                               vte::platform::Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Context menu selection done\n");
        that->unset_context_menu(menu, false);
}
catch (...)
{
        vte::log_exception();
}

static void
context_menu_detach_cb(GtkWidget* attach_widget,
                       GtkWidget* menu) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Context menu detached\n");

        auto const that = Widget::from_terminal(VTE_TERMINAL(attach_widget));
        that->unset_context_menu(menu, true);
}
catch (...)
{
        vte::log_exception();
}

#elif VTE_GTK == 4

static void
context_menu_unset_on_idle_cb(vte::platform::Widget* that) noexcept
try
{
        that->unset_context_menu_on_idle();
}
catch (...)
{
        vte::log_exception();
}

static void
context_menu_closed_cb(GtkWidget* menu,
                       vte::platform::Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Context menu closed\n");
        that->context_menu_closed(menu);
}
catch (...)
{
        vte::log_exception();
}

#endif // VTE_GTK

#if VTE_GTK == 4

/* Callbacks for event controllers */

static gboolean
key_pressed_cb(GtkEventControllerKey* controller,
               unsigned key,
               unsigned keycode,
               unsigned modifiers,
               Widget* that) noexcept
try
{
        return that->event_key_pressed(controller, key, keycode, modifiers);
}
catch (...)
{
        vte::log_exception();
        return false;
}

static void
key_released_cb(GtkEventControllerKey* controller,
                unsigned key,
                unsigned keycode,
                unsigned modifiers,
                Widget* that) noexcept
try
{
        that->event_key_released(controller, key, keycode, modifiers);
}
catch (...)
{
        vte::log_exception();
}

static gboolean
key_modifiers_cb(GtkEventControllerKey* controller,
                 unsigned modifiers,
                 Widget* that) noexcept
try
{
        return that->event_key_modifiers(controller, modifiers);
}
catch (...)
{
        vte::log_exception();
        return false;
}

static void
long_press_pressed_cb(GtkGestureLongPress* gesture,
                      double x,
                      double y,
                      Widget* that) noexcept
try
{
        that->gesture_long_press_pressed(gesture, x, y);
}
catch (...)
{
        vte::log_exception();
}

static void
long_press_cancelled_cb(GtkGestureLongPress* gesture,
                        Widget* that) noexcept
try
{
        that->gesture_long_press_cancelled(gesture);
}
catch (...)
{
        vte::log_exception();
}

static void
focus_enter_cb(GtkEventControllerFocus* controller,
               Widget* that) noexcept
try
{
        that->event_focus_enter(controller);
}
catch (...)
{
        vte::log_exception();
}

static void
focus_leave_cb(GtkEventControllerFocus* controller,
               Widget* that) noexcept
try
{
        that->event_focus_leave(controller);
}
catch (...)
{
        vte::log_exception();
}

static void
motion_enter_cb(GtkEventControllerMotion* controller,
                double x,
                double y,
                Widget* that) noexcept
try
{
        that->event_motion_enter(controller, x, y);
}
catch (...)
{
        vte::log_exception();
}

static void
motion_leave_cb(GtkEventControllerMotion* controller,
                Widget* that) noexcept
try
{
        that->event_motion_leave(controller);
}
catch (...)
{
        vte::log_exception();
}

static void
motion_motion_cb(GtkEventControllerMotion* controller,
                 double x,
                 double y,
                 Widget* that) noexcept
try
{
        that->event_motion(controller, x, y);
}
catch (...)
{
        vte::log_exception();
}

static void
motion_notify_is_pointer_cb(GtkEventControllerMotion* controller,
                            GParamSpec* pspec,
                            Widget* that) noexcept
try
{
        that->event_motion_notify_is_pointer(controller);
}
catch (...)
{
        vte::log_exception();
}

static void
motion_notify_contains_pointer_cb(GtkEventControllerMotion* controller,
                                  GParamSpec* pspec,
                                  Widget* that) noexcept
try
{
        that->event_motion_notify_contains_pointer(controller);
}
catch (...)
{
        vte::log_exception();
}

static void
scroll_begin_cb(GtkEventControllerScroll* controller,
                Widget* that) noexcept
try
{
        that->event_scroll_begin(controller);
}
catch (...)
{
        vte::log_exception();
}

static gboolean
scroll_scroll_cb(GtkEventControllerScroll* controller,
                 double dx,
                 double dy,
                 Widget* that) noexcept
try
{
        return that->event_scroll(controller, dx, dy);
}
catch (...)
{
        vte::log_exception();
        return false;
}

static void
scroll_end_cb(GtkEventControllerScroll* controller,
              Widget* that) noexcept
try
{
        that->event_scroll_end(controller);
}
catch (...)
{
        vte::log_exception();
}

static void
scroll_decelerate_cb(GtkEventControllerScroll* controller,
                     double vx,
                     double vy,
                     Widget* that) noexcept
try
{
        that->event_scroll_decelerate(controller, vx, vy);
}
catch (...)
{
        vte::log_exception();
}

static void
click_pressed_cb(GtkGestureClick* gesture,
                 int press_count,
                 double x,
                 double y,
                 Widget* that) noexcept
try
{
        that->gesture_click_pressed(gesture, press_count, x, y);
}
catch (...)
{
        vte::log_exception();
}

static void
click_released_cb(GtkGestureClick* gesture,
                  int press_count,
                  double x,
                  double y,
                  Widget* that) noexcept
try
{
        that->gesture_click_released(gesture, press_count, x, y);
}
catch (...)
{
        vte::log_exception();
}

static void
click_stopped_cb(GtkGestureClick* gesture,
                 Widget* that) noexcept
try
{
        that->gesture_click_stopped(gesture);
}
catch (...)
{
        vte::log_exception();
}

static void
click_unpaired_release_cb(GtkGestureClick* gesture,
                          double x,
                          double y,
                          unsigned button,
                          GdkEventSequence* sequence,
                          Widget* that) noexcept
try
{
        that->gesture_click_unpaired_release(gesture, x, y, button, sequence);
}
catch (...)
{
        vte::log_exception();
}

static void
root_realize_cb(GtkRoot* r,
                vte::platform::Widget* that) noexcept
try
{
        that->root_realize();
}
catch (...)
{
        vte::log_exception();
}

static void
root_unrealize_cb(GtkRoot* r,
                vte::platform::Widget* that) noexcept
try
{
        that->root_unrealize();
}
catch (...)
{
        vte::log_exception();
}

static void
root_surface_state_notify_cb(GdkToplevel* toplevel,
                             GParamSpec* pspec,
                             Widget* that) noexcept
try
{
        that->root_surface_state_notify();
}
catch (...)
{
        vte::log_exception();
}

#endif /* VTE_GTK == 4 */

Widget::Widget(VteTerminal* t)
        : m_widget{&t->widget}
{
        // Create a default adjustment
        set_vadjustment({});

#if VTE_GTK == 3
        gtk_widget_set_can_focus(gtk(), true);
#endif

#if VTE_GTK == 4
        gtk_widget_set_focusable(gtk(), true);
#endif

#if VTE_GTK == 3
        /* We do our own redrawing. */
        // FIXMEchpe is this still necessary?
        gtk_widget_set_redraw_on_allocate(gtk(), false);
#endif

        /* Until Terminal init is completely fixed, use zero'd memory */
        auto place = g_malloc0(sizeof(vte::terminal::Terminal));
        m_terminal = new (place) vte::terminal::Terminal(this, t);
}

Widget::~Widget() noexcept
try
{
        if (m_settings) {
                g_signal_handlers_disconnect_matched(m_settings.get(),
                                                     G_SIGNAL_MATCH_DATA,
                                                     0, 0, NULL, NULL,
                                                     this);
        }

        if (m_vadjustment) {
                g_signal_handlers_disconnect_by_func(m_vadjustment.get(),
                                                     (void*)vadjustment_value_changed_cb,
                                                     this);
        }

        if (m_menu_showing) {
                unset_context_menu(m_menu_showing.get(), true, false);
        }

        m_widget = nullptr;

        m_terminal->~Terminal();
        g_free(m_terminal);
}
catch (...)
{
        vte::log_exception();
}

void
Widget::beep() noexcept
{
        if (realized())
                gtk_widget_error_bell(gtk());
}

#if VTE_GTK == 4

bool
Widget::contains(double x,
                 double y)
{
        return false;
}

#endif /* VTE_GTK == 4 */

vte::glib::RefPtr<GdkCursor>
Widget::create_cursor(std::string const& name) const noexcept
{
#if VTE_GTK == 3
	return vte::glib::take_ref(gdk_cursor_new_from_name(gtk_widget_get_display(m_widget), name.c_str()));
#elif VTE_GTK == 4
        return vte::glib::take_ref(gdk_cursor_new_from_name(name.c_str(), nullptr /* fallback */));
#endif
}

void
Widget::set_cursor(GdkCursor* cursor) noexcept
{
#if VTE_GTK == 3
        gdk_window_set_cursor(m_event_window, cursor);
#elif VTE_GTK == 4
        gtk_widget_set_cursor(gtk(), cursor);
#endif
}

void
Widget::set_cursor(Cursor const& cursor) noexcept
{
        if (!realized())
                return;

        GdkCursor* gdk_cursor{nullptr};
        switch (cursor.index()) {
        case 0:
#if VTE_GTK == 3
                gdk_cursor = gdk_cursor_new_from_name(gtk_widget_get_display(gtk()),
                                                      std::get<0>(cursor).c_str());
#elif VTE_GTK == 4
                gdk_cursor = gdk_cursor_new_from_name(std::get<0>(cursor).c_str(),
                                                      nullptr /* fallback */);
#endif /* VTE_GTK */
                break;

        case 1:
                gdk_cursor = std::get<1>(cursor).get();
                if (gdk_cursor != nullptr
#if VTE_GTK == 3
                    && gdk_cursor_get_display(gdk_cursor) == gtk_widget_get_display(gtk())
#endif
                ) {
                        g_object_ref(gdk_cursor);
                } else {
                        gdk_cursor = nullptr;
                }
                break;

#if VTE_GTK == 3
        case 2:
                gdk_cursor = gdk_cursor_new_for_display(gtk_widget_get_display(gtk()), std::get<2>(cursor));
                break;
#endif
        }

        set_cursor(gdk_cursor);
        if (gdk_cursor)
                g_object_unref(gdk_cursor);
}

Clipboard&
Widget::clipboard_get(ClipboardType type) const
{
        switch (type) {
        case ClipboardType::CLIPBOARD: return *m_clipboard;
        case ClipboardType::PRIMARY: return *m_primary_clipboard;
        default: g_assert_not_reached(); throw std::runtime_error{""}; break;
        }
}

std::optional<std::string_view>
Widget::clipboard_data_get_cb(Clipboard const& clipboard,
                              ClipboardFormat format)
{
        return terminal()->widget_clipboard_data_get(clipboard, format);
}

void
Widget::clipboard_data_clear_cb(Clipboard const& clipboard)
{
        terminal()->widget_clipboard_data_clear(clipboard);
}

void
Widget::clipboard_request_received_cb(Clipboard const& clipboard,
                                      std::string_view const& text)
{
        terminal()->widget_paste(text);
}

void
Widget::clipboard_request_failed_cb(Clipboard const& clipboard)
{
        gtk_widget_error_bell(gtk());
}

void
Widget::clipboard_offer_data(ClipboardType type,
                             ClipboardFormat format) noexcept
{
        try {
                clipboard_get(type).offer_data(format,
                                               &Widget::clipboard_data_get_cb,
                                               &Widget::clipboard_data_clear_cb);
        } catch (...) {
                /* Let the caller know the request failed */
                terminal()->widget_clipboard_data_clear(clipboard_get(type));
        }
}

void
Widget::clipboard_request_text(ClipboardType type) noexcept
{
        try {
                clipboard_get(type).request_text(&Widget::clipboard_request_received_cb,
                                                 &Widget::clipboard_request_failed_cb);
        } catch (...) {
                /* Let the caller know the request failed */
                clipboard_request_failed_cb(clipboard_get(type));
        }
}

void
Widget::clipboard_set_text(ClipboardType type,
                           char const* str,
                           size_t size) noexcept
{
        clipboard_get(type).set_text(str, size);
}

#if VTE_GTK == 4

std::pair<bool, bool>
Widget::compute_expand()
{
        return {true, true};
}

#endif /* VTE_GTK == 4 */

void
Widget::constructed() noexcept
{
#if VTE_GTK == 3
        auto context = gtk_widget_get_style_context(m_widget);
        gtk_style_context_add_class (context, VTE_STYLE_CLASS_MONOSPACE);
#elif VTE_GTK == 4
        gtk_widget_add_css_class(gtk(), VTE_STYLE_CLASS_MONOSPACE);
#endif /* VTE_GTK */

#if VTE_GTK == 4

        connect_settings();

        /* Add event controllers */
        auto controller = vte::glib::take_ref(gtk_event_controller_key_new());
        g_signal_connect(controller.get(), "key-pressed",
                         G_CALLBACK(key_pressed_cb), this);
        g_signal_connect(controller.get(), "key-released",
                         G_CALLBACK(key_released_cb), this);
        g_signal_connect(controller.get(), "modifiers",
                         G_CALLBACK(key_modifiers_cb), this);
        gtk_event_controller_set_name(controller.get(), "vte-key-controller");
        gtk_widget_add_controller(m_widget, controller.release());

        controller = vte::glib::take_ref(gtk_event_controller_focus_new());
        g_signal_connect(controller.get(), "enter",
                         G_CALLBACK(focus_enter_cb), this);
        g_signal_connect(controller.get(), "leave",
                         G_CALLBACK(focus_leave_cb), this);
        gtk_event_controller_set_name(controller.get(), "vte-focus-controller");
        gtk_widget_add_controller(m_widget, controller.release());

        controller = vte::glib::take_ref(gtk_event_controller_motion_new());
        g_signal_connect(controller.get(), "enter",
                         G_CALLBACK(motion_enter_cb), this);
        g_signal_connect(controller.get(), "leave",
                         G_CALLBACK(motion_leave_cb), this);
        g_signal_connect(controller.get(), "motion",
                         G_CALLBACK(motion_motion_cb), this);
        g_signal_connect(controller.get(), "notify::is-pointer",
                         G_CALLBACK(motion_notify_is_pointer_cb), this);
        g_signal_connect(controller.get(), "notify::contains-pointer",
                         G_CALLBACK(motion_notify_contains_pointer_cb), this);
        gtk_event_controller_set_name(controller.get(), "vte-motion-controller");
        gtk_widget_add_controller(m_widget, controller.release());

        auto const scroll_flags = GtkEventControllerScrollFlags(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                                                                GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL);
        controller = vte::glib::take_ref(gtk_event_controller_scroll_new(scroll_flags));
        g_signal_connect(controller.get(), "scroll-begin",
                         G_CALLBACK(scroll_begin_cb), this);
        g_signal_connect(controller.get(), "scroll-end",
                         G_CALLBACK(scroll_end_cb), this);
        g_signal_connect(controller.get(), "scroll",
                         G_CALLBACK(scroll_scroll_cb), this);
        g_signal_connect(controller.get(), "decelerate",
                         G_CALLBACK(scroll_decelerate_cb), this);
        gtk_event_controller_set_name(controller.get(), "vte-scroll-controller");
        gtk_widget_add_controller(m_widget, controller.release());

        auto gesture = vte::glib::take_ref(gtk_gesture_click_new());
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture.get()), 0 /* any button */);
        gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(gesture.get()), true);
        g_signal_connect(gesture.get(), "pressed",
                         G_CALLBACK(click_pressed_cb), this);
        g_signal_connect(gesture.get(), "released",
                         G_CALLBACK(click_released_cb), this);
        g_signal_connect(gesture.get(), "stopped",
                         G_CALLBACK(click_stopped_cb), this);
        g_signal_connect(gesture.get(), "unpaired-release",
                         G_CALLBACK(click_unpaired_release_cb), this);
        gtk_event_controller_set_name(GTK_EVENT_CONTROLLER(gesture.get()), "vte-click-gesture");
        gtk_widget_add_controller(m_widget, GTK_EVENT_CONTROLLER(gesture.release()));

        gesture = vte::glib::take_ref(gtk_gesture_long_press_new());
        gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(gesture.get()), true);

        g_signal_connect(gesture.get(), "pressed",
                         G_CALLBACK(long_press_pressed_cb), this);
        g_signal_connect(gesture.get(), "cancelled",
                         G_CALLBACK(long_press_cancelled_cb), this);
        gtk_event_controller_set_name(GTK_EVENT_CONTROLLER(gesture.get()), "vte-long-press-gesture");
        gtk_widget_add_controller(m_widget, GTK_EVENT_CONTROLLER(gesture.release()));

#endif /* VTE_GTK == 4 */

#if VTE_GTK == 3
        /* Set the style as early as possible, before GTK+ starts
         * invoking various callbacks. This is needed in order to
         * compute the initial geometry correctly in presence of
         * non-default padding, see bug 787710.
         */
        style_updated();
#elif VTE_GTK == 4
        padding_changed();
#endif /* VTE_GTK  */
}

#if VTE_GTK == 4

void
Widget::css_changed(GtkCssStyleChange* change)
{
        /* This function is inefficient, since there's no public API
         * for GtkCssStyleChange to see what exactly changed, and if
         * we do need to queue the resize for it or not.
         */

        auto need_resize = padding_changed();

        m_terminal->widget_style_updated();

        if (need_resize)
                gtk_widget_queue_resize(gtk());
}

#endif /* VTE_GTK == 4 */

void
Widget::direction_changed(GtkTextDirection old_direction) noexcept
{
        // FIXME: does this need to feed to BiDi somehow?
}

void
Widget::dispose() noexcept
{
#if WITH_A11Y && VTE_GTK == 3
        m_terminal->set_accessible(nullptr);
#endif

        // Dismiss a showing context menu
        if (m_menu_showing) {
                unset_context_menu(m_menu_showing.get(), true, false);
        }

        if (m_terminal->terminate_child()) {
                int status = W_EXITCODE(0, SIGKILL);
                emit_child_exited(status);
        }
}

void
Widget::emit_child_exited(int status) noexcept
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `child-exited'.\n");
        g_signal_emit(object(), signals[SIGNAL_CHILD_EXITED], 0, status);
}

void
Widget::emit_eof() noexcept
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `eof'.\n");
        g_signal_emit(object(), signals[SIGNAL_EOF], 0);
}

bool
Widget::im_filter_keypress(KeyEvent const& event) noexcept
{
        // FIXMEchpe this can only be called when realized, so the m_im_context check is redundant
        return m_im_context &&
                gtk_im_context_filter_keypress(m_im_context.get(),
#if VTE_GTK == 3
                                               reinterpret_cast<GdkEventKey*>(event.platform_event())
#elif VTE_GTK == 4
                                               event.platform_event()
#endif
                                               );
}

#if VTE_GTK == 3

void
Widget::event_focus_in(GdkEventFocus *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus In");

#if VTE_GTK == 4
        if (!root_focused())
                return;
#endif

        m_terminal->widget_focus_in();
}

void
Widget::event_focus_out(GdkEventFocus *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus Out");

#if VTE_GTK == 4
        if (!root_focused())
                return;
#endif

        m_terminal->widget_focus_out();
}

bool
Widget::event_key_press(GdkEventKey *event)
{
        auto key_event = key_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Key press key=%x keycode=%x modifiers=%x\n",
                         key_event.keyval(), key_event.keycode(), key_event.modifiers());

        return m_terminal->widget_key_press(key_event);
}

bool
Widget::event_key_release(GdkEventKey *event)
{
        auto key_event = key_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Key release key=%x keycode=%x modifiers=%x\n",
                         key_event.keyval(), key_event.keycode(), key_event.modifiers());

        return m_terminal->widget_key_release(key_event);
}

bool
Widget::event_button_press(GdkEventButton *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Click press button=%d press_count=%d x=%.3f y=%.3f\n",
                         mouse_event.button_value(), mouse_event.press_count(),
                         mouse_event.x(), mouse_event.y());

        return m_terminal->widget_mouse_press(mouse_event);
}

bool
Widget::event_button_release(GdkEventButton *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Click release button=%d x=%.3f y=%.3f\n",
                         mouse_event.button_value(), mouse_event.x(), mouse_event.y());

        return m_terminal->widget_mouse_release(mouse_event);
}

void
Widget::event_enter(GdkEventCrossing *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Motion enter x=%.3f y=%.3f\n",
                         mouse_event.x(), mouse_event.y());

        m_terminal->widget_mouse_enter(mouse_event);
}

void
Widget::event_leave(GdkEventCrossing *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Motion leave x=%.3f y=%.3f\n",
                         mouse_event.x(), mouse_event.y());

        m_terminal->widget_mouse_leave(mouse_event);
}

bool
Widget::event_scroll(GdkEventScroll *event)
{
        if (auto const scroll_event = scroll_event_from_gdk(reinterpret_cast<GdkEvent*>(event))) {
                _vte_debug_print(VTE_DEBUG_EVENTS, "Scroll delta_x=%.3f delta_y=%.3f\n",
                                 scroll_event->dx(), scroll_event->dy());

                return m_terminal->widget_mouse_scroll(*scroll_event);
        }

        return false;
}

bool
Widget::event_motion_notify(GdkEventMotion *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Motion x=%.3f y=%.3f\n",
                         mouse_event.x(), mouse_event.y());

        return m_terminal->widget_mouse_motion(mouse_event);
}

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4

bool
Widget::event_key_pressed(GtkEventControllerKey* controller,
                          unsigned key,
                          unsigned keycode,
                          unsigned modifiers)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Key press key=%x keycode=%x modifiers=%x\n",
                         key, keycode, modifiers);

        auto event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
        if (!event)
                return false;

        return terminal()->widget_key_press(key_event_from_gdk(event));
}

void
Widget::event_key_released(GtkEventControllerKey* controller,
                           unsigned key,
                           unsigned keycode,
                           unsigned modifiers)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Key release key=%x keycode=%x modifiers=%x\n",
                         key, keycode, modifiers);

        auto event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
        if (!event)
                return;

        terminal()->widget_key_release(key_event_from_gdk(event));
}

bool
Widget::event_key_modifiers(GtkEventControllerKey* controller,
                            unsigned modifiers)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Key modifiers=%x\n", modifiers);

        return terminal()->widget_key_modifiers(modifiers);
}

void
Widget::gesture_long_press_pressed(GtkGestureLongPress* gesture,
                                   double x,
                                   double y)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Long Press gesture pressed x=%.3f y=%.3f\n", x, y);

        // FIXMEgtk4: could let Terminal have the event first

        if (show_context_menu(EventContext{x, y, true})) {
                gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        }
}

void
Widget::gesture_long_press_cancelled(GtkGestureLongPress* gesture)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Long Press gesture cancelled");
}

void
Widget::event_focus_enter(GtkEventControllerFocus* controller)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus In");

        terminal()->widget_focus_in();
}

void
Widget::event_focus_leave(GtkEventControllerFocus* controller)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus Out");

        terminal()->widget_focus_out();
}

void
Widget::event_motion_enter(GtkEventControllerMotion* controller,
                           double x,
                           double y)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Motion enter x=%.3f y=%.3f\n", x, y);

#if 0
        // FIXMEgtk4 this always returns nullptr, so how do we get the modifiers?
        auto event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
        if (!event)
                return;
#endif

        terminal()->widget_mouse_enter({nullptr, // event
                                        EventBase::Type::eMOUSE_MOTION,
                                        1, // press count,
                                        0, // gdk_event_get_modifier_state(event),
                                        MouseEvent::Button::eNONE,
                                        x, y});
}

void
Widget::event_motion_leave(GtkEventControllerMotion* controller)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Motion leave\n");

#if 0
        // FIXMEgtk4 this always returns nullptr, so how do we get the modifiers?
        auto event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
        if (!event)
                return;
#endif

        // FIXMEgtk4 how to get the coordinates here? GtkEventControllerMotion::update_pointer_focus
        // has them, but the signal doesn't carry them. File a gtk bug?
        terminal()->widget_mouse_leave({nullptr, // event
                                        EventBase::Type::eMOUSE_MOTION,
                                        1, // press count,
                                        0, // gdk_event_get_modifier_state(event),
                                        MouseEvent::Button::eNONE,
                                        -1, -1 /* FIXMEgtk4 bogus!!! */});
}

void
Widget::event_motion(GtkEventControllerMotion* controller,
                     double x,
                     double y)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Motion x=%.3f y=%.3f\n", x, y);

        auto event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
        if (!event)
                return;

        // FIXMEgtk4 could this also be a touch event??
        terminal()->widget_mouse_motion({nullptr, // event
                                         EventBase::Type::eMOUSE_MOTION,
                                         1, // press count
                                         gdk_event_get_modifier_state(event),
                                         MouseEvent::Button::eNONE,
                                         x, y});
}

void
Widget::event_motion_notify_is_pointer(GtkEventControllerMotion* controller)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Motion is-pointer now %s\n",
                         _vte_debug_tf(gtk_event_controller_motion_is_pointer(controller)));

        // FIXMEgtk4
}

void
Widget::event_motion_notify_contains_pointer(GtkEventControllerMotion* controller)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Motion contains-pointer now %s\n",
                         _vte_debug_tf(gtk_event_controller_motion_contains_pointer(controller)));
        // FIXMEgtk4
}

void
Widget::event_scroll_begin(GtkEventControllerScroll* controller)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Scroll begin\n");

        // FIXMEgtk4
}

bool
Widget::event_scroll(GtkEventControllerScroll* controller,
                     double dx,
                     double dy)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Scroll delta_x=%.3f delta_y=%.3f\n", dx, dy);

        auto event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
        if (!event)
                return false;

        return terminal()->widget_mouse_scroll({gdk_event_get_modifier_state(event),
                                                dx, dy});
}

void
Widget::event_scroll_end(GtkEventControllerScroll* controller)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Scroll end\n");

        // FIXMEgtk4
}

void
Widget::event_scroll_decelerate(GtkEventControllerScroll* controller,
                                double vx,
                                double vy)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Scroll decelerate v_x=%.3f v_y=%.3f\n", vx, vy);

        // FIXMEgtk4
}

void
Widget::gesture_click_pressed(GtkGestureClick* gesture,
                              int press_count,
                              double x,
                              double y)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Click gesture pressed press_count=%d x=%.3f y=%.3f\n",
                         press_count, x, y);

        // FIXMEgtk4 why does gtk4 not do that automatically?
        gtk_widget_grab_focus(gtk());

        auto const event = mouse_event_from_gesture_click(EventBase::Type::eMOUSE_PRESS,
                                                          gesture,
                                                          press_count,
                                                          x, y);
        if (terminal()->widget_mouse_press(event))
                gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
	// Note that we don't deny the sequence here in the |else| case, see issue #2630

        // FIXMEgtk4 GtkLabel does
        //        if (press_count >= 3)
        //                gtk_event_controller_reset(GTK_EVENT_CONTROLLER(gesture));
        // but this makes triple-click 'sticky'
}

void
Widget::gesture_click_released(GtkGestureClick* gesture,
                               int press_count,
                               double x,
                               double y)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Click gesture released press_count=%d x=%.3f y=%.3f\n",
                         press_count, x, y);

        // FIXMEgtk4 why does gtk4 not do that automatically?
        gtk_widget_grab_focus(gtk());

        auto const sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
        if (!gtk_gesture_handles_sequence(GTK_GESTURE(gesture), sequence)) // FIXMEgtk4 why!?
                return;

        auto const event = mouse_event_from_gesture_click(EventBase::Type::eMOUSE_RELEASE,
                                                          gesture,
                                                          press_count,
                                                          x, y);
        if (terminal()->widget_mouse_release(event))
                gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

void
Widget::gesture_click_stopped(GtkGestureClick* gesture)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Click gesture stopped\n");

        // FIXMEgtk4 what's the right thing to do here???
        // Should probably stop selection expansion mode, reset stored buttons, ...?
}

void
Widget::gesture_click_unpaired_release(GtkGestureClick* gesture,
                                       double x,
                                       double y,
                                       unsigned button,
                                       GdkEventSequence* sequence)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Click gesture unpaired release button=%d x=%.3f y=%.3f\n",
                         button, x, y);

        // FIXMEgtk4 what's the right thing to do here???

        // FIXMEgtk4 why does gtk4 not do that automatically?
        gtk_widget_grab_focus(gtk());

        if (!gtk_gesture_handles_sequence(GTK_GESTURE(gesture), sequence)) // why!?
                return;

        auto const event = mouse_event_from_gesture_click(EventBase::Type::eMOUSE_RELEASE,
                                                          gesture,
                                                          1, // press_count
                                                          x, y);
        if (terminal()->widget_mouse_release(event))
                gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

#endif /* VTE_GTK == 4 */

void
Widget::im_focus_in() noexcept
{
        gtk_im_context_focus_in(m_im_context.get());
}

void
Widget::im_focus_out() noexcept
{
        gtk_im_context_focus_out(m_im_context.get());
}

void
Widget::im_preedit_changed() noexcept
{
        auto str = vte::glib::StringPtr{};
        auto attrs = vte::Freeable<PangoAttrList>{};
        auto cursorpos = 0;
        gtk_im_context_get_preedit_string(m_im_context.get(),
                                          vte::glib::StringGetter{str},
                                          vte::get_freeable(attrs),
                                          &cursorpos);
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit changed (%s,%d).\n",
                         str.get(), cursorpos);

        if (str)
                m_terminal->im_preedit_changed(str.get(), cursorpos, std::move(attrs));
}

void
Widget::im_set_cursor_location(cairo_rectangle_int_t const* rect) noexcept
{
        gtk_im_context_set_cursor_location(m_im_context.get(), rect);
}

void
Widget::im_activate_osk() noexcept
{
#if VTE_GTK == 4
        gtk_im_context_activate_osk(m_im_context.get(), nullptr);
#endif
}

#if VTE_GTK == 3

unsigned
Widget::read_modifiers_from_gdk(GdkEvent* event) const noexcept
{
        /* Read the modifiers. See bug #663779 for more information on why we do this. */
        auto mods = GdkModifierType{};
        if (!gdk_event_get_state(event, &mods))
                return 0;

        /* HACK! Treat META as ALT; see bug #663779. */
        if (mods & GDK_META_MASK)
                mods = GdkModifierType(mods | GDK_MOD1_MASK);

        /* Map non-virtual modifiers to virtual modifiers (Super, Hyper, Meta) */
        auto display = gdk_window_get_display(gdk_event_get_window(event));
        auto keymap = gdk_keymap_get_for_display(display);
        gdk_keymap_add_virtual_modifiers(keymap, &mods);

        return unsigned(mods);
}

#endif /* VTE_GTK == 3 */

unsigned
Widget::key_event_translate_ctrlkey(KeyEvent const& event) const noexcept
{
	if (event.keyval() < 128)
		return event.keyval();

#if VTE_GTK == 3
        auto display = gdk_window_get_display(gdk_event_get_window(event.platform_event()));
        auto keymap = gdk_keymap_get_for_display(display);
        auto keyval = unsigned{event.keyval()};

	/* Try groups in order to find one mapping the key to ASCII */
	for (auto i = unsigned{0}; i < 4; i++) {
		auto consumed_modifiers = GdkModifierType{};
		gdk_keymap_translate_keyboard_state (keymap,
                                                     event.keycode(),
                                                     GdkModifierType(event.modifiers()),
                                                     i,
                                                     &keyval, NULL, NULL, &consumed_modifiers);
		if (keyval < 128) {
			_vte_debug_print (VTE_DEBUG_EVENTS,
                                          "ctrl+Key, group=%d de-grouped into keyval=0x%x\n",
                                          event.group(), keyval);
                        break;
		}
	}

        return keyval;

#elif VTE_GTK == 4
        auto const display = gdk_event_get_display(event.platform_event());

        /* Try groups in order to find one mapping the key to ASCII */
        for (auto i = unsigned{0}; i < 4; i++) {
                auto keyval = guint{};
                auto consumed_modifiers = GdkModifierType{};
                if (!gdk_display_translate_key(display,
                                               event.keycode(),
                                               GdkModifierType(event.modifiers()),
                                               i,
                                               &keyval,
                                               nullptr,
                                               nullptr,
                                               &consumed_modifiers))
                        continue;

                if (keyval >= 128)
                        continue;

                _vte_debug_print (VTE_DEBUG_EVENTS,
                                  "ctrl+Key, group=%d de-grouped into keyval=0x%x\n",
                                  event.group(), keyval);
                return keyval;
        }

        return event.keyval();
#endif /* VTE_GTK */
}

KeyEvent
Widget::key_event_from_gdk(GdkEvent* event) const
{
        auto type = EventBase::Type{};
        switch (gdk_event_get_event_type(event)) {
        case GDK_KEY_PRESS: type = KeyEvent::Type::eKEY_PRESS;     break;
        case GDK_KEY_RELEASE: type = KeyEvent::Type::eKEY_RELEASE; break;
        default: g_assert_not_reached(); return {};
        }

#if VTE_GTK == 3
        auto keyval = unsigned{};
        gdk_event_get_keyval(event, &keyval);
        auto const scancode = unsigned(reinterpret_cast<GdkEventKey*>(event)->hardware_keycode);
        auto const group = reinterpret_cast<GdkEventKey*>(event)->group;
        auto const is_modifier = reinterpret_cast<GdkEventKey*>(event)->is_modifier != 0;
#elif VTE_GTK == 4
        auto keyval = gdk_key_event_get_keyval(event);
        auto scancode = gdk_key_event_get_keycode(event);
        auto const group = gdk_key_event_get_level(event);
        auto const is_modifier = gdk_key_event_is_modifier(event) != false;
#endif /* VTE_GTK */

        return {event,
                type,
#if VTE_GTK == 3
                read_modifiers_from_gdk(event),
#elif VTE_GTK == 4
                gdk_event_get_modifier_state(event),
#endif
                keyval,
                scancode,
                group,
                is_modifier};
}

#if VTE_GTK == 3

MouseEvent
Widget::mouse_event_from_gdk(GdkEvent* event) const /* throws */
{
        auto type = EventBase::Type{};
        auto press_count = 0;
        switch (gdk_event_get_event_type(event)) {
        case GDK_2BUTTON_PRESS:
                type = MouseEvent::Type::eMOUSE_PRESS;
                press_count = 2;
                break;
        case GDK_3BUTTON_PRESS:
                type = MouseEvent::Type::eMOUSE_PRESS;
                press_count = 3;
                break;
        case GDK_BUTTON_PRESS:
                type = MouseEvent::Type::eMOUSE_PRESS;
                press_count = 1;
                break;
        case GDK_BUTTON_RELEASE:
                type = MouseEvent::Type::eMOUSE_RELEASE;
                press_count = 1;
                break;
        case GDK_ENTER_NOTIFY:   type = MouseEvent::Type::eMOUSE_ENTER;        break;
        case GDK_LEAVE_NOTIFY:   type = MouseEvent::Type::eMOUSE_LEAVE;        break;
        case GDK_MOTION_NOTIFY:  type = MouseEvent::Type::eMOUSE_MOTION;       break;
        case GDK_SCROLL:
                type = MouseEvent::Type::eMOUSE_SCROLL;
                press_count = 1;
                break;
        default:
                throw std::runtime_error{"Unexpected event type"};
        }

        auto x = double{};
        auto y = double{};
        if (gdk_event_get_window(event) != m_event_window ||
            !gdk_event_get_coords(event, &x, &y))
                x = y = -1.; // FIXMEchpe or throw?

        auto button = 0u;
        (void)gdk_event_get_button(event, &button);

        return {event,
                type,
                press_count,
                read_modifiers_from_gdk(event),
                MouseEvent::Button(button),
                x,
                y};
}

#endif /* VTE_GTK == 3 */

void
Widget::notify_char_size_changed(int width,
                                 int height)
{

        if (scroll_unit_is_pixels()) [[unlikely]] {
                /* When using pixels as adjustment values, changing the
                 * char size means we need to adjust the scroll bounds
                 * and value to keep the actual scroll position constant.
                 */
                notify_scroll_bounds_changed(true);
        }

	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `char-size-changed'.\n");
        /* FIXME on next API break, change the signature */
	g_signal_emit(gtk(), signals[SIGNAL_CHAR_SIZE_CHANGED], 0,
                      guint(width), guint(height));
}

void
Widget::notify_scroll_bounds_changed(bool value_changed)
{
        _vte_debug_print(VTE_DEBUG_ADJ,
                         "Updating scroll adjustment\n");

        auto const freezer = vte::glib::FreezeObjectNotify{m_vadjustment.get()};
        auto changed = false;

        auto const lower = terminal()->scroll_limit_lower();
        auto const upper = terminal()->scroll_limit_upper();
        auto dlower = 0.;
        auto dupper = double(upper - lower);
        auto dline = 1.;
        auto row_count = terminal()->row_count();
        if (scroll_unit_is_pixels()) [[unlikely]] {
                auto const factor = m_terminal->get_cell_height();
                dupper *= factor;
                dline *= factor;
                row_count *= factor;
        }

        auto current = gtk_adjustment_get_lower(m_vadjustment.get());
        if (!_vte_double_equal(current, dlower)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing lower bound from %.0f to %f\n",
                                 current, dlower);
                gtk_adjustment_set_lower(m_vadjustment.get(), dlower);
                changed = true;
        }

        current = gtk_adjustment_get_upper(m_vadjustment.get());
        if (!_vte_double_equal(current, dupper)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing upper bound from %.0f to %f\n",
                                 current, dupper);
                gtk_adjustment_set_upper(m_vadjustment.get(), dupper);
                changed = true;
        }

        /* The step increment should always be one. */
        current = gtk_adjustment_get_step_increment(m_vadjustment.get());
        if (!_vte_double_equal(current, dline)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing step increment from %.0lf to 1.0\n",
                                 current);
                gtk_adjustment_set_step_increment(m_vadjustment.get(), dline);
                changed = true;
        }

        current = gtk_adjustment_get_page_size(m_vadjustment.get());
        if (!_vte_double_equal(current, row_count)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing page size from %.0f to %ld\n",
                                 current, row_count);
                gtk_adjustment_set_page_size(m_vadjustment.get(), row_count);
                changed = true;
        }

        /* Clicking in the empty area should scroll exactly one screen,
         * so set the page size to the number of visible rows.
         */
        current = gtk_adjustment_get_page_increment(m_vadjustment.get());
        if (!_vte_double_equal(current, row_count)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing page increment from "
                                 "%.0f to %ld\n",
                                 current, row_count);
                gtk_adjustment_set_page_increment(m_vadjustment.get(), row_count);
                changed = true;
        }

        if (value_changed)
                notify_scroll_value_changed();

        if (changed)
                _vte_debug_print(VTE_DEBUG_SIGNALS,
                                 "Adjustment changed.\n");
}

void
Widget::notify_scroll_value_changed()
{
        _vte_debug_print(VTE_DEBUG_ADJ,
                         "Updating scroll adjustment value\n");

        auto const lower = terminal()->scroll_limit_lower();
        auto value = terminal()->scroll_position() - lower;
        if (scroll_unit_is_pixels()) [[unlikely]] {
                auto const factor = m_terminal->get_cell_height();
                value *= factor;
        }

        auto const v = gtk_adjustment_get_value(m_vadjustment.get());
        if (_vte_double_equal(v, value))
                return;

#if VTE_GTK == 4
        auto kinetic = false;
        GtkWidget* sw = nullptr;
        if (m_inside_scrolled_window) {
                // If a kinetic scroll is in progress in the containing
                // GtkScrolledWindow, it will continue even when we set
                // the new value.  GtkScrolledWindow lacks direct API to
                // stop kinetic scrolling, but it does stop when changing
                // the kinetic-scrolling property to false. So we unset
                // and then re-set kinetic-scrolling.

                sw = gtk_widget_get_ancestor(gtk(), GTK_TYPE_SCROLLED_WINDOW);
                kinetic = gtk_scrolled_window_get_kinetic_scrolling(GTK_SCROLLED_WINDOW(sw));
                if (kinetic)
                        gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(sw), false);
        }
#endif // VTE_GTK == 4

        m_changing_scroll_position = true;
        gtk_adjustment_set_value(m_vadjustment.get(), value);
        m_changing_scroll_position = false;

#if VTE_GTK == 4
        if (kinetic)
                gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(sw), true);
#endif // VTE_GTK == 4
}

void
Widget::notify_termprops_changed(int const* props,
                                 int n_props) noexcept
{
        m_in_termprops_changed_emission = true;

        auto retval = gboolean{};
        g_signal_emit(object(),
                      signals[SIGNAL_TERMPROPS_CHANGED],
                      0, /* detail */
                      props,
                      n_props,
                      &retval);

        m_in_termprops_changed_emission = false;
}

#if VTE_GTK == 3

std::optional<ScrollEvent>
Widget::scroll_event_from_gdk(GdkEvent* event) const
{
        /* Ignore emulated scroll events, see
         * https://gitlab.gnome.org/GNOME/vte/-/issues/2561
         */
        if (gdk_event_get_pointer_emulated(event))
                return std::nullopt;

        auto dx = double{}, dy = double{};
        if (!gdk_event_get_scroll_deltas(event, &dx, &dy)) {
                auto dir = GdkScrollDirection{};
                if (!gdk_event_get_scroll_direction(event, &dir))
                        __builtin_unreachable();

                switch (dir) {
                case GDK_SCROLL_UP:     dx =  0.; dy = -1.; break;
                case GDK_SCROLL_DOWN:   dx =  0.; dy =  1.; break;
                case GDK_SCROLL_LEFT:   dx = -1.; dy =  0.; break;
                case GDK_SCROLL_RIGHT:  dx =  1.; dy =  0.; break;
                case GDK_SCROLL_SMOOTH:
                default: __builtin_unreachable();
                }
        }


        return ScrollEvent{read_modifiers_from_gdk(event), dx, dy};
}

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4

MouseEvent
Widget::mouse_event_from_gesture_click(EventBase::Type type,
                                       GtkGestureClick* gesture,
                                       int press_count,
                                       double x,
                                       double y) const /* throws */
{
        auto const gesture_single = GTK_GESTURE_SINGLE(gesture);

        auto const button = gtk_gesture_single_get_current_button(gesture_single);
        auto const sequence = gtk_gesture_single_get_current_sequence(gesture_single);
        auto const event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), sequence);
        if (!event)
                throw std::runtime_error{"No last event!?"};

        return {event,
                type,
                press_count,
                gdk_event_get_modifier_state(event),
                MouseEvent::Button(button),
                x,
                y};
}

#endif /* VTE_GTK == 4 */

void
Widget::map() noexcept
{
#if VTE_GTK == 3
        if (m_event_window)
                gdk_window_show_unraised(m_event_window);
#endif
}

#if VTE_GTK == 4

void
Widget::measure(GtkOrientation orientation,
                int for_size,
                int* minimum,
                int* natural,
                int* minimum_baseline,
                int* natural_baseline) noexcept
{
        _vte_debug_print(VTE_DEBUG_WIDGET_SIZE, "Widget measure for_size=%d orientation=%s\n",
                         for_size,
                         orientation == GTK_ORIENTATION_HORIZONTAL ? "horizontal" : "vertical");

        switch (orientation) {
        case GTK_ORIENTATION_HORIZONTAL:
                terminal()->widget_measure_width(minimum, natural);
                break;
        case GTK_ORIENTATION_VERTICAL:
                *minimum_baseline = *natural_baseline = -1;
                terminal()->widget_measure_height(minimum, natural);
                break;
        }
}

#endif /* VTE_GTK == 4 */

bool
Widget::padding_changed() noexcept
{
        auto padding = GtkBorder{};
        auto const context = gtk_widget_get_style_context(gtk());
        gtk_style_context_get_padding(context,
#if VTE_GTK == 3
                                      gtk_style_context_get_state(context),
#endif
                                      &padding);

        /* FIXMEchpe: do we need to add the border from
         * gtk_style_context_get_border() to the padding?
         */

        return terminal()->set_style_border(padding);
}

bool
Widget::primary_paste_enabled() const noexcept
{
        auto primary_paste = gboolean{};
        g_object_get(m_settings.get(),
                     "gtk-enable-primary-paste", &primary_paste,
                     nullptr);

        return primary_paste != false;
}

bool
Widget::query_tooltip(int x,
                      int y,
                      bool keyboard,
                      GtkTooltip* tooltip) noexcept
{
        return false;
}

void
Widget::realize() noexcept
{
        //        m_mouse_cursor_over_widget = false;  /* We'll receive an enter_notify_event if the window appears under the cursor. */

	/* Create stock cursors */
	m_default_cursor = create_cursor(VTE_DEFAULT_CURSOR);
	m_invisible_cursor = create_cursor("none"s);
	m_mousing_cursor = create_cursor(VTE_MOUSING_CURSOR);
        if (_vte_debug_on(VTE_DEBUG_HYPERLINK))
                /* Differ from the standard regex match cursor in debug mode. */
                m_hyperlink_cursor = create_cursor(VTE_HYPERLINK_CURSOR_DEBUG);
        else
                m_hyperlink_cursor = create_cursor(VTE_HYPERLINK_CURSOR);

#if VTE_GTK == 3
	/* Create an input window for the widget. */
        auto allocation = m_terminal->get_allocated_rect();
	GdkWindowAttr attributes;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.visual = gtk_widget_get_visual(m_widget);
	attributes.event_mask =
                gtk_widget_get_events(m_widget) |
                GDK_EXPOSURE_MASK |
                GDK_FOCUS_CHANGE_MASK |
                GDK_SMOOTH_SCROLL_MASK |
                GDK_SCROLL_MASK |
                GDK_BUTTON_PRESS_MASK |
                GDK_BUTTON_RELEASE_MASK |
                GDK_POINTER_MOTION_MASK |
                GDK_BUTTON1_MOTION_MASK |
                GDK_ENTER_NOTIFY_MASK |
                GDK_LEAVE_NOTIFY_MASK |
                GDK_KEY_PRESS_MASK |
                GDK_KEY_RELEASE_MASK;
	attributes.cursor = m_default_cursor.get();
	guint attributes_mask =
                GDK_WA_X |
                GDK_WA_Y |
                (attributes.visual ? GDK_WA_VISUAL : 0) |
                GDK_WA_CURSOR;

	m_event_window = gdk_window_new(gtk_widget_get_parent_window (m_widget),
                                        &attributes, attributes_mask);
        gtk_widget_register_window(m_widget, m_event_window);
#endif /* VTE_GTK == 3 */

        assert(!m_im_context);
        m_im_context = vte::glib::take_ref(gtk_im_multicontext_new());
#if (VTE_GTK == 3 && GTK_CHECK_VERSION (3, 24, 14)) || VTE_GTK == 4
        g_object_set(m_im_context.get(),
                     "input-purpose", GTK_INPUT_PURPOSE_TERMINAL,
                     nullptr);
#endif

#if VTE_GTK == 3
	gtk_im_context_set_client_window(m_im_context.get(), m_event_window);
#elif VTE_GTK == 4
        gtk_im_context_set_client_widget(m_im_context.get(), gtk());
#endif
	g_signal_connect(m_im_context.get(), "commit",
			 G_CALLBACK(im_commit_cb), this);
	g_signal_connect(m_im_context.get(), "preedit-start",
			 G_CALLBACK(im_preedit_start_cb), this);
	g_signal_connect(m_im_context.get(), "preedit-changed",
			 G_CALLBACK(im_preedit_changed_cb), this);
	g_signal_connect(m_im_context.get(), "preedit-end",
			 G_CALLBACK(im_preedit_end_cb), this);
	g_signal_connect(m_im_context.get(), "retrieve-surrounding",
			 G_CALLBACK(im_retrieve_surrounding_cb), this);
	g_signal_connect(m_im_context.get(), "delete-surrounding",
			 G_CALLBACK(im_delete_surrounding_cb), this);
	gtk_im_context_set_use_preedit(m_im_context.get(), true);

        m_clipboard = std::make_shared<Clipboard>(*this, ClipboardType::CLIPBOARD);
        m_primary_clipboard = std::make_shared<Clipboard>(*this, ClipboardType::PRIMARY);

        m_terminal->widget_realize();
}

#if VTE_GTK == 4

void
Widget::root_surface_state_notify()
{
        auto const r = gtk_widget_get_root(gtk());
        auto const toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(r)));
        auto const new_state = toplevel ? gdk_toplevel_get_state(toplevel) : GdkToplevelState(0);
        auto const changed_mask = new_state ^ m_root_surface_state;

        m_root_surface_state = new_state;

        // If the widget is the focus widget in the toplevel, notify
        // the widget that it now has gained/lost the global focus
        if ((changed_mask & GDK_TOPLEVEL_STATE_FOCUSED) &&
            gtk_root_get_focus(r) == gtk()) {

                if (root_focused())
                        terminal()->widget_focus_in();
                else
                        terminal()->widget_focus_out();
        }
}

void
Widget::root_realize()
{
        if (m_root_surface_state_notify_id != 0)
                return;

        auto const r = gtk_widget_get_root(gtk());
        auto const toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(r)));
        m_root_surface_state_notify_id = g_signal_connect(toplevel,
                                                          "notify::state",
                                                          G_CALLBACK(root_surface_state_notify_cb),
                                                          this);

        root_surface_state_notify();
}

void
Widget::root_unrealize()
{
        root_surface_state_notify();
        m_root_surface_state = GdkToplevelState(0);

        if (m_root_surface_state_notify_id == 0)
                return;

        auto const r = gtk_widget_get_root(gtk());
        auto const toplevel = GDK_TOPLEVEL(gtk_native_get_surface(GTK_NATIVE(r)));
        g_signal_handler_disconnect(toplevel, m_root_surface_state_notify_id);
        m_root_surface_state_notify_id = 0;
}

void
Widget::root()
{
        auto const r = gtk_widget_get_root(gtk());
        m_root_realize_id = g_signal_connect(r,
                                             "realize",
                                             G_CALLBACK(root_realize_cb),
                                             this);
        m_root_unrealize_id = g_signal_connect(r,
                                               "unrealize",
                                               G_CALLBACK(root_unrealize_cb),
                                               this);

        // Find out if we're inside a GtkScrolledWindow
        auto const sw = gtk_widget_get_ancestor(gtk(), GTK_TYPE_SCROLLED_WINDOW);
        m_inside_scrolled_window = bool(sw);

        // Already realised?
        if (gtk_widget_get_realized(GTK_WIDGET(r)))
                root_realize();
}

#endif /* VTE_GTK == 4 */

#if VTE_GTK == 3

void
Widget::screen_changed(GdkScreen *previous_screen) noexcept
{
        auto gdk_screen = gtk_widget_get_screen(m_widget);
        if (gdk_screen == previous_screen || gdk_screen == nullptr)
                return;

        connect_settings();
}

#elif VTE_GTK == 4

void
Widget::display_changed() noexcept
{
        /* There appears to be no way to retrieve the previous display */
        connect_settings();
}

#endif /* VTE_GTK */

void
Widget::connect_settings()
{
        auto settings = vte::glib::make_ref(gtk_widget_get_settings(m_widget));
        if (settings == m_settings)
                return;

        if (m_settings)
                g_signal_handlers_disconnect_matched(m_settings.get(), G_SIGNAL_MATCH_DATA,
                                                     0, 0, nullptr, nullptr,
                                                     this);

        m_settings = std::move(settings);

        settings_changed();

        g_signal_connect(m_settings.get(), "notify::gtk-cursor-blink",
                         G_CALLBACK(settings_notify_cb), this);
        g_signal_connect(m_settings.get(), "notify::gtk-cursor-blink-time",
                         G_CALLBACK(settings_notify_cb), this);
        g_signal_connect(m_settings.get(), "notify::gtk-cursor-blink-timeout",
                         G_CALLBACK(settings_notify_cb), this);
#if VTE_GTK == 4
        g_signal_connect(m_settings.get(), "notify::gtk-cursor-aspect-ratio",
                         G_CALLBACK(settings_notify_cb), this);
#endif
}

void
Widget::settings_changed()
{
        auto blink = gboolean{};
        auto blink_time_ms = int{};
        auto blink_timeout_s = int{};
#if VTE_GTK == 4
        auto aspect = double{};
#endif
        g_object_get(m_settings.get(),
                     "gtk-cursor-blink", &blink,
                     "gtk-cursor-blink-time", &blink_time_ms,
                     "gtk-cursor-blink-timeout", &blink_timeout_s,
#if VTE_GTK == 4
                     "gtk-cursor-aspect-ratio", &aspect,
#endif
                     nullptr);

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Cursor blinking settings: blink=%d time=%d timeout=%d\n",
                         blink, blink_time_ms, blink_timeout_s * 1000);

        m_terminal->set_blink_settings(blink, blink_time_ms, blink_timeout_s * 1000);

#if VTE_GTK == 4
        m_terminal->set_cursor_aspect(aspect);
#endif
}

void
Widget::set_cursor(CursorType type) noexcept
{
        switch (type) {
        case CursorType::eDefault:   return set_cursor(m_default_cursor.get());
        case CursorType::eInvisible: return set_cursor(m_invisible_cursor.get());
        case CursorType::eMousing:   return set_cursor(m_mousing_cursor.get());
        case CursorType::eHyperlink: return set_cursor(m_hyperlink_cursor.get());
        }
}

void
Widget::set_hscroll_policy(GtkScrollablePolicy policy)
{
        m_hscroll_policy = policy;

#if VTE_GTK == 3
        gtk_widget_queue_resize_no_redraw(gtk());
#elif VTE_GTK == 4
        gtk_widget_queue_resize(gtk());
#endif
}

void
Widget::set_vscroll_policy(GtkScrollablePolicy policy)
{
        m_vscroll_policy = policy;

#if VTE_GTK == 3
        gtk_widget_queue_resize_no_redraw(gtk());
#elif VTE_GTK == 4
        gtk_widget_queue_resize(gtk());
#endif
}

bool
Widget::set_pty(VtePty* pty_obj) noexcept
{
        if (pty() == pty_obj)
                return false;

        m_pty = vte::glib::make_ref(pty_obj);
        terminal()->set_pty(_vte_pty_get_impl(pty()));

        return true;
}


static void
vadjustment_value_changed_cb(vte::platform::Widget* that) noexcept
try
{
        that->vadjustment_value_changed();
}
catch (...)
{
        vte::log_exception();
}

void
Widget::set_vadjustment(vte::glib::RefPtr<GtkAdjustment> adjustment)
{
        if (adjustment && adjustment == m_vadjustment)
                return;
        if (!adjustment && m_vadjustment)
                return;

        if (m_vadjustment) {
                g_signal_handlers_disconnect_by_func(m_vadjustment.get(),
                                                     (void*)vadjustment_value_changed_cb,
                                                     this);
        }

        if (adjustment)
                m_vadjustment = std::move(adjustment);
        else
                m_vadjustment = vte::glib::make_ref_sink(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0)));

        /* We care about the offset only, not the top or bottom. */
        g_signal_connect_swapped(m_vadjustment.get(),
                                 "value-changed",
                                 G_CALLBACK(vadjustment_value_changed_cb),
                                 this);
}

bool
Widget::set_word_char_exceptions(std::optional<std::string_view> stropt)
{
        if (m_word_char_exceptions == stropt)
                return false;

        if (terminal()->set_word_char_exceptions(stropt)) {
                m_word_char_exceptions = stropt;
                return true;
        }

        return false;
}

void
Widget::unset_pty() noexcept
{
        if (!pty())
                return;

        /* This is only called from Terminal, so we need
         * to explicitly notify the VteTerminal:pty property,
         * but we do NOT need to call Terminal::set_pty(nullptr).
         */
        m_pty.reset();
        g_object_notify_by_pspec(object(), pspecs[PROP_PTY]);
}

#if VTE_GTK == 3

void
Widget::size_allocate(GtkAllocation* allocation)
{
        _vte_debug_print(VTE_DEBUG_WIDGET_SIZE, "Widget size allocate width=%d height=%d x=%d y=%d\n",
                         allocation->width, allocation->height, allocation->x, allocation->y);

        m_terminal->widget_size_allocate(allocation->x, allocation->y,
                                         allocation->width, allocation->height,
                                         -1,
                                         vte::terminal::Terminal::Alignment(m_xalign),
                                         vte::terminal::Terminal::Alignment(m_yalign),
                                         m_xfill, m_yfill);

        gtk_widget_set_allocation(gtk(), allocation);

        if (realized())
		gdk_window_move_resize(m_event_window,
                                       allocation->x,
                                       allocation->y,
                                       allocation->width,
                                       allocation->height);
}

#elif VTE_GTK == 4

void
Widget::size_allocate(int width,
                      int height,
                      int baseline)
{
        _vte_debug_print(VTE_DEBUG_WIDGET_SIZE, "Widget size allocate width=%d height=%d baseline=%d\n",
                         width, height, baseline);

        terminal()->widget_size_allocate(width, height, baseline,
                                         vte::terminal::Terminal::Alignment(m_xalign),
                                         vte::terminal::Terminal::Alignment(m_yalign),
                                         m_xfill, m_yfill);

        // Need to size allocate the popup too
        if (m_menu_showing)
                gtk_popover_present(GTK_POPOVER(m_menu_showing.get()));
}

#endif /* VTE_GTK */

bool
Widget::should_emit_signal(int id) noexcept
{
        return g_signal_has_handler_pending(object(),
                                            signals[id],
                                            0 /* detail */,
                                            false /* not interested in blocked handlers */) != FALSE;
}

void
Widget::state_flags_changed(GtkStateFlags old_flags)
{
        _vte_debug_print(VTE_DEBUG_STYLE, "Widget state flags changed\n");
}

#if VTE_GTK == 3

void
Widget::style_updated() noexcept
{
        _vte_debug_print(VTE_DEBUG_STYLE, "Widget style changed\n");

        auto need_resize = padding_changed();

        auto aspect = float{};
        gtk_widget_style_get(gtk(), "cursor-aspect-ratio", &aspect, nullptr);
        m_terminal->set_cursor_aspect(aspect);

        m_terminal->widget_style_updated();

        if (need_resize)
                gtk_widget_queue_resize(m_widget);

}

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4

void
Widget::system_setting_changed(GtkSystemSetting setting)
{
        _vte_debug_print(VTE_DEBUG_STYLE, "Widget system settings %d changed\n", int(setting));

        switch (setting) {
        case GTK_SYSTEM_SETTING_DISPLAY:
                display_changed();
                break;

        case GTK_SYSTEM_SETTING_DPI:
                break;

        case GTK_SYSTEM_SETTING_FONT_CONFIG:
                break;

        case GTK_SYSTEM_SETTING_FONT_NAME:
                break;

        case GTK_SYSTEM_SETTING_ICON_THEME:
                break;

        default:
                break;
        }
}

#endif /* VTE_GTK == 4 */

void
Widget::unmap() noexcept
{
        m_terminal->widget_unmap();

#if VTE_GTK == 3
        if (m_event_window)
                gdk_window_hide(m_event_window);
#endif
}

void
Widget::unrealize() noexcept
{
        m_terminal->widget_unrealize();

        // FIXMEgtk4 only withdraw content from clipboard, not unselect?
        if (m_clipboard) {
                terminal()->widget_clipboard_data_clear(*m_clipboard);
                m_clipboard->disown();
                m_clipboard.reset();
        }
        if (m_primary_clipboard) {
                terminal()->widget_clipboard_data_clear(*m_primary_clipboard);
                m_primary_clipboard->disown();
                m_primary_clipboard.reset();
        }

        m_default_cursor.reset();
        m_invisible_cursor.reset();
        m_mousing_cursor.reset();
        m_hyperlink_cursor.reset();

	/* Shut down input methods. */
        assert(m_im_context);
        g_signal_handlers_disconnect_matched(m_im_context.get(),
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             this);
        m_terminal->im_preedit_reset();
#if VTE_GTK == 3
        gtk_im_context_set_client_window(m_im_context.get(), nullptr);
#elif VTE_GTK == 4
        gtk_im_context_set_client_widget(m_im_context.get(), nullptr);
#endif
        m_im_context.reset();

#if VTE_GTK == 3
        /* Destroy input window */
        gtk_widget_unregister_window(m_widget, m_event_window);
        gdk_window_destroy(m_event_window);
        m_event_window = nullptr;
#endif /* VTE_GTK == 3 */
}

#if VTE_GTK == 4

void
Widget::unroot()
{
        root_unrealize();

        auto const r = gtk_widget_get_root(gtk());
        g_signal_handler_disconnect(r, m_root_realize_id);
        m_root_realize_id = 0;
        g_signal_handler_disconnect(r, m_root_unrealize_id);
        m_root_unrealize_id = 0;

#if VTE_GTK == 4
        m_inside_scrolled_window = false;
#endif
}

void
Widget::context_menu_closed(GtkWidget* widget)
{
        if (!widget || m_menu_showing.get() != widget)
                return;

        // There is a design flaw in the gtk popover handling here
        // in that we cannot directly unset the context menu now,
        // because the selected action is resolved *after* this
        // function has run, and unsetting the parent will make
        // resolving the action fail.
        // So instead, we need to delay this to idle.
        // See https://gitlab.gnome.org/GNOME/vte/-/issues/2716

        if (m_context_menu_unset_on_idle_source != 0)
                return; // already scheduled

        m_context_menu_unset_on_idle_source =
                g_idle_add_once(GSourceOnceFunc(context_menu_unset_on_idle_cb),
                                this);
}

void
Widget::unset_context_menu_on_idle()
{
        m_context_menu_unset_on_idle_source = 0;

        // We can assume that the menu-to-unset is m_menu_showing,
        // since otherwise unset_context_menu() would have been called
        // directly, and that cancels the idle.
        unset_context_menu(m_menu_showing.get(), false, true);
}

#endif /* VTE_GTK == 4 */

void
Widget::vadjustment_value_changed()
{
        if (!m_terminal)
                return;

        if (m_changing_scroll_position)
                return;

        auto adj = gtk_adjustment_get_value(m_vadjustment.get());
        if (scroll_unit_is_pixels()) [[unlikely]] {
                auto const factor = m_terminal->get_cell_height();
                adj /= factor;
        }

        /* Add offset */
        auto const lower = terminal()->scroll_limit_lower();
        adj += lower;

        m_terminal->set_scroll_value(adj);
}

bool
Widget::set_context_menu(vte::glib::RefPtr<GtkWidget> menu)
{
        if (menu == m_context_menu)
                return false;

        if (m_context_menu) {
                g_signal_handlers_disconnect_matched(menu.get(),
                                                     G_SIGNAL_MATCH_DATA,
                                                     0, 0,
                                                     nullptr,
                                                     nullptr,
                                                     this);
        }

        m_context_menu = std::move(menu);
        return true;
}

void
Widget::emit_setup_context_menu(EventContext const* context)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Emitting setup-context-menu\n");
        g_signal_emit(object(), signals[SIGNAL_SETUP_CONTEXT_MENU], 0,
                      reinterpret_cast<VteEventContext const*>(context));
}

bool
Widget::show_context_menu(EventContext const& context)
{
        unset_context_menu(m_menu_showing.get(), true, false);

        // Let the embedder update or provide the menu model or menu
        emit_setup_context_menu(&context);

        // Create or get the menu
#if VTE_GTK == 3
        if (m_context_menu_model)
                m_menu_showing = vte::glib::make_ref_sink
                        (GTK_WIDGET(gtk_menu_new_from_model(m_context_menu_model.get())));
#elif VTE_GTK == 4
        if (m_context_menu_model)
                m_menu_showing = vte::glib::make_ref_sink
                        (GTK_WIDGET(gtk_popover_menu_new_from_model(G_MENU_MODEL(m_context_menu_model.get()))));
#endif // VTE_GTK
        else if (m_context_menu) {
                m_menu_showing = vte::glib::ref(m_context_menu);
        }

        _vte_debug_print(VTE_DEBUG_EVENTS, "Context menu is %p\n", (void*)m_menu_showing.get());

        if (!m_menu_showing)
                return false;

        gtk_style_context_add_class(gtk_widget_get_style_context(m_menu_showing.get()),
                                    "context-menu");

        auto const button = context.button();

#if VTE_GTK == 3

        g_object_set(G_OBJECT(m_menu_showing.get()),
                     "anchor-hints", GdkAnchorHints(GDK_ANCHOR_FLIP_Y),
                     "menu-type-hint", GdkWindowTypeHint(GDK_WINDOW_TYPE_HINT_POPUP_MENU),
                     nullptr);

        gtk_menu_attach_to_widget(GTK_MENU(m_menu_showing.get()), gtk(),
                                  GtkMenuDetachFunc(context_menu_detach_cb));

        g_signal_connect(m_menu_showing.get(), "selection-done",
                         G_CALLBACK(context_menu_selection_done_cb), this);

        if (button == -1) { // Keyboard

                auto const rect = terminal()->cursor_rect();
                gtk_menu_popup_at_rect(GTK_MENU(m_menu_showing.get()),
                                       event_window(),
                                       rect.cairo(),
                                       GDK_GRAVITY_SOUTH_WEST,
                                       GDK_GRAVITY_NORTH_WEST,
                                       context.platform_event());

                // Select first menu item
                gtk_menu_shell_select_first(GTK_MENU_SHELL(m_menu_showing.get()), true);

        } else { // Mouse
                gtk_menu_popup_at_pointer(GTK_MENU(m_menu_showing.get()),
                                          context.platform_event());
        }

#elif VTE_GTK == 4

        gtk_widget_set_parent(m_menu_showing.get(), gtk());

        auto const is_touch = context.is_long_press();

        if (is_touch)
                gtk_widget_set_halign(m_menu_showing.get(), GTK_ALIGN_FILL);
        else if (gtk_widget_get_direction(gtk()) == GTK_TEXT_DIR_RTL)
                gtk_widget_set_halign(m_menu_showing.get(), GTK_ALIGN_END);
        else
                gtk_widget_set_halign(m_menu_showing.get(), GTK_ALIGN_START);

        auto const popover = GTK_POPOVER(m_menu_showing.get());
        gtk_popover_set_autohide(popover, true);
        gtk_popover_set_cascade_popdown(popover, true);
        gtk_popover_set_has_arrow(popover, is_touch);
        gtk_popover_set_mnemonics_visible(popover, false);
        gtk_popover_set_position(popover, is_touch ? GTK_POS_TOP : GTK_POS_BOTTOM);

        if (button == -1) {
                // Keyboard, point to the cursor rectangle
                auto const rect = terminal()->cursor_rect().cairo();
                gtk_popover_set_pointing_to(popover, &rect);

                // Gtk apparently automatically selects the first sensitive
                // menu item in the popover.
        } else if (double x, y;
                   context.get_coords(&x, &y)) {
                auto rect = GdkRectangle{int(x), int(y), 0, 0};
                gtk_popover_set_pointing_to(popover, &rect);
        }

        g_signal_connect(m_menu_showing.get(), "closed",
                         G_CALLBACK(context_menu_closed_cb),
                         this);

        gtk_popover_popup(popover);
#endif // VTE_GTK

        return true;
}

} // namespace platform

} // namespace vte
