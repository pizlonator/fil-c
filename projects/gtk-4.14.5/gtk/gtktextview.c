/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtktextview.c Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtktextviewprivate.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "gtkaccessibletextprivate.h"
#include "gtkadjustmentprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssenumvalueprivate.h"
#include "gtkcsslineheightvalueprivate.h"
#include "gtkdebug.h"
#include "gtkdragsourceprivate.h"
#include "gtkdropcontrollermotion.h"
#include "gtkemojichooser.h"
#include "gtkimmulticontext.h"
#include "gtkjoinedmenuprivate.h"
#include "gtkmagnifierprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtknative.h"
#include "gtkpangoprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkscrollable.h"
#include "gtksettings.h"
#include "gtksnapshot.h"
#include "gtktextiterprivate.h"
#include "gtktexthandleprivate.h"
#include "gtktextviewchildprivate.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtktextbufferprivate.h"
#include "gtktextutilprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"

/**
 * GtkTextView:
 *
 * A widget that displays the contents of a [class@Gtk.TextBuffer].
 *
 * ![An example GtkTextview](multiline-text.png)
 *
 * You may wish to begin by reading the [conceptual overview](section-text-widget.html),
 * which gives an overview of all the objects and data types related to the
 * text widget and how they work together.
 *
 * ## CSS nodes
 *
 * ```
 * textview.view
 * ├── border.top
 * ├── border.left
 * ├── text
 * │   ╰── [selection]
 * ├── border.right
 * ├── border.bottom
 * ╰── [window.popup]
 * ```
 *
 * `GtkTextView` has a main css node with name textview and style class .view,
 * and subnodes for each of the border windows, and the main text area,
 * with names border and text, respectively. The border nodes each get
 * one of the style classes .left, .right, .top or .bottom.
 *
 * A node representing the selection will appear below the text node.
 *
 * If a context menu is opened, the window node will appear as a subnode
 * of the main node.
 *
 * ## Accessibility
 *
 * `GtkTextView` uses the %GTK_ACCESSIBLE_ROLE_TEXT_BOX role.
 */

/* How scrolling, validation, exposes, etc. work.
 *
 * The expose_event handler has the invariant that the onscreen lines
 * have been validated.
 *
 * There are two ways that onscreen lines can become invalid. The first
 * is to change which lines are onscreen. This happens when the value
 * of a scroll adjustment changes. So the code path begins in
 * gtk_text_view_value_changed() and goes like this:
 *   - gdk_surface_scroll() to reflect the new adjustment value
 *   - validate the lines that were moved onscreen
 *   - gdk_surface_process_updates() to handle the exposes immediately
 *
 * The second way is that you get the “invalidated” signal from the layout,
 * indicating that lines have become invalid. This code path begins in
 * invalidated_handler() and goes like this:
 *   - install high-priority idle which does the rest of the steps
 *   - if a scroll is pending from scroll_to_mark(), do the scroll,
 *     jumping to the gtk_text_view_value_changed() code path
 *   - otherwise, validate the onscreen lines
 *   - DO NOT process updates
 *
 * In both cases, validating the onscreen lines can trigger a scroll
 * due to maintaining the first_para on the top of the screen.
 * If validation triggers a scroll, we jump to the top of the code path
 * for value_changed, and bail out of the current code path.
 *
 * Also, in size_allocate, if we invalidate some lines from changing
 * the layout width, we need to go ahead and run the high-priority idle,
 * because GTK sends exposes right after doing the size allocates without
 * returning to the main loop. This is also why the high-priority idle
 * is at a higher priority than resizing.
 *
 */

#if 0
#define DEBUG_VALIDATION_AND_SCROLLING
#endif

#ifdef DEBUG_VALIDATION_AND_SCROLLING
#define DV(x) (x)
#else
#define DV(x)
#endif

#define SCREEN_WIDTH(widget) text_window_get_width (GTK_TEXT_VIEW (widget)->priv->text_window)
#define SCREEN_HEIGHT(widget) text_window_get_height (GTK_TEXT_VIEW (widget)->priv->text_window)

#define SPACE_FOR_CURSOR 1

typedef struct _GtkTextWindow GtkTextWindow;
typedef struct _GtkTextPendingScroll GtkTextPendingScroll;

enum
{
  TEXT_HANDLE_CURSOR,
  TEXT_HANDLE_SELECTION_BOUND,
  TEXT_HANDLE_N_HANDLES
};

struct _GtkTextViewPrivate
{
  GtkTextLayout *layout;
  GtkTextBuffer *buffer;

  guint blink_time;  /* time in msec the cursor has blinked since last user event */
  guint im_spot_idle;
  char *im_module;

  int dnd_x;
  int dnd_y;

  GtkTextHandle *text_handles[TEXT_HANDLE_N_HANDLES];
  GtkWidget *selection_bubble;
  guint selection_bubble_timeout_id;

  GtkWidget *magnifier_popover;
  GtkWidget *magnifier;

  GtkBorder border_window_size;
  GtkTextWindow *text_window;

  GQueue anchored_children;

  GtkTextViewChild *left_child;
  GtkTextViewChild *right_child;
  GtkTextViewChild *top_child;
  GtkTextViewChild *bottom_child;
  GtkTextViewChild *center_child;

  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  /* X offset between widget coordinates and buffer coordinates
   * taking left_padding in account
   */
  int xoffset;

  /* Y offset between widget coordinates and buffer coordinates
   * taking top_padding and top_margin in account
   */
  int yoffset;

  /* Width and height of the buffer */
  int width;
  int height;

  /* The virtual cursor position is normally the same as the
   * actual (strong) cursor position, except in two circumstances:
   *
   * a) When the cursor is moved vertically with the keyboard
   * b) When the text view is scrolled with the keyboard
   *
   * In case a), virtual_cursor_x is preserved, but not virtual_cursor_y
   * In case b), both virtual_cursor_x and virtual_cursor_y are preserved.
   */
  int virtual_cursor_x;   /* -1 means use actual cursor position */
  int virtual_cursor_y;   /* -1 means use actual cursor position */

  GtkTextMark *first_para_mark; /* Mark at the beginning of the first onscreen paragraph */
  int first_para_pixels;       /* Offset of top of screen in the first onscreen paragraph */

  guint64 blink_start_time;
  guint blink_tick;
  float cursor_alpha;

  guint scroll_timeout;

  guint first_validate_idle;        /* Idle to revalidate onscreen portion, runs before resize */
  guint incremental_validate_idle;  /* Idle to revalidate offscreen portions, runs after redraw */

  /* Mark for drop target */
  GtkTextMark *dnd_mark;

  /* Mark for selection of drag source */
  GtkTextMark *dnd_drag_begin_mark;
  GtkTextMark *dnd_drag_end_mark;

  GtkIMContext *im_context;
  GtkWidget *popup_menu;
  GMenuModel *extra_menu;

  GtkTextPendingScroll *pending_scroll;

  GtkGesture *drag_gesture;
  GtkEventController *key_controller;

  GtkCssNode *selection_node;

  GdkDrag *drag;

  /* Default style settings */
  int pixels_above_lines;
  int pixels_below_lines;
  int pixels_inside_wrap;
  GtkWrapMode wrap_mode;
  GtkJustification justify;

  int left_margin;
  int right_margin;
  int top_margin;
  int bottom_margin;
  int left_padding;
  int right_padding;
  int top_padding;
  int bottom_padding;

  int indent;

  guint32 obscured_cursor_timestamp;

  gint64 handle_place_time;
  PangoTabArray *tabs;

  guint editable : 1;

  guint overwrite_mode : 1;
  guint cursor_visible : 1;

  /* if we have reset the IM since the last character entered */
  guint need_im_reset : 1;

  guint accepts_tab : 1;

  /* debug flag - means that we've validated onscreen since the
   * last "invalidate" signal from the layout
   */
  guint onscreen_validated : 1;

  guint mouse_cursor_obscured : 1;

  guint scroll_after_paste : 1;

  guint text_handles_enabled : 1;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;
  guint cursor_handle_dragged : 1;
  guint selection_handle_dragged : 1;
};

struct _GtkTextPendingScroll
{
  GtkTextMark   *mark;
  double         within_margin;
  gboolean       use_align;
  double         xalign;
  double         yalign;
};

typedef enum
{
  SELECT_CHARACTERS,
  SELECT_WORDS,
  SELECT_LINES
} SelectionGranularity;

enum
{
  MOVE_CURSOR,
  PAGE_HORIZONTALLY,
  SET_ANCHOR,
  INSERT_AT_CURSOR,
  DELETE_FROM_CURSOR,
  BACKSPACE,
  CUT_CLIPBOARD,
  COPY_CLIPBOARD,
  PASTE_CLIPBOARD,
  TOGGLE_OVERWRITE,
  MOVE_VIEWPORT,
  SELECT_ALL,
  TOGGLE_CURSOR_VISIBLE,
  PREEDIT_CHANGED,
  EXTEND_SELECTION,
  INSERT_EMOJI,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_PIXELS_ABOVE_LINES,
  PROP_PIXELS_BELOW_LINES,
  PROP_PIXELS_INSIDE_WRAP,
  PROP_EDITABLE,
  PROP_WRAP_MODE,
  PROP_JUSTIFICATION,
  PROP_LEFT_MARGIN,
  PROP_RIGHT_MARGIN,
  PROP_TOP_MARGIN,
  PROP_BOTTOM_MARGIN,
  PROP_INDENT,
  PROP_TABS,
  PROP_CURSOR_VISIBLE,
  PROP_BUFFER,
  PROP_OVERWRITE,
  PROP_ACCEPTS_TAB,
  PROP_IM_MODULE,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  PROP_INPUT_PURPOSE,
  PROP_INPUT_HINTS,
  PROP_MONOSPACE,
  PROP_EXTRA_MENU
};

static GQuark quark_text_selection_data = 0;
static GQuark quark_gtk_signal = 0;
static GQuark quark_text_view_child = 0;

static void gtk_text_view_finalize             (GObject         *object);
static void gtk_text_view_set_property         (GObject         *object,
						guint            prop_id,
						const GValue    *value,
						GParamSpec      *pspec);
static void gtk_text_view_get_property         (GObject         *object,
						guint            prop_id,
						GValue          *value,
						GParamSpec      *pspec);
static void gtk_text_view_dispose              (GObject         *object);
static void gtk_text_view_measure (GtkWidget      *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline);
static void gtk_text_view_size_allocate        (GtkWidget           *widget,
                                                int                  width,
                                                int                  height,
                                                int                  baseline);
static void gtk_text_view_realize              (GtkWidget           *widget);
static void gtk_text_view_unrealize            (GtkWidget           *widget);
static void gtk_text_view_map                  (GtkWidget           *widget);
static void gtk_text_view_css_changed          (GtkWidget           *widget,
                                                GtkCssStyleChange   *change);
static void gtk_text_view_direction_changed    (GtkWidget        *widget,
                                                GtkTextDirection  previous_direction);
static void gtk_text_view_system_setting_changed (GtkWidget           *widget,
                                                  GtkSystemSetting     setting);
static void gtk_text_view_state_flags_changed  (GtkWidget        *widget,
					        GtkStateFlags     previous_state);

static void gtk_text_view_click_gesture_pressed (GtkGestureClick *gesture,
                                                 int                   n_press,
                                                 double                x,
                                                 double                y,
                                                 GtkTextView          *text_view);
static void gtk_text_view_click_gesture_released (GtkGestureClick *gesture,
                                                  int                   n_press,
                                                  double                x,
                                                  double                y,
                                                  GtkTextView          *text_view);
static void gtk_text_view_drag_gesture_update        (GtkGestureDrag *gesture,
                                                      double          offset_x,
                                                      double          offset_y,
                                                      GtkTextView    *text_view);
static void gtk_text_view_drag_gesture_end           (GtkGestureDrag *gesture,
                                                      double          offset_x,
                                                      double          offset_y,
                                                      GtkTextView    *text_view);

static gboolean gtk_text_view_key_controller_key_pressed  (GtkEventControllerKey *controller,
                                                           guint                  keyval,
                                                           guint                  keycode,
                                                           GdkModifierType        state,
                                                           GtkTextView           *text_view);
static void     gtk_text_view_key_controller_im_update    (GtkEventControllerKey *controller,
                                                           GtkTextView           *text_view);

static void gtk_text_view_focus_in             (GtkWidget            *widget);
static void gtk_text_view_focus_out            (GtkWidget            *widget);
static void gtk_text_view_motion               (GtkEventController *controller,
                                                double              x,
                                                double              y,
                                                gpointer            user_data);
static void gtk_text_view_snapshot             (GtkWidget        *widget,
                                                GtkSnapshot      *snapshot);
static void gtk_text_view_select_all           (GtkWidget        *widget,
                                                gboolean          select);
static gboolean get_middle_click_paste         (GtkTextView      *text_view);

static GtkTextBuffer* gtk_text_view_create_buffer (GtkTextView   *text_view);

/* Target side drag signals */
static void     gtk_text_view_drag_leave         (GtkDropTarget    *dest,
                                                  GtkTextView      *text_view);
static GdkDragAction
                gtk_text_view_drag_motion        (GtkDropTarget    *dest,
                                                  double            x,
                                                  double            y,
                                                  GtkTextView      *text_view);
static gboolean gtk_text_view_drag_drop          (GtkDropTarget    *dest,
                                                  const GValue     *value,
                                                  double            x,
                                                  double            y,
                                                  GtkTextView      *text_view);

static void gtk_text_view_popup_menu        (GtkWidget  *widget,
                                             const char *action_name,
                                             GVariant   *parameters);
static void gtk_text_view_move_cursor       (GtkTextView           *text_view,
                                             GtkMovementStep        step,
                                             int                    count,
                                             gboolean               extend_selection);
static void gtk_text_view_move_viewport     (GtkTextView           *text_view,
                                             GtkScrollStep          step,
                                             int                    count);
static void gtk_text_view_set_anchor       (GtkTextView           *text_view);
static gboolean gtk_text_view_scroll_pages (GtkTextView           *text_view,
                                            int                    count,
                                            gboolean               extend_selection);
static gboolean gtk_text_view_scroll_hpages(GtkTextView           *text_view,
                                            int                    count,
                                            gboolean               extend_selection);
static void gtk_text_view_insert_at_cursor (GtkTextView           *text_view,
                                            const char            *str);
static void gtk_text_view_delete_from_cursor (GtkTextView           *text_view,
                                              GtkDeleteType          type,
                                              int                    count);
static void gtk_text_view_backspace        (GtkTextView           *text_view);
static void gtk_text_view_cut_clipboard    (GtkTextView           *text_view);
static void gtk_text_view_copy_clipboard   (GtkTextView           *text_view);
static void gtk_text_view_paste_clipboard  (GtkTextView           *text_view);
static void gtk_text_view_toggle_overwrite (GtkTextView           *text_view);
static void gtk_text_view_toggle_cursor_visible (GtkTextView      *text_view);

static void gtk_text_view_unselect         (GtkTextView           *text_view);

static void     gtk_text_view_validate_onscreen     (GtkTextView        *text_view);
static void     gtk_text_view_get_first_para_iter   (GtkTextView        *text_view,
                                                     GtkTextIter        *iter);
static void     gtk_text_view_update_layout_width       (GtkTextView        *text_view);
static void     gtk_text_view_set_attributes_from_style (GtkTextView        *text_view,
                                                         GtkTextAttributes  *values);
static void     gtk_text_view_ensure_layout          (GtkTextView        *text_view);
static void     gtk_text_view_destroy_layout         (GtkTextView        *text_view);
static void     gtk_text_view_check_keymap_direction (GtkTextView        *text_view);
static void     gtk_text_view_start_selection_drag   (GtkTextView          *text_view,
                                                      const GtkTextIter    *iter,
                                                      SelectionGranularity  granularity,
                                                      gboolean              extends);
static gboolean gtk_text_view_end_selection_drag     (GtkTextView        *text_view);
static void     gtk_text_view_start_selection_dnd    (GtkTextView        *text_view,
                                                      const GtkTextIter  *iter,
                                                      GdkEvent           *event,
                                                      int                 x,
                                                      int                 y);
static void     gtk_text_view_check_cursor_blink     (GtkTextView        *text_view);
static void     gtk_text_view_pend_cursor_blink      (GtkTextView        *text_view);
static void     gtk_text_view_stop_cursor_blink      (GtkTextView        *text_view);
static void     gtk_text_view_reset_blink_time       (GtkTextView        *text_view);

static void     gtk_text_view_value_changed                (GtkAdjustment *adjustment,
							    GtkTextView   *view);
static void     gtk_text_view_commit_handler               (GtkIMContext  *context,
							    const char    *str,
							    GtkTextView   *text_view);
static void     gtk_text_view_commit_text                  (GtkTextView   *text_view,
                                                            const char    *text);
static void     gtk_text_view_preedit_start_handler        (GtkIMContext  *context,
                                                            GtkTextView   *text_view);
static void     gtk_text_view_preedit_changed_handler      (GtkIMContext  *context,
                                                            GtkTextView   *text_view);
static gboolean gtk_text_view_retrieve_surrounding_handler (GtkIMContext  *context,
							    GtkTextView   *text_view);
static gboolean gtk_text_view_delete_surrounding_handler   (GtkIMContext  *context,
							    int            offset,
							    int            n_chars,
							    GtkTextView   *text_view);

static void gtk_text_view_mark_set_handler       (GtkTextBuffer     *buffer,
                                                  const GtkTextIter *location,
                                                  GtkTextMark       *mark,
                                                  gpointer           data);
static void gtk_text_view_paste_done_handler     (GtkTextBuffer     *buffer,
                                                  GdkClipboard      *clipboard,
                                                  gpointer           data);
static void gtk_text_view_buffer_changed_handler (GtkTextBuffer     *buffer,
                                                  gpointer           data);
static void gtk_text_view_insert_text_handler    (GtkTextBuffer     *buffer,
                                                  GtkTextIter       *iter,
                                                  char              *text,
                                                  int                len,
                                                  gpointer           data);
static void gtk_text_view_delete_range_handler   (GtkTextBuffer     *buffer,
                                                  GtkTextIter       *start,
                                                  GtkTextIter       *end,
                                                  gpointer           data);
static void gtk_text_view_buffer_notify_redo     (GtkTextBuffer     *buffer,
                                                  GParamSpec        *pspec,
                                                  GtkTextView       *view);
static void gtk_text_view_buffer_notify_undo     (GtkTextBuffer     *buffer,
                                                  GParamSpec        *pspec,
                                                  GtkTextView       *view);
static void gtk_text_view_get_virtual_cursor_pos (GtkTextView       *text_view,
                                                  GtkTextIter       *cursor,
                                                  int               *x,
                                                  int               *y);
static void gtk_text_view_set_virtual_cursor_pos (GtkTextView       *text_view,
                                                  int                x,
                                                  int                y);

static void gtk_text_view_do_popup               (GtkTextView       *text_view,
						  GdkEvent          *event);

static void cancel_pending_scroll                (GtkTextView   *text_view);
static void gtk_text_view_queue_scroll           (GtkTextView   *text_view,
                                                  GtkTextMark   *mark,
                                                  double         within_margin,
                                                  gboolean       use_align,
                                                  double         xalign,
                                                  double         yalign);

static gboolean gtk_text_view_flush_scroll         (GtkTextView *text_view);
static void     gtk_text_view_update_adjustments   (GtkTextView *text_view);
static void     gtk_text_view_invalidate           (GtkTextView *text_view);
static void     gtk_text_view_flush_first_validate (GtkTextView *text_view);

static void     gtk_text_view_set_hadjustment        (GtkTextView   *text_view,
                                                      GtkAdjustment *adjustment);
static void     gtk_text_view_set_vadjustment        (GtkTextView   *text_view,
                                                      GtkAdjustment *adjustment);
static void     gtk_text_view_set_hadjustment_values (GtkTextView   *text_view);
static void     gtk_text_view_set_vadjustment_values (GtkTextView   *text_view);

static void gtk_text_view_update_im_spot_location (GtkTextView *text_view);
static void gtk_text_view_insert_emoji (GtkTextView *text_view);

static void update_node_ordering (GtkWidget    *widget);
static void gtk_text_view_update_pango_contexts (GtkTextView *text_view);

/* GtkTextHandle handlers */
static void gtk_text_view_handle_drag_started  (GtkTextHandle         *handle,
                                                GtkTextView           *text_view);
static void gtk_text_view_handle_dragged       (GtkTextHandle         *handle,
                                                int                    x,
                                                int                    y,
                                                GtkTextView           *text_view);
static void gtk_text_view_handle_drag_finished (GtkTextHandle         *handle,
                                                GtkTextView           *text_view);
static void gtk_text_view_update_handles       (GtkTextView           *text_view);

static void gtk_text_view_selection_bubble_popup_unset (GtkTextView *text_view);
static void gtk_text_view_selection_bubble_popup_set   (GtkTextView *text_view);

static gboolean gtk_text_view_extend_selection (GtkTextView            *text_view,
                                                GtkTextExtendSelection  granularity,
                                                const GtkTextIter      *location,
                                                GtkTextIter            *start,
                                                GtkTextIter            *end);
static void extend_selection (GtkTextView          *text_view,
                              SelectionGranularity  granularity,
                              const GtkTextIter    *location,
                              GtkTextIter          *start,
                              GtkTextIter          *end);


static void gtk_text_view_update_clipboard_actions (GtkTextView *text_view);
static void gtk_text_view_update_emoji_action      (GtkTextView *text_view);

static void gtk_text_view_activate_clipboard_cut        (GtkWidget  *widget,
                                                         const char *action_name,
                                                         GVariant   *parameter);
static void gtk_text_view_activate_clipboard_copy       (GtkWidget  *widget,
                                                         const char *action_name,
                                                         GVariant   *parameter);
static void gtk_text_view_activate_clipboard_paste      (GtkWidget  *widget,
                                                         const char *action_name,
                                                         GVariant   *parameter);
static void gtk_text_view_activate_selection_delete     (GtkWidget  *widget,
                                                         const char *action_name,
                                                         GVariant   *parameter);
static void gtk_text_view_activate_selection_select_all (GtkWidget  *widget,
                                                         const char *action_name,
                                                         GVariant   *parameter);
static void gtk_text_view_activate_misc_insert_emoji    (GtkWidget  *widget,
                                                         const char *action_name,
                                                         GVariant   *parameter);

static void gtk_text_view_real_undo (GtkWidget   *widget,
                                     const char *action_name,
                                     GVariant    *parameter);
static void gtk_text_view_real_redo (GtkWidget   *widget,
                                     const char *action_name,
                                     GVariant    *parameter);


/* FIXME probably need the focus methods. */

typedef struct
{
  GList               link;
  GtkWidget          *widget;
  GtkTextChildAnchor *anchor;
  int                 from_top_of_line;
  int                 from_left_of_buffer;
} AnchoredChild;

static AnchoredChild *anchored_child_new  (GtkWidget          *child,
                                           GtkTextChildAnchor *anchor,
                                           GtkTextLayout      *layout);
static void           anchored_child_free (AnchoredChild      *child);

struct _GtkTextWindow
{
  GtkTextWindowType type;
  GtkWidget *widget;
  GtkCssNode *css_node;
  GdkRectangle allocation;
};

static GtkTextWindow *text_window_new             (GtkWidget         *widget);
static void           text_window_free            (GtkTextWindow     *win);
static void           text_window_size_allocate   (GtkTextWindow     *win,
                                                   GdkRectangle      *rect);
static int            text_window_get_width       (GtkTextWindow     *win);
static int            text_window_get_height      (GtkTextWindow     *win);


static guint signals[LAST_SIGNAL] = { 0 };

static void     gtk_text_view_accessible_text_init (GtkAccessibleTextInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTextView, gtk_text_view, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkTextView)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_TEXT,
                                                gtk_text_view_accessible_text_init))

static GtkTextBuffer*
get_buffer (GtkTextView *text_view)
{
  if (text_view->priv->buffer == NULL)
    {
      GtkTextBuffer *b;
      b = GTK_TEXT_VIEW_GET_CLASS (text_view)->create_buffer (text_view);
      gtk_text_view_set_buffer (text_view, b);
      g_object_unref (b);
    }

  return text_view->priv->buffer;
}

#define UPPER_OFFSET_ANCHOR 0.8
#define LOWER_OFFSET_ANCHOR 0.2

static gboolean
check_scroll (double offset, GtkAdjustment *adjustment)
{
  if ((offset > UPPER_OFFSET_ANCHOR &&
       gtk_adjustment_get_value (adjustment) + gtk_adjustment_get_page_size (adjustment) < gtk_adjustment_get_upper (adjustment)) ||
      (offset < LOWER_OFFSET_ANCHOR &&
       gtk_adjustment_get_value (adjustment) > gtk_adjustment_get_lower (adjustment)))
    return TRUE;

  return FALSE;
}

static int
gtk_text_view_drop_motion_scroll_timeout (gpointer data)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;
  GtkTextIter newplace;
  double pointer_xoffset, pointer_yoffset;

  text_view = GTK_TEXT_VIEW (data);
  priv = text_view->priv;

  gtk_text_layout_get_iter_at_pixel (priv->layout,
                                     &newplace,
                                     priv->dnd_x + priv->xoffset,
                                     priv->dnd_y + priv->yoffset);

  gtk_text_buffer_move_mark (get_buffer (text_view), priv->dnd_mark, &newplace);

  pointer_xoffset = (double) priv->dnd_x / text_window_get_width (priv->text_window);
  pointer_yoffset = (double) priv->dnd_y / text_window_get_height (priv->text_window);

  if (check_scroll (pointer_xoffset, priv->hadjustment) ||
      check_scroll (pointer_yoffset, priv->vadjustment))
    {
      /* do not make offsets surpass lower nor upper anchors, this makes
       * scrolling speed relative to the distance of the pointer to the
       * anchors when it moves beyond them.
       */
      pointer_xoffset = CLAMP (pointer_xoffset, LOWER_OFFSET_ANCHOR, UPPER_OFFSET_ANCHOR);
      pointer_yoffset = CLAMP (pointer_yoffset, LOWER_OFFSET_ANCHOR, UPPER_OFFSET_ANCHOR);

      gtk_text_view_scroll_to_mark (text_view,
                                    priv->dnd_mark,
                                    0., TRUE, pointer_xoffset, pointer_yoffset);
    }

  return G_SOURCE_CONTINUE;
}

static void
gtk_text_view_drop_scroll_motion (GtkDropControllerMotion *motion,
                                  double                   x,
                                  double                   y,
                                  GtkTextView             *self)
{
  GtkTextViewPrivate *priv = self->priv;
  GdkRectangle target_rect;

  target_rect = priv->text_window->allocation;

  if (x < target_rect.x ||
      y < target_rect.y ||
      x > (target_rect.x + target_rect.width) ||
      y > (target_rect.y + target_rect.height))
    {
      priv->dnd_x = priv->dnd_y = -1;
      g_clear_handle_id (&priv->scroll_timeout, g_source_remove);
      return;
    }

  /* DnD uses text window coords, so subtract extra widget
   * coords that happen e.g. when displaying line numbers.
   */
  priv->dnd_x = x - target_rect.x;
  priv->dnd_y = y - target_rect.y;

  if (!priv->scroll_timeout)
  {
    priv->scroll_timeout = g_timeout_add (100, gtk_text_view_drop_motion_scroll_timeout, self);
    gdk_source_set_static_name_by_id (priv->scroll_timeout, "[gtk] gtk_text_view_drop_motion_scroll_timeout");
  }
}

static void
gtk_text_view_drop_scroll_leave (GtkDropControllerMotion *motion,
                                 GtkTextView             *self)
{
  GtkTextViewPrivate *priv = self->priv;

  priv->dnd_x = priv->dnd_y = -1;
  g_clear_handle_id (&priv->scroll_timeout, g_source_remove);
}

static void
add_move_binding (GtkWidgetClass *widget_class,
                  guint           keyval,
                  guint           modmask,
                  GtkMovementStep step,
                  int             count)
{
  g_assert ((modmask & GDK_SHIFT_MASK) == 0);

  gtk_widget_class_add_binding_signal (widget_class,
                                       keyval, modmask,
                                       "move-cursor",
                                       "(iib)", step, count, FALSE);

  /* Selection-extending version */
  gtk_widget_class_add_binding_signal (widget_class,
                                       keyval, modmask | GDK_SHIFT_MASK,
                                       "move-cursor",
                                       "(iib)", step, count, TRUE);
}

static void
gtk_text_view_notify (GObject    *object,
                      GParamSpec *pspec)
{
  if (pspec->name == I_("has-focus"))
    gtk_text_view_check_cursor_blink (GTK_TEXT_VIEW (object));

  if (G_OBJECT_CLASS (gtk_text_view_parent_class)->notify)
    G_OBJECT_CLASS (gtk_text_view_parent_class)->notify (object, pspec);
}

static void
gtk_text_view_class_init (GtkTextViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  /* Default handlers and virtual methods
   */
  gobject_class->set_property = gtk_text_view_set_property;
  gobject_class->get_property = gtk_text_view_get_property;
  gobject_class->finalize = gtk_text_view_finalize;
  gobject_class->dispose = gtk_text_view_dispose;
  gobject_class->notify = gtk_text_view_notify;

  widget_class->realize = gtk_text_view_realize;
  widget_class->unrealize = gtk_text_view_unrealize;
  widget_class->map = gtk_text_view_map;
  widget_class->css_changed = gtk_text_view_css_changed;
  widget_class->direction_changed = gtk_text_view_direction_changed;
  widget_class->system_setting_changed = gtk_text_view_system_setting_changed;
  widget_class->state_flags_changed = gtk_text_view_state_flags_changed;
  widget_class->measure = gtk_text_view_measure;
  widget_class->size_allocate = gtk_text_view_size_allocate;
  widget_class->snapshot = gtk_text_view_snapshot;

  klass->move_cursor = gtk_text_view_move_cursor;
  klass->set_anchor = gtk_text_view_set_anchor;
  klass->insert_at_cursor = gtk_text_view_insert_at_cursor;
  klass->delete_from_cursor = gtk_text_view_delete_from_cursor;
  klass->backspace = gtk_text_view_backspace;
  klass->cut_clipboard = gtk_text_view_cut_clipboard;
  klass->copy_clipboard = gtk_text_view_copy_clipboard;
  klass->paste_clipboard = gtk_text_view_paste_clipboard;
  klass->toggle_overwrite = gtk_text_view_toggle_overwrite;
  klass->create_buffer = gtk_text_view_create_buffer;
  klass->extend_selection = gtk_text_view_extend_selection;
  klass->insert_emoji = gtk_text_view_insert_emoji;

  /*
   * Properties
   */

  /**
   * GtkTextview:pixels-above-lines: (attributes org.gtk.Property.get=gtk_text_view_get_pixels_above_lines org.gtk.Property.set=gtk_text_view_set_pixels_above_lines)
   *
   * Pixels of blank space above paragraphs.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PIXELS_ABOVE_LINES,
                                   g_param_spec_int ("pixels-above-lines", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextview:pixels-below-lines: (attributes org.gtk.Property.get=gtk_text_view_get_pixels_below_lines org.gtk.Property.set=gtk_text_view_set_pixels_below_lines)
   *
   * Pixels of blank space below paragraphs.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PIXELS_BELOW_LINES,
                                   g_param_spec_int ("pixels-below-lines", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextview:pixels-inside-wrap: (attributes org.gtk.Property.get=gtk_text_view_get_pixels_inside_wrap org.gtk.Property.set=gtk_text_view_set_pixels_inside_wrap)
   *
   * Pixels of blank space between wrapped lines in a paragraph.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PIXELS_INSIDE_WRAP,
                                   g_param_spec_int ("pixels-inside-wrap", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextview:editable: (attributes org.gtk.Property.get=gtk_text_view_get_editable org.gtk.Property.set=gtk_text_view_set_editable)
   *
   * Whether the text can be modified by the user.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextview:wrap-mode: (attributes org.gtk.Property.get=gtk_text_view_get_wrap_mode org.gtk.Property.set=gtk_text_view_set_wrap_mode)
   *
   * Whether to wrap lines never, at word boundaries, or at character boundaries.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_WRAP_MODE,
                                   g_param_spec_enum ("wrap-mode", NULL, NULL,
                                                      GTK_TYPE_WRAP_MODE,
                                                      GTK_WRAP_NONE,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextview:justification: (attributes org.gtk.Property.get=gtk_text_view_get_justification org.gtk.Property.set=gtk_text_view_set_justification)
   *
   * Left, right, or center justification.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_JUSTIFICATION,
                                   g_param_spec_enum ("justification", NULL, NULL,
                                                      GTK_TYPE_JUSTIFICATION,
                                                      GTK_JUSTIFY_LEFT,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextView:left-margin: (attributes org.gtk.Property.get=gtk_text_view_get_left_margin org.gtk.Property.set=gtk_text_view_set_left_margin)
   *
   * The default left margin for text in the text view.
   *
   * Tags in the buffer may override the default.
   *
   * Note that this property is confusingly named. In CSS terms,
   * the value set here is padding, and it is applied in addition
   * to the padding from the theme.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LEFT_MARGIN,
                                   g_param_spec_int ("left-margin", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextView:right-margin: (attributes org.gtk.Property.get=gtk_text_view_get_right_margin org.gtk.Property.set=gtk_text_view_set_right_margin)
   *
   * The default right margin for text in the text view.
   *
   * Tags in the buffer may override the default.
   *
   * Note that this property is confusingly named. In CSS terms,
   * the value set here is padding, and it is applied in addition
   * to the padding from the theme.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_RIGHT_MARGIN,
                                   g_param_spec_int ("right-margin", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextView:top-margin: (attributes org.gtk.Property.get=gtk_text_view_get_top_margin org.gtk.Property.set=gtk_text_view_set_top_margin)
   *
   * The top margin for text in the text view.
   *
   * Note that this property is confusingly named. In CSS terms,
   * the value set here is padding, and it is applied in addition
   * to the padding from the theme.
   *
   * Don't confuse this property with [property@Gtk.Widget:margin-top].
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TOP_MARGIN,
                                   g_param_spec_int ("top-margin", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextView:bottom-margin: (attributes org.gtk.Property.get=gtk_text_view_get_bottom_margin org.gtk.Property.set=gtk_text_view_set_bottom_margin)
   *
   * The bottom margin for text in the text view.
   *
   * Note that this property is confusingly named. In CSS terms,
   * the value set here is padding, and it is applied in addition
   * to the padding from the theme.
   *
   * Don't confuse this property with [property@Gtk.Widget:margin-bottom].
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BOTTOM_MARGIN,
                                   g_param_spec_int ("bottom-margin", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextView:indent: (attributes org.gtk.Property.get=gtk_text_view_get_indent org.gtk.Property.set=gtk_text_view_set_indent)
   *
   * Amount to indent the paragraph, in pixels.
   *
   * A negative value of indent will produce a hanging indentation.
   * That is, the first line will have the full width, and subsequent
   * lines will be indented by the absolute value of indent.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_INDENT,
                                   g_param_spec_int ("indent", NULL, NULL,
                                                     G_MININT, G_MAXINT, 0,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextview:tabs: (attributes org.gtk.Property.get=gtk_text_view_get_tabs org.gtk.Property.set=gtk_text_view_set_tabs)
   *
   * Custom tabs for this text.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TABS,
                                   g_param_spec_boxed ("tabs", NULL, NULL,
                                                       PANGO_TYPE_TAB_ARRAY,
						       GTK_PARAM_READWRITE));

  /**
   * GtkTextView:cursor-visible: (attributes org.gtk.Property.get=gtk_text_view_get_cursor_visible org.gtk.Property.set=gtk_text_view_set_cursor_visible)
   *
   * If the insertion cursor is shown.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_VISIBLE,
                                   g_param_spec_boolean ("cursor-visible", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextView:buffer: (attributes org.gtk.Property.get=gtk_text_view_get_buffer org.gtk.Property.set=gtk_text_view_set_buffer)
   *
   * The buffer which is displayed.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BUFFER,
                                   g_param_spec_object ("buffer", NULL, NULL,
							GTK_TYPE_TEXT_BUFFER,
							GTK_PARAM_READWRITE));

  /**
   * GtkTextView:overwrite: (attributes org.gtk.Property.get=gtk_text_view_get_overwrite org.gtk.Property.set=gtk_text_view_set_overwrite)
   *
   * Whether entered text overwrites existing contents.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OVERWRITE,
                                   g_param_spec_boolean ("overwrite", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextView:accepts-tab: (attributes org.gtk.Property.get=gtk_text_view_get_accepts_tab org.gtk.Property.set=gtk_text_view_set_accepts_tab)
   *
   * Whether Tab will result in a tab character being entered.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEPTS_TAB,
                                   g_param_spec_boolean ("accepts-tab", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /**
    * GtkTextView:im-module:
    *
    * Which IM (input method) module should be used for this text_view.
    *
    * See [class@Gtk.IMMulticontext].
    *
    * Setting this to a non-%NULL value overrides the system-wide IM module
    * setting. See the GtkSettings [property@Gtk.Settings:gtk-im-module] property.
    */
   g_object_class_install_property (gobject_class,
                                    PROP_IM_MODULE,
                                    g_param_spec_string ("im-module", NULL, NULL,
                                                         NULL,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkTextView:input-purpose: (attributes org.gtk.Property.get=gtk_text_view_get_input_purpose org.gtk.Property.set=gtk_text_view_set_input_purpose)
   *
   * The purpose of this text field.
   *
   * This property can be used by on-screen keyboards and other input
   * methods to adjust their behaviour.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_INPUT_PURPOSE,
                                   g_param_spec_enum ("input-purpose", NULL, NULL,
                                                      GTK_TYPE_INPUT_PURPOSE,
                                                      GTK_INPUT_PURPOSE_FREE_FORM,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkTextView:input-hints: (attributes org.gtk.Property.get=gtk_text_view_get_input_hints org.gtk.Property.set=gtk_text_view_set_input_hints)
   *
   * Additional hints (beyond [property@Gtk.TextView:input-purpose])
   * that allow input methods to fine-tune their behaviour.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_INPUT_HINTS,
                                   g_param_spec_flags ("input-hints", NULL, NULL,
                                                       GTK_TYPE_INPUT_HINTS,
                                                       GTK_INPUT_HINT_NONE,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  /**
   * GtkTextView:monospace: (attributes org.gtk.Property.get=gtk_text_view_get_monospace org.gtk.Property.set=gtk_text_view_set_monospace)
   *
   * Whether text should be displayed in a monospace font.
   *
   * If %TRUE, set the .monospace style class on the
   * text view to indicate that a monospace font is desired.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MONOSPACE,
                                   g_param_spec_boolean ("monospace", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkTextView:extra-menu: (attributes org.gtk.Property.get=gtk_text_view_get_extra_menu org.gtk.Property.set=gtk_text_view_set_extra_menu)
   *
   * A menu model whose contents will be appended to the context menu.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_EXTRA_MENU,
                                   g_param_spec_object ("extra-menu", NULL, NULL,
                                                        G_TYPE_MENU_MODEL,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

   /* GtkScrollable interface */
   g_object_class_override_property (gobject_class, PROP_HADJUSTMENT,    "hadjustment");
   g_object_class_override_property (gobject_class, PROP_VADJUSTMENT,    "vadjustment");
   g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
   g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  /*
   * Signals
   */

  /**
   * GtkTextView::move-cursor:
   * @text_view: the object which received the signal
   * @step: the granularity of the move, as a `GtkMovementStep`
   * @count: the number of @step units to move
   * @extend_selection: %TRUE if the move should extend the selection
   *
   * Gets emitted when the user initiates a cursor movement.
   *
   * The ::move-cursor signal is a [keybinding signal](class.SignalAction.html).
   * If the cursor is not visible in @text_view, this signal causes
   * the viewport to be moved instead.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   *
   * The default bindings for this signal come in two variants,
   * the variant with the <kbd>Shift</kbd> modifier extends the
   * selection, the variant without it does not.
   * There are too many key combinations to list them all here.
   *
   * - <kbd>←</kbd>, <kbd>→</kbd>, <kbd>↑</kbd>, <kbd>↓</kbd>
   *   move by individual characters/lines
   * - <kbd>Ctrl</kbd>+<kbd>←</kbd>, etc. move by words/paragraphs
   * - <kbd>Home</kbd> and <kbd>End</kbd> move to the ends of the buffer
   * - <kbd>PgUp</kbd> and <kbd>PgDn</kbd> move vertically by pages
   * - <kbd>Ctrl</kbd>+<kbd>PgUp</kbd> and <kbd>Ctrl</kbd>+<kbd>PgDn</kbd>
   *   move horizontally by pages
   */
  signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT,
		  G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (signals[MOVE_CURSOR],
                              G_OBJECT_CLASS_TYPE (gobject_class),
                              _gtk_marshal_VOID__ENUM_INT_BOOLEANv);

  /**
   * GtkTextView::move-viewport:
   * @text_view: the object which received the signal
   * @step: the granularity of the movement, as a `GtkScrollStep`
   * @count: the number of @step units to move
   *
   * Gets emitted to move the viewport.
   *
   * The ::move-viewport signal is a [keybinding signal](class.SignalAction.html),
   * which can be bound to key combinations to allow the user to move the viewport,
   * i.e. change what part of the text view is visible in a containing scrolled
   * window.
   *
   * There are no default bindings for this signal.
   */
  signals[MOVE_VIEWPORT] =
    g_signal_new_class_handler (I_("move-viewport"),
                                G_OBJECT_CLASS_TYPE (gobject_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_text_view_move_viewport),
                                NULL, NULL,
                                _gtk_marshal_VOID__ENUM_INT,
                                G_TYPE_NONE, 2,
                                GTK_TYPE_SCROLL_STEP,
                                G_TYPE_INT);
  g_signal_set_va_marshaller (signals[MOVE_VIEWPORT],
                              G_OBJECT_CLASS_TYPE (gobject_class),
                              _gtk_marshal_VOID__ENUM_INTv);

  /**
   * GtkTextView::set-anchor:
   * @text_view: the object which received the signal
   *
   * Gets emitted when the user initiates settings the "anchor" mark.
   *
   * The ::set-anchor signal is a [keybinding signal](class.SignalAction.html)
   * which gets emitted when the user initiates setting the "anchor"
   * mark. The "anchor" mark gets placed at the same position as the
   * "insert" mark.
   *
   * This signal has no default bindings.
   */
  signals[SET_ANCHOR] =
    g_signal_new (I_("set-anchor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, set_anchor),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkTextView::insert-at-cursor:
   * @text_view: the object which received the signal
   * @string: the string to insert
   *
   * Gets emitted when the user initiates the insertion of a
   * fixed string at the cursor.
   *
   * The ::insert-at-cursor signal is a [keybinding signal](class.SignalAction.html).
   *
   * This signal has no default bindings.
   */
  signals[INSERT_AT_CURSOR] =
    g_signal_new (I_("insert-at-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, insert_at_cursor),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);

  /**
   * GtkTextView::delete-from-cursor:
   * @text_view: the object which received the signal
   * @type: the granularity of the deletion, as a `GtkDeleteType`
   * @count: the number of @type units to delete
   *
   * Gets emitted when the user initiates a text deletion.
   *
   * The ::delete-from-cursor signal is a [keybinding signal](class.SignalAction.html).
   *
   * If the @type is %GTK_DELETE_CHARS, GTK deletes the selection
   * if there is one, otherwise it deletes the requested number
   * of characters.
   *
   * The default bindings for this signal are <kbd>Delete</kbd> for
   * deleting a character, <kbd>Ctrl</kbd>+<kbd>Delete</kbd> for
   * deleting a word and <kbd>Ctrl</kbd>+<kbd>Backspace</kbd> for
   * deleting a word backwards.
   */
  signals[DELETE_FROM_CURSOR] =
    g_signal_new (I_("delete-from-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, delete_from_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_DELETE_TYPE,
		  G_TYPE_INT);
  g_signal_set_va_marshaller (signals[DELETE_FROM_CURSOR],
                              G_OBJECT_CLASS_TYPE (gobject_class),
                              _gtk_marshal_VOID__ENUM_INTv);

  /**
   * GtkTextView::backspace:
   * @text_view: the object which received the signal
   *
   * Gets emitted when the user asks for it.
   *
   * The ::backspace signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Backspace</kbd> and <kbd>Shift</kbd>+<kbd>Backspace</kbd>.
   */
  signals[BACKSPACE] =
    g_signal_new (I_("backspace"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, backspace),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkTextView::cut-clipboard:
   * @text_view: the object which received the signal
   *
   * Gets emitted to cut the selection to the clipboard.
   *
   * The ::cut-clipboard signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>x</kbd> and
   * <kbd>Shift</kbd>+<kbd>Delete</kbd>.
   */
  signals[CUT_CLIPBOARD] =
    g_signal_new (I_("cut-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, cut_clipboard),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkTextView::copy-clipboard:
   * @text_view: the object which received the signal
   *
   * Gets emitted to copy the selection to the clipboard.
   *
   * The ::copy-clipboard signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>c</kbd> and
   * <kbd>Ctrl</kbd>+<kbd>Insert</kbd>.
   */
  signals[COPY_CLIPBOARD] =
    g_signal_new (I_("copy-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, copy_clipboard),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkTextView::paste-clipboard:
   * @text_view: the object which received the signal
   *
   * Gets emitted to paste the contents of the clipboard
   * into the text view.
   *
   * The ::paste-clipboard signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>v</kbd> and
   * <kbd>Shift</kbd>+<kbd>Insert</kbd>.
   */
  signals[PASTE_CLIPBOARD] =
    g_signal_new (I_("paste-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, paste_clipboard),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkTextView::toggle-overwrite:
   * @text_view: the object which received the signal
   *
   * Gets emitted to toggle the overwrite mode of the text view.
   *
   * The ::toggle-overwrite signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is <kbd>Insert</kbd>.
   */
  signals[TOGGLE_OVERWRITE] =
    g_signal_new (I_("toggle-overwrite"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkTextViewClass, toggle_overwrite),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkTextView::select-all:
   * @text_view: the object which received the signal
   * @select: %TRUE to select, %FALSE to unselect
   *
   * Gets emitted to select or unselect the complete contents of the text view.
   *
   * The ::select-all signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>a</kbd> and
   * <kbd>Ctrl</kbd>+<kbd>/</kbd> for selecting and
   * <kbd>Shift</kbd>+<kbd>Ctrl</kbd>+<kbd>a</kbd> and
   * <kbd>Ctrl</kbd>+<kbd>\</kbd> for unselecting.
   */
  signals[SELECT_ALL] =
    g_signal_new_class_handler (I_("select-all"),
                                G_OBJECT_CLASS_TYPE (gobject_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_text_view_select_all),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  /**
   * GtkTextView::toggle-cursor-visible:
   * @text_view: the object which received the signal
   *
   * Gets emitted to toggle the `cursor-visible` property.
   *
   * The ::toggle-cursor-visible signal is a
   * [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is <kbd>F7</kbd>.
   */
  signals[TOGGLE_CURSOR_VISIBLE] =
    g_signal_new_class_handler (I_("toggle-cursor-visible"),
                                G_OBJECT_CLASS_TYPE (gobject_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gtk_text_view_toggle_cursor_visible),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);

  /**
   * GtkTextView::preedit-changed:
   * @text_view: the object which received the signal
   * @preedit: the current preedit string
   *
   * Emitted when preedit text of the active IM changes.
   *
   * If an input method is used, the typed text will not immediately
   * be committed to the buffer. So if you are interested in the text,
   * connect to this signal.
   *
   * This signal is only emitted if the text at the given position
   * is actually editable.
   */
  signals[PREEDIT_CHANGED] =
    g_signal_new_class_handler (I_("preedit-changed"),
                                G_OBJECT_CLASS_TYPE (gobject_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                NULL,
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1,
                                G_TYPE_STRING);

  /**
   * GtkTextView::extend-selection:
   * @text_view: the object which received the signal
   * @granularity: the granularity type
   * @location: the location where to extend the selection
   * @start: where the selection should start
   * @end: where the selection should end
   *
   * Emitted when the selection needs to be extended at @location.
   *
   * Returns: %GDK_EVENT_STOP to stop other handlers from being invoked for the
   *   event. %GDK_EVENT_PROPAGATE to propagate the event further.
   */
  signals[EXTEND_SELECTION] =
    g_signal_new (I_("extend-selection"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextViewClass, extend_selection),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__ENUM_BOXED_BOXED_BOXED,
                  G_TYPE_BOOLEAN, 4,
                  GTK_TYPE_TEXT_EXTEND_SELECTION,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals[EXTEND_SELECTION],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__ENUM_BOXED_BOXED_BOXEDv);

  /**
   * GtkTextView::insert-emoji:
   * @text_view: the object which received the signal
   *
   * Gets emitted to present the Emoji chooser for the @text_view.
   *
   * The ::insert-emoji signal is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>.</kbd> and
   * <kbd>Ctrl</kbd>+<kbd>;</kbd>
   */
  signals[INSERT_EMOJI] =
    g_signal_new (I_("insert-emoji"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextViewClass, insert_emoji),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /*
   * Actions
   */

  /**
   * GtkTextView|clipboard.cut:
   *
   * Copies the contents to the clipboard and deletes it from the widget.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.cut", NULL,
                                   gtk_text_view_activate_clipboard_cut);

  /**
   * GtkTextView|clipboard.copy:
   *
   * Copies the contents to the clipboard.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL,
                                   gtk_text_view_activate_clipboard_copy);

  /**
   * GtkTextView|clipboard.paste:
   *
   * Inserts the contents of the clipboard into the widget.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.paste", NULL,
                                   gtk_text_view_activate_clipboard_paste);

  /**
   * GtkTextView|selection.delete:
   *
   * Deletes the current selection.
   */
  gtk_widget_class_install_action (widget_class, "selection.delete", NULL,
                                   gtk_text_view_activate_selection_delete);

  /**
   * GtkTextView|selection.select-all:
   *
   * Selects all of the widgets content.
   */
  gtk_widget_class_install_action (widget_class, "selection.select-all", NULL,
                                   gtk_text_view_activate_selection_select_all);

  /**
   * GtkTextView|misc.insert-emoji:
   *
   * Opens the Emoji chooser.
   */
  gtk_widget_class_install_action (widget_class, "misc.insert-emoji", NULL,
                                   gtk_text_view_activate_misc_insert_emoji);

  /**
   * GtkTextView|text.undo:
   *
   * Undoes the last change to the contents.
   */
  gtk_widget_class_install_action (widget_class, "text.undo", NULL, gtk_text_view_real_undo);

  /**
   * GtkTextView|text.redo:
   *
   * Redoes the last change to the contents.
   */
  gtk_widget_class_install_action (widget_class, "text.redo", NULL, gtk_text_view_real_redo);

  /**
   * GtkTextView|menu.popup:
   *
   * Opens the context menu.
   */
  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, gtk_text_view_popup_menu);

  /*
   * Key bindings
   */

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_F10, GDK_SHIFT_MASK,
                                       "menu.popup",
                                       NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Menu, 0,
                                       "menu.popup",
                                       NULL);

  /* Moving the insertion point */
  add_move_binding (widget_class, GDK_KEY_Right, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Right, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, 1);

  add_move_binding (widget_class, GDK_KEY_Left, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Left, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (widget_class, GDK_KEY_Right, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (widget_class, GDK_KEY_Left, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (widget_class, GDK_KEY_Up, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Up, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, -1);

  add_move_binding (widget_class, GDK_KEY_Down, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Down, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, 1);

  add_move_binding (widget_class, GDK_KEY_Up, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_PARAGRAPHS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Up, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_PARAGRAPHS, -1);

  add_move_binding (widget_class, GDK_KEY_Down, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_PARAGRAPHS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Down, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_PARAGRAPHS, 1);

  add_move_binding (widget_class, GDK_KEY_Home, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Home, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_End, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_End, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_Home, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Home, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_End, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_End, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_Page_Up, 0,
                    GTK_MOVEMENT_PAGES, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Page_Up, 0,
                    GTK_MOVEMENT_PAGES, -1);

  add_move_binding (widget_class, GDK_KEY_Page_Down, 0,
                    GTK_MOVEMENT_PAGES, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Page_Down, 0,
                    GTK_MOVEMENT_PAGES, 1);

  add_move_binding (widget_class, GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_HORIZONTAL_PAGES, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Page_Up, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_HORIZONTAL_PAGES, -1);

  add_move_binding (widget_class, GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_HORIZONTAL_PAGES, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Page_Down, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_HORIZONTAL_PAGES, 1);

  /* Select all */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_a, GDK_CONTROL_MASK,
                                       "select-all",
                                       "(b)", TRUE);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_slash, GDK_CONTROL_MASK,
                                       "select-all",
                                       "(b)", TRUE);

  /* Unselect all */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_backslash, GDK_CONTROL_MASK,
                                       "select-all",
                                       "(b)", FALSE);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_a, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "select-all",
                                       "(b)", FALSE);

  /* Deleting text */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Delete, 0,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_CHARS, 1);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Delete, 0,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_CHARS, 1);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_BackSpace, 0,
                                       "backspace",
                                       NULL);

  /* Make this do the same as Backspace, to help with mis-typing */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_BackSpace, GDK_SHIFT_MASK,
                                       "backspace",
                                       NULL);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Delete, GDK_CONTROL_MASK,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_WORD_ENDS, 1);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Delete, GDK_CONTROL_MASK,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_WORD_ENDS, 1);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_BackSpace, GDK_CONTROL_MASK,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_WORD_ENDS, -1);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Delete, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_PARAGRAPH_ENDS, 1);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Delete, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_PARAGRAPH_ENDS, 1);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_BackSpace, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_PARAGRAPH_ENDS, -1);

  /* Cut/copy/paste */

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_x, GDK_CONTROL_MASK,
                                       "cut-clipboard",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_c, GDK_CONTROL_MASK,
                                       "copy-clipboard",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_v, GDK_CONTROL_MASK,
                                       "paste-clipboard",
                                       NULL);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Delete, GDK_SHIFT_MASK,
                                       "cut-clipboard",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Insert, GDK_CONTROL_MASK,
                                       "copy-clipboard",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Insert, GDK_SHIFT_MASK,
                                       "paste-clipboard",
                                       NULL);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Delete, GDK_SHIFT_MASK,
                                       "cut-clipboard",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Insert, GDK_CONTROL_MASK,
                                       "copy-clipboard",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Insert, GDK_SHIFT_MASK,
                                       "paste-clipboard",
                                       NULL);

  /* Undo/Redo */
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_z, GDK_CONTROL_MASK,
                                       "text.undo", NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_y, GDK_CONTROL_MASK,
                                       "text.redo", NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_z, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                       "text.redo", NULL);

  /* Overwrite */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Insert, 0,
                                       "toggle-overwrite",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Insert, 0,
                                       "toggle-overwrite",
                                       NULL);

  /* Emoji */
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_period, GDK_CONTROL_MASK,
                                       "misc.insert-emoji",
                                       NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_semicolon, GDK_CONTROL_MASK,
                                       "misc.insert-emoji",
                                       NULL);

  /* Caret mode */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_F7, 0,
                                       "toggle-cursor-visible",
                                       NULL);

  /* Control-tab focus motion */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Tab, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)", GTK_DIR_TAB_FORWARD);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Tab, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)", GTK_DIR_TAB_FORWARD);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Tab, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)", GTK_DIR_TAB_BACKWARD);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_KP_Tab, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)", GTK_DIR_TAB_BACKWARD);

  gtk_widget_class_set_css_name (widget_class, I_("textview"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TEXT_BOX);

  quark_text_selection_data = g_quark_from_static_string ("gtk-text-view-text-selection-data");
  quark_gtk_signal = g_quark_from_static_string ("gtk-signal");
  quark_text_view_child = g_quark_from_static_string ("gtk-text-view-child");
}

static void
_gtk_text_view_ensure_text_handles (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;
  int i;

  for (i = 0; i < TEXT_HANDLE_N_HANDLES; i++)
    {
      if (priv->text_handles[i])
	continue;
      priv->text_handles[i] = gtk_text_handle_new (GTK_WIDGET (text_view));
      g_signal_connect (priv->text_handles[i], "drag-started",
                        G_CALLBACK (gtk_text_view_handle_drag_started), text_view);
      g_signal_connect (priv->text_handles[i], "handle-dragged",
                        G_CALLBACK (gtk_text_view_handle_dragged), text_view);
      g_signal_connect (priv->text_handles[i], "drag-finished",
                        G_CALLBACK (gtk_text_view_handle_drag_finished), text_view);
    }
}

static void
gtk_text_view_init (GtkTextView *text_view)
{
  GtkWidget *widget = GTK_WIDGET (text_view);
  GtkDropTarget *dest;
  GtkTextViewPrivate *priv;
  GtkEventController *controller;
  GtkGesture *gesture;

  text_view->priv = gtk_text_view_get_instance_private (text_view);
  priv = text_view->priv;

  gtk_widget_set_focusable (widget, TRUE);
  gtk_widget_set_overflow (widget, GTK_OVERFLOW_HIDDEN);

  gtk_widget_add_css_class (widget, "view");

  gtk_widget_set_cursor_from_name (widget, "text");

  /* Set up default style */
  priv->wrap_mode = GTK_WRAP_NONE;
  priv->pixels_above_lines = 0;
  priv->pixels_below_lines = 0;
  priv->pixels_inside_wrap = 0;
  priv->justify = GTK_JUSTIFY_LEFT;
  priv->indent = 0;
  priv->tabs = NULL;
  priv->editable = TRUE;
  priv->cursor_alpha = 1.0;

  priv->scroll_after_paste = FALSE;

  dest = gtk_drop_target_new (G_TYPE_STRING, GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (dest, "enter", G_CALLBACK (gtk_text_view_drag_motion), text_view);
  g_signal_connect (dest, "motion", G_CALLBACK (gtk_text_view_drag_motion), text_view);
  g_signal_connect (dest, "leave", G_CALLBACK (gtk_text_view_drag_leave), text_view);
  g_signal_connect (dest, "drop", G_CALLBACK (gtk_text_view_drag_drop), text_view);
  gtk_widget_add_controller (GTK_WIDGET (text_view), GTK_EVENT_CONTROLLER (dest));

  controller = gtk_drop_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_text_view_drop_scroll_motion), text_view);
  g_signal_connect (controller, "motion", G_CALLBACK (gtk_text_view_drop_scroll_motion), text_view);
  g_signal_connect (controller, "leave", G_CALLBACK (gtk_text_view_drop_scroll_leave), text_view);
  gtk_widget_add_controller (GTK_WIDGET (text_view), controller);

  priv->virtual_cursor_x = -1;
  priv->virtual_cursor_y = -1;

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize ().
   */
  priv->im_context = gtk_im_multicontext_new ();

  g_signal_connect (priv->im_context, "commit",
                    G_CALLBACK (gtk_text_view_commit_handler), text_view);
  g_signal_connect (priv->im_context, "preedit-start",
                    G_CALLBACK (gtk_text_view_preedit_start_handler), text_view);
  g_signal_connect (priv->im_context, "preedit-changed",
 		    G_CALLBACK (gtk_text_view_preedit_changed_handler), text_view);
  g_signal_connect (priv->im_context, "retrieve-surrounding",
 		    G_CALLBACK (gtk_text_view_retrieve_surrounding_handler), text_view);
  g_signal_connect (priv->im_context, "delete-surrounding",
 		    G_CALLBACK (gtk_text_view_delete_surrounding_handler), text_view);

  priv->cursor_visible = TRUE;

  priv->accepts_tab = TRUE;

  priv->text_window = text_window_new (widget);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_text_view_click_gesture_pressed),
                    widget);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_text_view_click_gesture_released),
                    widget);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));

  priv->drag_gesture = gtk_gesture_drag_new ();
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_text_view_drag_gesture_update),
                    widget);
  g_signal_connect (priv->drag_gesture, "drag-end",
                    G_CALLBACK (gtk_text_view_drag_gesture_end),
                    widget);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->drag_gesture));

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (gtk_text_view_motion), widget);
  gtk_widget_add_controller (widget, controller);

  priv->key_controller = gtk_event_controller_key_new ();
  g_signal_connect (priv->key_controller, "key-pressed",
                    G_CALLBACK (gtk_text_view_key_controller_key_pressed),
                    widget);
  g_signal_connect (priv->key_controller, "im-update",
                    G_CALLBACK (gtk_text_view_key_controller_im_update),
                    widget);
  gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                           priv->im_context);
  gtk_widget_add_controller (widget, priv->key_controller);
  controller = gtk_event_controller_focus_new ();
  g_signal_connect_swapped (controller, "enter",
                            G_CALLBACK (gtk_text_view_focus_in), widget);
  g_signal_connect_swapped (controller, "leave",
                            G_CALLBACK (gtk_text_view_focus_out), widget);
  gtk_widget_add_controller (widget, controller);

  priv->selection_node = gtk_css_node_new ();
  gtk_css_node_set_name (priv->selection_node, g_quark_from_static_string ("selection"));
  gtk_css_node_set_parent (priv->selection_node, priv->text_window->css_node);
  gtk_css_node_set_state (priv->selection_node,
                          gtk_css_node_get_state (priv->text_window->css_node) & ~GTK_STATE_FLAG_DROP_ACTIVE);
  gtk_css_node_set_visible (priv->selection_node, FALSE);
  g_object_unref (priv->selection_node);

  gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "text.can-redo", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "text.can-undo", FALSE);

  gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
                                  GTK_ACCESSIBLE_PROPERTY_MULTI_LINE, TRUE,
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                  -1);
}

GtkCssNode *
gtk_text_view_get_text_node (GtkTextView *text_view)
{
  return text_view->priv->text_window->css_node;
}

GtkCssNode *
gtk_text_view_get_selection_node (GtkTextView *text_view)
{
  return text_view->priv->selection_node;
}

static void
_gtk_text_view_ensure_magnifier (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (priv->magnifier_popover)
    return;

  priv->magnifier = _gtk_magnifier_new (GTK_WIDGET (text_view));
  _gtk_magnifier_set_magnification (GTK_MAGNIFIER (priv->magnifier), 2.0);
  priv->magnifier_popover = gtk_popover_new ();
  gtk_popover_set_position (GTK_POPOVER (priv->magnifier_popover), GTK_POS_TOP);
  gtk_widget_set_parent (priv->magnifier_popover, GTK_WIDGET (text_view));
  gtk_widget_add_css_class (priv->magnifier_popover, "magnifier");
  gtk_popover_set_autohide (GTK_POPOVER (priv->magnifier_popover), FALSE);
  gtk_popover_set_child (GTK_POPOVER (priv->magnifier_popover), priv->magnifier);
  gtk_widget_set_visible (priv->magnifier, TRUE);
}

/**
 * gtk_text_view_new:
 *
 * Creates a new `GtkTextView`.
 *
 * If you don’t call [method@Gtk.TextView.set_buffer] before using the
 * text view, an empty default buffer will be created for you. Get the
 * buffer with [method@Gtk.TextView.get_buffer]. If you want to specify
 * your own buffer, consider [ctor@Gtk.TextView.new_with_buffer].
 *
 * Returns: a new `GtkTextView`
 */
GtkWidget*
gtk_text_view_new (void)
{
  return g_object_new (GTK_TYPE_TEXT_VIEW, NULL);
}

/**
 * gtk_text_view_new_with_buffer:
 * @buffer: a `GtkTextBuffer`
 *
 * Creates a new `GtkTextView` widget displaying the buffer @buffer.
 *
 * One buffer can be shared among many widgets. @buffer may be %NULL
 * to create a default buffer, in which case this function is equivalent
 * to [ctor@Gtk.TextView.new]. The text view adds its own reference count
 * to the buffer; it does not take over an existing reference.
 *
 * Returns: a new `GtkTextView`.
 */
GtkWidget*
gtk_text_view_new_with_buffer (GtkTextBuffer *buffer)
{
  GtkTextView *text_view;

  text_view = (GtkTextView*)gtk_text_view_new ();

  gtk_text_view_set_buffer (text_view, buffer);

  return GTK_WIDGET (text_view);
}

/**
 * gtk_text_view_set_buffer: (attributes org.gtk.Method.set_property=buffer)
 * @text_view: a `GtkTextView`
 * @buffer: (nullable): a `GtkTextBuffer`
 *
 * Sets @buffer as the buffer being displayed by @text_view.
 *
 * The previous buffer displayed by the text view is unreferenced, and
 * a reference is added to @buffer. If you owned a reference to @buffer
 * before passing it to this function, you must remove that reference
 * yourself; `GtkTextView` will not “adopt” it.
 */
void
gtk_text_view_set_buffer (GtkTextView   *text_view,
                          GtkTextBuffer *buffer)
{
  GtkTextViewPrivate *priv;
  GtkTextBuffer *old_buffer;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (buffer == NULL || GTK_IS_TEXT_BUFFER (buffer));

  priv = text_view->priv;

  if (priv->buffer == buffer)
    return;

  old_buffer = priv->buffer;

  if (old_buffer != NULL)
    {
      while (priv->anchored_children.length)
        {
          AnchoredChild *ac = g_queue_peek_head (&priv->anchored_children);
          gtk_text_view_remove (text_view, ac->widget);
          /* ac is now invalid! */
        }

      g_signal_handlers_disconnect_by_func (priv->buffer,
					    gtk_text_view_mark_set_handler,
					    text_view);
      g_signal_handlers_disconnect_by_func (priv->buffer,
                                            gtk_text_view_paste_done_handler,
                                            text_view);
      g_signal_handlers_disconnect_by_func (priv->buffer,
                                            gtk_text_view_buffer_changed_handler,
                                            text_view);
      g_signal_handlers_disconnect_by_func (priv->buffer,
                                            gtk_text_view_buffer_notify_redo,
                                            text_view);
      g_signal_handlers_disconnect_by_func (priv->buffer,
                                            gtk_text_view_buffer_notify_undo,
                                            text_view);
      g_signal_handlers_disconnect_by_func (priv->buffer,
                                            gtk_text_view_insert_text_handler,
                                            text_view);
      g_signal_handlers_disconnect_by_func (priv->buffer,
                                            gtk_text_view_delete_range_handler,
                                            text_view);

      if (gtk_widget_get_realized (GTK_WIDGET (text_view)))
	{
	  GdkClipboard *clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (text_view));
	  gtk_text_buffer_remove_selection_clipboard (priv->buffer, clipboard);
        }

      if (priv->layout)
        gtk_text_layout_set_buffer (priv->layout, NULL);

      priv->dnd_mark = NULL;
      priv->first_para_mark = NULL;
      cancel_pending_scroll (text_view);
    }

  priv->buffer = buffer;

  if (priv->layout)
    gtk_text_layout_set_buffer (priv->layout, buffer);

  if (buffer != NULL)
    {
      GtkTextIter start;
      gboolean can_undo = FALSE;
      gboolean can_redo = FALSE;

      g_object_ref (buffer);

      gtk_text_buffer_get_iter_at_offset (priv->buffer, &start, 0);

      priv->dnd_mark = gtk_text_buffer_create_mark (priv->buffer,
                                                    "gtk_drag_target",
                                                    &start, FALSE);

      priv->first_para_mark = gtk_text_buffer_create_mark (priv->buffer,
                                                           NULL,
                                                           &start, TRUE);

      priv->first_para_pixels = 0;


      g_signal_connect (priv->buffer, "mark-set",
			G_CALLBACK (gtk_text_view_mark_set_handler),
                        text_view);
      g_signal_connect (priv->buffer, "paste-done",
			G_CALLBACK (gtk_text_view_paste_done_handler),
                        text_view);
      g_signal_connect (priv->buffer, "changed",
			G_CALLBACK (gtk_text_view_buffer_changed_handler),
                        text_view);
      g_signal_connect (priv->buffer, "notify",
                        G_CALLBACK (gtk_text_view_buffer_notify_undo),
                        text_view);
      g_signal_connect (priv->buffer, "notify",
                        G_CALLBACK (gtk_text_view_buffer_notify_redo),
                        text_view);
      g_signal_connect_after (priv->buffer, "insert-text",
                              G_CALLBACK (gtk_text_view_insert_text_handler),
                              text_view);
      g_signal_connect (priv->buffer, "delete-range",
                        G_CALLBACK (gtk_text_view_delete_range_handler),
                        text_view);

      can_undo = gtk_text_buffer_get_can_undo (buffer);
      can_redo = gtk_text_buffer_get_can_redo (buffer);

      if (gtk_widget_get_realized (GTK_WIDGET (text_view)))
	{
	  GdkClipboard *clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (text_view));
	  gtk_text_buffer_add_selection_clipboard (priv->buffer, clipboard);
	}

      gtk_text_view_update_handles (text_view);

      gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "text.undo", can_undo);
      gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "text.redo", can_redo);
    }

  if (old_buffer)
    g_object_unref (old_buffer);

  g_object_notify (G_OBJECT (text_view), "buffer");

  if (gtk_widget_get_visible (GTK_WIDGET (text_view)))
    gtk_widget_queue_draw (GTK_WIDGET (text_view));

  DV(g_print ("Invalidating due to set_buffer\n"));
  gtk_text_view_invalidate (text_view);
}

static GtkTextBuffer*
gtk_text_view_create_buffer (GtkTextView *text_view)
{
  return gtk_text_buffer_new (NULL);
}

/**
 * gtk_text_view_get_buffer: (attributes org.gtk.Method.get_property=buffer)
 * @text_view: a `GtkTextView`
 *
 * Returns the `GtkTextBuffer` being displayed by this text view.
 *
 * The reference count on the buffer is not incremented; the caller
 * of this function won’t own a new reference.
 *
 * Returns: (transfer none): a `GtkTextBuffer`
 */
GtkTextBuffer*
gtk_text_view_get_buffer (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  return get_buffer (text_view);
}

/**
 * gtk_text_view_get_cursor_locations:
 * @text_view: a `GtkTextView`
 * @iter: (nullable): a `GtkTextIter`
 * @strong: (out) (optional): location to store the strong cursor position
 * @weak: (out) (optional): location to store the weak cursor position
 *
 * Determine the positions of the strong and weak cursors if the
 * insertion point is at @iter.
 *
 * The position of each cursor is stored as a zero-width rectangle.
 * The strong cursor location is the location where characters of
 * the directionality equal to the base direction of the paragraph
 * are inserted. The weak cursor location is the location where
 * characters of the directionality opposite to the base direction
 * of the paragraph are inserted.
 *
 * If @iter is %NULL, the actual cursor position is used.
 *
 * Note that if @iter happens to be the actual cursor position, and
 * there is currently an IM preedit sequence being entered, the
 * returned locations will be adjusted to account for the preedit
 * cursor’s offset within the preedit sequence.
 *
 * The rectangle position is in buffer coordinates; use
 * [method@Gtk.TextView.buffer_to_window_coords] to convert these
 * coordinates to coordinates for one of the windows in the text view.
 */
void
gtk_text_view_get_cursor_locations (GtkTextView       *text_view,
                                    const GtkTextIter *iter,
                                    GdkRectangle      *strong,
                                    GdkRectangle      *weak)
{
  GtkTextIter insert;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (iter == NULL ||
                    gtk_text_iter_get_buffer (iter) == get_buffer (text_view));

  gtk_text_view_ensure_layout (text_view);

  if (iter)
    insert = *iter;
  else
    gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                      gtk_text_buffer_get_insert (get_buffer (text_view)));

  gtk_text_layout_get_cursor_locations (text_view->priv->layout, &insert,
                                        strong, weak);
}

/**
 * gtk_text_view_get_iter_at_location:
 * @text_view: a `GtkTextView`
 * @iter: (out): a `GtkTextIter`
 * @x: x position, in buffer coordinates
 * @y: y position, in buffer coordinates
 *
 * Retrieves the iterator at buffer coordinates @x and @y.
 *
 * Buffer coordinates are coordinates for the entire buffer, not just
 * the currently-displayed portion. If you have coordinates from an
 * event, you have to convert those to buffer coordinates with
 * [method@Gtk.TextView.window_to_buffer_coords].
 *
 * Returns: %TRUE if the position is over text
 */
gboolean
gtk_text_view_get_iter_at_location (GtkTextView *text_view,
                                    GtkTextIter *iter,
                                    int          x,
                                    int          y)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_get_iter_at_pixel (text_view->priv->layout, iter, x, y);
}

/**
 * gtk_text_view_get_iter_at_position:
 * @text_view: a `GtkTextView`
 * @iter: (out): a `GtkTextIter`
 * @trailing: (out) (optional): if non-%NULL, location to store
 *    an integer indicating where in the grapheme the user clicked.
 *    It will either be zero, or the number of characters in the grapheme.
 *    0 represents the trailing edge of the grapheme.
 * @x: x position, in buffer coordinates
 * @y: y position, in buffer coordinates
 *
 * Retrieves the iterator pointing to the character at buffer
 * coordinates @x and @y.
 *
 * Buffer coordinates are coordinates for the entire buffer, not just
 * the currently-displayed portion. If you have coordinates from an event,
 * you have to convert those to buffer coordinates with
 * [method@Gtk.TextView.window_to_buffer_coords].
 *
 * Note that this is different from [method@Gtk.TextView.get_iter_at_location],
 * which returns cursor locations, i.e. positions between characters.
 *
 * Returns: %TRUE if the position is over text
 */
gboolean
gtk_text_view_get_iter_at_position (GtkTextView *text_view,
                                    GtkTextIter *iter,
                                    int         *trailing,
                                    int          x,
                                    int          y)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_get_iter_at_position (text_view->priv->layout, iter, trailing, x, y);
}

/**
 * gtk_text_view_get_iter_location:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 * @location: (out): bounds of the character at @iter
 *
 * Gets a rectangle which roughly contains the character at @iter.
 *
 * The rectangle position is in buffer coordinates; use
 * [method@Gtk.TextView.buffer_to_window_coords] to convert these
 * coordinates to coordinates for one of the windows in the text view.
 */
void
gtk_text_view_get_iter_location (GtkTextView       *text_view,
                                 const GtkTextIter *iter,
                                 GdkRectangle      *location)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == get_buffer (text_view));

  gtk_text_view_ensure_layout (text_view);

  gtk_text_layout_get_iter_location (text_view->priv->layout, iter, location);
}

/**
 * gtk_text_view_get_line_yrange:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 * @y: (out): return location for a y coordinate
 * @height: (out): return location for a height
 *
 * Gets the y coordinate of the top of the line containing @iter,
 * and the height of the line.
 *
 * The coordinate is a buffer coordinate; convert to window
 * coordinates with [method@Gtk.TextView.buffer_to_window_coords].
 */
void
gtk_text_view_get_line_yrange (GtkTextView       *text_view,
                               const GtkTextIter *iter,
                               int               *y,
                               int               *height)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == get_buffer (text_view));

  gtk_text_view_ensure_layout (text_view);

  gtk_text_layout_get_line_yrange (text_view->priv->layout,
                                   iter,
                                   y,
                                   height);
}

/**
 * gtk_text_view_get_line_at_y:
 * @text_view: a `GtkTextView`
 * @target_iter: (out): a `GtkTextIter`
 * @y: a y coordinate
 * @line_top: (out): return location for top coordinate of the line
 *
 * Gets the `GtkTextIter` at the start of the line containing
 * the coordinate @y.
 *
 * @y is in buffer coordinates, convert from window coordinates with
 * [method@Gtk.TextView.window_to_buffer_coords]. If non-%NULL,
 * @line_top will be filled with the coordinate of the top edge
 * of the line.
 */
void
gtk_text_view_get_line_at_y (GtkTextView *text_view,
                             GtkTextIter *target_iter,
                             int          y,
                             int         *line_top)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  gtk_text_view_ensure_layout (text_view);

  gtk_text_layout_get_line_at_y (text_view->priv->layout,
                                 target_iter,
                                 y,
                                 line_top);
}

/* Same as gtk_text_view_scroll_to_iter but deal with
 * (top_margin / top_padding) and (bottom_margin / bottom_padding).
 * When with_border == TRUE and you scroll on the edges,
 * all borders are shown for the corresponding edge.
 * When with_border == FALSE, only left margin and right_margin
 * can be seen because they can be can be overwritten by tags.
 */
static gboolean
_gtk_text_view_scroll_to_iter (GtkTextView   *text_view,
                               GtkTextIter   *iter,
                               double         within_margin,
                               gboolean       use_align,
                               double         xalign,
                               double         yalign,
                               gboolean       with_border)
{
  GtkTextViewPrivate *priv = text_view->priv;
  GtkWidget *widget;

  GdkRectangle cursor;
  int cursor_bottom;
  int cursor_right;

  GdkRectangle screen;
  GdkRectangle screen_dest;

  int screen_inner_left;
  int screen_inner_right;
  int screen_inner_top;
  int screen_inner_bottom;

  int border_xoffset = 0;
  int border_yoffset = 0;
  int within_margin_xoffset;
  int within_margin_yoffset;

  int buffer_bottom;
  int buffer_right;

  gboolean retval = FALSE;

  /* FIXME why don't we do the validate-at-scroll-destination thing
   * from flush_scroll in this function? I think it wasn't done before
   * because changed_handler was screwed up, but I could be wrong.
   */

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (within_margin >= 0.0 && within_margin < 0.5, FALSE);
  g_return_val_if_fail (xalign >= 0.0 && xalign <= 1.0, FALSE);
  g_return_val_if_fail (yalign >= 0.0 && yalign <= 1.0, FALSE);

  widget = GTK_WIDGET (text_view);

  DV(g_print(G_STRLOC"\n"));

  gtk_text_layout_get_iter_location (priv->layout,
                                     iter,
                                     &cursor);

  DV (g_print (" target cursor %d,%d  %d x %d\n", cursor.x, cursor.y, cursor.width, cursor.height));

  /* In each direction, *_border are the addition of *_padding and *_margin
   *
   * Vadjustment value:
   * (-priv->top_margin) [top padding][top margin] (0) [text][bottom margin][bottom padding]
   *
   * Hadjustment value:
   * (-priv->left_padding) [left padding] (0) [left margin][text][right margin][right padding]
   *
   * Buffer coordinates:
   * on x: (0) [left margin][text][right margin]
   * on y: (0) [text]
   *
   * left margin and right margin are part of the x buffer coordinate
   * because they are part of the pango layout so that they can be
   * overwritten by tags.
   *
   * Canvas coordinates:
   * (the canvas is the virtual window where the content of the buffer is drawn )
   *
   * on x: (-priv->left_padding) [left padding] (0) [left margin][text][right margin][right padding]
   * on y: (-priv->top_margin) [top margin][top padding] (0) [text][bottom margin][bottom padding]
   *
   * (priv->xoffset, priv->yoffset) is the origin of the view (visible part of the canvas)
   *  in canvas coordinates.
   * As you can see, canvas coordinates and buffer coordinates are compatible but the canvas
   * can be larger than the buffer depending of the border size.
   */

  cursor_bottom = cursor.y + cursor.height;
  cursor_right = cursor.x + cursor.width;

  /* Current position of the view in canvas coordinates */
  screen.x = priv->xoffset;
  screen.y = priv->yoffset;
  screen.width = SCREEN_WIDTH (widget);
  screen.height = SCREEN_HEIGHT (widget);

  within_margin_xoffset = screen.width * within_margin;
  within_margin_yoffset = screen.height * within_margin;

  screen_inner_left = screen.x + within_margin_xoffset;
  screen_inner_top = screen.y + within_margin_yoffset;
  screen_inner_right = screen.x + screen.width - within_margin_xoffset;
  screen_inner_bottom = screen.y + screen.height - within_margin_yoffset;

  buffer_bottom = priv->height - priv->bottom_margin;
  buffer_right = priv->width - priv->right_margin - priv->left_padding - 1;

  screen_dest.x = screen.x;
  screen_dest.y = screen.y;
  screen_dest.width = screen.width - within_margin_xoffset * 2;
  screen_dest.height = screen.height - within_margin_yoffset * 2;

  /* Minimum authorised size check */
  if (screen_dest.width < 1)
    screen_dest.width = 1;
  if (screen_dest.height < 1)
    screen_dest.height = 1;

  /* The alignment affects the point in the target character that we
   * choose to align. If we're doing right/bottom alignment, we align
   * the right/bottom edge of the character the mark is at; if we're
   * doing left/top we align the left/top edge of the character; if
   * we're doing center alignment we align the center of the
   * character.
   *
   * The different cases handle on each direction:
   *   1. cursor outside of the inner area define by within_margin
   *   2. if use_align == TRUE, alignment with xalign and yalign
   *   3. scrolling on the edges dependent of with_border
   */

  /* Vertical scroll */
  if (use_align)
    {
      int cursor_y_alignment_offset;

      cursor_y_alignment_offset = (cursor.height * yalign) - (screen_dest.height * yalign);
      screen_dest.y = cursor.y + cursor_y_alignment_offset - within_margin_yoffset;
    }
  else
    {
      /* move minimum to get onscreen, showing the
       * top_margin or bottom_margin when necessary
       */
      if (cursor.y < screen_inner_top)
        {
          if (cursor.y == 0)
            border_yoffset = with_border ? priv->top_padding : 0;

          screen_dest.y = cursor.y - MAX (within_margin_yoffset, border_yoffset);
        }
      else if (cursor_bottom > screen_inner_bottom)
        {
          if (cursor_bottom == buffer_bottom - priv->top_margin)
            border_yoffset = with_border ? priv->bottom_padding : 0;

          screen_dest.y = cursor_bottom - screen_dest.height -
                          MAX (within_margin_yoffset, border_yoffset);
        }
    }

  if (screen_dest.y != screen.y)
    {
      gtk_adjustment_animate_to_value (priv->vadjustment, screen_dest.y  + priv->top_margin);

      DV (g_print (" vert increment %d\n", screen_dest.y - screen.y));
    }

  /* Horizontal scroll */

  if (use_align)
    {
      int cursor_x_alignment_offset;

      cursor_x_alignment_offset = (cursor.width * xalign) - (screen_dest.width * xalign);
      screen_dest.x = cursor.x + cursor_x_alignment_offset - within_margin_xoffset;
    }
  else
    {
      /* move minimum to get onscreen, showing the
       * left_margin or right_margin when necessary
       */
      if (cursor.x < screen_inner_left)
        {
          if (cursor.x == priv->left_margin)
            border_xoffset = with_border ? priv->left_padding : 0;

          screen_dest.x = cursor.x - MAX (within_margin_xoffset, border_xoffset);
        }
      else if (cursor_right >= screen_inner_right - 1)
        {
          if (cursor.x >= buffer_right - priv->right_padding)
            border_xoffset = with_border ? priv->right_padding : 0;

          screen_dest.x = cursor_right - screen_dest.width -
                          MAX (within_margin_xoffset, border_xoffset) + 1;
        }
    }

  if (screen_dest.x != screen.x)
    {
      gtk_adjustment_animate_to_value (priv->hadjustment, screen_dest.x + priv->left_padding);

      DV (g_print (" horiz increment %d\n", screen_dest.x - screen.x));
    }

  retval = (screen.y != screen_dest.y) || (screen.x != screen_dest.x);

  DV(g_print (">%s ("G_STRLOC")\n", retval ? "Actually scrolled" : "Didn't end up scrolling"));

  return retval;
}

/**
 * gtk_text_view_scroll_to_iter:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 * @within_margin: margin as a [0.0,0.5) fraction of screen size
 * @use_align: whether to use alignment arguments (if %FALSE,
 *    just get the mark onscreen)
 * @xalign: horizontal alignment of mark within visible area
 * @yalign: vertical alignment of mark within visible area
 *
 * Scrolls @text_view so that @iter is on the screen in the position
 * indicated by @xalign and @yalign.
 *
 * An alignment of 0.0 indicates left or top, 1.0 indicates right or
 * bottom, 0.5 means center. If @use_align is %FALSE, the text scrolls
 * the minimal distance to get the mark onscreen, possibly not scrolling
 * at all. The effective screen for purposes of this function is reduced
 * by a margin of size @within_margin.
 *
 * Note that this function uses the currently-computed height of the
 * lines in the text buffer. Line heights are computed in an idle
 * handler; so this function may not have the desired effect if it’s
 * called before the height computations. To avoid oddness, consider
 * using [method@Gtk.TextView.scroll_to_mark] which saves a point to be
 * scrolled to after line validation.
 *
 * Returns: %TRUE if scrolling occurred
 */
gboolean
gtk_text_view_scroll_to_iter (GtkTextView   *text_view,
                              GtkTextIter   *iter,
                              double         within_margin,
                              gboolean       use_align,
                              double         xalign,
                              double         yalign)
{
  return _gtk_text_view_scroll_to_iter (text_view,
                                        iter,
                                        within_margin,
                                        use_align,
                                        xalign,
                                        yalign,
                                        FALSE);
}

static void
free_pending_scroll (GtkTextPendingScroll *scroll)
{
  if (!gtk_text_mark_get_deleted (scroll->mark))
    gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (scroll->mark),
                                 scroll->mark);
  g_object_unref (scroll->mark);
  g_free (scroll);
}

static void
cancel_pending_scroll (GtkTextView *text_view)
{
  if (text_view->priv->pending_scroll)
    {
      free_pending_scroll (text_view->priv->pending_scroll);
      text_view->priv->pending_scroll = NULL;
    }
}

static void
gtk_text_view_queue_scroll (GtkTextView   *text_view,
                            GtkTextMark   *mark,
                            double         within_margin,
                            gboolean       use_align,
                            double         xalign,
                            double         yalign)
{
  GtkTextIter iter;
  GtkTextPendingScroll *scroll;

  DV(g_print(G_STRLOC"\n"));

  scroll = g_new (GtkTextPendingScroll, 1);

  scroll->within_margin = within_margin;
  scroll->use_align = use_align;
  scroll->xalign = xalign;
  scroll->yalign = yalign;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &iter, mark);

  scroll->mark = gtk_text_buffer_create_mark (get_buffer (text_view),
                                              NULL,
                                              &iter,
                                              gtk_text_mark_get_left_gravity (mark));

  g_object_ref (scroll->mark);

  cancel_pending_scroll (text_view);

  text_view->priv->pending_scroll = scroll;
}

static gboolean
gtk_text_view_flush_scroll (GtkTextView *text_view)
{
  int height;
  GtkTextIter iter;
  GtkTextPendingScroll *scroll;
  gboolean retval;
  GtkWidget *widget;

  widget = GTK_WIDGET (text_view);

  DV(g_print(G_STRLOC"\n"));

  if (text_view->priv->pending_scroll == NULL)
    {
      DV (g_print ("in flush scroll, no pending scroll\n"));
      return FALSE;
    }

  scroll = text_view->priv->pending_scroll;

  /* avoid recursion */
  text_view->priv->pending_scroll = NULL;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &iter, scroll->mark);

  /* Validate area around the scroll destination, so the adjustment
   * can meaningfully point into that area. We must validate
   * enough area to be sure that after we scroll, everything onscreen
   * is valid; otherwise, validation will maintain the first para
   * in one place, but may push the target iter off the bottom of
   * the screen.
   */
  DV(g_print (">Validating scroll destination ("G_STRLOC")\n"));
  height = gtk_widget_get_height (widget);
  gtk_text_layout_validate_yrange (text_view->priv->layout, &iter,
                                   - (height * 2),
                                   height * 2);

  DV(g_print (">Done validating scroll destination ("G_STRLOC")\n"));

  /* Ensure we have updated width/height */
  gtk_text_view_update_adjustments (text_view);

  retval = _gtk_text_view_scroll_to_iter (text_view,
                                          &iter,
                                          scroll->within_margin,
                                          scroll->use_align,
                                          scroll->xalign,
                                          scroll->yalign,
                                          TRUE);

  gtk_text_view_update_handles (text_view);

  free_pending_scroll (scroll);

  return retval;
}

static void
gtk_text_view_update_adjustments (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;
  int width = 0, height = 0;

  DV(g_print(">Updating adjustments ("G_STRLOC")\n"));

  priv = text_view->priv;

  if (priv->layout)
    gtk_text_layout_get_size (priv->layout, &width, &height);

  /* Make room for the cursor after the last character in the widest line */
  width += SPACE_FOR_CURSOR;
  height += priv->top_margin + priv->bottom_margin;

  if (priv->width != width || priv->height != height)
    {
      priv->width = width;
      priv->height = height;

      gtk_text_view_set_hadjustment_values (text_view);
      gtk_text_view_set_vadjustment_values (text_view);
    }
}

static void
gtk_text_view_update_layout_width (GtkTextView *text_view)
{
  DV(g_print(">Updating layout width ("G_STRLOC")\n"));

  gtk_text_view_ensure_layout (text_view);

  gtk_text_layout_set_screen_width (text_view->priv->layout,
                                    MAX (1, SCREEN_WIDTH (text_view) - SPACE_FOR_CURSOR));
}

static void
gtk_text_view_update_im_spot_location (GtkTextView *text_view)
{
  GdkRectangle area;

  if (text_view->priv->layout == NULL)
    return;

  gtk_text_view_get_cursor_locations (text_view, NULL, &area, NULL);

  area.x -= text_view->priv->xoffset;
  area.y -= text_view->priv->yoffset;

  /* Width returned by Pango indicates direction of cursor,
   * by its sign more than the size of cursor.
   */
  area.width = 0;

  gtk_im_context_set_cursor_location (text_view->priv->im_context, &area);
}

static gboolean
do_update_im_spot_location (gpointer text_view)
{
  GtkTextViewPrivate *priv;

  priv = GTK_TEXT_VIEW (text_view)->priv;
  priv->im_spot_idle = 0;

  gtk_text_view_update_im_spot_location (text_view);
  return FALSE;
}

static void
queue_update_im_spot_location (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;

  priv = text_view->priv;

  /* Use priority a little higher than GTK_TEXT_VIEW_PRIORITY_VALIDATE,
   * so we don't wait until the entire buffer has been validated. */
  if (!priv->im_spot_idle)
    {
      priv->im_spot_idle = g_idle_add_full (GTK_TEXT_VIEW_PRIORITY_VALIDATE - 1,
                                            do_update_im_spot_location,
                                            text_view,
                                            NULL);
      gdk_source_set_static_name_by_id (priv->im_spot_idle, "[gtk] do_update_im_spot_location");
    }
}

static void
flush_update_im_spot_location (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;

  priv = text_view->priv;

  if (priv->im_spot_idle)
    {
      g_source_remove (priv->im_spot_idle);
      priv->im_spot_idle = 0;
      gtk_text_view_update_im_spot_location (text_view);
    }
}

/**
 * gtk_text_view_scroll_to_mark:
 * @text_view: a `GtkTextView`
 * @mark: a `GtkTextMark`
 * @within_margin: margin as a [0.0,0.5) fraction of screen size
 * @use_align: whether to use alignment arguments (if %FALSE, just
 *    get the mark onscreen)
 * @xalign: horizontal alignment of mark within visible area
 * @yalign: vertical alignment of mark within visible area
 *
 * Scrolls @text_view so that @mark is on the screen in the position
 * indicated by @xalign and @yalign.
 *
 * An alignment of 0.0 indicates left or top, 1.0 indicates right or
 * bottom, 0.5 means center. If @use_align is %FALSE, the text scrolls
 * the minimal distance to get the mark onscreen, possibly not scrolling
 * at all. The effective screen for purposes of this function is reduced
 * by a margin of size @within_margin.
 */
void
gtk_text_view_scroll_to_mark (GtkTextView *text_view,
                              GtkTextMark *mark,
                              double       within_margin,
                              gboolean     use_align,
                              double       xalign,
                              double       yalign)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (within_margin >= 0.0 && within_margin < 0.5);
  g_return_if_fail (xalign >= 0.0 && xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0 && yalign <= 1.0);

  /* We need to verify that the buffer contains the mark, otherwise this
   * can lead to data structure corruption later on.
   */
  g_return_if_fail (get_buffer (text_view) == gtk_text_mark_get_buffer (mark));

  gtk_text_view_queue_scroll (text_view, mark,
                              within_margin,
                              use_align,
                              xalign,
                              yalign);

  /* If no validation is pending, we need to go ahead and force an
   * immediate scroll.
   */
  if (text_view->priv->layout &&
      gtk_text_layout_is_valid (text_view->priv->layout))
    gtk_text_view_flush_scroll (text_view);
}

/**
 * gtk_text_view_scroll_mark_onscreen:
 * @text_view: a `GtkTextView`
 * @mark: a mark in the buffer for @text_view
 *
 * Scrolls @text_view the minimum distance such that @mark is contained
 * within the visible area of the widget.
 */
void
gtk_text_view_scroll_mark_onscreen (GtkTextView *text_view,
                                    GtkTextMark *mark)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));

  /* We need to verify that the buffer contains the mark, otherwise this
   * can lead to data structure corruption later on.
   */
  g_return_if_fail (get_buffer (text_view) == gtk_text_mark_get_buffer (mark));

  gtk_text_view_scroll_to_mark (text_view, mark, 0.0, FALSE, 0.0, 0.0);
}

static gboolean
clamp_iter_onscreen (GtkTextView *text_view, GtkTextIter *iter)
{
  GdkRectangle visible_rect;
  gtk_text_view_get_visible_rect (text_view, &visible_rect);

  return gtk_text_layout_clamp_iter_to_vrange (text_view->priv->layout, iter,
                                               visible_rect.y,
                                               visible_rect.y + visible_rect.height);
}

/**
 * gtk_text_view_move_mark_onscreen:
 * @text_view: a `GtkTextView`
 * @mark: a `GtkTextMark`
 *
 * Moves a mark within the buffer so that it's
 * located within the currently-visible text area.
 *
 * Returns: %TRUE if the mark moved (wasn’t already onscreen)
 */
gboolean
gtk_text_view_move_mark_onscreen (GtkTextView *text_view,
                                  GtkTextMark *mark)
{
  GtkTextIter iter;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (mark != NULL, FALSE);

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &iter, mark);

  if (clamp_iter_onscreen (text_view, &iter))
    {
      gtk_text_buffer_move_mark (get_buffer (text_view), mark, &iter);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_view_get_visible_rect:
 * @text_view: a `GtkTextView`
 * @visible_rect: (out): rectangle to fill
 *
 * Fills @visible_rect with the currently-visible
 * region of the buffer, in buffer coordinates.
 *
 * Convert to window coordinates with
 * [method@Gtk.TextView.buffer_to_window_coords].
 */
void
gtk_text_view_get_visible_rect (GtkTextView  *text_view,
                                GdkRectangle *visible_rect)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  widget = GTK_WIDGET (text_view);

  if (visible_rect)
    {
      visible_rect->x = text_view->priv->xoffset;
      visible_rect->y = text_view->priv->yoffset;
      visible_rect->width = SCREEN_WIDTH (widget);
      visible_rect->height = SCREEN_HEIGHT (widget);

      DV(g_print(" visible rect: %d,%d %d x %d\n",
                 visible_rect->x,
                 visible_rect->y,
                 visible_rect->width,
                 visible_rect->height));
    }
}

/**
 * gtk_text_view_set_wrap_mode: (attributes org.gtk.Method.set_property=wrap-mode)
 * @text_view: a `GtkTextView`
 * @wrap_mode: a `GtkWrapMode`
 *
 * Sets the line wrapping for the view.
 */
void
gtk_text_view_set_wrap_mode (GtkTextView *text_view,
                             GtkWrapMode  wrap_mode)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;

  if (priv->wrap_mode != wrap_mode)
    {
      priv->wrap_mode = wrap_mode;

      if (priv->layout && priv->layout->default_style)
        {
          priv->layout->default_style->wrap_mode = wrap_mode;
          gtk_text_layout_default_style_changed (priv->layout);
        }
      g_object_notify (G_OBJECT (text_view), "wrap-mode");
    }
}

/**
 * gtk_text_view_get_wrap_mode: (attributes org.gtk.Method.get_property=wrap-mode)
 * @text_view: a `GtkTextView`
 *
 * Gets the line wrapping for the view.
 *
 * Returns: the line wrap setting
 */
GtkWrapMode
gtk_text_view_get_wrap_mode (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), GTK_WRAP_NONE);

  return text_view->priv->wrap_mode;
}

/**
 * gtk_text_view_set_editable: (attributes org.gtk.Method.set_property=editable)
 * @text_view: a `GtkTextView`
 * @setting: whether it’s editable
 *
 * Sets the default editability of the `GtkTextView`.
 *
 * You can override this default setting with tags in the buffer,
 * using the “editable” attribute of tags.
 */
void
gtk_text_view_set_editable (GtkTextView *text_view,
                            gboolean     setting)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;
  setting = setting != FALSE;

  if (priv->editable != setting)
    {
      if (!setting)
        {
          gtk_text_view_reset_im_context (text_view);
          if (gtk_widget_has_focus (GTK_WIDGET (text_view)))
            gtk_im_context_focus_out (priv->im_context);
        }

      priv->editable = setting;

      if (setting && gtk_widget_has_focus (GTK_WIDGET (text_view)))
        gtk_im_context_focus_in (priv->im_context);

      gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                               setting ? priv->im_context : NULL);

      if (priv->layout && priv->layout->default_style)
        {
          gtk_text_layout_set_overwrite_mode (priv->layout,
                                              priv->overwrite_mode && priv->editable);
          priv->layout->default_style->editable = priv->editable;
          gtk_text_layout_default_style_changed (priv->layout);
        }

      gtk_accessible_update_property (GTK_ACCESSIBLE (text_view),
                                      GTK_ACCESSIBLE_PROPERTY_READ_ONLY, !setting,
                                      -1);
      gtk_text_view_update_emoji_action (text_view);

      g_object_notify (G_OBJECT (text_view), "editable");
    }
}

/**
 * gtk_text_view_get_editable: (attributes org.gtk.Method.get_property=editable)
 * @text_view: a `GtkTextView`
 *
 * Returns the default editability of the `GtkTextView`.
 *
 * Tags in the buffer may override this setting for some ranges of text.
 *
 * Returns: whether text is editable by default
 */
gboolean
gtk_text_view_get_editable (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  return text_view->priv->editable;
}

/**
 * gtk_text_view_set_pixels_above_lines: (attributes org.gtk.Method.set_property=pixels-above-lines)
 * @text_view: a `GtkTextView`
 * @pixels_above_lines: pixels above paragraphs
 *
 * Sets the default number of blank pixels above paragraphs in @text_view.
 *
 * Tags in the buffer for @text_view may override the defaults.
 */
void
gtk_text_view_set_pixels_above_lines (GtkTextView *text_view,
                                      int          pixels_above_lines)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;

  if (priv->pixels_above_lines != pixels_above_lines)
    {
      priv->pixels_above_lines = pixels_above_lines;

      if (priv->layout && priv->layout->default_style)
        {
          priv->layout->default_style->pixels_above_lines = pixels_above_lines;
          gtk_text_layout_default_style_changed (priv->layout);
        }

      g_object_notify (G_OBJECT (text_view), "pixels-above-lines");
    }
}

/**
 * gtk_text_view_get_pixels_above_lines: (attributes org.gtk.Method.get_property=pixels-above-lines)
 * @text_view: a `GtkTextView`
 *
 * Gets the default number of pixels to put above paragraphs.
 *
 * Adding this function with [method@Gtk.TextView.get_pixels_below_lines]
 * is equal to the line space between each paragraph.
 *
 * Returns: default number of pixels above paragraphs
 */
int
gtk_text_view_get_pixels_above_lines (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->priv->pixels_above_lines;
}

/**
 * gtk_text_view_set_pixels_below_lines: (attributes org.gtk.Method.set_property=pixels-below-lines)
 * @text_view: a `GtkTextView`
 * @pixels_below_lines: pixels below paragraphs
 *
 * Sets the default number of pixels of blank space
 * to put below paragraphs in @text_view.
 *
 * May be overridden by tags applied to @text_view’s buffer.
 */
void
gtk_text_view_set_pixels_below_lines (GtkTextView *text_view,
                                      int          pixels_below_lines)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;

  if (priv->pixels_below_lines != pixels_below_lines)
    {
      priv->pixels_below_lines = pixels_below_lines;

      if (priv->layout && priv->layout->default_style)
        {
          priv->layout->default_style->pixels_below_lines = pixels_below_lines;
          gtk_text_layout_default_style_changed (priv->layout);
        }

      g_object_notify (G_OBJECT (text_view), "pixels-below-lines");
    }
}

/**
 * gtk_text_view_get_pixels_below_lines: (attributes org.gtk.Method.get_property=pixels-below-lines)
 * @text_view: a `GtkTextView`
 *
 * Gets the default number of pixels to put below paragraphs.
 *
 * The line space is the sum of the value returned by this function and
 * the value returned by [method@Gtk.TextView.get_pixels_above_lines].
 *
 * Returns: default number of blank pixels below paragraphs
 */
int
gtk_text_view_get_pixels_below_lines (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->priv->pixels_below_lines;
}

/**
 * gtk_text_view_set_pixels_inside_wrap: (attributes org.gtk.Method.set_property=pixels-inside-wrap)
 * @text_view: a `GtkTextView`
 * @pixels_inside_wrap: default number of pixels between wrapped lines
 *
 * Sets the default number of pixels of blank space to leave between
 * display/wrapped lines within a paragraph.
 *
 * May be overridden by tags in @text_view’s buffer.
 */
void
gtk_text_view_set_pixels_inside_wrap (GtkTextView *text_view,
                                      int          pixels_inside_wrap)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;

  if (priv->pixels_inside_wrap != pixels_inside_wrap)
    {
      priv->pixels_inside_wrap = pixels_inside_wrap;

      if (priv->layout && priv->layout->default_style)
        {
          priv->layout->default_style->pixels_inside_wrap = pixels_inside_wrap;
          gtk_text_layout_default_style_changed (priv->layout);
        }

      g_object_notify (G_OBJECT (text_view), "pixels-inside-wrap");
    }
}

/**
 * gtk_text_view_get_pixels_inside_wrap: (attributes org.gtk.Method.get_property=pixels-inside-wrap)
 * @text_view: a `GtkTextView`
 *
 * Gets the default number of pixels to put between wrapped lines
 * inside a paragraph.
 *
 * Returns: default number of pixels of blank space between wrapped lines
 */
int
gtk_text_view_get_pixels_inside_wrap (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->priv->pixels_inside_wrap;
}

/**
 * gtk_text_view_set_justification: (attributes org.gtk.Method.set_property=justification)
 * @text_view: a `GtkTextView`
 * @justification: justification
 *
 * Sets the default justification of text in @text_view.
 *
 * Tags in the view’s buffer may override the default.
 */
void
gtk_text_view_set_justification (GtkTextView     *text_view,
                                 GtkJustification justification)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;

  if (priv->justify != justification)
    {
      priv->justify = justification;

      if (priv->layout && priv->layout->default_style)
        {
          priv->layout->default_style->justification = justification;
          gtk_text_layout_default_style_changed (priv->layout);
        }

      g_object_notify (G_OBJECT (text_view), "justification");
    }
}

/**
 * gtk_text_view_get_justification: (attributes org.gtk.Method.get_property=justification)
 * @text_view: a `GtkTextView`
 *
 * Gets the default justification of paragraphs in @text_view.
 *
 * Tags in the buffer may override the default.
 *
 * Returns: default justification
 */
GtkJustification
gtk_text_view_get_justification (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), GTK_JUSTIFY_LEFT);

  return text_view->priv->justify;
}

/**
 * gtk_text_view_set_left_margin: (attributes org.gtk.Method.set_property=left-margin)
 * @text_view: a `GtkTextView`
 * @left_margin: left margin in pixels
 *
 * Sets the default left margin for text in @text_view.
 *
 * Tags in the buffer may override the default.
 *
 * Note that this function is confusingly named.
 * In CSS terms, the value set here is padding.
 */
void
gtk_text_view_set_left_margin (GtkTextView *text_view,
                               int          left_margin)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (priv->left_margin != left_margin)
    {
      priv->left_margin = left_margin;
      priv->left_margin = left_margin + priv->left_padding;

      if (priv->layout && priv->layout->default_style)
        {
          priv->layout->default_style->left_margin = left_margin;
          gtk_text_layout_default_style_changed (priv->layout);
        }

      g_object_notify (G_OBJECT (text_view), "left-margin");
    }
}

/**
 * gtk_text_view_get_left_margin: (attributes org.gtk.Method.get_property=left-margin)
 * @text_view: a `GtkTextView`
 *
 * Gets the default left margin size of paragraphs in the @text_view.
 *
 * Tags in the buffer may override the default.
 *
 * Returns: left margin in pixels
 */
int
gtk_text_view_get_left_margin (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->priv->left_margin;
}

/**
 * gtk_text_view_set_right_margin: (attributes org.gtk.Method.set_property=right-margin)
 * @text_view: a `GtkTextView`
 * @right_margin: right margin in pixels
 *
 * Sets the default right margin for text in the text view.
 *
 * Tags in the buffer may override the default.
 *
 * Note that this function is confusingly named.
 * In CSS terms, the value set here is padding.
 */
void
gtk_text_view_set_right_margin (GtkTextView *text_view,
                                int          right_margin)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (priv->right_margin != right_margin)
    {
      priv->right_margin = right_margin;
      priv->right_margin = right_margin + priv->right_padding;

      if (priv->layout && priv->layout->default_style)
        {
          priv->layout->default_style->right_margin = right_margin;
          gtk_text_layout_default_style_changed (priv->layout);
        }

      g_object_notify (G_OBJECT (text_view), "right-margin");
    }
}

/**
 * gtk_text_view_get_right_margin: (attributes org.gtk.Method.get_property=right-margin)
 * @text_view: a `GtkTextView`
 *
 * Gets the default right margin for text in @text_view.
 *
 * Tags in the buffer may override the default.
 *
 * Returns: right margin in pixels
 */
int
gtk_text_view_get_right_margin (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->priv->right_margin;
}

/**
 * gtk_text_view_set_top_margin: (attributes org.gtk.Method.set_property=top-margin)
 * @text_view: a `GtkTextView`
 * @top_margin: top margin in pixels
 *
 * Sets the top margin for text in @text_view.
 *
 * Note that this function is confusingly named.
 * In CSS terms, the value set here is padding.
 */
void
gtk_text_view_set_top_margin (GtkTextView *text_view,
                              int          top_margin)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (priv->top_margin != top_margin)
    {
      priv->yoffset += priv->top_margin - top_margin;

      priv->top_margin = top_margin;
      priv->top_margin = top_margin + priv->top_padding;

      if (priv->layout && priv->layout->default_style)
        gtk_text_layout_default_style_changed (priv->layout);

      gtk_text_view_invalidate (text_view);

      g_object_notify (G_OBJECT (text_view), "top-margin");
    }
}

/**
 * gtk_text_view_get_top_margin: (attributes org.gtk.Method.get_property=top-margin)
 * @text_view: a `GtkTextView`
 *
 * Gets the top margin for text in the @text_view.
 *
 * Returns: top margin in pixels
 */
int
gtk_text_view_get_top_margin (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->priv->top_margin;
}

/**
 * gtk_text_view_set_bottom_margin: (attributes org.gtk.Method.set_property=bottom-margin)
 * @text_view: a `GtkTextView`
 * @bottom_margin: bottom margin in pixels
 *
 * Sets the bottom margin for text in @text_view.
 *
 * Note that this function is confusingly named.
 * In CSS terms, the value set here is padding.
 */
void
gtk_text_view_set_bottom_margin (GtkTextView *text_view,
                                 int          bottom_margin)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (priv->bottom_margin != bottom_margin)
    {
      priv->bottom_margin = bottom_margin;
      priv->bottom_margin = bottom_margin + priv->bottom_padding;

      if (priv->layout && priv->layout->default_style)
        gtk_text_layout_default_style_changed (priv->layout);

      g_object_notify (G_OBJECT (text_view), "bottom-margin");
    }
}

/**
 * gtk_text_view_get_bottom_margin: (attributes org.gtk.Method.get_property=bottom-margin)
 * @text_view: a `GtkTextView`
 *
 * Gets the bottom margin for text in the @text_view.
 *
 * Returns: bottom margin in pixels
 */
int
gtk_text_view_get_bottom_margin (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->priv->bottom_margin;
}

/**
 * gtk_text_view_set_indent: (attributes org.gtk.Method.set_property=indent)
 * @text_view: a `GtkTextView`
 * @indent: indentation in pixels
 *
 * Sets the default indentation for paragraphs in @text_view.
 *
 * Tags in the buffer may override the default.
 */
void
gtk_text_view_set_indent (GtkTextView *text_view,
                          int          indent)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;

  if (priv->indent != indent)
    {
      priv->indent = indent;

      if (priv->layout && priv->layout->default_style)
        {
          priv->layout->default_style->indent = indent;
          gtk_text_layout_default_style_changed (priv->layout);
        }

      g_object_notify (G_OBJECT (text_view), "indent");
    }
}

/**
 * gtk_text_view_get_indent: (attributes org.gtk.Method.get_property=indent)
 * @text_view: a `GtkTextView`
 *
 * Gets the default indentation of paragraphs in @text_view.
 *
 * Tags in the view’s buffer may override the default.
 * The indentation may be negative.
 *
 * Returns: number of pixels of indentation
 */
int
gtk_text_view_get_indent (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), 0);

  return text_view->priv->indent;
}

/**
 * gtk_text_view_set_tabs: (attributes org.gtk.Method.set_property=tabs)
 * @text_view: a `GtkTextView`
 * @tabs: tabs as a `PangoTabArray`
 *
 * Sets the default tab stops for paragraphs in @text_view.
 *
 * Tags in the buffer may override the default.
 */
void
gtk_text_view_set_tabs (GtkTextView   *text_view,
                        PangoTabArray *tabs)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;

  if (priv->tabs)
    pango_tab_array_free (priv->tabs);

  priv->tabs = tabs ? pango_tab_array_copy (tabs) : NULL;

  if (priv->layout && priv->layout->default_style)
    {
      /* some unkosher futzing in internal struct details... */
      if (priv->layout->default_style->tabs)
        pango_tab_array_free (priv->layout->default_style->tabs);

      priv->layout->default_style->tabs =
        priv->tabs ? pango_tab_array_copy (priv->tabs) : NULL;

      gtk_text_layout_default_style_changed (priv->layout);
    }

  g_object_notify (G_OBJECT (text_view), "tabs");
}

/**
 * gtk_text_view_get_tabs: (attributes org.gtk.Method.get_property=tabs)
 * @text_view: a `GtkTextView`
 *
 * Gets the default tabs for @text_view.
 *
 * Tags in the buffer may override the defaults. The returned array
 * will be %NULL if “standard” (8-space) tabs are used. Free the
 * return value with [method@Pango.TabArray.free].
 *
 * Returns: (nullable) (transfer full): copy of default tab array,
 *   or %NULL if standard tabs are used; must be freed with
 *   [method@Pango.TabArray.free].
 */
PangoTabArray*
gtk_text_view_get_tabs (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  return text_view->priv->tabs ? pango_tab_array_copy (text_view->priv->tabs) : NULL;
}

static void
gtk_text_view_toggle_cursor_visible (GtkTextView *text_view)
{
  gtk_text_view_set_cursor_visible (text_view, !text_view->priv->cursor_visible);
}

/**
 * gtk_text_view_set_cursor_visible: (attributes org.gtk.Method.set_property=cursor-visible)
 * @text_view: a `GtkTextView`
 * @setting: whether to show the insertion cursor
 *
 * Toggles whether the insertion point should be displayed.
 *
 * A buffer with no editable text probably shouldn’t have a visible
 * cursor, so you may want to turn the cursor off.
 *
 * Note that this property may be overridden by the
 * [property@Gtk.Settings:gtk-keynav-use-caret] setting.
 */
void
gtk_text_view_set_cursor_visible (GtkTextView *text_view,
				  gboolean     setting)
{
  GtkTextViewPrivate *priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = text_view->priv;
  setting = (setting != FALSE);

  if (priv->cursor_visible != setting)
    {
      priv->cursor_visible = setting;

      if (gtk_widget_has_focus (GTK_WIDGET (text_view)))
        {
          if (priv->layout)
            {
              gtk_text_layout_set_cursor_visible (priv->layout, setting);
	      gtk_text_view_check_cursor_blink (text_view);
            }
        }

      g_object_notify (G_OBJECT (text_view), "cursor-visible");
    }
}

/**
 * gtk_text_view_get_cursor_visible: (attributes org.gtk.Method.get_property=cursor-visible)
 * @text_view: a `GtkTextView`
 *
 * Find out whether the cursor should be displayed.
 *
 * Returns: whether the insertion mark is visible
 */
gboolean
gtk_text_view_get_cursor_visible (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  return text_view->priv->cursor_visible;
}

/**
 * gtk_text_view_reset_cursor_blink:
 * @text_view: a `GtkTextView`
 *
 * Ensures that the cursor is shown.
 *
 * This also resets the time that it will stay blinking (or
 * visible, in case blinking is disabled).
 *
 * This function should be called in response to user input
 * (e.g. from derived classes that override the textview's
 * event handlers).
 */
void
gtk_text_view_reset_cursor_blink (GtkTextView *text_view)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  gtk_text_view_reset_blink_time (text_view);
  gtk_text_view_pend_cursor_blink (text_view);
}

/**
 * gtk_text_view_place_cursor_onscreen:
 * @text_view: a `GtkTextView`
 *
 * Moves the cursor to the currently visible region of the
 * buffer.
 *
 * Returns: %TRUE if the cursor had to be moved.
 */
gboolean
gtk_text_view_place_cursor_onscreen (GtkTextView *text_view)
{
  GtkTextIter insert;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_insert (get_buffer (text_view)));

  if (clamp_iter_onscreen (text_view, &insert))
    {
      gtk_text_buffer_place_cursor (get_buffer (text_view), &insert);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_text_view_remove_validate_idles (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (priv->first_validate_idle != 0)
    {
      DV (g_print ("Removing first validate idle: %s\n", G_STRLOC));
      g_source_remove (priv->first_validate_idle);
      priv->first_validate_idle = 0;
    }

  if (priv->incremental_validate_idle != 0)
    {
      g_source_remove (priv->incremental_validate_idle);
      priv->incremental_validate_idle = 0;
    }
}

static void
gtk_text_view_dispose (GObject *object)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (object);
  GtkTextViewPrivate *priv = text_view->priv;
  GtkWidget *child;

  child = g_object_get_data (object, "gtk-emoji-chooser");
  if (child)
    {
      gtk_widget_unparent (child);
      g_object_set_data (object, "gtk-emoji-chooser", NULL);
    }

  gtk_text_view_remove_validate_idles (text_view);
  gtk_text_view_set_buffer (text_view, NULL);
  gtk_text_view_destroy_layout (text_view);

  if (text_view->priv->scroll_timeout)
    {
      g_source_remove (text_view->priv->scroll_timeout);
      text_view->priv->scroll_timeout = 0;
    }

  if (priv->im_spot_idle)
    {
      g_source_remove (priv->im_spot_idle);
      priv->im_spot_idle = 0;
    }

  if (priv->magnifier)
    _gtk_magnifier_set_inspected (GTK_MAGNIFIER (priv->magnifier), NULL);

  g_clear_pointer ((GtkWidget **) &priv->text_handles[TEXT_HANDLE_CURSOR], gtk_widget_unparent);
  g_clear_pointer ((GtkWidget **) &priv->text_handles[TEXT_HANDLE_SELECTION_BOUND], gtk_widget_unparent);

  g_clear_pointer (&priv->selection_bubble, gtk_widget_unparent);
  g_clear_pointer (&priv->magnifier_popover, gtk_widget_unparent);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (text_view))))
    gtk_text_view_remove (text_view, child);

  G_OBJECT_CLASS (gtk_text_view_parent_class)->dispose (object);
}

static void
gtk_text_view_finalize (GObject *object)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;

  text_view = GTK_TEXT_VIEW (object);
  priv = text_view->priv;

  gtk_text_view_destroy_layout (text_view);
  gtk_text_view_set_buffer (text_view, NULL);

  /* at this point, no "notify::buffer" handler should recreate the buffer. */
  g_assert (priv->buffer == NULL);

  /* Ensure all children were removed */
  g_assert (priv->anchored_children.length == 0);
  g_assert (priv->left_child == NULL);
  g_assert (priv->right_child == NULL);
  g_assert (priv->top_child == NULL);
  g_assert (priv->bottom_child == NULL);
  g_assert (priv->center_child == NULL);

  cancel_pending_scroll (text_view);

  if (priv->tabs)
    pango_tab_array_free (priv->tabs);

  if (priv->hadjustment)
    g_object_unref (priv->hadjustment);
  if (priv->vadjustment)
    g_object_unref (priv->vadjustment);

  text_window_free (priv->text_window);

  g_object_unref (priv->im_context);

  g_free (priv->im_module);

  g_clear_pointer (&priv->popup_menu, gtk_widget_unparent);
  g_clear_object (&priv->extra_menu);

  G_OBJECT_CLASS (gtk_text_view_parent_class)->finalize (object);
}

static void
gtk_text_view_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;

  text_view = GTK_TEXT_VIEW (object);
  priv = text_view->priv;

  switch (prop_id)
    {
    case PROP_PIXELS_ABOVE_LINES:
      gtk_text_view_set_pixels_above_lines (text_view, g_value_get_int (value));
      break;

    case PROP_PIXELS_BELOW_LINES:
      gtk_text_view_set_pixels_below_lines (text_view, g_value_get_int (value));
      break;

    case PROP_PIXELS_INSIDE_WRAP:
      gtk_text_view_set_pixels_inside_wrap (text_view, g_value_get_int (value));
      break;

    case PROP_EDITABLE:
      gtk_text_view_set_editable (text_view, g_value_get_boolean (value));
      break;

    case PROP_WRAP_MODE:
      gtk_text_view_set_wrap_mode (text_view, g_value_get_enum (value));
      break;

    case PROP_JUSTIFICATION:
      gtk_text_view_set_justification (text_view, g_value_get_enum (value));
      break;

    case PROP_LEFT_MARGIN:
      gtk_text_view_set_left_margin (text_view, g_value_get_int (value));
      break;

    case PROP_RIGHT_MARGIN:
      gtk_text_view_set_right_margin (text_view, g_value_get_int (value));
      break;

    case PROP_TOP_MARGIN:
      gtk_text_view_set_top_margin (text_view, g_value_get_int (value));
      break;

    case PROP_BOTTOM_MARGIN:
      gtk_text_view_set_bottom_margin (text_view, g_value_get_int (value));
      break;

    case PROP_INDENT:
      gtk_text_view_set_indent (text_view, g_value_get_int (value));
      break;

    case PROP_TABS:
      gtk_text_view_set_tabs (text_view, g_value_get_boxed (value));
      break;

    case PROP_CURSOR_VISIBLE:
      gtk_text_view_set_cursor_visible (text_view, g_value_get_boolean (value));
      break;

    case PROP_OVERWRITE:
      gtk_text_view_set_overwrite (text_view, g_value_get_boolean (value));
      break;

    case PROP_BUFFER:
      gtk_text_view_set_buffer (text_view, GTK_TEXT_BUFFER (g_value_get_object (value)));
      break;

    case PROP_ACCEPTS_TAB:
      gtk_text_view_set_accepts_tab (text_view, g_value_get_boolean (value));
      break;

    case PROP_IM_MODULE:
      g_free (priv->im_module);
      priv->im_module = g_value_dup_string (value);
      if (GTK_IS_IM_MULTICONTEXT (priv->im_context))
        gtk_im_multicontext_set_context_id (GTK_IM_MULTICONTEXT (priv->im_context), priv->im_module);
      break;

    case PROP_HADJUSTMENT:
      gtk_text_view_set_hadjustment (text_view, g_value_get_object (value));
      break;

    case PROP_VADJUSTMENT:
      gtk_text_view_set_vadjustment (text_view, g_value_get_object (value));
      break;

    case PROP_HSCROLL_POLICY:
      if (priv->hscroll_policy != g_value_get_enum (value))
        {
          priv->hscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (text_view));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_VSCROLL_POLICY:
      if (priv->vscroll_policy != g_value_get_enum (value))
        {
          priv->vscroll_policy = g_value_get_enum (value);
          gtk_widget_queue_resize (GTK_WIDGET (text_view));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_INPUT_PURPOSE:
      gtk_text_view_set_input_purpose (text_view, g_value_get_enum (value));
      break;

    case PROP_INPUT_HINTS:
      gtk_text_view_set_input_hints (text_view, g_value_get_flags (value));
      break;

    case PROP_MONOSPACE:
      gtk_text_view_set_monospace (text_view, g_value_get_boolean (value));
      break;

    case PROP_EXTRA_MENU:
      gtk_text_view_set_extra_menu (text_view, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_view_get_property (GObject         *object,
			    guint            prop_id,
			    GValue          *value,
			    GParamSpec      *pspec)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;

  text_view = GTK_TEXT_VIEW (object);
  priv = text_view->priv;

  switch (prop_id)
    {
    case PROP_PIXELS_ABOVE_LINES:
      g_value_set_int (value, priv->pixels_above_lines);
      break;

    case PROP_PIXELS_BELOW_LINES:
      g_value_set_int (value, priv->pixels_below_lines);
      break;

    case PROP_PIXELS_INSIDE_WRAP:
      g_value_set_int (value, priv->pixels_inside_wrap);
      break;

    case PROP_EDITABLE:
      g_value_set_boolean (value, priv->editable);
      break;

    case PROP_WRAP_MODE:
      g_value_set_enum (value, priv->wrap_mode);
      break;

    case PROP_JUSTIFICATION:
      g_value_set_enum (value, priv->justify);
      break;

    case PROP_LEFT_MARGIN:
      g_value_set_int (value, priv->left_margin);
      break;

    case PROP_RIGHT_MARGIN:
      g_value_set_int (value, priv->right_margin);
      break;

    case PROP_TOP_MARGIN:
      g_value_set_int (value, priv->top_margin);
      break;

    case PROP_BOTTOM_MARGIN:
      g_value_set_int (value, priv->bottom_margin);
      break;

    case PROP_INDENT:
      g_value_set_int (value, priv->indent);
      break;

    case PROP_TABS:
      g_value_set_boxed (value, priv->tabs);
      break;

    case PROP_CURSOR_VISIBLE:
      g_value_set_boolean (value, priv->cursor_visible);
      break;

    case PROP_BUFFER:
      g_value_set_object (value, get_buffer (text_view));
      break;

    case PROP_OVERWRITE:
      g_value_set_boolean (value, priv->overwrite_mode);
      break;

    case PROP_ACCEPTS_TAB:
      g_value_set_boolean (value, priv->accepts_tab);
      break;

    case PROP_IM_MODULE:
      g_value_set_string (value, priv->im_module);
      break;

    case PROP_HADJUSTMENT:
      g_value_set_object (value, priv->hadjustment);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, priv->vadjustment);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, priv->hscroll_policy);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, priv->vscroll_policy);
      break;

    case PROP_INPUT_PURPOSE:
      g_value_set_enum (value, gtk_text_view_get_input_purpose (text_view));
      break;

    case PROP_INPUT_HINTS:
      g_value_set_flags (value, gtk_text_view_get_input_hints (text_view));
      break;

    case PROP_MONOSPACE:
      g_value_set_boolean (value, gtk_text_view_get_monospace (text_view));
      break;

    case PROP_EXTRA_MENU:
      g_value_set_object (value, gtk_text_view_get_extra_menu (text_view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_view_measure_borders (GtkTextView *text_view,
                               GtkBorder   *border)
{
  GtkTextViewPrivate *priv = text_view->priv;
  int left = 0;
  int right = 0;
  int top = 0;
  int bottom = 0;

  if (priv->left_child)
    gtk_widget_measure (GTK_WIDGET (priv->left_child),
                        GTK_ORIENTATION_HORIZONTAL, -1,
                        &left, NULL, NULL, NULL);

  if (priv->right_child)
    gtk_widget_measure (GTK_WIDGET (priv->right_child),
                        GTK_ORIENTATION_HORIZONTAL, -1,
                        &right, NULL, NULL, NULL);

  if (priv->top_child)
    gtk_widget_measure (GTK_WIDGET (priv->top_child),
                        GTK_ORIENTATION_VERTICAL, -1,
                        &top, NULL, NULL, NULL);

  if (priv->bottom_child)
    gtk_widget_measure (GTK_WIDGET (priv->bottom_child),
                        GTK_ORIENTATION_VERTICAL, -1,
                        &bottom, NULL, NULL, NULL);

  border->left = left;
  border->right = right;
  border->top = top;
  border->bottom = bottom;
}

static void
gtk_text_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextViewPrivate *priv = text_view->priv;
  const GList *list;
  GtkBorder borders;
  int min = 0;
  int nat = 0;
  int extra;

  gtk_text_view_measure_borders (text_view, &borders);

  if (priv->center_child)
    gtk_widget_measure (GTK_WIDGET (priv->center_child),
                        orientation, for_size,
                        &min, &nat, NULL, NULL);

  for (list = priv->anchored_children.head; list; list = list->next)
    {
      const AnchoredChild *child = list->data;
      int child_min = 0;
      int child_nat = 0;

      gtk_widget_measure (child->widget, orientation, for_size,
                          &child_min, &child_nat,
                          NULL, NULL);

      /* Invalidate layout lines if required */
      if (child->anchor && priv->layout)
        gtk_text_child_anchor_queue_resize (child->anchor, priv->layout);

      min = MAX (min, child_min);
      nat = MAX (nat, child_nat);
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    extra = borders.left + priv->left_margin + priv->right_margin + borders.right;
  else
    extra = borders.top + priv->height + borders.bottom;

  *minimum = min + extra;
  *natural = nat + extra;
}

static void
gtk_text_view_compute_child_allocation (GtkTextView         *text_view,
                                        const AnchoredChild *vc,
                                        GtkAllocation       *allocation,
                                        int                  gutter_width,
                                        int                  gutter_height)
{
  int buffer_y;
  GtkTextIter iter;
  GtkRequisition req;

  gtk_text_buffer_get_iter_at_child_anchor (get_buffer (text_view),
                                            &iter,
                                            vc->anchor);

  gtk_text_layout_get_line_yrange (text_view->priv->layout, &iter,
                                   &buffer_y, NULL);

  buffer_y += vc->from_top_of_line;

  allocation->x = vc->from_left_of_buffer - text_view->priv->xoffset + gutter_width;
  allocation->y = buffer_y - text_view->priv->yoffset + gutter_height;

  gtk_widget_get_preferred_size (vc->widget, &req, NULL);
  allocation->width = req.width;
  allocation->height = req.height;
}

static void
gtk_text_view_update_child_allocation (GtkTextView         *text_view,
                                       const AnchoredChild *vc,
                                       int                  gutter_width,
                                       int                  gutter_height)
{
  GtkAllocation allocation;

  gtk_text_view_compute_child_allocation (text_view, vc, &allocation, gutter_width, gutter_height);

  gtk_widget_size_allocate (vc->widget, &allocation, -1);

#if 0
  g_print ("allocation for %p allocated to %d,%d yoffset = %d\n",
           vc->widget,
           vc->widget->allocation.x,
           vc->widget->allocation.y,
           text_view->priv->yoffset);
#endif
}

static void
calculate_gutter_offsets (GtkTextView *text_view,
                          int         *width,
                          int         *height)
{
  GtkWidget *x_gutter;
  GtkWidget *y_gutter;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (width != NULL && height != NULL);

  x_gutter = gtk_text_view_get_gutter (text_view, GTK_TEXT_WINDOW_LEFT);

  if (x_gutter != NULL)
    {
      GtkRequisition x_req = {0};
      gtk_widget_get_preferred_size (x_gutter, &x_req, NULL);
      *width = x_req.width;
    }
  else
    {
      *width = 0;
    }

  y_gutter = gtk_text_view_get_gutter (text_view, GTK_TEXT_WINDOW_TOP);

  if (y_gutter != NULL)
    {
      GtkRequisition y_req = {0};
      gtk_widget_get_preferred_size (y_gutter, &y_req, NULL);
      *height = y_req.height;
    }
  else
    {
      *height = 0;
    }
}

static void
gtk_anchored_child_allocated (GtkTextLayout *layout,
                              GtkWidget     *child,
                              int            x,
                              int            y,
                              gpointer       data)
{
  AnchoredChild *vc = NULL;
  GtkTextView *text_view = data;
  int x_offset = 0;
  int y_offset = 0;

  calculate_gutter_offsets (text_view, &x_offset, &y_offset);

  /* x,y is the position of the child from the top of the line, and
   * from the left of the buffer. We have to translate that into text
   * window coordinates, then size_allocate the child.
   */

  vc = g_object_get_qdata (G_OBJECT (child), quark_text_view_child);

  g_assert (vc != NULL);

  DV (g_print ("child allocated at %d,%d\n", x, y));

  vc->from_left_of_buffer = x;
  vc->from_top_of_line = y;

  gtk_text_view_update_child_allocation (text_view, vc, x_offset, y_offset);
}

static void
gtk_text_view_allocate_children (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;
  const GList *iter;

  DV(g_print(G_STRLOC"\n"));

  for (iter = priv->anchored_children.head; iter; iter = iter->next)
    {
      const AnchoredChild *child = iter->data;
      GtkTextIter child_loc;
      GtkRequisition child_req;
      GtkAllocation allocation;

      /* We need to force-validate the regions containing children. */
      gtk_text_buffer_get_iter_at_child_anchor (get_buffer (text_view),
                                                &child_loc,
                                                child->anchor);

      /* Since anchored children are only ever allocated from
       * gtk_text_layout_get_line_display() we have to make sure
       * that the display line caching in the layout doesn't
       * get in the way. Invalidating the layout around the anchor
       * achieves this.
       */
      if (_gtk_widget_get_alloc_needed (child->widget))
        {
          GtkTextIter end = child_loc;
          gtk_text_iter_forward_char (&end);
          gtk_text_layout_invalidate (priv->layout, &child_loc, &end);
        }

      gtk_text_layout_validate_yrange (priv->layout, &child_loc, 0, 1);

      gtk_widget_get_preferred_size (child->widget, &child_req, NULL);

      allocation.x = - child_req.width;
      allocation.y = - child_req.height;
      allocation.width = child_req.width;
      allocation.height = child_req.height;

      gtk_widget_size_allocate (child->widget, &allocation, -1);
    }
}

static GtkTextViewChild **
find_child_for_window_type (GtkTextView       *text_view,
                            GtkTextWindowType  window_type)
{
  switch (window_type)
    {
    case GTK_TEXT_WINDOW_LEFT:
      return &text_view->priv->left_child;
    case GTK_TEXT_WINDOW_RIGHT:
      return &text_view->priv->right_child;
    case GTK_TEXT_WINDOW_TOP:
      return &text_view->priv->top_child;
    case GTK_TEXT_WINDOW_BOTTOM:
      return &text_view->priv->bottom_child;
    case GTK_TEXT_WINDOW_TEXT:
      return &text_view->priv->center_child;
    case GTK_TEXT_WINDOW_WIDGET:
    default:
      return NULL;
    }
}

/**
 * gtk_text_view_get_gutter:
 * @text_view: a `GtkTextView`
 * @win: a `GtkTextWindowType`
 *
 * Gets a `GtkWidget` that has previously been set as gutter.
 *
 * See [method@Gtk.TextView.set_gutter].
 *
 * @win must be one of %GTK_TEXT_WINDOW_LEFT, %GTK_TEXT_WINDOW_RIGHT,
 * %GTK_TEXT_WINDOW_TOP, or %GTK_TEXT_WINDOW_BOTTOM.
 *
 * Returns: (transfer none) (nullable): a `GtkWidget`
 */
GtkWidget *
gtk_text_view_get_gutter (GtkTextView       *text_view,
                          GtkTextWindowType  win)
{
  GtkTextViewChild **childp;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);
  g_return_val_if_fail (win == GTK_TEXT_WINDOW_LEFT ||
                        win == GTK_TEXT_WINDOW_RIGHT ||
                        win == GTK_TEXT_WINDOW_TOP ||
                        win == GTK_TEXT_WINDOW_BOTTOM, NULL);

  childp = find_child_for_window_type (text_view, win);

  if (childp != NULL && *childp != NULL)
    return GTK_WIDGET (*childp);

  return NULL;
}

/**
 * gtk_text_view_set_gutter:
 * @text_view: a `GtkTextView`
 * @win: a `GtkTextWindowType`
 * @widget: (nullable): a `GtkWidget`
 *
 * Places @widget into the gutter specified by @win.
 *
 * @win must be one of %GTK_TEXT_WINDOW_LEFT, %GTK_TEXT_WINDOW_RIGHT,
 * %GTK_TEXT_WINDOW_TOP, or %GTK_TEXT_WINDOW_BOTTOM.
 */
void
gtk_text_view_set_gutter (GtkTextView       *text_view,
                          GtkTextWindowType  win,
                          GtkWidget         *widget)
{
  GtkTextViewChild **childp;
  GtkTextViewChild *old_child;
  GtkTextViewChild *new_child;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));
  g_return_if_fail (win == GTK_TEXT_WINDOW_LEFT ||
                    win == GTK_TEXT_WINDOW_RIGHT ||
                    win == GTK_TEXT_WINDOW_TOP ||
                    win == GTK_TEXT_WINDOW_BOTTOM);

  childp = find_child_for_window_type (text_view, win);
  if (childp == NULL)
    return;

  old_child = *childp;

  if ((GtkWidget *)old_child == widget)
    return;

  if (old_child != NULL)
    {
      *childp = NULL;
      gtk_widget_unparent (GTK_WIDGET (old_child));
      g_object_unref (old_child);
    }

  if (widget == NULL)
    return;

  new_child = GTK_TEXT_VIEW_CHILD (gtk_text_view_child_new (win));
  gtk_text_view_child_add (new_child, widget);

  *childp = g_object_ref (new_child);
  gtk_widget_set_parent (GTK_WIDGET (new_child), GTK_WIDGET (text_view));
  update_node_ordering (GTK_WIDGET (text_view));
}

static void
gtk_text_view_size_allocate (GtkWidget *widget,
                             int        widget_width,
                             int        widget_height,
                             int        baseline)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;
  int width, height;
  GdkRectangle text_rect;
  GdkRectangle left_rect;
  GdkRectangle right_rect;
  GdkRectangle top_rect;
  GdkRectangle bottom_rect;
  GtkWidget *chooser;
  PangoLayout *layout;
  guint mru_size;

  text_view = GTK_TEXT_VIEW (widget);
  priv = text_view->priv;

  DV(g_print(G_STRLOC"\n"));

  gtk_text_view_measure_borders (text_view, &priv->border_window_size);

  /* distribute width/height among child windows. Ensure all
   * windows get at least a 1x1 allocation.
   */
  left_rect.width = priv->border_window_size.left;
  right_rect.width = priv->border_window_size.right;
  width = widget_width - left_rect.width - right_rect.width;
  text_rect.width = MAX (1, width);
  top_rect.width = text_rect.width;
  bottom_rect.width = text_rect.width;

  top_rect.height = priv->border_window_size.top;
  bottom_rect.height = priv->border_window_size.bottom;
  height = widget_height - top_rect.height - bottom_rect.height;
  text_rect.height = MAX (1, height);
  left_rect.height = text_rect.height;
  right_rect.height = text_rect.height;

  /* Origins */
  left_rect.x = 0;
  top_rect.y = 0;

  text_rect.x = left_rect.x + left_rect.width;
  text_rect.y = top_rect.y + top_rect.height;

  left_rect.y = text_rect.y;
  right_rect.y = text_rect.y;

  top_rect.x = text_rect.x;
  bottom_rect.x = text_rect.x;

  right_rect.x = text_rect.x + text_rect.width;
  bottom_rect.y = text_rect.y + text_rect.height;

  text_window_size_allocate (priv->text_window, &text_rect);

  if (priv->center_child)
    {
      gtk_text_view_child_set_offset (priv->center_child, priv->xoffset, priv->yoffset);
      gtk_widget_size_allocate (GTK_WIDGET (priv->center_child), &text_rect, -1);
    }

  if (priv->left_child)
    {
      gtk_text_view_child_set_offset (priv->left_child, priv->xoffset, priv->yoffset);
      gtk_widget_size_allocate (GTK_WIDGET (priv->left_child), &left_rect, -1);
    }

  if (priv->right_child)
    {
      gtk_text_view_child_set_offset (priv->right_child, priv->xoffset, priv->yoffset);
      gtk_widget_size_allocate (GTK_WIDGET (priv->right_child), &right_rect, -1);
    }

  if (priv->top_child)
    {
      gtk_text_view_child_set_offset (priv->top_child, priv->xoffset, priv->yoffset);
      gtk_widget_size_allocate (GTK_WIDGET (priv->top_child), &top_rect, -1);
    }

  if (priv->bottom_child)
    {
      gtk_text_view_child_set_offset (priv->bottom_child, priv->xoffset, priv->yoffset);
      gtk_widget_size_allocate (GTK_WIDGET (priv->bottom_child), &bottom_rect, -1);
    }

  gtk_text_view_update_layout_width (text_view);

  /* Note that this will do some layout validation */
  gtk_text_view_allocate_children (text_view);

  /* Update adjustments */
  if (!gtk_adjustment_is_animating (priv->hadjustment))
    gtk_text_view_set_hadjustment_values (text_view);
  if (!gtk_adjustment_is_animating (priv->vadjustment))
    gtk_text_view_set_vadjustment_values (text_view);

  /* Optimize display cache size */
  layout = gtk_widget_create_pango_layout (widget, "X");
  pango_layout_get_pixel_size (layout, &width, &height);
  if (height > 0)
    {
      mru_size = SCREEN_HEIGHT (widget) / height * 3;
      gtk_text_layout_set_mru_size (priv->layout, mru_size);
    }
  g_object_unref (layout);

  /* The GTK resize loop processes all the pending exposes right
   * after doing the resize stuff, so the idle sizer won't have a
   * chance to run. So we do the work here.
   */
  gtk_text_view_flush_first_validate (text_view);

  chooser = g_object_get_data (G_OBJECT (text_view), "gtk-emoji-chooser");
  if (chooser)
    gtk_popover_present (GTK_POPOVER (chooser));

  if (priv->magnifier_popover)
    gtk_popover_present (GTK_POPOVER (priv->magnifier_popover));

  if (priv->popup_menu)
    gtk_popover_present (GTK_POPOVER (priv->popup_menu));

  if (priv->text_handles[TEXT_HANDLE_CURSOR])
    gtk_text_handle_present (priv->text_handles[TEXT_HANDLE_CURSOR]);

  if (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])
    gtk_text_handle_present (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND]);

  if (priv->selection_bubble)
    gtk_popover_present (GTK_POPOVER (priv->selection_bubble));
}

static void
gtk_text_view_get_first_para_iter (GtkTextView *text_view,
                                   GtkTextIter *iter)
{
  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), iter,
                                    text_view->priv->first_para_mark);
}

static void
gtk_text_view_validate_onscreen (GtkTextView *text_view)
{
  GtkWidget *widget;
  GtkTextViewPrivate *priv;

  widget = GTK_WIDGET (text_view);
  priv = text_view->priv;

  DV(g_print(">Validating onscreen ("G_STRLOC")\n"));

  if (SCREEN_HEIGHT (widget) > 0)
    {
      GtkTextIter first_para;

      /* Be sure we've validated the stuff onscreen; if we
       * scrolled, these calls won't have any effect, because
       * they were called in the recursive validate_onscreen
       */
      gtk_text_view_get_first_para_iter (text_view, &first_para);

      gtk_text_layout_validate_yrange (priv->layout,
                                       &first_para,
                                       0,
                                       priv->first_para_pixels +
                                       SCREEN_HEIGHT (widget));
    }

  priv->onscreen_validated = TRUE;

  DV(g_print(">Done validating onscreen, onscreen_validated = TRUE ("G_STRLOC")\n"));

  /* This can have the odd side effect of triggering a scroll, which should
   * flip "onscreen_validated" back to FALSE, but should also get us
   * back into this function to turn it on again.
   */
  gtk_text_view_update_adjustments (text_view);

  g_assert (priv->onscreen_validated);
}

static void
gtk_text_view_flush_first_validate (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (priv->first_validate_idle == 0)
    return;

  /* Do this first, which means that if an "invalidate"
   * occurs during any of this process, a new first_validate_callback
   * will be installed, and we'll start again.
   */
  DV (g_print ("removing first validate in %s\n", G_STRLOC));
  g_source_remove (priv->first_validate_idle);
  priv->first_validate_idle = 0;

  /* be sure we have up-to-date screen size set on the
   * layout.
   */
  gtk_text_view_update_layout_width (text_view);

  /* Bail out if we invalidated stuff; scrolling right away will just
   * confuse the issue.
   */
  if (priv->first_validate_idle != 0)
    {
      DV(g_print(">Width change forced requeue ("G_STRLOC")\n"));
    }
  else
    {
      /* scroll to any marks, if that's pending. This can jump us to
       * the validation codepath used for scrolling onscreen, if so we
       * bail out.  It won't jump if already in that codepath since
       * value_changed is not recursive, so also validate if
       * necessary.
       */
      if (!gtk_text_view_flush_scroll (text_view) ||
          !priv->onscreen_validated)
	gtk_text_view_validate_onscreen (text_view);

      DV(g_print(">Leaving first validate idle ("G_STRLOC")\n"));

      g_assert (priv->onscreen_validated);
    }
}

static gboolean
first_validate_callback (gpointer data)
{
  GtkTextView *text_view = data;

  /* Note that some of this code is duplicated at the end of size_allocate,
   * keep in sync with that.
   */

  DV(g_print(G_STRLOC"\n"));

  gtk_text_view_flush_first_validate (text_view);

  return FALSE;
}

static gboolean
incremental_validate_callback (gpointer data)
{
  GtkTextView *text_view = data;
  gboolean result = TRUE;

  DV(g_print(G_STRLOC"\n"));

  gtk_text_layout_validate (text_view->priv->layout, 2000);

  gtk_text_view_update_adjustments (text_view);

  if (gtk_text_layout_is_valid (text_view->priv->layout))
    {
      text_view->priv->incremental_validate_idle = 0;
      result = FALSE;
    }

  return result;
}

static void
gtk_text_view_invalidate (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  DV (g_print (">Invalidate, onscreen_validated = %d now FALSE ("G_STRLOC")\n",
               priv->onscreen_validated));

  priv->onscreen_validated = FALSE;

  /* We'll invalidate when the layout is created */
  if (priv->layout == NULL)
    return;

  if (!priv->first_validate_idle)
    {
      priv->first_validate_idle = g_idle_add_full (GTK_PRIORITY_RESIZE - 2, first_validate_callback, text_view, NULL);
      gdk_source_set_static_name_by_id (priv->first_validate_idle, "[gtk] first_validate_callback");
      DV (g_print (G_STRLOC": adding first validate idle %d\n",
                   priv->first_validate_idle));
    }

  if (!priv->incremental_validate_idle)
    {
      priv->incremental_validate_idle = g_idle_add_full (GTK_TEXT_VIEW_PRIORITY_VALIDATE, incremental_validate_callback, text_view, NULL);
      gdk_source_set_static_name_by_id (priv->incremental_validate_idle, "[gtk] incremental_validate_callback");
      DV (g_print (G_STRLOC": adding incremental validate idle %d\n",
                   priv->incremental_validate_idle));
    }
}

static void
invalidated_handler (GtkTextLayout *layout,
                     gpointer       data)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (data);

  DV (g_print ("Invalidating due to layout invalidate signal\n"));
  gtk_text_view_invalidate (text_view);
}

static void
changed_handler (GtkTextLayout     *layout,
                 int                start_y,
                 int                old_height,
                 int                new_height,
                 gpointer           data)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;
  GtkWidget *widget;

  text_view = GTK_TEXT_VIEW (data);
  priv = text_view->priv;
  widget = GTK_WIDGET (data);

  DV(g_print(">Lines Validated ("G_STRLOC")\n"));

  if (gtk_widget_get_realized (widget))
    {
      gtk_widget_queue_draw (widget);

      queue_update_im_spot_location (text_view);
    }

  if (old_height != new_height)
    {
      const GList *iter;
      GtkTextIter first;
      int new_first_para_top;
      int old_first_para_top;
      int x_offset = 0;
      int y_offset = 0;

      calculate_gutter_offsets (text_view, &x_offset, &y_offset);

      /* If the bottom of the old area was above the top of the
       * screen, we need to scroll to keep the current top of the
       * screen in place.  Remember that first_para_pixels is the
       * position of the top of the screen in coordinates relative to
       * the first paragraph onscreen.
       *
       * In short we are adding the height delta of the portion of the
       * changed region above first_para_mark to priv->yoffset.
       */
      gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &first,
                                        priv->first_para_mark);

      gtk_text_layout_get_line_yrange (layout, &first, &new_first_para_top, NULL);

      old_first_para_top = priv->yoffset - priv->first_para_pixels + priv->top_margin;

      if (new_first_para_top != old_first_para_top)
        {
          priv->yoffset += new_first_para_top - old_first_para_top;

          gtk_adjustment_set_value (text_view->priv->vadjustment, priv->yoffset);
        }

      /* FIXME be smarter about which anchored widgets we update */

      for (iter = priv->anchored_children.head; iter; iter = iter->next)
        {
          const AnchoredChild *ac = iter->data;
          gtk_text_view_update_child_allocation (text_view, ac, x_offset, y_offset);
        }

      gtk_widget_queue_resize (widget);
    }
}

static void
gtk_text_view_realize (GtkWidget *widget)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;

  text_view = GTK_TEXT_VIEW (widget);
  priv = text_view->priv;

  GTK_WIDGET_CLASS (gtk_text_view_parent_class)->realize (widget);

  if (gtk_widget_is_sensitive (widget))
    {
      gtk_im_context_set_client_widget (GTK_TEXT_VIEW (widget)->priv->im_context,
                                        widget);
    }

  gtk_text_view_ensure_layout (text_view);
  gtk_text_view_invalidate (text_view);

  if (priv->buffer)
    {
      GdkClipboard *clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (text_view));
      gtk_text_buffer_add_selection_clipboard (priv->buffer, clipboard);
    }

  /* Ensure updating the spot location. */
  gtk_text_view_update_im_spot_location (text_view);
}

static void
gtk_text_view_unrealize (GtkWidget *widget)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;

  text_view = GTK_TEXT_VIEW (widget);
  priv = text_view->priv;

  if (priv->buffer)
    {
      GdkClipboard *clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (text_view));
      gtk_text_buffer_remove_selection_clipboard (priv->buffer, clipboard);
    }

  gtk_text_view_remove_validate_idles (text_view);

  g_clear_pointer (&priv->popup_menu, gtk_widget_unparent);

  gtk_im_context_set_client_widget (priv->im_context, NULL);

  GTK_WIDGET_CLASS (gtk_text_view_parent_class)->unrealize (widget);
}

static void
gtk_text_view_map (GtkWidget *widget)
{
  gtk_widget_set_cursor_from_name (widget, "text");

  GTK_WIDGET_CLASS (gtk_text_view_parent_class)->map (widget);
}

static void
gtk_text_view_css_changed (GtkWidget         *widget,
                           GtkCssStyleChange *change)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;

  text_view = GTK_TEXT_VIEW (widget);
  priv = text_view->priv;

  GTK_WIDGET_CLASS (gtk_text_view_parent_class)->css_changed (widget, change);

  if ((change == NULL ||
       gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT |
                                             GTK_CSS_AFFECTS_TEXT_ATTRS |
                                             GTK_CSS_AFFECTS_BACKGROUND |
                                             GTK_CSS_AFFECTS_CONTENT)) &&
      priv->layout && priv->layout->default_style)
    {
      gtk_text_view_set_attributes_from_style (text_view,
                                               priv->layout->default_style);
      gtk_text_layout_default_style_changed (priv->layout);
    }

  if ((change == NULL ||
       gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT)) &&
      priv->layout)
    {
      gtk_text_view_update_pango_contexts (text_view);
    }
}

static void
gtk_text_view_direction_changed (GtkWidget        *widget,
                                 GtkTextDirection  previous_direction)
{
  GtkTextViewPrivate *priv = GTK_TEXT_VIEW (widget)->priv;

  if (priv->layout && priv->layout->default_style)
    {
      priv->layout->default_style->direction = gtk_widget_get_direction (widget);

      gtk_text_layout_default_style_changed (priv->layout);
    }
}

static void
gtk_text_view_update_pango_contexts (GtkTextView *text_view)
{
  GtkWidget *widget = GTK_WIDGET (text_view);
  GtkTextViewPrivate *priv = text_view->priv;
  gboolean update_ltr, update_rtl;

  if (!priv->layout)
    return;

 update_ltr = gtk_widget_update_pango_context (widget, priv->layout->ltr_context, GTK_TEXT_DIR_LTR);

 update_rtl = gtk_widget_update_pango_context (widget, priv->layout->rtl_context, GTK_TEXT_DIR_RTL);

  if (update_ltr || update_rtl)
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_bounds (get_buffer (text_view), &start, &end);
      gtk_text_layout_invalidate (priv->layout, &start, &end);
      gtk_widget_queue_draw (widget);
    }
}

static void
gtk_text_view_system_setting_changed (GtkWidget        *widget,
                                      GtkSystemSetting  setting)
{
  if (setting == GTK_SYSTEM_SETTING_DPI ||
      setting == GTK_SYSTEM_SETTING_FONT_NAME ||
      setting == GTK_SYSTEM_SETTING_FONT_CONFIG)
    {
      gtk_text_view_update_pango_contexts (GTK_TEXT_VIEW (widget));
    }
}

static void
gtk_text_view_state_flags_changed (GtkWidget     *widget,
                                   GtkStateFlags  previous_state)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextViewPrivate *priv = text_view->priv;
  GtkStateFlags state;

  if (!gtk_widget_is_sensitive (widget))
    {
      /* Clear any selection */
      gtk_text_view_unselect (text_view);
    }

  state = gtk_widget_get_state_flags (widget);
  gtk_css_node_set_state (priv->text_window->css_node, state);

  state &= ~GTK_STATE_FLAG_DROP_ACTIVE;

  gtk_css_node_set_state (priv->selection_node, state);

  if (priv->layout)
    gtk_text_layout_invalidate_selection (priv->layout);

  gtk_widget_queue_draw (widget);
}

static void
gtk_text_view_obscure_mouse_cursor (GtkTextView *text_view)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;

  if (text_view->priv->mouse_cursor_obscured)
    return;

  gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "none");

  display = gtk_widget_get_display (GTK_WIDGET (text_view));
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);

  text_view->priv->obscured_cursor_timestamp = gdk_device_get_timestamp (device);
  text_view->priv->mouse_cursor_obscured = TRUE;
}

static void
gtk_text_view_unobscure_mouse_cursor (GtkTextView *text_view)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;

  display = gtk_widget_get_display (GTK_WIDGET (text_view));
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);

  if (text_view->priv->mouse_cursor_obscured &&
      gdk_device_get_timestamp (device) != text_view->priv->obscured_cursor_timestamp)
    {
      gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "text");
      text_view->priv->mouse_cursor_obscured = FALSE;
    }
}

/*
 * Events
 */

static void
_text_window_to_widget_coords (GtkTextView *text_view,
                               int         *x,
                               int         *y)
{
  GtkTextViewPrivate *priv = text_view->priv;

  (*x) += priv->border_window_size.left;
  (*y) += priv->border_window_size.top;
}

static void
_widget_to_text_surface_coords (GtkTextView *text_view,
                               int         *x,
                               int         *y)
{
  GtkTextViewPrivate *priv = text_view->priv;

  (*x) -= priv->border_window_size.left;
  (*y) -= priv->border_window_size.top;
}

static void
gtk_text_view_set_handle_position (GtkTextView   *text_view,
                                   GtkTextHandle *handle,
                                   GtkTextIter   *iter)
{
  GtkTextViewPrivate *priv;
  GdkRectangle rect;
  int x, y;

  priv = text_view->priv;
  gtk_text_view_get_cursor_locations (text_view, iter, &rect, NULL);

  x = rect.x - priv->xoffset;
  y = rect.y - priv->yoffset;

  if (!gtk_text_handle_get_is_dragged (handle) &&
      (x < 0 || x > SCREEN_WIDTH (text_view) ||
       y < 0 || y > SCREEN_HEIGHT (text_view)))
    {
      /* Hide the handle if it's not being manipulated
       * and fell outside of the visible text area.
       */
      gtk_widget_set_visible (GTK_WIDGET (handle), FALSE);
    }
  else
    {
      GtkTextDirection dir = GTK_TEXT_DIR_LTR;
      GtkTextAttributes attributes = { 0 };

      gtk_widget_set_visible (GTK_WIDGET (handle), TRUE);

      rect.x = CLAMP (x, 0, SCREEN_WIDTH (text_view));
      rect.y = CLAMP (y, 0, SCREEN_HEIGHT (text_view));
      _text_window_to_widget_coords (text_view, &rect.x, &rect.y);

      gtk_text_handle_set_position (handle, &rect);

      if (gtk_text_iter_get_attributes (iter, &attributes))
        dir = attributes.direction;

      gtk_widget_set_direction (GTK_WIDGET (handle), dir);
    }
}

static void
gtk_text_view_show_magnifier (GtkTextView *text_view,
                              GtkTextIter *iter,
                              int          x,
                              int          y)
{
  cairo_rectangle_int_t rect;
  GtkTextViewPrivate *priv;
  GtkRequisition req;

#define N_LINES 1

  priv = text_view->priv;
  _gtk_text_view_ensure_magnifier (text_view);

  /* Set size/content depending on iter rect */
  gtk_text_view_get_iter_location (text_view, iter,
                                   (GdkRectangle *) &rect);
  rect.x = x + priv->xoffset;
  gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                         rect.x, rect.y, &rect.x, &rect.y);
  _text_window_to_widget_coords (text_view, &rect.x, &rect.y);
  req.height = rect.height * N_LINES *
    _gtk_magnifier_get_magnification (GTK_MAGNIFIER (priv->magnifier));
  req.width = MAX ((req.height * 4) / 3, 80);
  gtk_widget_set_size_request (priv->magnifier, req.width, req.height);

  _gtk_magnifier_set_coords (GTK_MAGNIFIER (priv->magnifier),
                             rect.x, rect.y + rect.height / 2);

  rect.x = CLAMP (rect.x, 0, gtk_widget_get_width (GTK_WIDGET (text_view)));
  rect.y += rect.height / 4;
  rect.height -= rect.height / 4;
  gtk_popover_set_pointing_to (GTK_POPOVER (priv->magnifier_popover), &rect);

  gtk_popover_popup (GTK_POPOVER (priv->magnifier_popover));

#undef N_LINES
}

static void
gtk_text_view_handle_dragged (GtkTextHandle *handle,
                              int            x,
                              int            y,
                              GtkTextView   *text_view)
{
  GtkTextViewPrivate *priv;
  GtkTextIter cursor, bound, iter, *old_iter;
  GtkTextBuffer *buffer;

  priv = text_view->priv;
  buffer = get_buffer (text_view);

  _widget_to_text_surface_coords (text_view, &x, &y);

  gtk_text_view_selection_bubble_popup_unset (text_view);
  gtk_text_layout_get_iter_at_pixel (priv->layout, &iter,
                                     x + priv->xoffset,
                                     y + priv->yoffset);

  gtk_text_buffer_get_iter_at_mark (buffer, &cursor,
                                    gtk_text_buffer_get_insert (buffer));
  gtk_text_buffer_get_iter_at_mark (buffer, &bound,
                                    gtk_text_buffer_get_selection_bound (buffer));


  if (handle == priv->text_handles[TEXT_HANDLE_CURSOR])
    {
      /* Avoid running past the other handle in selection mode */
      if (gtk_text_iter_compare (&iter, &bound) >= 0 &&
          gtk_widget_is_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])))
        {
          iter = bound;
          gtk_text_iter_backward_char (&iter);
        }

      old_iter = &cursor;
      gtk_text_view_set_handle_position (text_view, handle, &iter);
    }
  else if (handle == priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])
    {
      /* Avoid running past the other handle */
      if (gtk_text_iter_compare (&iter, &cursor) <= 0)
        {
          iter = cursor;
          gtk_text_iter_forward_char (&iter);
        }

      old_iter = &bound;
      gtk_text_view_set_handle_position (text_view, handle, &iter);
    }
  else
    g_assert_not_reached ();

  if (gtk_text_iter_compare (&iter, old_iter) != 0)
    {
      *old_iter = iter;

      if (handle == priv->text_handles[TEXT_HANDLE_CURSOR] &&
          gtk_text_handle_get_role (priv->text_handles[TEXT_HANDLE_CURSOR]) == GTK_TEXT_HANDLE_ROLE_CURSOR)
        gtk_text_buffer_place_cursor (buffer, &cursor);
      else
        gtk_text_buffer_select_range (buffer, &cursor, &bound);

      if (handle == priv->text_handles[TEXT_HANDLE_CURSOR])
        {
          text_view->priv->cursor_handle_dragged = TRUE;
          gtk_text_view_scroll_mark_onscreen (text_view,
                                              gtk_text_buffer_get_insert (buffer));
        }
      else if (handle == priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])
        {
          text_view->priv->selection_handle_dragged = TRUE;
          gtk_text_view_scroll_mark_onscreen (text_view,
                                              gtk_text_buffer_get_selection_bound (buffer));
        }

      gtk_text_view_update_handles (text_view);
    }

  gtk_text_view_show_magnifier (text_view, &iter, x, y);
}

static void
gtk_text_view_handle_drag_started (GtkTextHandle *handle,
                                   GtkTextView   *text_view)
{
  text_view->priv->cursor_handle_dragged = FALSE;
  text_view->priv->selection_handle_dragged = FALSE;
}

static void
gtk_text_view_handle_drag_finished (GtkTextHandle *handle,
                                    GtkTextView   *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (!priv->cursor_handle_dragged && !priv->selection_handle_dragged)
    {
      GtkTextBuffer *buffer;
      GtkTextIter cursor, start, end;
      GtkSettings *settings;
      guint double_click_time;

      settings = gtk_widget_get_settings (GTK_WIDGET (text_view));
      g_object_get (settings, "gtk-double-click-time", &double_click_time, NULL);
      if (g_get_monotonic_time() - priv->handle_place_time < double_click_time * 1000)
        {
          buffer = get_buffer (text_view);
          gtk_text_buffer_get_iter_at_mark (buffer, &cursor,
                                            gtk_text_buffer_get_insert (buffer));
          extend_selection (text_view, SELECT_WORDS, &cursor, &start, &end);
          gtk_text_buffer_select_range (buffer, &start, &end);

          gtk_text_view_update_handles (text_view);
        }
      else
        gtk_text_view_selection_bubble_popup_set (text_view);
    }

  if (priv->magnifier_popover)
    gtk_popover_popdown (GTK_POPOVER (priv->magnifier_popover));
}

static gboolean cursor_visible (GtkTextView *text_view);

static void
gtk_text_view_update_handles (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;
  GtkTextIter cursor, bound;
  GtkTextBuffer *buffer;

  if (!priv->text_handles_enabled)
    {
      if (priv->text_handles[TEXT_HANDLE_CURSOR])
	gtk_widget_set_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_CURSOR]), FALSE);
      if (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])
	gtk_widget_set_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND]), FALSE);
    }
  else
    {
      _gtk_text_view_ensure_text_handles (text_view);
	buffer = get_buffer (text_view);

      gtk_text_buffer_get_iter_at_mark (buffer, &cursor,
                                        gtk_text_buffer_get_insert (buffer));
      gtk_text_buffer_get_iter_at_mark (buffer, &bound,
                                        gtk_text_buffer_get_selection_bound (buffer));

      if (gtk_text_iter_compare (&cursor, &bound) == 0 && priv->editable)
        {
          /* Cursor mode */
          gtk_widget_set_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND]), FALSE);

          gtk_text_view_set_handle_position (text_view,
                                             priv->text_handles[TEXT_HANDLE_CURSOR],
                                             &cursor);
          gtk_text_handle_set_role (priv->text_handles[TEXT_HANDLE_CURSOR],
                                    GTK_TEXT_HANDLE_ROLE_CURSOR);
        }
      else if (gtk_text_iter_compare (&cursor, &bound) != 0)
        {
          /* Selection mode */
          gtk_text_view_set_handle_position (text_view,
                                             priv->text_handles[TEXT_HANDLE_CURSOR],
                                             &cursor);
          gtk_text_handle_set_role (priv->text_handles[TEXT_HANDLE_CURSOR],
                                    GTK_TEXT_HANDLE_ROLE_SELECTION_START);

          gtk_text_view_set_handle_position (text_view,
                                             priv->text_handles[TEXT_HANDLE_SELECTION_BOUND],
                                             &bound);
          gtk_text_handle_set_role (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND],
                                    GTK_TEXT_HANDLE_ROLE_SELECTION_END);
        }
      else
        {
          gtk_widget_set_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_CURSOR]), FALSE);
          gtk_widget_set_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND]), FALSE);
        }
    }
}

static gboolean
gtk_text_view_key_controller_key_pressed (GtkEventControllerKey *controller,
                                          guint                  keyval,
                                          guint                  keycode,
                                          GdkModifierType        state,
                                          GtkTextView           *text_view)
{
  GtkTextViewPrivate *priv;
  gboolean retval = FALSE;

  priv = text_view->priv;

  if (priv->layout == NULL || get_buffer (text_view) == NULL)
    return FALSE;

  /* Make sure input method knows where it is */
  flush_update_im_spot_location (text_view);

  /* use overall editability not can_insert, more predictable for users */

  if (priv->editable &&
      (keyval == GDK_KEY_Return ||
       keyval == GDK_KEY_ISO_Enter ||
       keyval == GDK_KEY_KP_Enter))
    {
      /* this won't actually insert the newline if the cursor isn't
       * editable
       */
      gtk_text_view_reset_im_context (text_view);
      gtk_text_view_commit_text (text_view, "\n");
      retval = TRUE;
    }
  /* Pass through Tab as literal tab, unless Control is held down */
  else if ((keyval == GDK_KEY_Tab ||
            keyval == GDK_KEY_KP_Tab ||
            keyval == GDK_KEY_ISO_Left_Tab) &&
           !(state & GDK_CONTROL_MASK))
    {
      /* If the text widget isn't editable overall, or if the application
       * has turned off "accepts_tab", move the focus instead
       */
      if (priv->accepts_tab && priv->editable)
	{
	  gtk_text_view_reset_im_context (text_view);
	  gtk_text_view_commit_text (text_view, "\t");
	}
      else
	g_signal_emit_by_name (text_view, "move-focus",
                               (state & GDK_SHIFT_MASK) ?
                               GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);

      retval = TRUE;
    }
  else
    retval = FALSE;

  gtk_text_view_reset_blink_time (text_view);
  gtk_text_view_pend_cursor_blink (text_view);

  text_view->priv->text_handles_enabled = FALSE;
  gtk_text_view_update_handles (text_view);

  gtk_text_view_selection_bubble_popup_unset (text_view);

  return retval;
}

static void
gtk_text_view_key_controller_im_update (GtkEventControllerKey *controller,
                                        GtkTextView           *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  priv->need_im_reset = TRUE;
}

static gboolean
get_iter_from_gesture (GtkTextView *text_view,
                       GtkGesture  *gesture,
                       GtkTextIter *iter,
                       int         *x,
                       int         *y)
{
  GdkEventSequence *sequence;
  GtkTextViewPrivate *priv;
  int xcoord, ycoord;
  double px, py;

  priv = text_view->priv;
  sequence =
    gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (!gtk_gesture_get_point (gesture, sequence, &px, &py))
    return FALSE;

  xcoord = px + priv->xoffset;
  ycoord = py + priv->yoffset;
  _widget_to_text_surface_coords (text_view, &xcoord, &ycoord);
  gtk_text_layout_get_iter_at_pixel (priv->layout, iter, xcoord, ycoord);

  if (x)
    *x = xcoord;
  if (y)
    *y = ycoord;

  return TRUE;
}

static void
gtk_text_view_click_gesture_pressed (GtkGestureClick *gesture,
                                     int              n_press,
                                     double           x,
                                     double           y,
                                     GtkTextView     *text_view)
{
  GdkEventSequence *sequence;
  GtkTextViewPrivate *priv;
  GdkEvent *event;
  gboolean is_touchscreen;
  GdkDevice *device;
  GtkTextIter iter;
  guint button;

  priv = text_view->priv;
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  gtk_widget_grab_focus (GTK_WIDGET (text_view));

  gtk_text_view_reset_blink_time (text_view);

  device = gdk_event_get_device ((GdkEvent *) event);
  is_touchscreen = gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN;

  if (n_press == 1)
    {
      /* Always emit reset when preedit is shown */
      priv->need_im_reset = TRUE;
      gtk_text_view_reset_im_context (text_view);
    }

  if (n_press == 1 &&
      gdk_event_triggers_context_menu (event))
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_CLAIMED);
      gtk_text_view_do_popup (text_view, event);
    }
  else if (button == GDK_BUTTON_MIDDLE &&
           get_middle_click_paste (text_view))
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture),
                             GTK_EVENT_SEQUENCE_CLAIMED);
      get_iter_from_gesture (text_view, GTK_GESTURE (gesture),
                             &iter, NULL, NULL);
      gtk_text_buffer_paste_clipboard (get_buffer (text_view),
                                       gtk_widget_get_primary_clipboard (GTK_WIDGET (text_view)),
                                       &iter,
                                       priv->editable);
    }
  else if (button == GDK_BUTTON_PRIMARY)
    {
      gboolean extends = FALSE;
      GdkModifierType state;

      state = gdk_event_get_modifier_state (event);

      if (state & GDK_SHIFT_MASK)
        extends = TRUE;

      if (n_press > 1)
	{
          GtkTextBuffer *buffer;
          GtkTextIter cur, ins;

          buffer = get_buffer (text_view);
          get_iter_from_gesture (text_view, GTK_GESTURE (gesture),
                                 &cur, NULL, NULL);
          gtk_text_buffer_get_iter_at_mark (buffer, &ins,
                                            gtk_text_buffer_get_insert (buffer));

          /* Reset count if double/triple clicking on a different line */
          if (gtk_text_iter_get_line (&cur) !=
              gtk_text_iter_get_line (&ins))
            {
              gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
              return;
            }
        }

      switch (n_press)
        {
        case 1:
          {
            /* If we're in the selection, start a drag copy/move of the
             * selection; otherwise, start creating a new selection.
             */
            GtkTextIter start, end;

            priv->text_handles_enabled = is_touchscreen;

            get_iter_from_gesture (text_view, GTK_GESTURE (gesture),
                                   &iter, NULL, NULL);

            if (gtk_text_buffer_get_selection_bounds (get_buffer (text_view),
                                                      &start, &end) &&
                gtk_text_iter_in_range (&iter, &start, &end) && !extends)
              {
                if (is_touchscreen)
                  {
                    gtk_gesture_set_state (GTK_GESTURE (gesture),
                                           GTK_EVENT_SEQUENCE_CLAIMED);
                    if (!priv->selection_bubble ||
                        !gtk_widget_get_visible (priv->selection_bubble))
                      {
                        gtk_text_view_selection_bubble_popup_set (text_view);
                        priv->text_handles_enabled = FALSE;
                      }
                    else
                      {
                        gtk_text_view_selection_bubble_popup_unset (text_view);
                      }
                  }
                else
                  {
                    /* Claim the sequence on the drag gesture, but attach no
                     * selection data, this is a special case to start DnD.
                     */
                    gtk_gesture_set_state (priv->drag_gesture,
                                           GTK_EVENT_SEQUENCE_CLAIMED);
                  }
                break;
              }
            else
              {
                gtk_text_view_selection_bubble_popup_unset (text_view);

                if (is_touchscreen)
                  priv->handle_place_time = g_get_monotonic_time ();
                else
                  gtk_text_view_start_selection_drag (text_view, &iter,
                                                      SELECT_CHARACTERS, extends);
              }
            break;
          }
        case 2:
        case 3:
          gtk_text_view_end_selection_drag (text_view);

          get_iter_from_gesture (text_view, GTK_GESTURE (gesture),
                                 &iter, NULL, NULL);
          gtk_text_view_start_selection_drag (text_view, &iter,
                                              n_press == 2 ? SELECT_WORDS : SELECT_LINES,
                                              extends);
          break;
        default:
          break;
        }

      gtk_text_view_update_handles (text_view);
    }

  if (n_press >= 3)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_text_view_click_gesture_released (GtkGestureClick *gesture,
                                      int              n_press,
                                      double           x,
                                      double           y,
                                      GtkTextView     *text_view)
{
  GdkEvent *event =
    gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (gesture));
  GtkTextViewPrivate *priv = text_view->priv;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  buffer = get_buffer (text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);

  if (gtk_text_iter_compare (&start, &end) == 0 &&
      gtk_text_iter_can_insert (&start, priv->editable))
    gtk_im_context_activate_osk (priv->im_context, event);
}

static void
direction_changed (GdkDevice  *device,
                   GParamSpec *pspec,
                   GtkTextView *text_view)
{
  gtk_text_view_check_keymap_direction (text_view);
}

static void
gtk_text_view_focus_in (GtkWidget *widget)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextViewPrivate *priv = text_view->priv;
  GdkSeat *seat;
  GdkDevice *keyboard;

  gtk_widget_queue_draw (widget);

  DV(g_print (G_STRLOC": focus_in\n"));

  gtk_text_view_reset_blink_time (text_view);

  if (cursor_visible (text_view) && priv->layout)
    {
      gtk_text_layout_set_cursor_visible (priv->layout, TRUE);
      gtk_text_view_check_cursor_blink (text_view);
    }

  seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
  if (seat)
    keyboard = gdk_seat_get_keyboard (seat);
  else
    keyboard = NULL;

  if (keyboard)
    g_signal_connect (keyboard, "notify::direction",
                      G_CALLBACK (direction_changed), text_view);
  gtk_text_view_check_keymap_direction (text_view);

  if (priv->editable)
    {
      priv->need_im_reset = TRUE;
      gtk_im_context_focus_in (priv->im_context);
    }
}

static void
gtk_text_view_focus_out (GtkWidget *widget)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextViewPrivate *priv = text_view->priv;
  GdkSeat *seat;
  GdkDevice *keyboard;

  gtk_text_view_end_selection_drag (text_view);

  gtk_widget_queue_draw (widget);

  DV(g_print (G_STRLOC": focus_out\n"));

  if (cursor_visible (text_view) && priv->layout)
    {
      gtk_text_view_check_cursor_blink (text_view);
      gtk_text_layout_set_cursor_visible (priv->layout, FALSE);
    }

  seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
  if (seat)
    keyboard = gdk_seat_get_keyboard (seat);
  else
    keyboard = NULL;
  if (keyboard)
    g_signal_handlers_disconnect_by_func (keyboard, direction_changed, text_view);
  gtk_text_view_selection_bubble_popup_unset (text_view);

  text_view->priv->text_handles_enabled = FALSE;
  gtk_text_view_update_handles (text_view);

  if (priv->editable)
    {
      priv->need_im_reset = TRUE;
      gtk_im_context_focus_out (priv->im_context);
    }
}

static void
gtk_text_view_motion (GtkEventController *controller,
                      double              x,
                      double              y,
                      gpointer            user_data)
{
  gtk_text_view_unobscure_mouse_cursor (GTK_TEXT_VIEW (user_data));
}

static void
gtk_text_view_paint (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  GtkTextView *text_view;
  GtkTextViewPrivate *priv;

  text_view = GTK_TEXT_VIEW (widget);
  priv = text_view->priv;

  g_return_if_fail (priv->layout != NULL);
  g_return_if_fail (priv->xoffset >= - priv->left_padding);
  g_return_if_fail (priv->yoffset >= - priv->top_margin);

  while (priv->first_validate_idle != 0)
    {
      DV (g_print (G_STRLOC": first_validate_idle: %d\n",
                   priv->first_validate_idle));
      gtk_text_view_flush_first_validate (text_view);
    }

  if (!priv->onscreen_validated)
    {
      g_warning (G_STRLOC ": somehow some text lines were modified or scrolling occurred since the last validation of lines on the screen - may be a text widget bug.");
      g_assert_not_reached ();
    }

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-priv->xoffset, -priv->yoffset));

  gtk_text_layout_snapshot (priv->layout,
                            widget,
                            snapshot,
                            &(GdkRectangle) {
                              priv->xoffset,
                              priv->yoffset,
                              gtk_widget_get_width (widget),
                              gtk_widget_get_height (widget)
                            },
                            priv->cursor_alpha);

  gtk_snapshot_restore (snapshot);
}

static void
draw_text (GtkWidget   *widget,
           GtkSnapshot *snapshot)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextViewPrivate *priv = text_view->priv;
  GtkCssStyle *style;
  gboolean did_save = FALSE;
  GtkCssBoxes boxes;

  if (priv->border_window_size.left || priv->border_window_size.top)
    {
      did_save = TRUE;
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot,
                              &GRAPHENE_POINT_INIT (priv->border_window_size.left,
                                                    priv->border_window_size.top));
    }

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (0,
                                               0,
                                               SCREEN_WIDTH (widget),
                                               SCREEN_HEIGHT (widget)));

  style = gtk_css_node_get_style (text_view->priv->text_window->css_node);
  gtk_css_boxes_init_border_box (&boxes, style,
                                 -priv->xoffset, -priv->yoffset - priv->top_margin,
                                 MAX (SCREEN_WIDTH (text_view), priv->width),
                                 MAX (SCREEN_HEIGHT (text_view), priv->height));
  gtk_css_style_snapshot_background (&boxes, snapshot);
  gtk_css_style_snapshot_border (&boxes, snapshot);

  if (GTK_TEXT_VIEW_GET_CLASS (text_view)->snapshot_layer != NULL)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-priv->xoffset, -priv->yoffset));
      GTK_TEXT_VIEW_GET_CLASS (text_view)->snapshot_layer (text_view, GTK_TEXT_VIEW_LAYER_BELOW_TEXT, snapshot);
      gtk_snapshot_restore (snapshot);
    }

  gtk_text_view_paint (widget, snapshot);

  if (GTK_TEXT_VIEW_GET_CLASS (text_view)->snapshot_layer != NULL)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-priv->xoffset, -priv->yoffset));
      GTK_TEXT_VIEW_GET_CLASS (text_view)->snapshot_layer (text_view, GTK_TEXT_VIEW_LAYER_ABOVE_TEXT, snapshot);
      gtk_snapshot_restore (snapshot);
    }

  gtk_snapshot_pop (snapshot);

  if (did_save)
    gtk_snapshot_restore (snapshot);
}

static inline void
snapshot_text_view_child (GtkWidget        *widget,
                          GtkTextViewChild *child,
                          GtkSnapshot      *snapshot)
{
  if (child != NULL)
    gtk_widget_snapshot_child (widget, GTK_WIDGET (child), snapshot);
}

static void
gtk_text_view_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextViewPrivate *priv = text_view->priv;
  const GList *iter;

  DV(g_print (">Exposed ("G_STRLOC")\n"));

  draw_text (widget, snapshot);

  snapshot_text_view_child (widget, priv->left_child, snapshot);
  snapshot_text_view_child (widget, priv->right_child, snapshot);
  snapshot_text_view_child (widget, priv->top_child, snapshot);
  snapshot_text_view_child (widget, priv->bottom_child, snapshot);
  snapshot_text_view_child (widget, priv->center_child, snapshot);

  for (iter = priv->anchored_children.head; iter; iter = iter->next)
    {
      const AnchoredChild *vc = iter->data;
      gtk_widget_snapshot_child (widget, vc->widget, snapshot);
    }
}

/**
 * gtk_text_view_remove:
 * @text_view: a `GtkTextView`
 * @child: the child to remove
 *
 * Removes a child widget from @text_view.
 */
void
gtk_text_view_remove (GtkTextView *text_view,
                      GtkWidget   *child)
{
  GtkTextViewPrivate *priv = text_view->priv;
  AnchoredChild *ac;

  if (GTK_IS_TEXT_VIEW_CHILD (child))
    {
      GtkTextViewChild *vc = GTK_TEXT_VIEW_CHILD (child);
      GtkTextViewChild **vcp;

      if (vc == priv->left_child)
        vcp = &priv->left_child;
      else if (vc == priv->right_child)
        vcp = &priv->right_child;
      else if (vc == priv->top_child)
        vcp = &priv->top_child;
      else if (vc == priv->bottom_child)
        vcp = &priv->bottom_child;
      else if (vc == priv->center_child)
        vcp = &priv->center_child;
      else
        vcp = NULL;

      if (vcp)
        {
          *vcp = NULL;
          gtk_widget_unparent (child);
          g_object_unref (child);
          return;
        }
    }

  ac = g_object_get_qdata (G_OBJECT (child), quark_text_view_child);

  if (ac == NULL)
    {
      g_warning ("%s is not a child of %s",
                 G_OBJECT_TYPE_NAME (child),
                 G_OBJECT_TYPE_NAME (text_view));
      return;
    }

  g_queue_unlink (&priv->anchored_children, &ac->link);
  gtk_widget_unparent (ac->widget);
  anchored_child_free (ac);
}

#define CURSOR_ON_MULTIPLIER 2
#define CURSOR_OFF_MULTIPLIER 1
#define CURSOR_PEND_MULTIPLIER 3
#define CURSOR_DIVIDER 3

static gboolean
cursor_blinks (GtkTextView *text_view)
{
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (text_view));

#ifdef DEBUG_VALIDATION_AND_SCROLLING
  return FALSE;
#endif

  if (gtk_widget_get_mapped (GTK_WIDGET (text_view)) &&
      gtk_window_is_active (GTK_WINDOW (root)) &&
      gtk_widget_has_focus (GTK_WIDGET (text_view)))
    {
      GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (text_view));
      gboolean blink;

      g_object_get (settings, "gtk-cursor-blink", &blink, NULL);

      if (!blink)
        return FALSE;

      if (text_view->priv->editable)
        {
          GtkTextMark *insert;
          GtkTextIter iter;

          insert = gtk_text_buffer_get_insert (get_buffer (text_view));
          gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &iter, insert);

          if (gtk_text_iter_editable (&iter, text_view->priv->editable))
            return blink;
        }
    }

  return FALSE;
}

static gboolean
cursor_visible (GtkTextView *text_view)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (text_view));
  gboolean use_caret;

  g_object_get (settings, "gtk-keynav-use-caret", &use_caret, NULL);

   return use_caret || text_view->priv->cursor_visible;
}

static gboolean
get_middle_click_paste (GtkTextView *text_view)
{
  GtkSettings *settings;
  gboolean paste;

  settings = gtk_widget_get_settings (GTK_WIDGET (text_view));
  g_object_get (settings, "gtk-enable-primary-paste", &paste, NULL);

  return paste;
}

static int
get_cursor_time (GtkTextView *text_view)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (text_view));
  int time;

  g_object_get (settings, "gtk-cursor-blink-time", &time, NULL);

  return time;
}

static int
get_cursor_blink_timeout (GtkTextView *text_view)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (text_view));
  int time;

  g_object_get (settings, "gtk-cursor-blink-timeout", &time, NULL);

  return time;
}


/*
 * Blink!
 */

typedef struct {
  guint64 start;
  guint64 end;
} BlinkData;

static gboolean blink_cb (GtkWidget     *widget,
                          GdkFrameClock *clock,
                          gpointer       user_data);


static void
add_blink_timeout (GtkTextView *self,
                   gboolean     delay)
{
  GtkTextViewPrivate *priv = self->priv;
  BlinkData *data;
  int blink_time;

  priv->blink_start_time = g_get_monotonic_time ();
  priv->cursor_alpha = 1.0;

  blink_time = get_cursor_time (self);

  data = g_new (BlinkData, 1);
  data->start = priv->blink_start_time;
  if (delay)
    data->start += blink_time * 1000 / 2;
  data->end = data->start + blink_time * 1000;

  priv->blink_tick = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                   blink_cb,
                                                   data,
                                                   g_free);
}

static void
remove_blink_timeout (GtkTextView *self)
{
  GtkTextViewPrivate *priv = self->priv;

  if (priv->blink_tick)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), priv->blink_tick);
      priv->blink_tick = 0;
    }
}

static float
blink_alpha (float phase)
{
  /* keep it simple, and split the blink cycle evenly
   * into visible, fading out, invisible, fading in
   */
  if (phase < 0.25)
    return 1;
  else if (phase < 0.5)
    return 1 - 4 * (phase - 0.25);
  else if (phase < 0.75)
    return 0;
  else
    return 4 * (phase - 0.75);
}

static gboolean
blink_cb (GtkWidget     *widget,
          GdkFrameClock *clock,
          gpointer       user_data)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextViewPrivate *priv = text_view->priv;
  BlinkData *data = user_data;
  int blink_timeout;
  int blink_time;
  guint64 now;
  float phase;
  float  alpha;

  g_assert (priv->layout);
  g_assert (cursor_visible (text_view));

  blink_timeout = get_cursor_blink_timeout (text_view);
  blink_time = get_cursor_time (text_view);

  now = g_get_monotonic_time ();

  if (now > priv->blink_start_time + blink_timeout * 1000000)
    {
      /* we've blinked enough without the user doing anything, stop blinking */
      priv->cursor_alpha = 1.0;
      remove_blink_timeout (text_view);
      gtk_widget_queue_draw (widget);

      return G_SOURCE_REMOVE;
    }

  phase = (now - data->start) / (float) (data->end - data->start);

  if (now >= data->end)
    {
      data->start = data->end;
      data->end = data->start + blink_time * 1000;
    }

  alpha = blink_alpha (phase);

  if (priv->cursor_alpha != alpha)
    {
      priv->cursor_alpha = alpha;
      gtk_widget_queue_draw (widget);
    }

  return G_SOURCE_CONTINUE;
}


static void
gtk_text_view_stop_cursor_blink (GtkTextView *text_view)
{
  remove_blink_timeout (text_view);
}

static void
gtk_text_view_check_cursor_blink (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (cursor_blinks (text_view) && cursor_visible (text_view))
    {
      if (!priv->blink_tick)
        add_blink_timeout (text_view, FALSE);
    }
  else
    {
      if (priv->blink_tick)
        remove_blink_timeout (text_view);
    }
}

static void
gtk_text_view_pend_cursor_blink (GtkTextView *text_view)
{
  if (cursor_blinks (text_view) && cursor_visible (text_view))
    {
      remove_blink_timeout (text_view);
      add_blink_timeout (text_view, TRUE);
    }
}

static void
gtk_text_view_reset_blink_time (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  priv->blink_start_time = g_get_monotonic_time ();
}


/*
 * Key binding handlers
 */

static gboolean
gtk_text_view_move_iter_by_lines (GtkTextView *text_view,
                                  GtkTextIter *newplace,
                                  int          count)
{
  gboolean ret = TRUE;

  while (count < 0)
    {
      ret = gtk_text_layout_move_iter_to_previous_line (text_view->priv->layout, newplace);
      count++;
    }

  while (count > 0)
    {
      ret = gtk_text_layout_move_iter_to_next_line (text_view->priv->layout, newplace);
      count--;
    }

  return ret;
}

static void
move_cursor (GtkTextView       *text_view,
             const GtkTextIter *new_location,
             gboolean           extend_selection)
{
  if (extend_selection)
    gtk_text_buffer_move_mark_by_name (get_buffer (text_view),
                                       "insert",
                                       new_location);
  else
      gtk_text_buffer_place_cursor (get_buffer (text_view),
				    new_location);
  gtk_text_view_check_cursor_blink (text_view);
}

static gboolean
iter_line_is_rtl (const GtkTextIter *iter)
{
  GtkTextIter start, end;
  char *text;
  PangoDirection direction;

  start = end = *iter;
  gtk_text_iter_set_line_offset (&start, 0);
  gtk_text_iter_forward_line (&end);
  text = gtk_text_iter_get_visible_text (&start, &end);
  direction = gdk_find_base_dir (text, -1);

  g_free (text);

  return direction == PANGO_DIRECTION_RTL;
}

static void
gtk_text_view_move_cursor (GtkTextView     *text_view,
                           GtkMovementStep  step,
                           int              count,
                           gboolean         extend_selection)
{
  GtkTextViewPrivate *priv;
  GtkTextIter insert;
  GtkTextIter newplace;
  gboolean cancel_selection = FALSE;
  int cursor_x_pos = 0;
  GtkDirectionType leave_direction = -1;

  priv = text_view->priv;

  if (!cursor_visible (text_view))
    {
      GtkScrollStep scroll_step;
      double old_xpos, old_ypos;

      switch (step)
	{
        case GTK_MOVEMENT_VISUAL_POSITIONS:
          leave_direction = count > 0 ? GTK_DIR_RIGHT : GTK_DIR_LEFT;
          G_GNUC_FALLTHROUGH;
        case GTK_MOVEMENT_LOGICAL_POSITIONS:
        case GTK_MOVEMENT_WORDS:
	  scroll_step = GTK_SCROLL_HORIZONTAL_STEPS;
	  break;
        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	  scroll_step = GTK_SCROLL_HORIZONTAL_ENDS;
	  break;
        case GTK_MOVEMENT_DISPLAY_LINES:
          leave_direction = count > 0 ? GTK_DIR_DOWN : GTK_DIR_UP;
          G_GNUC_FALLTHROUGH;
        case GTK_MOVEMENT_PARAGRAPHS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
	  scroll_step = GTK_SCROLL_STEPS;
	  break;
	case GTK_MOVEMENT_PAGES:
	  scroll_step = GTK_SCROLL_PAGES;
	  break;
	case GTK_MOVEMENT_HORIZONTAL_PAGES:
	  scroll_step = GTK_SCROLL_HORIZONTAL_PAGES;
	  break;
	case GTK_MOVEMENT_BUFFER_ENDS:
	  scroll_step = GTK_SCROLL_ENDS;
	  break;
	default:
          scroll_step = GTK_SCROLL_PAGES;
          break;
	}

      old_xpos = gtk_adjustment_get_value (priv->hadjustment);
      old_ypos = gtk_adjustment_get_value (priv->vadjustment);
      gtk_text_view_move_viewport (text_view, scroll_step, count);
      if ((old_xpos == gtk_adjustment_get_target_value (priv->hadjustment) &&
           old_ypos == gtk_adjustment_get_target_value (priv->vadjustment)) &&
          leave_direction != (GtkDirectionType)-1 &&
          !gtk_widget_keynav_failed (GTK_WIDGET (text_view),
                                     leave_direction))
        {
          g_signal_emit_by_name (text_view, "move-focus", leave_direction);
        }

      return;
    }

  if (step == GTK_MOVEMENT_PAGES)
    {
      if (!gtk_text_view_scroll_pages (text_view, count, extend_selection))
        gtk_widget_error_bell (GTK_WIDGET (text_view));

      gtk_text_view_check_cursor_blink (text_view);
      gtk_text_view_pend_cursor_blink (text_view);
      return;
    }
  else if (step == GTK_MOVEMENT_HORIZONTAL_PAGES)
    {
      if (!gtk_text_view_scroll_hpages (text_view, count, extend_selection))
        gtk_widget_error_bell (GTK_WIDGET (text_view));

      gtk_text_view_check_cursor_blink (text_view);
      gtk_text_view_pend_cursor_blink (text_view);
      return;
    }

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_insert (get_buffer (text_view)));

  if (! extend_selection)
    {
      gboolean move_forward = count > 0;
      GtkTextIter sel_bound;

      gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &sel_bound,
                                        gtk_text_buffer_get_selection_bound (get_buffer (text_view)));

      if (iter_line_is_rtl (&insert))
        move_forward = !move_forward;

      /* if we move forward, assume the cursor is at the end of the selection;
       * if we move backward, assume the cursor is at the start
       */
      if (move_forward)
        gtk_text_iter_order (&sel_bound, &insert);
      else
        gtk_text_iter_order (&insert, &sel_bound);

      /* if we actually have a selection, just move *to* the beginning/end
       * of the selection and not *from* there on LOGICAL_POSITIONS
       * and VISUAL_POSITIONS movement
       */
      if (! gtk_text_iter_equal (&sel_bound, &insert))
        cancel_selection = TRUE;
    }

  newplace = insert;

  if (step == GTK_MOVEMENT_DISPLAY_LINES)
    gtk_text_view_get_virtual_cursor_pos (text_view, &insert, &cursor_x_pos, NULL);

  switch (step)
    {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
      if (! cancel_selection)
        gtk_text_iter_forward_visible_cursor_positions (&newplace, count);
      break;

    case GTK_MOVEMENT_VISUAL_POSITIONS:
      if (! cancel_selection)
        gtk_text_layout_move_iter_visually (priv->layout,
                                            &newplace, count);
      break;

    case GTK_MOVEMENT_WORDS:
      if (iter_line_is_rtl (&newplace))
        count *= -1;

      if (count < 0)
        {
          if (!gtk_text_iter_backward_visible_word_starts (&newplace, -count))
            gtk_text_iter_set_line_offset (&newplace, 0);
        }
      else if (count > 0)
	{
	  if (!gtk_text_iter_forward_visible_word_ends (&newplace, count))
	    gtk_text_iter_forward_to_line_end (&newplace);
	}
      break;

    case GTK_MOVEMENT_DISPLAY_LINES:
      if (count < 0)
        {
          leave_direction = GTK_DIR_UP;

          if (gtk_text_view_move_iter_by_lines (text_view, &newplace, count))
            gtk_text_layout_move_iter_to_x (priv->layout, &newplace, cursor_x_pos);
          else
            gtk_text_iter_set_line_offset (&newplace, 0);
        }
      if (count > 0)
        {
          leave_direction = GTK_DIR_DOWN;

          if (gtk_text_view_move_iter_by_lines (text_view, &newplace, count))
            gtk_text_layout_move_iter_to_x (priv->layout, &newplace, cursor_x_pos);
          else
            gtk_text_iter_forward_to_line_end (&newplace);
        }
      break;

    case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
      if (count > 1)
        gtk_text_view_move_iter_by_lines (text_view, &newplace, --count);
      else if (count < -1)
        gtk_text_view_move_iter_by_lines (text_view, &newplace, ++count);

      if (count != 0)
        gtk_text_layout_move_iter_to_line_end (priv->layout, &newplace, count);
      break;

    case GTK_MOVEMENT_PARAGRAPHS:
      if (count > 0)
        {
          if (!gtk_text_iter_ends_line (&newplace))
            {
              gtk_text_iter_forward_to_line_end (&newplace);
              --count;
            }
          gtk_text_iter_forward_visible_lines (&newplace, count);
          gtk_text_iter_forward_to_line_end (&newplace);
        }
      else if (count < 0)
        {
          if (gtk_text_iter_get_line_offset (&newplace) > 0)
	    gtk_text_iter_set_line_offset (&newplace, 0);
          gtk_text_iter_forward_visible_lines (&newplace, count);
          gtk_text_iter_set_line_offset (&newplace, 0);
        }
      break;

    case GTK_MOVEMENT_PARAGRAPH_ENDS:
      if (count > 0)
        {
          if (!gtk_text_iter_ends_line (&newplace))
            gtk_text_iter_forward_to_line_end (&newplace);
        }
      else if (count < 0)
        {
          gtk_text_iter_set_line_offset (&newplace, 0);
        }
      break;

    case GTK_MOVEMENT_BUFFER_ENDS:
      if (count > 0)
        gtk_text_buffer_get_end_iter (get_buffer (text_view), &newplace);
      else if (count < 0)
        gtk_text_buffer_get_iter_at_offset (get_buffer (text_view), &newplace, 0);
     break;

    case GTK_MOVEMENT_PAGES:
    case GTK_MOVEMENT_HORIZONTAL_PAGES:
      /* We handle these cases above and return early from them. */
    default:
      g_assert_not_reached ();
      break;
    }

  /* call move_cursor() even if the cursor hasn't moved, since it
     cancels the selection
  */
  move_cursor (text_view, &newplace, extend_selection);

  DV(g_print (G_STRLOC": scrolling onscreen\n"));
  gtk_text_view_scroll_mark_onscreen (text_view,
                                      gtk_text_buffer_get_insert (get_buffer (text_view)));

  if (step == GTK_MOVEMENT_DISPLAY_LINES)
    gtk_text_view_set_virtual_cursor_pos (text_view, cursor_x_pos, -1);

  if (gtk_text_iter_equal (&insert, &newplace))
    {
      if (leave_direction != (GtkDirectionType)-1)
        {
          if (!gtk_widget_keynav_failed (GTK_WIDGET (text_view), leave_direction))
            g_signal_emit_by_name (text_view, "move-focus", leave_direction);
        }
      else if (!cancel_selection)
        gtk_widget_error_bell (GTK_WIDGET (text_view));
    }

  gtk_text_view_check_cursor_blink (text_view);
  gtk_text_view_pend_cursor_blink (text_view);

  priv->need_im_reset = TRUE;
  gtk_text_view_reset_im_context (text_view);
}

static void
gtk_text_view_move_viewport (GtkTextView     *text_view,
                             GtkScrollStep    step,
                             int              count)
{
  GtkAdjustment *adjustment;
  double increment;

  switch (step)
    {
    case GTK_SCROLL_STEPS:
    case GTK_SCROLL_PAGES:
    case GTK_SCROLL_ENDS:
      adjustment = text_view->priv->vadjustment;
      break;
    case GTK_SCROLL_HORIZONTAL_STEPS:
    case GTK_SCROLL_HORIZONTAL_PAGES:
    case GTK_SCROLL_HORIZONTAL_ENDS:
      adjustment = text_view->priv->hadjustment;
      break;
    default:
      adjustment = text_view->priv->vadjustment;
      break;
    }

  switch (step)
    {
    case GTK_SCROLL_STEPS:
    case GTK_SCROLL_HORIZONTAL_STEPS:
      increment = gtk_adjustment_get_step_increment (adjustment);
      break;
    case GTK_SCROLL_PAGES:
    case GTK_SCROLL_HORIZONTAL_PAGES:
      increment = gtk_adjustment_get_page_increment (adjustment);
      break;
    case GTK_SCROLL_ENDS:
    case GTK_SCROLL_HORIZONTAL_ENDS:
      increment = gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment);
      break;
    default:
      increment = 0.0;
      break;
    }

  gtk_adjustment_animate_to_value (adjustment, gtk_adjustment_get_value (adjustment) + count * increment);
}

static void
gtk_text_view_set_anchor (GtkTextView *text_view)
{
  GtkTextIter insert;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_insert (get_buffer (text_view)));

  gtk_text_buffer_create_mark (get_buffer (text_view), "anchor", &insert, TRUE);
}

static gboolean
gtk_text_view_scroll_pages (GtkTextView *text_view,
                            int          count,
                            gboolean     extend_selection)
{
  GtkTextViewPrivate *priv;
  GtkAdjustment *adjustment;
  int cursor_x_pos, cursor_y_pos;
  GtkTextMark *insert_mark;
  GtkTextIter old_insert;
  GtkTextIter new_insert;
  GtkTextIter anchor;
  double newval;
  double oldval;
  int y0, y1;

  priv = text_view->priv;

  g_return_val_if_fail (priv->vadjustment != NULL, FALSE);

  adjustment = priv->vadjustment;

  insert_mark = gtk_text_buffer_get_insert (get_buffer (text_view));

  /* Make sure we start from the current cursor position, even
   * if it was offscreen, but don't queue more scrolls if we're
   * already behind.
   */
  if (priv->pending_scroll)
    cancel_pending_scroll (text_view);
  else
    gtk_text_view_scroll_mark_onscreen (text_view, insert_mark);

  /* Validate the region that will be brought into view by the cursor motion
   */
  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view),
                                    &old_insert, insert_mark);

  if (count < 0)
    {
      gtk_text_view_get_first_para_iter (text_view, &anchor);
      y0 = gtk_adjustment_get_page_size (adjustment);
      y1 = gtk_adjustment_get_page_size (adjustment) + count * gtk_adjustment_get_page_increment (adjustment);
    }
  else
    {
      gtk_text_view_get_first_para_iter (text_view, &anchor);
      y0 = count * gtk_adjustment_get_page_increment (adjustment) + gtk_adjustment_get_page_size (adjustment);
      y1 = 0;
    }

  gtk_text_layout_validate_yrange (priv->layout, &anchor, y0, y1);
  /* FIXME do we need to update the adjustment ranges here? */

  new_insert = old_insert;

  if (count < 0 && gtk_adjustment_get_value (adjustment) <= (gtk_adjustment_get_lower (adjustment) + 1e-12))
    {
      /* already at top, just be sure we are at offset 0 */
      gtk_text_buffer_get_start_iter (get_buffer (text_view), &new_insert);
      move_cursor (text_view, &new_insert, extend_selection);
    }
  else if (count > 0 && gtk_adjustment_get_value (adjustment) >= (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment) - 1e-12))
    {
      /* already at bottom, just be sure we are at the end */
      gtk_text_buffer_get_end_iter (get_buffer (text_view), &new_insert);
      move_cursor (text_view, &new_insert, extend_selection);
    }
  else
    {
      gtk_text_view_get_virtual_cursor_pos (text_view, NULL, &cursor_x_pos, &cursor_y_pos);

      oldval = newval = gtk_adjustment_get_target_value (adjustment);
      newval += count * gtk_adjustment_get_page_increment (adjustment);

      gtk_adjustment_animate_to_value (adjustment, newval);
      cursor_y_pos += newval - oldval;

      gtk_text_layout_get_iter_at_pixel (priv->layout, &new_insert, cursor_x_pos, cursor_y_pos);

      move_cursor (text_view, &new_insert, extend_selection);

      gtk_text_view_set_virtual_cursor_pos (text_view, cursor_x_pos, cursor_y_pos);
    }

  /* Adjust to have the cursor _entirely_ onscreen, move_mark_onscreen
   * only guarantees 1 pixel onscreen.
   */
  DV(g_print (G_STRLOC": scrolling onscreen\n"));

  return !gtk_text_iter_equal (&old_insert, &new_insert);
}

static gboolean
gtk_text_view_scroll_hpages (GtkTextView *text_view,
                             int          count,
                             gboolean     extend_selection)
{
  GtkTextViewPrivate *priv;
  GtkAdjustment *adjustment;
  int cursor_x_pos, cursor_y_pos;
  GtkTextMark *insert_mark;
  GtkTextIter old_insert;
  GtkTextIter new_insert;
  double newval;
  double oldval;
  int y, height;

  priv = text_view->priv;

  g_return_val_if_fail (priv->hadjustment != NULL, FALSE);

  adjustment = priv->hadjustment;

  insert_mark = gtk_text_buffer_get_insert (get_buffer (text_view));

  /* Make sure we start from the current cursor position, even
   * if it was offscreen, but don't queue more scrolls if we're
   * already behind.
   */
  if (priv->pending_scroll)
    cancel_pending_scroll (text_view);
  else
    gtk_text_view_scroll_mark_onscreen (text_view, insert_mark);

  /* Validate the line that we're moving within.
   */
  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view),
                                    &old_insert, insert_mark);

  gtk_text_layout_get_line_yrange (priv->layout, &old_insert, &y, &height);
  gtk_text_layout_validate_yrange (priv->layout, &old_insert, y, y + height);
  /* FIXME do we need to update the adjustment ranges here? */

  new_insert = old_insert;

  if (count < 0 && gtk_adjustment_get_value (adjustment) <= (gtk_adjustment_get_lower (adjustment) + 1e-12))
    {
      /* already at far left, just be sure we are at offset 0 */
      gtk_text_iter_set_line_offset (&new_insert, 0);
      move_cursor (text_view, &new_insert, extend_selection);
    }
  else if (count > 0 && gtk_adjustment_get_value (adjustment) >= (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment) - 1e-12))
    {
      /* already at far right, just be sure we are at the end */
      if (!gtk_text_iter_ends_line (&new_insert))
	  gtk_text_iter_forward_to_line_end (&new_insert);
      move_cursor (text_view, &new_insert, extend_selection);
    }
  else
    {
      gtk_text_view_get_virtual_cursor_pos (text_view, NULL, &cursor_x_pos, &cursor_y_pos);

      oldval = newval = gtk_adjustment_get_target_value (adjustment);
      newval += count * gtk_adjustment_get_page_increment (adjustment);

      gtk_adjustment_animate_to_value (adjustment, newval);
      cursor_x_pos += newval - oldval;

      gtk_text_layout_get_iter_at_pixel (priv->layout, &new_insert, cursor_x_pos, cursor_y_pos);
      move_cursor (text_view, &new_insert, extend_selection);

      gtk_text_view_set_virtual_cursor_pos (text_view, cursor_x_pos, cursor_y_pos);
    }

  /*  FIXME for lines shorter than the overall widget width, this results in a
   *  "bounce" effect as we scroll to the right of the widget, then scroll
   *  back to get the end of the line onscreen.
   *      http://bugzilla.gnome.org/show_bug.cgi?id=68963
   */

  /* Adjust to have the cursor _entirely_ onscreen, move_mark_onscreen
   * only guarantees 1 pixel onscreen.
   */
  DV(g_print (G_STRLOC": scrolling onscreen\n"));

  return !gtk_text_iter_equal (&old_insert, &new_insert);
}

static gboolean
whitespace (gunichar ch, gpointer user_data)
{
  return (ch == ' ' || ch == '\t');
}

static gboolean
not_whitespace (gunichar ch, gpointer user_data)
{
  return !whitespace (ch, user_data);
}

static gboolean
find_whitepace_region (const GtkTextIter *center,
                       GtkTextIter *start, GtkTextIter *end)
{
  *start = *center;
  *end = *center;

  if (gtk_text_iter_backward_find_char (start, not_whitespace, NULL, NULL))
    gtk_text_iter_forward_char (start); /* we want the first whitespace... */
  if (whitespace (gtk_text_iter_get_char (end), NULL))
    gtk_text_iter_forward_find_char (end, not_whitespace, NULL, NULL);

  return !gtk_text_iter_equal (start, end);
}

static void
gtk_text_view_insert_at_cursor (GtkTextView *text_view,
                                const char *str)
{
  if (!gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view), str, -1,
                                                     text_view->priv->editable))
    {
      gtk_widget_error_bell (GTK_WIDGET (text_view));
    }
}

static void
gtk_text_view_delete_from_cursor (GtkTextView   *text_view,
                                  GtkDeleteType  type,
                                  int            count)
{
  GtkTextViewPrivate *priv;
  GtkTextIter insert;
  GtkTextIter start;
  GtkTextIter end;
  gboolean leave_one = FALSE;

  priv = text_view->priv;

  /* If a selection exists, we operate on it first */
  if (gtk_text_buffer_delete_selection (get_buffer (text_view), TRUE,
                                        priv->editable))
    {
      priv->need_im_reset = TRUE;
      gtk_text_view_reset_im_context (text_view);
      return;
    }

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_insert (get_buffer (text_view)));

  start = insert;
  end = insert;

  switch (type)
    {
    case GTK_DELETE_CHARS:
      gtk_text_iter_forward_cursor_positions (&end, count);
      break;

    case GTK_DELETE_WORD_ENDS:
      if (count > 0)
        gtk_text_iter_forward_word_ends (&end, count);
      else if (count < 0)
        {
          if (!gtk_text_iter_backward_word_starts (&start, 0 - count))
            gtk_text_iter_set_line_offset (&start, 0);
        }
      break;

    case GTK_DELETE_WORDS:
      break;

    case GTK_DELETE_DISPLAY_LINE_ENDS:
      break;

    case GTK_DELETE_DISPLAY_LINES:
      break;

    case GTK_DELETE_PARAGRAPH_ENDS:
      if (count > 0)
        {
          /* If we're already at a newline, we need to
           * simply delete that newline, instead of
           * moving to the next one.
           */
          if (gtk_text_iter_ends_line (&end))
            {
              gtk_text_iter_forward_line (&end);
              --count;
            }

          while (count > 0)
            {
              if (!gtk_text_iter_forward_to_line_end (&end))
                break;

              --count;
            }
        }
      else if (count < 0)
        {
          if (gtk_text_iter_starts_line (&start))
            {
              gtk_text_iter_backward_line (&start);
              if (!gtk_text_iter_ends_line (&end))
                gtk_text_iter_forward_to_line_end (&start);
            }
          else
            {
              gtk_text_iter_set_line_offset (&start, 0);
            }
          ++count;

          gtk_text_iter_backward_lines (&start, -count);
        }
      break;

    case GTK_DELETE_PARAGRAPHS:
      if (count > 0)
        {
          gtk_text_iter_set_line_offset (&start, 0);
          gtk_text_iter_forward_to_line_end (&end);

          /* Do the lines beyond the first. */
          while (count > 1)
            {
              gtk_text_iter_forward_to_line_end (&end);

              --count;
            }
        }

      /* FIXME negative count? */

      break;

    case GTK_DELETE_WHITESPACE:
      {
        find_whitepace_region (&insert, &start, &end);
      }
      break;

    default:
      break;
    }

  if (!gtk_text_iter_equal (&start, &end))
    {
      gtk_text_buffer_begin_user_action (get_buffer (text_view));

      if (gtk_text_buffer_delete_interactive (get_buffer (text_view), &start, &end,
                                              priv->editable))
        {
          if (leave_one)
            gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view),
                                                          " ", 1,
                                                          priv->editable);
        }
      else
        {
          gtk_widget_error_bell (GTK_WIDGET (text_view));
        }

      gtk_text_buffer_end_user_action (get_buffer (text_view));
      gtk_text_view_set_virtual_cursor_pos (text_view, -1, -1);

      DV(g_print (G_STRLOC": scrolling onscreen\n"));
      gtk_text_view_scroll_mark_onscreen (text_view,
                                          gtk_text_buffer_get_insert (get_buffer (text_view)));
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (text_view));
    }

  priv->need_im_reset = TRUE;
  gtk_text_view_reset_im_context (text_view);
}

static void
gtk_text_view_backspace (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;
  GtkTextIter insert;

  priv = text_view->priv;

  /* Backspace deletes the selection, if one exists */
  if (gtk_text_buffer_delete_selection (get_buffer (text_view), TRUE,
                                        priv->editable))
    {
      priv->need_im_reset = TRUE;
      gtk_text_view_reset_im_context (text_view);
      return;
    }

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view),
                                    &insert,
                                    gtk_text_buffer_get_insert (get_buffer (text_view)));

  if (gtk_text_buffer_backspace (get_buffer (text_view), &insert,
				 TRUE, priv->editable))
    {
      gtk_text_view_set_virtual_cursor_pos (text_view, -1, -1);
      DV(g_print (G_STRLOC": scrolling onscreen\n"));
      gtk_text_view_scroll_mark_onscreen (text_view,
                                          gtk_text_buffer_get_insert (get_buffer (text_view)));

      priv->need_im_reset = TRUE;
      gtk_text_view_reset_im_context (text_view);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (text_view));
    }
}

static void
gtk_text_view_cut_clipboard (GtkTextView *text_view)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));

  gtk_text_buffer_cut_clipboard (get_buffer (text_view),
				 clipboard,
				 text_view->priv->editable);
  DV(g_print (G_STRLOC": scrolling onscreen\n"));
  gtk_text_view_scroll_mark_onscreen (text_view,
                                      gtk_text_buffer_get_insert (get_buffer (text_view)));
  gtk_text_view_selection_bubble_popup_unset (text_view);
}

static void
gtk_text_view_copy_clipboard (GtkTextView *text_view)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));

  gtk_text_buffer_copy_clipboard (get_buffer (text_view), clipboard);

  /* on copy do not scroll, we are already onscreen */
}

static void
gtk_text_view_paste_clipboard (GtkTextView *text_view)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));

  text_view->priv->scroll_after_paste = TRUE;

  gtk_text_buffer_paste_clipboard (get_buffer (text_view),
				   clipboard,
				   NULL,
				   text_view->priv->editable);
}

static void
gtk_text_view_paste_done_handler (GtkTextBuffer *buffer,
                                  GdkClipboard  *clipboard,
                                  gpointer       data)
{
  GtkTextView *text_view = data;
  GtkTextViewPrivate *priv;

  priv = text_view->priv;

  if (priv->scroll_after_paste)
    {
      DV(g_print (G_STRLOC": scrolling onscreen\n"));
      gtk_text_view_scroll_mark_onscreen (text_view, gtk_text_buffer_get_insert (buffer));
    }

  priv->scroll_after_paste = FALSE;
}

static void
gtk_text_view_buffer_changed_handler (GtkTextBuffer *buffer,
                                      gpointer       data)
{
  GtkTextView *text_view = data;

  gtk_text_view_update_handles (text_view);
}

static void
gtk_text_view_insert_text_handler (GtkTextBuffer *buffer,
                                   GtkTextIter   *iter,
                                   char          *text,
                                   int            len,
                                   gpointer       data)
{
  GtkTextView *text_view = data;
  int position;
  int length;

  position = gtk_text_iter_get_offset (iter);
  length = g_utf8_strlen (text, len);

  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (text_view),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                       position - length,
                                       position);
}

static void
gtk_text_view_delete_range_handler (GtkTextBuffer *buffer,
                                    GtkTextIter   *start,
                                    GtkTextIter   *end,
                                    gpointer       data)
{
  GtkTextView *text_view = data;

  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (text_view),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE,
                                       gtk_text_iter_get_offset (start),
                                       gtk_text_iter_get_offset (end));
}

static void
gtk_text_view_toggle_overwrite (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  priv->overwrite_mode = !priv->overwrite_mode;

  if (priv->layout)
    gtk_text_layout_set_overwrite_mode (priv->layout,
					priv->overwrite_mode && priv->editable);

  gtk_widget_queue_draw (GTK_WIDGET (text_view));

  gtk_text_view_pend_cursor_blink (text_view);

  g_object_notify (G_OBJECT (text_view), "overwrite");
}

/**
 * gtk_text_view_get_overwrite: (attributes org.gtk.Method.get_property=overwrite)
 * @text_view: a `GtkTextView`
 *
 * Returns whether the `GtkTextView` is in overwrite mode or not.
 *
 * Returns: whether @text_view is in overwrite mode or not.
 */
gboolean
gtk_text_view_get_overwrite (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  return text_view->priv->overwrite_mode;
}

/**
 * gtk_text_view_set_overwrite: (attributes org.gtk.Method.set_property=overwrite)
 * @text_view: a `GtkTextView`
 * @overwrite: %TRUE to turn on overwrite mode, %FALSE to turn it off
 *
 * Changes the `GtkTextView` overwrite mode.
 */
void
gtk_text_view_set_overwrite (GtkTextView *text_view,
			     gboolean     overwrite)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  overwrite = overwrite != FALSE;

  if (text_view->priv->overwrite_mode != overwrite)
    gtk_text_view_toggle_overwrite (text_view);
}

/**
 * gtk_text_view_set_accepts_tab: (attributes org.gtk.Method.set_property=accepts-tab)
 * @text_view: A `GtkTextView`
 * @accepts_tab: %TRUE if pressing the Tab key should insert a tab
 *    character, %FALSE, if pressing the Tab key should move the
 *    keyboard focus.
 *
 * Sets the behavior of the text widget when the <kbd>Tab</kbd> key is pressed.
 *
 * If @accepts_tab is %TRUE, a tab character is inserted. If @accepts_tab
 * is %FALSE the keyboard focus is moved to the next widget in the focus
 * chain.
 *
 * Focus can always be moved using <kbd>Ctrl</kbd>+<kbd>Tab</kbd>.
 */
void
gtk_text_view_set_accepts_tab (GtkTextView *text_view,
			       gboolean     accepts_tab)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  accepts_tab = accepts_tab != FALSE;

  if (text_view->priv->accepts_tab != accepts_tab)
    {
      text_view->priv->accepts_tab = accepts_tab;

      g_object_notify (G_OBJECT (text_view), "accepts-tab");
    }
}

/**
 * gtk_text_view_get_accepts_tab: (attributes org.gtk.Method.get_property=accepts-tab)
 * @text_view: A `GtkTextView`
 *
 * Returns whether pressing the <kbd>Tab</kbd> key inserts a tab characters.
 *
 * See [method@Gtk.TextView.set_accepts_tab].
 *
 * Returns: %TRUE if pressing the Tab key inserts a tab character,
 *   %FALSE if pressing the Tab key moves the keyboard focus.
 */
gboolean
gtk_text_view_get_accepts_tab (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  return text_view->priv->accepts_tab;
}

/*
 * Selections
 */

static void
gtk_text_view_unselect (GtkTextView *text_view)
{
  GtkTextIter insert;

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                    gtk_text_buffer_get_insert (get_buffer (text_view)));

  gtk_text_buffer_move_mark (get_buffer (text_view),
                             gtk_text_buffer_get_selection_bound (get_buffer (text_view)),
                             &insert);
}

static void
move_mark_to_pointer_and_scroll (GtkTextView    *text_view,
                                 const char     *mark_name)
{
  GtkTextIter newplace;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  buffer = get_buffer (text_view);
  get_iter_from_gesture (text_view, text_view->priv->drag_gesture,
                         &newplace, NULL, NULL);

  mark = gtk_text_buffer_get_mark (buffer, mark_name);

  /* This may invalidate the layout */
  DV(g_print (G_STRLOC": move mark\n"));

  gtk_text_buffer_move_mark (buffer, mark, &newplace);

  DV(g_print (G_STRLOC": scrolling onscreen\n"));
  gtk_text_view_scroll_mark_onscreen (text_view, mark);

  DV (g_print ("first validate idle leaving %s is %d\n",
               G_STRLOC, text_view->priv->first_validate_idle));
}

static gboolean
selection_scan_timeout (gpointer data)
{
  GtkTextView *text_view;

  text_view = GTK_TEXT_VIEW (data);

  gtk_text_view_scroll_mark_onscreen (text_view,
				      gtk_text_buffer_get_insert (get_buffer (text_view)));

  return TRUE; /* remain installed. */
}

static void
extend_selection (GtkTextView          *text_view,
                  SelectionGranularity  granularity,
                  const GtkTextIter    *location,
                  GtkTextIter          *start,
                  GtkTextIter          *end)
{
  GtkTextExtendSelection extend_selection_granularity;
  gboolean handled = FALSE;

  switch (granularity)
    {
    case SELECT_CHARACTERS:
      *start = *location;
      *end = *location;
      return;

    case SELECT_WORDS:
      extend_selection_granularity = GTK_TEXT_EXTEND_SELECTION_WORD;
      break;

    case SELECT_LINES:
      extend_selection_granularity = GTK_TEXT_EXTEND_SELECTION_LINE;
      break;

    default:
      g_assert_not_reached ();
    }

  g_signal_emit (text_view,
                 signals[EXTEND_SELECTION], 0,
                 extend_selection_granularity,
                 location,
                 start,
                 end,
                 &handled);

  if (!handled)
    {
      *start = *location;
      *end = *location;
    }
}

static gboolean
gtk_text_view_extend_selection (GtkTextView            *text_view,
                                GtkTextExtendSelection  granularity,
                                const GtkTextIter      *location,
                                GtkTextIter            *start,
                                GtkTextIter            *end)
{
  *start = *location;
  *end = *location;

  switch (granularity)
    {
    case GTK_TEXT_EXTEND_SELECTION_WORD:
      if (gtk_text_iter_inside_word (start))
	{
	  if (!gtk_text_iter_starts_word (start))
	    gtk_text_iter_backward_visible_word_start (start);

	  if (!gtk_text_iter_ends_word (end))
	    {
	      if (!gtk_text_iter_forward_visible_word_end (end))
		gtk_text_iter_forward_to_end (end);
	    }
	}
      else
	{
	  GtkTextIter tmp;

          /* @start is not contained in a word: the selection is extended to all
           * the white spaces between the end of the word preceding @start and
           * the start of the one following.
           */

	  tmp = *start;
	  if (gtk_text_iter_backward_visible_word_start (&tmp))
	    gtk_text_iter_forward_visible_word_end (&tmp);

	  if (gtk_text_iter_get_line (&tmp) == gtk_text_iter_get_line (start))
	    *start = tmp;
	  else
	    gtk_text_iter_set_line_offset (start, 0);

	  tmp = *end;
	  if (!gtk_text_iter_forward_visible_word_end (&tmp))
	    gtk_text_iter_forward_to_end (&tmp);

	  if (gtk_text_iter_ends_word (&tmp))
	    gtk_text_iter_backward_visible_word_start (&tmp);

	  if (gtk_text_iter_get_line (&tmp) == gtk_text_iter_get_line (end))
	    *end = tmp;
	}
      break;

    case GTK_TEXT_EXTEND_SELECTION_LINE:
      if (gtk_text_view_starts_display_line (text_view, start))
	{
	  /* If on a display line boundary, we assume the user
	   * clicked off the end of a line and we therefore select
	   * the line before the boundary.
	   */
	  gtk_text_view_backward_display_line_start (text_view, start);
	}
      else
	{
	  /* start isn't on the start of a line, so we move it to the
	   * start, and move end to the end unless it's already there.
	   */
	  gtk_text_view_backward_display_line_start (text_view, start);

	  if (!gtk_text_view_starts_display_line (text_view, end))
	    gtk_text_view_forward_display_line_end (text_view, end);
	}
      break;

    default:
      g_return_val_if_reached (GDK_EVENT_STOP);
    }

  return GDK_EVENT_STOP;
}

typedef struct
{
  SelectionGranularity granularity;
  GtkTextMark *orig_start;
  GtkTextMark *orig_end;
  GtkTextBuffer *buffer;
} SelectionData;

static void
selection_data_free (SelectionData *data)
{
  if (data->orig_start != NULL)
    gtk_text_buffer_delete_mark (data->buffer, data->orig_start);

  if (data->orig_end != NULL)
    gtk_text_buffer_delete_mark (data->buffer, data->orig_end);

  g_object_unref (data->buffer);

  g_free (data);
}

static gboolean
drag_gesture_get_text_surface_coords (GtkGestureDrag *gesture,
                                      GtkTextView    *text_view,
                                      int            *start_x,
                                      int            *start_y,
                                      int            *x,
                                      int            *y)
{
  double sx, sy, ox, oy;

  if (!gtk_gesture_drag_get_start_point (gesture, &sx, &sy) ||
      !gtk_gesture_drag_get_offset (gesture, &ox, &oy))
    return FALSE;

  *start_x = sx;
  *start_y = sy;
  _widget_to_text_surface_coords (text_view, start_x, start_y);

  *x = sx + ox;
  *y = sy + oy;
  _widget_to_text_surface_coords (text_view, x, y);

  return TRUE;
}

static void
gtk_text_view_drag_gesture_update (GtkGestureDrag *gesture,
                                   double          offset_x,
                                   double          offset_y,
                                   GtkTextView    *text_view)
{
  int start_x, start_y, x, y;
  GdkEventSequence *sequence;
  gboolean is_touchscreen;
  GdkEvent *event;
  SelectionData *data;
  GdkDevice *device;
  GtkTextIter cursor;
  GtkTextIter orig_start, orig_end;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;

  data = g_object_get_qdata (G_OBJECT (gesture), quark_text_selection_data);
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (!drag_gesture_get_text_surface_coords (gesture, text_view,
                                             &start_x, &start_y, &x, &y))
    return;

  device = gdk_event_get_device (event);

  is_touchscreen = gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN;

  get_iter_from_gesture (text_view, text_view->priv->drag_gesture,
                         &cursor, NULL, NULL);

  if (!data)
    {
      /* If no data is attached, the initial press happened within the current
       * text selection, check for drag and drop to be initiated.
       */
      if (gtk_drag_check_threshold_double (GTK_WIDGET (text_view), 0, 0, offset_x, offset_y))
        {
          if (!is_touchscreen)
            {
              GtkTextIter iter;
              int buffer_x, buffer_y;

              gtk_text_view_window_to_buffer_coords (text_view,
                                                     GTK_TEXT_WINDOW_TEXT,
                                                     start_x, start_y,
                                                     &buffer_x,
                                                     &buffer_y);

              gtk_text_layout_get_iter_at_pixel (text_view->priv->layout,
                                                 &iter, buffer_x, buffer_y);

              gtk_text_view_start_selection_dnd (text_view, &iter, event,
                                                 start_x, start_y);

              /* Deny the gesture so we don't get further updates */
              gtk_gesture_set_state (text_view->priv->drag_gesture,
                                     GTK_EVENT_SEQUENCE_DENIED);
              return;
            }
          else
            {
              gtk_text_view_start_selection_drag (text_view, &cursor,
                                                  SELECT_WORDS, TRUE);
              data = g_object_get_qdata (G_OBJECT (gesture), quark_text_selection_data);
            }
        }
      else
        return;
    }

  g_assert (data != NULL);

  buffer = get_buffer (text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &orig_start, data->orig_start);
  gtk_text_buffer_get_iter_at_mark (buffer, &orig_end, data->orig_end);

  /* Text selection */
  if (data->granularity == SELECT_CHARACTERS)
    {
      move_mark_to_pointer_and_scroll (text_view, "insert");
      gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
    }
  else
    {
      get_iter_from_gesture (text_view, text_view->priv->drag_gesture,
                             &cursor, NULL, NULL);

      extend_selection (text_view, data->granularity, &cursor, &start, &end);

      /* either the selection extends to the front, or end (or not) */
      if (gtk_text_iter_compare (&orig_start, &start) < 0)
        start = orig_start;
      if (gtk_text_iter_compare (&orig_end, &end) > 0)
        end = orig_end;
      gtk_text_buffer_select_range (buffer, &start, &end);

      gtk_text_view_scroll_mark_onscreen (text_view,
					  gtk_text_buffer_get_insert (buffer));
    }

  if (gtk_text_iter_compare (&orig_start, &start) != 0 ||
      gtk_text_iter_compare (&orig_end, &end) != 0)
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  /* If we had to scroll offscreen, insert a timeout to do so
   * again. Note that in the timeout, even if the mouse doesn't
   * move, due to this scroll xoffset/yoffset will have changed
   * and we'll need to scroll again.
   */
  if (text_view->priv->scroll_timeout != 0) /* reset on every motion event */
    g_source_remove (text_view->priv->scroll_timeout);

  text_view->priv->scroll_timeout = g_timeout_add (50, selection_scan_timeout, text_view);
  gdk_source_set_static_name_by_id (text_view->priv->scroll_timeout, "[gtk] selection_scan_timeout");

  gtk_text_view_selection_bubble_popup_unset (text_view);

  if (is_touchscreen)
    {
      text_view->priv->text_handles_enabled = TRUE;
      gtk_text_view_update_handles (text_view);
      gtk_text_view_show_magnifier (text_view, &cursor, x, y);
    }
}

static void
gtk_text_view_drag_gesture_end (GtkGestureDrag *gesture,
                                double          offset_x,
                                double          offset_y,
                                GtkTextView    *text_view)
{
  gboolean is_touchscreen, clicked_in_selection;
  int start_x, start_y, x, y;
  GdkEventSequence *sequence;
  GtkTextViewPrivate *priv;
  GdkEvent *event;
  GdkDevice *device;

  priv = text_view->priv;
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  clicked_in_selection =
    g_object_get_qdata (G_OBJECT (gesture), quark_text_selection_data) == NULL;
  g_object_set_qdata (G_OBJECT (gesture), quark_text_selection_data, NULL);
  gtk_text_view_unobscure_mouse_cursor (text_view);

  if (priv->scroll_timeout != 0)
    {
      g_source_remove (priv->scroll_timeout);
      priv->scroll_timeout = 0;
    }

  if (priv->magnifier_popover)
    gtk_widget_set_visible (priv->magnifier_popover, FALSE);

  if (!drag_gesture_get_text_surface_coords (gesture, text_view,
                                             &start_x, &start_y, &x, &y))
    return;

  /* Check whether the drag was cancelled rather than finished */
  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    return;

  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  device = gdk_event_get_device (event);
  is_touchscreen = gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN;

  if ((is_touchscreen || clicked_in_selection) &&
      !gtk_drag_check_threshold_double (GTK_WIDGET (text_view), 0, 0, offset_x, offset_y))
    {
      GtkTextIter iter;

      /* Unselect everything; we clicked inside selection, but
       * didn't move by the drag threshold, so just clear selection
       * and place cursor.
       */
      gtk_text_layout_get_iter_at_pixel (priv->layout, &iter,
                                         x + priv->xoffset, y + priv->yoffset);

      gtk_text_buffer_place_cursor (get_buffer (text_view), &iter);
      gtk_text_view_check_cursor_blink (text_view);

      gtk_text_view_update_handles (text_view);
    }
}

static void
gtk_text_view_start_selection_drag (GtkTextView          *text_view,
                                    const GtkTextIter    *iter,
                                    SelectionGranularity  granularity,
                                    gboolean              extend)
{
  GtkTextViewPrivate *priv;
  GtkTextIter cursor, ins, bound;
  GtkTextIter orig_start, orig_end;
  GtkTextBuffer *buffer;
  SelectionData *data;

  priv = text_view->priv;
  data = g_new0 (SelectionData, 1);
  data->granularity = granularity;

  buffer = get_buffer (text_view);

  cursor = *iter;
  extend_selection (text_view, data->granularity, &cursor, &ins, &bound);

  orig_start = ins;
  orig_end = bound;

  if (extend)
    {
      /* Extend selection */
      GtkTextIter old_ins, old_bound;
      GtkTextIter old_start, old_end;

      gtk_text_buffer_get_iter_at_mark (buffer, &old_ins, gtk_text_buffer_get_insert (buffer));
      gtk_text_buffer_get_iter_at_mark (buffer, &old_bound, gtk_text_buffer_get_selection_bound (buffer));
      old_start = old_ins;
      old_end = old_bound;
      gtk_text_iter_order (&old_start, &old_end);

      /* move the front cursor, if the mouse is in front of the selection. Should the
       * cursor however be inside the selection (this happens on triple click) then we
       * move the side which was last moved (current insert mark) */
      if (gtk_text_iter_compare (&cursor, &old_start) <= 0 ||
          (gtk_text_iter_compare (&cursor, &old_end) < 0 &&
           gtk_text_iter_compare (&old_ins, &old_bound) <= 0))
        {
          bound = old_end;
        }
      else
        {
          ins = bound;
          bound = old_start;
        }

      /* Store any previous selection */
      if (gtk_text_iter_compare (&old_start, &old_end) != 0)
        {
          orig_start = old_ins;
          orig_end = old_bound;
        }
    }

  gtk_text_buffer_select_range (buffer, &ins, &bound);

  gtk_text_iter_order (&orig_start, &orig_end);
  data->orig_start = gtk_text_buffer_create_mark (buffer, NULL,
                                                  &orig_start, TRUE);
  data->orig_end = gtk_text_buffer_create_mark (buffer, NULL,
                                                &orig_end, TRUE);
  data->buffer = g_object_ref (buffer);
  gtk_text_view_check_cursor_blink (text_view);

  g_object_set_qdata_full (G_OBJECT (priv->drag_gesture),
                           quark_text_selection_data,
                           data, (GDestroyNotify) selection_data_free);
//  gtk_gesture_set_state (priv->drag_gesture,
//                         GTK_EVENT_SEQUENCE_CLAIMED);
}

/* returns whether we were really dragging */
static gboolean
gtk_text_view_end_selection_drag (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;

  priv = text_view->priv;

  if (!gtk_gesture_is_active (priv->drag_gesture))
    return FALSE;

  if (priv->scroll_timeout != 0)
    {
      g_source_remove (priv->scroll_timeout);
      priv->scroll_timeout = 0;
    }

  if (priv->magnifier_popover)
    gtk_widget_set_visible (priv->magnifier_popover, FALSE);

  return TRUE;
}

/*
 * Layout utils
 */

static void
gtk_text_view_set_attributes_from_style (GtkTextView        *text_view,
                                         GtkTextAttributes  *values)
{
  GtkCssStyle *style;
  const GdkRGBA black = { 0, };
  const GdkRGBA *color;
  const GdkRGBA *decoration_color;
  GtkTextDecorationLine decoration_line;
  GtkTextDecorationStyle decoration_style;

  if (!values->appearance.bg_rgba)
    values->appearance.bg_rgba = gdk_rgba_copy (&black);
  if (!values->appearance.fg_rgba)
    values->appearance.fg_rgba = gdk_rgba_copy (&black);

  style = gtk_css_node_get_style (gtk_widget_get_css_node (GTK_WIDGET (text_view)));

  color = gtk_css_color_value_get_rgba (style->background->background_color);
  *values->appearance.bg_rgba = *color;
  color = gtk_css_color_value_get_rgba (style->core->color);
  *values->appearance.fg_rgba = *color;

  if (values->font)
    pango_font_description_free (values->font);

  values->font = gtk_css_style_get_pango_font (style);

  /* text-decoration */

  decoration_line = _gtk_css_text_decoration_line_value_get (style->font_variant->text_decoration_line);
  decoration_style = _gtk_css_text_decoration_style_value_get (style->font_variant->text_decoration_style);
  decoration_color = gtk_css_color_value_get_rgba (style->font_variant->text_decoration_color
                                                   ? style->font_variant->text_decoration_color
                                                   : style->core->color);

  if (decoration_line & GTK_CSS_TEXT_DECORATION_LINE_UNDERLINE)
    {
      switch (decoration_style)
        {
        case GTK_CSS_TEXT_DECORATION_STYLE_DOUBLE:
          values->appearance.underline = PANGO_UNDERLINE_DOUBLE;
          break;
        case GTK_CSS_TEXT_DECORATION_STYLE_WAVY:
          values->appearance.underline = PANGO_UNDERLINE_ERROR;
          break;
        case GTK_CSS_TEXT_DECORATION_STYLE_SOLID:
        default:
          values->appearance.underline = PANGO_UNDERLINE_SINGLE;
          break;
        }

      if (values->appearance.underline_rgba)
        *values->appearance.underline_rgba = *decoration_color;
      else
        values->appearance.underline_rgba = gdk_rgba_copy (decoration_color);
    }
  else
    {
      values->appearance.underline = PANGO_UNDERLINE_NONE;
      g_clear_pointer (&values->appearance.underline_rgba, gdk_rgba_free);
    }

  if (decoration_line & GTK_CSS_TEXT_DECORATION_LINE_OVERLINE)
    {
      values->appearance.overline = PANGO_OVERLINE_SINGLE;
      if (values->appearance.overline_rgba)
        *values->appearance.overline_rgba = *decoration_color;
      else
        values->appearance.overline_rgba = gdk_rgba_copy (decoration_color);
    }
  else
    {
      values->appearance.overline = PANGO_OVERLINE_NONE;
      g_clear_pointer (&values->appearance.overline_rgba, gdk_rgba_free);
    }

  if (decoration_line & GTK_CSS_TEXT_DECORATION_LINE_LINE_THROUGH)
    {
      values->appearance.strikethrough = TRUE;
      if (values->appearance.strikethrough_rgba)
        *values->appearance.strikethrough_rgba = *decoration_color;
      else
        values->appearance.strikethrough_rgba = gdk_rgba_copy (decoration_color);
    }
  else
    {
      values->appearance.strikethrough = FALSE;
      g_clear_pointer (&values->appearance.strikethrough_rgba, gdk_rgba_free);
    }

  /* letter-spacing */
  values->letter_spacing = _gtk_css_number_value_get (style->font->letter_spacing, 100) * PANGO_SCALE;

  /* line-height */

  values->line_height = gtk_css_line_height_value_get (style->font->line_height);
  values->line_height_is_absolute = FALSE;
  if (values->line_height != 0.0)
    {
      if (gtk_css_number_value_get_dimension (style->font->line_height) == GTK_CSS_DIMENSION_LENGTH)
        values->line_height_is_absolute = TRUE;
    }

  /* OpenType features */
  g_free (values->font_features);
  values->font_features = gtk_css_style_compute_font_features (style);

  /* text-transform */
  values->text_transform = gtk_css_style_get_pango_text_transform (style);
}

static void
gtk_text_view_check_keymap_direction (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (text_view));
  GdkSeat *seat;
  GdkDevice *keyboard;
  PangoDirection direction;
  GtkTextDirection new_cursor_dir;
  GtkTextDirection new_keyboard_dir;
  gboolean split_cursor;

  if (!priv->layout)
    return;

  seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (text_view)));
  if (seat)
    keyboard = gdk_seat_get_keyboard (seat);
  else
    keyboard = NULL;

  if (keyboard)
    direction = gdk_device_get_direction (keyboard);
  else
    direction = PANGO_DIRECTION_LTR;

  g_object_get (settings,
                "gtk-split-cursor", &split_cursor,
                NULL);

  if (direction == PANGO_DIRECTION_RTL)
    new_keyboard_dir = GTK_TEXT_DIR_RTL;
  else
    new_keyboard_dir  = GTK_TEXT_DIR_LTR;

  if (split_cursor)
    new_cursor_dir = GTK_TEXT_DIR_NONE;
  else
    new_cursor_dir = new_keyboard_dir;

  gtk_text_layout_set_cursor_direction (priv->layout, new_cursor_dir);
  gtk_text_layout_set_keyboard_direction (priv->layout, new_keyboard_dir);
}

static void
gtk_text_view_ensure_layout (GtkTextView *text_view)
{
  GtkWidget *widget;
  GtkTextViewPrivate *priv;

  widget = GTK_WIDGET (text_view);
  priv = text_view->priv;

  if (priv->layout == NULL)
    {
      GtkTextAttributes *style;
      const GList *iter;
      PangoContext *ltr_context, *rtl_context;

      DV(g_print(G_STRLOC"\n"));

      priv->layout = gtk_text_layout_new ();

      g_signal_connect (priv->layout,
			"invalidated",
			G_CALLBACK (invalidated_handler),
			text_view);

      g_signal_connect (priv->layout,
			"changed",
			G_CALLBACK (changed_handler),
			text_view);

      g_signal_connect (priv->layout,
			"allocate-child",
			G_CALLBACK (gtk_anchored_child_allocated),
			text_view);

      if (get_buffer (text_view))
        gtk_text_layout_set_buffer (priv->layout, get_buffer (text_view));

      if ((gtk_widget_has_focus (widget) && cursor_visible (text_view)))
        gtk_text_view_pend_cursor_blink (text_view);
      else
        gtk_text_layout_set_cursor_visible (priv->layout, FALSE);

      gtk_text_layout_set_overwrite_mode (priv->layout,
					  priv->overwrite_mode && priv->editable);

      ltr_context = gtk_widget_create_pango_context (GTK_WIDGET (text_view));
      rtl_context = gtk_widget_create_pango_context (GTK_WIDGET (text_view));
      pango_context_set_base_dir (ltr_context, PANGO_DIRECTION_LTR);
      pango_context_set_base_dir (rtl_context, PANGO_DIRECTION_RTL);
      gtk_text_layout_set_contexts (priv->layout, ltr_context, rtl_context);
      g_object_unref (ltr_context);
      g_object_unref (rtl_context);

      gtk_text_view_update_pango_contexts (text_view);

      gtk_text_view_check_keymap_direction (text_view);

      style = gtk_text_attributes_new ();

      gtk_text_view_set_attributes_from_style (text_view, style);

      style->pixels_above_lines = priv->pixels_above_lines;
      style->pixels_below_lines = priv->pixels_below_lines;
      style->pixels_inside_wrap = priv->pixels_inside_wrap;

      style->left_margin = priv->left_margin;
      style->right_margin = priv->right_margin;
      priv->layout->right_padding = priv->right_padding;
      priv->layout->left_padding = priv->left_padding;

      style->indent = priv->indent;
      style->tabs = priv->tabs ? pango_tab_array_copy (priv->tabs) : NULL;

      style->wrap_mode = priv->wrap_mode;
      style->justification = priv->justify;
      style->direction = gtk_widget_get_direction (GTK_WIDGET (text_view));

      gtk_text_layout_set_default_style (priv->layout, style);

      gtk_text_attributes_unref (style);

      /* Set layout for all anchored children */

      iter = priv->anchored_children.head;
      while (iter != NULL)
        {
          const AnchoredChild *ac = iter->data;
          iter = iter->next;
          gtk_text_anchored_child_set_layout (ac->widget, priv->layout);
          /* ac may now be invalid! */
        }
    }
}

GtkTextAttributes*
gtk_text_view_get_default_attributes (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_attributes_copy (text_view->priv->layout->default_style);
}

static void
gtk_text_view_destroy_layout (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (priv->layout)
    {
      const GList *iter;

      gtk_text_view_remove_validate_idles (text_view);

      g_signal_handlers_disconnect_by_func (priv->layout,
					    invalidated_handler,
					    text_view);
      g_signal_handlers_disconnect_by_func (priv->layout,
					    changed_handler,
					    text_view);

      iter = priv->anchored_children.head;
      while (iter != NULL)
        {
          const AnchoredChild *ac = iter->data;
          iter = iter->next;
          gtk_text_anchored_child_set_layout (ac->widget, NULL);
          /* vc may now be invalid! */
        }

      gtk_text_view_stop_cursor_blink (text_view);
      gtk_text_view_end_selection_drag (text_view);

      g_object_unref (priv->layout);
      priv->layout = NULL;
    }
}

/**
 * gtk_text_view_reset_im_context:
 * @text_view: a `GtkTextView`
 *
 * Reset the input method context of the text view if needed.
 *
 * This can be necessary in the case where modifying the buffer
 * would confuse on-going input method behavior.
 */
void
gtk_text_view_reset_im_context (GtkTextView *text_view)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (text_view->priv->need_im_reset)
    {
      text_view->priv->need_im_reset = FALSE;
      gtk_im_context_reset (text_view->priv->im_context);
    }
}

/**
 * gtk_text_view_im_context_filter_keypress:
 * @text_view: a `GtkTextView`
 * @event: the key event
 *
 * Allow the `GtkTextView` input method to internally handle key press
 * and release events.
 *
 * If this function returns %TRUE, then no further processing should be
 * done for this key event. See [method@Gtk.IMContext.filter_keypress].
 *
 * Note that you are expected to call this function from your handler
 * when overriding key event handling. This is needed in the case when
 * you need to insert your own key handling between the input method
 * and the default key event handling of the `GtkTextView`.
 *
 * ```c
 * static gboolean
 * gtk_foo_bar_key_press_event (GtkWidget *widget,
 *                              GdkEvent  *event)
 * {
 *   guint keyval;
 *
 *   gdk_event_get_keyval ((GdkEvent*)event, &keyval);
 *
 *   if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
 *     {
 *       if (gtk_text_view_im_context_filter_keypress (GTK_TEXT_VIEW (widget), event))
 *         return TRUE;
 *     }
 *
 *   // Do some stuff
 *
 *   return GTK_WIDGET_CLASS (gtk_foo_bar_parent_class)->key_press_event (widget, event);
 * }
 * ```
 *
 * Returns: %TRUE if the input method handled the key event.
 */
gboolean
gtk_text_view_im_context_filter_keypress (GtkTextView *text_view,
                                          GdkEvent    *event)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  return gtk_im_context_filter_keypress (text_view->priv->im_context, event);
}

/*
 * DND feature
 */

static void
dnd_finished_cb (GdkDrag     *drag,
                 GtkTextView *self)
{
  GtkTextBuffer *buffer = self->priv->buffer;

  if (self->priv->dnd_drag_begin_mark)
    {
      if (gdk_drag_get_selected_action (drag) == GDK_ACTION_MOVE)
        {
            {
              GtkTextIter begin, end;

              gtk_text_buffer_get_iter_at_mark (buffer, &begin, self->priv->dnd_drag_begin_mark);
              gtk_text_buffer_get_iter_at_mark (buffer, &end, self->priv->dnd_drag_end_mark);
              gtk_text_buffer_delete (buffer, &begin, &end);
            }
        }

      gtk_text_buffer_delete_mark (buffer, self->priv->dnd_drag_begin_mark);
      gtk_text_buffer_delete_mark (buffer, self->priv->dnd_drag_end_mark);
      self->priv->dnd_drag_begin_mark = NULL;
      self->priv->dnd_drag_end_mark = NULL;
    }

  self->priv->drag = NULL;
}

static void
dnd_cancel_cb (GdkDrag *drag,
               GdkDragCancelReason reason,
               GtkTextView *self)
{
  GtkTextBuffer *buffer = self->priv->buffer;

  if (self->priv->dnd_drag_begin_mark)
    {
      gtk_text_buffer_delete_mark (buffer, self->priv->dnd_drag_begin_mark);
      gtk_text_buffer_delete_mark (buffer, self->priv->dnd_drag_end_mark);
      self->priv->dnd_drag_begin_mark = NULL;
      self->priv->dnd_drag_end_mark = NULL;
    }

  self->priv->drag = NULL;
}

static void
gtk_text_view_start_selection_dnd (GtkTextView       *text_view,
                                   const GtkTextIter *iter,
                                   GdkEvent          *event,
                                   int                x,
                                   int                y)
{
  GtkWidget *widget = GTK_WIDGET (text_view);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (text_view);
  GdkContentProvider *content;
  GtkTextIter start, end;
  GdkDragAction actions;
  GdkSurface *surface;
  GdkDevice *device;
  GdkDrag *drag;

  if (text_view->priv->editable)
    actions = GDK_ACTION_COPY | GDK_ACTION_MOVE;
  else
    actions = GDK_ACTION_COPY;

  content = gtk_text_buffer_get_selection_content (buffer);

  surface = gdk_event_get_surface (event);
  device = gdk_event_get_device (event);
  drag = gdk_drag_begin (surface, device, content, actions, x, y);

  g_object_unref (content);

  g_signal_connect (drag, "dnd-finished", G_CALLBACK (dnd_finished_cb), text_view);
  g_signal_connect (drag, "cancel", G_CALLBACK (dnd_cancel_cb), text_view);

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      GdkPaintable *paintable;
      paintable = gtk_text_util_create_rich_drag_icon (widget, buffer, &start, &end);
      gtk_drag_icon_set_from_paintable (drag, paintable, 0, 0);
      g_object_unref (paintable);

      text_view->priv->dnd_drag_begin_mark = gtk_text_buffer_create_mark (buffer, NULL, &start, TRUE);
      text_view->priv->dnd_drag_end_mark = gtk_text_buffer_create_mark (buffer, NULL, &end, TRUE);
    }

  text_view->priv->drag = drag;

  g_object_unref (drag);
}

static void
gtk_text_view_drag_leave (GtkDropTarget *dest,
                          GtkTextView   *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  gtk_text_mark_set_visible (priv->dnd_mark, FALSE);
}

static GdkDragAction
gtk_text_view_drag_motion (GtkDropTarget *dest,
                           double         x,
                           double         y,
                           GtkTextView   *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;
  GtkTextIter newplace;
  GtkTextIter start;
  GtkTextIter end;
  int bx, by;
  gboolean can_accept = FALSE;

  gtk_text_view_window_to_buffer_coords (text_view,
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y,
                                         &bx, &by);

  gtk_text_layout_get_iter_at_pixel (priv->layout,
                                     &newplace,
                                     bx, by);

  if (gtk_text_buffer_get_selection_bounds (get_buffer (text_view),
                                            &start, &end) &&
      gtk_text_iter_compare (&newplace, &start) >= 0 &&
      gtk_text_iter_compare (&newplace, &end) <= 0)
    {
      /* We're inside the selection. */
    }
  else
    {
      can_accept = gtk_text_iter_can_insert (&newplace, priv->editable);
    }

  if (can_accept)
    {
      gtk_text_mark_set_visible (priv->dnd_mark, cursor_visible (text_view));
      if (text_view->priv->drag)
        return GDK_ACTION_MOVE;
      else
        return GDK_ACTION_COPY;
    }
  else
    {
      gtk_text_mark_set_visible (priv->dnd_mark, FALSE);
      return 0;
    }
}

static gboolean
gtk_text_view_drag_drop (GtkDropTarget *dest,
                         const GValue  *value,
                         double         x,
                         double         y,
                         GtkTextView   *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;
  GtkTextBuffer *buffer;
  GtkTextIter drop_point;

  buffer = get_buffer (text_view);
  gtk_text_buffer_get_iter_at_mark (buffer, &drop_point, priv->dnd_mark);

  if (!gtk_text_iter_can_insert (&drop_point, priv->editable))
    return FALSE;

  gtk_text_buffer_begin_user_action (buffer);

  if (!gtk_text_buffer_insert_interactive (buffer,
                                           &drop_point, (char *) g_value_get_string (value), -1,
                                           text_view->priv->editable))
    gtk_widget_error_bell (GTK_WIDGET (text_view));

  gtk_text_buffer_get_iter_at_mark (buffer, &drop_point, priv->dnd_mark);
  gtk_text_buffer_place_cursor (buffer, &drop_point);

  gtk_text_buffer_end_user_action (buffer);

  return TRUE;
}

static void
gtk_text_view_set_hadjustment (GtkTextView   *text_view,
                               GtkAdjustment *adjustment)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (adjustment && priv->hadjustment == adjustment)
    return;

  if (priv->hadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->hadjustment,
                                            gtk_text_view_value_changed,
                                            text_view);
      g_object_unref (priv->hadjustment);
    }

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_text_view_value_changed), text_view);
  priv->hadjustment = g_object_ref_sink (adjustment);
  gtk_text_view_set_hadjustment_values (text_view);

  g_object_notify (G_OBJECT (text_view), "hadjustment");
}

static void
gtk_text_view_set_vadjustment (GtkTextView   *text_view,
                               GtkAdjustment *adjustment)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (adjustment && priv->vadjustment == adjustment)
    return;

  if (priv->vadjustment != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->vadjustment,
                                            gtk_text_view_value_changed,
                                            text_view);
      g_object_unref (priv->vadjustment);
    }

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_text_view_value_changed), text_view);
  priv->vadjustment = g_object_ref_sink (adjustment);
  gtk_text_view_set_vadjustment_values (text_view);

  g_object_notify (G_OBJECT (text_view), "vadjustment");
}

static void
gtk_text_view_set_hadjustment_values (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;
  int screen_width;
  double old_value;
  double new_value;
  double new_upper;

  priv = text_view->priv;

  screen_width = SCREEN_WIDTH (text_view);
  old_value = gtk_adjustment_get_value (priv->hadjustment);
  new_upper = MAX (screen_width, priv->width);

  g_object_set (priv->hadjustment,
                "lower", 0.0,
                "upper", new_upper,
                "page-size", (double)screen_width,
                "step-increment", screen_width * 0.1,
                "page-increment", screen_width * 0.9,
                NULL);

  new_value = CLAMP (old_value, 0, new_upper - screen_width);
  if (new_value != old_value)
    gtk_adjustment_set_value (priv->hadjustment, new_value);
}

static void
gtk_text_view_set_vadjustment_values (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;
  GtkTextIter first_para;
  int screen_height;
  int y;
  double old_value;
  double new_value;
  double new_upper;

  priv = text_view->priv;

  screen_height = SCREEN_HEIGHT (text_view);
  old_value = gtk_adjustment_get_value (priv->vadjustment);
  new_upper = MAX (screen_height, priv->height);

  g_object_set (priv->vadjustment,
                "lower", 0.0,
                "upper", new_upper,
                "page-size", (double)screen_height,
                "step-increment", screen_height * 0.1,
                "page-increment", screen_height * 0.9,
                NULL);

  /* Now adjust the value of the adjustment to keep the cursor at the
   * same place in the buffer */
  gtk_text_view_ensure_layout (text_view);
  gtk_text_view_get_first_para_iter (text_view, &first_para);
  gtk_text_layout_get_line_yrange (priv->layout, &first_para, &y, NULL);

  y += priv->first_para_pixels;

  new_value = CLAMP (y, 0, new_upper - screen_height);
  if (new_value != old_value)
    gtk_adjustment_set_value (priv->vadjustment, new_value);
}

static void
gtk_text_view_value_changed (GtkAdjustment *adjustment,
                             GtkTextView   *text_view)
{
  GtkTextViewPrivate *priv;
  GtkTextIter iter;
  int line_top;
  int dx = 0;
  int dy = 0;

  priv = text_view->priv;

  /* Note that we oddly call this function with adjustment == NULL
   * sometimes
   */

  priv->onscreen_validated = FALSE;

  DV(g_print(">Scroll offset changed %s/%g, onscreen_validated = FALSE ("G_STRLOC")\n",
             adjustment == priv->hadjustment ? "hadjustment" : adjustment == priv->vadjustment ? "vadjustment" : "none",
             adjustment ? gtk_adjustment_get_value (adjustment) : 0.0));

  if (adjustment == priv->hadjustment)
    {
      dx = priv->xoffset - (int)gtk_adjustment_get_value (adjustment);
      priv->xoffset = (int)gtk_adjustment_get_value (adjustment) - priv->left_padding;
    }
  else if (adjustment == priv->vadjustment)
    {
      dy = priv->yoffset - (int)gtk_adjustment_get_value (adjustment) + priv->top_margin ;
      priv->yoffset -= dy;

      if (priv->layout)
        {
          gtk_text_layout_get_line_at_y (priv->layout, &iter, gtk_adjustment_get_value (adjustment), &line_top);

          gtk_text_buffer_move_mark (get_buffer (text_view), priv->first_para_mark, &iter);

          priv->first_para_pixels = gtk_adjustment_get_value (adjustment) - line_top;
        }
    }

  if (dx != 0 || dy != 0)
    {
      if (gtk_widget_get_realized (GTK_WIDGET (text_view)))
        {
          if (priv->selection_bubble)
            gtk_widget_set_visible (priv->selection_bubble, FALSE);
        }
    }

  /* This could result in invalidation, which would install the
   * first_validate_idle, which would validate onscreen;
   * but we're going to go ahead and validate here, so
   * first_validate_idle shouldn't have anything to do.
   */
  gtk_text_view_update_layout_width (text_view);

  /* We also update the IM spot location here, since the IM context
   * might do something that leads to validation.
   */
  gtk_text_view_update_im_spot_location (text_view);

  /* note that validation of onscreen could invoke this function
   * recursively, by scrolling to maintain first_para, or in response
   * to updating the layout width, however there is no problem with
   * that, or shouldn't be.
   */
  gtk_text_view_validate_onscreen (text_view);

  /* If this got installed, get rid of it, it's just a waste of time. */
  if (priv->first_validate_idle != 0)
    {
      g_source_remove (priv->first_validate_idle);
      priv->first_validate_idle = 0;
    }

  /* Allow to extend selection with mouse scrollwheel. Bug 710612 */
  if (gtk_gesture_is_active (priv->drag_gesture))
    {
      GdkEvent *current_event;
      current_event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (priv->drag_gesture));
      if (current_event != NULL)
        {
          if (gdk_event_get_event_type (current_event) == GDK_SCROLL)
            move_mark_to_pointer_and_scroll (text_view, "insert");
        }
    }

  /* Finally we update the IM cursor location again, to ensure any
   * changes made by the validation are pushed through.
   */
  gtk_text_view_update_im_spot_location (text_view);

  gtk_text_view_update_handles (text_view);

  if (priv->anchored_children.length > 0)
    gtk_widget_queue_allocate (GTK_WIDGET (text_view));
  else
    gtk_widget_queue_draw (GTK_WIDGET (text_view));

  DV(g_print(">End scroll offset changed handler ("G_STRLOC")\n"));
}

static void
gtk_text_view_commit_handler (GtkIMContext  *context,
                              const char    *str,
                              GtkTextView   *text_view)
{
  gtk_text_view_commit_text (text_view, str);
  gtk_text_view_reset_blink_time (text_view);
  gtk_text_view_pend_cursor_blink (text_view);
}

static void
gtk_text_view_commit_text (GtkTextView   *text_view,
                           const char    *str)
{
  GtkTextViewPrivate *priv;
  gboolean had_selection;
  GtkTextIter begin, end;
  guint length;

  priv = text_view->priv;

  gtk_text_view_obscure_mouse_cursor (text_view);
  gtk_text_buffer_begin_user_action (get_buffer (text_view));

  had_selection = gtk_text_buffer_get_selection_bounds (get_buffer (text_view), &begin, &end);
  gtk_text_iter_order (&begin, &end);
  length = gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin);

  if (gtk_text_buffer_delete_selection (get_buffer (text_view), TRUE, priv->editable))
    {
      /* If something was deleted, create a second group for the insert. This
       * ensures that there are two undo operations. One for the deletion, and
       * one for the insertion of new text. However, if there is only a single
       * character overwritten, that isn't very useful, just keep the single
       * undo group.
       */
      if (length > 1)
        {
          gtk_text_buffer_end_user_action (get_buffer (text_view));
          gtk_text_buffer_begin_user_action (get_buffer (text_view));
        }
    }

  if (!strcmp (str, "\n"))
    {
      if (!gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view), "\n", 1,
                                                         priv->editable))
        {
          gtk_widget_error_bell (GTK_WIDGET (text_view));
        }
    }
  else
    {
      if (!had_selection && priv->overwrite_mode)
	{
	  GtkTextIter insert;

	  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view),
					    &insert,
					    gtk_text_buffer_get_insert (get_buffer (text_view)));
	  if (!gtk_text_iter_ends_line (&insert))
	    gtk_text_view_delete_from_cursor (text_view, GTK_DELETE_CHARS, 1);
	}

      if (!gtk_text_buffer_insert_interactive_at_cursor (get_buffer (text_view), str, -1,
                                                         priv->editable))
        {
          gtk_widget_error_bell (GTK_WIDGET (text_view));
        }
    }

  gtk_text_buffer_end_user_action (get_buffer (text_view));

  gtk_text_view_set_virtual_cursor_pos (text_view, -1, -1);
  DV(g_print (G_STRLOC": scrolling onscreen\n"));
  gtk_text_view_scroll_mark_onscreen (text_view,
                                      gtk_text_buffer_get_insert (get_buffer (text_view)));
}

static void
gtk_text_view_preedit_start_handler (GtkIMContext *context,
                                     GtkTextView  *self)
{
  gtk_text_buffer_delete_selection (self->priv->buffer, TRUE, self->priv->editable);
}

static void
gtk_text_view_preedit_changed_handler (GtkIMContext *context,
				       GtkTextView  *text_view)
{
  GtkTextViewPrivate *priv;
  char *str;
  PangoAttrList *attrs;
  int cursor_pos;
  GtkTextIter iter;

  priv = text_view->priv;

  gtk_text_view_obscure_mouse_cursor (text_view);
  gtk_text_buffer_get_iter_at_mark (priv->buffer, &iter,
				    gtk_text_buffer_get_insert (priv->buffer));

  /* Keypress events are passed to input method even if cursor position is
   * not editable; so beep here if it's multi-key input sequence, input
   * method will be reset in when the event is handled by GTK.
   */
  gtk_im_context_get_preedit_string (context, &str, &attrs, &cursor_pos);

  if (str && str[0] && !gtk_text_iter_can_insert (&iter, priv->editable))
    {
      gtk_widget_error_bell (GTK_WIDGET (text_view));
      goto out;
    }

  g_signal_emit (text_view, signals[PREEDIT_CHANGED], 0, str);

  if (priv->layout)
    gtk_text_layout_set_preedit_string (priv->layout, str, attrs, cursor_pos);
  if (gtk_widget_has_focus (GTK_WIDGET (text_view)))
    gtk_text_view_scroll_mark_onscreen (text_view,
					gtk_text_buffer_get_insert (get_buffer (text_view)));

out:
  pango_attr_list_unref (attrs);
  g_free (str);
}

static gboolean
gtk_text_view_retrieve_surrounding_handler (GtkIMContext  *context,
					    GtkTextView   *text_view)
{
  GtkTextIter start;
  GtkTextIter end;
  GtkTextIter start1;
  GtkTextIter end1;
  GtkTextIter start2;
  GtkTextIter end2;
  int cursor_pos;
  int anchor_pos;
  char *text;
  char *pre;
  char *sel;
  char *post;
  gboolean flip;

  gtk_text_buffer_get_iter_at_mark (text_view->priv->buffer, &start,
                                    gtk_text_buffer_get_selection_bound (text_view->priv->buffer));
  gtk_text_buffer_get_iter_at_mark (text_view->priv->buffer, &end,
                                    gtk_text_buffer_get_insert (text_view->priv->buffer));

  flip = gtk_text_iter_compare (&start, &end) > 0;

  gtk_text_iter_order (&start, &end);

  start1 = start;
  end1 = end;

  gtk_text_iter_set_line_offset (&start1, 0);
  gtk_text_iter_forward_to_line_end (&end1);

  start2 = start;
  gtk_text_iter_backward_word_starts (&start2, 3);
  if (gtk_text_iter_compare (&start2, &start1) < 0)
    start1 = start2;

  end2 = end;
  gtk_text_iter_forward_word_ends (&end2, 3);
  if (gtk_text_iter_compare (&end2, &end1) > 0)
    end1 = end2;

  pre = gtk_text_iter_get_slice (&start1, &start);
  sel = gtk_text_iter_get_slice (&start, &end);
  post = gtk_text_iter_get_slice (&end, &end1);

  if (flip)
    {
      cursor_pos = strlen (pre);
      anchor_pos = cursor_pos + strlen (sel);
    }
  else
    {
      anchor_pos = strlen (pre);
      cursor_pos = anchor_pos + strlen (sel);
    }

  text = g_strconcat (pre, sel, post, NULL);

  g_free (pre);
  g_free (sel);
  g_free (post);

  gtk_im_context_set_surrounding_with_selection (context, text, -1, cursor_pos, anchor_pos);

  g_free (text);

  return TRUE;
}

static gboolean
gtk_text_view_delete_surrounding_handler (GtkIMContext  *context,
					  int            offset,
					  int            n_chars,
					  GtkTextView   *text_view)
{
  GtkTextViewPrivate *priv;
  GtkTextIter start;
  GtkTextIter end;

  priv = text_view->priv;

  gtk_text_buffer_get_iter_at_mark (priv->buffer, &start,
				    gtk_text_buffer_get_insert (priv->buffer));
  end = start;

  gtk_text_iter_forward_chars (&start, offset);
  gtk_text_iter_forward_chars (&end, offset + n_chars);

  gtk_text_buffer_delete_interactive (priv->buffer, &start, &end,
                                      priv->editable);

  return TRUE;
}

static void
gtk_text_view_mark_set_handler (GtkTextBuffer     *buffer,
                                const GtkTextIter *location,
                                GtkTextMark       *mark,
                                gpointer           data)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (data);
  gboolean need_reset = FALSE;
  gboolean has_selection;

  if (mark == gtk_text_buffer_get_insert (buffer))
    {
      text_view->priv->virtual_cursor_x = -1;
      text_view->priv->virtual_cursor_y = -1;
      gtk_text_view_update_im_spot_location (text_view);
      gtk_accessible_text_update_caret_position (GTK_ACCESSIBLE_TEXT (text_view));
      need_reset = TRUE;
    }
  else if (mark == gtk_text_buffer_get_selection_bound (buffer))
    {
      gtk_accessible_text_update_selection_bound (GTK_ACCESSIBLE_TEXT (text_view));
      need_reset = TRUE;
    }

  if (need_reset)
    {
      gtk_text_view_reset_im_context (text_view);
      gtk_text_view_update_handles (text_view);

      has_selection = gtk_text_buffer_get_selection_bounds (get_buffer (text_view), NULL, NULL);
      gtk_css_node_set_visible (text_view->priv->selection_node, has_selection);
    }
}

static void
gtk_text_view_get_virtual_cursor_pos (GtkTextView *text_view,
                                      GtkTextIter *cursor,
                                      int         *x,
                                      int         *y)
{
  GtkTextViewPrivate *priv;
  GtkTextIter insert;
  GdkRectangle pos;

  priv = text_view->priv;

  if (cursor)
    insert = *cursor;
  else
    gtk_text_buffer_get_iter_at_mark (get_buffer (text_view), &insert,
                                      gtk_text_buffer_get_insert (get_buffer (text_view)));

  if ((x && priv->virtual_cursor_x == -1) ||
      (y && priv->virtual_cursor_y == -1))
    gtk_text_layout_get_cursor_locations (priv->layout, &insert, &pos, NULL);

  if (x)
    {
      if (priv->virtual_cursor_x != -1)
        *x = priv->virtual_cursor_x;
      else
        *x = pos.x;
    }

  if (y)
    {
      if (priv->virtual_cursor_y != -1)
        *y = priv->virtual_cursor_y;
      else
        *y = pos.y + pos.height / 2;
    }
}

static void
gtk_text_view_set_virtual_cursor_pos (GtkTextView *text_view,
                                      int          x,
                                      int          y)
{
  GdkRectangle pos;

  if (!text_view->priv->layout)
    return;

  if (x == -1 || y == -1)
    gtk_text_view_get_cursor_locations (text_view, NULL, &pos, NULL);

  text_view->priv->virtual_cursor_x = (x == -1) ? pos.x : x;
  text_view->priv->virtual_cursor_y = (y == -1) ? pos.y + pos.height / 2 : y;
}

static void
hide_selection_bubble (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (priv->selection_bubble && gtk_widget_get_visible (priv->selection_bubble))
    gtk_widget_set_visible (priv->selection_bubble, FALSE);
}

static void
gtk_text_view_select_all (GtkWidget *widget,
			  gboolean select)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);
  GtkTextBuffer *buffer;
  GtkTextIter start_iter, end_iter, insert;

  buffer = text_view->priv->buffer;
  if (select)
    {
      gtk_text_buffer_get_bounds (buffer, &start_iter, &end_iter);
      gtk_text_buffer_select_range (buffer, &start_iter, &end_iter);
    }
  else
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &insert,
					gtk_text_buffer_get_insert (buffer));
      gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &insert);
    }
}


static gboolean
range_contains_editable_text (const GtkTextIter *start,
                              const GtkTextIter *end,
                              gboolean default_editability)
{
  GtkTextIter iter = *start;

  while (gtk_text_iter_compare (&iter, end) < 0)
    {
      if (gtk_text_iter_editable (&iter, default_editability))
        return TRUE;

      gtk_text_iter_forward_to_tag_toggle (&iter, NULL);
    }

  return FALSE;
}

static void
gtk_text_view_activate_clipboard_cut (GtkWidget  *widget,
                                      const char *action_name,
                                      GVariant   *parameter)
{
  GtkTextView *self = GTK_TEXT_VIEW (widget);
  g_signal_emit_by_name (self, "cut-clipboard");
  hide_selection_bubble (self);
}

static void
gtk_text_view_activate_clipboard_copy (GtkWidget  *widget,
                                       const char *action_name,
                                       GVariant   *parameter)
{
  GtkTextView *self = GTK_TEXT_VIEW (widget);
  g_signal_emit_by_name (self, "copy-clipboard");
  hide_selection_bubble (self);
}

static void
gtk_text_view_activate_clipboard_paste (GtkWidget  *widget,
                                        const char *action_name,
                                        GVariant   *parameter)
{
  GtkTextView *self = GTK_TEXT_VIEW (widget);
  g_signal_emit_by_name (self, "paste-clipboard");
  hide_selection_bubble (self);
}

static void
gtk_text_view_activate_selection_select_all (GtkWidget  *widget,
                                             const char *action_name,
                                             GVariant   *parameter)
{
  gtk_text_view_select_all (widget, TRUE);
}

static void
gtk_text_view_activate_selection_delete (GtkWidget  *widget,
                                         const char *action_name,
                                         GVariant   *parameter)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  gtk_text_buffer_delete_selection (get_buffer (text_view), TRUE,
				    text_view->priv->editable);
}

static void
gtk_text_view_activate_misc_insert_emoji (GtkWidget  *widget,
                                          const char *action_name,
                                          GVariant   *parameter)
{
  gtk_text_view_insert_emoji (GTK_TEXT_VIEW (widget));
}

static void
gtk_text_view_update_clipboard_actions (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;
  GdkClipboard *clipboard;
  gboolean have_selection;
  gboolean can_paste, can_insert;
  GtkTextIter iter, sel_start, sel_end;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (text_view));
  can_paste = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), G_TYPE_STRING);

  have_selection = gtk_text_buffer_get_selection_bounds (get_buffer (text_view),
                                                         &sel_start, &sel_end);

  gtk_text_buffer_get_iter_at_mark (get_buffer (text_view),
                                    &iter,
                                    gtk_text_buffer_get_insert (get_buffer (text_view)));

  can_insert = gtk_text_iter_can_insert (&iter, priv->editable);

  gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "clipboard.cut",
                                 have_selection &&
                                 range_contains_editable_text (&sel_start, &sel_end, priv->editable));
  gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "clipboard.copy",
                                 have_selection);
  gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "clipboard.paste",
                                 can_insert && can_paste);
  gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "selection.delete",
                                 have_selection &&
                                 range_contains_editable_text (&sel_start, &sel_end, priv->editable));
  gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "selection.select-all",
                                 gtk_text_buffer_get_char_count (priv->buffer) > 0);
}

static void
gtk_text_view_update_emoji_action (GtkTextView *text_view)
{
  gtk_widget_action_set_enabled (GTK_WIDGET (text_view), "misc.insert-emoji",
                                 (gtk_text_view_get_input_hints (text_view) & GTK_INPUT_HINT_NO_EMOJI) == 0 &&
                                 text_view->priv->editable);
}

static GMenuModel *
gtk_text_view_get_menu_model (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;
  GtkJoinedMenu *joined;
  GMenu *menu, *section;
  GMenuItem *item;

  joined = gtk_joined_menu_new ();

  menu = g_menu_new ();

  section = g_menu_new ();
  item = g_menu_item_new (_("Cu_t"), "clipboard.cut");
  g_menu_item_set_attribute (item, "touch-icon", "s", "edit-cut-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);
  item = g_menu_item_new (_("_Copy"), "clipboard.copy");
  g_menu_item_set_attribute (item, "touch-icon", "s", "edit-copy-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);
  item = g_menu_item_new (_("_Paste"), "clipboard.paste");
  g_menu_item_set_attribute (item, "touch-icon", "s", "edit-paste-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);
  item = g_menu_item_new (_("_Delete"), "selection.delete");
  g_menu_item_set_attribute (item, "touch-icon", "s", "edit-delete-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();
  item = g_menu_item_new (_("_Undo"), "text.undo");
  g_menu_item_set_attribute (item, "touch-icon", "s", "edit-undo-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);
  item = g_menu_item_new (_("_Redo"), "text.redo");
  g_menu_item_set_attribute (item, "touch-icon", "s", "edit-redo-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  section = g_menu_new ();

  item = g_menu_item_new (_("Select _All"), "selection.select-all");
  g_menu_item_set_attribute (item, "touch-icon", "s", "edit-select-all-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);

  item = g_menu_item_new ( _("Insert _Emoji"), "misc.insert-emoji");
  g_menu_item_set_attribute (item, "hidden-when", "s", "action-disabled");
  g_menu_item_set_attribute (item, "touch-icon", "s", "face-smile-symbolic");
  g_menu_append_item (section, item);
  g_object_unref (item);
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  g_object_unref (section);

  gtk_joined_menu_append_menu (joined, G_MENU_MODEL (menu));
  g_object_unref (menu);

  if (priv->extra_menu)
    gtk_joined_menu_append_menu (joined, priv->extra_menu);

  return G_MENU_MODEL (joined);
}

static void
gtk_text_view_do_popup (GtkTextView *text_view,
                        GdkEvent    *trigger_event)
{
  GtkTextViewPrivate *priv = text_view->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (text_view)))
    return;

  gtk_text_view_update_clipboard_actions (text_view);

  if (!priv->popup_menu)
    {
      GMenuModel *model;

      model = gtk_text_view_get_menu_model (text_view);
      priv->popup_menu = gtk_popover_menu_new_from_model (model);
      gtk_css_node_insert_after (gtk_widget_get_css_node (GTK_WIDGET (text_view)),
                                 gtk_widget_get_css_node (priv->popup_menu),
                                 priv->text_window->css_node);
      gtk_widget_set_parent (priv->popup_menu, GTK_WIDGET (text_view));
      gtk_popover_set_position (GTK_POPOVER (priv->popup_menu), GTK_POS_BOTTOM);

      gtk_popover_set_has_arrow (GTK_POPOVER (priv->popup_menu), FALSE);
      gtk_widget_set_halign (priv->popup_menu, GTK_ALIGN_START);

      gtk_accessible_update_property (GTK_ACCESSIBLE (priv->popup_menu),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, _("Context menu"),
                                      -1);

      g_object_unref (model);
    }

  if (trigger_event && gdk_event_triggers_context_menu (trigger_event))
    {
      GdkDevice *device;
      GdkSeat *seat;
      GdkRectangle rect = { 0, 0, 1, 1 };

      device = gdk_event_get_device (trigger_event);
      seat = gdk_event_get_seat (trigger_event);

      if (device == gdk_seat_get_keyboard (seat))
        device = gdk_seat_get_pointer (seat);

      if (device)
        {
          GtkNative *native;
          GdkSurface *surface;
          double px, py;
          double nx, ny;
          graphene_point_t p;

          native = gtk_widget_get_native (GTK_WIDGET (text_view));
          surface = gtk_native_get_surface (native);
          gdk_surface_get_device_position (surface, device, &px, &py, NULL);
          gtk_native_get_surface_transform (native, &nx, &ny);

          if (!gtk_widget_compute_point (GTK_WIDGET (gtk_widget_get_native (GTK_WIDGET (text_view))),
                                         GTK_WIDGET (text_view),
                                         &GRAPHENE_POINT_INIT (px - nx, py - ny),
                                         &p))
            graphene_point_init (&p, px - nx, px - nx);
          rect.x = p.x;
          rect.y = p.y;
        }

      gtk_popover_set_pointing_to (GTK_POPOVER (priv->popup_menu), &rect);
    }
  else
    {
      GtkTextBuffer *buffer;
      GtkTextIter iter;
      GdkRectangle iter_location;
      GdkRectangle visible_rect;
      gboolean is_visible;

      buffer = get_buffer (text_view);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                        gtk_text_buffer_get_insert (buffer));
      gtk_text_view_get_iter_location (text_view, &iter, &iter_location);
      gtk_text_view_get_visible_rect (text_view, &visible_rect);

      is_visible = (iter_location.x + iter_location.width > visible_rect.x &&
                    iter_location.x < visible_rect.x + visible_rect.width &&
                    iter_location.y + iter_location.height > visible_rect.y &&
                    iter_location.y < visible_rect.y + visible_rect.height);

      if (is_visible)
        {
          gtk_text_view_buffer_to_window_coords (text_view,
                                                 GTK_TEXT_WINDOW_WIDGET,
                                                 iter_location.x,
                                                 iter_location.y,
                                                 &iter_location.x,
                                                 &iter_location.y);

          gtk_popover_set_pointing_to (GTK_POPOVER (priv->popup_menu), &iter_location);
        }
    }

  gtk_popover_popup (GTK_POPOVER (priv->popup_menu));
}

static void
gtk_text_view_popup_menu (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *parameters)
{
  gtk_text_view_do_popup (GTK_TEXT_VIEW (widget), NULL);
}

static void
gtk_text_view_get_selection_rect (GtkTextView           *text_view,
				  cairo_rectangle_int_t *rect)
{
  cairo_rectangle_int_t rect_cursor, rect_bound;
  GtkTextIter cursor, bound;
  GtkTextBuffer *buffer;
  int x1, y1, x2, y2;

  buffer = get_buffer (text_view);
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor,
                                    gtk_text_buffer_get_insert (buffer));
  gtk_text_buffer_get_iter_at_mark (buffer, &bound,
                                    gtk_text_buffer_get_selection_bound (buffer));

  gtk_text_view_get_cursor_locations (text_view, &cursor, &rect_cursor, NULL);
  gtk_text_view_get_cursor_locations (text_view, &bound, &rect_bound, NULL);

  x1 = MIN (rect_cursor.x, rect_bound.x);
  x2 = MAX (rect_cursor.x, rect_bound.x);
  y1 = MIN (rect_cursor.y, rect_bound.y);
  y2 = MAX (rect_cursor.y + rect_cursor.height, rect_bound.y + rect_bound.height);

  rect->x = x1;
  rect->y = y1;
  rect->width = x2 - x1;
  rect->height = y2 - y1;
}

static void
show_or_hide_handles (GtkWidget   *popover,
                      GParamSpec  *pspec,
                      GtkTextView *text_view)
{
  gboolean visible;

  visible = gtk_widget_get_visible (popover);
  text_view->priv->text_handles_enabled = !visible;
  gtk_text_view_update_handles (text_view);
}

static void
append_bubble_item (GtkTextView *text_view,
                    GtkWidget   *toolbar,
                    GMenuModel  *model,
                    int          index)
{
  GtkWidget *item, *image;
  GVariant *att;
  const char *icon_name;
  const char *action_name;
  GMenuModel *link;
  gboolean is_toggle_action = FALSE;
  GtkActionMuxer *muxer;
  gboolean enabled;
  const GVariantType *param_type;
  const GVariantType *state_type;

  link = g_menu_model_get_item_link (model, index, "section");
  if (link)
    {
      int i;
      for (i = 0; i < g_menu_model_get_n_items (link); i++)
        append_bubble_item (text_view, toolbar, link, i);
      g_object_unref (link);
      return;
    }

  att = g_menu_model_get_item_attribute_value (model, index, "touch-icon", G_VARIANT_TYPE_STRING);
  if (att == NULL)
    return;

  icon_name = g_variant_get_string (att, NULL);
  g_variant_unref (att);

  att = g_menu_model_get_item_attribute_value (model, index, "action", G_VARIANT_TYPE_STRING);
  if (att == NULL)
    return;
  action_name = g_variant_get_string (att, NULL);
  g_variant_unref (att);

  muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (text_view), FALSE);
  if (muxer)
    {
      if (!gtk_action_muxer_query_action (muxer, action_name, &enabled, &param_type, &state_type, NULL, NULL))
        return;

      if (!enabled)
        return;

      if (param_type == NULL &&
          state_type != NULL &&
          g_variant_type_equal (state_type, G_VARIANT_TYPE_BOOLEAN))
        is_toggle_action = TRUE;
    }

  if (is_toggle_action)
    item = gtk_toggle_button_new ();
  else
    item = gtk_button_new ();
  gtk_widget_set_focus_on_click (item, FALSE);
  image = gtk_image_new_from_icon_name (icon_name);
  gtk_button_set_child (GTK_BUTTON (item), image);
  gtk_widget_add_css_class (item, "image-button");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (item), action_name);

  gtk_box_append (GTK_BOX (toolbar), item);
}

static gboolean
gtk_text_view_selection_bubble_popup_show (gpointer user_data)
{
  GtkTextView *text_view = user_data;
  GtkTextViewPrivate *priv = text_view->priv;
  cairo_rectangle_int_t rect;
  GtkWidget *box;
  GtkWidget *toolbar;
  GMenuModel *model;
  int i;

  gtk_text_view_update_clipboard_actions (text_view);

  priv->selection_bubble_timeout_id = 0;

  g_clear_pointer (&priv->selection_bubble, gtk_widget_unparent);

  priv->selection_bubble = gtk_popover_new ();
  gtk_widget_set_parent (priv->selection_bubble, GTK_WIDGET (text_view));
  gtk_widget_add_css_class (priv->selection_bubble, "touch-selection");
  gtk_popover_set_position (GTK_POPOVER (priv->selection_bubble), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (priv->selection_bubble), FALSE);
  g_signal_connect (priv->selection_bubble, "notify::visible",
                    G_CALLBACK (show_or_hide_handles), text_view);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_margin_start (box, 10);
  gtk_widget_set_margin_end (box, 10);
  gtk_widget_set_margin_top (box, 10);
  gtk_widget_set_margin_bottom (box, 10);
  toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (toolbar, "linked");
  gtk_popover_set_child (GTK_POPOVER (priv->selection_bubble), box);
  gtk_box_append (GTK_BOX (box), toolbar);

  model = gtk_text_view_get_menu_model (text_view);

  for (i = 0; i < g_menu_model_get_n_items (model); i++)
    append_bubble_item (text_view, toolbar, model, i);

  g_object_unref (model);

  gtk_text_view_get_selection_rect (text_view, &rect);
  rect.x -= priv->xoffset;
  rect.y -= priv->yoffset;

  _text_window_to_widget_coords (text_view, &rect.x, &rect.y);

  rect.x -= 5;
  rect.y -= 5;
  rect.width += 10;
  rect.height += 10;

  gtk_popover_set_pointing_to (GTK_POPOVER (priv->selection_bubble), &rect);
  gtk_widget_set_visible (priv->selection_bubble, TRUE);

  return G_SOURCE_REMOVE;
}

static void
gtk_text_view_selection_bubble_popup_unset (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;

  priv = text_view->priv;

  if (priv->selection_bubble)
    gtk_widget_set_visible (priv->selection_bubble, FALSE);

  if (priv->selection_bubble_timeout_id)
    {
      g_source_remove (priv->selection_bubble_timeout_id);
      priv->selection_bubble_timeout_id = 0;
    }
}

static void
gtk_text_view_selection_bubble_popup_set (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv;

  priv = text_view->priv;

  if (priv->selection_bubble_timeout_id)
    g_source_remove (priv->selection_bubble_timeout_id);

  priv->selection_bubble_timeout_id = g_timeout_add (50, gtk_text_view_selection_bubble_popup_show, text_view);
  gdk_source_set_static_name_by_id (priv->selection_bubble_timeout_id, "[gtk] gtk_text_view_selection_bubble_popup_cb");
}

/* Child GdkSurfaces */

static void
node_style_changed_cb (GtkCssNode        *node,
                       GtkCssStyleChange *change,
                       GtkWidget         *widget)
{
  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_SIZE))
    gtk_widget_queue_resize (widget);
  else
    gtk_widget_queue_draw (widget);
}

static void
update_node_ordering (GtkWidget *widget)
{
  GtkTextViewPrivate *priv = GTK_TEXT_VIEW (widget)->priv;
  GtkCssNode *widget_node, *sibling, *child_node;

  if (priv->text_window == NULL)
    return;

  widget_node = gtk_widget_get_css_node (widget);
  sibling = priv->text_window->css_node;

  if (priv->left_child)
    {
      child_node = gtk_widget_get_css_node (GTK_WIDGET (priv->left_child));
      gtk_css_node_insert_before (widget_node, child_node, sibling);
      sibling = child_node;
    }

  if (priv->top_child)
    {
      child_node = gtk_widget_get_css_node (GTK_WIDGET (priv->top_child));
      gtk_css_node_insert_before (widget_node, child_node, sibling);
    }

  sibling = priv->text_window->css_node;

  if (priv->right_child)
    {
      child_node = gtk_widget_get_css_node (GTK_WIDGET (priv->right_child));
      gtk_css_node_insert_after (widget_node, child_node, sibling);
      sibling = child_node;
    }

  if (priv->bottom_child)
    {
      child_node = gtk_widget_get_css_node (GTK_WIDGET (priv->bottom_child));
      gtk_css_node_insert_after (widget_node, child_node, sibling);
    }
}

static GtkTextWindow*
text_window_new (GtkWidget *widget)
{
  GtkTextWindow *win;
  GtkCssNode *widget_node;

  win = g_new (GtkTextWindow, 1);

  win->type = GTK_TEXT_WINDOW_TEXT;
  win->widget = widget;
  win->allocation.width = 0;
  win->allocation.height = 0;
  win->allocation.x = 0;
  win->allocation.y = 0;

  widget_node = gtk_widget_get_css_node (widget);
  win->css_node = gtk_css_node_new ();
  gtk_css_node_set_parent (win->css_node, widget_node);
  gtk_css_node_set_state (win->css_node, gtk_css_node_get_state (widget_node));
  g_signal_connect_object (win->css_node, "style-changed", G_CALLBACK (node_style_changed_cb), widget, 0);
  gtk_css_node_set_name (win->css_node, g_quark_from_static_string ("text"));

  g_object_unref (win->css_node);

  return win;
}

static void
text_window_free (GtkTextWindow *win)
{
  gtk_css_node_set_parent (win->css_node, NULL);

  g_free (win);
}

static void
text_window_size_allocate (GtkTextWindow *win,
                           GdkRectangle  *rect)
{
  win->allocation = *rect;
}

static int
text_window_get_width (GtkTextWindow *win)
{
  return win->allocation.width;
}

static int
text_window_get_height (GtkTextWindow *win)
{
  return win->allocation.height;
}

/* Windows */

/**
 * gtk_text_view_buffer_to_window_coords:
 * @text_view: a `GtkTextView`
 * @win: a `GtkTextWindowType`
 * @buffer_x: buffer x coordinate
 * @buffer_y: buffer y coordinate
 * @window_x: (out) (optional): window x coordinate return location
 * @window_y: (out) (optional): window y coordinate return location
 *
 * Converts buffer coordinates to window coordinates.
 */
void
gtk_text_view_buffer_to_window_coords (GtkTextView      *text_view,
                                       GtkTextWindowType win,
                                       int               buffer_x,
                                       int               buffer_y,
                                       int              *window_x,
                                       int              *window_y)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  buffer_x -= priv->xoffset;
  buffer_y -= priv->yoffset;

  switch (win)
    {
    case GTK_TEXT_WINDOW_WIDGET:
      buffer_x += priv->border_window_size.left;
      buffer_y += priv->border_window_size.top;
      break;

    case GTK_TEXT_WINDOW_TEXT:
      break;

    case GTK_TEXT_WINDOW_LEFT:
      buffer_x += priv->border_window_size.left;
      break;

    case GTK_TEXT_WINDOW_RIGHT:
      buffer_x -= text_window_get_width (priv->text_window);
      break;

    case GTK_TEXT_WINDOW_TOP:
      buffer_y += priv->border_window_size.top;
      break;

    case GTK_TEXT_WINDOW_BOTTOM:
      buffer_y -= text_window_get_height (priv->text_window);
      break;

    default:
      g_warning ("%s: Unknown GtkTextWindowType", G_STRFUNC);
      break;
    }

  if (window_x)
    *window_x = buffer_x;
  if (window_y)
    *window_y = buffer_y;
}

/**
 * gtk_text_view_window_to_buffer_coords:
 * @text_view: a `GtkTextView`
 * @win: a `GtkTextWindowType`
 * @window_x: window x coordinate
 * @window_y: window y coordinate
 * @buffer_x: (out) (optional): buffer x coordinate return location
 * @buffer_y: (out) (optional): buffer y coordinate return location
 *
 * Converts coordinates on the window identified by @win to buffer
 * coordinates.
 */
void
gtk_text_view_window_to_buffer_coords (GtkTextView      *text_view,
                                       GtkTextWindowType win,
                                       int               window_x,
                                       int               window_y,
                                       int              *buffer_x,
                                       int              *buffer_y)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  switch (win)
    {
    case GTK_TEXT_WINDOW_WIDGET:
      window_x -= priv->border_window_size.left;
      window_y -= priv->border_window_size.top;
      break;

    case GTK_TEXT_WINDOW_TEXT:
      break;

    case GTK_TEXT_WINDOW_LEFT:
      window_x -= priv->border_window_size.left;
      break;

    case GTK_TEXT_WINDOW_RIGHT:
      window_x += text_window_get_width (priv->text_window);
      break;

    case GTK_TEXT_WINDOW_TOP:
      window_y -= priv->border_window_size.top;
      break;

    case GTK_TEXT_WINDOW_BOTTOM:
      window_y += text_window_get_height (priv->text_window);
      break;

    default:
      g_warning ("%s: Unknown GtkTextWindowType", G_STRFUNC);
      break;
    }

  if (buffer_x)
    *buffer_x = window_x + priv->xoffset;
  if (buffer_y)
    *buffer_y = window_y + priv->yoffset;
}

/*
 * Child widgets
 */

static AnchoredChild *
anchored_child_new (GtkWidget          *child,
                    GtkTextChildAnchor *anchor,
                    GtkTextLayout      *layout)
{
  AnchoredChild *vc;

  vc = g_new0 (AnchoredChild, 1);
  vc->link.data = vc;
  vc->widget = g_object_ref (child);
  vc->anchor = g_object_ref (anchor);
  vc->from_top_of_line = 0;
  vc->from_left_of_buffer = 0;

  g_object_set_qdata (G_OBJECT (child), quark_text_view_child, vc);

  gtk_text_child_anchor_register_child (anchor, child, layout);

  return vc;
}

static void
anchored_child_free (AnchoredChild *child)
{
  g_assert (child->link.prev == NULL);
  g_assert (child->link.next == NULL);

  g_object_set_qdata (G_OBJECT (child->widget), quark_text_view_child, NULL);

  gtk_text_child_anchor_unregister_child (child->anchor, child->widget);

  g_object_unref (child->anchor);
  g_object_unref (child->widget);

  g_free (child);
}

static void
add_child (GtkTextView   *text_view,
           AnchoredChild *vc)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_queue_push_head_link (&priv->anchored_children, &vc->link);
  gtk_css_node_set_parent (gtk_widget_get_css_node (vc->widget),
                           priv->text_window->css_node);
  gtk_widget_set_parent (vc->widget, GTK_WIDGET (text_view));
}

/**
 * gtk_text_view_add_child_at_anchor:
 * @text_view: a `GtkTextView`
 * @child: a `GtkWidget`
 * @anchor: a `GtkTextChildAnchor` in the `GtkTextBuffer` for @text_view
 *
 * Adds a child widget in the text buffer, at the given @anchor.
 */
void
gtk_text_view_add_child_at_anchor (GtkTextView          *text_view,
                                   GtkWidget            *child,
                                   GtkTextChildAnchor   *anchor)
{
  AnchoredChild *vc;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  gtk_text_view_ensure_layout (text_view);

  vc = anchored_child_new (child, anchor, text_view->priv->layout);

  add_child (text_view, vc);

  g_assert (vc->widget == child);
  g_assert (gtk_widget_get_parent (child) == GTK_WIDGET (text_view));
}

static void
ensure_child (GtkTextView        *text_view,
              GtkTextViewChild  **child,
              GtkTextWindowType   window_type)
{
  GtkCssNode *css_node;
  GtkWidget *new_child;

  if (*child != NULL)
    return;

  new_child = gtk_text_view_child_new (window_type);
  css_node = gtk_widget_get_css_node (new_child);
  gtk_css_node_set_parent (css_node,
                           gtk_widget_get_css_node (GTK_WIDGET (text_view)));
  *child = g_object_ref (GTK_TEXT_VIEW_CHILD (new_child));
  gtk_widget_set_parent (GTK_WIDGET (new_child), GTK_WIDGET (text_view));
}

/**
 * gtk_text_view_add_overlay:
 * @text_view: a `GtkTextView`
 * @child: a `GtkWidget`
 * @xpos: X position of child in window coordinates
 * @ypos: Y position of child in window coordinates
 *
 * Adds @child at a fixed coordinate in the `GtkTextView`'s text window.
 *
 * The @xpos and @ypos must be in buffer coordinates (see
 * [method@Gtk.TextView.get_iter_location] to convert to
 * buffer coordinates).
 *
 * @child will scroll with the text view.
 *
 * If instead you want a widget that will not move with the
 * `GtkTextView` contents see `GtkOverlay`.
 */
void
gtk_text_view_add_overlay (GtkTextView *text_view,
                           GtkWidget   *child,
                           int          xpos,
                           int          ypos)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  ensure_child (text_view,
                &text_view->priv->center_child,
                GTK_TEXT_WINDOW_TEXT);

  gtk_text_view_child_add_overlay (text_view->priv->center_child,
                                   child, xpos, ypos);
}

/**
 * gtk_text_view_move_overlay:
 * @text_view: a `GtkTextView`
 * @child: a widget already added with [method@Gtk.TextView.add_overlay]
 * @xpos: new X position in buffer coordinates
 * @ypos: new Y position in buffer coordinates
 *
 * Updates the position of a child.
 *
 * See [method@Gtk.TextView.add_overlay].
 */
void
gtk_text_view_move_overlay (GtkTextView *text_view,
                            GtkWidget   *child,
                            int          xpos,
                            int          ypos)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (text_view->priv->center_child != NULL);
  g_return_if_fail (gtk_widget_get_parent (child) == (GtkWidget *)text_view->priv->center_child);

  gtk_text_view_child_move_overlay (text_view->priv->center_child,
                                    child, xpos, ypos);
}


/* Iterator operations */

/**
 * gtk_text_view_forward_display_line:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 *
 * Moves the given @iter forward by one display (wrapped) line.
 *
 * A display line is different from a paragraph. Paragraphs are
 * separated by newlines or other paragraph separator characters.
 * Display lines are created by line-wrapping a paragraph. If
 * wrapping is turned off, display lines and paragraphs will be the
 * same. Display lines are divided differently for each view, since
 * they depend on the view’s width; paragraphs are the same in all
 * views, since they depend on the contents of the `GtkTextBuffer`.
 *
 * Returns: %TRUE if @iter was moved and is not on the end iterator
 */
gboolean
gtk_text_view_forward_display_line (GtkTextView *text_view,
                                    GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_to_next_line (text_view->priv->layout, iter);
}

/**
 * gtk_text_view_backward_display_line:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 *
 * Moves the given @iter backward by one display (wrapped) line.
 *
 * A display line is different from a paragraph. Paragraphs are
 * separated by newlines or other paragraph separator characters.
 * Display lines are created by line-wrapping a paragraph. If
 * wrapping is turned off, display lines and paragraphs will be the
 * same. Display lines are divided differently for each view, since
 * they depend on the view’s width; paragraphs are the same in all
 * views, since they depend on the contents of the `GtkTextBuffer`.
 *
 * Returns: %TRUE if @iter was moved and is not on the end iterator
 */
gboolean
gtk_text_view_backward_display_line (GtkTextView *text_view,
                                     GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_to_previous_line (text_view->priv->layout, iter);
}

/**
 * gtk_text_view_forward_display_line_end:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 *
 * Moves the given @iter forward to the next display line end.
 *
 * A display line is different from a paragraph. Paragraphs are
 * separated by newlines or other paragraph separator characters.
 * Display lines are created by line-wrapping a paragraph. If
 * wrapping is turned off, display lines and paragraphs will be the
 * same. Display lines are divided differently for each view, since
 * they depend on the view’s width; paragraphs are the same in all
 * views, since they depend on the contents of the `GtkTextBuffer`.
 *
 * Returns: %TRUE if @iter was moved and is not on the end iterator
 */
gboolean
gtk_text_view_forward_display_line_end (GtkTextView *text_view,
                                        GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_to_line_end (text_view->priv->layout, iter, 1);
}

/**
 * gtk_text_view_backward_display_line_start:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 *
 * Moves the given @iter backward to the next display line start.
 *
 * A display line is different from a paragraph. Paragraphs are
 * separated by newlines or other paragraph separator characters.
 * Display lines are created by line-wrapping a paragraph. If
 * wrapping is turned off, display lines and paragraphs will be the
 * same. Display lines are divided differently for each view, since
 * they depend on the view’s width; paragraphs are the same in all
 * views, since they depend on the contents of the `GtkTextBuffer`.
 *
 * Returns: %TRUE if @iter was moved and is not on the end iterator
 */
gboolean
gtk_text_view_backward_display_line_start (GtkTextView *text_view,
                                           GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_to_line_end (text_view->priv->layout, iter, -1);
}

/**
 * gtk_text_view_starts_display_line:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 *
 * Determines whether @iter is at the start of a display line.
 *
 * See [method@Gtk.TextView.forward_display_line] for an
 * explanation of display lines vs. paragraphs.
 *
 * Returns: %TRUE if @iter begins a wrapped line
 */
gboolean
gtk_text_view_starts_display_line (GtkTextView       *text_view,
                                   const GtkTextIter *iter)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_iter_starts_line (text_view->priv->layout, iter);
}

/**
 * gtk_text_view_move_visually:
 * @text_view: a `GtkTextView`
 * @iter: a `GtkTextIter`
 * @count: number of characters to move (negative moves left,
 *    positive moves right)
 *
 * Move the iterator a given number of characters visually, treating
 * it as the strong cursor position.
 *
 * If @count is positive, then the new strong cursor position will
 * be @count positions to the right of the old cursor position.
 * If @count is negative then the new strong cursor position will
 * be @count positions to the left of the old cursor position.
 *
 * In the presence of bi-directional text, the correspondence
 * between logical and visual order will depend on the direction
 * of the current run, and there may be jumps when the cursor
 * is moved off of the end of a run.
 *
 * Returns: %TRUE if @iter moved and is not on the end iterator
 */
gboolean
gtk_text_view_move_visually (GtkTextView *text_view,
                             GtkTextIter *iter,
                             int          count)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_text_view_ensure_layout (text_view);

  return gtk_text_layout_move_iter_visually (text_view->priv->layout, iter, count);
}

/**
 * gtk_text_view_set_input_purpose: (attributes org.gtk.Method.set_property=input-purpose)
 * @text_view: a `GtkTextView`
 * @purpose: the purpose
 *
 * Sets the `input-purpose` of the `GtkTextView`.
 *
 * The `input-purpose` can be used by on-screen keyboards
 * and other input methods to adjust their behaviour.
 */
void
gtk_text_view_set_input_purpose (GtkTextView     *text_view,
                                 GtkInputPurpose  purpose)

{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (gtk_text_view_get_input_purpose (text_view) != purpose)
    {
      g_object_set (G_OBJECT (text_view->priv->im_context),
                    "input-purpose", purpose,
                    NULL);

      g_object_notify (G_OBJECT (text_view), "input-purpose");
    }
}

/**
 * gtk_text_view_get_input_purpose: (attributes org.gtk.Method.get_property=input-purpose)
 * @text_view: a `GtkTextView`
 *
 * Gets the `input-purpose` of the `GtkTextView`.
 */
GtkInputPurpose
gtk_text_view_get_input_purpose (GtkTextView *text_view)
{
  GtkInputPurpose purpose;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), GTK_INPUT_PURPOSE_FREE_FORM);

  g_object_get (G_OBJECT (text_view->priv->im_context),
                "input-purpose", &purpose,
                NULL);

  return purpose;
}

/**
 * gtk_text_view_set_input_hints: (attributes org.gtk.Method.set_property=input-hints)
 * @text_view: a `GtkTextView`
 * @hints: the hints
 *
 * Sets the `input-hints` of the `GtkTextView`.
 *
 * The `input-hints` allow input methods to fine-tune
 * their behaviour.
 */
void
gtk_text_view_set_input_hints (GtkTextView   *text_view,
                               GtkInputHints  hints)

{
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (gtk_text_view_get_input_hints (text_view) != hints)
    {
      g_object_set (G_OBJECT (text_view->priv->im_context),
                    "input-hints", hints,
                    NULL);

      g_object_notify (G_OBJECT (text_view), "input-hints");
      gtk_text_view_update_emoji_action (text_view);
    }
}

/**
 * gtk_text_view_get_input_hints: (attributes org.gtk.Method.get_property=input-hints)
 * @text_view: a `GtkTextView`
 *
 * Gets the `input-hints` of the `GtkTextView`.
 */
GtkInputHints
gtk_text_view_get_input_hints (GtkTextView *text_view)
{
  GtkInputHints hints;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), GTK_INPUT_HINT_NONE);

  g_object_get (G_OBJECT (text_view->priv->im_context),
                "input-hints", &hints,
                NULL);

  return hints;
}

/**
 * gtk_text_view_set_monospace: (attributes org.gtk.Method.set_property=monospace)
 * @text_view: a `GtkTextView`
 * @monospace: %TRUE to request monospace styling
 *
 * Sets whether the `GtkTextView` should display text in
 * monospace styling.
 */
void
gtk_text_view_set_monospace (GtkTextView *text_view,
                             gboolean     monospace)
{
  gboolean has_monospace;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  has_monospace = gtk_text_view_get_monospace (text_view);

  if (has_monospace != monospace)
    {
      if (monospace)
        gtk_widget_add_css_class (GTK_WIDGET (text_view), "monospace");
      else
        gtk_widget_remove_css_class (GTK_WIDGET (text_view), "monospace");

      g_object_notify (G_OBJECT (text_view), "monospace");
    }
}

/**
 * gtk_text_view_get_monospace: (attributes org.gtk.Method.get_property=monospace)
 * @text_view: a `GtkTextView`
 *
 * Gets whether the `GtkTextView` uses monospace styling.
 *
 * Return: %TRUE if monospace fonts are desired
 */
gboolean
gtk_text_view_get_monospace (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);

  return gtk_widget_has_css_class (GTK_WIDGET (text_view), "monospace");
}

static void
emoji_picked (GtkEmojiChooser *chooser,
              const char      *text,
              GtkTextView     *self)
{
  GtkTextBuffer *buffer;

  buffer = get_buffer (self);

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete_selection (buffer, TRUE, TRUE);
  gtk_text_buffer_insert_at_cursor (buffer, text, -1);
  gtk_text_buffer_end_user_action (buffer);
}

static void
gtk_text_view_insert_emoji (GtkTextView *text_view)
{
  GtkWidget *chooser;
  GtkTextIter iter;
  GdkRectangle rect;
  GdkRectangle rect2;
  GtkTextBuffer *buffer;

  if (gtk_widget_get_ancestor (GTK_WIDGET (text_view), GTK_TYPE_EMOJI_CHOOSER) != NULL)
    return;

  chooser = GTK_WIDGET (g_object_get_data (G_OBJECT (text_view), "gtk-emoji-chooser"));
  if (!chooser)
    {
      chooser = gtk_emoji_chooser_new ();
      g_object_set_data (G_OBJECT (text_view), "gtk-emoji-chooser", chooser);

      gtk_widget_set_parent (chooser, GTK_WIDGET (text_view));
      g_signal_connect (chooser, "emoji-picked", G_CALLBACK (emoji_picked), text_view);
      g_signal_connect_swapped (chooser, "hide", G_CALLBACK (gtk_widget_grab_focus), text_view);
    }

  buffer = get_buffer (text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                    gtk_text_buffer_get_insert (buffer));

  gtk_text_view_get_iter_location (text_view, &iter, (GdkRectangle *) &rect);

  rect.width = MAX (rect.width, 1);
  rect.height = MAX (rect.height, 1);

  gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                         rect.x, rect.y, &rect.x, &rect.y);
  _text_window_to_widget_coords (text_view, &rect.x, &rect.y);
  gtk_text_view_get_visible_rect (text_view, &rect2);
  gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                         rect2.x, rect2.y, &rect2.x, &rect2.y);
  _text_window_to_widget_coords (text_view, &rect2.x, &rect2.y);

  if (!gdk_rectangle_intersect (&rect2, &rect, &rect))
    {
      rect.x = rect2.width / 2;
      rect.y = rect2.height / 2;
      rect.width = 0;
      rect.height = 0;
    }

  gtk_popover_set_pointing_to (GTK_POPOVER (chooser), &rect);

  gtk_popover_popup (GTK_POPOVER (chooser));
}

/**
 * gtk_text_view_set_extra_menu: (attributes org.gtk.Method.set_property=extra-menu)
 * @text_view: a `GtkTextView`
 * @model: (nullable): a `GMenuModel`
 *
 * Sets a menu model to add when constructing the context
 * menu for @text_view.
 *
 * You can pass %NULL to remove a previously set extra menu.
 */
void
gtk_text_view_set_extra_menu (GtkTextView *text_view,
                              GMenuModel  *model)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  if (g_set_object (&priv->extra_menu, model))
    {
      g_clear_pointer (&priv->popup_menu, gtk_widget_unparent);
      g_object_notify (G_OBJECT (text_view), "extra-menu");
    }
}

/**
 * gtk_text_view_get_extra_menu: (attributes org.gtk.Method.get_property=extra-menu)
 * @text_view: a `GtkTextView`
 *
 * Gets the menu model that gets added to the context menu
 * or %NULL if none has been set.
 *
 * Returns: (transfer none): the menu model
 */
GMenuModel *
gtk_text_view_get_extra_menu (GtkTextView *text_view)
{
  GtkTextViewPrivate *priv = text_view->priv;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  return priv->extra_menu;
}

static void
gtk_text_view_real_undo (GtkWidget   *widget,
                         const char *action_name,
                         GVariant    *parameters)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  if (gtk_text_view_get_editable (text_view))
    {
      gtk_text_buffer_undo (text_view->priv->buffer);
      gtk_text_view_scroll_mark_onscreen (text_view,
                                          gtk_text_buffer_get_insert (text_view->priv->buffer));
    }
}

static void
gtk_text_view_real_redo (GtkWidget   *widget,
                         const char *action_name,
                         GVariant    *parameters)
{
  GtkTextView *text_view = GTK_TEXT_VIEW (widget);

  if (gtk_text_view_get_editable (text_view))
    {
      gtk_text_buffer_redo (text_view->priv->buffer);
      gtk_text_view_scroll_mark_onscreen (text_view,
                                          gtk_text_buffer_get_insert (text_view->priv->buffer));
    }
}

static void
gtk_text_view_buffer_notify_redo (GtkTextBuffer *buffer,
                                  GParamSpec    *pspec,
                                  GtkTextView   *view)
{
  gtk_widget_action_set_enabled (GTK_WIDGET (view),
                                 "text.redo",
                                 (gtk_text_view_get_editable (view) &&
                                  gtk_text_buffer_get_can_redo (buffer)));
}

static void
gtk_text_view_buffer_notify_undo (GtkTextBuffer *buffer,
                                  GParamSpec    *pspec,
                                  GtkTextView   *view)
{
  gtk_widget_action_set_enabled (GTK_WIDGET (view),
                                 "text.undo",
                                 (gtk_text_view_get_editable (view) &&
                                  gtk_text_buffer_get_can_undo (buffer)));
}

/**
 * gtk_text_view_get_ltr_context:
 * @text_view: a `GtkTextView`
 *
 * Gets the `PangoContext` that is used for rendering LTR directed
 * text layouts.
 *
 * The context may be replaced when CSS changes occur.
 *
 * Returns: (transfer none): a `PangoContext`
 *
 * Since: 4.4
 */
PangoContext *
gtk_text_view_get_ltr_context (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  gtk_text_view_ensure_layout (text_view);

  return text_view->priv->layout->ltr_context;
}

/**
 * gtk_text_view_get_rtl_context:
 * @text_view: a `GtkTextView`
 *
 * Gets the `PangoContext` that is used for rendering RTL directed
 * text layouts.
 *
 * The context may be replaced when CSS changes occur.
 *
 * Returns: (transfer none): a `PangoContext`
 *
 * Since: 4.4
 */
PangoContext *
gtk_text_view_get_rtl_context (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  gtk_text_view_ensure_layout (text_view);

  return text_view->priv->layout->rtl_context;
}

GtkEventController *
gtk_text_view_get_key_controller (GtkTextView *text_view)
{
  return text_view->priv->key_controller;
}

/* {{{ GtkAccessibleText implementation */

static GBytes *
gtk_text_view_accessible_text_get_contents (GtkAccessibleText *self,
                                            unsigned int       start,
                                            unsigned int       end)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextIter start_iter, end_iter;
  char *string;

  gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start);
  gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, end == G_MAXUINT ? -1 : end);

  string = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);

  return g_bytes_new_take (string, strlen (string) + 1);
}

static GBytes *
gtk_text_view_accessible_text_get_contents_at (GtkAccessibleText            *self,
                                               unsigned int                  offset,
                                               GtkAccessibleTextGranularity  granularity,
                                               unsigned int                 *start,
                                               unsigned int                 *end)
{
  GtkTextViewPrivate *priv = GTK_TEXT_VIEW (self)->priv;
  GtkTextLayout *text_layout = priv->layout;
  GtkTextBuffer *text_buffer;
  GtkTextIter iter;
  GtkTextLine *line;
  PangoLayout *line_layout;
  char *string;
  unsigned int line_start, line_end, line_offset;

  text_buffer = gtk_text_layout_get_buffer (text_layout);

  gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, offset);
  line = _gtk_text_iter_get_text_line (&iter);
  line_offset = gtk_text_iter_get_offset (&iter) - gtk_text_iter_get_line_offset (&iter);

  line_layout = gtk_text_layout_get_line_display (text_layout, line, FALSE)->layout;
  string = gtk_pango_get_string_at (line_layout, offset - line_offset, granularity, &line_start, &line_end);

  if (start != NULL)
    *start = line_offset + line_start;
  if (end != NULL)
    *end = line_offset + line_end;

  return g_bytes_new_take (string, strlen (string));
}

static unsigned int
gtk_text_view_accessible_text_get_caret_position (GtkAccessibleText *self)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextMark *insert;
  GtkTextIter iter;

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  return gtk_text_iter_get_offset (&iter);
}

static gboolean
gtk_text_view_accessible_text_get_selection (GtkAccessibleText       *self,
                                             gsize                   *n_ranges,
                                             GtkAccessibleTextRange **ranges)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextIter start_iter, end_iter;
  int start, end;

   if (!gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter))
     {
       *n_ranges = 0;
       return FALSE;
     }

  start = gtk_text_iter_get_offset (&start_iter);
  end = gtk_text_iter_get_offset (&end_iter);

  *n_ranges = 1;

  *ranges = g_new (GtkAccessibleTextRange, 1);
  (*ranges)[0].start = start;
  (*ranges)[0].length = end - start;

  return TRUE;
}

static gboolean
gtk_text_view_accessible_text_get_attributes (GtkAccessibleText        *self,
                                              unsigned int              offset,
                                              gsize                    *n_ranges,
                                              GtkAccessibleTextRange  **ranges,
                                              char                   ***attribute_names,
                                              char                   ***attribute_values)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GHashTable *attrs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  GHashTableIter iter;
  gpointer key, value;
  guint n_attrs, i;
  int start, end;

  gtk_text_buffer_add_run_attributes (buffer, offset, attrs, &start, &end);

  n_attrs = g_hash_table_size (attrs);
  if (n_attrs == 0)
    {
      g_hash_table_unref (attrs);
      *n_ranges = 0;
      *ranges = NULL;
      *attribute_names = NULL;
      *attribute_values = NULL;
      return FALSE;
    }

  *n_ranges = n_attrs;
  *ranges = g_new (GtkAccessibleTextRange, n_attrs);
  *attribute_names = g_new (char *, n_attrs + 1);
  *attribute_values = g_new (char *, n_attrs + 1);

  i = 0;
  g_hash_table_iter_init (&iter, attrs);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ((*ranges)[i]).start = start;
      ((*ranges)[i]).length = end - start;

      (*attribute_names)[i] = g_strdup (key);
      (*attribute_values)[i] = g_strdup (value);

      i += 1;
    }

  (*attribute_names)[n_attrs] = NULL;
  (*attribute_values)[n_attrs] = NULL;

  return TRUE;
}

void
gtk_text_view_add_default_attributes (GtkTextView *view,
                                      GHashTable  *attributes)
{
  GtkTextAttributes *text_attrs;
  PangoFontDescription *font;

  text_attrs = gtk_text_view_get_default_attributes (view);

  font = text_attrs->font;

  if (font)
    {
      char **names, **values;

      gtk_pango_get_font_attributes (font, &names, &values);

      for (unsigned i = 0; names[i] != NULL; i++)
        g_hash_table_insert (attributes,
                             g_steal_pointer (&names[i]),
                             g_steal_pointer (&values[i]));

      g_free (names);
      g_free (values);
    }

#define ADD_STR_ATTR(ht,name,value) \
  g_hash_table_insert ((ht), g_strdup ((name)), g_strdup ((value)))

#define ADD_BOOL_ATTR(ht,name,value) \
  g_hash_table_insert ((ht), g_strdup ((name)), (value) ? g_strdup ("true") : g_strdup ("false"))

#define ADD_COLOR_ATTR(ht,name,color) G_STMT_START { \
  char *__value = g_strdup_printf ("%u,%u,%u", \
                                   (guint) ((color)->red * 65535), \
                                   (guint) ((color)->green * 65535), \
                                   (guint) ((color)->blue * 65535)); \
  g_hash_table_insert (ht, g_strdup (name), __value); \
} G_STMT_END

#define ADD_FLOAT_ATTR(ht,name,value) G_STMT_START { \
  char *__value = g_strdup_printf ("%g", (value)); \
  g_hash_table_insert (ht, g_strdup (name), __value); \
} G_STMT_END

#define ADD_INT_ATTR(ht,name,value) G_STMT_START { \
  char *__value = g_strdup_printf ("%i", (value)); \
  g_hash_table_insert (ht, g_strdup (name), __value); \
} G_STMT_END

  ADD_STR_ATTR (attributes, "justification", gtk_justification_to_string (text_attrs->justification));
  ADD_STR_ATTR (attributes, "direction", gtk_text_direction_to_string (text_attrs->direction));
  ADD_STR_ATTR (attributes, "wrap-mode", gtk_wrap_mode_to_string (text_attrs->wrap_mode));
  ADD_STR_ATTR (attributes, "underline", pango_underline_to_string (text_attrs->appearance.underline));

  ADD_BOOL_ATTR (attributes, "editable", text_attrs->editable);
  ADD_BOOL_ATTR (attributes, "invisible", text_attrs->invisible);
  ADD_BOOL_ATTR (attributes, "bg-full-height", text_attrs->bg_full_height);
  ADD_BOOL_ATTR (attributes, "strikethrough", text_attrs->appearance.strikethrough);

  ADD_COLOR_ATTR (attributes, "bg-color", text_attrs->appearance.bg_rgba);
  ADD_COLOR_ATTR (attributes, "fg-color", text_attrs->appearance.fg_rgba);

  ADD_FLOAT_ATTR (attributes, "scale", text_attrs->font_scale);

  ADD_STR_ATTR (attributes, "language", (const char *) text_attrs->language);

  ADD_INT_ATTR (attributes, "rise", text_attrs->appearance.rise);
  ADD_INT_ATTR (attributes, "pixels-inside-wrap", text_attrs->pixels_inside_wrap);
  ADD_INT_ATTR (attributes, "pixels-below-lines", text_attrs->pixels_below_lines);
  ADD_INT_ATTR (attributes, "pixels-above-lines", text_attrs->pixels_above_lines);
  ADD_INT_ATTR (attributes, "indent", text_attrs->indent);
  ADD_INT_ATTR (attributes, "left-margin", text_attrs->left_margin);
  ADD_INT_ATTR (attributes, "right-margin", text_attrs->right_margin);

#undef ADD_STR_ATTR
#undef ADD_BOOL_ATTR
#undef ADD_COLOR_ATTR
#undef ADD_FLOAT_ATTR
#undef ADD_INT_ATTR

  gtk_text_attributes_unref (text_attrs);
}

/*< private >
 * gtk_text_view_get_attributes_run:
 * @self: a text view
 * @offset: the offset, in characters
 * @include_defaults: whether the default attributes should be included
 * @start: (out): the beginning of the run, in characters
 * @end: (out): the end of the run, in characters
 *
 * Retrieves the text attributes at the given offset.
 *
 * The serialization format is private to GTK, but conforms to the AT-SPI
 * text attributes for default attribute names.
 *
 * Returns: (transfer full) (element-type utf8,utf8): a dictionary of
 *   text attributes
 */
GHashTable *
gtk_text_view_get_attributes_run (GtkTextView *self,
                                  int          offset,
                                  gboolean     include_defaults,
                                  int         *start,
                                  int         *end)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (self);
  GHashTable *attrs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (include_defaults)
    gtk_text_view_add_default_attributes (self, attrs);

  gtk_text_buffer_add_run_attributes (buffer, offset, attrs, start, end);

  return attrs;
}

static void
gtk_text_view_accessible_text_get_default_attributes (GtkAccessibleText   *self,
                                                      char              ***attribute_names,
                                                      char              ***attribute_values)
{
  GHashTable *attrs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  GHashTableIter iter;
  gpointer key, value;
  guint n_attrs, i;

  gtk_text_view_add_default_attributes (GTK_TEXT_VIEW (self), attrs);

  n_attrs = g_hash_table_size (attrs);
  if (n_attrs == 0)
    {
      g_hash_table_unref (attrs);
      *attribute_names = NULL;
      *attribute_values = NULL;
      return;
    }

  *attribute_names = g_new (char *, n_attrs + 1);
  *attribute_values = g_new (char *, n_attrs + 1);

  i = 0;
  g_hash_table_iter_init (&iter, attrs);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      (*attribute_names)[i] = g_strdup (key);
      (*attribute_values)[i] = g_strdup (value);

      i += 1;
    }

  (*attribute_names)[n_attrs] = NULL;
  (*attribute_values)[n_attrs] = NULL;

  g_hash_table_unref (attrs);
}

static void
gtk_text_view_accessible_text_init (GtkAccessibleTextInterface *iface)
{
  iface->get_contents = gtk_text_view_accessible_text_get_contents;
  iface->get_contents_at = gtk_text_view_accessible_text_get_contents_at;
  iface->get_caret_position = gtk_text_view_accessible_text_get_caret_position;
  iface->get_selection = gtk_text_view_accessible_text_get_selection;
  iface->get_attributes = gtk_text_view_accessible_text_get_attributes;
  iface->get_default_attributes = gtk_text_view_accessible_text_get_default_attributes;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
