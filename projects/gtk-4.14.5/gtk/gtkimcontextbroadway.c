/*
 * gtkimmodulebroadway
 * Copyright (C) 2013 Alexander Larsson
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * $Id:$
 */

#include "config.h"
#include <string.h>

#include "gtk/gtkimcontextbroadway.h"
#include "gtk/gtkimmoduleprivate.h"

#include "gdk/broadway/gdkbroadway.h"

#define GTK_TYPE_IM_CONTEXT_BROADWAY (gtk_im_context_broadway_get_type ())
#define GTK_IM_CONTEXT_BROADWAY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_IM_CONTEXT_BROADWAY, GtkIMContextBroadway))
#define GTK_IM_CONTEXT_BROADWAY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_IM_CONTEXT_BROADWAY, GtkIMContextBroadwayClass))

typedef struct _GtkIMContextBroadway
{
  GtkIMContextSimple parent;
  GtkWidget *client_widget;
} GtkIMContextBroadway;

typedef struct _GtkIMContextBroadwayClass
{
  GtkIMContextSimpleClass parent_class;
} GtkIMContextBroadwayClass;

G_DEFINE_TYPE_WITH_CODE (GtkIMContextBroadway, gtk_im_context_broadway, GTK_TYPE_IM_CONTEXT_SIMPLE,
                         gtk_im_module_ensure_extension_point ();
                         g_io_extension_point_implement (GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "broadway",
                                                         0))

static void
broadway_set_client_widget (GtkIMContext *context, GtkWidget *widget)
{
  GtkIMContextBroadway *bw = GTK_IM_CONTEXT_BROADWAY (context);

  bw->client_widget = widget;
}

static void
broadway_focus_in (GtkIMContext *context)
{
  GtkIMContextBroadway *bw = GTK_IM_CONTEXT_BROADWAY (context);
  GdkDisplay *display;

  if (bw->client_widget)
    {
      display = gtk_widget_get_display (bw->client_widget);
      gdk_broadway_display_show_keyboard (GDK_BROADWAY_DISPLAY (display));
    }
}

static void
broadway_focus_out (GtkIMContext *context)
{
  GtkIMContextBroadway *bw = GTK_IM_CONTEXT_BROADWAY (context);
  GdkDisplay *display;

  if (bw->client_widget)
    {
      display = gtk_widget_get_display (bw->client_widget);
      gdk_broadway_display_hide_keyboard (GDK_BROADWAY_DISPLAY (display));
    }
}

static void
gtk_im_context_broadway_class_init (GtkIMContextBroadwayClass *class)
{
  GtkIMContextClass *klass = GTK_IM_CONTEXT_CLASS (class);

  klass->focus_in = broadway_focus_in;
  klass->focus_out = broadway_focus_out;
  klass->set_client_widget = broadway_set_client_widget;
}

static void
gtk_im_context_broadway_init (GtkIMContextBroadway *im_context)
{
}
