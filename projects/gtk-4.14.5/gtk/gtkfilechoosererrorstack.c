/* This library is free software; you can redistribute it and/or
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
#include "gtkfilechoosererrorstackprivate.h"
#include "gtkstack.h"
#include "gtklabel.h"
#include <glib/gi18n-lib.h>
#include "gtkbinlayout.h"

G_DEFINE_TYPE (GtkFileChooserErrorStack, gtk_file_chooser_error_stack, GTK_TYPE_WIDGET)

static void
gtk_file_chooser_error_stack_dispose (GObject *object)
{
  GtkFileChooserErrorStack *self = GTK_FILE_CHOOSER_ERROR_STACK (object);

  g_clear_pointer (&self->stack, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_file_chooser_error_stack_parent_class)->dispose (object);
}

static void
gtk_file_chooser_error_stack_class_init (GtkFileChooserErrorStackClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_file_chooser_error_stack_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_file_chooser_error_stack_init (GtkFileChooserErrorStack *self)
{
  GtkWidget *label;
  GtkStack *stack;

  self->stack = gtk_stack_new ();
  gtk_widget_set_parent (self->stack, GTK_WIDGET (self));
  stack = GTK_STACK (self->stack);

  gtk_stack_set_transition_type (stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_set_transition_duration (stack, 50);

  label = gtk_label_new ("");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "no-error");

  label = gtk_label_new ("");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "custom");

  label = gtk_label_new (_("A folder cannot be called “.”"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "folder-cannot-be-called-dot");

  label = gtk_label_new (_("A file cannot be called “.”"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "file-cannot-be-called-dot");

  label = gtk_label_new (_("A folder cannot be called “..”"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "folder-cannot-be-called-dot-dot");

  label = gtk_label_new (_("A file cannot be called “..”"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "file-cannot-be-called-dot-dot");

  label = gtk_label_new (_("Folder names cannot contain “/”"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "folder-name-cannot-contain-slash");

  label = gtk_label_new (_("File names cannot contain “/”"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "file-name-cannot-contain-slash");

  label = gtk_label_new (_("Folder names should not begin with a space"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "folder-name-should-not-begin-with-space");

  label = gtk_label_new (_("File names should not begin with a space"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "file-name-should-not-begin-with-space");

  label = gtk_label_new (_("Folder names should not end with a space"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "folder-name-should-not-end-with-space");

  label = gtk_label_new (_("File names should not end with a space"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "file-name-should-not-end-with-space");

  label = gtk_label_new (_("Folder names starting with a “.” are hidden"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "folder-name-with-dot-is-hidden");

  label = gtk_label_new (_("File names starting with a “.” are hidden"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "file-name-with-dot-is-hidden");

  label = gtk_label_new (_("A folder with that name already exists"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "folder-name-already-exists");

  label = gtk_label_new (_("A file with that name already exists"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_stack_add_named (stack, label, "file-name-already-exists");

  gtk_stack_set_visible_child_name (stack, "no-error");
}

void
gtk_file_chooser_error_stack_set_error (GtkFileChooserErrorStack *self,
                                        gboolean                  is_folder,
                                        const char               *label_name)
{
  char *child_name;

  if (g_strcmp0 (label_name, "no-error") == 0)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "no-error");
      return;
    }

  child_name = g_strdup_printf ("%s-%s",
                                is_folder ? "folder" : "file",
                                label_name);

  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), child_name);

  g_free (child_name);
}


void
gtk_file_chooser_error_stack_set_custom_error  (GtkFileChooserErrorStack *self,
                                                const char               *label_text)
{
  GtkWidget *label = gtk_stack_get_child_by_name (GTK_STACK (self->stack), "custom");

  gtk_label_set_text (GTK_LABEL (label), label_text);

  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "custom");
}
