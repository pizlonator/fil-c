/* gtkatspieditabletext.c: EditableText interface for GtkAtspiContext
 *
 * Copyright 2020 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkatspieditabletextprivate.h"
#include "gtkatcontextprivate.h"

#include "a11y/atspi/atspi-editabletext.h"

#include "gtkeditable.h"
#include "gtkentry.h"
#include "gtksearchentry.h"
#include "gtkpasswordentry.h"
#include "gtkspinbutton.h"
#include "gtktextview.h"

#include <gio/gio.h>

/* {{{ GtkEditable */

typedef struct
{
  GtkWidget *widget;
  int position;
} PasteData;

static void
text_received (GObject      *source,
               GAsyncResult *result,
               gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  PasteData *pdata = data;
  char *text;

  text = gdk_clipboard_read_text_finish (clipboard, result, NULL);
  if (text)
    gtk_editable_insert_text (GTK_EDITABLE (pdata->widget), text, -1, &pdata->position);
  g_free (text);
  g_free (pdata);
}

static void
editable_handle_method (GDBusConnection       *connection,
                        const gchar           *sender,
                        const gchar           *object_path,
                        const gchar           *interface_name,
                        const gchar           *method_name,
                        GVariant              *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (method_name, "SetTextContents") == 0)
    {
      char *text;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(&s)", &text);

      if (gtk_editable_get_editable (GTK_EDITABLE (widget)))
        {
          gtk_editable_set_text (GTK_EDITABLE (widget), text);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "InsertText") == 0)
    {
      int position;
      char *text;
      int len;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(i&si)", &position, &text, &len);

      if (gtk_editable_get_editable (GTK_EDITABLE (widget)))
        {
          gtk_editable_insert_text (GTK_EDITABLE (widget), text, -1, &position);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "CopyText") == 0)
    {
      int start, end;
      char *str;

      g_variant_get (parameters, "(ii)", &start, &end);

      str = gtk_editable_get_chars (GTK_EDITABLE (widget), start, end);
      gdk_clipboard_set_text (gtk_widget_get_clipboard (widget), str);
      g_free (str);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "CutText") == 0)
    {
      int start, end;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(ii)", &start, &end);

      if (gtk_editable_get_editable (GTK_EDITABLE (widget)))
        {
          char *str;

          str = gtk_editable_get_chars (GTK_EDITABLE (widget), start, end);
          gdk_clipboard_set_text (gtk_widget_get_clipboard (widget), str);
          g_free (str);
          gtk_editable_delete_text (GTK_EDITABLE (widget), start, end);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "DeleteText") == 0)
    {
      int start, end;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(ii)", &start, &end);

      if (gtk_editable_get_editable (GTK_EDITABLE (widget)))
        {
          gtk_editable_delete_text (GTK_EDITABLE (widget), start, end);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "PasteText") == 0)
    {
      int position;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(i)", &position);

      if (gtk_editable_get_editable (GTK_EDITABLE (widget)))
        {
          PasteData *data;

          data = g_new (PasteData, 1);
          data->widget = widget;
          data->position = position;

          gdk_clipboard_read_text_async (gtk_widget_get_clipboard (widget), NULL, text_received, data);

          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
}

static const GDBusInterfaceVTable editable_vtable = {
  editable_handle_method,
  NULL,
};

/* }}} */
/* {{{ GtkTextView */

static void
text_view_received (GObject      *source,
                    GAsyncResult *result,
                    gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  PasteData *pdata = data;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (pdata->widget));
  GtkTextIter iter;
  char *text;

  text = gdk_clipboard_read_text_finish (clipboard, result, NULL);
  if (text)
    {
      gtk_text_buffer_get_iter_at_offset (buffer, &iter, pdata->position);
      gtk_text_buffer_insert (buffer, &iter, text, -1);
    }

  g_free (text);
  g_free (pdata);
}

static void
text_view_handle_method (GDBusConnection       *connection,
                         const gchar           *sender,
                         const gchar           *object_path,
                         const gchar           *interface_name,
                         const gchar           *method_name,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation,
                         gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (method_name, "SetTextContents") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      char *text;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(&s)", &text);

      if (gtk_text_view_get_editable (GTK_TEXT_VIEW (widget)))
        {
          gtk_text_buffer_set_text (buffer, text, -1);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "InsertText") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter iter;
      int position;
      char *text;
      int len;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(i&si)", &position, &text, &len);

      if (gtk_text_view_get_editable (GTK_TEXT_VIEW (widget)))
        {
          gtk_text_buffer_get_iter_at_offset (buffer, &iter, position);
          gtk_text_buffer_insert (buffer, &iter, text, len);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "CopyText") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;
      int start, end;
      char *str;

      g_variant_get (parameters, "(ii)", &start, &end);

      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start);
      gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, end);
      str = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
      gdk_clipboard_set_text (gtk_widget_get_clipboard (widget), str);
      g_free (str);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else if (g_strcmp0 (method_name, "CutText") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;
      int start, end;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(ii)", &start, &end);

      if (gtk_text_view_get_editable (GTK_TEXT_VIEW (widget)))
        {
          char *str;

          gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start);
          gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, end);
          str = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
          gdk_clipboard_set_text (gtk_widget_get_clipboard (widget), str);
          g_free (str);
          gtk_text_buffer_delete (buffer, &start_iter, &end_iter);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "DeleteText") == 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
      GtkTextIter start_iter, end_iter;
      int start, end;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(ii)", &start, &end);

      if (gtk_text_view_get_editable (GTK_TEXT_VIEW (widget)))
        {
          gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start);
          gtk_text_buffer_get_iter_at_offset (buffer, &end_iter, end);
          gtk_text_buffer_delete (buffer, &start_iter, &end_iter);
          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
  else if (g_strcmp0 (method_name, "PasteText") == 0)
    {
      int position;
      gboolean ret = FALSE;

      g_variant_get (parameters, "(i)", &position);

      if (gtk_text_view_get_editable (GTK_TEXT_VIEW (widget)))
        {
          PasteData *data;

          data = g_new (PasteData, 1);
          data->widget = widget;
          data->position = position;

          gdk_clipboard_read_text_async (gtk_widget_get_clipboard (widget), NULL, text_view_received, data);

          ret = TRUE;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
    }
}

static const GDBusInterfaceVTable text_view_vtable = {
  text_view_handle_method,
  NULL,
};

/* }}} */

const GDBusInterfaceVTable *
gtk_atspi_get_editable_text_vtable (GtkAccessible *accessible)
{
  if (GTK_IS_EDITABLE (accessible))
    return &editable_vtable;
  else if (GTK_IS_TEXT_VIEW (accessible))
    return &text_view_vtable;

  return NULL;
}

/* vim:set foldmethod=marker expandtab: */
