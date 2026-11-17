/*
 * Copyright © 2018 Christian Persch
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
#include <tuple>
#include <utility>
#include <variant>

#include "vteterminal.h"
#include "vtepty.h"

#include "vteinternal.hh"

#include "fwd.hh"

#include "clipboard-gtk.hh"
#include "regex.hh"
#include "refptr.hh"

namespace vte {

namespace terminal {

class Terminal;

} // namespace terminal

namespace platform {

class EventBase {
        friend class vte::platform::Widget;
        friend class Terminal;
        friend class EventContext;

public:
        enum class Type {
                eKEY_PRESS,
                eKEY_RELEASE,
                eMOUSE_ENTER,
                eMOUSE_LEAVE,
                eMOUSE_MOTION,
                eMOUSE_PRESS,
                eMOUSE_RELEASE,
                eMOUSE_SCROLL,
        };

protected:

        EventBase() noexcept = default;

        constexpr EventBase(Type type) noexcept
                : m_type{type}
        {
        }

public:
        ~EventBase() noexcept = default;

        EventBase(EventBase const&) = default;
        EventBase(EventBase&&) = default;
        EventBase& operator=(EventBase const&) = delete;
        EventBase& operator=(EventBase&&) = delete;

        constexpr auto type()        const noexcept { return m_type;        }

private:
        Type m_type;
}; // class EventBase

class KeyEvent : public EventBase {
        friend class vte::platform::Widget;
        friend class Terminal;
        friend class EventContext;

protected:

        KeyEvent() noexcept = default;

        constexpr KeyEvent(GdkEvent* gdk_event,
                           Type type,
                           unsigned modifiers,
                           unsigned keyval,
                           unsigned keycode,
                           unsigned group,
                           bool is_modifier) noexcept
                : EventBase{type},
                  m_platform_event{gdk_event},
                  m_modifiers{modifiers},
                  m_keyval{keyval},
                  m_keycode{keycode},
                  m_group{group},
                  m_is_modifier{is_modifier}
        {
        }

        constexpr auto platform_event() const noexcept { return m_platform_event; }

public:
        ~KeyEvent() noexcept = default;

        KeyEvent(KeyEvent const&) = delete;
        KeyEvent(KeyEvent&&) = delete;
        KeyEvent& operator=(KeyEvent const&) = delete;
        KeyEvent& operator=(KeyEvent&&) = delete;

        constexpr auto group()       const noexcept { return m_group;       }
        constexpr auto is_modifier() const noexcept { return m_is_modifier; }
        constexpr auto keycode()     const noexcept { return m_keycode;     }
        constexpr auto keyval()      const noexcept { return m_keyval;      }
        constexpr auto modifiers()   const noexcept { return m_modifiers;   }

        constexpr auto is_key_press()   const noexcept { return type() == Type::eKEY_PRESS;   }
        constexpr auto is_key_release() const noexcept { return type() == Type::eKEY_RELEASE; }

        bool matches(unsigned key,
                     unsigned mods) const noexcept
        {
#if VTE_GTK == 3
                return keyval() == key &&
                        (modifiers() & gtk_accelerator_get_default_mod_mask()) == mods;
#elif VTE_GTK == 4
                return gdk_key_event_matches(platform_event(),
                                             key, GdkModifierType(mods)) == GDK_KEY_MATCH_EXACT;
#endif
        }

private:
        GdkEvent* m_platform_event;
        unsigned m_modifiers;
        unsigned m_keyval;
        unsigned m_keycode;
        unsigned m_group;
        bool m_is_modifier;
}; // class KeyEvent

class MouseEvent : public EventBase {
        friend class vte::platform::Widget;
        friend class Terminal;
        friend class EventContext;

public:
        enum class Button {
                eNONE   = 0,
                eLEFT   = 1,
                eMIDDLE = 2,
                eRIGHT  = 3,
                eFOURTH = 4,
                eFIFTH  = 5,
        };

protected:

        MouseEvent() noexcept = default;

        constexpr MouseEvent(GdkEvent* gdk_event,
                             Type type,
                             int press_count,
                             unsigned modifiers,
                             Button button,
                             double x,
                             double y) noexcept
                : EventBase{type},
                  m_platform_event{gdk_event},
                  m_press_count{press_count},
                  m_modifiers{modifiers},
                  m_button{button},
                  m_x{x},
                  m_y{y}
        {
        }

        constexpr auto platform_event() const noexcept { return m_platform_event; }

public:
        ~MouseEvent() noexcept = default;

        MouseEvent(MouseEvent const&) = default;
        MouseEvent(MouseEvent&&) = default;
        MouseEvent& operator=(MouseEvent const&) = delete;
        MouseEvent& operator=(MouseEvent&&) = delete;

        constexpr auto button()       const noexcept { return m_button;           }
        constexpr auto button_value() const noexcept { return unsigned(m_button); }
        constexpr auto press_count()  const noexcept { return m_press_count;      }
        constexpr auto modifiers()    const noexcept { return m_modifiers;        }
        constexpr auto x()            const noexcept { return m_x;                }
        constexpr auto y()            const noexcept { return m_y;                }

        constexpr auto is_mouse_enter()        const noexcept { return type() == Type::eMOUSE_ENTER;        }
        constexpr auto is_mouse_leave()        const noexcept { return type() == Type::eMOUSE_LEAVE;        }
        constexpr auto is_mouse_motion()       const noexcept { return type() == Type::eMOUSE_MOTION;       }
        constexpr auto is_mouse_press()        const noexcept { return type() == Type::eMOUSE_PRESS;      }
        constexpr auto is_mouse_release()      const noexcept { return type() == Type::eMOUSE_RELEASE;      }

private:
        GdkEvent* m_platform_event;
        int m_press_count;
        unsigned m_modifiers;
        Button m_button;
        double m_x;
        double m_y;
}; // class MouseEvent

class ScrollEvent : public EventBase {
        friend class vte::platform::Widget;
        friend class Terminal;
        friend class EventContext;

protected:

        ScrollEvent() noexcept = default;

        constexpr ScrollEvent(unsigned modifiers,
                              double dx,
                              double dy) noexcept
                : EventBase{EventBase::Type::eMOUSE_SCROLL},
                  m_modifiers{modifiers},
                  m_dx{dx},
                  m_dy{dy}
        {
        }

public:
        ~ScrollEvent() noexcept = default;

        ScrollEvent(ScrollEvent const&) = default;
        ScrollEvent(ScrollEvent&&) = default;
        ScrollEvent& operator=(ScrollEvent const&) = delete;
        ScrollEvent& operator=(ScrollEvent&&) = delete;

        constexpr auto modifiers() const noexcept { return m_modifiers; }
        constexpr auto dx()        const noexcept { return m_dx;        }
        constexpr auto dy()        const noexcept { return m_dy;        }

private:
        unsigned m_modifiers;
        double m_dx;
        double m_dy;
}; // class ScrollEvent

class EventContext {
private:
        int m_button{-1};
#if VTE_GTK == 3
        GdkEvent* m_platform_event{nullptr}; // unowned
#elif VTE_GTK == 4
        bool m_xy_set{false};
        double m_x{0};
        double m_y{0};
        bool m_is_long_press{false};
#endif // VTE_GTK

public:
        ~EventContext() = default;

        EventContext(EventContext const&) = delete;
        EventContext(EventContext&&) = delete;

        EventContext& operator=(EventContext const&) = delete;
        EventContext& operator=(EventContext&&) = delete;

#if VTE_GTK == 3
        EventContext() noexcept :
                m_button{-1},
                m_platform_event{gtk_get_current_event()}
        {
        }
#elif VTE_GTK == 4
        constexpr EventContext() noexcept = default;
#endif

#if VTE_GTK == 4
        constexpr EventContext(double x,
                               double y,
                               bool is_long_press) :
                m_xy_set{true},
                m_x{x},
                m_y{y},
                m_is_long_press{is_long_press}
        {
        }
#endif // VTE_GTK == 4

        explicit constexpr EventContext(MouseEvent const& event) noexcept :
                m_button{int(event.button())},
#if VTE_GTK == 3
                m_platform_event{event.platform_event()}
#elif VTE_GTK == 4
                m_xy_set{true},
                m_x{event.x()},
                m_y{event.y()}
#endif
        {
        }

        explicit constexpr EventContext(KeyEvent const& event) noexcept :
                m_button{-1},
#if VTE_GTK == 3
                  m_platform_event{event.platform_event()}
#elif VTE_GTK == 4
                  m_xy_set{false}
#endif
        {
        }

        constexpr auto button()    const noexcept { return m_button;    }
#if VTE_GTK == 3
        constexpr auto platform_event() const noexcept { return m_platform_event; }
#elif VTE_GTK == 4
        constexpr auto xy_set() const noexcept { return m_xy_set; }
        constexpr auto x()      const noexcept { return m_x;      }
        constexpr auto y()      const noexcept { return m_y;      }
        constexpr auto get_coords(double* _x,
                                  double* _y) const noexcept
        {
                if (!xy_set())
                        return false;
                if (_x)
                        *_x = x();
                if (_y)
                        *_y = y();
                return true;
        }
        constexpr auto is_long_press() const noexcept { return m_is_long_press; }
#endif // VTE_GTK

}; // class EventContext

class Widget : public std::enable_shared_from_this<Widget> {
public:
        friend class vte::terminal::Terminal;

        static Widget* from_terminal(VteTerminal*);

        Widget(VteTerminal* t);
        ~Widget() noexcept;

        Widget(Widget const&) = delete;
        Widget(Widget&&) = delete;
        Widget& operator= (Widget const&) = delete;
        Widget& operator= (Widget&&) = delete;

        GObject* object() const noexcept { return reinterpret_cast<GObject*>(m_widget); }
        GtkWidget* gtk() const noexcept { return m_widget; }
        VteTerminal* vte() const noexcept { return reinterpret_cast<VteTerminal*>(m_widget); }

        inline constexpr vte::terminal::Terminal* terminal() const noexcept { return m_terminal; }

        inline int scale_factor() const noexcept { return gtk_widget_get_scale_factor(gtk()); }

        void constructed() noexcept;
        void dispose() noexcept;
        void realize() noexcept;
        void unrealize() noexcept;
        void map() noexcept;
        void unmap() noexcept;
        void state_flags_changed(GtkStateFlags old_flags);
        void direction_changed(GtkTextDirection old_direction) noexcept;
        bool query_tooltip(int x,
                           int y,
                           bool keyboard,
                           GtkTooltip* tooltip) noexcept;

        void connect_settings();
        bool padding_changed() noexcept;
        void settings_changed();

#if VTE_GTK == 3
        void style_updated() noexcept;
        void draw(cairo_t *cr) noexcept { m_terminal->widget_draw(cr); }
        void get_preferred_width(int *minimum_width,
                                 int *natural_width) const noexcept { m_terminal->widget_measure_width(minimum_width, natural_width); }
        void get_preferred_height(int *minimum_height,
                                  int *natural_height) const noexcept { m_terminal->widget_measure_height(minimum_height, natural_height); }
        void size_allocate(GtkAllocation *allocation);

        void event_focus_in(GdkEventFocus *event);
        void event_focus_out(GdkEventFocus *event);
        bool event_key_press(GdkEventKey *event);
        bool event_key_release(GdkEventKey *event);
        bool event_button_press(GdkEventButton *event);
        bool event_button_release(GdkEventButton *event);
        void event_enter(GdkEventCrossing *event);
        void event_leave(GdkEventCrossing *event);
        bool event_scroll(GdkEventScroll *event);
        bool event_motion_notify(GdkEventMotion *event);

        void screen_changed (GdkScreen *previous_screen) noexcept;
#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4
        void size_allocate(int width,
                           int height,
                           int baseline);
        void root();
        void unroot();
        void measure(GtkOrientation orientation,
                     int for_size,
                     int* minimum,
                     int* natural,
                     int* minimum_baseline,
                     int* natural_baseline) noexcept;
        std::pair<bool, bool> compute_expand();
        void css_changed(GtkCssStyleChange* change);
        void root_realize();
        void root_unrealize();
        void root_surface_state_notify();
        void root_surface_focused_changed();
        auto root_focused() const noexcept { return (m_root_surface_state & GDK_TOPLEVEL_STATE_FOCUSED) != 0; }
        void system_setting_changed(GtkSystemSetting setting);
        void snapshot(GtkSnapshot* snapshot) noexcept { terminal()->widget_snapshot(snapshot); }
        bool contains(double x,
                      double y);
        void display_changed() noexcept;

        bool event_key_pressed(GtkEventControllerKey* controller,
                               unsigned key,
                               unsigned keycode,
                               unsigned modifiers);
        void event_key_released(GtkEventControllerKey* controller,
                                unsigned key,
                                unsigned keycode,
                                unsigned modifiers);
        bool event_key_modifiers(GtkEventControllerKey* controller,
                                 unsigned modifiers);
        void event_focus_enter(GtkEventControllerFocus* controller);
        void event_focus_leave(GtkEventControllerFocus* controller);
        void event_motion_enter(GtkEventControllerMotion* controller,
                                double x,
                                double y);
        void event_motion_leave(GtkEventControllerMotion* controller);
        void event_motion(GtkEventControllerMotion* controller,
                          double x,
                          double y);
        void event_motion_notify_is_pointer(GtkEventControllerMotion* controller);
        void event_motion_notify_contains_pointer(GtkEventControllerMotion* controller);
        void event_scroll_begin(GtkEventControllerScroll* controller);
        bool event_scroll(GtkEventControllerScroll* controller,
                          double dx,
                          double dy);
        void event_scroll_end(GtkEventControllerScroll* controller);
        void event_scroll_decelerate(GtkEventControllerScroll* controller,
                                     double vx,
                                     double vy);
        void gesture_click_pressed(GtkGestureClick* gesture,
                                   int press_count,
                                   double x,
                                   double y);
        void gesture_click_released(GtkGestureClick* gesture,
                                    int press_count,
                                    double x,
                                    double y);
        void gesture_click_stopped(GtkGestureClick* gesture);
        void gesture_click_unpaired_release(GtkGestureClick* gesture,
                                            double x,
                                            double y,
                                            unsigned button,
                                            GdkEventSequence* sequence);
        void gesture_long_press_pressed(GtkGestureLongPress* gesture,
                                        double x,
                                        double y);
        void gesture_long_press_cancelled(GtkGestureLongPress* gesture);
#endif /* VTE_GTK == 4 */

        void grab_focus() noexcept { gtk_widget_grab_focus(gtk()); }

        bool primary_paste_enabled() const noexcept;

        Clipboard& clipboard_get(ClipboardType type) const;
        void clipboard_offer_data(ClipboardType type,
                                  ClipboardFormat format) noexcept;
        void clipboard_request_text(ClipboardType type) noexcept;
        void clipboard_set_text(ClipboardType type,
                                char const* str,
                                size_t size) noexcept;

        void paste_text(std::string_view const& text) { m_terminal->widget_paste(text); }
        void paste(vte::platform::ClipboardType type) { clipboard_request_text(type); }
        void copy(vte::platform::ClipboardType type,
                  vte::platform::ClipboardFormat format) noexcept { m_terminal->widget_copy(type, format); }

        void beep() noexcept;

        void set_hadjustment(vte::glib::RefPtr<GtkAdjustment> adjustment) noexcept { m_hadjustment = std::move(adjustment); }
        void set_vadjustment(vte::glib::RefPtr<GtkAdjustment> adjustment);
        auto hadjustment() noexcept { return m_hadjustment.get(); }
        auto vadjustment() noexcept { return m_vadjustment.get(); }
        void set_hscroll_policy(GtkScrollablePolicy policy);
        void set_vscroll_policy(GtkScrollablePolicy policy);
        auto hscroll_policy() const noexcept { return m_hscroll_policy; }
        auto vscroll_policy() const noexcept { return m_vscroll_policy; }

        constexpr bool set_scroll_unit_is_pixels(bool enable) noexcept
        {
                auto const rv = m_scroll_unit_is_pixels != enable;
                m_scroll_unit_is_pixels = enable;
                return rv;
        }

        constexpr auto scroll_unit_is_pixels() const noexcept { return m_scroll_unit_is_pixels; }

        bool set_cursor_blink_mode(VteCursorBlinkMode mode) { return terminal()->set_cursor_blink_mode(vte::terminal::Terminal::CursorBlinkMode(mode)); }
        auto cursor_blink_mode() const noexcept { return VteCursorBlinkMode(terminal()->cursor_blink_mode()); }

        bool set_cursor_shape(VteCursorShape shape) { return terminal()->set_cursor_shape(vte::terminal::Terminal::CursorShape(shape)); }
        auto cursor_shape() const noexcept { return VteCursorShape(terminal()->cursor_shape()); }

        bool set_backspace_binding(VteEraseBinding mode) { return terminal()->set_backspace_binding(vte::terminal::Terminal::EraseMode(mode)); }
        auto backspace_binding() const noexcept { return VteEraseBinding(terminal()->backspace_binding()); }

        bool set_delete_binding(VteEraseBinding mode) { return terminal()->set_delete_binding(vte::terminal::Terminal::EraseMode(mode)); }
        auto delete_binding() const noexcept { return VteEraseBinding(terminal()->delete_binding()); }

        bool set_text_blink_mode(VteTextBlinkMode mode) { return terminal()->set_text_blink_mode(vte::terminal::Terminal::TextBlinkMode(mode)); }
        auto text_blink_mode() const noexcept { return VteTextBlinkMode(terminal()->text_blink_mode()); }

        bool set_word_char_exceptions(std::optional<std::string_view> stropt);
        auto word_char_exceptions() const noexcept { return m_word_char_exceptions ? m_word_char_exceptions.value().c_str() : nullptr; }

        bool set_fallback_scrolling(bool set) { return terminal()->set_fallback_scrolling(set); }
        bool fallback_scrolling() const noexcept { return terminal()->fallback_scrolling(); }

        bool set_enable_legacy_osc777(bool enable) { return terminal()->set_enable_legacy_osc777(enable); }
        auto enable_legacy_osc777() const noexcept { return terminal()->enable_legacy_osc777(); }

        char const* encoding() const noexcept { return m_terminal->encoding(); }

        void emit_child_exited(int status) noexcept;
        void emit_eof() noexcept;

        bool set_pty(VtePty* pty) noexcept;
        inline auto pty() const noexcept { return m_pty.get(); }

        void feed(std::string_view const& str) { terminal()->feed(str); }
        void feed_child(std::string_view const& str) { terminal()->feed_child(str); }
        void feed_child_binary(std::string_view const& str) { terminal()->feed_child_binary(str); }

        char *regex_match_check(vte::grid::column_t column,
                                vte::grid::row_t row,
                                int* tag)
        {
                return terminal()->regex_match_check(column, row, tag);
        }

#if VTE_GTK == 3

        char* regex_match_check(GdkEvent* event,
                                int* tag)
        {
                return terminal()->regex_match_check(mouse_event_from_gdk(event), tag);
        }


        bool regex_match_check_extra(GdkEvent* event,
                                     vte::base::Regex const** regexes,
                                     size_t n_regexes,
                                     uint32_t match_flags,
                                     char** matches)
        {
                return terminal()->regex_match_check_extra(mouse_event_from_gdk(event),
                                                           regexes, n_regexes, match_flags, matches);
        }

        char* hyperlink_check(GdkEvent* event)
        {
                return terminal()->hyperlink_check(mouse_event_from_gdk(event));
        }

#elif VTE_GTK == 4

        char* regex_match_check_at(double x,
                                   double y,
                                   int* tag)
        {
                return terminal()->regex_match_check_at(x, y, tag);
        }

        bool regex_match_check_extra_at(double x,
                                        double y,
                                        vte::base::Regex const** regexes,
                                        size_t n_regexes,
                                        uint32_t match_flags,
                                        char** matches)
        {
                return terminal()->regex_match_check_extra_at(x, y,
                                                              regexes, n_regexes,
                                                              match_flags, matches);
        }

        char* hyperlink_check_at(double x,
                                 double y)
        {
                return terminal()->hyperlink_check_at(x, y);
        }

#endif /* VTE_GTK */

        bool should_emit_signal(int id) noexcept;

        constexpr auto xalign() const noexcept { return m_xalign; }
        constexpr auto yalign() const noexcept { return m_yalign; }
        constexpr auto xfill() const noexcept { return m_xfill; }
        constexpr auto yfill() const noexcept { return m_yfill; }

        bool set_xalign(VteAlign align) noexcept
        {
                if (align == m_xalign)
                        return false;

                m_xalign = VteAlign(align);
                gtk_widget_queue_allocate(gtk());
                return true;
        }

        bool set_yalign(VteAlign align) noexcept
        {
                if (align == m_yalign)
                        return false;

                m_yalign = align;
                gtk_widget_queue_allocate(gtk());
                return true;
        }

        bool set_xfill(bool fill) noexcept
        {
                if (fill == m_xfill)
                        return false;

                m_xfill = fill;
                gtk_widget_queue_allocate(gtk());
                return true;
        }

        bool set_yfill(bool fill) noexcept
        {
                if (fill == m_yfill)
                        return false;

                m_yfill = fill;
                gtk_widget_queue_allocate(gtk());
                return true;
        }

        bool set_context_menu_model(vte::glib::RefPtr<GMenuModel> model)
        {
                if (model == m_context_menu_model)
                        return false;

                m_context_menu_model = std::move(model);
                return true;
        }

        GMenuModel* get_context_menu_model() const noexcept
        {
                return m_context_menu_model.get();
        }

        bool set_context_menu(vte::glib::RefPtr<GtkWidget> menu);

        void unset_context_menu(GtkWidget* widget,
                                bool deactivate,
                                bool notify = true);

#if VTE_GTK == 4
        void unset_context_menu_on_idle();
        void context_menu_closed(GtkWidget* widget);
#endif

        GtkWidget* get_context_menu() const noexcept
        {
                return m_context_menu.get();
        }

        bool show_context_menu(EventContext const& context);

        void emit_setup_context_menu(EventContext const* context);

protected:

        enum class CursorType {
                eDefault,
                eInvisible,
                eMousing,
                eHyperlink
        };

#if VTE_GTK == 3
        GdkWindow* event_window() const noexcept { return m_event_window; }
#endif

        bool realized() const noexcept
        {
                return gtk_widget_get_realized(m_widget);
        }

        vte::glib::RefPtr<GdkCursor> create_cursor(std::string const& name) const noexcept;

        void set_cursor(CursorType type) noexcept;
        void set_cursor(GdkCursor* cursor) noexcept;
        void set_cursor(Cursor const& cursor) noexcept;

        bool im_filter_keypress(KeyEvent const& event) noexcept;

        void im_focus_in() noexcept;
        void im_focus_out() noexcept;
        void im_activate_osk() noexcept;

        void im_reset() noexcept
        {
                if (m_im_context)
                        gtk_im_context_reset(m_im_context.get());
        }

        void im_set_cursor_location(cairo_rectangle_int_t const* rect) noexcept;

        void unset_pty() noexcept;

        unsigned key_event_translate_ctrlkey(KeyEvent const& event) const noexcept;

        void notify_scroll_bounds_changed(bool value_changed = false);
        void notify_scroll_value_changed();
        void notify_char_size_changed(int width,
                                      int height);

        void notify_termprops_changed(int const* props,
                                      int n_props) noexcept;

public:

        void register_termprop(std::string_view const& name,
                               uint32_t id,
                               vte::terminal::TermpropType type)
        {
                vte::terminal::register_termprop(name, id, type);
        }

        auto get_termprop_info(std::string_view const& name) const
        {
                return vte::terminal::get_termprop_info(name);
        }

        auto get_termprop_info(int id) const
        {
                return vte::terminal::get_termprop_info(id);
        }

        auto get_termprop_info_checked(int id) const
        {
                auto const info = vte::terminal::get_termprop_info(id);

                return info &&
                        (!(unsigned(info->flags()) & unsigned(vte::terminal::TermpropFlags::EPHEMERAL)) ||
                         m_in_termprops_changed_emission) ? info : nullptr;
        }

        auto get_termprop(vte::terminal::TermpropInfo const& info) const
        {
                return terminal()->get_termprop(info);
        }

        void reset_termprop(vte::terminal::TermpropInfo const& info) const
        {
                terminal()->reset_termprop(info);
        }

        void set_no_legacy_signals() noexcept
        {
                m_no_legacy_signals = true;
                terminal()->set_no_legacy_signals();
        }

        bool get_no_legacy_signals() const noexcept
        {
                return m_no_legacy_signals;
        }

public: // FIXMEchpe
        void im_preedit_changed() noexcept;
        void vadjustment_value_changed();

private:
        KeyEvent key_event_from_gdk(GdkEvent* event) const;
#if VTE_GTK == 3
        unsigned read_modifiers_from_gdk(GdkEvent* event) const noexcept;
        MouseEvent mouse_event_from_gdk(GdkEvent* event) const /* throws */;
        std::optional<ScrollEvent> scroll_event_from_gdk(GdkEvent* event) const;
#elif VTE_GTK == 4
        MouseEvent mouse_event_from_gesture_click(EventBase::Type type,
                                                  GtkGestureClick* gesture,
                                                  int press_count,
                                                  double x,
                                                  double y) const /* throws */;
#endif

        void clipboard_request_received_cb(Clipboard const& clipboard,
                                           std::string_view const& text);
        void clipboard_request_failed_cb(Clipboard const& clipboard);

        std::optional<std::string_view> clipboard_data_get_cb(Clipboard const& clipboard,
                                                              ClipboardFormat format);
        void clipboard_data_clear_cb(Clipboard const& clipboard);

        GtkWidget* m_widget;

        vte::terminal::Terminal* m_terminal;

#if VTE_GTK == 3
        /* Event window */
        GdkWindow *m_event_window;
#endif

        vte::glib::RefPtr<GtkSettings> m_settings{nullptr};

        /* Cursors */
        vte::glib::RefPtr<GdkCursor> m_default_cursor;
        vte::glib::RefPtr<GdkCursor> m_invisible_cursor;
        vte::glib::RefPtr<GdkCursor> m_mousing_cursor;
        vte::glib::RefPtr<GdkCursor> m_hyperlink_cursor;

        /* Input method */
        vte::glib::RefPtr<GtkIMContext> m_im_context;

        /* PTY */
        vte::glib::RefPtr<VtePty> m_pty;

        /* Clipboard */
        std::shared_ptr<Clipboard> m_clipboard;
        std::shared_ptr<Clipboard> m_primary_clipboard;

        /* Misc */
        std::optional<std::string> m_word_char_exceptions{};

        vte::glib::RefPtr<GtkAdjustment> m_vadjustment{};
        vte::glib::RefPtr<GtkAdjustment> m_hadjustment{};

        unsigned m_hscroll_policy:1{GTK_SCROLL_NATURAL};
        unsigned m_vscroll_policy:1{GTK_SCROLL_NATURAL};
        unsigned m_scroll_unit_is_pixels:1{false};
        unsigned m_changing_scroll_position:1{false};

        VteAlign m_xalign{VTE_ALIGN_START};
        VteAlign m_yalign{VTE_ALIGN_START};
        bool m_xfill{true};
        bool m_yfill{true};

        bool m_no_legacy_signals{false};
        bool m_in_termprops_changed_emission{false};

#if VTE_GTK == 4
        GdkToplevelState m_root_surface_state{GdkToplevelState(0)};
        long m_root_realize_id{0};
        long m_root_unrealize_id{0};
        long m_root_surface_state_notify_id{0};
        bool m_inside_scrolled_window{false};
#endif /* VTE_GTK == 4 */

        vte::glib::RefPtr<GMenuModel> m_context_menu_model;
        vte::glib::RefPtr<GtkWidget> m_context_menu;
        vte::glib::RefPtr<GtkWidget> m_menu_showing;
#if VTE_GTK == 4
        unsigned m_context_menu_unset_on_idle_source{0};
#endif
};

} // namespace platform

} // namespace vte
