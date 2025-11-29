/*
 * Copyright © 2019 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "constraint-editor-window.h"
#include "constraint-view.h"
#include "constraint-editor.h"
#include "guide-editor.h"

struct _ConstraintEditorWindow
{
  GtkApplicationWindow parent_instance;

  GtkWidget *paned;
  GtkWidget *view;
  GtkWidget *list;
};

G_DEFINE_TYPE(ConstraintEditorWindow, constraint_editor_window, GTK_TYPE_APPLICATION_WINDOW);

static GtkConstraintTarget *
find_target (GListModel          *model,
             GtkConstraintTarget *orig)
{
  const char *name;
  const char *model_name;
  gpointer item;
  int i;

  if (orig == NULL)
    return NULL;

  if (GTK_IS_LABEL (orig))
    name = gtk_label_get_label (GTK_LABEL (orig));
  else if (GTK_IS_CONSTRAINT_GUIDE (orig))
    name = gtk_constraint_guide_get_name (GTK_CONSTRAINT_GUIDE (orig));
  else
    {
      g_warning ("Don't know how to handle %s targets", G_OBJECT_TYPE_NAME (orig));
      return NULL;
    }
  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      item = g_list_model_get_item (model, i);
      g_object_unref (item);
      if (GTK_IS_WIDGET (item))
        model_name = gtk_widget_get_name (GTK_WIDGET (item));
      else
        model_name = gtk_constraint_guide_get_name (GTK_CONSTRAINT_GUIDE (item));

      if (strcmp (name, model_name) == 0)
        return GTK_CONSTRAINT_TARGET (item);
    }
  g_warning ("Failed to find target '%s'", name);

  return NULL;
}

gboolean
constraint_editor_window_load (ConstraintEditorWindow *self,
                               GFile            *file)
{
  char *path;
  GtkBuilder *builder;
  GError *error = NULL;
  GtkWidget *view;
  GtkLayoutManager *layout;
  GtkWidget *child;
  const char *name;
  gpointer item;
  int i;
  GListModel *list;

  path = g_file_get_path (file);

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, path, &error))
    {
      g_print ("Could not load %s: %s", path, error->message);
      g_error_free (error);
      g_free (path);
      g_object_unref (builder);
      return FALSE;
    }

  view = GTK_WIDGET (gtk_builder_get_object (builder, "view"));
  if (!GTK_IS_BOX (view))
    {
      g_print ("Could not load %s: No GtkBox named 'view'", path);
      g_free (path);
      g_object_unref (builder);
      return FALSE;
    }
  layout = gtk_widget_get_layout_manager (view);
  if (!GTK_IS_CONSTRAINT_LAYOUT (layout))
    {
      g_print ("Could not load %s: Widget 'view' does not use GtkConstraintLayout", path);
      g_free (path);
      g_object_unref (builder);
      return FALSE;
    }

  for (child = gtk_widget_get_first_child (view);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!GTK_IS_LABEL (child))
        {
          g_print ("Skipping non-GtkLabel child\n");
          continue;
        }

      name = gtk_label_get_label (GTK_LABEL (child));
      constraint_view_add_child (CONSTRAINT_VIEW (self->view), name);
    }

  list = gtk_constraint_layout_observe_guides (GTK_CONSTRAINT_LAYOUT (layout));
  for (i = 0; i < g_list_model_get_n_items (list); i++)
    {
      GtkConstraintGuide *guide, *clone;
      int w, h;

      item = g_list_model_get_item (list, i);
      guide = GTK_CONSTRAINT_GUIDE (item);

      /* need to clone here, to attach to the right targets */
      clone = gtk_constraint_guide_new ();
      gtk_constraint_guide_set_name (clone, gtk_constraint_guide_get_name (guide));
      gtk_constraint_guide_set_strength (clone, gtk_constraint_guide_get_strength (guide));
      gtk_constraint_guide_get_min_size (guide, &w, &h);
      gtk_constraint_guide_set_min_size (clone, w, h);
      gtk_constraint_guide_get_nat_size (guide, &w, &h);
      gtk_constraint_guide_set_nat_size (clone, w, h);
      gtk_constraint_guide_get_max_size (guide, &w, &h);
      gtk_constraint_guide_set_max_size (clone, w, h);
      constraint_view_add_guide (CONSTRAINT_VIEW (self->view), clone);
      g_object_unref (guide);
      g_object_unref (clone);
    }
  g_object_unref (list);

  list = gtk_constraint_layout_observe_constraints (GTK_CONSTRAINT_LAYOUT (layout));
  for (i = 0; i < g_list_model_get_n_items (list); i++)
    {
      GtkConstraint *constraint;
      GtkConstraint *clone;
      GtkConstraintTarget *target;
      GtkConstraintTarget *source;
      GtkConstraintAttribute source_attr;

      item = g_list_model_get_item (list, i);
      constraint = GTK_CONSTRAINT (item);

      target = gtk_constraint_get_target (constraint);
      source = gtk_constraint_get_source (constraint);
      source_attr = gtk_constraint_get_source_attribute (constraint);

      if (source == NULL && source_attr == GTK_CONSTRAINT_ATTRIBUTE_NONE)
        clone = gtk_constraint_new_constant (find_target (constraint_view_get_model (CONSTRAINT_VIEW (self->view)), target),
                                  gtk_constraint_get_target_attribute (constraint),
                                  gtk_constraint_get_relation (constraint),
                                  gtk_constraint_get_constant (constraint),
                                  gtk_constraint_get_strength (constraint));
      else
        clone = gtk_constraint_new (find_target (constraint_view_get_model (CONSTRAINT_VIEW (self->view)), target),
                                    gtk_constraint_get_target_attribute (constraint),
                                    gtk_constraint_get_relation (constraint),
                                    find_target (constraint_view_get_model (CONSTRAINT_VIEW (self->view)), source),
                                    source_attr,
                                    gtk_constraint_get_multiplier (constraint),
                                    gtk_constraint_get_constant (constraint),
                                    gtk_constraint_get_strength (constraint));

      constraint_view_add_constraint (CONSTRAINT_VIEW (self->view), clone);

      g_object_unref (constraint);
      g_object_unref (clone);
    }
  g_object_unref (list);

  g_free (path);
  g_object_unref (builder);

  return TRUE;
}

static void
open_response_cb (GObject *source,
                  GAsyncResult *result,
                  void *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  ConstraintEditorWindow *self = user_data;
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      constraint_editor_window_load (self, file);
      g_object_unref (file);
    }
}

static void
open_cb (GtkWidget              *button,
         ConstraintEditorWindow *self)
{
  GtkFileDialog *dialog;
  GFile *cwd;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Open file");
  cwd = g_file_new_for_path (".");
  gtk_file_dialog_set_initial_folder (dialog, cwd);
  g_object_unref (cwd);
  gtk_file_dialog_open (dialog, GTK_WINDOW (self), NULL, open_response_cb, self);
  g_object_unref (dialog);
}

static void
serialize_child (GString   *str,
                 int        indent,
                 GtkWidget *child)
{
  const char *name;

  name = gtk_widget_get_name (child);
  g_string_append_printf (str, "%*s<child>\n", indent, "");
  g_string_append_printf (str, "%*s  <object class=\"GtkLabel\" id=\"%s\">\n", indent, "", name);
  g_string_append_printf (str, "%*s    <property name=\"label\">%s</property>\n", indent, "", name);
  g_string_append_printf (str, "%*s  </object>\n", indent, "");
  g_string_append_printf (str, "%*s</child>\n", indent, "");
}

static char *
serialize_model (GListModel *list)
{
  GString *str = g_string_new ("");
  int i;

  g_string_append (str, "<interface>\n");
  g_string_append (str, "  <object class=\"GtkBox\" id=\"view\">\n");
  g_string_append (str, "    <property name=\"layout-manager\">\n");
  g_string_append (str, "      <object class=\"GtkConstraintLayout\">\n");
  g_string_append (str, "        <constraints>\n");
  for (i = 0; i < g_list_model_get_n_items (list); i++)
    {
      gpointer item = g_list_model_get_item (list, i);
      g_object_unref (item);
      if (GTK_IS_CONSTRAINT (item))
        constraint_editor_serialize_constraint (str, 10, GTK_CONSTRAINT (item));
      else if (GTK_IS_CONSTRAINT_GUIDE (item))
        guide_editor_serialize_guide (str, 10, GTK_CONSTRAINT_GUIDE (item));
    }
  g_string_append (str, "        </constraints>\n");
  g_string_append (str, "      </object>\n");
  g_string_append (str, "    </property>\n");
  for (i = 0; i < g_list_model_get_n_items (list); i++)
    {
      gpointer item = g_list_model_get_item (list, i);
      g_object_unref (item);
      if (GTK_IS_WIDGET (item))
        serialize_child (str, 4, GTK_WIDGET (item));
    }
  g_string_append (str, "  </object>\n");
  g_string_append (str, "</interface>\n");

  return g_string_free (str, FALSE);
}


static void
save_response_cb (GObject *source,
                  GAsyncResult *result,
                  void *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  ConstraintEditorWindow *self = user_data;
  GFile *file;

  file = gtk_file_dialog_save_finish (dialog, result, NULL);
  if (file)
    {
      GListModel *model;
      char *text;
      GError *error = NULL;

      model = constraint_view_get_model (CONSTRAINT_VIEW (self->view));
      text = serialize_model (model);
      g_file_replace_contents (file, text, strlen (text),
                               NULL, FALSE,
                               G_FILE_CREATE_NONE,
                               NULL,
                               NULL,
                               &error);
      if (error != NULL)
        {
          GtkAlertDialog *alert;

          alert = gtk_alert_dialog_new ("Saving failed");
          gtk_alert_dialog_set_detail (alert, error->message);
          gtk_alert_dialog_show (alert,
                                 GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))));
          g_object_unref (alert);
          g_error_free (error);
        }

      g_free (text);
      g_object_unref (file);
    }
}

static void
save_cb (GtkWidget              *button,
         ConstraintEditorWindow *self)
{
  GtkFileDialog *dialog;
  GFile *cwd;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Save constraints");
  cwd = g_file_new_for_path (".");
  gtk_file_dialog_set_initial_folder (dialog, cwd);
  g_object_unref (cwd);
  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (button))),
                        NULL,
                        save_response_cb, self);
  g_object_unref (dialog);
}

static void
constraint_editor_window_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CONSTRAINT_EDITOR_WINDOW_TYPE);

  G_OBJECT_CLASS (constraint_editor_window_parent_class)->dispose (object);
}

static int child_counter;
static int guide_counter;

static void
add_child (ConstraintEditorWindow *win)
{
  char *name;

  child_counter++;
  name = g_strdup_printf ("Child %d", child_counter);
  constraint_view_add_child (CONSTRAINT_VIEW (win->view), name);
  g_free (name);
}

static void
add_guide (ConstraintEditorWindow *win)
{
  char *name;
  GtkConstraintGuide *guide;

  guide_counter++;
  name = g_strdup_printf ("Guide %d", guide_counter);
  guide = gtk_constraint_guide_new ();
  gtk_constraint_guide_set_name (guide, name);
  g_free (name);

  constraint_view_add_guide (CONSTRAINT_VIEW (win->view), guide);
}

static void
constraint_editor_done (ConstraintEditor *editor,
                        GtkConstraint    *constraint,
                        ConstraintEditorWindow *win)
{
  GtkConstraint *old_constraint;

  g_object_get (editor, "constraint", &old_constraint, NULL);

  if (old_constraint)
    constraint_view_remove_constraint (CONSTRAINT_VIEW (win->view), old_constraint);

  constraint_view_add_constraint (CONSTRAINT_VIEW (win->view), constraint);

  g_clear_object (&old_constraint);

  gtk_window_destroy (GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_WINDOW)));
}

static void
edit_constraint (ConstraintEditorWindow *win,
                 GtkConstraint          *constraint)
{
  GtkWidget *window;
  ConstraintEditor *editor;
  GListModel *model;

  window = gtk_window_new ();
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (win));
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  if (constraint)
    gtk_window_set_title (GTK_WINDOW (window), "Edit Constraint");
  else
    gtk_window_set_title (GTK_WINDOW (window), "Create Constraint");

  model = constraint_view_get_model (CONSTRAINT_VIEW (win->view));

  editor = constraint_editor_new (model, constraint);

  gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (editor));

  g_signal_connect (editor, "done", G_CALLBACK (constraint_editor_done), win);

  gtk_window_present (GTK_WINDOW (window));
}

static void
add_constraint (ConstraintEditorWindow *win)
{
  edit_constraint (win, NULL);
}

static void
guide_editor_done (GuideEditor            *editor,
                   GtkConstraintGuide     *guide,
                   ConstraintEditorWindow *win)
{
  gtk_window_destroy (GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_WINDOW)));
}

static void
edit_guide (ConstraintEditorWindow *win,
            GtkConstraintGuide     *guide)
{
  GtkWidget *window;
  GuideEditor *editor;

  window = gtk_window_new ();
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (win));
  gtk_window_set_title (GTK_WINDOW (window), "Edit Guide");

  editor = guide_editor_new (guide);
  gtk_window_set_child (GTK_WINDOW (window), GTK_WIDGET (editor));

  g_signal_connect (editor, "done", G_CALLBACK (guide_editor_done), win);
  gtk_window_present (GTK_WINDOW (window));
}

static void
row_activated (GtkListBox *list,
               GtkListBoxRow *row,
               ConstraintEditorWindow *win)
{
  GObject *item;

  item = G_OBJECT (g_object_get_data (G_OBJECT (row), "item"));

  if (GTK_IS_CONSTRAINT (item))
    edit_constraint (win, GTK_CONSTRAINT (item));
  else if (GTK_IS_CONSTRAINT_GUIDE (item))
    edit_guide (win, GTK_CONSTRAINT_GUIDE (item));
}

static void
constraint_editor_window_class_init (ConstraintEditorWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  g_type_ensure (CONSTRAINT_VIEW_TYPE);

  object_class->dispose = constraint_editor_window_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/gtk4/constraint-editor/constraint-editor-window.ui");

  gtk_widget_class_bind_template_child (widget_class, ConstraintEditorWindow, paned);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditorWindow, view);
  gtk_widget_class_bind_template_child (widget_class, ConstraintEditorWindow, list);

  gtk_widget_class_bind_template_callback (widget_class, open_cb);
  gtk_widget_class_bind_template_callback (widget_class, save_cb);
  gtk_widget_class_bind_template_callback (widget_class, add_child);
  gtk_widget_class_bind_template_callback (widget_class, add_guide);
  gtk_widget_class_bind_template_callback (widget_class, add_constraint);
  gtk_widget_class_bind_template_callback (widget_class, row_activated);
}

static void
row_edit (GtkButton *button,
          ConstraintEditorWindow *win)
{
  GtkWidget *row;
  GObject *item;

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
  item = (GObject *)g_object_get_data (G_OBJECT (row), "item");
  if (GTK_IS_CONSTRAINT (item))
    edit_constraint (win, GTK_CONSTRAINT (item));
  else if (GTK_IS_CONSTRAINT_GUIDE (item))
    edit_guide (win, GTK_CONSTRAINT_GUIDE (item));
}

static void
mark_constraints_invalid (ConstraintEditorWindow *win,
                          gpointer                removed)
{
  GtkWidget *child;
  GObject *item;

  for (child = gtk_widget_get_first_child (win->list);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      item = (GObject *)g_object_get_data (G_OBJECT (child), "item");
      if (GTK_IS_CONSTRAINT (item))
        {
          GtkConstraint *constraint = GTK_CONSTRAINT (item);

          if (gtk_constraint_get_target (constraint) == (GtkConstraintTarget *)removed ||
              gtk_constraint_get_source (constraint) == (GtkConstraintTarget *)removed)
            {
              GtkWidget *button;
              button = (GtkWidget *)g_object_get_data (G_OBJECT (child), "edit");
              gtk_button_set_icon_name (GTK_BUTTON (button), "dialog-warning-symbolic");
              gtk_widget_set_tooltip_text (button, "Constraint is invalid");
            }
        }
    }
}

static void
row_delete (GtkButton *button,
            ConstraintEditorWindow *win)
{
  GtkWidget *row;
  GObject *item;

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
  item = (GObject *)g_object_get_data (G_OBJECT (row), "item");
  if (GTK_IS_CONSTRAINT (item))
    constraint_view_remove_constraint (CONSTRAINT_VIEW (win->view),
                                       GTK_CONSTRAINT (item));
  else if (GTK_IS_CONSTRAINT_GUIDE (item))
    {
      mark_constraints_invalid (win, item);
      constraint_view_remove_guide (CONSTRAINT_VIEW (win->view),
                                    GTK_CONSTRAINT_GUIDE (item));
    }
  else if (GTK_IS_WIDGET (item))
    {
      mark_constraints_invalid (win, item);
      constraint_view_remove_child (CONSTRAINT_VIEW (win->view),
                                    GTK_WIDGET (item));
    }
}

static GtkWidget *
create_widget_func (gpointer item,
                    gpointer user_data)
{
  ConstraintEditorWindow *win = user_data;
  const char *name;
  char *freeme = NULL;
  GtkWidget *row, *box, *label, *button;

  if (GTK_IS_WIDGET (item))
    name = gtk_widget_get_name (GTK_WIDGET (item));
  else if (GTK_IS_CONSTRAINT_GUIDE (item))
    name = gtk_constraint_guide_get_name (GTK_CONSTRAINT_GUIDE (item));
  else if (GTK_IS_CONSTRAINT (item))
    name = freeme = constraint_editor_constraint_to_string (GTK_CONSTRAINT (item));
  else
    name = "";

  row = gtk_list_box_row_new ();
  g_object_set_data_full (G_OBJECT (row), "item", g_object_ref (item), g_object_unref);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new (name);
  if (GTK_IS_WIDGET (item) || GTK_IS_CONSTRAINT_GUIDE (item))
    g_object_bind_property (item, "name",
                            label, "label",
                            G_BINDING_DEFAULT);
  gtk_widget_set_margin_start (label, 10);
  gtk_widget_set_margin_end (label, 10);
  gtk_widget_set_margin_top (label, 10);
  gtk_widget_set_margin_bottom (label, 10);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
  gtk_box_append (GTK_BOX (box), label);

  if (GTK_IS_CONSTRAINT (item) || GTK_IS_CONSTRAINT_GUIDE (item))
    {
      button = gtk_button_new_from_icon_name ("document-edit-symbolic");
      gtk_button_set_has_frame (GTK_BUTTON (button), FALSE);
      g_signal_connect (button, "clicked", G_CALLBACK (row_edit), win);
      g_object_set_data (G_OBJECT (row), "edit", button);
      gtk_box_append (GTK_BOX (box), button);
      button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
      gtk_button_set_has_frame (GTK_BUTTON (button), FALSE);
      g_signal_connect (button, "clicked", G_CALLBACK (row_delete), win);
      gtk_box_append (GTK_BOX (box), button);
    }
  else if (GTK_IS_WIDGET (item))
    {
      button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
      gtk_button_set_has_frame (GTK_BUTTON (button), FALSE);
      g_signal_connect (button, "clicked", G_CALLBACK (row_delete), win);
      gtk_box_append (GTK_BOX (box), button);
    }

  g_free (freeme);

  return row;
}

static void
constraint_editor_window_init (ConstraintEditorWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_bind_model (GTK_LIST_BOX (self->list),
                           constraint_view_get_model (CONSTRAINT_VIEW (self->view)),
                           create_widget_func,
                           self,
                           NULL);
}

ConstraintEditorWindow *
constraint_editor_window_new (ConstraintEditorApplication *application)
{
  return g_object_new (CONSTRAINT_EDITOR_WINDOW_TYPE,
                       "application", application,
                       NULL);
}
