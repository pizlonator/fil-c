/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Red Hat, Inc.
 * Copyright (C) 2018 Purism SPC
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

#include <string.h>
#include <wayland-client-protocol.h>

#include "gtk/gtkdragsourceprivate.h"
#include "gtk/gtkimcontextwayland.h"
#include "gtk/gtkimmoduleprivate.h"

#include "gdk/wayland/gdkwayland.h"
#include "text-input-unstable-v3-client-protocol.h"

typedef struct _GtkIMContextWaylandGlobal GtkIMContextWaylandGlobal;
typedef struct _GtkIMContextWayland GtkIMContextWayland;
typedef struct _GtkIMContextWaylandClass GtkIMContextWaylandClass;

struct _GtkIMContextWaylandGlobal
{
  struct wl_display *display;
  struct wl_registry *registry;
  uint32_t text_input_manager_wl_id;
  struct zwp_text_input_manager_v3 *text_input_manager;
  struct zwp_text_input_v3 *text_input;

  GtkIMContext *current;

  /* The input-method.enter event may happen before or after GTK focus-in,
   * so the context may not exist at the time. Same for leave and focus-out. */
  gboolean focused;

  guint serial;
  guint done_serial;
};

struct _GtkIMContextWaylandClass
{
  GtkIMContextSimpleClass parent_class;
};

struct preedit {
  char *text;
  int cursor_begin;
  int cursor_end;
};

struct surrounding_delete {
  guint before_length;
  guint after_length;
};

struct _GtkIMContextWayland
{
  GtkIMContextSimple parent_instance;
  GtkWidget *widget;

  struct {
    char *text;
    int cursor_idx;
    int anchor_idx;
  } surrounding;

  enum zwp_text_input_v3_change_cause surrounding_change;

  struct surrounding_delete pending_surrounding_delete;

  struct preedit current_preedit;
  struct preedit pending_preedit;

  char *pending_commit;

  cairo_rectangle_int_t cursor_rect;
  guint use_preedit : 1;
};

static void gtk_im_context_wayland_focus_out (GtkIMContext *context);

static void commit_state (GtkIMContextWayland *context);
static void notify_surrounding_text (GtkIMContextWayland *context);
static void notify_cursor_location (GtkIMContextWayland *context);
static void notify_content_type (GtkIMContextWayland *context);

G_DEFINE_TYPE_WITH_CODE (GtkIMContextWayland, gtk_im_context_wayland, GTK_TYPE_IM_CONTEXT_SIMPLE,
			 gtk_im_module_ensure_extension_point ();
                         g_io_extension_point_implement (GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "wayland",
                                                         100));

#define GTK_IM_CONTEXT_WAYLAND(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), gtk_im_context_wayland_get_type (), GtkIMContextWayland))

static GtkIMContextWaylandGlobal *
gtk_im_context_wayland_global_get (GdkDisplay *display);

static GtkIMContextWaylandGlobal *
gtk_im_context_wayland_get_global (GtkIMContextWayland *self)
{
  GtkIMContextWaylandGlobal *global;

  if (self->widget == NULL)
    return NULL;

  global = gtk_im_context_wayland_global_get (gtk_widget_get_display (self->widget));
  if (global->current != GTK_IM_CONTEXT (self))
    return NULL;
  if (global->text_input == NULL)
    return NULL;

  return global;
}

static void
notify_im_change (GtkIMContextWayland                 *context,
                  enum zwp_text_input_v3_change_cause  cause)
{
  GtkIMContextWaylandGlobal *global;
  gboolean result;

  global = gtk_im_context_wayland_get_global (context);
  if (global == NULL)
    return;

  context->surrounding_change = cause;

  g_signal_emit_by_name (global->current, "retrieve-surrounding", &result);
  notify_surrounding_text (context);
  notify_content_type (context);
  notify_cursor_location (context);
  commit_state (context);
}

static void
text_input_preedit (void                     *data,
                    struct zwp_text_input_v3 *text_input,
                    const char               *text,
                    int                       cursor_begin,
                    int                       cursor_end)
{
  GtkIMContextWayland *context;
  GtkIMContextWaylandGlobal *global = data;

  if (!global->current)
    return;

  context = GTK_IM_CONTEXT_WAYLAND (global->current);
      
  g_free (context->pending_preedit.text);
  context->pending_preedit.text = g_strdup (text);
  context->pending_preedit.cursor_begin = cursor_begin;
  context->pending_preedit.cursor_end = cursor_end;
}

static void
text_input_preedit_apply (GtkIMContextWaylandGlobal *global)
{
  GtkIMContextWayland *context;
  gboolean state_change;
  struct preedit defaults = {0};

  if (!global->current)
    return;

  context = GTK_IM_CONTEXT_WAYLAND (global->current);
  if (context->pending_preedit.text == NULL &&
      context->current_preedit.text == NULL)
    return;

  state_change = ((context->pending_preedit.text == NULL)
                 != (context->current_preedit.text == NULL));

  if (state_change && !context->current_preedit.text)
    g_signal_emit_by_name (context, "preedit-start");

  g_free (context->current_preedit.text);
  context->current_preedit = context->pending_preedit;
  context->pending_preedit = defaults;

  g_signal_emit_by_name (context, "preedit-changed");

  if (state_change && !context->current_preedit.text)
    g_signal_emit_by_name (context, "preedit-end");
}

static void
text_input_commit (void                     *data,
                   struct zwp_text_input_v3 *text_input,
                   const char               *text)
{
  GtkIMContextWaylandGlobal *global = data;
  GtkIMContextWayland *context;

  if (!global->current)
      return;
  
  context = GTK_IM_CONTEXT_WAYLAND (global->current);

  g_free (context->pending_commit);
  context->pending_commit = g_strdup (text);
}

static void
text_input_commit_apply (GtkIMContextWaylandGlobal *global)
{
  GtkIMContextWayland *context;
  context = GTK_IM_CONTEXT_WAYLAND (global->current);
  if (context->pending_commit)
    g_signal_emit_by_name (global->current, "commit", context->pending_commit);
  g_free (context->pending_commit);
  context->pending_commit = NULL;
}

static void
text_input_delete_surrounding_text (void                     *data,
                                    struct zwp_text_input_v3 *text_input,
                                    uint32_t                  before_length,
                                    uint32_t                  after_length)
{
  GtkIMContextWaylandGlobal *global = data;
  GtkIMContextWayland *context;
  char *cursor_pointer;
  uint32_t char_before_length;
  uint32_t char_after_length;

  if (!global->current)
      return;
  
  context = GTK_IM_CONTEXT_WAYLAND (global->current);

  /* We already got byte lengths from text_input_v3, but GTK uses char lengths
   * for delete_surrounding, So convert it here.
   */
  cursor_pointer = context->surrounding.text + context->surrounding.cursor_idx;
  char_before_length = g_utf8_pointer_to_offset (cursor_pointer - before_length, cursor_pointer);
  char_after_length = g_utf8_pointer_to_offset (cursor_pointer, cursor_pointer + after_length);

  context->pending_surrounding_delete.before_length = char_before_length;
  context->pending_surrounding_delete.after_length = char_after_length;
}

static void
text_input_delete_surrounding_text_apply (GtkIMContextWaylandGlobal *global)
{
  GtkIMContextWayland *context;
  gboolean retval;
  int len;
  struct surrounding_delete defaults = {0};

  context = GTK_IM_CONTEXT_WAYLAND (global->current);

  len = context->pending_surrounding_delete.after_length
      + context->pending_surrounding_delete.before_length;
  if (len > 0)
    {
      g_signal_emit_by_name (global->current, "delete-surrounding",
                             -context->pending_surrounding_delete.before_length,
                             len, &retval);
      notify_im_change (GTK_IM_CONTEXT_WAYLAND (context),
                        ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
    }

  context->pending_surrounding_delete = defaults;
}

static void
text_input_done (void                     *data,
                 struct zwp_text_input_v3 *text_input,
                 uint32_t                  serial)
{
  GtkIMContextWaylandGlobal *global = data;
  GtkIMContextWayland *context;
  gboolean update_im;

  global->done_serial = serial;

  if (!global->current)
    return;

  context = GTK_IM_CONTEXT_WAYLAND (global->current);
  update_im = (context->pending_commit != NULL ||
               g_strcmp0 (context->pending_preedit.text,
                          context->current_preedit.text) != 0);

  text_input_delete_surrounding_text_apply (global);
  text_input_commit_apply (global);
  text_input_preedit_apply (global);

  if (update_im && global->serial == serial)
    notify_im_change (context, ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
}

static void
notify_surrounding_text (GtkIMContextWayland *context)
{
#define MAX_LEN 4000
  GtkIMContextWaylandGlobal *global;
  const char *start, *end;
  int len, cursor, anchor;
  char *str = NULL;

  if (!context->surrounding.text)
    return;
  global = gtk_im_context_wayland_get_global (context);
  if (global == NULL)
    return;

  len = strlen (context->surrounding.text);
  cursor = context->surrounding.cursor_idx;
  anchor = context->surrounding.anchor_idx;

  /* The protocol specifies a maximum length of 4KiB on transfers,
   * mangle the surrounding text if it's bigger than that, and relocate
   * cursor/anchor locations as per the string being sent.
   */
  if (len > MAX_LEN)
    {
      if (context->surrounding.cursor_idx < MAX_LEN &&
          context->surrounding.anchor_idx < MAX_LEN)
        {
          start = context->surrounding.text;
          end = &context->surrounding.text[MAX_LEN];
        }
      else if (context->surrounding.cursor_idx > len - MAX_LEN &&
               context->surrounding.anchor_idx > len - MAX_LEN)
        {
          start = &context->surrounding.text[len - MAX_LEN];
          end = &context->surrounding.text[len];
        }
      else
        {
          int mid, a, b;
          int cursor_len = ABS (context->surrounding.cursor_idx -
                                context->surrounding.anchor_idx);

          if (cursor_len > MAX_LEN)
            {
              g_warn_if_reached ();
              return;
            }

          mid = MIN (context->surrounding.cursor_idx,
                     context->surrounding.anchor_idx) + (cursor_len / 2);
          a = MAX (0, mid - (MAX_LEN / 2));
          b = MIN (len, mid + (MAX_LEN / 2));

          start = &context->surrounding.text[a];
          end = &context->surrounding.text[b];
        }

      if (start != context->surrounding.text)
        start = g_utf8_next_char (start);
      if (end != &context->surrounding.text[len])
        end = g_utf8_find_prev_char (context->surrounding.text, end);

      cursor -= start - context->surrounding.text;
      anchor -= start - context->surrounding.text;

      str = g_strndup (start, end - start);
    }

  zwp_text_input_v3_set_surrounding_text (global->text_input,
                                          str ? str : context->surrounding.text,
                                          cursor, anchor);
  zwp_text_input_v3_set_text_change_cause (global->text_input,
                                           context->surrounding_change);
  g_free (str);
#undef MAX_LEN
}

static void
notify_cursor_location (GtkIMContextWayland *context)
{
  GtkIMContextWaylandGlobal *global;
  cairo_rectangle_int_t rect;
  double nx, ny;
  graphene_point_t p;

  global = gtk_im_context_wayland_get_global (context);
  if (global == NULL)
    return;

  rect = context->cursor_rect;
  if (!gtk_widget_compute_point (context->widget,
                                 GTK_WIDGET (gtk_widget_get_root (context->widget)),
                                 &GRAPHENE_POINT_INIT (rect.x, rect.y),
                                 &p))
    graphene_point_init (&p, rect.x, rect.y);

  gtk_native_get_surface_transform (gtk_widget_get_native (context->widget), &nx, &ny);

  rect.x = p.x + nx;
  rect.y = p.y + ny;
  zwp_text_input_v3_set_cursor_rectangle (global->text_input,
                                          rect.x, rect.y,
                                          rect.width, rect.height);
}

static uint32_t
translate_hints (GtkInputHints   input_hints,
                 GtkInputPurpose purpose)
{
  uint32_t hints = 0;

  if (input_hints & GTK_INPUT_HINT_SPELLCHECK)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK;
  if (input_hints & GTK_INPUT_HINT_WORD_COMPLETION)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION;
  if (input_hints & GTK_INPUT_HINT_LOWERCASE)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_LOWERCASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_CHARS)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_UPPERCASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_WORDS)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_TITLECASE;
  if (input_hints & GTK_INPUT_HINT_UPPERCASE_SENTENCES)
    hints |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION;

  if (purpose == GTK_INPUT_PURPOSE_PIN ||
      purpose == GTK_INPUT_PURPOSE_PASSWORD)
    {
      hints |= (ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT |
                ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA);
    }

  return hints;
}

static uint32_t
translate_purpose (GtkInputPurpose purpose)
{
  switch (purpose)
    {
    case GTK_INPUT_PURPOSE_FREE_FORM:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    case GTK_INPUT_PURPOSE_ALPHA:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_ALPHA;
    case GTK_INPUT_PURPOSE_DIGITS:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DIGITS;
    case GTK_INPUT_PURPOSE_NUMBER:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER;
    case GTK_INPUT_PURPOSE_PHONE:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE;
    case GTK_INPUT_PURPOSE_URL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL;
    case GTK_INPUT_PURPOSE_EMAIL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL;
    case GTK_INPUT_PURPOSE_NAME:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME;
    case GTK_INPUT_PURPOSE_PASSWORD:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD;
    case GTK_INPUT_PURPOSE_PIN:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PIN;
    case GTK_INPUT_PURPOSE_TERMINAL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL;
    default:
      g_assert_not_reached ();
    }

  return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
}

static void
notify_content_type (GtkIMContextWayland *context)
{
  GtkIMContextWaylandGlobal *global;
  GtkInputHints hints;
  GtkInputPurpose purpose;

  global = gtk_im_context_wayland_get_global (context);
  if (global == NULL)
    return;

  g_object_get (context,
                "input-hints", &hints,
                "input-purpose", &purpose,
                NULL);

  zwp_text_input_v3_set_content_type (global->text_input,
                                      translate_hints (hints, purpose),
                                      translate_purpose (purpose));
}

static void
commit_state (GtkIMContextWayland *context)
{
  GtkIMContextWaylandGlobal *global;

  global = gtk_im_context_wayland_get_global (context);
  if (global == NULL)
    return;

  global->serial++;
  zwp_text_input_v3_commit (global->text_input);
  context->surrounding_change = ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
}

static void
gtk_im_context_wayland_finalize (GObject *object)
{
  GtkIMContextWayland *context = GTK_IM_CONTEXT_WAYLAND (object);

  gtk_im_context_wayland_focus_out (GTK_IM_CONTEXT (context));

  g_clear_object (&context->widget);
  g_free (context->surrounding.text);
  g_free (context->current_preedit.text);
  g_free (context->pending_preedit.text);
  g_free (context->pending_commit);

  G_OBJECT_CLASS (gtk_im_context_wayland_parent_class)->finalize (object);
}

static void
gtk_im_context_wayland_set_client_widget (GtkIMContext *context,
                                          GtkWidget    *widget)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (widget == context_wayland->widget)
    return;

  if (context_wayland->widget)
    gtk_im_context_wayland_focus_out (context);

  g_set_object (&context_wayland->widget, widget);
}

static void
gtk_im_context_wayland_get_preedit_string (GtkIMContext   *context,
                                           char          **str,
                                           PangoAttrList **attrs,
                                           int            *cursor_pos)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);
  const char *preedit_str;

  if (attrs)
    *attrs = NULL;

  GTK_IM_CONTEXT_CLASS (gtk_im_context_wayland_parent_class)->get_preedit_string (context,
                                                                                  str, attrs,
                                                                                  cursor_pos);

  /* If the parent implementation returns a len>0 string, go with it */
  if (str && *str)
    {
      if (**str)
        return;

      g_free (*str);
    }

  preedit_str =
    context_wayland->current_preedit.text ? context_wayland->current_preedit.text : "";

  if (cursor_pos)
    *cursor_pos = g_utf8_strlen (preedit_str,
                                 context_wayland->current_preedit.cursor_begin);

  if (str)
    *str = g_strdup (preedit_str);
  if (attrs)
    {
      PangoAttribute *attr;
      guint len = strlen (preedit_str);

      if (!*attrs)
        *attrs = pango_attr_list_new ();

      attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
      attr->start_index = 0;
      attr->end_index = len;
      pango_attr_list_insert (*attrs, attr);

      /* enable fallback, since IBus will send us things like ⎄ */
      attr = pango_attr_fallback_new (TRUE);
      attr->start_index = 0;
      attr->end_index = len;
      pango_attr_list_insert (*attrs, attr);

      if (context_wayland->current_preedit.cursor_begin
          != context_wayland->current_preedit.cursor_end)
        {
          /* FIXME: Oh noes, how to highlight while taking into account user preferences? */
          PangoAttribute *cursor = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
          cursor->start_index = context_wayland->current_preedit.cursor_begin;
          cursor->end_index = context_wayland->current_preedit.cursor_end;
          pango_attr_list_insert (*attrs, cursor);
        }
    }
}

static gboolean
gtk_im_context_wayland_filter_keypress (GtkIMContext *context,
                                        GdkEvent     *key)
{
  /* This is done by the compositor */
  return GTK_IM_CONTEXT_CLASS (gtk_im_context_wayland_parent_class)->filter_keypress (context, key);
}

static void
enable (GtkIMContextWayland       *context_wayland,
        GtkIMContextWaylandGlobal *global)
{
  zwp_text_input_v3_enable (global->text_input);
  notify_im_change (context_wayland,
                    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
}

static void
disable (GtkIMContextWayland       *context_wayland,
         GtkIMContextWaylandGlobal *global)
{
  zwp_text_input_v3_disable (global->text_input);
  commit_state (context_wayland);

  /* The commit above will still count in the .done event accounting,
   * we should account for it, lest the serial gets out of sync after
   * a future focus_in/enable.
   */
  global->done_serial++;

  /* after disable, incoming state changes won't take effect anyway */
  if (context_wayland->current_preedit.text)
    {
      text_input_preedit (global, global->text_input, NULL, 0, 0);
      text_input_preedit_apply (global);
    }
}

static void
text_input_enter (void                     *data,
                  struct zwp_text_input_v3 *text_input,
                  struct wl_surface        *surface)
{
  GtkIMContextWaylandGlobal *global = data;

  global->focused = TRUE;

  if (global->current)
    enable (GTK_IM_CONTEXT_WAYLAND (global->current), global);
}

static void
text_input_leave (void                     *data,
                  struct zwp_text_input_v3 *text_input,
                  struct wl_surface        *surface)
{
  GtkIMContextWaylandGlobal *global = data;

  global->focused = FALSE;

  if (global->current)
    disable (GTK_IM_CONTEXT_WAYLAND (global->current), global);
}


static const struct zwp_text_input_v3_listener text_input_listener = {
  text_input_enter,
  text_input_leave,
  text_input_preedit,
  text_input_commit,
  text_input_delete_surrounding_text,
  text_input_done,
};

static void
registry_handle_global (void               *data,
                        struct wl_registry *registry,
                        uint32_t            id,
                        const char         *interface,
                        uint32_t            version)
{
  GtkIMContextWaylandGlobal *global = data;
  GdkSeat *seat = gdk_display_get_default_seat (gdk_display_get_default ());

  if (strcmp (interface, "zwp_text_input_manager_v3") == 0)
    {
      global->text_input_manager_wl_id = id;
      global->text_input_manager =
        wl_registry_bind (global->registry, global->text_input_manager_wl_id,
                          &zwp_text_input_manager_v3_interface, 1);
      global->text_input =
        zwp_text_input_manager_v3_get_text_input (global->text_input_manager,
                                                  gdk_wayland_seat_get_wl_seat (seat));
      global->serial = 0;
      zwp_text_input_v3_add_listener (global->text_input,
                                      &text_input_listener, global);
    }
}

static void
registry_handle_global_remove (void               *data,
                               struct wl_registry *registry,
                               uint32_t            id)
{
  GtkIMContextWaylandGlobal *global = data;

  if (id != global->text_input_manager_wl_id)
    return;

  g_clear_pointer (&global->text_input, zwp_text_input_v3_destroy);
  g_clear_pointer (&global->text_input_manager, zwp_text_input_manager_v3_destroy);
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static void
gtk_im_context_wayland_global_free (gpointer data)
{
  GtkIMContextWaylandGlobal *global = data;

  g_free (global);
}

static GtkIMContextWaylandGlobal *
gtk_im_context_wayland_global_get (GdkDisplay *display)
{
  GtkIMContextWaylandGlobal *global;

  global = g_object_get_data (G_OBJECT (display), "gtk-im-context-wayland-global");
  if (global != NULL)
    return global;

  global = g_new0 (GtkIMContextWaylandGlobal, 1);
  global->display = gdk_wayland_display_get_wl_display (display);
  global->registry = wl_display_get_registry (global->display);

  wl_registry_add_listener (global->registry, &registry_listener, global);

  g_object_set_data_full (G_OBJECT (display),
                          "gtk-im-context-wayland-global",
                          global,
                          gtk_im_context_wayland_global_free);

  return global;
}

static void
gtk_im_context_wayland_focus_in (GtkIMContext *context)
{
  GtkIMContextWayland *self = GTK_IM_CONTEXT_WAYLAND (context);
  GtkIMContextWaylandGlobal *global;

  if (self->widget == NULL)
    return;

  global = gtk_im_context_wayland_global_get (gtk_widget_get_display (self->widget));
  if (global->current == context)
    return;

  global->current = context;
  if (!global->text_input)
    return;

  if (global->focused)
    enable (self, global);
}

static void
gtk_im_context_wayland_focus_out (GtkIMContext *context)
{
  GtkIMContextWayland *self = GTK_IM_CONTEXT_WAYLAND (context);
  GtkIMContextWaylandGlobal *global;

  global = gtk_im_context_wayland_get_global (self);
  if (global == NULL)
    return;

  if (global->focused)
    disable (self, global);

  global->current = NULL;
}

static void
gtk_im_context_wayland_reset (GtkIMContext *context)
{
  notify_im_change (GTK_IM_CONTEXT_WAYLAND (context),
                    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);

  GTK_IM_CONTEXT_CLASS (gtk_im_context_wayland_parent_class)->reset (context);
}

static void
gtk_im_context_wayland_set_cursor_location (GtkIMContext *context,
                                            GdkRectangle *rect)
{
  GtkIMContextWayland *context_wayland;

  context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (context_wayland->cursor_rect.x == rect->x &&
      context_wayland->cursor_rect.y == rect->y &&
      context_wayland->cursor_rect.width == rect->width &&
      context_wayland->cursor_rect.height == rect->height)
    return;

  context_wayland->cursor_rect = *rect;
}

static void
gtk_im_context_wayland_set_use_preedit (GtkIMContext *context,
                                        gboolean      use_preedit)
{
  GtkIMContextWayland *context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  context_wayland->use_preedit = !!use_preedit;
}

static void
gtk_im_context_wayland_set_surrounding (GtkIMContext *context,
                                        const char   *text,
                                        int           len,
                                        int           cursor_index,
                                        int           selection_bound)
{
  GtkIMContextWayland *context_wayland;

  context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (context_wayland->surrounding.text && text &&
      (len < 0 || len == strlen (context_wayland->surrounding.text)) &&
      strncmp (context_wayland->surrounding.text, text, len) == 0 &&
      context_wayland->surrounding.cursor_idx == cursor_index &&
      context_wayland->surrounding.anchor_idx == selection_bound)
    return;

  g_free (context_wayland->surrounding.text);
  context_wayland->surrounding.text = g_strndup (text, len);
  context_wayland->surrounding.cursor_idx = cursor_index;
  context_wayland->surrounding.anchor_idx = selection_bound;
}

static gboolean
gtk_im_context_wayland_get_surrounding (GtkIMContext  *context,
                                        char         **text,
                                        int           *cursor_index,
                                        int           *selection_bound)
{
  GtkIMContextWayland *context_wayland;

  context_wayland = GTK_IM_CONTEXT_WAYLAND (context);

  if (!context_wayland->surrounding.text)
    return FALSE;

  *text = context_wayland->surrounding.text;
  *cursor_index = context_wayland->surrounding.cursor_idx;
  *selection_bound = context_wayland->surrounding.anchor_idx;
  return TRUE;
}

static void
gtk_im_context_wayland_commit (GtkIMContext *context,
                               const gchar  *str)
{
  if (GTK_IM_CONTEXT_CLASS (gtk_im_context_wayland_parent_class)->commit)
    GTK_IM_CONTEXT_CLASS (gtk_im_context_wayland_parent_class)->commit (context, str);

  notify_im_change (GTK_IM_CONTEXT_WAYLAND (context),
                    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
}

static gboolean
gtk_im_context_wayland_activate_osk_with_event (GtkIMContext *context,
                                                GdkEvent     *event)
{
  GtkIMContextWaylandGlobal *global;

  global = gtk_im_context_wayland_get_global (GTK_IM_CONTEXT_WAYLAND (context));
  if (global == NULL)
    return FALSE;

  zwp_text_input_v3_enable (global->text_input);
  notify_im_change (GTK_IM_CONTEXT_WAYLAND (context),
                    ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
  return TRUE;
}

static void
gtk_im_context_wayland_class_init (GtkIMContextWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (klass);

  object_class->finalize = gtk_im_context_wayland_finalize;

  im_context_class->set_client_widget = gtk_im_context_wayland_set_client_widget;
  im_context_class->get_preedit_string = gtk_im_context_wayland_get_preedit_string;
  im_context_class->filter_keypress = gtk_im_context_wayland_filter_keypress;
  im_context_class->focus_in = gtk_im_context_wayland_focus_in;
  im_context_class->focus_out = gtk_im_context_wayland_focus_out;
  im_context_class->reset = gtk_im_context_wayland_reset;
  im_context_class->set_cursor_location = gtk_im_context_wayland_set_cursor_location;
  im_context_class->set_use_preedit = gtk_im_context_wayland_set_use_preedit;
  im_context_class->set_surrounding_with_selection = gtk_im_context_wayland_set_surrounding;
  im_context_class->get_surrounding_with_selection = gtk_im_context_wayland_get_surrounding;
  im_context_class->commit = gtk_im_context_wayland_commit;
  im_context_class->activate_osk_with_event = gtk_im_context_wayland_activate_osk_with_event;
}

static void
on_content_type_changed (GtkIMContextWayland *context)
{
  notify_content_type (context);
  commit_state (context);
}

static void
gtk_im_context_wayland_init (GtkIMContextWayland *context)
{
  context->use_preedit = TRUE;
  g_signal_connect_swapped (context, "notify::input-purpose",
                            G_CALLBACK (on_content_type_changed), context);
  g_signal_connect_swapped (context, "notify::input-hints",
                            G_CALLBACK (on_content_type_changed), context);
}
