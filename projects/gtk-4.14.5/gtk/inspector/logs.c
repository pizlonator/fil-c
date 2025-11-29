/*
 * Copyright (c) 2018, Red Hat, Inc.
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

#include "logs.h"
#include "window.h"

#include "gtktextview.h"
#include "gtkcheckbutton.h"
#include "gtklabel.h"
#include "gtktooltip.h"
#include "gtktextiter.h"
#include "gtkprivate.h"
#include "gtkroot.h"
#include "gtkdebug.h"
#include "gtknative.h"
#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gtkboxlayout.h"

#include "gdk/gdkdebugprivate.h"

struct _GtkInspectorLogs
{
  GtkWidget parent;

  GtkWidget *box;
  GtkWidget *events;
  GtkWidget *misc;
  GtkWidget *dnd;
  GtkWidget *input;
  GtkWidget *eventloop;
  GtkWidget *frames;
  GtkWidget *settings;
  GtkWidget *opengl;
  GtkWidget *vulkan;
  GtkWidget *selection;
  GtkWidget *clipboard;
  GtkWidget *dmabuf;
  GtkWidget *offload;

  GtkWidget *renderer;
  GtkWidget *cairo;
  GtkWidget *vulkan_gsk;
  GtkWidget *shaders;
  GtkWidget *glyphcache;
  GtkWidget *verbose;

  GtkWidget *actions;
  GtkWidget *builder;
  GtkWidget *sizes;
  GtkWidget *icons;
  GtkWidget *keybindings;
  GtkWidget *modules;
  GtkWidget *printing;
  GtkWidget *tree;
  GtkWidget *text;
  GtkWidget *constraints;
  GtkWidget *layout;
  GtkWidget *a11y;

  GdkDisplay *display;
};

typedef struct _GtkInspectorLogsClass
{
  GtkWidgetClass parent;
} GtkInspectorLogsClass;

G_DEFINE_TYPE (GtkInspectorLogs, gtk_inspector_logs, GTK_TYPE_WIDGET)

static void
gtk_inspector_logs_init (GtkInspectorLogs *logs)
{
  gtk_widget_init_template (GTK_WIDGET (logs));
}

static void
dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), GTK_TYPE_INSPECTOR_LOGS);

  G_OBJECT_CLASS (gtk_inspector_logs_parent_class)->dispose (object);
}

static void
update_flag (GtkWidget *widget,
             guint     *flags,
             guint      flag)
{
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (widget)))
    *flags = *flags | flag;
  else
    *flags = *flags & ~flag;
}

static void
flag_toggled (GtkWidget        *button,
              GtkInspectorLogs *logs)
{
  guint flags;
  GList *toplevels, *l;

  flags = gdk_display_get_debug_flags (logs->display);
  update_flag (logs->events, &flags, GDK_DEBUG_EVENTS);
  update_flag (logs->misc, &flags, GDK_DEBUG_MISC);
  update_flag (logs->dnd, &flags, GDK_DEBUG_DND);
  update_flag (logs->input, &flags, GDK_DEBUG_INPUT);
  update_flag (logs->eventloop, &flags, GDK_DEBUG_EVENTLOOP);
  update_flag (logs->frames, &flags, GDK_DEBUG_FRAMES);
  update_flag (logs->settings, &flags, GDK_DEBUG_SETTINGS);
  update_flag (logs->opengl, &flags, GDK_DEBUG_OPENGL);
  update_flag (logs->vulkan, &flags, GDK_DEBUG_VULKAN);
  update_flag (logs->selection, &flags, GDK_DEBUG_SELECTION);
  update_flag (logs->clipboard, &flags, GDK_DEBUG_CLIPBOARD);
  update_flag (logs->dmabuf, &flags, GDK_DEBUG_DMABUF);
  update_flag (logs->offload, &flags, GDK_DEBUG_OFFLOAD);
  gdk_display_set_debug_flags (logs->display, flags);

  flags = gsk_get_debug_flags ();
  update_flag (logs->renderer, &flags, GSK_DEBUG_RENDERER);
  update_flag (logs->cairo, &flags, GSK_DEBUG_CAIRO);
  update_flag (logs->vulkan_gsk, &flags, GSK_DEBUG_VULKAN);
  update_flag (logs->shaders, &flags, GSK_DEBUG_SHADERS);
  update_flag (logs->glyphcache, &flags, GSK_DEBUG_GLYPH_CACHE);
  update_flag (logs->verbose, &flags, GSK_DEBUG_VERBOSE);
  gsk_set_debug_flags (flags);

  toplevels = gtk_window_list_toplevels ();
  for (l = toplevels; l; l = l->next)
    {
      GtkWidget *toplevel = l->data;

      if (gtk_root_get_display (GTK_ROOT (toplevel)) == logs->display)
        {
          GskRenderer *renderer = gtk_native_get_renderer (GTK_NATIVE (toplevel));
          if (renderer)
            gsk_renderer_set_debug_flags (renderer, flags);
        }
    }
  g_list_free (toplevels);

  flags = gtk_get_display_debug_flags (logs->display);
  update_flag (logs->actions, &flags, GTK_DEBUG_ACTIONS);
  update_flag (logs->builder, &flags, GTK_DEBUG_BUILDER);
  update_flag (logs->sizes, &flags, GTK_DEBUG_SIZE_REQUEST);
  update_flag (logs->icons, &flags, GTK_DEBUG_ICONTHEME);
  update_flag (logs->keybindings, &flags, GTK_DEBUG_KEYBINDINGS);
  update_flag (logs->modules, &flags, GTK_DEBUG_MODULES);
  update_flag (logs->printing, &flags, GTK_DEBUG_PRINTING);
  update_flag (logs->tree, &flags, GTK_DEBUG_TREE);
  update_flag (logs->text, &flags, GTK_DEBUG_TEXT);
  update_flag (logs->constraints, &flags, GTK_DEBUG_CONSTRAINTS);
  update_flag (logs->layout, &flags, GTK_DEBUG_LAYOUT);
  update_flag (logs->a11y, &flags, GTK_DEBUG_A11Y);
  gtk_set_display_debug_flags (logs->display, flags);
}

static void
gtk_inspector_logs_class_init (GtkInspectorLogsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/logs.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, events);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, misc);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, dnd);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, input);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, eventloop);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, frames);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, settings);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, opengl);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, vulkan);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, selection);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, clipboard);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, dmabuf);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, offload);

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, renderer);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, cairo);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, vulkan_gsk);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, shaders);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, glyphcache);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, verbose);

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, actions);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, builder);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, sizes);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, icons);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, keybindings);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, modules);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, printing);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, text);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, constraints);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, layout);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorLogs, a11y);
  gtk_widget_class_bind_template_callback (widget_class, flag_toggled);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

void
gtk_inspector_logs_set_display (GtkInspectorLogs *logs,
                                GdkDisplay *display)
{
  logs->display = display;
}

// vim: set et sw=2 ts=2:
