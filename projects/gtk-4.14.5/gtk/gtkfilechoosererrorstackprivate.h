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

#pragma once


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include "gtkstack.h"

G_BEGIN_DECLS

#define GTK_TYPE_FILE_CHOOSER_ERROR_STACK                 (gtk_file_chooser_error_stack_get_type ())
#define GTK_FILE_CHOOSER_ERROR_STACK(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_CHOOSER_ERROR_STACK, GtkFileChooserErrorStack))
#define GTK_FILE_CHOOSER_ERROR_STACK_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FILE_CHOOSER_ERROR_STACK, GtkFileChooserErrorStackClass))
#define GTK_IS_FILE_CHOOSER_ERROR_STACK(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_CHOOSER_ERROR_STACK))
#define GTK_IS_FILE_CHOOSER_ERROR_STACK_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FILE_CHOOSER_ERROR_STACK))
#define GTK_FILE_CHOOSER_ERROR_STACK_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FILE_CHOOSER_ERROR_STACK, GtkFileChooserErrorStackClass))

typedef struct _GtkFileChooserErrorStack             GtkFileChooserErrorStack;
typedef struct _GtkFileChooserErrorStackClass        GtkFileChooserErrorStackClass;

struct _GtkFileChooserErrorStack
{
  GtkWidget parent_instance;

  GtkWidget *stack;
};

struct _GtkFileChooserErrorStackClass
{
  GtkWidgetClass parent_class;
};

GType  gtk_file_chooser_error_stack_get_type          (void) G_GNUC_CONST;

void   gtk_file_chooser_error_stack_set_error         (GtkFileChooserErrorStack *self,
                                                       gboolean                  is_folder,
                                                       const char               *label_name);

void   gtk_file_chooser_error_stack_set_custom_error  (GtkFileChooserErrorStack *self,
                                                       const char               *label_text);

G_END_DECLS

