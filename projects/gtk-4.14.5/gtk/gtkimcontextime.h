/*
 * gtkimmoduleime
 * Copyright (C) 2003 Takuro Ashie
 * Copyright (C) 2003 Kazuki IWAMOTO
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
 *
 * $Id$
 */

#include <gtk/gtk.h>

#define GTK_TYPE_IM_CONTEXT_IME            (gtk_im_context_ime_get_type ())
#define GTK_IM_CONTEXT_IME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_IM_CONTEXT_IME, GtkIMContextIME))
#define GTK_IM_CONTEXT_IME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT_IME, GtkIMContextIMEClass))
#define GTK_IS_IM_CONTEXT_IME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_IM_CONTEXT_IME))
#define GTK_IS_IM_CONTEXT_IME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT_IME))
#define GTK_IM_CONTEXT_IME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT_IME, GtkIMContextIMEClass))

typedef struct _GtkIMContextIME GtkIMContextIME;
typedef struct _GtkIMContextIMEPrivate GtkIMContextIMEPrivate;
typedef struct _GtkIMContextIMEClass GtkIMContextIMEClass;

struct _GtkIMContextIME
{
  GtkIMContext object;

  GtkWidget *client_widget;
  GdkSurface *client_surface;
  guint use_preedit : 1;
  guint preediting : 1;
  guint opened : 1;
  guint focus : 1;
  GdkRectangle cursor_location;
  char *commit_string;

  GtkIMContextIMEPrivate *priv;
};

struct _GtkIMContextIMEClass
{
  GtkIMContextClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType         gtk_im_context_ime_get_type      (void) G_GNUC_CONST;

void          gtk_im_context_ime_register_type (GTypeModule * type_module);
GtkIMContext *gtk_im_context_ime_new           (void);
