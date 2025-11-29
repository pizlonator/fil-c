/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2004-2006 Christian Hammond
 * Copyright (C) 2008 Cody Russell
 * Copyright (C) 2008 Red Hat, Inc.
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

#include "config.h"

#include "gtktextprivate.h"

#include "gtkaccessibletextprivate.h"
#include "gtkactionable.h"
#include "gtkactionmuxerprivate.h"
#include "gtkadjustment.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkdebug.h"
#include "gtkdragicon.h"
#include "gtkdragsourceprivate.h"
#include "gtkdroptarget.h"
#include "gtkeditable.h"
#include "gtkemojichooser.h"
#include "gtkemojicompletion.h"
#include "gtkentrybuffer.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgesturedrag.h"
#include "gtkgestureclick.h"
#include "gtkgesturesingle.h"
#include "gtkimageprivate.h"
#include "gtkimcontextsimple.h"
#include "gtkimmulticontext.h"
#include "gtkjoinedmenuprivate.h"
#include "gtklabel.h"
#include "gtkmagnifierprivate.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtknative.h"
#include "gtkpangoprivate.h"
#include "gtkpopovermenu.h"
#include "gtkprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrenderlayoutprivate.h"
#include "gtksettings.h"
#include "gtksnapshot.h"
#include "gtktexthandleprivate.h"
#include "gtktexthistoryprivate.h"
#include "gtktextutilprivate.h"
#include "gtktooltip.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindow.h"

#include "deprecated/gtkrender.h"
#include "a11y/gtkatspipangoprivate.h"

#include <string.h>
#include <cairo-gobject.h>
#include <glib/gi18n-lib.h>

/**
 * GtkText:
 *
 * The `GtkText` widget is a single-line text entry widget.
 *
 * `GtkText` is the common implementation of single-line text editing
 * that is shared between [class@Gtk.Entry], [class@Gtk.PasswordEntry],
 * [class@Gtk.SpinButton], and other widgets. In all of these, `GtkText` is
 * used as the delegate for the [iface@Gtk.Editable] implementation.
 *
 * A fairly large set of key bindings are supported by default. If the
 * entered text is longer than the allocation of the widget, the widget
 * will scroll so that the cursor position is visible.
 *
 * When using an entry for passwords and other sensitive information,
 * it can be put into “password mode” using [method@Gtk.Text.set_visibility].
 * In this mode, entered text is displayed using a “invisible” character.
 * By default, GTK picks the best invisible character that is available
 * in the current font, but it can be changed with
 * [method@Gtk.Text.set_invisible_char].
 *
 * If you are looking to add icons or progress display in an entry, look
 * at [class@Gtk.Entry]. There other alternatives for more specialized use
 * cases, such as [class@Gtk.SearchEntry].
 *
 * If you need multi-line editable text, look at [class@Gtk.TextView].
 *
 * # CSS nodes
 *
 * ```
 * text[.read-only]
 * ├── placeholder
 * ├── undershoot.left
 * ├── undershoot.right
 * ├── [selection]
 * ├── [block-cursor]
 * ╰── [window.popup]
 * ```
 *
 * `GtkText` has a main node with the name `text`. Depending on the properties
 * of the widget, the `.read-only` style class may appear.
 *
 * When the entry has a selection, it adds a subnode with the name `selection`.
 *
 * When the entry is in overwrite mode, it adds a subnode with the name
 * `block-cursor` that determines how the block cursor is drawn.
 *
 * The CSS node for a context menu is added as a subnode with the name `popup`.
 *
 * The `undershoot` nodes are used to draw the underflow indication when content
 * is scrolled out of view. These nodes get the `.left` or `.right` style class
 * added depending on where the indication is drawn.
 *
 * When touch is used and touch selection handles are shown, they are using
 * CSS nodes with name `cursor-handle`. They get the `.top` or `.bottom` style
 * class depending on where they are shown in relation to the selection. If
 * there is just a single handle for the text cursor, it gets the style class
 * `.insertion-cursor`.
 *
 * # Accessibility
 *
 * `GtkText` uses the %GTK_ACCESSIBLE_ROLE_NONE role, which causes it to be
 * skipped for accessibility. This is because `GtkText` is expected to be used
 * as a delegate for a `GtkEditable` implementation that will be represented
 * to accessibility.
 */

#define NAT_ENTRY_WIDTH  150

#define UNDERSHOOT_SIZE 20

#define DEFAULT_MAX_UNDO 200

static GQuark          quark_password_hint  = 0;

enum
{
  TEXT_HANDLE_CURSOR,
  TEXT_HANDLE_SELECTION_BOUND,
  TEXT_HANDLE_N_HANDLES
};

typedef struct _GtkTextPasswordHint GtkTextPasswordHint;

typedef struct _GtkTextPrivate GtkTextPrivate;
struct _GtkTextPrivate
{
  GtkEntryBuffer *buffer;
  GtkIMContext   *im_context;

  int             text_baseline;

  PangoLayout    *cached_layout;
  PangoAttrList  *attrs;
  PangoTabArray  *tabs;

  GdkContentProvider *selection_content;

  char         *im_module;

  GtkWidget     *emoji_completion;
  GtkTextHandle *text_handles[TEXT_HANDLE_N_HANDLES];
  GtkWidget     *selection_bubble;
  guint          selection_bubble_timeout_id;

  GtkWidget     *magnifier_popover;
  GtkWidget     *magnifier;

  GtkWidget     *placeholder;

  GtkGesture    *drag_gesture;
  GtkEventController *key_controller;
  GtkEventController *focus_controller;

  GtkCssNode    *selection_node;
  GtkCssNode    *block_cursor_node;
  GtkCssNode    *undershoot_node[2];

  GtkWidget     *popup_menu;
  GMenuModel    *extra_menu;

  GtkTextHistory *history;

  GdkDrag       *drag;

  float         xalign;

  int           ascent;                     /* font ascent in pango units  */
  int           current_pos;
  int           descent;                    /* font descent in pango units */
  int           dnd_position;               /* In chars, -1 == no DND cursor */
  int           drag_start_x;
  int           drag_start_y;
  int           insert_pos;
  int           selection_bound;
  int           scroll_offset;
  int           width_chars;
  int           max_width_chars;
  guint32       obscured_cursor_timestamp;

  gunichar      invisible_char;

  guint64       blink_start_time;
  guint         blink_tick;
  float         cursor_alpha;

  guint16       preedit_length;              /* length of preedit string, in bytes */
  guint16       preedit_cursor;              /* offset of cursor within preedit string, in chars */

  gint64        handle_place_time;

  guint         editable                : 1;
  guint         enable_emoji_completion : 1;
  guint         in_drag                 : 1;
  guint         overwrite_mode          : 1;
  guint         visible                 : 1;

  guint         activates_default       : 1;
  guint         cache_includes_preedit  : 1;
  guint         change_count            : 8;
  guint         in_click                : 1; /* Flag so we don't select all when clicking in entry to focus in */
  guint         invisible_char_set      : 1;
  guint         mouse_cursor_obscured   : 1;
  guint         need_im_reset           : 1;
  guint         real_changed            : 1;
  guint         resolved_dir            : 4; /* PangoDirection */
  guint         select_words            : 1;
  guint         select_lines            : 1;
  guint         truncate_multiline      : 1;
  guint         cursor_handle_dragged   : 1;
  guint         selection_handle_dragged : 1;
  guint         populate_all            : 1;
  guint         propagate_text_width    : 1;
  guint         text_handles_enabled    : 1;
  guint         enable_undo             : 1;
};

struct _GtkTextPasswordHint
{
  int position;      /* Position (in text) of the last password hint */
  guint source_id;    /* Timeout source id */
};

enum {
  ACTIVATE,
  MOVE_CURSOR,
  INSERT_AT_CURSOR,
  DELETE_FROM_CURSOR,
  BACKSPACE,
  CUT_CLIPBOARD,
  COPY_CLIPBOARD,
  PASTE_CLIPBOARD,
  TOGGLE_OVERWRITE,
  PREEDIT_CHANGED,
  INSERT_EMOJI,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_MAX_LENGTH,
  PROP_VISIBILITY,
  PROP_INVISIBLE_CHAR,
  PROP_INVISIBLE_CHAR_SET,
  PROP_ACTIVATES_DEFAULT,
  PROP_SCROLL_OFFSET,
  PROP_TRUNCATE_MULTILINE,
  PROP_OVERWRITE_MODE,
  PROP_IM_MODULE,
  PROP_PLACEHOLDER_TEXT,
  PROP_INPUT_PURPOSE,
  PROP_INPUT_HINTS,
  PROP_ATTRIBUTES,
  PROP_TABS,
  PROP_ENABLE_EMOJI_COMPLETION,
  PROP_PROPAGATE_TEXT_WIDTH,
  PROP_EXTRA_MENU,
  NUM_PROPERTIES
};

static GParamSpec *text_props[NUM_PROPERTIES] = { NULL, };

static guint signals[LAST_SIGNAL] = { 0 };

typedef enum {
  CURSOR_STANDARD,
  CURSOR_DND
} CursorType;

typedef enum
{
  DISPLAY_NORMAL,       /* The text is being shown */
  DISPLAY_INVISIBLE,    /* In invisible mode, text replaced by (eg) bullets */
  DISPLAY_BLANK         /* In invisible mode, nothing shown at all */
} DisplayMode;

/* GObject methods
 */
static void   gtk_text_set_property         (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void   gtk_text_get_property         (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);
static void   gtk_text_notify               (GObject      *object,
                                             GParamSpec   *pspec);
static void   gtk_text_finalize             (GObject      *object);
static void   gtk_text_dispose              (GObject      *object);

/* GtkWidget methods
 */
static void   gtk_text_realize              (GtkWidget        *widget);
static void   gtk_text_unrealize            (GtkWidget        *widget);
static void   gtk_text_map                  (GtkWidget        *widget);
static void   gtk_text_unmap                (GtkWidget        *widget);
static void   gtk_text_measure              (GtkWidget        *widget,
                                             GtkOrientation    orientation,
                                             int               for_size,
                                             int              *minimum,
                                             int              *natural,
                                             int              *minimum_baseline,
                                             int              *natural_baseline);
static void   gtk_text_size_allocate        (GtkWidget        *widget,
                                             int               width,
                                             int               height,
                                             int               baseline);
static void   gtk_text_snapshot             (GtkWidget        *widget,
                                             GtkSnapshot      *snapshot);
static void   gtk_text_focus_changed        (GtkEventControllerFocus *focus,
                                             GParamSpec              *pspec,
                                             GtkWidget               *widget);
static gboolean gtk_text_grab_focus         (GtkWidget        *widget);
static void   gtk_text_css_changed          (GtkWidget        *widget,
                                             GtkCssStyleChange *change);
static void   gtk_text_direction_changed    (GtkWidget        *widget,
                                             GtkTextDirection  previous_dir);
static void   gtk_text_state_flags_changed  (GtkWidget        *widget,
                                             GtkStateFlags     previous_state);

static gboolean gtk_text_drag_drop          (GtkDropTarget    *dest,
                                             const GValue     *value,
                                             double            x,
                                             double            y,
                                             GtkText          *text);
static gboolean gtk_text_drag_accept        (GtkDropTarget    *dest,
                                             GdkDrop          *drop,
                                             GtkText          *self);
static GdkDragAction gtk_text_drag_motion   (GtkDropTarget    *dest,
                                             double            x,
                                             double            y,
                                             GtkText          *text);
static void     gtk_text_drag_leave         (GtkDropTarget    *dest,
                                             GtkText          *text);


/* GtkEditable method implementations
 */
static void        gtk_text_editable_init        (GtkEditableInterface *iface);
static void        gtk_text_insert_text          (GtkText    *self,
                                                  const char *text,
                                                  int         length,
                                                  int        *position);
static void        gtk_text_delete_text          (GtkText    *self,
                                                  int         start_pos,
                                                  int         end_pos);
static void        gtk_text_delete_selection     (GtkText    *self);
static void        gtk_text_set_selection_bounds (GtkText    *self,
                                                  int         start,
                                                  int         end);
static gboolean    gtk_text_get_selection_bounds (GtkText    *self,
                                                  int        *start,
                                                  int        *end);

static void        gtk_text_set_editable         (GtkText    *self,
                                                  gboolean    is_editable);
static void        gtk_text_set_text             (GtkText    *self,
                                                  const char *text);
static void        gtk_text_set_width_chars      (GtkText    *self,
                                                  int         n_chars);
static void        gtk_text_set_max_width_chars  (GtkText    *self,
                                                  int         n_chars);
static void        gtk_text_set_alignment        (GtkText    *self,
                                                  float       xalign);

static void        gtk_text_set_enable_undo      (GtkText    *self,
                                                  gboolean    enable_undo);

/* Default signal handlers
 */
static GMenuModel *gtk_text_get_menu_model  (GtkText         *self);
static void     gtk_text_popup_menu         (GtkWidget       *widget,
                                             const char      *action_name,
                                             GVariant        *parameters);
static void     gtk_text_move_cursor        (GtkText         *self,
                                             GtkMovementStep  step,
                                             int              count,
                                             gboolean         extend);
static void     gtk_text_insert_at_cursor   (GtkText         *self,
                                             const char      *str);
static void     gtk_text_delete_from_cursor (GtkText         *self,
                                             GtkDeleteType    type,
                                             int              count);
static void     gtk_text_backspace          (GtkText         *self);
static void     gtk_text_cut_clipboard      (GtkText         *self);
static void     gtk_text_copy_clipboard     (GtkText         *self);
static void     gtk_text_paste_clipboard    (GtkText         *self);
static void     gtk_text_toggle_overwrite   (GtkText         *self);
static void     gtk_text_insert_emoji       (GtkText         *self);
static void     gtk_text_select_all         (GtkText         *self);
static void     gtk_text_real_activate      (GtkText         *self);

static void     direction_changed           (GdkDevice       *keyboard,
                                             GParamSpec      *pspec,
                                             GtkText         *self);

/* IM Context Callbacks
 */
static void     gtk_text_commit_cb               (GtkIMContext *context,
                                                  const char   *str,
                                                  GtkText      *self);
static void     gtk_text_preedit_start_cb        (GtkIMContext *context,
                                                  GtkText      *self);
static void     gtk_text_preedit_changed_cb      (GtkIMContext *context,
                                                  GtkText      *self);
static gboolean gtk_text_retrieve_surrounding_cb (GtkIMContext *context,
                                                  GtkText      *self);
static gboolean gtk_text_delete_surrounding_cb   (GtkIMContext *context,
                                                  int           offset,
                                                  int           n_chars,
                                                  GtkText      *self);

/* Entry buffer signal handlers
 */
static void buffer_inserted_text     (GtkEntryBuffer *buffer,
                                      guint           position,
                                      const char     *chars,
                                      guint           n_chars,
                                      GtkText        *self);
static void buffer_deleted_text      (GtkEntryBuffer *buffer,
                                      guint           position,
                                      guint           n_chars,
                                      GtkText        *self);
static void buffer_notify_text       (GtkEntryBuffer *buffer,
                                      GParamSpec     *spec,
                                      GtkText        *self);
static void buffer_notify_max_length (GtkEntryBuffer *buffer,
                                      GParamSpec     *spec,
                                      GtkText        *self);

/* Event controller callbacks
 */
static void     gtk_text_motion_controller_motion   (GtkEventControllerMotion *controller,
                                                     double                    x,
                                                     double                    y,
                                                     GtkText                  *self);
static void     gtk_text_click_gesture_pressed (GtkGestureClick     *gesture,
                                                int                       n_press,
                                                double                    x,
                                                double                    y,
                                                GtkText                  *self);
static void     gtk_text_click_gesture_released (GtkGestureClick     *gesture,
                                                 int                       n_press,
                                                 double                    x,
                                                 double                    y,
                                                 GtkText                  *self);
static void     gtk_text_drag_gesture_update        (GtkGestureDrag           *gesture,
                                                     double                    offset_x,
                                                     double                    offset_y,
                                                     GtkText                  *self);
static void     gtk_text_drag_gesture_end           (GtkGestureDrag           *gesture,
                                                     double                     offset_x,
                                                     double                    offset_y,
                                                     GtkText                  *self);
static gboolean gtk_text_key_controller_key_pressed  (GtkEventControllerKey   *controller,
                                                      guint                    keyval,
                                                      guint                    keycode,
                                                      GdkModifierType          state,
                                                      GtkText                 *self);


/* GtkTextHandle handlers */
static void gtk_text_handle_drag_started  (GtkTextHandle          *handle,
                                           GtkText                *self);
static void gtk_text_handle_dragged       (GtkTextHandle          *handle,
                                           int                     x,
                                           int                     y,
                                           GtkText                *self);
static void gtk_text_handle_drag_finished (GtkTextHandle          *handle,
                                           GtkText                *self);

/* Internal routines
 */
static void         gtk_text_draw_text                (GtkText       *self,
                                                       GtkSnapshot   *snapshot);
static void         gtk_text_draw_cursor              (GtkText       *self,
                                                       GtkSnapshot   *snapshot,
                                                       CursorType     type);
static PangoLayout *gtk_text_ensure_layout            (GtkText       *self,
                                                       gboolean       include_preedit);
static void         gtk_text_reset_layout             (GtkText       *self);
static void         gtk_text_recompute                (GtkText       *self);
static int          gtk_text_find_position            (GtkText       *self,
                                                       int            x);
static void         gtk_text_get_cursor_locations     (GtkText       *self,
                                                       int           *strong_x,
                                                       int           *weak_x);
static void         gtk_text_adjust_scroll            (GtkText       *self);
static int          gtk_text_move_visually            (GtkText        *editable,
                                                       int             start,
                                                       int             count);
static int          gtk_text_move_logically           (GtkText        *self,
                                                       int             start,
                                                       int             count);
static int          gtk_text_move_forward_word        (GtkText        *self,
                                                       int             start,
                                                       gboolean        allow_whitespace);
static int          gtk_text_move_backward_word       (GtkText        *self,
                                                       int             start,
                                                       gboolean        allow_whitespace);
static void         gtk_text_delete_whitespace        (GtkText        *self);
static void         gtk_text_select_word              (GtkText        *self);
static void         gtk_text_select_line              (GtkText        *self);
static void         gtk_text_paste                    (GtkText        *self,
                                                       GdkClipboard   *clipboard);
static void         gtk_text_update_primary_selection (GtkText        *self);
static void         gtk_text_schedule_im_reset        (GtkText        *self);
static gboolean     gtk_text_mnemonic_activate        (GtkWidget      *widget,
                                                       gboolean        group_cycling);
static void         gtk_text_check_cursor_blink       (GtkText        *self);
static void         remove_blink_timeout              (GtkText        *self);
static void         gtk_text_pend_cursor_blink        (GtkText        *self);
static void         gtk_text_reset_blink_time         (GtkText        *self);
static void         gtk_text_update_cached_style_values(GtkText       *self);
static gboolean     get_middle_click_paste             (GtkText       *self);
static void         gtk_text_get_scroll_limits        (GtkText        *self,
                                                       int            *min_offset,
                                                       int            *max_offset);
static GtkEntryBuffer *get_buffer                      (GtkText       *self);
static void         set_text_cursor                    (GtkWidget     *widget);
static void         update_placeholder_visibility      (GtkText       *self);

static void         buffer_connect_signals             (GtkText       *self);
static void         buffer_disconnect_signals          (GtkText       *self);

static void         gtk_text_selection_bubble_popup_set   (GtkText *self);
static void         gtk_text_selection_bubble_popup_unset (GtkText *self);

static void         begin_change                       (GtkText *self);
static void         end_change                         (GtkText *self);
static void         emit_changed                       (GtkText *self);

static void         gtk_text_update_history           (GtkText *self);
static void         gtk_text_update_clipboard_actions (GtkText *self);
static void         gtk_text_update_emoji_action      (GtkText *self);
static void         gtk_text_update_handles           (GtkText *self);

static void gtk_text_activate_clipboard_cut          (GtkWidget  *widget,
                                                      const char *action_name,
                                                      GVariant   *parameter);
static void gtk_text_activate_clipboard_copy         (GtkWidget  *widget,
                                                      const char *action_name,
                                                      GVariant   *parameter);
static void gtk_text_activate_clipboard_paste        (GtkWidget  *widget,
                                                      const char *action_name,
                                                      GVariant   *parameter);
static void gtk_text_activate_selection_delete       (GtkWidget  *widget,
                                                      const char *action_name,
                                                      GVariant   *parameter);
static void gtk_text_activate_selection_select_all   (GtkWidget  *widget,
                                                      const char *action_name,
                                                      GVariant   *parameter);
static void gtk_text_activate_misc_insert_emoji      (GtkWidget  *widget,
                                                      const char *action_name,
                                                      GVariant   *parameter);
static void gtk_text_real_undo                       (GtkWidget  *widget,
                                                      const char *action_name,
                                                      GVariant   *parameters);
static void gtk_text_real_redo                       (GtkWidget  *widget,
                                                      const char *action_name,
                                                      GVariant   *parameters);
static void gtk_text_history_change_state_cb         (gpointer    funcs_data,
                                                      gboolean    is_modified,
                                                      gboolean    can_undo,
                                                      gboolean    can_redo);
static void gtk_text_history_insert_cb               (gpointer    funcs_data,
                                                      guint       begin,
                                                      guint       end,
                                                      const char *text,
                                                      guint       len);
static void gtk_text_history_delete_cb               (gpointer    funcs_data,
                                                      guint       begin,
                                                      guint       end,
                                                      const char *expected_text,
                                                      guint       len);
static void gtk_text_history_select_cb               (gpointer    funcs_data,
                                                      int         selection_insert,
                                                      int         selection_bound);

/* GtkTextContent implementation
 */
#define GTK_TYPE_TEXT_CONTENT            (gtk_text_content_get_type ())
#define GTK_TEXT_CONTENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TEXT_CONTENT, GtkTextContent))
#define GTK_IS_TEXT_CONTENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TEXT_CONTENT))
#define GTK_TEXT_CONTENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_CONTENT, GtkTextContentClass))
#define GTK_IS_TEXT_CONTENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_CONTENT))
#define GTK_TEXT_CONTENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT_CONTENT, GtkTextContentClass))

typedef struct _GtkTextContent GtkTextContent;
typedef struct _GtkTextContentClass GtkTextContentClass;

struct _GtkTextContent
{
  GdkContentProvider parent;

  GtkText *self;
};

struct _GtkTextContentClass
{
  GdkContentProviderClass parent_class;
};

GType gtk_text_content_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkTextContent, gtk_text_content, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gtk_text_content_ref_formats (GdkContentProvider *provider)
{
  return gdk_content_formats_new_for_gtype (G_TYPE_STRING);
}

static gboolean
gtk_text_content_get_value (GdkContentProvider  *provider,
                            GValue              *value,
                            GError             **error)
{
  GtkTextContent *content = GTK_TEXT_CONTENT (provider);

  if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    {
      int start, end;

      if (gtk_text_get_selection_bounds (content->self, &start, &end))
        {
          char *str = gtk_text_get_display_text (content->self, start, end);
          g_value_take_string (value, str);
        }
      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (gtk_text_content_parent_class)->get_value (provider, value, error);
}

static void
gtk_text_content_detach (GdkContentProvider *provider,
                          GdkClipboard       *clipboard)
{
  GtkTextContent *content = GTK_TEXT_CONTENT (provider);
  int current_pos, selection_bound;

  gtk_text_get_selection_bounds (content->self, &current_pos, &selection_bound);
  gtk_text_set_selection_bounds (content->self, current_pos, current_pos);
}

static void
gtk_text_content_class_init (GtkTextContentClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  provider_class->ref_formats = gtk_text_content_ref_formats;
  provider_class->get_value = gtk_text_content_get_value;
  provider_class->detach_clipboard = gtk_text_content_detach;
}

static void
gtk_text_content_init (GtkTextContent *content)
{
}

/* GtkText
 */

static const GtkTextHistoryFuncs history_funcs = {
  gtk_text_history_change_state_cb,
  gtk_text_history_insert_cb,
  gtk_text_history_delete_cb,
  gtk_text_history_select_cb,
};

static void     gtk_text_accessible_text_init (GtkAccessibleTextInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkText, gtk_text, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkText)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, gtk_text_editable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_TEXT,
                                                gtk_text_accessible_text_init))


static void
add_move_binding (GtkWidgetClass *widget_class,
                  guint           keyval,
                  guint           modmask,
                  GtkMovementStep step,
                  int             count)
{
  g_return_if_fail ((modmask & GDK_SHIFT_MASK) == 0);

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
gtk_text_class_init (GtkTextClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->dispose = gtk_text_dispose;
  gobject_class->finalize = gtk_text_finalize;
  gobject_class->set_property = gtk_text_set_property;
  gobject_class->get_property = gtk_text_get_property;
  gobject_class->notify = gtk_text_notify;

  widget_class->map = gtk_text_map;
  widget_class->unmap = gtk_text_unmap;
  widget_class->realize = gtk_text_realize;
  widget_class->unrealize = gtk_text_unrealize;
  widget_class->measure = gtk_text_measure;
  widget_class->size_allocate = gtk_text_size_allocate;
  widget_class->snapshot = gtk_text_snapshot;
  widget_class->grab_focus = gtk_text_grab_focus;
  widget_class->css_changed = gtk_text_css_changed;
  widget_class->direction_changed = gtk_text_direction_changed;
  widget_class->state_flags_changed = gtk_text_state_flags_changed;
  widget_class->mnemonic_activate = gtk_text_mnemonic_activate;

  class->move_cursor = gtk_text_move_cursor;
  class->insert_at_cursor = gtk_text_insert_at_cursor;
  class->delete_from_cursor = gtk_text_delete_from_cursor;
  class->backspace = gtk_text_backspace;
  class->cut_clipboard = gtk_text_cut_clipboard;
  class->copy_clipboard = gtk_text_copy_clipboard;
  class->paste_clipboard = gtk_text_paste_clipboard;
  class->toggle_overwrite = gtk_text_toggle_overwrite;
  class->insert_emoji = gtk_text_insert_emoji;
  class->activate = gtk_text_real_activate;

  quark_password_hint = g_quark_from_static_string ("gtk-entry-password-hint");

  /**
   * GtkText:buffer: (attributes org.gtk.Property.get=gtk_text_get_buffer org.gtk.Property.set=gtk_text_set_buffer)
   *
   * The `GtkEntryBuffer` object which stores the text.
   */
  text_props[PROP_BUFFER] =
      g_param_spec_object ("buffer", NULL, NULL,
                           GTK_TYPE_ENTRY_BUFFER,
                           GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:max-length: (attributes org.gtk.Property.get=gtk_text_get_max_length org.gtk.Property.set=gtk_text_set_max_length)
   *
   * Maximum number of characters that are allowed.
   *
   * Zero indicates no limit.
   */
  text_props[PROP_MAX_LENGTH] =
      g_param_spec_int ("max-length", NULL, NULL,
                        0, GTK_ENTRY_BUFFER_MAX_SIZE,
                        0,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:invisible-char: (attributes org.gtk.Property.get=gtk_text_set_invisible_char org.gtk.Property.set=gtk_text_set_invisible_char)
   *
   * The character to used when masking contents (in “password mode”).
   */
  text_props[PROP_INVISIBLE_CHAR] =
      g_param_spec_unichar ("invisible-char", NULL, NULL,
                            '*',
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:activates-default: (attributes org.gtk.Property.get=gtk_text_get_activates_default org.gtk.Property.set=gtk_text_set_activates_default)
   *
   * Whether to activate the default widget when Enter is pressed.
   */
  text_props[PROP_ACTIVATES_DEFAULT] =
      g_param_spec_boolean ("activates-default", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:scroll-offset:
   *
   * Number of pixels scrolled of the screen to the left.
   */
  text_props[PROP_SCROLL_OFFSET] =
      g_param_spec_int ("scroll-offset", NULL, NULL,
                        0, G_MAXINT,
                        0,
                        GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:truncate-multiline: (attributes org.gtk.Property.get=gtk_text_get_truncate_multiline org.gtk.Property.set=gtk_text_set_truncate_multiline)
   *
   * When %TRUE, pasted multi-line text is truncated to the first line.
   */
  text_props[PROP_TRUNCATE_MULTILINE] =
      g_param_spec_boolean ("truncate-multiline", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:overwrite-mode: (attributes org.gtk.Property.get=gtk_text_get_overwrite_mode org.gtk.Property.set=gtk_text_set_overwrite_mode)
   *
   * If text is overwritten when typing in the `GtkText`.
   */
  text_props[PROP_OVERWRITE_MODE] =
      g_param_spec_boolean ("overwrite-mode", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:invisible-char-set:
   *
   * Whether the invisible char has been set for the `GtkText`.
   */
  text_props[PROP_INVISIBLE_CHAR_SET] =
      g_param_spec_boolean ("invisible-char-set", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE);

  /**
  * GtkText:placeholder-text: (attributes org.gtk.Property.get=gtk_text_get_placeholder_text org.gtk.Property.set=gtk_text_set_placeholder_text)
  *
  * The text that will be displayed in the `GtkText` when it is empty
  * and unfocused.
  */
  text_props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:im-module:
   *
   * Which IM (input method) module should be used for this self.
   *
   * See [class@Gtk.IMMulticontext].
   *
   * Setting this to a non-%NULL value overrides the system-wide
   * IM module setting. See the [property@Gtk.Settings:gtk-im-module]
   * property.
   */
  text_props[PROP_IM_MODULE] =
      g_param_spec_string ("im-module", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:input-purpose: (attributes org.gtk.Property.get=gtk_text_get_input_purpose org.gtk.Property.set=gtk_text_set_input_purpose)
   *
   * The purpose of this text field.
   *
   * This property can be used by on-screen keyboards and other input
   * methods to adjust their behaviour.
   *
   * Note that setting the purpose to %GTK_INPUT_PURPOSE_PASSWORD or
   * %GTK_INPUT_PURPOSE_PIN is independent from setting
   * [property@Gtk.Text:visibility].
   */
  text_props[PROP_INPUT_PURPOSE] =
      g_param_spec_enum ("input-purpose", NULL, NULL,
                         GTK_TYPE_INPUT_PURPOSE,
                         GTK_INPUT_PURPOSE_FREE_FORM,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:input-hints: (attributes org.gtk.Property.get=gtk_text_get_input_hints org.gtk.Property.set=gtk_text_set_input_hints)
   *
   * Additional hints that allow input methods to fine-tune
   * their behaviour.
   */
  text_props[PROP_INPUT_HINTS] =
      g_param_spec_flags ("input-hints", NULL, NULL,
                          GTK_TYPE_INPUT_HINTS,
                          GTK_INPUT_HINT_NONE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:attributes: (attributes org.gtk.Property.get=gtk_text_get_attributes org.gtk.Property.set=gtk_text_set_attributes)
   *
   * A list of Pango attributes to apply to the text of the `GtkText`.
   *
   * This is mainly useful to change the size or weight of the text.
   *
   * The `PangoAttribute`'s @start_index and @end_index must refer to the
   * `GtkEntryBuffer` text, i.e. without the preedit string.
   */
  text_props[PROP_ATTRIBUTES] =
      g_param_spec_boxed ("attributes", NULL, NULL,
                          PANGO_TYPE_ATTR_LIST,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:tabs: (attributes org.gtk.Property.get=gtk_text_get_tabs org.gtk.Property.set=gtk_text_set_tabs)
   *
   * A list of tabstops to apply to the text of the `GtkText`.
   */
  text_props[PROP_TABS] =
      g_param_spec_boxed ("tabs", NULL, NULL,
                          PANGO_TYPE_TAB_ARRAY,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:enable-emoji-completion: (attributes org.gtk.Property.get=gtk_text_get_enable_emoji_completion org.gtk.Property.set=gtk_text_set_enable_emoji_completion)
   *
   * Whether to suggest Emoji replacements.
   */
  text_props[PROP_ENABLE_EMOJI_COMPLETION] =
      g_param_spec_boolean ("enable-emoji-completion", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:visibility: (attributes org.gtk.Property.get=gtk_text_get_visibility org.gtk.Property.set=gtk_text_set_visibility)
   *
   * If %FALSE, the text is masked with the “invisible char”.
   */
  text_props[PROP_VISIBILITY] =
      g_param_spec_boolean ("visibility", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:propagate-text-width: (attributes org.gtk.Property.get=gtk_text_get_propagate_text_width org.gtk.Property.set=gtk_text_set_propagate_text_width)
   *
   * Whether the widget should grow and shrink with the content.
   */
  text_props[PROP_PROPAGATE_TEXT_WIDTH] =
      g_param_spec_boolean ("propagate-text-width", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkText:extra-menu: (attributes org.gtk.Property.get=gtk_text_get_extra_menu org.gtk.Property.set=gtk_text_set_extra_menu)
   *
   * A menu model whose contents will be appended to
   * the context menu.
   */
  text_props[PROP_EXTRA_MENU] =
      g_param_spec_object ("extra-menu", NULL, NULL,
                          G_TYPE_MENU_MODEL,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, text_props);

  gtk_editable_install_properties (gobject_class, NUM_PROPERTIES);

 /* Action signals */

  /**
   * GtkText::activate:
   * @self: The widget on which the signal is emitted
   *
   * Emitted when the user hits the <kbd>Enter</kbd> key.
   *
   * The default bindings for this signal are all forms
   * of the <kbd>Enter</kbd> key.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::move-cursor:
   * @self: the object which received the signal
   * @step: the granularity of the move, as a `GtkMovementStep`
   * @count: the number of @step units to move
   * @extend: %TRUE if the move should extend the selection
   *
   * Emitted when the user initiates a cursor movement.
   *
   * If the cursor is not visible in @self, this signal causes
   * the viewport to be moved instead.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
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
   */
  signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, move_cursor),
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
   * GtkText::insert-at-cursor:
   * @self: the object which received the signal
   * @string: the string to insert
   *
   * Emitted when the user initiates the insertion of a
   * fixed string at the cursor.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * This signal has no default bindings.
   */
  signals[INSERT_AT_CURSOR] =
    g_signal_new (I_("insert-at-cursor"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, insert_at_cursor),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  /**
   * GtkText::delete-from-cursor:
   * @self: the object which received the signal
   * @type: the granularity of the deletion, as a `GtkDeleteType`
   * @count: the number of @type units to delete
   *
   * Emitted when the user initiates a text deletion.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * If the @type is %GTK_DELETE_CHARS, GTK deletes the selection
   * if there is one, otherwise it deletes the requested number
   * of characters.
   *
   * The default bindings for this signal are <kbd>Delete</kbd>
   * for deleting a character and <kbd>Ctrl</kbd>+<kbd>Delete</kbd>
   * for deleting a word.
   */
  signals[DELETE_FROM_CURSOR] =
    g_signal_new (I_("delete-from-cursor"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, delete_from_cursor),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_INT,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_DELETE_TYPE,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (signals[DELETE_FROM_CURSOR],
                              G_OBJECT_CLASS_TYPE (gobject_class),
                              _gtk_marshal_VOID__ENUM_INTv);

  /**
   * GtkText::backspace:
   * @self: the object which received the signal
   *
   * Emitted when the user asks for it.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Backspace</kbd> and <kbd>Shift</kbd>+<kbd>Backspace</kbd>.
   */
  signals[BACKSPACE] =
    g_signal_new (I_("backspace"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, backspace),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::cut-clipboard:
   * @self: the object which received the signal
   *
   * Emitted to cut the selection to the clipboard.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>x</kbd> and
   * <kbd>Shift</kbd>+<kbd>Delete</kbd>.
   */
  signals[CUT_CLIPBOARD] =
    g_signal_new (I_("cut-clipboard"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, cut_clipboard),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::copy-clipboard:
   * @self: the object which received the signal
   *
   * Emitted to copy the selection to the clipboard.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>c</kbd> and
   * <kbd>Ctrl</kbd>+<kbd>Insert</kbd>.
   */
  signals[COPY_CLIPBOARD] =
    g_signal_new (I_("copy-clipboard"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, copy_clipboard),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::paste-clipboard:
   * @self: the object which received the signal
   *
   * Emitted to paste the contents of the clipboard.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>v</kbd> and <kbd>Shift</kbd>+<kbd>Insert</kbd>.
   */
  signals[PASTE_CLIPBOARD] =
    g_signal_new (I_("paste-clipboard"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, paste_clipboard),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::toggle-overwrite:
   * @self: the object which received the signal
   *
   * Emitted to toggle the overwrite mode of the `GtkText`.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal is <kbd>Insert</kbd>.
   */
  signals[TOGGLE_OVERWRITE] =
    g_signal_new (I_("toggle-overwrite"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, toggle_overwrite),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkText::preedit-changed:
   * @self: the object which received the signal
   * @preedit: the current preedit string
   *
   * Emitted when the preedit text changes.
   *
   * If an input method is used, the typed text will not immediately
   * be committed to the buffer. So if you are interested in the text,
   * connect to this signal.
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
   * GtkText::insert-emoji:
   * @self: the object which received the signal
   *
   * Emitted to present the Emoji chooser for the widget.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default bindings for this signal are
   * <kbd>Ctrl</kbd>+<kbd>.</kbd> and
   * <kbd>Ctrl</kbd>+<kbd>;</kbd>
   */
  signals[INSERT_EMOJI] =
    g_signal_new (I_("insert-emoji"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTextClass, insert_emoji),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /*
   * Actions
   */

  /**
   * GtkText|clipboard.cut:
   *
   * Copies the contents to the clipboard and deletes it from the widget.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.cut", NULL,
                                   gtk_text_activate_clipboard_cut);

  /**
   * GtkText|clipboard.copy:
   *
   * Copies the contents to the clipboard.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL,
                                   gtk_text_activate_clipboard_copy);

  /**
   * GtkText|clipboard.paste:
   *
   * Inserts the contents of the clipboard into the widget.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.paste", NULL,
                                   gtk_text_activate_clipboard_paste);

  /**
   * GtkText|selection.delete:
   *
   * Deletes the current selection.
   */
  gtk_widget_class_install_action (widget_class, "selection.delete", NULL,
                                   gtk_text_activate_selection_delete);

  /**
   * GtkText|selection.select-all:
   *
   * Selects all of the widgets content.
   */
  gtk_widget_class_install_action (widget_class, "selection.select-all", NULL,
                                   gtk_text_activate_selection_select_all);

  /**
   * GtkText|misc.insert-emoji:
   *
   * Opens the Emoji chooser.
   */
  gtk_widget_class_install_action (widget_class, "misc.insert-emoji", NULL,
                                   gtk_text_activate_misc_insert_emoji);

  /**
   * GtkText|misc.toggle-visibility:
   *
   * Toggles the `GtkText`:visibility property.
   */
  gtk_widget_class_install_property_action (widget_class,
                                            "misc.toggle-visibility",
                                            "visibility");

  /**
   * GtkText|text.undo:
   *
   * Undoes the last change to the contents.
   */
  gtk_widget_class_install_action (widget_class, "text.undo", NULL, gtk_text_real_undo);

  /**
   * GtkText|text.redo:
   *
   * Redoes the last change to the contents.
   */
  gtk_widget_class_install_action (widget_class, "text.redo", NULL, gtk_text_real_redo);

  /**
   * GtkText|menu.popup:
   *
   * Opens the context menu.
   */
  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, gtk_text_popup_menu);

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

  add_move_binding (widget_class, GDK_KEY_Left, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Right, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Left, 0,
                    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (widget_class, GDK_KEY_Right, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (widget_class, GDK_KEY_Left, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (widget_class, GDK_KEY_Home, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_End, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Home, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_End, 0,
                    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_Home, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_End, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (widget_class, GDK_KEY_KP_Home, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (widget_class, GDK_KEY_KP_End, GDK_CONTROL_MASK,
                    GTK_MOVEMENT_BUFFER_ENDS, 1);

  /* Select all
   */
  gtk_widget_class_add_binding (widget_class,
                                GDK_KEY_a, GDK_CONTROL_MASK,
                                (GtkShortcutFunc) gtk_text_select_all,
                                NULL);

  gtk_widget_class_add_binding (widget_class,
                                GDK_KEY_slash, GDK_CONTROL_MASK,
                                (GtkShortcutFunc) gtk_text_select_all,
                                NULL);
  /* Unselect all
   */
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_backslash, GDK_CONTROL_MASK,
                                       "move-cursor",
                                       "(iib)", GTK_MOVEMENT_VISUAL_POSITIONS, 0, FALSE);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_a, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "move-cursor",
                                       "(iib)", GTK_MOVEMENT_VISUAL_POSITIONS, 0, FALSE);

  /* Activate
   */
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0,
                                       "activate",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter, 0,
                                       "activate",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0,
                                       "activate",
                                       NULL);

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

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_u, GDK_CONTROL_MASK,
                                       "delete-from-cursor",
                                       "(ii)", GTK_DELETE_PARAGRAPH_ENDS, -1);

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

  gtk_widget_class_set_css_name (widget_class, I_("text"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_NONE);
}

static void
editable_insert_text (GtkEditable *editable,
                      const char  *text,
                      int          length,
                      int         *position)
{
  gtk_text_insert_text (GTK_TEXT (editable), text, length, position);
}

static void
editable_delete_text (GtkEditable *editable,
                      int          start_pos,
                      int          end_pos)
{
  gtk_text_delete_text (GTK_TEXT (editable), start_pos, end_pos);
}

static const char *
editable_get_text (GtkEditable *editable)
{
  return gtk_entry_buffer_get_text (get_buffer (GTK_TEXT (editable)));
}

static void
editable_set_selection_bounds (GtkEditable *editable,
                               int          start_pos,
                               int          end_pos)
{
  gtk_text_set_selection_bounds (GTK_TEXT (editable), start_pos, end_pos);
}

static gboolean
editable_get_selection_bounds (GtkEditable *editable,
                               int         *start_pos,
                               int         *end_pos)
{
  return gtk_text_get_selection_bounds (GTK_TEXT (editable), start_pos, end_pos);
}

static void
gtk_text_editable_init (GtkEditableInterface *iface)
{
  iface->insert_text = editable_insert_text;
  iface->delete_text = editable_delete_text;
  iface->get_text = editable_get_text;
  iface->set_selection_bounds = editable_set_selection_bounds;
  iface->get_selection_bounds = editable_get_selection_bounds;
}

static void
gtk_text_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkText *self = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  switch (prop_id)
    {
    /* GtkEditable properties */
    case NUM_PROPERTIES + GTK_EDITABLE_PROP_EDITABLE:
      gtk_text_set_editable (self, g_value_get_boolean (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_WIDTH_CHARS:
      gtk_text_set_width_chars (self, g_value_get_int (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      gtk_text_set_max_width_chars (self, g_value_get_int (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_TEXT:
      gtk_text_set_text (self, g_value_get_string (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_XALIGN:
      gtk_text_set_alignment (self, g_value_get_float (value));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_ENABLE_UNDO:
      gtk_text_set_enable_undo (self, g_value_get_boolean (value));
      break;

    /* GtkText properties */
    case PROP_BUFFER:
      gtk_text_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_MAX_LENGTH:
      gtk_text_set_max_length (self, g_value_get_int (value));
      break;

    case PROP_VISIBILITY:
      gtk_text_set_visibility (self, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR:
      gtk_text_set_invisible_char (self, g_value_get_uint (value));
      break;

    case PROP_ACTIVATES_DEFAULT:
      gtk_text_set_activates_default (self, g_value_get_boolean (value));
      break;

    case PROP_TRUNCATE_MULTILINE:
      gtk_text_set_truncate_multiline (self, g_value_get_boolean (value));
      break;

    case PROP_OVERWRITE_MODE:
      gtk_text_set_overwrite_mode (self, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR_SET:
      if (g_value_get_boolean (value))
        priv->invisible_char_set = TRUE;
      else
        gtk_text_unset_invisible_char (self);
      break;

    case PROP_PLACEHOLDER_TEXT:
      gtk_text_set_placeholder_text (self, g_value_get_string (value));
      break;

    case PROP_IM_MODULE:
      g_free (priv->im_module);
      priv->im_module = g_value_dup_string (value);
      if (GTK_IS_IM_MULTICONTEXT (priv->im_context))
        gtk_im_multicontext_set_context_id (GTK_IM_MULTICONTEXT (priv->im_context), priv->im_module);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_INPUT_PURPOSE:
      gtk_text_set_input_purpose (self, g_value_get_enum (value));
      break;

    case PROP_INPUT_HINTS:
      gtk_text_set_input_hints (self, g_value_get_flags (value));
      break;

    case PROP_ATTRIBUTES:
      gtk_text_set_attributes (self, g_value_get_boxed (value));
      break;

    case PROP_TABS:
      gtk_text_set_tabs (self, g_value_get_boxed (value));
      break;

    case PROP_ENABLE_EMOJI_COMPLETION:
      gtk_text_set_enable_emoji_completion (self, g_value_get_boolean (value));
      break;

    case PROP_PROPAGATE_TEXT_WIDTH:
      gtk_text_set_propagate_text_width (self, g_value_get_boolean (value));
      break;

    case PROP_EXTRA_MENU:
      gtk_text_set_extra_menu (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkText *self = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  switch (prop_id)
    {
    /* GtkEditable properties */
    case NUM_PROPERTIES + GTK_EDITABLE_PROP_CURSOR_POSITION:
      g_value_set_int (value, priv->current_pos);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_SELECTION_BOUND:
      g_value_set_int (value, priv->selection_bound);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_EDITABLE:
      g_value_set_boolean (value, priv->editable);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_WIDTH_CHARS:
      g_value_set_int (value, priv->width_chars);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, priv->max_width_chars);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_TEXT:
      g_value_set_string (value, gtk_entry_buffer_get_text (get_buffer (self)));
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_XALIGN:
      g_value_set_float (value, priv->xalign);
      break;

    case NUM_PROPERTIES + GTK_EDITABLE_PROP_ENABLE_UNDO:
      g_value_set_boolean (value, priv->enable_undo);
      break;

    /* GtkText properties */
    case PROP_BUFFER:
      g_value_set_object (value, get_buffer (self));
      break;

    case PROP_MAX_LENGTH:
      g_value_set_int (value, gtk_entry_buffer_get_max_length (get_buffer (self)));
      break;

    case PROP_VISIBILITY:
      g_value_set_boolean (value, priv->visible);
      break;

    case PROP_INVISIBLE_CHAR:
      g_value_set_uint (value, priv->invisible_char);
      break;

    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, priv->activates_default);
      break;

    case PROP_SCROLL_OFFSET:
      g_value_set_int (value, priv->scroll_offset);
      break;

    case PROP_TRUNCATE_MULTILINE:
      g_value_set_boolean (value, priv->truncate_multiline);
      break;

    case PROP_OVERWRITE_MODE:
      g_value_set_boolean (value, priv->overwrite_mode);
      break;

    case PROP_INVISIBLE_CHAR_SET:
      g_value_set_boolean (value, priv->invisible_char_set);
      break;

    case PROP_IM_MODULE:
      g_value_set_string (value, priv->im_module);
      break;

    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_text_get_placeholder_text (self));
      break;

    case PROP_INPUT_PURPOSE:
      g_value_set_enum (value, gtk_text_get_input_purpose (self));
      break;

    case PROP_INPUT_HINTS:
      g_value_set_flags (value, gtk_text_get_input_hints (self));
      break;

    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, priv->attrs);
      break;

    case PROP_TABS:
      g_value_set_boxed (value, priv->tabs);
      break;

    case PROP_ENABLE_EMOJI_COMPLETION:
      g_value_set_boolean (value, priv->enable_emoji_completion);
      break;

    case PROP_PROPAGATE_TEXT_WIDTH:
      g_value_set_boolean (value, priv->propagate_text_width);
      break;

    case PROP_EXTRA_MENU:
      g_value_set_object (value, priv->extra_menu);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_notify (GObject    *object,
                 GParamSpec *pspec)
{
  if (pspec->name == I_("has-focus"))
    gtk_text_check_cursor_blink (GTK_TEXT (object));

  if (G_OBJECT_CLASS (gtk_text_parent_class)->notify)
    G_OBJECT_CLASS (gtk_text_parent_class)->notify (object, pspec);
}

static void
gtk_text_ensure_text_handles (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int i;

  for (i = 0; i < TEXT_HANDLE_N_HANDLES; i++)
    {
      if (priv->text_handles[i])
        continue;
      priv->text_handles[i] = gtk_text_handle_new (GTK_WIDGET (self));
      g_signal_connect (priv->text_handles[i], "drag-started",
                        G_CALLBACK (gtk_text_handle_drag_started), self);
      g_signal_connect (priv->text_handles[i], "handle-dragged",
                        G_CALLBACK (gtk_text_handle_dragged), self);
      g_signal_connect (priv->text_handles[i], "drag-finished",
                        G_CALLBACK (gtk_text_handle_drag_finished), self);
    }
}

static void
gtk_text_init (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkCssNode *widget_node;
  GtkGesture *gesture;
  GtkEventController *controller;
  int i;
  GtkDropTarget *target;

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);

  priv->editable = TRUE;
  priv->visible = TRUE;
  priv->dnd_position = -1;
  priv->width_chars = -1;
  priv->max_width_chars = -1;
  priv->truncate_multiline = FALSE;
  priv->xalign = 0.0;
  priv->insert_pos = -1;
  priv->cursor_alpha = 1.0;
  priv->invisible_char = 0;
  priv->history = gtk_text_history_new (&history_funcs, self);
  priv->enable_undo = TRUE;

  gtk_text_history_set_max_undo_levels (priv->history, DEFAULT_MAX_UNDO);

  priv->selection_content = g_object_new (GTK_TYPE_TEXT_CONTENT, NULL);
  GTK_TEXT_CONTENT (priv->selection_content)->self = self;

  target = gtk_drop_target_new (G_TYPE_STRING, GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_event_controller_set_static_name (GTK_EVENT_CONTROLLER (target), "gtk-text-drop-target");
  g_signal_connect (target, "accept", G_CALLBACK (gtk_text_drag_accept), self);
  g_signal_connect (target, "enter", G_CALLBACK (gtk_text_drag_motion), self);
  g_signal_connect (target, "motion", G_CALLBACK (gtk_text_drag_motion), self);
  g_signal_connect (target, "leave", G_CALLBACK (gtk_text_drag_leave), self);
  g_signal_connect (target, "drop", G_CALLBACK (gtk_text_drag_drop), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (target));

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  priv->im_context = gtk_im_multicontext_new ();

  g_signal_connect (priv->im_context, "preedit-start",
                    G_CALLBACK (gtk_text_preedit_start_cb), self);
  g_signal_connect (priv->im_context, "commit",
                    G_CALLBACK (gtk_text_commit_cb), self);
  g_signal_connect (priv->im_context, "preedit-changed",
                    G_CALLBACK (gtk_text_preedit_changed_cb), self);
  g_signal_connect (priv->im_context, "retrieve-surrounding",
                    G_CALLBACK (gtk_text_retrieve_surrounding_cb), self);
  g_signal_connect (priv->im_context, "delete-surrounding",
                    G_CALLBACK (gtk_text_delete_surrounding_cb), self);

  priv->drag_gesture = gtk_gesture_drag_new ();
  gtk_event_controller_set_static_name (GTK_EVENT_CONTROLLER (priv->drag_gesture), "gtk-text-drag-gesture");
  g_signal_connect (priv->drag_gesture, "drag-update",
                    G_CALLBACK (gtk_text_drag_gesture_update), self);
  g_signal_connect (priv->drag_gesture, "drag-end",
                    G_CALLBACK (gtk_text_drag_gesture_end), self);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->drag_gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (priv->drag_gesture), TRUE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (priv->drag_gesture));

  gesture = gtk_gesture_click_new ();
  gtk_event_controller_set_static_name (GTK_EVENT_CONTROLLER (gesture), "gtk-text-click-gesture");
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_text_click_gesture_pressed), self);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_text_click_gesture_released), self);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_static_name (controller, "gtk-text-motion-controller");
  g_signal_connect (controller, "motion",
                    G_CALLBACK (gtk_text_motion_controller_motion), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  priv->key_controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (priv->key_controller, GTK_PHASE_TARGET);
  gtk_event_controller_set_static_name (priv->key_controller, "gtk-text-key-controller");
  g_signal_connect (priv->key_controller, "key-pressed",
                    G_CALLBACK (gtk_text_key_controller_key_pressed), self);
  g_signal_connect_swapped (priv->key_controller, "im-update",
                            G_CALLBACK (gtk_text_schedule_im_reset), self);
  gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                           priv->im_context);
  gtk_widget_add_controller (GTK_WIDGET (self), priv->key_controller);

  priv->focus_controller = gtk_event_controller_focus_new ();
  gtk_event_controller_set_static_name (priv->focus_controller, "gtk-text-focus-controller");
  g_signal_connect (priv->focus_controller, "notify::is-focus",
                    G_CALLBACK (gtk_text_focus_changed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), priv->focus_controller);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  for (i = 0; i < 2; i++)
    {
      priv->undershoot_node[i] = gtk_css_node_new ();
      gtk_css_node_set_name (priv->undershoot_node[i], g_quark_from_static_string ("undershoot"));
      gtk_css_node_add_class (priv->undershoot_node[i], g_quark_from_static_string (i == 0 ? "left" : "right"));
      gtk_css_node_set_parent (priv->undershoot_node[i], widget_node);
      gtk_css_node_set_state (priv->undershoot_node[i], gtk_css_node_get_state (widget_node) & ~GTK_STATE_FLAG_DROP_ACTIVE);
      g_object_unref (priv->undershoot_node[i]);
    }

  set_text_cursor (GTK_WIDGET (self));
}

static void
gtk_text_dispose (GObject *object)
{
  GtkText *self = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkSeat *seat;
  GdkDevice *keyboard = NULL;
  GtkWidget *chooser;

  priv->current_pos = priv->selection_bound = 0;
  gtk_text_reset_im_context (self);
  gtk_text_reset_layout (self);

  if (priv->blink_tick)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (object), priv->blink_tick);
      priv->blink_tick = 0;
    }

  if (priv->magnifier)
    _gtk_magnifier_set_inspected (GTK_MAGNIFIER (priv->magnifier), NULL);

  if (priv->buffer)
    {
      buffer_disconnect_signals (self);
      g_object_unref (priv->buffer);
      priv->buffer = NULL;
    }

  g_clear_pointer (&priv->emoji_completion, gtk_widget_unparent);
  chooser = g_object_get_data (object, "gtk-emoji-chooser");
  if (chooser)
    gtk_widget_unparent (chooser);

  seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (object)));
  if (seat)
    keyboard = gdk_seat_get_keyboard (seat);
  if (keyboard)
    g_signal_handlers_disconnect_by_func (keyboard, direction_changed, self);

  g_clear_pointer (&priv->selection_bubble, gtk_widget_unparent);
  g_clear_pointer (&priv->popup_menu, gtk_widget_unparent);
  g_clear_pointer ((GtkWidget **) &priv->text_handles[TEXT_HANDLE_CURSOR], gtk_widget_unparent);
  g_clear_pointer ((GtkWidget **) &priv->text_handles[TEXT_HANDLE_SELECTION_BOUND], gtk_widget_unparent);
  g_clear_object (&priv->extra_menu);

  g_clear_pointer (&priv->magnifier_popover, gtk_widget_unparent);
  g_clear_pointer (&priv->placeholder, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_text_parent_class)->dispose (object);
}

static void
gtk_text_finalize (GObject *object)
{
  GtkText *self = GTK_TEXT (object);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_clear_object (&priv->selection_content);

  g_clear_object (&priv->history);
  g_clear_object (&priv->cached_layout);
  g_clear_object (&priv->im_context);
  g_free (priv->im_module);

  if (priv->tabs)
    pango_tab_array_free (priv->tabs);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);


  G_OBJECT_CLASS (gtk_text_parent_class)->finalize (object);
}

static void
gtk_text_ensure_magnifier (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->magnifier_popover)
    return;

  priv->magnifier = _gtk_magnifier_new (GTK_WIDGET (self));
  gtk_widget_set_size_request (priv->magnifier, 100, 60);
  _gtk_magnifier_set_magnification (GTK_MAGNIFIER (priv->magnifier), 2.0);
  priv->magnifier_popover = gtk_popover_new ();
  gtk_popover_set_position (GTK_POPOVER (priv->magnifier_popover), GTK_POS_TOP);
  gtk_widget_set_parent (priv->magnifier_popover, GTK_WIDGET (self));
  gtk_widget_add_css_class (priv->magnifier_popover, "magnifier");
  gtk_popover_set_autohide (GTK_POPOVER (priv->magnifier_popover), FALSE);
  gtk_popover_set_child (GTK_POPOVER (priv->magnifier_popover), priv->magnifier);
  gtk_widget_set_visible (priv->magnifier, TRUE);
}

static void
begin_change (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->change_count++;

  g_object_freeze_notify (G_OBJECT (self));
}

static void
end_change (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (priv->change_count > 0);

  g_object_thaw_notify (G_OBJECT (self));

  priv->change_count--;

  if (priv->change_count == 0)
    {
       if (priv->real_changed)
         {
           g_signal_emit_by_name (self, "changed");
           priv->real_changed = FALSE;
         }
    }
}

static void
emit_changed (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->change_count == 0)
    g_signal_emit_by_name (self, "changed");
  else
    priv->real_changed = TRUE;
}

static DisplayMode
gtk_text_get_display_mode (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->visible)
    return DISPLAY_NORMAL;

  if (priv->invisible_char == 0 && priv->invisible_char_set)
    return DISPLAY_BLANK;

  return DISPLAY_INVISIBLE;
}

char *
gtk_text_get_display_text (GtkText *self,
                           int      start_pos,
                           int      end_pos)
{
  GtkTextPasswordHint *password_hint;
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gunichar invisible_char;
  const char *start;
  const char *end;
  const char *text;
  char char_str[7];
  int char_len;
  GString *str;
  guint length;
  int i;

  text = gtk_entry_buffer_get_text (get_buffer (self));
  length = gtk_entry_buffer_get_length (get_buffer (self));

  if (end_pos < 0 || end_pos > length)
    end_pos = length;
  if (start_pos > length)
    start_pos = length;

  if (end_pos <= start_pos)
      return g_strdup ("");
  else if (priv->visible)
    {
      start = g_utf8_offset_to_pointer (text, start_pos);
      end = g_utf8_offset_to_pointer (start, end_pos - start_pos);
      return g_strndup (start, end - start);
    }
  else
    {
      str = g_string_sized_new (length * 2);

      /* Figure out what our invisible char is and encode it */
      if (!priv->invisible_char)
          invisible_char = priv->invisible_char_set ? ' ' : '*';
      else
          invisible_char = priv->invisible_char;
      char_len = g_unichar_to_utf8 (invisible_char, char_str);

      /*
       * Add hidden characters for each character in the text
       * buffer. If there is a password hint, then keep that
       * character visible.
       */

      password_hint = g_object_get_qdata (G_OBJECT (self), quark_password_hint);
      for (i = start_pos; i < end_pos; ++i)
        {
          if (password_hint && i == password_hint->position)
            {
              start = g_utf8_offset_to_pointer (text, i);
              g_string_append_len (str, start, g_utf8_next_char (start) - start);
            }
          else
            {
              g_string_append_len (str, char_str, char_len);
            }
        }

      return g_string_free (str, FALSE);
    }
}

static void
gtk_text_map (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->map (widget);

  gtk_text_recompute (self);
}

static void
gtk_text_unmap (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->text_handles_enabled = FALSE;
  gtk_text_update_handles (self);
  priv->cursor_alpha = 1.0;

  GTK_WIDGET_CLASS (gtk_text_parent_class)->unmap (widget);
}

static void
gtk_text_im_set_focus_in (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!priv->editable)
    return;

  gtk_text_schedule_im_reset (self);
  gtk_im_context_focus_in (priv->im_context);
}

static void
gtk_text_realize (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->realize (widget);

  gtk_im_context_set_client_widget (priv->im_context, widget);
  if (gtk_widget_is_focus (GTK_WIDGET (self)))
    gtk_text_im_set_focus_in (self);

  gtk_text_adjust_scroll (self);
  gtk_text_update_primary_selection (self);
}

static void
gtk_text_unrealize (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkClipboard *clipboard;

  gtk_text_reset_layout (self);

  gtk_im_context_set_client_widget (priv->im_context, NULL);

  clipboard = gtk_widget_get_primary_clipboard (widget);
  if (gdk_clipboard_get_content (clipboard) == priv->selection_content)
    gdk_clipboard_set_content (clipboard, NULL);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->unrealize (widget);
}

static void
update_im_cursor_location (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  const int text_width = gtk_widget_get_width (GTK_WIDGET (self));
  GdkRectangle area;
  int strong_x;
  int strong_xoffset;

  gtk_text_get_cursor_locations (self, &strong_x, NULL);

  strong_xoffset = strong_x - priv->scroll_offset;
  if (strong_xoffset < 0)
    strong_xoffset = 0;
  else if (strong_xoffset > text_width)
    strong_xoffset = text_width;

  area.x = strong_xoffset;
  area.y = 0;
  area.width = 0;
  area.height = gtk_widget_get_height (GTK_WIDGET (self));

  gtk_im_context_set_cursor_location (priv->im_context, &area);
}

static void
gtk_text_move_handle (GtkText       *self,
                      GtkTextHandle *handle,
                      int            x,
                      int            y,
                      int            height)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!gtk_text_handle_get_is_dragged (handle) &&
      (x < 0 || x > gtk_widget_get_width (GTK_WIDGET (self))))
    {
      /* Hide the handle if it's not being manipulated
       * and fell outside of the visible text area.
       */
      gtk_widget_set_visible (GTK_WIDGET (handle), FALSE);
    }
  else
    {
      GdkRectangle rect;

      rect.x = x;
      rect.y = y;
      rect.width = 1;
      rect.height = height;

      gtk_text_handle_set_position (handle, &rect);
      gtk_widget_set_direction (GTK_WIDGET (handle), priv->resolved_dir);
      gtk_widget_set_visible (GTK_WIDGET (handle), TRUE);
    }
}

static int
gtk_text_get_selection_bound_location (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoLayout *layout;
  PangoRectangle pos;
  int x;
  const char *text;
  int index;

  layout = gtk_text_ensure_layout (self, FALSE);
  text = pango_layout_get_text (layout);
  index = g_utf8_offset_to_pointer (text, priv->selection_bound) - text;
  pango_layout_index_to_pos (layout, index, &pos);

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    x = (pos.x + pos.width) / PANGO_SCALE;
  else
    x = pos.x / PANGO_SCALE;

  return x;
}

static void
gtk_text_update_handles (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  const int text_height = gtk_widget_get_height (GTK_WIDGET (self));
  int strong_x;
  int cursor, bound;

  if (!priv->text_handles_enabled)
    {
      if (priv->text_handles[TEXT_HANDLE_CURSOR])
        gtk_widget_set_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_CURSOR]), FALSE);
      if (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])
        gtk_widget_set_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND]), FALSE);
    }
  else
    {
      gtk_text_ensure_text_handles (self);
      gtk_text_get_cursor_locations (self, &strong_x, NULL);
      cursor = strong_x - priv->scroll_offset;

      if (priv->selection_bound != priv->current_pos)
        {
          int start, end;

          bound = gtk_text_get_selection_bound_location (self) - priv->scroll_offset;

          if (priv->selection_bound > priv->current_pos)
            {
              start = cursor;
              end = bound;
            }
          else
            {
              start = bound;
              end = cursor;
            }

          /* Update start selection bound */
          gtk_text_handle_set_role (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND],
                                    GTK_TEXT_HANDLE_ROLE_SELECTION_END);
          gtk_text_move_handle (self,
                                priv->text_handles[TEXT_HANDLE_SELECTION_BOUND],
                                end, 0, text_height);
          gtk_text_handle_set_role (priv->text_handles[TEXT_HANDLE_CURSOR],
                                    GTK_TEXT_HANDLE_ROLE_SELECTION_START);
          gtk_text_move_handle (self,
                                priv->text_handles[TEXT_HANDLE_CURSOR],
                                start, 0, text_height);
        }
      else
        {
          gtk_widget_set_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND]), FALSE);
          gtk_text_handle_set_role (priv->text_handles[TEXT_HANDLE_CURSOR],
                                    GTK_TEXT_HANDLE_ROLE_CURSOR);
          gtk_text_move_handle (self,
                                priv->text_handles[TEXT_HANDLE_CURSOR],
                                cursor, 0, text_height);
        }
    }
}

static void
gtk_text_measure (GtkWidget      *widget,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoContext *context;
  PangoFontMetrics *metrics;

  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context, NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int min, nat;
      int char_width;
      int digit_width;
      int char_pixels;

      char_width = pango_font_metrics_get_approximate_char_width (metrics);
      digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
      char_pixels = (MAX (char_width, digit_width) + PANGO_SCALE - 1) / PANGO_SCALE;

      if (priv->width_chars >= 0)
        min = char_pixels * priv->width_chars;
      else
        min = 0;

      if (priv->max_width_chars < 0)
        nat = NAT_ENTRY_WIDTH;
      else
        nat = char_pixels * priv->max_width_chars;

      if (priv->propagate_text_width)
        {
          PangoLayout *layout;
          int act;

          layout = gtk_text_ensure_layout (self, TRUE);
          pango_layout_get_pixel_size (layout, &act, NULL);

          nat = MIN (act, nat);
        }

      nat = MAX (min, nat);

      if (priv->placeholder)
        {
          int pmin, pnat;

          gtk_widget_measure (priv->placeholder, GTK_ORIENTATION_HORIZONTAL, -1,
                              &pmin, &pnat, NULL, NULL);
          min = MAX (min, pmin);
          nat = MAX (nat, pnat);
        }

      *minimum = min;
      *natural = nat;
    }
  else
    {
      int height, baseline;
      PangoLayout *layout;

      layout = gtk_text_ensure_layout (self, TRUE);

      priv->ascent = pango_font_metrics_get_ascent (metrics);
      priv->descent = pango_font_metrics_get_descent (metrics);

      pango_layout_get_pixel_size (layout, NULL, &height);

      height = MAX (height, PANGO_PIXELS (priv->ascent + priv->descent));

      baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

      *minimum = *natural = height;

      if (priv->placeholder)
        {
          int min, nat;

          gtk_widget_measure (priv->placeholder, GTK_ORIENTATION_VERTICAL, -1,
                              &min, &nat, NULL, NULL);
          *minimum = MAX (*minimum, min);
          *natural = MAX (*natural, nat);
        }

      if (minimum_baseline)
        *minimum_baseline = baseline;
      if (natural_baseline)
        *natural_baseline = baseline;
    }

  pango_font_metrics_unref (metrics);
}

static void
gtk_text_size_allocate (GtkWidget *widget,
                        int        width,
                        int        height,
                        int        baseline)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkEmojiChooser *chooser;

  priv->text_baseline = baseline;

  if (priv->placeholder)
    {
      gtk_widget_size_allocate (priv->placeholder,
                                &(GtkAllocation) { 0, 0, width, height },
                                -1);
    }

  gtk_text_adjust_scroll (self);
  gtk_text_check_cursor_blink (self);
  update_im_cursor_location (self);

  chooser = g_object_get_data (G_OBJECT (self), "gtk-emoji-chooser");
  if (chooser)
    gtk_popover_present (GTK_POPOVER (chooser));

  gtk_text_update_handles (self);

  if (priv->emoji_completion)
    gtk_popover_present (GTK_POPOVER (priv->emoji_completion));

  if (priv->magnifier_popover)
    gtk_popover_present (GTK_POPOVER (priv->magnifier_popover));

  if (priv->popup_menu)
    gtk_popover_present (GTK_POPOVER (priv->popup_menu));

  if (priv->selection_bubble)
    gtk_popover_present (GTK_POPOVER (priv->selection_bubble));

  if (priv->text_handles[TEXT_HANDLE_CURSOR])
    gtk_text_handle_present (priv->text_handles[TEXT_HANDLE_CURSOR]);

  if (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])
    gtk_text_handle_present (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND]);
}

static void
gtk_text_draw_undershoot (GtkText     *self,
                          GtkSnapshot *snapshot)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  const int text_width = gtk_widget_get_width (GTK_WIDGET (self));
  const int text_height = gtk_widget_get_height (GTK_WIDGET (self));
  GtkCssStyle *style;
  int min_offset, max_offset;
  GtkCssBoxes boxes;

  gtk_text_get_scroll_limits (self, &min_offset, &max_offset);

  if (priv->scroll_offset > min_offset)
    {
      style = gtk_css_node_get_style (priv->undershoot_node[0]);
      gtk_css_boxes_init_border_box (&boxes, style, 0, 0, UNDERSHOOT_SIZE, text_height);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }

  if (priv->scroll_offset < max_offset)
    {
      style = gtk_css_node_get_style (priv->undershoot_node[1]);
      gtk_css_boxes_init_border_box (&boxes, style,
                                     text_width - UNDERSHOOT_SIZE, 0, UNDERSHOOT_SIZE, text_height);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_border (&boxes, snapshot);
    }
}

static void
gtk_text_snapshot (GtkWidget   *widget,
                   GtkSnapshot *snapshot)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  /* Draw text and cursor */
  if (priv->dnd_position != -1)
    gtk_text_draw_cursor (self, snapshot, CURSOR_DND);

  if (priv->placeholder)
    gtk_widget_snapshot_child (widget, priv->placeholder, snapshot);

  gtk_text_draw_text (self, snapshot);

  /* When no text is being displayed at all, don't show the cursor */
  if (gtk_text_get_display_mode (self) != DISPLAY_BLANK &&
      gtk_widget_has_focus (widget) &&
      priv->selection_bound == priv->current_pos)
    {
      gtk_snapshot_push_opacity (snapshot, priv->cursor_alpha);
      gtk_text_draw_cursor (self, snapshot, CURSOR_STANDARD);
      gtk_snapshot_pop (snapshot);
    }

  gtk_text_draw_undershoot (self, snapshot);
}

static void
gtk_text_get_pixel_ranges (GtkText  *self,
                           int     **ranges,
                           int      *n_ranges)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bound != priv->current_pos)
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, TRUE);
      PangoLayoutLine *line = pango_layout_get_lines_readonly (layout)->data;
      const char *text = pango_layout_get_text (layout);
      int start_index = g_utf8_offset_to_pointer (text, priv->selection_bound) - text;
      int end_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;
      int real_n_ranges, i;

      pango_layout_line_get_x_ranges (line,
                                      MIN (start_index, end_index),
                                      MAX (start_index, end_index),
                                      ranges,
                                      &real_n_ranges);

      if (ranges)
        {
          int *r = *ranges;

          for (i = 0; i < real_n_ranges; ++i)
            {
              r[2 * i + 1] = (r[2 * i + 1] - r[2 * i]) / PANGO_SCALE;
              r[2 * i] = r[2 * i] / PANGO_SCALE;
            }
        }

      if (n_ranges)
        *n_ranges = real_n_ranges;
    }
  else
    {
      if (n_ranges)
        *n_ranges = 0;
      if (ranges)
        *ranges = NULL;
    }
}

static gboolean
in_selection (GtkText *self,
              int      x)
{
  int *ranges;
  int n_ranges, i;
  int retval = FALSE;

  gtk_text_get_pixel_ranges (self, &ranges, &n_ranges);

  for (i = 0; i < n_ranges; ++i)
    {
      if (x >= ranges[2 * i] && x < ranges[2 * i] + ranges[2 * i + 1])
        {
          retval = TRUE;
          break;
        }
    }

  g_free (ranges);
  return retval;
}

static int
gesture_get_current_point_in_layout (GtkGestureSingle *gesture,
                                     GtkText          *self)
{
  int tx;
  GdkEventSequence *sequence;
  double px;

  sequence = gtk_gesture_single_get_current_sequence (gesture);
  gtk_gesture_get_point (GTK_GESTURE (gesture), sequence, &px, NULL);
  gtk_text_get_layout_offsets (self, &tx, NULL);

  return px - tx;
}

static void
gtk_text_do_popup (GtkText *self,
                   double   x,
                   double   y)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  gtk_text_update_clipboard_actions (self);
  gtk_text_update_emoji_action (self);

  if (!priv->popup_menu)
    {
      GMenuModel *model;

      model = gtk_text_get_menu_model (self);
      priv->popup_menu = gtk_popover_menu_new_from_model (model);
      gtk_widget_set_parent (priv->popup_menu, GTK_WIDGET (self));
      gtk_popover_set_position (GTK_POPOVER (priv->popup_menu), GTK_POS_BOTTOM);

      gtk_popover_set_has_arrow (GTK_POPOVER (priv->popup_menu), FALSE);
      gtk_widget_set_halign (priv->popup_menu, GTK_ALIGN_START);

      gtk_accessible_update_property (GTK_ACCESSIBLE (priv->popup_menu),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, _("Context menu"),
                                      -1);

      g_object_unref (model);
    }

  if (x != -1 && y != -1)
    {
      GdkRectangle rect = { x, y, 1, 1 };
      gtk_popover_set_pointing_to (GTK_POPOVER (priv->popup_menu), &rect);
    }
  else
    gtk_popover_set_pointing_to (GTK_POPOVER (priv->popup_menu), NULL);

  gtk_popover_popup (GTK_POPOVER (priv->popup_menu));
}

static void
gtk_text_click_gesture_pressed (GtkGestureClick *gesture,
                                int              n_press,
                                double           widget_x,
                                double           widget_y,
                                GtkText         *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkEventSequence *current;
  GdkEvent *event;
  int x, y, sel_start, sel_end;
  guint button;
  int tmp_pos;

  if (!gtk_widget_has_focus (widget))
    {
      if (!gtk_widget_get_focus_on_click (widget))
        return;
      priv->in_click = TRUE;
      gtk_widget_grab_focus (widget);
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      priv->in_click = FALSE;
    }

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), current);

  x = gesture_get_current_point_in_layout (GTK_GESTURE_SINGLE (gesture), self);
  y = widget_y;
  gtk_text_reset_blink_time (self);

  tmp_pos = gtk_text_find_position (self, x);

  if (gdk_event_triggers_context_menu (event))
    {
      gtk_text_do_popup (self, widget_x, widget_y);
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
  else if (n_press == 1 && button == GDK_BUTTON_MIDDLE &&
           get_middle_click_paste (self))
    {
      if (priv->editable)
        {
          priv->insert_pos = tmp_pos;
          gtk_text_paste (self, gtk_widget_get_primary_clipboard (widget));
        }
      else
        {
          gtk_widget_error_bell (widget);
        }

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
  else if (button == GDK_BUTTON_PRIMARY)
    {
      gboolean have_selection;
      gboolean is_touchscreen, extend_selection;
      GdkDevice *source;
      guint state;

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

      sel_start = priv->selection_bound;
      sel_end = priv->current_pos;
      have_selection = sel_start != sel_end;

      source = gdk_event_get_device (event);
      is_touchscreen = gdk_device_get_source (source) == GDK_SOURCE_TOUCHSCREEN;

      priv->text_handles_enabled = is_touchscreen;

      priv->in_drag = FALSE;
      priv->select_words = FALSE;
      priv->select_lines = FALSE;

      state = gdk_event_get_modifier_state (event);

      extend_selection = (state & GDK_SHIFT_MASK) != 0;

      /* Always emit reset when preedit is shown */
      priv->need_im_reset = TRUE;
      gtk_text_reset_im_context (self);

      switch (n_press)
        {
        case 1:
          if (in_selection (self, x))
            {
              if (is_touchscreen)
                {
                  if (priv->selection_bubble &&
                      gtk_widget_get_visible (priv->selection_bubble))
                    gtk_text_selection_bubble_popup_unset (self);
                  else
                    gtk_text_selection_bubble_popup_set (self);
                }
              else if (extend_selection)
                {
                  /* Truncate current selection, but keep it as big as possible */
                  if (tmp_pos - sel_start > sel_end - tmp_pos)
                    gtk_text_set_positions (self, sel_start, tmp_pos);
                  else
                    gtk_text_set_positions (self, tmp_pos, sel_end);

                  /* all done, so skip the extend_to_left stuff later */
                  extend_selection = FALSE;
                }
              else
                {
                  /* We'll either start a drag, or clear the selection */
                  priv->in_drag = TRUE;
                  priv->drag_start_x = x;
                  priv->drag_start_y = y;
                }
            }
          else
            {
              gtk_text_selection_bubble_popup_unset (self);

              if (!extend_selection)
                {
                  gtk_text_set_selection_bounds (self, tmp_pos, tmp_pos);
                  priv->handle_place_time = g_get_monotonic_time ();
                }
              else
                {
                  /* select from the current position to the clicked position */
                  if (!have_selection)
                    sel_start = sel_end = priv->current_pos;

                  gtk_text_set_positions (self, tmp_pos, tmp_pos);
                }
            }

          break;

        case 2:
          priv->select_words = TRUE;
          gtk_text_select_word (self);
          break;

        case 3:
          priv->select_lines = TRUE;
          gtk_text_select_line (self);
          break;

        default:
          break;
        }

      if (extend_selection)
        {
          gboolean extend_to_left;
          int start, end;

          start = MIN (priv->current_pos, priv->selection_bound);
          start = MIN (sel_start, start);

          end = MAX (priv->current_pos, priv->selection_bound);
          end = MAX (sel_end, end);

          if (tmp_pos == sel_start || tmp_pos == sel_end)
            extend_to_left = (tmp_pos == start);
          else
            extend_to_left = (end == sel_end);

          if (extend_to_left)
            gtk_text_set_positions (self, start, end);
          else
            gtk_text_set_positions (self, end, start);
        }

      gtk_text_update_handles (self);
    }

  if (n_press >= 3)
    gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_text_click_gesture_released (GtkGestureClick *gesture,
                                 int              n_press,
                                 double           widget_x,
                                 double           widget_y,
                                 GtkText         *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkEvent *event =
    gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (gesture));

  if (n_press == 1 &&
      !priv->in_drag &&
      priv->current_pos == priv->selection_bound)
    gtk_im_context_activate_osk (priv->im_context, event);
}

static char *
_gtk_text_get_selected_text (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bound != priv->current_pos)
    {
      const int start = MIN (priv->selection_bound, priv->current_pos);
      const int end = MAX (priv->selection_bound, priv->current_pos);
      const char *text = gtk_entry_buffer_get_text (get_buffer (self));
      const int start_index = g_utf8_offset_to_pointer (text, start) - text;
      const int end_index = g_utf8_offset_to_pointer (text, end) - text;

      return g_strndup (text + start_index, end_index - start_index);
    }

  return NULL;
}

static void
gtk_text_show_magnifier (GtkText *self,
                         int      x,
                         int      y)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  const int text_height = gtk_widget_get_height (GTK_WIDGET (self));
  cairo_rectangle_int_t rect;

  gtk_text_ensure_magnifier (self);

  rect.x = x;
  rect.width = 1;
  rect.y = 0;
  rect.height = text_height;

  _gtk_magnifier_set_coords (GTK_MAGNIFIER (priv->magnifier), rect.x,
                             rect.y + rect.height / 2);
  gtk_popover_set_pointing_to (GTK_POPOVER (priv->magnifier_popover),
                               &rect);
  gtk_popover_popup (GTK_POPOVER (priv->magnifier_popover));
}

static void
gtk_text_motion_controller_motion (GtkEventControllerMotion *controller,
                                   double                    x,
                                   double                    y,
                                   GtkText                  *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkDevice *device;

  device = gtk_event_controller_get_current_event_device (GTK_EVENT_CONTROLLER (controller));

  if (priv->mouse_cursor_obscured &&
      gdk_device_get_timestamp (device) != priv->obscured_cursor_timestamp)
    {
      set_text_cursor (GTK_WIDGET (self));
      priv->mouse_cursor_obscured = FALSE;
    }
}

static void
dnd_finished_cb (GdkDrag *drag,
                 GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (gdk_drag_get_selected_action (drag) == GDK_ACTION_MOVE)
    gtk_text_delete_selection (self);

  priv->drag = NULL;
}

static void
dnd_cancel_cb (GdkDrag             *drag,
               GdkDragCancelReason  reason,
               GtkText             *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->drag = NULL;
}

static void
gtk_text_drag_gesture_update (GtkGestureDrag *gesture,
                              double          offset_x,
                              double          offset_y,
                              GtkText        *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkEventSequence *sequence;
  GdkEvent *event;
  int x, y;
  double start_y;

  gtk_text_selection_bubble_popup_unset (self);

  x = gesture_get_current_point_in_layout (GTK_GESTURE_SINGLE (gesture), self);
  gtk_gesture_drag_get_start_point (gesture, NULL, &start_y);
  y = start_y + offset_y;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  if (priv->mouse_cursor_obscured)
    {
      set_text_cursor (widget);
      priv->mouse_cursor_obscured = FALSE;
    }

  if (priv->select_lines)
    return;

  if (priv->in_drag)
    {
      if (gtk_text_get_display_mode (self) == DISPLAY_NORMAL &&
          gtk_drag_check_threshold_double (widget, 0, 0, offset_x, offset_y))
        {
          int *ranges;
          int n_ranges;
          char *text;
          GdkDragAction actions;
          GdkDrag *drag;
          GdkPaintable *paintable;
          GdkContentProvider *content;

          text = _gtk_text_get_selected_text (self);
          gtk_text_get_pixel_ranges (self, &ranges, &n_ranges);

          g_assert (n_ranges > 0);

          if (priv->editable)
            actions = GDK_ACTION_COPY|GDK_ACTION_MOVE;
          else
            actions = GDK_ACTION_COPY;

          content = gdk_content_provider_new_typed (G_TYPE_STRING, text);

          drag = gdk_drag_begin (gdk_event_get_surface ((GdkEvent*) event),
                                 gdk_event_get_device ((GdkEvent*) event),
                                 content,
                                 actions,
                                 priv->drag_start_x,
                                 priv->drag_start_y);
          g_object_unref (content);

          g_signal_connect (drag, "dnd-finished", G_CALLBACK (dnd_finished_cb), self);
          g_signal_connect (drag, "cancel", G_CALLBACK (dnd_cancel_cb), self);

          paintable = gtk_text_util_create_drag_icon (widget, text, -1);
          gtk_drag_icon_set_from_paintable (drag, paintable,
                                            (priv->drag_start_x - ranges[0]),
                                            priv->drag_start_y);
          g_clear_object (&paintable);

          priv->drag = drag;

          g_object_unref (drag);

          g_free (ranges);
          g_free (text);

          priv->in_drag = FALSE;

          /* Deny the gesture so we don't get further updates */
          gtk_gesture_set_state (priv->drag_gesture, GTK_EVENT_SEQUENCE_DENIED);
        }
    }
  else
    {
      GdkInputSource input_source;
      GdkDevice *source;
      guint length;
      int tmp_pos;
      int pos, bound;

      length = gtk_entry_buffer_get_length (get_buffer (self));

      if (y < 0)
        tmp_pos = 0;
      else if (y >= gtk_widget_get_height (GTK_WIDGET (self)))
        tmp_pos = length;
      else
        tmp_pos = gtk_text_find_position (self, x);

      source = gdk_event_get_device (event);
      input_source = gdk_device_get_source (source);

      if (priv->select_words)
        {
          int min, max;
          int old_min, old_max;

          min = gtk_text_move_backward_word (self, tmp_pos, TRUE);
          max = gtk_text_move_forward_word (self, tmp_pos, TRUE);

          pos = priv->current_pos;
          bound = priv->selection_bound;

          old_min = MIN (priv->current_pos, priv->selection_bound);
          old_max = MAX (priv->current_pos, priv->selection_bound);

          if (min < old_min)
            {
              pos = min;
              bound = old_max;
            }
          else if (old_max < max)
            {
              pos = max;
              bound = old_min;
            }
          else if (pos == old_min)
            {
              if (priv->current_pos != min)
                pos = max;
            }
          else
            {
              if (priv->current_pos != max)
                pos = min;
            }
        }
      else
        {
          pos = tmp_pos;
          bound = -1;
        }

      if (pos != priv->current_pos)
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

      gtk_text_set_positions (self, pos, bound);

      /* Update touch handles' position */
      if (input_source == GDK_SOURCE_TOUCHSCREEN)
        {
          priv->text_handles_enabled = TRUE;
          gtk_text_update_handles (self);
          gtk_text_show_magnifier (self, x - priv->scroll_offset, y);
        }
    }
}

static void
gtk_text_drag_gesture_end (GtkGestureDrag *gesture,
                           double          offset_x,
                           double          offset_y,
                           GtkText        *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gboolean in_drag;
  GdkEventSequence *sequence;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  in_drag = priv->in_drag;
  priv->in_drag = FALSE;

  if (priv->magnifier_popover)
    gtk_popover_popdown (GTK_POPOVER (priv->magnifier_popover));

  /* Check whether the drag was cancelled rather than finished */
  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    return;

  if (in_drag)
    {
      int tmp_pos = gtk_text_find_position (self, priv->drag_start_x);

      gtk_text_set_selection_bounds (self, tmp_pos, tmp_pos);
    }

  gtk_text_update_handles (self);

  gtk_text_update_primary_selection (self);
}

static void
gtk_text_obscure_mouse_cursor (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;

  if (priv->mouse_cursor_obscured)
    return;

  gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "none");

  display = gtk_widget_get_display (GTK_WIDGET (self));
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);

  priv->obscured_cursor_timestamp = gdk_device_get_timestamp (device);
  priv->mouse_cursor_obscured = TRUE;
}

static gboolean
gtk_text_key_controller_key_pressed (GtkEventControllerKey *controller,
                                      guint                  keyval,
                                      guint                  keycode,
                                      GdkModifierType        state,
                                      GtkText               *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gunichar unichar;

  gtk_text_reset_blink_time (self);
  gtk_text_pend_cursor_blink (self);

  gtk_text_selection_bubble_popup_unset (self);

  priv->text_handles_enabled = FALSE;
  gtk_text_update_handles (self);

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter ||
      keyval == GDK_KEY_Escape)
    gtk_text_reset_im_context (self);

  unichar = gdk_keyval_to_unicode (keyval);

  if (!priv->editable && unichar != 0)
    gtk_widget_error_bell (GTK_WIDGET (self));

  gtk_text_obscure_mouse_cursor (self);

  return FALSE;
}

static void
gtk_text_focus_changed (GtkEventControllerFocus *controller,
                        GParamSpec              *pspec,
                        GtkWidget               *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkSeat *seat = NULL;
  GdkDevice *keyboard = NULL;

  seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
  if (seat)
    keyboard = gdk_seat_get_keyboard (seat);

  gtk_widget_queue_draw (widget);

  if (gtk_event_controller_focus_is_focus (controller))
    {
      if (keyboard)
        g_signal_connect (keyboard, "notify::direction",
                          G_CALLBACK (direction_changed), self);

      gtk_text_im_set_focus_in (self);
      gtk_text_reset_blink_time (self);
      gtk_text_check_cursor_blink (self);
      gtk_text_update_primary_selection (self);
    }
  else /* Focus out */
    {
      gtk_text_selection_bubble_popup_unset (self);

      priv->text_handles_enabled = FALSE;
      gtk_text_update_handles (self);

      if (keyboard)
        g_signal_handlers_disconnect_by_func (keyboard, direction_changed, self);

      if (priv->editable)
        {
          gtk_text_schedule_im_reset (self);
          gtk_im_context_focus_out (priv->im_context);
        }

      if (priv->blink_tick)
        remove_blink_timeout (self);
    }
}

static gboolean
gtk_text_grab_focus (GtkWidget *widget)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gboolean select_on_focus;
  GtkWidget *prev_focus;
  gboolean prev_focus_was_child;

  prev_focus = gtk_root_get_focus (gtk_widget_get_root (widget));
  prev_focus_was_child = prev_focus && gtk_widget_is_ancestor (prev_focus, widget);

  if (!GTK_WIDGET_CLASS (gtk_text_parent_class)->grab_focus (GTK_WIDGET (self)))
    return FALSE;

  if (priv->editable && !priv->in_click && !prev_focus_was_child)
    {
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-entry-select-on-focus",
                    &select_on_focus,
                    NULL);

      if (select_on_focus)
        gtk_text_set_selection_bounds (self, 0, -1);
    }

  return TRUE;
}

/**
 * gtk_text_grab_focus_without_selecting:
 * @self: a `GtkText`
 *
 * Causes @self to have keyboard focus.
 *
 * It behaves like [method@Gtk.Widget.grab_focus],
 * except that it doesn't select the contents of @self.
 * You only want to call this on some special entries
 * which the user usually doesn't want to replace all text in,
 * such as search-as-you-type entries.
 *
 * Returns: %TRUE if focus is now inside @self
 */
gboolean
gtk_text_grab_focus_without_selecting (GtkText *self)
{
  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return GTK_WIDGET_CLASS (gtk_text_parent_class)->grab_focus (GTK_WIDGET (self));
}

static void
gtk_text_direction_changed (GtkWidget        *widget,
                            GtkTextDirection  previous_dir)
{
  GtkText *self = GTK_TEXT (widget);

  gtk_text_recompute (self);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_text_state_flags_changed (GtkWidget     *widget,
                              GtkStateFlags  previous_state)
{
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (GTK_WIDGET (self));

  if (gtk_widget_get_realized (widget))
    {
      set_text_cursor (widget);
      priv->mouse_cursor_obscured = FALSE;
    }

  if (!gtk_widget_is_sensitive (widget))
    {
      /* Clear any selection */
      gtk_text_set_selection_bounds (self, priv->current_pos, priv->current_pos);
    }

  state &= ~GTK_STATE_FLAG_DROP_ACTIVE;
  if (priv->selection_node)
    gtk_css_node_set_state (priv->selection_node, state);

  if (priv->block_cursor_node)
    gtk_css_node_set_state (priv->block_cursor_node, state);

  gtk_css_node_set_state (priv->undershoot_node[0], state);
  gtk_css_node_set_state (priv->undershoot_node[1], state);

  gtk_text_update_cached_style_values (self);

  gtk_widget_queue_draw (widget);
}

/* GtkEditable method implementations
 */
static void
gtk_text_insert_text (GtkText    *self,
                      const char *text,
                      int         length,
                      int        *position)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int n_inserted;
  int n_chars;

  if (length == 0)
    return;

  n_chars = g_utf8_strlen (text, length);

  /*
   * The incoming text may a password or other secret. We make sure
   * not to copy it into temporary buffers.
   */
  if (priv->change_count == 0)
    gtk_text_history_begin_irreversible_action (priv->history);
  begin_change (self);

  n_inserted = gtk_entry_buffer_insert_text (get_buffer (self), *position, text, n_chars);

  end_change (self);
  if (priv->change_count == 0)
    gtk_text_history_end_irreversible_action (priv->history);

  if (n_inserted != n_chars)
    gtk_widget_error_bell (GTK_WIDGET (self));

  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                       *position,
                                       *position + n_inserted);

  *position += n_inserted;

  update_placeholder_visibility (self);
  if (priv->propagate_text_width)
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_text_delete_text (GtkText *self,
                      int      start_pos,
                      int      end_pos)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (end_pos < 0)
    end_pos = gtk_entry_buffer_get_length (get_buffer (self));

  if (start_pos == end_pos)
    return;

  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE,
                                       start_pos,
                                       end_pos);

  if (priv->change_count == 0)
    gtk_text_history_begin_irreversible_action (priv->history);
  begin_change (self);

  gtk_entry_buffer_delete_text (get_buffer (self), start_pos, end_pos - start_pos);

  end_change (self);
  if (priv->change_count == 0)
    gtk_text_history_end_irreversible_action (priv->history);

  update_placeholder_visibility (self);
  if (priv->propagate_text_width)
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gtk_text_delete_selection (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  int start_pos = MIN (priv->selection_bound, priv->current_pos);
  int end_pos = MAX (priv->selection_bound, priv->current_pos);

  gtk_editable_delete_text (GTK_EDITABLE (self), start_pos, end_pos);
  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE,
                                       start_pos, end_pos);
}

static void
gtk_text_set_selection_bounds (GtkText *self,
                               int      start,
                               int      end)
{
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (self));
  if (start < 0)
    start = length;
  if (end < 0)
    end = length;

  gtk_text_reset_im_context (self);

  gtk_text_set_positions (self, MIN (end, length), MIN (start, length));

  gtk_text_update_primary_selection (self);

  gtk_accessible_text_update_caret_position (GTK_ACCESSIBLE_TEXT (self));
  gtk_accessible_text_update_selection_bound (GTK_ACCESSIBLE_TEXT (self));
}

static gboolean
gtk_text_get_selection_bounds (GtkText *self,
                               int     *start,
                               int     *end)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  *start = priv->selection_bound;
  *end = priv->current_pos;

  return (priv->selection_bound != priv->current_pos);
}

static gunichar
find_invisible_char (GtkWidget *widget)
{
  PangoLayout *layout;
  PangoAttrList *attr_list;
  int i;
  gunichar invisible_chars [] = {
    0x25cf, /* BLACK CIRCLE */
    0x2022, /* BULLET */
    0x2731, /* HEAVY ASTERISK */
    0x273a  /* SIXTEEN POINTED ASTERISK */
  };

  layout = gtk_widget_create_pango_layout (widget, NULL);

  attr_list = pango_attr_list_new ();
  pango_attr_list_insert (attr_list, pango_attr_fallback_new (FALSE));

  pango_layout_set_attributes (layout, attr_list);
  pango_attr_list_unref (attr_list);

  for (i = 0; i < G_N_ELEMENTS (invisible_chars); i++)
    {
      char text[7] = { 0, };
      int len, count;

      len = g_unichar_to_utf8 (invisible_chars[i], text);
      pango_layout_set_text (layout, text, len);

      count = pango_layout_get_unknown_glyphs_count (layout);

      if (count == 0)
        {
          g_object_unref (layout);
          return invisible_chars[i];
        }
    }

  g_object_unref (layout);

  return '*';
}

static void
gtk_text_update_cached_style_values (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!priv->visible && !priv->invisible_char_set)
    {
      gunichar ch = find_invisible_char (GTK_WIDGET (self));

      if (priv->invisible_char != ch)
        {
          priv->invisible_char = ch;
          g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR]);
        }
    }
}

static void
gtk_text_css_changed (GtkWidget         *widget,
                      GtkCssStyleChange *change)
{
  GtkText *self = GTK_TEXT (widget);

  GTK_WIDGET_CLASS (gtk_text_parent_class)->css_changed (widget, change);

  gtk_text_update_cached_style_values (self);

  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT |
                                            GTK_CSS_AFFECTS_BACKGROUND |
                                            GTK_CSS_AFFECTS_CONTENT))
    gtk_widget_queue_draw (widget);

  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_ATTRS))
    gtk_text_recompute (self);
}

static void
gtk_text_password_hint_free (GtkTextPasswordHint *password_hint)
{
  if (password_hint->source_id)
    g_source_remove (password_hint->source_id);

  g_free (password_hint);
}


static gboolean
gtk_text_remove_password_hint (gpointer data)
{
  GtkTextPasswordHint *password_hint = g_object_get_qdata (data, quark_password_hint);
  password_hint->position = -1;
  password_hint->source_id = 0;

  /* Force the string to be redrawn, but now without a visible character */
  gtk_text_recompute (GTK_TEXT (data));

  return G_SOURCE_REMOVE;
}

static void
update_placeholder_visibility (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->placeholder)
    gtk_widget_set_child_visible (priv->placeholder,
                                  priv->preedit_length == 0 &&
                                  (priv->buffer == NULL ||
                                   gtk_entry_buffer_get_length (priv->buffer) == 0));
}

/* GtkEntryBuffer signal handlers
 */
static void
buffer_inserted_text (GtkEntryBuffer *buffer,
                      guint           position,
                      const char     *chars,
                      guint           n_chars,
                      GtkText        *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  guint password_hint_timeout;
  guint current_pos;
  int selection_bound;

  current_pos = priv->current_pos;
  if (current_pos > position)
    current_pos += n_chars;

  selection_bound = priv->selection_bound;
  if (selection_bound > position)
    selection_bound += n_chars;

  gtk_text_set_positions (self, current_pos, selection_bound);
  gtk_text_recompute (self);

  gtk_text_history_text_inserted (priv->history, position, chars, -1);

  /* Calculate the password hint if it needs to be displayed. */
  if (n_chars == 1 && !priv->visible)
    {
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                    "gtk-entry-password-hint-timeout", &password_hint_timeout,
                    NULL);

      if (password_hint_timeout > 0)
        {
          GtkTextPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (self),
                                                                    quark_password_hint);
          if (!password_hint)
            {
              password_hint = g_new0 (GtkTextPasswordHint, 1);
              g_object_set_qdata_full (G_OBJECT (self), quark_password_hint, password_hint,
                                       (GDestroyNotify)gtk_text_password_hint_free);
            }

          password_hint->position = position;
          if (password_hint->source_id)
            g_source_remove (password_hint->source_id);
          password_hint->source_id = g_timeout_add (password_hint_timeout,
                                                    (GSourceFunc)gtk_text_remove_password_hint,
                                                    self);
          gdk_source_set_static_name_by_id (password_hint->source_id, "[gtk] gtk_text_remove_password_hint");
        }
    }
}

static void
buffer_deleted_text (GtkEntryBuffer *buffer,
                     guint           position,
                     guint           n_chars,
                     GtkText        *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  guint end_pos = position + n_chars;

  if (gtk_text_history_get_enabled (priv->history))
    {
      char *deleted_text;

      deleted_text = gtk_editable_get_chars (GTK_EDITABLE (self),
                                             position,
                                             end_pos);
      gtk_text_history_selection_changed (priv->history,
                                          priv->current_pos,
                                          priv->selection_bound);
      gtk_text_history_text_deleted (priv->history,
                                     position,
                                     end_pos,
                                     deleted_text,
                                     -1);

      g_free (deleted_text);
    }
}

static void
buffer_deleted_text_after (GtkEntryBuffer *buffer,
                           guint           position,
                           guint           n_chars,
                           GtkText        *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  guint end_pos = position + n_chars;
  int selection_bound;
  guint current_pos;

  current_pos = priv->current_pos;
  if (current_pos > position)
    current_pos -= MIN (current_pos, end_pos) - position;

  selection_bound = priv->selection_bound;
  if (selection_bound > position)
    selection_bound -= MIN (selection_bound, end_pos) - position;

  gtk_text_set_positions (self, current_pos, selection_bound);
  gtk_text_recompute (self);

  /* We might have deleted the selection */
  gtk_text_update_primary_selection (self);

  /* Disable the password hint if one exists. */
  if (!priv->visible)
    {
      GtkTextPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (self),
                                                                quark_password_hint);
      if (password_hint)
        {
          if (password_hint->source_id)
            g_source_remove (password_hint->source_id);
          password_hint->source_id = 0;
          password_hint->position = -1;
        }
    }
}

static void
buffer_notify_text (GtkEntryBuffer *buffer,
                    GParamSpec     *spec,
                    GtkText        *self)
{
  emit_changed (self);
  update_placeholder_visibility (self);
  g_object_notify (G_OBJECT (self), "text");
}

static void
buffer_notify_max_length (GtkEntryBuffer *buffer,
                          GParamSpec     *spec,
                          GtkText        *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_MAX_LENGTH]);
}

static void
buffer_connect_signals (GtkText *self)
{
  g_signal_connect (get_buffer (self), "inserted-text", G_CALLBACK (buffer_inserted_text), self);
  g_signal_connect (get_buffer (self), "deleted-text", G_CALLBACK (buffer_deleted_text), self);
  g_signal_connect_after (get_buffer (self), "deleted-text", G_CALLBACK (buffer_deleted_text_after), self);
  g_signal_connect (get_buffer (self), "notify::text", G_CALLBACK (buffer_notify_text), self);
  g_signal_connect (get_buffer (self), "notify::max-length", G_CALLBACK (buffer_notify_max_length), self);
}

static void
buffer_disconnect_signals (GtkText *self)
{
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_inserted_text, self);
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_deleted_text, self);
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_deleted_text_after, self);
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_notify_text, self);
  g_signal_handlers_disconnect_by_func (get_buffer (self), buffer_notify_max_length, self);
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static int
get_better_cursor_x (GtkText *self,
                     int      offset)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkSeat *seat;
  GdkDevice *keyboard = NULL;
  PangoDirection direction = PANGO_DIRECTION_LTR;
  gboolean split_cursor;
  PangoLayout *layout = gtk_text_ensure_layout (self, TRUE);
  const char *text = pango_layout_get_text (layout);
  int index = g_utf8_offset_to_pointer (text, offset) - text;
  PangoRectangle strong_pos, weak_pos;

  seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (self)));
  if (seat)
    keyboard = gdk_seat_get_keyboard (seat);
  if (keyboard)
    direction = gdk_device_get_direction (keyboard);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                "gtk-split-cursor", &split_cursor,
                NULL);

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  if (split_cursor)
    return strong_pos.x / PANGO_SCALE;
  else
    return (direction == priv->resolved_dir) ? strong_pos.x / PANGO_SCALE : weak_pos.x / PANGO_SCALE;
}

static void
gtk_text_move_cursor (GtkText         *self,
                      GtkMovementStep  step,
                      int              count,
                      gboolean         extend_selection)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int new_pos = priv->current_pos;

  if (priv->current_pos != priv->selection_bound && !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
        {
        case GTK_MOVEMENT_VISUAL_POSITIONS:
          {
            int current_x = get_better_cursor_x (self, priv->current_pos);
            int bound_x = get_better_cursor_x (self, priv->selection_bound);

            if (count <= 0)
              new_pos = current_x < bound_x ? priv->current_pos : priv->selection_bound;
            else
              new_pos = current_x > bound_x ? priv->current_pos : priv->selection_bound;
          }
          break;

        case GTK_MOVEMENT_WORDS:
          if (priv->resolved_dir == PANGO_DIRECTION_RTL)
            count *= -1;
          G_GNUC_FALLTHROUGH;

        case GTK_MOVEMENT_LOGICAL_POSITIONS:
          if (count < 0)
            new_pos = MIN (priv->current_pos, priv->selection_bound);
          else
            new_pos = MAX (priv->current_pos, priv->selection_bound);

          break;

        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
        case GTK_MOVEMENT_BUFFER_ENDS:
          new_pos = count < 0 ? 0 : gtk_entry_buffer_get_length (get_buffer (self));
          break;

        case GTK_MOVEMENT_DISPLAY_LINES:
        case GTK_MOVEMENT_PARAGRAPHS:
        case GTK_MOVEMENT_PAGES:
        case GTK_MOVEMENT_HORIZONTAL_PAGES:
        default:
          break;
        }
    }
  else
    {
      switch (step)
        {
        case GTK_MOVEMENT_LOGICAL_POSITIONS:
          new_pos = gtk_text_move_logically (self, new_pos, count);
          break;

        case GTK_MOVEMENT_VISUAL_POSITIONS:
          new_pos = gtk_text_move_visually (self, new_pos, count);

          if (priv->current_pos == new_pos)
            {
              if (!extend_selection)
                {
                  if (!gtk_widget_keynav_failed (GTK_WIDGET (self),
                                                 count > 0 ?
                                                 GTK_DIR_RIGHT : GTK_DIR_LEFT))
                    {
                      GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

                      if (toplevel)
                        gtk_widget_child_focus (toplevel,
                                                count > 0 ?
                                                GTK_DIR_RIGHT : GTK_DIR_LEFT);
                    }
                }
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (self));
                }
            }
          break;

        case GTK_MOVEMENT_WORDS:
          if (priv->resolved_dir == PANGO_DIRECTION_RTL)
            count *= -1;

          while (count > 0)
            {
              new_pos = gtk_text_move_forward_word (self, new_pos, FALSE);
              count--;
            }

          while (count < 0)
            {
              new_pos = gtk_text_move_backward_word (self, new_pos, FALSE);
              count++;
            }

          if (priv->current_pos == new_pos)
            gtk_widget_error_bell (GTK_WIDGET (self));

          break;

        case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
        case GTK_MOVEMENT_PARAGRAPH_ENDS:
        case GTK_MOVEMENT_BUFFER_ENDS:
          new_pos = count < 0 ? 0 : gtk_entry_buffer_get_length (get_buffer (self));

          if (priv->current_pos == new_pos)
            gtk_widget_error_bell (GTK_WIDGET (self));

          break;

        case GTK_MOVEMENT_DISPLAY_LINES:
        case GTK_MOVEMENT_PARAGRAPHS:
        case GTK_MOVEMENT_PAGES:
        case GTK_MOVEMENT_HORIZONTAL_PAGES:
        default:
          break;
        }
    }

  if (extend_selection)
    gtk_text_set_selection_bounds (self, priv->selection_bound, new_pos);
  else
    gtk_text_set_selection_bounds (self, new_pos, new_pos);

  gtk_text_pend_cursor_blink (self);

  priv->need_im_reset = TRUE;
  gtk_text_reset_im_context (self);
}

static void
gtk_text_insert_at_cursor (GtkText    *self,
                           const char *str)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int pos = priv->current_pos;

  if (priv->editable)
    {
      begin_change (self);
      gtk_text_reset_im_context (self);
      gtk_editable_insert_text (GTK_EDITABLE (self), str, -1, &pos);
      gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                           GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                           pos,
                                           pos + g_utf8_strlen (str, -1));
      gtk_text_set_selection_bounds (self, pos, pos);
      end_change (self);
    }
}

static void
gtk_text_delete_from_cursor (GtkText       *self,
                             GtkDeleteType  type,
                             int            count)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int start_pos = priv->current_pos;
  int end_pos = priv->current_pos;
  int old_n_bytes = gtk_entry_buffer_get_bytes (get_buffer (self));

  if (!priv->editable)
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
      return;
    }

  begin_change (self);

  if (priv->selection_bound != priv->current_pos)
    {
      gtk_text_delete_selection (self);
      gtk_text_schedule_im_reset (self);
      gtk_text_reset_im_context (self);
      goto done;
    }

  switch (type)
    {
    case GTK_DELETE_CHARS:
      end_pos = gtk_text_move_logically (self, priv->current_pos, count);
      gtk_editable_delete_text (GTK_EDITABLE (self), MIN (start_pos, end_pos), MAX (start_pos, end_pos));
      break;

    case GTK_DELETE_WORDS:
      if (count < 0)
        {
          /* Move to end of current word, or if not on a word, end of previous word */
          end_pos = gtk_text_move_backward_word (self, end_pos, FALSE);
          end_pos = gtk_text_move_forward_word (self, end_pos, FALSE);
        }
      else if (count > 0)
        {
          /* Move to beginning of current word, or if not on a word, beginning of next word */
          start_pos = gtk_text_move_forward_word (self, start_pos, FALSE);
          start_pos = gtk_text_move_backward_word (self, start_pos, FALSE);
        }
      G_GNUC_FALLTHROUGH;
    case GTK_DELETE_WORD_ENDS:
      while (count < 0)
        {
          start_pos = gtk_text_move_backward_word (self, start_pos, FALSE);
          count++;
        }

      while (count > 0)
        {
          end_pos = gtk_text_move_forward_word (self, end_pos, FALSE);
          count--;
        }

      gtk_editable_delete_text (GTK_EDITABLE (self), start_pos, end_pos);
      break;

    case GTK_DELETE_DISPLAY_LINE_ENDS:
    case GTK_DELETE_PARAGRAPH_ENDS:
      if (count < 0)
        gtk_editable_delete_text (GTK_EDITABLE (self), 0, priv->current_pos);
      else
        gtk_editable_delete_text (GTK_EDITABLE (self), priv->current_pos, -1);
      break;

    case GTK_DELETE_DISPLAY_LINES:
    case GTK_DELETE_PARAGRAPHS:
      gtk_editable_delete_text (GTK_EDITABLE (self), 0, -1);
      break;

    case GTK_DELETE_WHITESPACE:
      gtk_text_delete_whitespace (self);
      break;

    default:
      break;
    }

  if (gtk_entry_buffer_get_bytes (get_buffer (self)) == old_n_bytes)
    gtk_widget_error_bell (GTK_WIDGET (self));
  else
    {
      gtk_text_schedule_im_reset (self);
      gtk_text_reset_im_context (self);
    }

done:
  end_change (self);
  gtk_text_pend_cursor_blink (self);
}

static void
gtk_text_backspace (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int prev_pos;

  if (!priv->editable)
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
      return;
    }

  begin_change (self);

  if (priv->selection_bound != priv->current_pos)
    {
      gtk_text_delete_selection (self);
      gtk_text_schedule_im_reset (self);
      gtk_text_reset_im_context (self);
      goto done;
    }

  prev_pos = gtk_text_move_logically (self, priv->current_pos, -1);

  if (prev_pos < priv->current_pos)
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

      /* Deleting parts of characters */
      if (log_attrs[priv->current_pos].backspace_deletes_character)
        {
          char *cluster_text;
          char *normalized_text;
          glong  len;

          cluster_text = gtk_text_get_display_text (self, prev_pos, priv->current_pos);
          normalized_text = g_utf8_normalize (cluster_text,
                                              strlen (cluster_text),
                                              G_NORMALIZE_NFD);
          len = g_utf8_strlen (normalized_text, -1);

          gtk_editable_delete_text (GTK_EDITABLE (self), prev_pos, priv->current_pos);
          if (len > 1)
            {
              int pos = priv->current_pos;

              gtk_editable_insert_text (GTK_EDITABLE (self), normalized_text,
                                        g_utf8_offset_to_pointer (normalized_text, len - 1) - normalized_text,
                                        &pos);
              gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                                   GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                                   pos,
                                                   pos + len);
              gtk_text_set_selection_bounds (self, pos, pos);
            }

          g_free (normalized_text);
          g_free (cluster_text);
        }
      else
        {
          gtk_editable_delete_text (GTK_EDITABLE (self), prev_pos, priv->current_pos);
        }

      gtk_text_schedule_im_reset (self);
      gtk_text_reset_im_context (self);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
    }

done:
  end_change (self);
  gtk_text_pend_cursor_blink (self);
}

static void
gtk_text_copy_clipboard (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bound != priv->current_pos)
    {
      char *str;

      if (!priv->visible)
        {
          gtk_widget_error_bell (GTK_WIDGET (self));
          return;
        }

      if (priv->selection_bound < priv->current_pos)
        str = gtk_text_get_display_text (self, priv->selection_bound, priv->current_pos);
      else
        str = gtk_text_get_display_text (self, priv->current_pos, priv->selection_bound);

      gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)), str);
      g_free (str);
    }
}

static void
gtk_text_cut_clipboard (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!priv->visible)
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
      return;
    }

  gtk_text_copy_clipboard (self);

  if (priv->editable)
    {
      if (priv->selection_bound != priv->current_pos)
        {
          begin_change (self);
          gtk_text_delete_selection (self);
          end_change (self);
        }
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
    }

  gtk_text_selection_bubble_popup_unset (self);

  gtk_text_update_handles (self);
}

static void
gtk_text_paste_clipboard (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    {
      begin_change (self);
      gtk_text_paste (self, gtk_widget_get_clipboard (GTK_WIDGET (self)));
      end_change (self);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
    }

  gtk_text_update_handles (self);
}

static void
gtk_text_delete_cb (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    {
      if (priv->selection_bound != priv->current_pos)
        gtk_text_delete_selection (self);
    }
}

static void
gtk_text_toggle_overwrite (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->overwrite_mode = !priv->overwrite_mode;

  if (priv->overwrite_mode)
    {
      if (!priv->block_cursor_node)
        {
          GtkCssNode *widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));

          priv->block_cursor_node = gtk_css_node_new ();
          gtk_css_node_set_name (priv->block_cursor_node, g_quark_from_static_string ("block-cursor"));
          gtk_css_node_set_parent (priv->block_cursor_node, widget_node);
          gtk_css_node_set_state (priv->block_cursor_node, gtk_css_node_get_state (widget_node));
          g_object_unref (priv->block_cursor_node);
        }
    }
  else
    {
      if (priv->block_cursor_node)
        {
          gtk_css_node_set_parent (priv->block_cursor_node, NULL);
          priv->block_cursor_node = NULL;
        }
    }

  gtk_text_pend_cursor_blink (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_text_select_all (GtkText *self)
{
  gtk_text_select_line (self);
}

static void
gtk_text_real_activate (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->activates_default)
    gtk_widget_activate_default (GTK_WIDGET (self));
}

static void
direction_changed (GdkDevice  *device,
                   GParamSpec *pspec,
                   GtkText    *self)
{
  gtk_text_recompute (self);
}

/* IM Context Callbacks
 */

static void
gtk_text_preedit_start_cb (GtkIMContext *context,
                           GtkText      *self)
{
  gtk_text_delete_selection (self);
}

static void
gtk_text_commit_cb (GtkIMContext *context,
                    const char   *str,
                    GtkText      *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    {
      gtk_text_enter_text (self, str);
      gtk_text_obscure_mouse_cursor (self);
    }
}

static void
gtk_text_preedit_changed_cb (GtkIMContext *context,
                             GtkText      *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    {
      char *preedit_string;
      int cursor_pos;

      gtk_text_obscure_mouse_cursor (self);

      gtk_im_context_get_preedit_string (priv->im_context,
                                         &preedit_string, NULL,
                                         &cursor_pos);
      g_signal_emit (self, signals[PREEDIT_CHANGED], 0, preedit_string);
      priv->preedit_length = strlen (preedit_string);
      cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1));
      priv->preedit_cursor = cursor_pos;
      g_free (preedit_string);

      gtk_text_recompute (self);
      update_placeholder_visibility (self);
    }
}

static gboolean
gtk_text_retrieve_surrounding_cb (GtkIMContext *context,
                                  GtkText      *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  char *text;

  /* XXXX ??? does this even make sense when text is not visible? Should we return FALSE? */
  text = gtk_text_get_display_text (self, 0, -1);
  gtk_im_context_set_surrounding_with_selection (context, text, strlen (text), /* Length in bytes */
                                                 g_utf8_offset_to_pointer (text, priv->current_pos) - text,
                                                 g_utf8_offset_to_pointer (text, priv->selection_bound) - text);
  g_free (text);

  return TRUE;
}

static gboolean
gtk_text_delete_surrounding_cb (GtkIMContext *context,
                                int           offset,
                                int           n_chars,
                                GtkText      *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->editable)
    {
      gtk_editable_delete_text (GTK_EDITABLE (self),
                                priv->current_pos + offset,
                                priv->current_pos + offset + n_chars);
    }

  return TRUE;
}

/* Internal functions
 */

/* Used for im_commit_cb and inserting Unicode chars */
void
gtk_text_enter_text (GtkText    *self,
                     const char *str)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int tmp_pos;
  guint text_length;

  gtk_text_history_begin_user_action (priv->history);
  begin_change (self);

  priv->need_im_reset = FALSE;

  if (priv->selection_bound != priv->current_pos)
    gtk_text_delete_selection (self);
  else
    {
      if (priv->overwrite_mode)
        {
          text_length = gtk_entry_buffer_get_length (get_buffer (self));
          if (priv->current_pos < text_length)
            gtk_text_delete_from_cursor (self, GTK_DELETE_CHARS, 1);
        }
    }

  tmp_pos = priv->current_pos;
  gtk_editable_insert_text (GTK_EDITABLE (self), str, strlen (str), &tmp_pos);
  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                       tmp_pos, tmp_pos + g_utf8_strlen (str, -1));
  gtk_text_set_selection_bounds (self, tmp_pos, tmp_pos);

  end_change (self);
  gtk_text_history_end_user_action (priv->history);
}

/* All changes to priv->current_pos and priv->selection_bound
 * should go through this function.
 */
void
gtk_text_set_positions (GtkText *self,
                        int      current_pos,
                        int      selection_bound)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (self));

  if (current_pos != -1 &&
      priv->current_pos != current_pos)
    {
      priv->current_pos = current_pos;
      changed = TRUE;

      g_object_notify (G_OBJECT (self), "cursor-position");
    }

  if (selection_bound != -1 &&
      priv->selection_bound != selection_bound)
    {
      priv->selection_bound = selection_bound;
      changed = TRUE;

      g_object_notify (G_OBJECT (self), "selection-bound");
    }

  g_object_thaw_notify (G_OBJECT (self));

  if (priv->current_pos != priv->selection_bound)
    {
      if (!priv->selection_node)
        {
          GtkCssNode *widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));

          priv->selection_node = gtk_css_node_new ();
          gtk_css_node_set_name (priv->selection_node, g_quark_from_static_string ("selection"));
          gtk_css_node_set_parent (priv->selection_node, widget_node);
          gtk_css_node_set_state (priv->selection_node, gtk_css_node_get_state (widget_node));
          g_object_unref (priv->selection_node);
        }
    }
  else
    {
      if (priv->selection_node)
        {
          gtk_css_node_set_parent (priv->selection_node, NULL);
          priv->selection_node = NULL;
        }
    }

  if (changed)
    {
      gtk_text_update_clipboard_actions (self);
      gtk_text_recompute (self);
    }
}

static void
gtk_text_reset_layout (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->cached_layout)
    {
      g_object_unref (priv->cached_layout);
      priv->cached_layout = NULL;
    }
}

static void
gtk_text_recompute (GtkText *self)
{
  gtk_text_reset_layout (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  if (!gtk_widget_get_mapped (GTK_WIDGET (self)))
    return;

  gtk_text_check_cursor_blink (self);
  gtk_text_adjust_scroll (self);
  update_im_cursor_location (self);
  gtk_text_update_handles (self);
}

static PangoLayout *
gtk_text_create_layout (GtkText  *self,
                        gboolean  include_preedit)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);
  PangoLayout *layout;
  PangoAttrList *tmp_attrs = NULL;
  char *preedit_string = NULL;
  int preedit_length = 0;
  PangoAttrList *preedit_attrs = NULL;
  char *display_text;
  guint n_bytes;

  layout = gtk_widget_create_pango_layout (widget, NULL);
  pango_layout_set_single_paragraph_mode (layout, TRUE);

  tmp_attrs = gtk_css_style_get_pango_attributes (gtk_css_node_get_style (gtk_widget_get_css_node (widget)));
  if (!tmp_attrs)
    tmp_attrs = pango_attr_list_new ();
  tmp_attrs = _gtk_pango_attr_list_merge (tmp_attrs, priv->attrs);

  display_text = gtk_text_get_display_text (self, 0, -1);

  n_bytes = strlen (display_text);

  if (include_preedit)
    {
      gtk_im_context_get_preedit_string (priv->im_context,
                                         &preedit_string, &preedit_attrs, NULL);
      preedit_length = priv->preedit_length;
    }

  if (preedit_length)
    {
      GString *tmp_string = g_string_new (display_text);
      int pos;

      pos = g_utf8_offset_to_pointer (display_text, priv->current_pos) - display_text;
      g_string_insert (tmp_string, pos, preedit_string);
      pango_layout_set_text (layout, tmp_string->str, tmp_string->len);
      pango_attr_list_splice (tmp_attrs, preedit_attrs, pos, preedit_length);
      g_string_free (tmp_string, TRUE);
    }
  else
    {
      PangoDirection pango_dir;

      if (gtk_text_get_display_mode (self) == DISPLAY_NORMAL)
        pango_dir = gdk_find_base_dir (display_text, n_bytes);
      else
        pango_dir = PANGO_DIRECTION_NEUTRAL;

      if (pango_dir == PANGO_DIRECTION_NEUTRAL)
        {
          if (gtk_widget_has_focus (widget))
            {
              GdkDisplay *display;
              GdkSeat *seat;
              GdkDevice *keyboard = NULL;
              PangoDirection direction = PANGO_DIRECTION_LTR;

              display = gtk_widget_get_display (widget);
              seat = gdk_display_get_default_seat (display);
              if (seat)
                keyboard = gdk_seat_get_keyboard (seat);
              if (keyboard)
                direction = gdk_device_get_direction (keyboard);

              if (direction == PANGO_DIRECTION_RTL)
                pango_dir = PANGO_DIRECTION_RTL;
              else
                pango_dir = PANGO_DIRECTION_LTR;
            }
          else
            {
              if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
                pango_dir = PANGO_DIRECTION_RTL;
              else
                pango_dir = PANGO_DIRECTION_LTR;
            }
        }

      pango_context_set_base_dir (gtk_widget_get_pango_context (widget), pango_dir);

      priv->resolved_dir = pango_dir;

      pango_layout_set_text (layout, display_text, n_bytes);
    }

  pango_layout_set_attributes (layout, tmp_attrs);

  if (priv->tabs)
    pango_layout_set_tabs (layout, priv->tabs);

  g_free (preedit_string);
  g_free (display_text);

  pango_attr_list_unref (preedit_attrs);
  pango_attr_list_unref (tmp_attrs);

  return layout;
}

static PangoLayout *
gtk_text_ensure_layout (GtkText  *self,
                        gboolean  include_preedit)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->preedit_length > 0 &&
      !include_preedit != !priv->cache_includes_preedit)
    gtk_text_reset_layout (self);

  if (!priv->cached_layout)
    {
      priv->cached_layout = gtk_text_create_layout (self, include_preedit);
      priv->cache_includes_preedit = include_preedit;
    }

  return priv->cached_layout;
}

static void
get_layout_position (GtkText *self,
                     int     *x,
                     int     *y)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  const int text_height = gtk_widget_get_height (GTK_WIDGET (self));
  PangoLayout *layout;
  PangoRectangle logical_rect;
  int y_pos, area_height;
  PangoLayoutLine *line;

  layout = gtk_text_ensure_layout (self, TRUE);

  area_height = PANGO_SCALE * text_height;

  line = pango_layout_get_lines_readonly (layout)->data;
  pango_layout_line_get_extents (line, NULL, &logical_rect);

  /* Align primarily for locale's ascent/descent */
  if (priv->text_baseline < 0)
    y_pos = ((area_height - priv->ascent - priv->descent) / 2 +
             priv->ascent + logical_rect.y);
  else
    y_pos = PANGO_SCALE * priv->text_baseline - pango_layout_get_baseline (layout);

  /* Now see if we need to adjust to fit in actual drawn string */
  if (logical_rect.height > area_height)
    y_pos = (area_height - logical_rect.height) / 2;
  else if (y_pos < 0)
    y_pos = 0;
  else if (y_pos + logical_rect.height > area_height)
    y_pos = area_height - logical_rect.height;

  y_pos = y_pos / PANGO_SCALE;

  if (x)
    *x = - priv->scroll_offset;

  if (y)
    *y = y_pos;
}

#define GRAPHENE_RECT_FROM_RECT(_r) (GRAPHENE_RECT_INIT ((_r)->x, (_r)->y, (_r)->width, (_r)->height))

static void
gtk_text_draw_text (GtkText     *self,
                    GtkSnapshot *snapshot)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);
  GtkCssStyle *style;
  PangoLayout *layout;
  int x, y;
  GtkCssBoxes boxes;

  /* Nothing to display at all */
  if (gtk_text_get_display_mode (self) == DISPLAY_BLANK)
    return;

  layout = gtk_text_ensure_layout (self, TRUE);

  gtk_text_get_layout_offsets (self, &x, &y);

  gtk_css_boxes_init (&boxes, widget);
  gtk_css_style_snapshot_layout (&boxes, snapshot, x, y, layout);

  if (priv->selection_bound != priv->current_pos)
    {
      const char *text = pango_layout_get_text (layout);
      int start_index = g_utf8_offset_to_pointer (text, priv->selection_bound) - text;
      int end_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;
      cairo_region_t *clip;
      cairo_rectangle_int_t clip_extents;
      int range[2];
      int width, height;

      width = gtk_widget_get_width (widget);
      height = gtk_widget_get_height (widget);

      range[0] = MIN (start_index, end_index);
      range[1] = MAX (start_index, end_index);

      style = gtk_css_node_get_style (priv->selection_node);

      clip = gdk_pango_layout_get_clip_region (layout, x, y, range, 1);
      cairo_region_get_extents (clip, &clip_extents);

      gtk_css_boxes_init_border_box (&boxes, style, 0, 0, width, height);
      gtk_snapshot_push_clip (snapshot, &GRAPHENE_RECT_FROM_RECT (&clip_extents));
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_layout (&boxes, snapshot, x, y, layout);
      gtk_snapshot_pop (snapshot);

      cairo_region_destroy (clip);
    }
}

static void
gtk_text_draw_cursor (GtkText     *self,
                      GtkSnapshot *snapshot,
                      CursorType   type)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);
  GtkCssStyle *style;
  PangoRectangle cursor_rect;
  int cursor_index;
  gboolean block;
  gboolean block_at_line_end;
  PangoLayout *layout;
  const char *text;
  int x, y;
  GtkCssBoxes boxes;
  GdkDisplay *display;

  display = gtk_widget_get_display (widget);

  layout = g_object_ref (gtk_text_ensure_layout (self, TRUE));
  text = pango_layout_get_text (layout);
  gtk_text_get_layout_offsets (self, &x, &y);

  if (type == CURSOR_DND)
    cursor_index = g_utf8_offset_to_pointer (text, priv->dnd_position) - text;
  else
    cursor_index = g_utf8_offset_to_pointer (text, priv->current_pos + priv->preedit_cursor) - text;

  if (!priv->overwrite_mode)
    block = FALSE;
  else
    block = _gtk_text_util_get_block_cursor_location (layout,
                                                      cursor_index, &cursor_rect, &block_at_line_end);

  if (!block)
    {
      gtk_css_boxes_init (&boxes, widget);
      gtk_css_style_snapshot_caret (&boxes, display, snapshot,
                                    x, y,
                                    layout, cursor_index, priv->resolved_dir);
    }
  else /* overwrite_mode */
    {
      int width = gtk_widget_get_width (widget);
      int height = gtk_widget_get_height (widget);
      graphene_rect_t bounds;

      bounds.origin.x = PANGO_PIXELS (cursor_rect.x) + x;
      bounds.origin.y = PANGO_PIXELS (cursor_rect.y) + y;
      bounds.size.width = PANGO_PIXELS (cursor_rect.width);
      bounds.size.height = PANGO_PIXELS (cursor_rect.height);

      style = gtk_css_node_get_style (priv->block_cursor_node);
      gtk_css_boxes_init_border_box (&boxes, style, 0, 0, width, height);
      gtk_snapshot_push_clip (snapshot, &bounds);
      gtk_css_style_snapshot_background (&boxes, snapshot);
      gtk_css_style_snapshot_layout (&boxes,snapshot, x, y, layout);
      gtk_snapshot_pop (snapshot);
    }

  g_object_unref (layout);
}

static void
gtk_text_handle_dragged (GtkTextHandle *handle,
                         int            x,
                         int            y,
                         GtkText       *self)
{
  int cursor_pos, selection_bound_pos, tmp_pos, *old_pos;
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  gtk_text_selection_bubble_popup_unset (self);

  cursor_pos = priv->current_pos;
  selection_bound_pos = priv->selection_bound;

  tmp_pos = gtk_text_find_position (self, x + priv->scroll_offset);

  if (handle == priv->text_handles[TEXT_HANDLE_CURSOR])
    {
      /* Avoid running past the other handle in selection mode */
      if (tmp_pos >= selection_bound_pos &&
          gtk_widget_is_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])))
        {
          tmp_pos = selection_bound_pos - 1;
        }

      old_pos = &cursor_pos;
    }
  else if (handle == priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])
    {
      /* Avoid running past the other handle */
      if (tmp_pos <= cursor_pos)
        tmp_pos = cursor_pos + 1;

      old_pos = &selection_bound_pos;
    }
  else
    g_assert_not_reached ();

  if (tmp_pos != *old_pos)
    {
      *old_pos = tmp_pos;

      if (handle == priv->text_handles[TEXT_HANDLE_CURSOR] &&
          !gtk_widget_is_visible (GTK_WIDGET (priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])))
        gtk_text_set_positions (self, cursor_pos, cursor_pos);
      else
        gtk_text_set_positions (self, cursor_pos, selection_bound_pos);

      if (handle == priv->text_handles[TEXT_HANDLE_CURSOR])
        priv->cursor_handle_dragged = TRUE;
      else if (handle == priv->text_handles[TEXT_HANDLE_SELECTION_BOUND])
        priv->selection_handle_dragged = TRUE;

      gtk_text_update_handles (self);
    }

  gtk_text_show_magnifier (self, x, y);
}

static void
gtk_text_handle_drag_started (GtkTextHandle *handle,
                              GtkText       *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->cursor_handle_dragged = FALSE;
  priv->selection_handle_dragged = FALSE;
}

static void
gtk_text_handle_drag_finished (GtkTextHandle *handle,
                               GtkText       *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!priv->cursor_handle_dragged && !priv->selection_handle_dragged)
    {
      GtkSettings *settings;
      guint double_click_time;

      settings = gtk_widget_get_settings (GTK_WIDGET (self));
      g_object_get (settings, "gtk-double-click-time", &double_click_time, NULL);
      if (g_get_monotonic_time() - priv->handle_place_time < double_click_time * 1000)
        {
          gtk_text_select_word (self);
          gtk_text_update_handles (self);
        }
      else
        gtk_text_selection_bubble_popup_set (self);
    }

  if (priv->magnifier_popover)
    gtk_popover_popdown (GTK_POPOVER (priv->magnifier_popover));
}

static void
gtk_text_schedule_im_reset (GtkText *self)
{
  GtkTextPrivate *priv;

  priv = gtk_text_get_instance_private (self);

  priv->need_im_reset = TRUE;
}

void
gtk_text_reset_im_context (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->need_im_reset)
    {
      priv->need_im_reset = FALSE;
      gtk_im_context_reset (priv->im_context);
    }
}

static int
gtk_text_find_position (GtkText *self,
                        int      x)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoLayout *layout;
  PangoLayoutLine *line;
  int index;
  int pos;
  int trailing;
  const char *text;
  int cursor_index;

  layout = gtk_text_ensure_layout (self, TRUE);
  text = pango_layout_get_text (layout);
  cursor_index = g_utf8_offset_to_pointer (text, priv->current_pos) - text;

  line = pango_layout_get_lines_readonly (layout)->data;
  pango_layout_line_x_to_index (line, x * PANGO_SCALE, &index, &trailing);

  if (index >= cursor_index && priv->preedit_length)
    {
      if (index >= cursor_index + priv->preedit_length)
        index -= priv->preedit_length;
      else
        {
          index = cursor_index;
          trailing = 0;
        }
    }

  pos = g_utf8_pointer_to_offset (text, text + index);
  pos += trailing;

  return pos;
}

static void
gtk_text_get_cursor_locations (GtkText   *self,
                               int       *strong_x,
                               int       *weak_x)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  DisplayMode mode = gtk_text_get_display_mode (self);

  /* Nothing to display at all, so no cursor is relevant */
  if (mode == DISPLAY_BLANK)
    {
      if (strong_x)
        *strong_x = 0;

      if (weak_x)
        *weak_x = 0;
    }
  else
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, TRUE);
      const char *text = pango_layout_get_text (layout);
      PangoRectangle strong_pos, weak_pos;
      int index;

      index = g_utf8_offset_to_pointer (text, priv->current_pos + priv->preedit_cursor) - text;

      pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

      if (strong_x)
        *strong_x = strong_pos.x / PANGO_SCALE;

      if (weak_x)
        *weak_x = weak_pos.x / PANGO_SCALE;
    }
}

static gboolean
gtk_text_get_is_selection_handle_dragged (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkTextHandle *handle;

  if (priv->current_pos >= priv->selection_bound)
    handle = priv->text_handles[TEXT_HANDLE_CURSOR];
  else
    handle = priv->text_handles[TEXT_HANDLE_SELECTION_BOUND];

  return handle && gtk_text_handle_get_is_dragged (handle);
}

static void
gtk_text_get_scroll_limits (GtkText *self,
                            int     *min_offset,
                            int     *max_offset)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  float xalign;
  PangoLayout *layout;
  PangoLayoutLine *line;
  PangoRectangle logical_rect;
  int text_width, width;

  layout = gtk_text_ensure_layout (self, TRUE);
  line = pango_layout_get_lines_readonly (layout)->data;

  pango_layout_line_get_extents (line, NULL, &logical_rect);

  /* Display as much text as we can */

  if (priv->resolved_dir == PANGO_DIRECTION_LTR)
      xalign = priv->xalign;
  else
      xalign = 1.0 - priv->xalign;

  text_width = PANGO_PIXELS(logical_rect.width);
  width = gtk_widget_get_width (GTK_WIDGET (self));

  if (text_width > width)
    {
      *min_offset = 0;
      *max_offset = text_width - width;
    }
  else
    {
      *min_offset = (text_width - width) * xalign;
      *max_offset = *min_offset;
    }
}

static void
gtk_text_adjust_scroll (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  const int text_width = gtk_widget_get_width (GTK_WIDGET (self));
  int min_offset, max_offset;
  int strong_x, weak_x;
  int strong_xoffset, weak_xoffset;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  gtk_text_get_scroll_limits (self, &min_offset, &max_offset);

  priv->scroll_offset = CLAMP (priv->scroll_offset, min_offset, max_offset);

  if (gtk_text_get_is_selection_handle_dragged (self))
    {
      /* The text handle corresponding to the selection bound is
       * being dragged, ensure it stays onscreen even if we scroll
       * cursors away, this is so both handles can cause content
       * to scroll.
       */
      strong_x = weak_x = gtk_text_get_selection_bound_location (self);
    }
  else
    {
      /* And make sure cursors are on screen. Note that the cursor is
       * actually drawn one pixel into the INNER_BORDER space on
       * the right, when the scroll is at the utmost right. This
       * looks better to me than confining the cursor inside the
       * border entirely, though it means that the cursor gets one
       * pixel closer to the edge of the widget on the right than
       * on the left. This might need changing if one changed
       * INNER_BORDER from 2 to 1, as one would do on a
       * small-screen-real-estate display.
       *
       * We always make sure that the strong cursor is on screen, and
       * put the weak cursor on screen if possible.
       */
      gtk_text_get_cursor_locations (self, &strong_x, &weak_x);
    }

  strong_xoffset = strong_x - priv->scroll_offset;

  if (strong_xoffset < 0)
    {
      priv->scroll_offset += strong_xoffset;
      strong_xoffset = 0;
    }
  else if (strong_xoffset > text_width)
    {
      priv->scroll_offset += strong_xoffset - text_width;
      strong_xoffset = text_width;
    }

  weak_xoffset = weak_x - priv->scroll_offset;

  if (weak_xoffset < 0 && strong_xoffset - weak_xoffset <= text_width)
    {
      priv->scroll_offset += weak_xoffset;
    }
  else if (weak_xoffset > text_width &&
           strong_xoffset - (weak_xoffset - text_width) >= 0)
    {
      priv->scroll_offset += weak_xoffset - text_width;
    }

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_SCROLL_OFFSET]);

  gtk_text_update_handles (self);
}

static int
gtk_text_move_visually (GtkText *self,
                        int      start,
                        int      count)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int index;
  PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
  const char *text;
  gboolean split_cursor;
  gboolean strong;

  text = pango_layout_get_text (layout);

  index = g_utf8_offset_to_pointer (text, start) - text;


  g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                "gtk-split-cursor", &split_cursor,
                NULL);

  if (split_cursor)
    strong = TRUE;
  else
    {
      GdkDisplay *display;
      GdkSeat *seat;
      GdkDevice *keyboard = NULL;
      PangoDirection direction = PANGO_DIRECTION_LTR;

      display = gtk_widget_get_display (GTK_WIDGET (self));
      seat = gdk_display_get_default_seat (display);
      if (seat)
        keyboard = gdk_seat_get_keyboard (seat);
      if (keyboard)
        direction = gdk_device_get_direction (keyboard);

      strong = direction == priv->resolved_dir;
    }

  while (count != 0)
    {
      int new_index, new_trailing;

      if (count > 0)
        {
          pango_layout_move_cursor_visually (layout, strong, index, 0, 1, &new_index, &new_trailing);
          count--;
        }
      else
        {
          pango_layout_move_cursor_visually (layout, strong, index, 0, -1, &new_index, &new_trailing);
          count++;
        }

      if (new_index < 0)
        index = 0;
      else if (new_index != G_MAXINT)
        index = new_index;

      while (new_trailing--)
        index = g_utf8_next_char (text + index) - text;
    }

  return g_utf8_pointer_to_offset (text, text + index);
}

static int
gtk_text_move_logically (GtkText *self,
                         int      start,
                         int      count)
{
  int new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (self));

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (self) != DISPLAY_NORMAL)
    {
      new_pos = CLAMP (start + count, 0, length);
    }
  else
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

      while (count > 0 && new_pos < length)
        {
          do
            new_pos++;
          while (new_pos < length && !log_attrs[new_pos].is_cursor_position);

          count--;
        }
      while (count < 0 && new_pos > 0)
        {
          do
            new_pos--;
          while (new_pos > 0 && !log_attrs[new_pos].is_cursor_position);

          count++;
        }
    }

  return new_pos;
}

static int
gtk_text_move_forward_word (GtkText  *self,
                            int       start,
                            gboolean  allow_whitespace)
{
  int new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (self));

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (self) != DISPLAY_NORMAL)
    {
      new_pos = length;
    }
  else if (new_pos < length)
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

      /* Find the next word boundary */
      new_pos++;
      while (new_pos < n_attrs - 1 && !(log_attrs[new_pos].is_word_end ||
                                        (log_attrs[new_pos].is_word_start && allow_whitespace)))
        new_pos++;
    }

  return new_pos;
}


static int
gtk_text_move_backward_word (GtkText  *self,
                             int       start,
                             gboolean  allow_whitespace)
{
  int new_pos = start;

  /* Prevent any leak of information */
  if (gtk_text_get_display_mode (self) != DISPLAY_NORMAL)
    {
      new_pos = 0;
    }
  else if (start > 0)
    {
      PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
      const PangoLogAttr *log_attrs;
      int n_attrs;

      log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

      new_pos = start - 1;

      /* Find the previous word boundary */
      while (new_pos > 0 && !(log_attrs[new_pos].is_word_start ||
                              (log_attrs[new_pos].is_word_end && allow_whitespace)))
        new_pos--;
    }

  return new_pos;
}

static void
gtk_text_delete_whitespace (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  PangoLayout *layout = gtk_text_ensure_layout (self, FALSE);
  const PangoLogAttr *log_attrs;
  int n_attrs;
  int start, end;

  log_attrs = pango_layout_get_log_attrs_readonly (layout, &n_attrs);

  start = end = priv->current_pos;

  while (start > 0 && log_attrs[start-1].is_white)
    start--;

  while (end < n_attrs && log_attrs[end].is_white)
    end++;

  if (start != end)
    gtk_editable_delete_text (GTK_EDITABLE (self), start, end);
}


static void
gtk_text_select_word (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int start_pos = gtk_text_move_backward_word (self, priv->current_pos, TRUE);
  int end_pos = gtk_text_move_forward_word (self, priv->current_pos, TRUE);

  gtk_text_set_selection_bounds (self, start_pos, end_pos);
}

static void
gtk_text_select_line (GtkText *self)
{
  gtk_text_set_selection_bounds (self, 0, -1);
}

static int
truncate_multiline (const char *text)
{
  int length;

  for (length = 0;
       text[length] && text[length] != '\n' && text[length] != '\r';
       length++);

  return length;
}

static void
paste_received (GObject      *clipboard,
                GAsyncResult *result,
                gpointer      data)
{
  GtkText *self = GTK_TEXT (data);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  char *text;
  int pos, start, end;
  int length = -1;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), result, NULL);
  if (text == NULL)
    {
      gtk_widget_error_bell (GTK_WIDGET (self));
      return;
    }

  if (priv->insert_pos >= 0)
    {
      pos = priv->insert_pos;
      start = priv->selection_bound;
      end = priv->current_pos;
      if (!((start <= pos && pos <= end) || (end <= pos && pos <= start)))
        gtk_text_set_selection_bounds (self, pos, pos);
      priv->insert_pos = -1;
    }

  if (priv->truncate_multiline)
    length = truncate_multiline (text);
  else
    length = strlen (text);

  begin_change (self);
  if (priv->selection_bound != priv->current_pos)
    gtk_text_delete_selection (self);

  pos = priv->current_pos;
  gtk_editable_insert_text (GTK_EDITABLE (self), text, length, &pos);
  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                       pos, pos + length);
  gtk_text_set_selection_bounds (self, pos, pos);
  end_change (self);

  g_free (text);
  g_object_unref (self);
}

static void
gtk_text_paste (GtkText      *self,
                GdkClipboard *clipboard)
{
  gdk_clipboard_read_text_async (clipboard, NULL, paste_received, g_object_ref (self));
}

static void
gtk_text_update_primary_selection (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GdkClipboard *clipboard;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  if (!gtk_widget_has_focus (GTK_WIDGET (self)))
    return;

  clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (self));

  if (priv->selection_bound != priv->current_pos)
    {
      gdk_clipboard_set_content (clipboard, priv->selection_content);
    }
  else
    {
      if (gdk_clipboard_get_content (clipboard) == priv->selection_content)
        gdk_clipboard_set_content (clipboard, NULL);
    }
}

/* Public API
 */

/**
 * gtk_text_new:
 *
 * Creates a new `GtkText`.
 *
 * Returns: a new `GtkText`.
 */
GtkWidget *
gtk_text_new (void)
{
  return g_object_new (GTK_TYPE_TEXT, NULL);
}

/**
 * gtk_text_new_with_buffer:
 * @buffer: The buffer to use for the new `GtkText`.
 *
 * Creates a new `GtkText` with the specified text buffer.
 *
 * Returns: a new `GtkText`
 */
GtkWidget *
gtk_text_new_with_buffer (GtkEntryBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_ENTRY_BUFFER (buffer), NULL);

  return g_object_new (GTK_TYPE_TEXT, "buffer", buffer, NULL);
}

static GtkEntryBuffer *
get_buffer (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->buffer == NULL)
    {
      GtkEntryBuffer *buffer;
      buffer = gtk_entry_buffer_new (NULL, 0);
      gtk_text_set_buffer (self, buffer);
      g_object_unref (buffer);
    }

  return priv->buffer;
}

/**
 * gtk_text_get_buffer: (attributes org.gtk.Method.get_property=buffer)
 * @self: a `GtkText`
 *
 * Get the `GtkEntryBuffer` object which holds the text for
 * this widget.
 *
 * Returns: (transfer none): A `GtkEntryBuffer` object.
 */
GtkEntryBuffer *
gtk_text_get_buffer (GtkText *self)
{
  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  return get_buffer (self);
}

/**
 * gtk_text_set_buffer: (attributes org.gtk.Method.set_property=buffer)
 * @self: a `GtkText`
 * @buffer: a `GtkEntryBuffer`
 *
 * Set the `GtkEntryBuffer` object which holds the text for
 * this widget.
 */
void
gtk_text_set_buffer (GtkText        *self,
                     GtkEntryBuffer *buffer)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GObject *obj;
  gboolean had_buffer = FALSE;
  guint old_length = 0;
  guint new_length = 0;

  g_return_if_fail (GTK_IS_TEXT (self));

  if (buffer)
    {
      g_return_if_fail (GTK_IS_ENTRY_BUFFER (buffer));
      g_object_ref (buffer);
    }

  if (priv->buffer)
    {
      had_buffer = TRUE;
      old_length = gtk_entry_buffer_get_length (priv->buffer);
      buffer_disconnect_signals (self);
      g_object_unref (priv->buffer);
    }

  priv->buffer = buffer;

  if (priv->buffer)
    {
      new_length = gtk_entry_buffer_get_length (priv->buffer);
      buffer_connect_signals (self);
    }

  update_placeholder_visibility (self);

  obj = G_OBJECT (self);
  g_object_freeze_notify (obj);
  g_object_notify_by_pspec (obj, text_props[PROP_BUFFER]);
  g_object_notify_by_pspec (obj, text_props[PROP_MAX_LENGTH]);
  if (old_length != 0 || new_length != 0)
    g_object_notify (obj, "text");

  if (had_buffer)
    {
      gtk_text_set_selection_bounds (self, 0, 0);
      gtk_text_recompute (self);
    }

  g_object_thaw_notify (obj);
}

static void
gtk_text_set_editable (GtkText  *self,
                       gboolean  is_editable)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (is_editable != priv->editable)
    {
      GtkWidget *widget = GTK_WIDGET (self);

      if (!is_editable)
        {
          gtk_text_reset_im_context (self);
          if (gtk_widget_has_focus (widget))
            gtk_im_context_focus_out (priv->im_context);

          priv->preedit_length = 0;
          priv->preedit_cursor = 0;

          gtk_widget_remove_css_class (GTK_WIDGET (self), "read-only");
        }
      else
        {
          gtk_widget_add_css_class (GTK_WIDGET (self), "read-only");
        }

      priv->editable = is_editable;

      if (is_editable && gtk_widget_has_focus (widget))
        gtk_im_context_focus_in (priv->im_context);

      gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (priv->key_controller),
                                               is_editable ? priv->im_context : NULL);

      gtk_text_update_history (self);
      gtk_text_update_clipboard_actions (self);
      gtk_text_update_emoji_action (self);

      gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_PROPERTY_READ_ONLY, !priv->editable,
                                      -1);

      g_object_notify (G_OBJECT (self), "editable");
    }
}

static void
gtk_text_set_text (GtkText     *self,
                   const char *text)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int tmp_pos;

  g_return_if_fail (GTK_IS_TEXT (self));
  g_return_if_fail (text != NULL);

  /* Actually setting the text will affect the cursor and selection;
   * if the contents don't actually change, this will look odd to the user.
   */
  if (strcmp (gtk_entry_buffer_get_text (get_buffer (self)), text) == 0)
    return;

  gtk_text_history_begin_irreversible_action (priv->history);

  begin_change (self);
  g_object_freeze_notify (G_OBJECT (self));
  gtk_editable_delete_text (GTK_EDITABLE (self), 0, -1);
  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE,
                                       0, G_MAXUINT);
  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE (self), text, strlen (text), &tmp_pos);
  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                       tmp_pos, tmp_pos + g_utf8_strlen (text, -1));
  g_object_thaw_notify (G_OBJECT (self));
  end_change (self);

  gtk_text_history_end_irreversible_action (priv->history);
}

/**
 * gtk_text_set_visibility: (attributes org.gtk.Method.set_property=visibility)
 * @self: a `GtkText`
 * @visible: %TRUE if the contents of the `GtkText` are displayed
 *   as plaintext
 *
 * Sets whether the contents of the `GtkText` are visible or not.
 *
 * When visibility is set to %FALSE, characters are displayed
 * as the invisible char, and will also appear that way when
 * the text in the widget is copied to the clipboard.
 *
 * By default, GTK picks the best invisible character available
 * in the current font, but it can be changed with
 * [method@Gtk.Text.set_invisible_char].
 *
 * Note that you probably want to set [property@Gtk.Text:input-purpose]
 * to %GTK_INPUT_PURPOSE_PASSWORD or %GTK_INPUT_PURPOSE_PIN to
 * inform input methods about the purpose of this self,
 * in addition to setting visibility to %FALSE.
 */
void
gtk_text_set_visibility (GtkText  *self,
                         gboolean  visible)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  visible = visible != FALSE;

  if (priv->visible != visible)
    {
      priv->visible = visible;

      g_object_notify (G_OBJECT (self), "visibility");
      gtk_text_update_cached_style_values (self);
      gtk_text_recompute (self);

      /* disable undo when invisible text is used */
      gtk_text_update_history (self);

      gtk_text_update_clipboard_actions (self);
    }
}

/**
 * gtk_text_get_visibility: (attributes org.gtk.Method.get_property=visibility)
 * @self: a `GtkText`
 *
 * Retrieves whether the text in @self is visible.
 *
 * Returns: %TRUE if the text is currently visible
 */
gboolean
gtk_text_get_visibility (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->visible;
}

/**
 * gtk_text_set_invisible_char: (attributes org.gtk.Method.set_property=invisible-char)
 * @self: a `GtkText`
 * @ch: a Unicode character
 *
 * Sets the character to use when in “password mode”.
 *
 * By default, GTK picks the best invisible char available in the
 * current font. If you set the invisible char to 0, then the user
 * will get no feedback at all; there will be no text on the screen
 * as they type.
 */
void
gtk_text_set_invisible_char (GtkText  *self,
                             gunichar  ch)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (!priv->invisible_char_set)
    {
      priv->invisible_char_set = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR_SET]);
    }

  if (ch == priv->invisible_char)
    return;

  priv->invisible_char = ch;
  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR]);
  gtk_text_recompute (self);
}

/**
 * gtk_text_get_invisible_char: (attributes org.gtk.Method.get_property=invisible-char)
 * @self: a `GtkText`
 *
 * Retrieves the character displayed when visibility is set to false.
 *
 * Note that GTK does not compute this value unless it needs it,
 * so the value returned by this function is not very useful unless
 * it has been explicitly set with [method@Gtk.Text.set_invisible_char].
 *
 * Returns: the current invisible char, or 0, if @text does not
 *   show invisible text at all.
 */
gunichar
gtk_text_get_invisible_char (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), 0);

  return priv->invisible_char;
}

/**
 * gtk_text_unset_invisible_char:
 * @self: a `GtkText`
 *
 * Unsets the invisible char.
 *
 * After calling this, the default invisible
 * char is used again.
 */
void
gtk_text_unset_invisible_char (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gunichar ch;

  g_return_if_fail (GTK_IS_TEXT (self));

  if (!priv->invisible_char_set)
    return;

  priv->invisible_char_set = FALSE;
  ch = find_invisible_char (GTK_WIDGET (self));

  if (priv->invisible_char != ch)
    {
      priv->invisible_char = ch;
      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR]);
    }

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INVISIBLE_CHAR_SET]);
  gtk_text_recompute (self);
}

/**
 * gtk_text_set_overwrite_mode: (attributes org.gtk.Method.set_property=overwrite-mode)
 * @self: a `GtkText`
 * @overwrite: new value
 *
 * Sets whether the text is overwritten when typing
 * in the `GtkText`.
 */
void
gtk_text_set_overwrite_mode (GtkText  *self,
                             gboolean  overwrite)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->overwrite_mode == overwrite)
    return;

  gtk_text_toggle_overwrite (self);

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_OVERWRITE_MODE]);
}

/**
 * gtk_text_get_overwrite_mode: (attributes org.gtk.Method.get_property=overwrite-mode)
 * @self: a `GtkText`
 *
 * Gets whether text is overwritten when typing in the `GtkText`.
 *
 * See [method@Gtk.Text.set_overwrite_mode].
 *
 * Returns: whether the text is overwritten when typing
 */
gboolean
gtk_text_get_overwrite_mode (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->overwrite_mode;

}

/**
 * gtk_text_set_max_length: (attributes org.gtk.Method.set_property=max-length)
 * @self: a `GtkText`
 * @length: the maximum length of the `GtkText`, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed
 *   in will be clamped to the range 0-65536.
 *
 * Sets the maximum allowed length of the contents of the widget.
 *
 * If the current contents are longer than the given length, then
 * they will be truncated to fit.
 *
 * This is equivalent to getting @self's `GtkEntryBuffer` and
 * calling [method@Gtk.EntryBuffer.set_max_length] on it.
 */
void
gtk_text_set_max_length (GtkText *self,
                         int      length)
{
  g_return_if_fail (GTK_IS_TEXT (self));
  gtk_entry_buffer_set_max_length (get_buffer (self), length);
}

/**
 * gtk_text_get_max_length: (attributes org.gtk.Method.get_property=max-length)
 * @self: a `GtkText`
 *
 * Retrieves the maximum allowed length of the text in @self.
 *
 * See [method@Gtk.Text.set_max_length].
 *
 * This is equivalent to getting @self's `GtkEntryBuffer` and
 * calling [method@Gtk.EntryBuffer.get_max_length] on it.
 *
 * Returns: the maximum allowed number of characters
 *   in `GtkText`, or 0 if there is no maximum.
 */
int
gtk_text_get_max_length (GtkText *self)
{
  g_return_val_if_fail (GTK_IS_TEXT (self), 0);

  return gtk_entry_buffer_get_max_length (get_buffer (self));
}

/**
 * gtk_text_get_text_length:
 * @self: a `GtkText`
 *
 * Retrieves the current length of the text in @self.
 *
 * This is equivalent to getting @self's `GtkEntryBuffer`
 * and calling [method@Gtk.EntryBuffer.get_length] on it.
 *
 * Returns: the current number of characters
 *   in `GtkText`, or 0 if there are none.
 */
guint16
gtk_text_get_text_length (GtkText *self)
{
  g_return_val_if_fail (GTK_IS_TEXT (self), 0);

  return gtk_entry_buffer_get_length (get_buffer (self));
}

/**
 * gtk_text_set_activates_default: (attributes org.gtk.Method.set_property=activates-default)
 * @self: a `GtkText`
 * @activates: %TRUE to activate window’s default widget on Enter keypress
 *
 * If @activates is %TRUE, pressing Enter will activate
 * the default widget for the window containing @self.
 *
 * This usually means that the dialog containing the `GtkText`
 * will be closed, since the default widget is usually one of
 * the dialog buttons.
 */
void
gtk_text_set_activates_default (GtkText  *self,
                                gboolean  activates)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  activates = activates != FALSE;

  if (priv->activates_default != activates)
    {
      priv->activates_default = activates;
      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_ACTIVATES_DEFAULT]);
    }
}

/**
 * gtk_text_get_activates_default: (attributes org.gtk.Method.get_property=activates-default)
 * @self: a `GtkText`
 *
 * Returns whether pressing Enter will activate
 * the default widget for the window containing @self.
 *
 * See [method@Gtk.Text.set_activates_default].
 *
 * Returns: %TRUE if the `GtkText` will activate the default widget
 */
gboolean
gtk_text_get_activates_default (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->activates_default;
}

static void
gtk_text_set_width_chars (GtkText *self,
                          int      n_chars)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->width_chars != n_chars)
    {
      priv->width_chars = n_chars;
      g_object_notify (G_OBJECT (self), "width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

static void
gtk_text_set_max_width_chars (GtkText *self,
                              int      n_chars)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->max_width_chars != n_chars)
    {
      priv->max_width_chars = n_chars;
      g_object_notify (G_OBJECT (self), "max-width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

PangoLayout *
gtk_text_get_layout (GtkText *self)
{
  PangoLayout *layout;

  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  layout = gtk_text_ensure_layout (self, TRUE);

  return layout;
}

void
gtk_text_get_layout_offsets (GtkText *self,
                             int     *x,
                             int     *y)
{
  g_return_if_fail (GTK_IS_TEXT (self));

  get_layout_position (self, x, y);
}

static void
gtk_text_set_alignment (GtkText *self,
                        float    xalign)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (xalign != priv->xalign)
    {
      priv->xalign = xalign;
      gtk_text_recompute (self);
      if (priv->placeholder)
        gtk_label_set_xalign (GTK_LABEL (priv->placeholder), xalign);

      g_object_notify (G_OBJECT (self), "xalign");
    }
}

static void
hide_selection_bubble (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bubble && gtk_widget_get_visible (priv->selection_bubble))
    gtk_widget_set_visible (priv->selection_bubble, FALSE);
}

static void
gtk_text_activate_clipboard_cut (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *parameter)
{
  GtkText *self = GTK_TEXT (widget);
  g_signal_emit_by_name (self, "cut-clipboard");
  hide_selection_bubble (self);
}

static void
gtk_text_activate_clipboard_copy (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *parameter)
{
  GtkText *self = GTK_TEXT (widget);
  g_signal_emit_by_name (self, "copy-clipboard");
  hide_selection_bubble (self);
}

static void
gtk_text_activate_clipboard_paste (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *parameter)
{
  GtkText *self = GTK_TEXT (widget);
  g_signal_emit_by_name (self, "paste-clipboard");
  hide_selection_bubble (self);
}

static void
gtk_text_activate_selection_delete (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *parameter)
{
  GtkText *self = GTK_TEXT (widget);
  gtk_text_delete_cb (self);
  hide_selection_bubble (self);
}

static void
gtk_text_activate_selection_select_all (GtkWidget  *widget,
                                        const char *action_name,
                                        GVariant   *parameter)
{
  GtkText *self = GTK_TEXT (widget);
  gtk_text_select_all (self);
}

static void
gtk_text_activate_misc_insert_emoji (GtkWidget  *widget,
                                     const char *action_name,
                                     GVariant   *parameter)
{
  GtkText *self = GTK_TEXT (widget);
  gtk_text_insert_emoji (self);
  hide_selection_bubble (self);
}

static void
gtk_text_update_clipboard_actions (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  DisplayMode mode;
  GdkClipboard *clipboard;
  gboolean has_clipboard;
  gboolean has_selection;
  gboolean has_content;
  gboolean visible;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));
  mode = gtk_text_get_display_mode (self);
  has_clipboard = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), G_TYPE_STRING);
  has_selection = priv->current_pos != priv->selection_bound;
  has_content = priv->buffer && (gtk_entry_buffer_get_length (priv->buffer) > 0);
  visible = mode == DISPLAY_NORMAL;

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.cut",
                                 visible && priv->editable && has_selection);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.copy",
                                 visible && has_selection);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.paste",
                                 priv->editable && has_clipboard);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "selection.delete",
                                 priv->editable && has_selection);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "selection.select-all",
                                 has_content);
}

static void
gtk_text_update_emoji_action (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "misc.insert-emoji",
                                 priv->editable &&
                                 (gtk_text_get_input_hints (self) & GTK_INPUT_HINT_NO_EMOJI) == 0);
}

static GMenuModel *
gtk_text_get_menu_model (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
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

static gboolean
gtk_text_mnemonic_activate (GtkWidget *widget,
                            gboolean   group_cycling)
{
  gtk_widget_grab_focus (widget);
  return GDK_EVENT_STOP;
}

static void
gtk_text_popup_menu (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *parameters)
{
  gtk_text_do_popup (GTK_TEXT (widget), -1, -1);
}

static void
show_or_hide_handles (GtkWidget  *popover,
                      GParamSpec *pspec,
                      GtkText    *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  gboolean visible;

  visible = gtk_widget_get_visible (popover);
  priv->text_handles_enabled = !visible;
  gtk_text_update_handles (self);
}

static void
append_bubble_item (GtkText    *self,
                    GtkWidget  *toolbar,
                    GMenuModel *model,
                    int         index)
{
  GtkActionMuxer *muxer;
  GtkWidget *item, *image;
  GVariant *att;
  const char *icon_name;
  const char *action_name;
  GMenuModel *link;
  gboolean enabled;

  link = g_menu_model_get_item_link (model, index, "section");
  if (link)
    {
      int i;
      for (i = 0; i < g_menu_model_get_n_items (link); i++)
        append_bubble_item (self, toolbar, link, i);
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

  muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (self), FALSE);
  if (!gtk_action_muxer_query_action (muxer, action_name, &enabled,
                                      NULL, NULL, NULL, NULL) ||
      !enabled)
    return;

  item = gtk_button_new ();
  gtk_widget_set_focus_on_click (item, FALSE);
  image = gtk_image_new_from_icon_name (icon_name);
  gtk_button_set_child (GTK_BUTTON (item), image);
  gtk_widget_add_css_class (item, "image-button");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (item), action_name);
  gtk_box_append (GTK_BOX (toolbar), item);
}

static gboolean
gtk_text_selection_bubble_popup_show (gpointer user_data)
{
  GtkText *self = user_data;
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  const int text_width = gtk_widget_get_width (GTK_WIDGET (self));
  const int text_height = gtk_widget_get_height (GTK_WIDGET (self));
  cairo_rectangle_int_t rect;
  graphene_point_t p;
  gboolean has_selection;
  int start_x, end_x;
  GtkWidget *box;
  GtkWidget *toolbar;
  GMenuModel *model;
  int i;

  gtk_text_update_clipboard_actions (self);

  has_selection = priv->selection_bound != priv->current_pos;

  if (!has_selection && !priv->editable)
    {
      priv->selection_bubble_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  g_clear_pointer (&priv->selection_bubble, gtk_widget_unparent);

  priv->selection_bubble = gtk_popover_new ();
  gtk_widget_set_parent (priv->selection_bubble, GTK_WIDGET (self));
  gtk_widget_add_css_class (priv->selection_bubble, "touch-selection");
  gtk_popover_set_position (GTK_POPOVER (priv->selection_bubble), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (priv->selection_bubble), FALSE);
  g_signal_connect (priv->selection_bubble, "notify::visible",
                    G_CALLBACK (show_or_hide_handles), self);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_margin_start (box, 10);
  gtk_widget_set_margin_end (box, 10);
  gtk_widget_set_margin_top (box, 10);
  gtk_widget_set_margin_bottom (box, 10);
  toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (toolbar, "linked");
  gtk_popover_set_child (GTK_POPOVER (priv->selection_bubble), box);
  gtk_box_append (GTK_BOX (box), toolbar);

  model = gtk_text_get_menu_model (self);

  for (i = 0; i < g_menu_model_get_n_items (model); i++)
    append_bubble_item (self, toolbar, model, i);

  g_object_unref (model);

  if (!gtk_widget_compute_point (GTK_WIDGET (self), gtk_widget_get_parent (GTK_WIDGET (self)),
                                 &GRAPHENE_POINT_INIT (0, 0), &p))
    graphene_point_init (&p, 0, 0);

  gtk_text_get_cursor_locations (self, &start_x, NULL);

  start_x -= priv->scroll_offset;
  start_x = CLAMP (start_x, 0, text_width);
  rect.y = - p.y;
  rect.height = text_height;

  if (has_selection)
    {
      end_x = gtk_text_get_selection_bound_location (self) - priv->scroll_offset;
      end_x = CLAMP (end_x, 0, text_width);

      rect.x = - p.x + MIN (start_x, end_x);
      rect.width = ABS (end_x - start_x);
    }
  else
    {
      rect.x = - p.x + start_x;
      rect.width = 0;
    }

  rect.x -= 5;
  rect.y -= 5;
  rect.width += 10;
  rect.height += 10;

  gtk_popover_set_pointing_to (GTK_POPOVER (priv->selection_bubble), &rect);
  gtk_popover_popup (GTK_POPOVER (priv->selection_bubble));

  priv->selection_bubble_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
gtk_text_selection_bubble_popup_unset (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bubble)
    gtk_widget_set_visible (priv->selection_bubble, FALSE);

  if (priv->selection_bubble_timeout_id)
    {
      g_source_remove (priv->selection_bubble_timeout_id);
      priv->selection_bubble_timeout_id = 0;
    }
}

static void
gtk_text_selection_bubble_popup_set (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->selection_bubble_timeout_id)
    g_source_remove (priv->selection_bubble_timeout_id);

  priv->selection_bubble_timeout_id =
    g_timeout_add (50, gtk_text_selection_bubble_popup_show, self);
  gdk_source_set_static_name_by_id (priv->selection_bubble_timeout_id, "[gtk] gtk_text_selection_bubble_popup_cb");
}

static void
gtk_text_drag_leave (GtkDropTarget *dest,
                     GtkText       *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);

  priv->dnd_position = -1;
  gtk_widget_queue_draw (widget);
}

static gboolean
gtk_text_drag_drop (GtkDropTarget *dest,
                    const GValue  *value,
                    double         x,
                    double         y,
                    GtkText       *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int drop_position;
  int length;
  const char *str;

  if (!priv->editable)
    return FALSE;

  drop_position = gtk_text_find_position (self, x + priv->scroll_offset);

  str = g_value_get_string (value);
  if (str == NULL)
    str = "";

  if (priv->truncate_multiline)
    length = truncate_multiline (str);
  else
    length = -1;

  if (priv->selection_bound == priv->current_pos ||
      drop_position < priv->selection_bound ||
      drop_position > priv->current_pos)
    {
      gtk_editable_insert_text (GTK_EDITABLE (self), str, length, &drop_position);
      gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                           GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                           drop_position, drop_position + length);
    }
  else
    {
      int pos;
      /* Replacing selection */
      begin_change (self);
      gtk_text_delete_selection (self);
      pos = MIN (priv->selection_bound, priv->current_pos);
      gtk_editable_insert_text (GTK_EDITABLE (self), str, length, &pos);
      gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                           GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                           pos, pos + length);
      end_change (self);
    }

  return TRUE;
}

static gboolean
gtk_text_drag_accept (GtkDropTarget *dest,
                      GdkDrop       *drop,
                      GtkText       *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (!priv->editable)
    return FALSE;

  if ((gdk_drop_get_actions (drop) & gtk_drop_target_get_actions (dest)) == 0)
    return FALSE;

  return gdk_content_formats_match (gtk_drop_target_get_formats (dest), gdk_drop_get_formats (drop));
}

static GdkDragAction
gtk_text_drag_motion (GtkDropTarget *target,
                      double         x,
                      double         y,
                      GtkText       *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  int new_position, old_position;

  if (!priv->editable)
    {
      gtk_drop_target_reject (target);
      return 0;
    }

  old_position = priv->dnd_position;
  new_position = gtk_text_find_position (self, x + priv->scroll_offset);

  if (priv->selection_bound == priv->current_pos ||
      new_position < priv->selection_bound ||
      new_position > priv->current_pos)
    {
      priv->dnd_position = new_position;
    }
  else
    {
      priv->dnd_position = -1;
    }

  if (priv->dnd_position != old_position)
    gtk_widget_queue_draw (GTK_WIDGET (self));

  if (priv->drag)
    return GDK_ACTION_MOVE;
  else
    return GDK_ACTION_COPY;
}

/* We display the cursor when
 *
 *  - the selection is empty, AND
 *  - the widget has focus
 */

#define CURSOR_ON_MULTIPLIER 2
#define CURSOR_OFF_MULTIPLIER 1
#define CURSOR_PEND_MULTIPLIER 3
#define CURSOR_DIVIDER 3

static gboolean
cursor_blinks (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

  if (gtk_widget_get_mapped (GTK_WIDGET (self)) &&
      gtk_window_is_active (GTK_WINDOW (root)) &&
      gtk_event_controller_focus_is_focus (GTK_EVENT_CONTROLLER_FOCUS (priv->focus_controller)) &&
      priv->editable &&
      priv->selection_bound == priv->current_pos)
    {
      GtkSettings *settings;
      gboolean blink;

      settings = gtk_widget_get_settings (GTK_WIDGET (self));
      g_object_get (settings, "gtk-cursor-blink", &blink, NULL);

      return blink;
    }
  else
    return FALSE;
}

static gboolean
get_middle_click_paste (GtkText *self)
{
  GtkSettings *settings;
  gboolean paste;

  settings = gtk_widget_get_settings (GTK_WIDGET (self));
  g_object_get (settings, "gtk-enable-primary-paste", &paste, NULL);

  return paste;
}

static int
get_cursor_time (GtkText *self)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (self));
  int time;

  g_object_get (settings, "gtk-cursor-blink-time", &time, NULL);

  return time;
}

static int
get_cursor_blink_timeout (GtkText *self)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (self));
  int timeout;

  g_object_get (settings, "gtk-cursor-blink-timeout", &timeout, NULL);

  return timeout;
}

typedef struct {
  guint64 start;
  guint64 end;
} BlinkData;

static gboolean blink_cb (GtkWidget     *widget,
                          GdkFrameClock *clock,
                          gpointer       user_data);

static void
add_blink_timeout (GtkText  *self,
                   gboolean  delay)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
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
remove_blink_timeout (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->blink_tick)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), priv->blink_tick);
      priv->blink_tick = 0;
    }
}

/*
 * Blink!
 */

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
  GtkText *self = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  BlinkData *data = user_data;
  int blink_timeout;
  int blink_time;
  guint64 now;
  float phase;
  float alpha;

  if (!gtk_widget_has_focus (GTK_WIDGET (self)))
    {
      g_warning ("GtkText - did not receive a focus-out event.\n"
                 "If you handle this event, you must return\n"
                 "GDK_EVENT_PROPAGATE so the default handler\n"
                 "gets the event as well");

      gtk_text_check_cursor_blink (self);
      return G_SOURCE_REMOVE;
    }

  if (priv->selection_bound != priv->current_pos)
    {
      g_warning ("GtkText - unexpected blinking selection. Removing");

      gtk_text_check_cursor_blink (self);
      return G_SOURCE_REMOVE;
    }

  blink_timeout = get_cursor_blink_timeout (self);
  blink_time = get_cursor_time (self);

  now = g_get_monotonic_time ();

  if (now > priv->blink_start_time + blink_timeout * 1000000)
    {
      /* we've blinked enough without the user doing anything, stop blinking */
      priv->cursor_alpha = 1.0;
      remove_blink_timeout (self);
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
gtk_text_check_cursor_blink (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (cursor_blinks (self))
    {
      if (!priv->blink_tick)
        add_blink_timeout (self, FALSE);
    }
  else
    {
      if (priv->blink_tick)
        remove_blink_timeout (self);
    }
}

static void
gtk_text_pend_cursor_blink (GtkText *self)
{
  if (cursor_blinks (self))
    {
      remove_blink_timeout (self);
      add_blink_timeout (self, TRUE);
    }
}

static void
gtk_text_reset_blink_time (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  priv->blink_start_time = g_get_monotonic_time ();
}

/**
 * gtk_text_set_placeholder_text: (attributes org.gtk.Method.set_property=placeholder-text)
 * @self: a `GtkText`
 * @text: (nullable): a string to be displayed when @self
 *   is empty and unfocused
 *
 * Sets text to be displayed in @self when it is empty.
 *
 * This can be used to give a visual hint of the expected
 * contents of the `GtkText`.
 */
void
gtk_text_set_placeholder_text (GtkText    *self,
                               const char *text)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->placeholder == NULL)
    {
      priv->placeholder = g_object_new (GTK_TYPE_LABEL,
                                        "label", text,
                                        "css-name", "placeholder",
                                        "xalign", priv->xalign,
                                        "ellipsize", PANGO_ELLIPSIZE_END,
                                        "max-width-chars", 3,
                                        NULL);
      gtk_label_set_attributes (GTK_LABEL (priv->placeholder), priv->attrs);
      gtk_widget_insert_after (priv->placeholder, GTK_WIDGET (self), NULL);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (priv->placeholder), text);
    }

  update_placeholder_visibility (self);

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_PLACEHOLDER_TEXT]);
}

/**
 * gtk_text_get_placeholder_text: (attributes org.gtk.Method.get_property=placeholder-text)
 * @self: a `GtkText`
 *
 * Retrieves the text that will be displayed when
 * @self is empty and unfocused
 *
 * If no placeholder text has been set, %NULL will be returned.
 *
 * Returns: (nullable) (transfer none): the placeholder text
 */
const char *
gtk_text_get_placeholder_text (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  if (!priv->placeholder)
    return NULL;

  return gtk_label_get_text (GTK_LABEL (priv->placeholder));
}

/**
 * gtk_text_set_input_purpose: (attributes org.gtk.Method.set_property=input-purpose)
 * @self: a `GtkText`
 * @purpose: the purpose
 *
 * Sets the input purpose of the `GtkText`.
 *
 * This can be used by on-screen keyboards and other
 * input methods to adjust their behaviour.
 */
void
gtk_text_set_input_purpose (GtkText         *self,
                            GtkInputPurpose  purpose)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (gtk_text_get_input_purpose (self) != purpose)
    {
      g_object_set (G_OBJECT (priv->im_context),
                    "input-purpose", purpose,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INPUT_PURPOSE]);
    }
}

/**
 * gtk_text_get_input_purpose: (attributes org.gtk.Method.get_property=input-purpose)
 * @self: a `GtkText`
 *
 * Gets the input purpose of the `GtkText`.
 */
GtkInputPurpose
gtk_text_get_input_purpose (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkInputPurpose purpose;

  g_return_val_if_fail (GTK_IS_TEXT (self), GTK_INPUT_PURPOSE_FREE_FORM);

  g_object_get (G_OBJECT (priv->im_context),
                "input-purpose", &purpose,
                NULL);

  return purpose;
}

/**
 * gtk_text_set_input_hints: (attributes org.gtk.Method.set_property=input-hints)
 * @self: a `GtkText`
 * @hints: the hints
 *
 * Sets input hints that allow input methods
 * to fine-tune their behaviour.
 */
void
gtk_text_set_input_hints (GtkText       *self,
                          GtkInputHints  hints)

{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (gtk_text_get_input_hints (self) != hints)
    {
      g_object_set (G_OBJECT (priv->im_context),
                    "input-hints", hints,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_INPUT_HINTS]);
      gtk_text_update_emoji_action (self);
    }
}

/**
 * gtk_text_get_input_hints: (attributes org.gtk.Method.get_property=input-hints)
 * @self: a `GtkText`
 *
 * Gets the input hints of the `GtkText`.
 */
GtkInputHints
gtk_text_get_input_hints (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);
  GtkInputHints hints;

  g_return_val_if_fail (GTK_IS_TEXT (self), GTK_INPUT_HINT_NONE);

  g_object_get (G_OBJECT (priv->im_context),
                "input-hints", &hints,
                NULL);

  return hints;
}

/**
 * gtk_text_set_attributes: (attributes org.gtk.Method.set_property=attributes)
 * @self: a `GtkText`
 * @attrs: (nullable): a `PangoAttrList`
 *
 * Sets attributes that are applied to the text.
 */
void
gtk_text_set_attributes (GtkText       *self,
                         PangoAttrList *attrs)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (attrs)
    pango_attr_list_ref (attrs);

  if (priv->attrs)
    pango_attr_list_unref (priv->attrs);
  priv->attrs = attrs;

  if (priv->placeholder)
    gtk_label_set_attributes (GTK_LABEL (priv->placeholder), attrs);

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_ATTRIBUTES]);

  gtk_text_recompute (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_text_get_attributes: (attributes org.gtk.Method.get_property=attributes)
 * @self: a `GtkText`
 *
 * Gets the attribute list that was set on the `GtkText`.
 *
 * See [method@Gtk.Text.set_attributes].
 *
 * Returns: (transfer none) (nullable): the attribute list
 */
PangoAttrList *
gtk_text_get_attributes (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  return priv->attrs;
}

/**
 * gtk_text_set_tabs: (attributes org.gtk.Method.set_property=tabs)
 * @self: a `GtkText`
 * @tabs: (nullable): a `PangoTabArray`
 *
 * Sets tabstops that are applied to the text.
 */
void
gtk_text_set_tabs (GtkText       *self,
                   PangoTabArray *tabs)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->tabs)
    pango_tab_array_free(priv->tabs);

  if (tabs)
    priv->tabs = pango_tab_array_copy (tabs);
  else
    priv->tabs = NULL;

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_TABS]);

  gtk_text_recompute (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * gtk_text_get_tabs: (attributes org.gtk.Method.get_property=tabs)
 * @self: a `GtkText`
 *
 * Gets the tabstops that were set on the `GtkText`.
 *
 * See [method@Gtk.Text.set_tabs].
 *
 * Returns: (nullable) (transfer none): the tabstops
 */
PangoTabArray *
gtk_text_get_tabs (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  return priv->tabs;
}

static void
emoji_picked (GtkEmojiChooser *chooser,
              const char      *text,
              GtkText         *self)
{
  int pos;

  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  begin_change (self);
  if (priv->selection_bound != priv->current_pos)
    gtk_text_delete_selection (self);

  pos = priv->current_pos;
  gtk_editable_insert_text (GTK_EDITABLE (self), text, -1, &pos);
  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (self),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                       pos, pos + 1);
  gtk_text_set_selection_bounds (self, pos, pos);
  end_change (self);
}

static void
gtk_text_insert_emoji (GtkText *self)
{
  GtkWidget *chooser;

  if (gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_EMOJI_CHOOSER) != NULL)
    return;

  chooser = GTK_WIDGET (g_object_get_data (G_OBJECT (self), "gtk-emoji-chooser"));
  if (!chooser)
    {
      chooser = gtk_emoji_chooser_new ();
      g_object_set_data (G_OBJECT (self), "gtk-emoji-chooser", chooser);

      gtk_widget_set_parent (chooser, GTK_WIDGET (self));
      g_signal_connect (chooser, "emoji-picked", G_CALLBACK (emoji_picked), self);
      g_signal_connect_swapped (chooser, "hide", G_CALLBACK (gtk_text_grab_focus_without_selecting), self);
    }

  gtk_popover_popup (GTK_POPOVER (chooser));
}

static void
set_text_cursor (GtkWidget *widget)
{
  gtk_widget_set_cursor_from_name (widget, "text");
}

GtkEventController *
gtk_text_get_key_controller (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  return priv->key_controller;
}

/**
 * gtk_text_set_extra_menu: (attributes org.gtk.Method.set_property=extra-menu)
 * @self: a `GtkText`
 * @model: (nullable): a `GMenuModel`
 *
 * Sets a menu model to add when constructing
 * the context menu for @self.
 */
void
gtk_text_set_extra_menu (GtkText    *self,
                         GMenuModel *model)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (g_set_object (&priv->extra_menu, model))
    {
      g_clear_pointer (&priv->popup_menu, gtk_widget_unparent);

      g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_EXTRA_MENU]);
    }
}

/**
 * gtk_text_get_extra_menu: (attributes org.gtk.Method.get_property=extra-menu)
 * @self: a `GtkText`
 *
 * Gets the menu model for extra items in the context menu.
 *
 * See [method@Gtk.Text.set_extra_menu].
 *
 * Returns: (transfer none) (nullable): the menu model
 */
GMenuModel *
gtk_text_get_extra_menu (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), NULL);

  return priv->extra_menu;
}

/**
 * gtk_text_set_enable_emoji_completion: (attributes org.gtk.Method.set_property=enable-emoji-completion)
 * @self: a `GtkText`
 * @enable_emoji_completion: %TRUE to enable Emoji completion
 *
 * Sets whether Emoji completion is enabled.
 *
 * If it is, typing ':', followed by a recognized keyword,
 * will pop up a window with suggested Emojis matching the
 * keyword.
 */
void
gtk_text_set_enable_emoji_completion (GtkText  *self,
                                      gboolean  enable_emoji_completion)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->enable_emoji_completion == enable_emoji_completion)
    return;

  priv->enable_emoji_completion = enable_emoji_completion;

  if (priv->enable_emoji_completion)
    priv->emoji_completion = gtk_emoji_completion_new (self);
  else
    g_clear_pointer (&priv->emoji_completion, gtk_widget_unparent);

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_ENABLE_EMOJI_COMPLETION]);
}

/**
 * gtk_text_get_enable_emoji_completion: (attributes org.gtk.Method.get_property=enable-emoji-completion)
 * @self: a `GtkText`
 *
 * Returns whether Emoji completion is enabled for this
 * `GtkText` widget.
 *
 * Returns: %TRUE if Emoji completion is enabled
 */
gboolean
gtk_text_get_enable_emoji_completion (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->enable_emoji_completion;
}

/**
 * gtk_text_set_propagate_text_width: (attributes org.gtk.Method.set_property=propagate-text-width)
 * @self: a `GtkText`
 * @propagate_text_width: %TRUE to propagate the text width
 *
 * Sets whether the `GtkText` should grow and shrink with the content.
 */
void
gtk_text_set_propagate_text_width (GtkText  *self,
                                   gboolean  propagate_text_width)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->propagate_text_width == propagate_text_width)
    return;

  priv->propagate_text_width = propagate_text_width;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_PROPAGATE_TEXT_WIDTH]);
}

/**
 * gtk_text_get_propagate_text_width: (attributes org.gtk.Method.get_property=propagate-text-width)
 * @self: a `GtkText`
 *
 * Returns whether the `GtkText` will grow and shrink
 * with the content.
 *
 * Returns: %TRUE if @self will propagate the text width
 */
gboolean
gtk_text_get_propagate_text_width (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->propagate_text_width;
}

/**
 * gtk_text_set_truncate_multiline: (attributes org.gtk.Method.set_property=truncate-multiline)
 * @self: a `GtkText`
 * @truncate_multiline: %TRUE to truncate multi-line text
 *
 * Sets whether the `GtkText` should truncate multi-line text
 * that is pasted into the widget.
 */
void
gtk_text_set_truncate_multiline (GtkText  *self,
                                 gboolean  truncate_multiline)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_if_fail (GTK_IS_TEXT (self));

  if (priv->truncate_multiline == truncate_multiline)
    return;

  priv->truncate_multiline = truncate_multiline;

  g_object_notify_by_pspec (G_OBJECT (self), text_props[PROP_TRUNCATE_MULTILINE]);
}

/**
 * gtk_text_get_truncate_multiline: (attributes org.gtk.Method.get_property=truncate-multiline)
 * @self: a `GtkText`
 *
 * Returns whether the `GtkText` will truncate multi-line text
 * that is pasted into the widget
 *
 * Returns: %TRUE if @self will truncate multi-line text
 */
gboolean
gtk_text_get_truncate_multiline (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TEXT (self), FALSE);

  return priv->truncate_multiline;
}

/**
 * gtk_text_compute_cursor_extents:
 * @self: a `GtkText`
 * @position: the character position
 * @strong: (out) (optional): location to store the strong cursor position
 * @weak: (out) (optional): location to store the weak cursor position
 *
 * Determine the positions of the strong and weak cursors if the
 * insertion point in the layout is at @position.
 *
 * The position of each cursor is stored as a zero-width rectangle.
 * The strong cursor location is the location where characters of
 * the directionality equal to the base direction are inserted.
 * The weak cursor location is the location where characters of
 * the directionality opposite to the base direction are inserted.
 *
 * The rectangle positions are in widget coordinates.
 *
 * Since: 4.4
 */
void
gtk_text_compute_cursor_extents (GtkText         *self,
                                 gsize            position,
                                 graphene_rect_t *strong,
                                 graphene_rect_t *weak)
{
  PangoLayout *layout;
  PangoRectangle pango_strong_pos;
  PangoRectangle pango_weak_pos;
  int offset_x, offset_y, index;
  const char *text;

  g_return_if_fail (GTK_IS_TEXT (self));

  layout = gtk_text_ensure_layout (self, TRUE);
  text = pango_layout_get_text (layout);
  position = CLAMP (position, 0, g_utf8_strlen (text, -1));
  index = g_utf8_offset_to_pointer (text, position) - text;

  pango_layout_get_cursor_pos (layout, index,
                               strong ? &pango_strong_pos : NULL,
                               weak ? &pango_weak_pos : NULL);
  gtk_text_get_layout_offsets (self, &offset_x, &offset_y);

  if (strong)
    {
      graphene_rect_init (strong,
                          offset_x + pango_strong_pos.x / PANGO_SCALE,
                          offset_y + pango_strong_pos.y / PANGO_SCALE,
                          0,
                          pango_strong_pos.height / PANGO_SCALE);
    }

  if (weak)
    {
      graphene_rect_init (weak,
                          offset_x + pango_weak_pos.x / PANGO_SCALE,
                          offset_y + pango_weak_pos.y / PANGO_SCALE,
                          0,
                          pango_weak_pos.height / PANGO_SCALE);
    }
}

static void
gtk_text_real_undo (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameters)
{
  GtkText *text = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (text);

  gtk_text_history_undo (priv->history);
}

static void
gtk_text_real_redo (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameters)
{
  GtkText *text = GTK_TEXT (widget);
  GtkTextPrivate *priv = gtk_text_get_instance_private (text);

  gtk_text_history_redo (priv->history);
}

static void
gtk_text_history_change_state_cb (gpointer funcs_data,
                                  gboolean is_modified,
                                  gboolean can_undo,
                                  gboolean can_redo)
{
  /* Do nothing */
}

static void
gtk_text_history_insert_cb (gpointer    funcs_data,
                            guint       begin,
                            guint       end,
                            const char *str,
                            guint       len)
{
  GtkText *text = funcs_data;
  int location = begin;

  gtk_editable_insert_text (GTK_EDITABLE (text), str, len, &location);
  gtk_accessible_text_update_contents (GTK_ACCESSIBLE_TEXT (text),
                                       GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT,
                                       location, location + len);
}

static void
gtk_text_history_delete_cb (gpointer    funcs_data,
                            guint       begin,
                            guint       end,
                            const char *expected_text,
                            guint       len)
{
  GtkText *text = funcs_data;

  gtk_editable_delete_text (GTK_EDITABLE (text), begin, end);
}

static void
gtk_text_history_select_cb (gpointer funcs_data,
                            int      selection_insert,
                            int      selection_bound)
{
  GtkText *text = funcs_data;

  gtk_editable_select_region (GTK_EDITABLE (text),
                              selection_insert,
                              selection_bound);
}

static void
gtk_text_set_enable_undo (GtkText  *self,
                          gboolean  enable_undo)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  if (priv->enable_undo == enable_undo)
    return;

  priv->enable_undo = enable_undo;
  gtk_text_update_history (self);
  g_object_notify (G_OBJECT (self), "enable-undo");
}

static void
gtk_text_update_history (GtkText *self)
{
  GtkTextPrivate *priv = gtk_text_get_instance_private (self);

  gtk_text_history_set_enabled (priv->history,
                                priv->enable_undo &&
                                priv->visible &&
                                priv->editable);
}

/* {{{ GtkAccessibleText implementation */

static GBytes *
gtk_text_accessible_text_get_contents (GtkAccessibleText *self,
                                       unsigned int       start,
                                       unsigned int       end)
{
  const char *text;
  int len;
  char *string;
  gsize size;

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  len = g_utf8_strlen (text, -1);

  start = CLAMP (start, 0, len);
  end = CLAMP (end, 0, len);

  if (end <= start)
    {
      string = g_strdup ("");
      size = 1;
    }
  else
    {
      const char *p, *q;
      p = g_utf8_offset_to_pointer (text, start);
      q = g_utf8_offset_to_pointer (text, end);
      size = q - p + 1;
      string = g_strndup (p, q - p);
    }

  return g_bytes_new_take (string, size);
}

static GBytes *
gtk_text_accessible_text_get_contents_at (GtkAccessibleText            *self,
                                          unsigned int                  offset,
                                          GtkAccessibleTextGranularity  granularity,
                                          unsigned int                 *start,
                                          unsigned int                 *end)
{
  PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (self));
  char *string = gtk_pango_get_string_at (layout, offset, granularity, start, end);

  return g_bytes_new_take (string, strlen (string));
}

static unsigned int
gtk_text_accessible_text_get_caret_position (GtkAccessibleText *self)
{
  return gtk_editable_get_position (GTK_EDITABLE (self));
}

static gboolean
gtk_text_accessible_text_get_selection (GtkAccessibleText       *self,
                                        gsize                   *n_ranges,
                                        GtkAccessibleTextRange **ranges)
{
  int start, end;

  if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (self), &start, &end))
    return FALSE;

  *n_ranges = 1;
  *ranges = g_new (GtkAccessibleTextRange, 1);
  (*ranges)[0].start = start;
  (*ranges)[0].length = end - start;

  return TRUE;
}

static gboolean
gtk_text_accessible_text_get_attributes (GtkAccessibleText        *self,
                                         unsigned int              offset,
                                         gsize                    *n_ranges,
                                         GtkAccessibleTextRange  **ranges,
                                         char                   ***attribute_names,
                                         char                   ***attribute_values)
{
  PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (self));
  unsigned int start, end;
  char **names, **values;

  gtk_pango_get_run_attributes (layout, offset, &names, &values, &start, &end);

  *n_ranges = g_strv_length (names);
  *ranges = g_new (GtkAccessibleTextRange, *n_ranges);

  for (unsigned i = 0; i < *n_ranges; i++)
    {
      GtkAccessibleTextRange *range = &(*ranges)[i];

      range->start = start;
      range->length = end - start;
    }

  *attribute_names = names;
  *attribute_values = values;

  return TRUE;
}

static void
gtk_text_accessible_text_get_default_attributes (GtkAccessibleText   *self,
                                                 char              ***attribute_names,
                                                 char              ***attribute_values)
{
  PangoLayout *layout = gtk_text_get_layout (GTK_TEXT (self));
  char **names, **values;

  gtk_pango_get_default_attributes (layout, &names, &values);

  *attribute_names = names;
  *attribute_values = values;
}

static void
gtk_text_accessible_text_init (GtkAccessibleTextInterface *iface)
{
  iface->get_contents = gtk_text_accessible_text_get_contents;
  iface->get_contents_at = gtk_text_accessible_text_get_contents_at;
  iface->get_caret_position = gtk_text_accessible_text_get_caret_position;
  iface->get_selection = gtk_text_accessible_text_get_selection;
  iface->get_attributes = gtk_text_accessible_text_get_attributes;
  iface->get_default_attributes = gtk_text_accessible_text_get_default_attributes;
}

/* vim:set foldmethod=marker expandtab: */
