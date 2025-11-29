/*
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "window.h"
#include "css-editor.h"

#include "gtkcssprovider.h"
#include "gtkstyleprovider.h"
#include "gtktextview.h"
#include "gtkalertdialog.h"
#include "gtkfiledialog.h"
#include "gtktogglebutton.h"
#include "gtklabel.h"
#include "gtktooltip.h"
#include "gtktextiter.h"

#include "gtk/css/gtkcss.h"

struct _GtkInspectorCssEditorPrivate
{
  GtkWidget *view;
  GtkTextBuffer *text;
  GdkDisplay *display;
  GtkCssProvider *provider;
  GtkToggleButton *disable_button;
  guint timeout;
  GList *errors;
};

typedef struct {
  GError *error;
  GtkTextIter start;
  GtkTextIter end;
} CssError;

static void
css_error_free (gpointer data)
{
  CssError *error = data;
  g_error_free (error->error);
  g_free (error);
}

static gboolean
query_tooltip_cb (GtkWidget             *widget,
                  int                    x,
                  int                    y,
                  gboolean               keyboard_tip,
                  GtkTooltip            *tooltip,
                  GtkInspectorCssEditor *ce)
{
  GtkTextIter iter;
  GList *l;

  if (keyboard_tip)
    {
      int offset;

      g_object_get (ce->priv->text, "cursor-position", &offset, NULL);
      gtk_text_buffer_get_iter_at_offset (ce->priv->text, &iter, offset);
    }
  else
    {
      int bx, by, trailing;

      gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (ce->priv->view), GTK_TEXT_WINDOW_TEXT,
                                             x, y, &bx, &by);
      gtk_text_view_get_iter_at_position (GTK_TEXT_VIEW (ce->priv->view), &iter, &trailing, bx, by);
    }

  for (l = ce->priv->errors; l; l = l->next)
    {
      CssError *css_error = l->data;

      if (gtk_text_iter_in_range (&iter, &css_error->start, &css_error->end))
        {
          gtk_tooltip_set_text (tooltip, css_error->error->message);
          return TRUE;
        }
    }

  return FALSE;
}

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssEditor, gtk_inspector_css_editor, GTK_TYPE_BOX)

static char *
get_autosave_path (void)
{
  return g_build_filename (g_get_user_cache_dir (), "gtk-4.0", "inspector-css-autosave", NULL);
}

static void
set_initial_text (GtkInspectorCssEditor *ce)
{
  char *initial_text = NULL;
  char *autosave_file = NULL;
  gsize len;

  autosave_file = get_autosave_path ();

  if (g_file_get_contents (autosave_file, &initial_text, &len, NULL))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ce->priv->disable_button), TRUE);
  else
    initial_text = g_strconcat ("/*\n",
                                _("You can type here any CSS rule recognized by GTK."), "\n",
                                _("You can temporarily disable this custom CSS by clicking on the “Pause” button above."), "\n\n",
                                _("Changes are applied instantly and globally, for the whole application."), "\n",
                                "*/\n\n", NULL);
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (ce->priv->text), initial_text, -1);
  g_free (initial_text);
  g_free (autosave_file);
}

static void
autosave_contents (GtkInspectorCssEditor *ce)
{
  char *autosave_file = NULL;
  char *dir = NULL;
  char *contents;
  GtkTextIter start, end;

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (ce->priv->text), &start, &end);
  contents = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (ce->priv->text), &start, &end, TRUE);
  autosave_file = get_autosave_path ();
  dir = g_path_get_dirname (autosave_file);
  g_mkdir_with_parents (dir, 0755);
  g_file_set_contents (autosave_file, contents, -1, NULL);

  g_free (dir);
  g_free (autosave_file);
  g_free (contents);
}

static void
disable_toggled (GtkToggleButton       *button,
                 GtkInspectorCssEditor *ce)
{
  if (!ce->priv->display)
    return;

  if (gtk_toggle_button_get_active (button))
    gtk_style_context_remove_provider_for_display (ce->priv->display,
                                                   GTK_STYLE_PROVIDER (ce->priv->provider));
  else
    gtk_style_context_add_provider_for_display (ce->priv->display,
                                                GTK_STYLE_PROVIDER (ce->priv->provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_USER);
}

static char *
get_current_text (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_remove_all_tags (buffer, &start, &end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static void
save_to_file (GtkInspectorCssEditor *ce,
              GFile                 *file)
{
  GError *error = NULL;
  char *text;

  text = get_current_text (ce->priv->text);

  g_file_replace_contents (file, text, strlen (text),
                           NULL,
                           FALSE,
                           G_FILE_CREATE_NONE,
                           NULL,
                           NULL,
                           &error);

  if (error != NULL)
    {
      GtkAlertDialog *alert;

      alert = gtk_alert_dialog_new (_("Saving CSS failed"));
      gtk_alert_dialog_set_detail (alert, error->message);
      gtk_alert_dialog_show (alert, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (ce))));
      g_object_unref (alert);
      g_error_free (error);
    }

  g_free (text);
}

static void
save_response (GObject *source,
               GAsyncResult *result,
               gpointer data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GtkInspectorCssEditor *ce = data;
  GError *error = NULL;
  GFile *file;

  file = gtk_file_dialog_save_finish (dialog, result, &error);
  if (file)
    {
      save_to_file (ce, file);
      g_object_unref (file);
    }
  else
    {
      g_print ("Error saving css: %s\n", error->message);
      g_error_free (error);
    }
}

static void
save_clicked (GtkButton             *button,
              GtkInspectorCssEditor *ce)
{
  GtkFileDialog *dialog;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_initial_name (dialog, "custom.css");
  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (ce))),
                        NULL,
                        save_response, ce);
  g_object_unref (dialog);
}

static void
update_style (GtkInspectorCssEditor *ce)
{
  char *text;

  g_list_free_full (ce->priv->errors, css_error_free);
  ce->priv->errors = NULL;

  text = get_current_text (ce->priv->text);
  gtk_css_provider_load_from_string (ce->priv->provider, text);
  g_free (text);
}

static gboolean
update_timeout (gpointer data)
{
  GtkInspectorCssEditor *ce = data;

  ce->priv->timeout = 0;

  autosave_contents (ce);
  update_style (ce);

  return G_SOURCE_REMOVE;
}

static void
text_changed (GtkTextBuffer         *buffer,
              GtkInspectorCssEditor *ce)
{
  if (ce->priv->timeout != 0)
    g_source_remove (ce->priv->timeout);

  ce->priv->timeout = g_timeout_add (100, update_timeout, ce); 

  g_list_free_full (ce->priv->errors, css_error_free);
  ce->priv->errors = NULL;
}

static void
show_parsing_error (GtkCssProvider        *provider,
                    GtkCssSection         *section,
                    const GError          *error,
                    GtkInspectorCssEditor *ce)
{
  const char *tag_name;
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (ce->priv->text);
  const GtkCssLocation *start, *end;
  CssError *css_error;

  css_error = g_new (CssError, 1);
  css_error->error = g_error_copy (error);

  start = gtk_css_section_get_start_location (section);
  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &css_error->start,
                                          start->lines,
                                          start->line_bytes);
  end = gtk_css_section_get_end_location (section);
  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &css_error->end,
                                          end->lines,
                                          end->line_bytes);

  if (error->domain == GTK_CSS_PARSER_WARNING)
    tag_name = "warning";
  else
    tag_name = "error";

  if (gtk_text_iter_equal (&css_error->start, &css_error->end))
    gtk_text_iter_forward_char (&css_error->end);

  gtk_text_buffer_apply_tag_by_name (buffer, tag_name, &css_error->start, &css_error->end);

  ce->priv->errors = g_list_prepend (ce->priv->errors, css_error);
}

static void
create_provider (GtkInspectorCssEditor *ce)
{
  ce->priv->provider = gtk_css_provider_new ();
  g_signal_connect (ce->priv->provider, "parsing-error",
                    G_CALLBACK (show_parsing_error), ce);

}

static void
destroy_provider (GtkInspectorCssEditor *ce)
{
  g_signal_handlers_disconnect_by_func (ce->priv->provider, show_parsing_error, ce);
  g_clear_object (&ce->priv->provider);
}

static void
add_provider (GtkInspectorCssEditor *ce,
              GdkDisplay *display)
{
  gtk_style_context_add_provider_for_display (display,
                                              GTK_STYLE_PROVIDER (ce->priv->provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
}

static void
remove_provider (GtkInspectorCssEditor *ce,
                 GdkDisplay *display)
{
  gtk_style_context_remove_provider_for_display (display,
                                                 GTK_STYLE_PROVIDER (ce->priv->provider));
}

static void
gtk_inspector_css_editor_init (GtkInspectorCssEditor *ce)
{
  ce->priv = gtk_inspector_css_editor_get_instance_private (ce);
  gtk_widget_init_template (GTK_WIDGET (ce));
}

static void
constructed (GObject *object)
{
  GtkInspectorCssEditor *ce = GTK_INSPECTOR_CSS_EDITOR (object);

  create_provider (ce);
}

static void
finalize (GObject *object)
{
  GtkInspectorCssEditor *ce = GTK_INSPECTOR_CSS_EDITOR (object);

  if (ce->priv->timeout != 0)
    g_source_remove (ce->priv->timeout);

  if (ce->priv->display)
    remove_provider (ce, ce->priv->display);
  destroy_provider (ce);

  g_list_free_full (ce->priv->errors, css_error_free);

  G_OBJECT_CLASS (gtk_inspector_css_editor_parent_class)->finalize (object);
}

static void
gtk_inspector_css_editor_class_init (GtkInspectorCssEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = constructed;
  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/css-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, text);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, disable_button);
  gtk_widget_class_bind_template_callback (widget_class, disable_toggled);
  gtk_widget_class_bind_template_callback (widget_class, save_clicked);
  gtk_widget_class_bind_template_callback (widget_class, text_changed);
  gtk_widget_class_bind_template_callback (widget_class, query_tooltip_cb);
}

void
gtk_inspector_css_editor_set_display (GtkInspectorCssEditor *ce,
                                      GdkDisplay *display)
{
  ce->priv->display = display;
  add_provider (ce, display);
  set_initial_text (ce);
}

// vim: set et sw=2 ts=2:
