/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2017 Red Hat, Inc.
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

#include "gdkclipboardprivate.h"
#include "gdkclipboard-x11.h"

#include <glib/gi18n-lib.h>
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"
#include "gdkselectioninputstream-x11.h"
#include "gdkselectionoutputstream-x11.h"
#include "gdktextlistconverter-x11.h"
#include "gdk/gdkprivate.h"

#include <string.h>
#include <X11/Xatom.h>

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#define IDLE_ABORT_TIME 30 /* seconds */

typedef struct _GdkX11ClipboardClass GdkX11ClipboardClass;

typedef struct _RetrievalInfo RetrievalInfo;

struct _GdkX11Clipboard
{
  GdkClipboard parent;

  char       *selection;
  Atom        xselection;
  gulong      timestamp;
  
  GTask      *store_task;
};

struct _GdkX11ClipboardClass
{
  GdkClipboardClass parent_class;
};

G_DEFINE_TYPE (GdkX11Clipboard, gdk_x11_clipboard, GDK_TYPE_CLIPBOARD)

static void
print_atoms (GdkX11Clipboard *cb,
             const char      *prefix,
             const Atom      *atoms,
             gsize            n_atoms)
{
  GdkDisplay *display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));

  if (GDK_DISPLAY_DEBUG_CHECK (display, CLIPBOARD))
    {
      gsize i;
      GString *str;

      str = g_string_new ("");
      g_string_append_printf (str, "%s: %s [ ", cb->selection, prefix);
      for (i = 0; i < n_atoms; i++)
        g_string_append_printf (str, "%s%s", i > 0 ? ", " : "", gdk_x11_get_xatom_name_for_display (display , atoms[i]));
      g_string_append (str, " ]");

      gdk_debug_message ("%s", str->str);
      g_string_free (str, TRUE);
    }
}

static void
gdk_x11_clipboard_default_output_closed (GObject      *stream,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GError *error = NULL;

  if (!g_output_stream_close_finish (G_OUTPUT_STREAM (stream), result, &error))
    {
      GDK_DEBUG (CLIPBOARD, "-------: failed to close stream: %s", error->message);
      g_error_free (error);
    }

  g_object_unref (stream);
}

static void
gdk_x11_clipboard_default_output_done (GObject      *clipboard,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GOutputStream *stream = user_data;
  GError *error = NULL;

  if (!gdk_clipboard_write_finish (GDK_CLIPBOARD (clipboard), result, &error))
    {
      GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (clipboard)), CLIPBOARD,
                         "%s: failed to write stream: %s",
                         GDK_X11_CLIPBOARD (clipboard)->selection, error->message);
      g_error_free (error);
    }

  g_output_stream_close_async (stream,
                               G_PRIORITY_DEFAULT,
                               NULL, 
                               gdk_x11_clipboard_default_output_closed,
                               NULL);
}

static void
gdk_x11_clipboard_default_output_handler (GOutputStream   *stream,
                                          const char      *mime_type,
                                          gpointer         user_data)
{
  gdk_clipboard_write_async (GDK_CLIPBOARD (user_data),
                             mime_type,
                             stream,
                             G_PRIORITY_DEFAULT,
                             NULL,
                             gdk_x11_clipboard_default_output_done,
                             stream);
}

static GInputStream * 
text_list_convert (GdkX11Clipboard *cb,
                   GInputStream    *stream,
                   const char      *encoding,
                   int              format)
{
  GInputStream *converter_stream;
  GConverter *converter;

  converter = gdk_x11_text_list_converter_to_utf8_new (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                                       encoding,
                                                       format);
  converter_stream = g_converter_input_stream_new (stream, converter);

  g_object_unref (converter);
  g_object_unref (stream);

  return converter_stream;
}

static GInputStream * 
no_convert (GdkX11Clipboard *cb,
            GInputStream    *stream,
            const char      *encoding,
            int              format)
{
  return stream;
}

static const struct {
  const char *x_target;
  const char *mime_type;
  GInputStream * (* convert) (GdkX11Clipboard *, GInputStream *, const char *, int);
  const char *type;
  int format;
} special_targets[] = {
  { "UTF8_STRING",   "text/plain;charset=utf-8", no_convert,        "UTF8_STRING",   8 },
  { "COMPOUND_TEXT", "text/plain;charset=utf-8", text_list_convert, "COMPOUND_TEXT", 8 },
  { "TEXT",          "text/plain;charset=utf-8", text_list_convert, "STRING",        8 },
  { "STRING",        "text/plain;charset=utf-8", text_list_convert, "STRING",        8 },
  { "TARGETS",       NULL,                       NULL,              "ATOM",          32 },
  { "TIMESTAMP",     NULL,                       NULL,              "INTEGER",       32 },
  { "SAVE_TARGETS",  NULL,                       NULL,              "NULL",          32 }
};

GSList *
gdk_x11_clipboard_formats_to_targets (GdkContentFormats *formats)
{
  GSList *targets;
  const char * const *mime_types;
  gsize i, j, n_mime_types;

  targets = NULL;
  mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);

  for (i = 0; i < n_mime_types; i++)
    {
      for (j = 0; j < G_N_ELEMENTS (special_targets); j++)
        {
          if (special_targets[j].mime_type == NULL)
            continue;

          if (g_str_equal (mime_types[i], special_targets[j].mime_type))
            targets = g_slist_prepend (targets, (gpointer) g_intern_static_string (special_targets[j].x_target));
        }
      targets = g_slist_prepend (targets, (gpointer) mime_types[i]);
    }

  return g_slist_reverse (targets);
}

Atom *
gdk_x11_clipboard_formats_to_atoms (GdkDisplay        *display,
                                    gboolean           include_special,
                                    GdkContentFormats *formats,
                                    gsize             *n_atoms)
{
  GSList *l, *targets;
  Atom *atoms;
  gsize i;

  targets = gdk_x11_clipboard_formats_to_targets (formats);

  if (include_special)
    {
      for (i = 0; i < G_N_ELEMENTS (special_targets); i++)
        {
          if (special_targets[i].mime_type != NULL)
            continue;

          targets = g_slist_prepend (targets, (gpointer) g_intern_string (special_targets[i].x_target));
        }
    }

  *n_atoms = g_slist_length (targets);
  atoms = g_new (Atom, *n_atoms);
  i = 0;
  for (l = targets; l; l = l->next)
    atoms[i++] = gdk_x11_get_xatom_by_name_for_display (display, l->data);

  g_slist_free (targets);

  return atoms;
}

static GdkContentFormats *
gdk_x11_clipboard_formats_from_atoms (GdkDisplay *display,
                                      const Atom *atoms,
                                      gsize       n_atoms)
{
  GdkContentFormatsBuilder *builder;
  gsize i, j;

  builder = gdk_content_formats_builder_new ();
  for (i = 0; i < n_atoms; i++)
    {
      const char *name;

      name = gdk_x11_get_xatom_name_for_display (display , atoms[i]);
      if (!name)
        {
          continue;
        }
      if (strchr (name, '/'))
        {
          gdk_content_formats_builder_add_mime_type (builder, name);
          continue;
        }

      for (j = 0; j < G_N_ELEMENTS (special_targets); j++)
        {
          if (g_str_equal (name, special_targets[j].x_target))
            {
              if (special_targets[j].mime_type)
                gdk_content_formats_builder_add_mime_type (builder, special_targets[j].mime_type);
              break;
            }
        }
    }

  return gdk_content_formats_builder_free_to_formats (builder);
}

static void
gdk_x11_clipboard_request_targets_finish (GObject      *source_object,
                                          GAsyncResult *res,
                                          gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (source_object);
  GdkX11Clipboard *cb = user_data;
  GdkDisplay *display;
  GdkContentFormats *formats;
  GBytes *bytes;
  GError *error = NULL;

  display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));

  bytes = g_input_stream_read_bytes_finish (stream, res, &error);
  if (bytes == NULL)
    {
      GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                         "%s: error reading TARGETS: %s\n", cb->selection, error->message);
      g_error_free (error);
      g_object_unref (stream);
      g_object_unref (cb);
      return;
    }
  else if (g_bytes_get_size (bytes) == 0)
    {
      g_bytes_unref (bytes);
      g_object_unref (stream);
      g_object_unref (cb);
      return;
    }
  else if (gdk_clipboard_is_local (GDK_CLIPBOARD (cb)))
    {
      /* FIXME: Use a cancellable for this request, so that we don't do he brittle
       * is_local() check */
      g_bytes_unref (bytes);
      g_object_unref (stream);
      g_object_unref (cb);
      return;
    }

  print_atoms (cb,
               "received targets",
               g_bytes_get_data (bytes, NULL),
               g_bytes_get_size (bytes) / sizeof (Atom));

  formats = gdk_x11_clipboard_formats_from_atoms (display,
                                                  g_bytes_get_data (bytes, NULL),
                                                  g_bytes_get_size (bytes) / sizeof (Atom));
  if (GDK_DISPLAY_DEBUG_CHECK (display, CLIPBOARD))
    {
      char *s = gdk_content_formats_to_string (formats);
      gdk_debug_message ("%s: got formats: %s", cb->selection, s);
      g_free (s);
    }

  /* union with previously loaded formats */
  formats = gdk_content_formats_union (formats, gdk_clipboard_get_formats (GDK_CLIPBOARD (cb)));
  gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb), formats);
  gdk_content_formats_unref (formats);
  g_bytes_unref (bytes);

  g_input_stream_read_bytes_async (stream,
                                   gdk_x11_display_get_max_request_size (display),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   gdk_x11_clipboard_request_targets_finish,
                                   cb);
}

static void
gdk_x11_clipboard_request_targets_got_stream (GObject      *source,
                                              GAsyncResult *result,
                                              gpointer      data)
{
  GdkX11Clipboard *cb = data;
  GInputStream *stream;
  GdkDisplay *display;
  GError *error = NULL;
  const char *type;
  int format;

  display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));
  stream = gdk_x11_selection_input_stream_new_finish (result, &type, &format, &error);
  if (stream == NULL)
    {
      GDK_DISPLAY_DEBUG(display, CLIPBOARD, "%s: can't request TARGETS: %s", cb->selection, error->message);
      g_object_unref (cb);
      g_error_free (error);
      return;
    }
  else if (g_strcmp0 (type, "ATOM") != 0 || format != 32)
    {
      GDK_DISPLAY_DEBUG (display, CLIPBOARD, "%s: Wrong reply type to TARGETS: type %s != ATOM or format %d != 32",
                                      cb->selection, type ? type : "NULL", format);
      g_input_stream_close (stream, NULL, NULL);
      g_object_unref (stream);
      g_object_unref (cb);
      return;
    }

  g_input_stream_read_bytes_async (stream,
                                   gdk_x11_display_get_max_request_size (display),
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   gdk_x11_clipboard_request_targets_finish,
                                   cb);
}

static void
gdk_x11_clipboard_request_targets (GdkX11Clipboard *cb)
{
  gdk_x11_selection_input_stream_new_async (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                            cb->selection,
                                            "TARGETS",
                                            cb->timestamp,
                                            G_PRIORITY_DEFAULT,
                                            NULL,
                                            gdk_x11_clipboard_request_targets_got_stream,
                                            g_object_ref (cb));
}

static void
gdk_x11_clipboard_claim_remote (GdkX11Clipboard *cb,
                                guint32          timestamp)
{
  GdkContentFormats *empty;

  empty = gdk_content_formats_new (NULL, 0);
  gdk_clipboard_claim_remote (GDK_CLIPBOARD (cb), empty);
  gdk_content_formats_unref (empty);
  cb->timestamp = timestamp;
  gdk_x11_clipboard_request_targets (cb);
}

static gboolean
gdk_x11_clipboard_xevent (GdkDisplay   *display,
                          const XEvent *xevent,
                          gpointer   data)
{
  GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (data);
  Window xwindow;

  xwindow = GDK_X11_DISPLAY (display)->leader_window;

  if (xevent->xany.window != xwindow)
    return FALSE;

  switch (xevent->type)
  {
    case SelectionClear:
      if (xevent->xselectionclear.selection != cb->xselection)
        return FALSE;

      if (xevent->xselectionclear.time < cb->timestamp)
        {
          GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                             "%s: ignoring SelectionClear with too old timestamp (%lu vs %lu)",
                             cb->selection, xevent->xselectionclear.time, cb->timestamp);
          return FALSE;
        }

      GDK_DISPLAY_DEBUG (display, CLIPBOARD, "%s: got SelectionClear", cb->selection);
      gdk_x11_clipboard_claim_remote (cb, xevent->xselectionclear.time);
      return TRUE;

    case SelectionNotify:
      /* This code only checks clipboard manager replies, so... */
      if (!g_str_equal (cb->selection, "CLIPBOARD"))
        return FALSE;

      /* selection is not for us */
      if (xevent->xselection.selection != gdk_x11_get_xatom_by_name_for_display (display, "CLIPBOARD_MANAGER") ||
          xevent->xselection.target != gdk_x11_get_xatom_by_name_for_display (display, "SAVE_TARGETS"))
        return FALSE;

      /* We already received a selectionNotify before */
      if (cb->store_task == NULL)
        {
          GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                             "%s: got SelectionNotify for nonexisting task?!", cb->selection);
          return FALSE;
        }

      GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                         "%s: got SelectionNotify for store task", cb->selection);

      if (xevent->xselection.property != None)
        g_task_return_boolean (cb->store_task, TRUE);
      else
        g_task_return_new_error (cb->store_task, G_IO_ERROR, G_IO_ERROR_FAILED,
                                 _("Clipboard manager could not store selection."));
      g_clear_object (&cb->store_task);
      
      return FALSE;

    case SelectionRequest:
      {
       const char *target, *property;

        if (xevent->xselectionrequest.selection != cb->xselection)
          return FALSE;

        target = gdk_x11_get_xatom_name_for_display (display, xevent->xselectionrequest.target);
        if (xevent->xselectionrequest.property == None)
          property = target;
        else
          property = gdk_x11_get_xatom_name_for_display (display, xevent->xselectionrequest.property);

        if (!gdk_clipboard_is_local (GDK_CLIPBOARD (cb)))
          {
            GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                               "%s: got SelectionRequest for %s @ %s even though we don't own the selection, huh?",
                                  cb->selection, target, property);
            return TRUE;
          }
        if (xevent->xselectionrequest.requestor == None)
          {
            GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                               "%s: got SelectionRequest for %s @ %s with NULL window, ignoring",
                                  cb->selection, target, property);
            return TRUE;
          }
        
        GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                           "%s: got SelectionRequest for %s @ %s", cb->selection, target, property);

        gdk_x11_selection_output_streams_create (display,
                                                 gdk_clipboard_get_formats (GDK_CLIPBOARD (cb)),
                                                 xevent->xselectionrequest.requestor,
                                                 xevent->xselectionrequest.selection,
                                                 xevent->xselectionrequest.target,
                                                 xevent->xselectionrequest.property ? xevent->xselectionrequest.property
                                                                                    : xevent->xselectionrequest.target,
                                                 xevent->xselectionrequest.time,
                                                 gdk_x11_clipboard_default_output_handler,
                                                 cb);
        return TRUE;
      }

    default:
#ifdef HAVE_XFIXES
      if (xevent->type - GDK_X11_DISPLAY (display)->xfixes_event_base == XFixesSelectionNotify)
        {
          XFixesSelectionNotifyEvent *sn = (XFixesSelectionNotifyEvent *) xevent;

          if (sn->selection != cb->xselection)
            return FALSE;

          if (sn->selection_timestamp < cb->timestamp)
            {
              GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                                 "%s: Ignoring XFixesSelectionNotify with too old timestamp (%lu vs %lu)",
                                    cb->selection, sn->selection_timestamp, cb->timestamp);
              return FALSE;
            }

          if (sn->owner == GDK_X11_DISPLAY (display)->leader_window)
            {
              GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                                 "%s: Ignoring XFixesSelectionNotify for ourselves", cb->selection);
              return FALSE;
            }

          GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                             "%s: Received XFixesSelectionNotify, claiming selection", cb->selection);

          gdk_x11_clipboard_claim_remote (cb, sn->selection_timestamp);
        }
#endif
      return FALSE;
  }
}

static void
gdk_x11_clipboard_finalize (GObject *object)
{
  GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (object);

  g_signal_handlers_disconnect_by_func (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                        gdk_x11_clipboard_xevent,
                                        cb);
  g_free (cb->selection);

  G_OBJECT_CLASS (gdk_x11_clipboard_parent_class)->finalize (object);
}

static gboolean
gdk_x11_clipboard_claim (GdkClipboard       *clipboard,
                         GdkContentFormats  *formats,
                         gboolean            local,
                         GdkContentProvider *content)
{
  if (local)
    {
      GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (clipboard);
      GdkDisplay *display = gdk_clipboard_get_display (GDK_CLIPBOARD (cb));
      Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
      Window xwindow = GDK_X11_DISPLAY (display)->leader_window;
      guint32 time;

      time = gdk_x11_get_server_time (GDK_X11_DISPLAY (display)->leader_gdk_surface);

      if (content)
        {
          XSetSelectionOwner (xdisplay, cb->xselection, xwindow, time);

          if (XGetSelectionOwner (xdisplay, cb->xselection) != xwindow)
            {
              GDK_DISPLAY_DEBUG (display, CLIPBOARD, "%s: failed XSetSelectionOwner()", cb->selection);
              return FALSE;
            }
        }
      else
        {
          XSetSelectionOwner (xdisplay, cb->xselection, None, time);
        }

      cb->timestamp = time;
      GDK_DISPLAY_DEBUG (display, CLIPBOARD, "%s: claimed via XSetSelectionOwner()", cb->selection);
    }

  return GDK_CLIPBOARD_CLASS (gdk_x11_clipboard_parent_class)->claim (clipboard, formats, local, content);
}

static void
gdk_x11_clipboard_store_async (GdkClipboard        *clipboard,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (clipboard);
  GdkDisplay *display = gdk_clipboard_get_display (clipboard);
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  Atom clipboard_manager, save_targets, property_name;
  GdkContentProvider *content;
  GdkContentFormats *formats;
  Atom *atoms;
  gsize n_atoms;
  int error;

  /* clipboard managers don't work on anything but the clipbpoard selection */
  if (!g_str_equal (cb->selection, "CLIPBOARD"))
    {
      GDK_DISPLAY_DEBUG (display, CLIPBOARD, "%s: can only store on CLIPBOARD", cb->selection);
      GDK_CLIPBOARD_CLASS (gdk_x11_clipboard_parent_class)->store_async (clipboard,
                                                                         io_priority,
                                                                         cancellable,
                                                                         callback,
                                                                         user_data);
      return;
    }

  cb->store_task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (cb->store_task, io_priority);
  g_task_set_source_tag (cb->store_task, gdk_x11_clipboard_store_async);

  clipboard_manager = gdk_x11_get_xatom_by_name_for_display (display, "CLIPBOARD_MANAGER");
  save_targets = gdk_x11_get_xatom_by_name_for_display (display, "SAVE_TARGETS");

  if (XGetSelectionOwner (xdisplay, clipboard_manager) == None)
    {
      GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                         "%s: XGetSelectionOwner (CLIPBOARD_MANAGER) returned None, aborting.",
                            cb->selection);
      g_task_return_new_error (cb->store_task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("Cannot store clipboard. No clipboard manager is active."));
      g_clear_object (&cb->store_task);
      return;
    }

  content = gdk_clipboard_get_content (clipboard);
  if (content == NULL)
    {
      GDK_DISPLAY_DEBUG (display, CLIPBOARD, "%s: storing empty clipboard: SUCCESS!", cb->selection);
      g_task_return_boolean (cb->store_task, TRUE);
      g_clear_object (&cb->store_task);
      return;
    }

  formats = gdk_content_provider_ref_storable_formats (content);
  formats = gdk_content_formats_union_serialize_mime_types (formats);
  atoms = gdk_x11_clipboard_formats_to_atoms (display, FALSE, formats, &n_atoms);
  print_atoms (cb, "requesting store from clipboard manager", atoms, n_atoms);
  gdk_content_formats_unref (formats);

  gdk_x11_display_error_trap_push (display);

  if (n_atoms > 0)
    {
      property_name = gdk_x11_get_xatom_by_name_for_display (display, "GDK_CLIPBOARD_SAVE_TARGETS");

      XChangeProperty (xdisplay, GDK_X11_DISPLAY (display)->leader_window,
                       property_name, XA_ATOM,
                       32, PropModeReplace, (guchar *)atoms, n_atoms);
    }
  else
    property_name = None;

  XConvertSelection (xdisplay,
                     clipboard_manager, save_targets, property_name,
                     GDK_X11_DISPLAY (display)->leader_window, cb->timestamp);

  error = gdk_x11_display_error_trap_pop (display);
  if (error != Success)
    {
      GDK_DISPLAY_DEBUG (display, CLIPBOARD,
                         "%s: X error during ConvertSelection() while storing selection: %d", cb->selection, error);
    }

  g_free (atoms);
}

static gboolean
gdk_x11_clipboard_store_finish (GdkClipboard  *clipboard,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, clipboard), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == gdk_x11_clipboard_store_async, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gdk_x11_clipboard_read_got_stream (GObject      *source,
                                   GAsyncResult *res,
                                   gpointer      data)
{
  GTask *task = data;
  GError *error = NULL;
  GInputStream *stream;
  const char *type;
  int format;
  
  stream = gdk_x11_selection_input_stream_new_finish (res, &type, &format, &error);
  if (stream == NULL)
    {
      GSList *targets, *next;
      
      targets = g_task_get_task_data (task);
      next = targets->next;
      if (next)
        {
          GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (g_task_get_source_object (task));

          GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD(cb)), CLIPBOARD,
                             "%s: reading %s failed, trying %s next\n",
                             cb->selection, (char *) targets->data, (char *) next->data);
          targets->next = NULL;
          g_task_set_task_data (task, next, (GDestroyNotify) g_slist_free);
          gdk_x11_selection_input_stream_new_async (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                                    cb->selection,
                                                    next->data,
                                                    cb->timestamp,
                                                    g_task_get_priority (task),
                                                    g_task_get_cancellable (task),
                                                    gdk_x11_clipboard_read_got_stream,
                                                    task);
          g_error_free (error);
          return;
        }

      g_task_return_error (task, error);
    }
  else
    {
      GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (g_task_get_source_object (task));
      const char *mime_type = ((GSList *) g_task_get_task_data (task))->data;
      gsize i;

      for (i = 0; i < G_N_ELEMENTS (special_targets); i++)
        {
          if (g_str_equal (mime_type, special_targets[i].x_target))
            {
              g_assert (special_targets[i].mime_type != NULL);

              GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), CLIPBOARD,
                                 "%s: reading with converter from %s to %s",
                                    cb->selection, mime_type, special_targets[i].mime_type);
              mime_type = g_intern_string (special_targets[i].mime_type);
              g_task_set_task_data (task, g_slist_prepend (NULL, (gpointer) mime_type), (GDestroyNotify) g_slist_free);
              stream = special_targets[i].convert (cb, stream, type, format);
              break;
            }
        }

      GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)), CLIPBOARD,
                         "%s: reading clipboard as %s now", cb->selection, mime_type);
      g_task_return_pointer (task, stream, g_object_unref);
    }

  g_object_unref (task);
}

static void
gdk_x11_clipboard_read_async (GdkClipboard        *clipboard,
                              GdkContentFormats   *formats,
                              int                  io_priority,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GdkX11Clipboard *cb = GDK_X11_CLIPBOARD (clipboard);
  GSList *targets;
  GTask *task;

  task = g_task_new (clipboard, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, gdk_x11_clipboard_read_async);

  targets = gdk_x11_clipboard_formats_to_targets (formats);
  g_task_set_task_data (task, targets, (GDestroyNotify) g_slist_free);
  if (targets == NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                               _("No compatible transfer format found"));
      g_object_unref (task);
      return;
    }

  GDK_DISPLAY_DEBUG (gdk_clipboard_get_display (clipboard), CLIPBOARD,
                     "%s: new read for %s (%u other options)",
                        cb->selection, (char *) targets->data, g_slist_length (targets->next));
  gdk_x11_selection_input_stream_new_async (gdk_clipboard_get_display (GDK_CLIPBOARD (cb)),
                                            cb->selection,
                                            targets->data,
                                            cb->timestamp,
                                            io_priority,
                                            cancellable,
                                            gdk_x11_clipboard_read_got_stream,
                                            task);
}

static GInputStream *
gdk_x11_clipboard_read_finish (GdkClipboard  *clipboard,
                               GAsyncResult  *result,
                               const char   **out_mime_type,
                               GError       **error)
{
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (clipboard)), NULL);
  task = G_TASK (result);
  g_return_val_if_fail (g_task_get_source_tag (task) == gdk_x11_clipboard_read_async, NULL);

  if (out_mime_type)
    {
      GSList *targets;

      targets = g_task_get_task_data (task);
      *out_mime_type = targets ? targets->data : NULL;
    }

  return g_task_propagate_pointer (task, error);
}

static void
gdk_x11_clipboard_class_init (GdkX11ClipboardClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkClipboardClass *clipboard_class = GDK_CLIPBOARD_CLASS (class);

  object_class->finalize = gdk_x11_clipboard_finalize;

  clipboard_class->claim = gdk_x11_clipboard_claim;
  clipboard_class->store_async = gdk_x11_clipboard_store_async;
  clipboard_class->store_finish = gdk_x11_clipboard_store_finish;
  clipboard_class->read_async = gdk_x11_clipboard_read_async;
  clipboard_class->read_finish = gdk_x11_clipboard_read_finish;
}

static void
gdk_x11_clipboard_init (GdkX11Clipboard *cb)
{
  cb->timestamp = CurrentTime;
}

GdkClipboard *
gdk_x11_clipboard_new (GdkDisplay  *display,
                       const char *selection)
{
  GdkX11Clipboard *cb;

  cb = g_object_new (GDK_TYPE_X11_CLIPBOARD,
                     "display", display,
                     NULL);

  cb->selection = g_strdup (selection);
  cb->xselection = gdk_x11_get_xatom_by_name_for_display (display, selection);

  gdk_x11_display_request_selection_notification (display, selection);
  g_signal_connect (display, "xevent", G_CALLBACK (gdk_x11_clipboard_xevent), cb);
  gdk_x11_clipboard_claim_remote (cb, CurrentTime);

  return GDK_CLIPBOARD (cb);
}

