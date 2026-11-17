/* -*- mode:c; c-basic-offset: 8; indent-tabs-mode: nil; -*- */
/* Tool to set the property _GNOME_SESSION_ACCELERATED on the root window */
/*
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:
 *   Bastien Nocera <hadess@hadess.net>
 *   Matthias Clasen <mclasen@redhat.com>
 */

#include <gtk/gtk.h>
#include <epoxy/gl.h>

static char *
get_gtk_gles_renderer (void)
{
        GdkSurface *surface;
        GtkNative *native;
        GtkWidget *win;
        GdkGLContext *context;
        char *renderer = NULL;

        win = gtk_window_new ();
        gtk_widget_realize (win);
        native = gtk_widget_get_native (win);
        surface = gtk_native_get_surface (native);
        context = gdk_surface_create_gl_context (surface, NULL);
        if (!context)
                return NULL;
        gdk_gl_context_make_current (context);
        renderer = g_strdup ((char *) glGetString (GL_RENDERER));
        gdk_gl_context_clear_current ();
        g_object_unref (context);

        return renderer;
}

int
main (int argc, char **argv)
{
        g_autofree char *renderer_string = NULL;

        g_log_writer_default_set_use_stderr (TRUE);

        gtk_init ();

        renderer_string = get_gtk_gles_renderer ();
        if (renderer_string) {
                g_print ("%s", renderer_string);
                return 0;
        }
        return 1;
}
