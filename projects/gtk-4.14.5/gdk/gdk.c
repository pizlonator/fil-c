/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "version/gdkversionmacros.h"

#include "gdkresources.h"

#include "gdkconstructorprivate.h"
#include "gdkdebugprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkglcontextprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate.h"
#include <glib/gprintf.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <fribidi.h>

/* GTK has a general architectural assumption that gsize is pointer-sized
 * (equivalent to uintptr_t), making it non-portable to architectures like
 * CHERI where that isn't true. If a future release relaxes that
 * assumption, changes will be needed in numerous places.
 * See also https://gitlab.gnome.org/GNOME/glib/-/issues/2842 for the
 * equivalent in GLib, which would be a prerequisite. */
G_STATIC_ASSERT (sizeof (gsize) == sizeof (void *));
G_STATIC_ASSERT (G_ALIGNOF (gsize) == G_ALIGNOF (void *));

/**
 * GDK_WINDOWING_X11:
 *
 * The `GDK_WINDOWING_X11` macro is defined if the X11 backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the X11 backend.
 */

/**
 * GDK_WINDOWING_WIN32:
 *
 * The `GDK_WINDOWING_WIN32` macro is defined if the Win32 backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Win32 backend.
 */

/**
 * GDK_WINDOWING_MACOS:
 *
 * The `GDK_WINDOWING_MACOS` macro is defined if the MacOS backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the MacOS backend.
 */

/**
 * GDK_WINDOWING_WAYLAND:
 *
 * The `GDK_WINDOWING_WAYLAND` macro is defined if the Wayland backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Wayland backend.
 */

/**
 * GDK_DISABLE_DEPRECATION_WARNINGS:
 *
 * A macro that should be defined before including the gdk.h header.
 *
 * If it is defined, no compiler warnings will be produced for uses
 * of deprecated GDK APIs.
 */

typedef struct _GdkThreadsDispatch GdkThreadsDispatch;

struct _GdkThreadsDispatch
{
  GSourceFunc func;
  gpointer data;
  GDestroyNotify destroy;
};


/* Private variable declarations
 */
static int gdk_initialized = 0;                     /* 1 if the library is initialized,
                                                     * 0 otherwise.
                                                     */

static const GdkDebugKey gdk_debug_keys[] = {
  { "misc",            GDK_DEBUG_MISC, "Miscellaneous information" },
  { "events",          GDK_DEBUG_EVENTS, "Information about events" },
  { "dnd",             GDK_DEBUG_DND, "Information about Drag-and-Drop" },
  { "input",           GDK_DEBUG_INPUT, "Information about input (Windows)" },
  { "eventloop",       GDK_DEBUG_EVENTLOOP, "Information about event loop operation (Quartz)" },
  { "frames",          GDK_DEBUG_FRAMES, "Information about the frame clock" },
  { "settings",        GDK_DEBUG_SETTINGS, "Information about xsettings" },
  { "opengl",          GDK_DEBUG_OPENGL, "Information about OpenGL" },
  { "vulkan",          GDK_DEBUG_VULKAN, "Information about Vulkan" },
  { "selection",       GDK_DEBUG_SELECTION, "Information about selections" },
  { "clipboard",       GDK_DEBUG_CLIPBOARD, "Information about clipboards" },
  { "dmabuf",          GDK_DEBUG_DMABUF, "Information about dmabuf buffers" },
  { "offload",         GDK_DEBUG_OFFLOAD, "Information about subsurfaces and graphics offload" },

  { "portals",         GDK_DEBUG_PORTALS, "Force use of portals" },
  { "no-portals",      GDK_DEBUG_NO_PORTALS, "Disable use of portals" },
  { "gl-disable",      GDK_DEBUG_GL_DISABLE, "Disable OpenGL support" },
  { "gl-no-fractional", GDK_DEBUG_GL_NO_FRACTIONAL, "Disable fractional scaling for OpenGL" },
  { "gl-debug",        GDK_DEBUG_GL_DEBUG, "Insert debugging information in OpenGL" },
  { "gl-disable-gl",   GDK_DEBUG_GL_DISABLE_GL, "Only allow OpenGL GLES API" },
  { "gl-disable-gles", GDK_DEBUG_GL_DISABLE_GLES, "Don't allow OpenGL GLES API" },
  { "gl-prefer-gl",    GDK_DEBUG_GL_PREFER_GL, "Prefer GL over GLES API" },
  { "gl-egl",          GDK_DEBUG_GL_EGL, "Use EGL on X11 or Windows" },
  { "gl-glx",          GDK_DEBUG_GL_GLX, "Use GLX on X11" },
  { "gl-wgl",          GDK_DEBUG_GL_WGL, "Use WGL on Windows" },
  { "vulkan-disable",  GDK_DEBUG_VULKAN_DISABLE, "Disable Vulkan support" },
  { "vulkan-validate", GDK_DEBUG_VULKAN_VALIDATE, "Load the Vulkan validation layer" },
  { "default-settings",GDK_DEBUG_DEFAULT_SETTINGS, "Force default values for xsettings" },
  { "high-depth",      GDK_DEBUG_HIGH_DEPTH, "Use high bit depth rendering if possible" },
  { "no-vsync",        GDK_DEBUG_NO_VSYNC, "Repaint instantly (uses 100% CPU with animations)" },
  { "dmabuf-disable",  GDK_DEBUG_DMABUF_DISABLE, "Disable dmabuf support" },
};


#ifdef G_HAS_CONSTRUCTORS
#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(stash_and_unset_environment)
#endif
G_DEFINE_CONSTRUCTOR(stash_and_unset_environment)
#endif

static char *startup_notification_id = NULL;
static char *xdg_activation_token = NULL;

static void
stash_and_unset_environment (void)
{
  /* Copies environment variables and unsets them so they won't be inherited by
   * child processes and confuse things.
   *
   * Changing environment variables can be racy so we try to do this as early as
   * possible in the program flow and before any printing that might involve
   * environment variables.
   */
  struct {
    const char *key;
    char **dst;
  } vars[] = {
    { "DESKTOP_STARTUP_ID", &startup_notification_id },
    { "XDG_ACTIVATION_TOKEN", &xdg_activation_token },
  };
  size_t i;

  for (i = 0; i < G_N_ELEMENTS (vars); i++)
    *vars[i].dst = g_strdup (g_getenv (vars[i].key));

  for (i = 0; i < G_N_ELEMENTS (vars); i++)
    g_unsetenv (vars[i].key);

  for (i = 0; i < G_N_ELEMENTS (vars); i++)
    {
      if (*vars[i].dst == NULL)
        continue;

      if (!g_utf8_validate (*vars[i].dst, -1, NULL))
        {
          g_warning ("%s contains invalid UTF-8", vars[i].key);
          g_clear_pointer (vars[i].dst, g_free);
        }
    }
}

static gpointer
register_resources (gpointer dummy G_GNUC_UNUSED)
{
  _gdk_register_resource ();

  return NULL;
}

static void
gdk_ensure_resources (void)
{
  static GOnce register_resources_once = G_ONCE_INIT;

  g_once (&register_resources_once, register_resources, NULL);
}

guint
gdk_parse_debug_var (const char        *variable,
                     const GdkDebugKey *keys,
                     guint              nkeys)
{
  guint i;
  guint result = 0;
  const char *string;
  const char *p;
  const char *q;
  gboolean invert;
  gboolean help;

  string = g_getenv (variable);
  if (string == NULL)
    return 0;

  p = string;
  invert = FALSE;
  help = FALSE;

  while (*p)
    {
      q = strpbrk (p, ":;, \t");
      if (!q)
        q = p + strlen (p);

      if (3 == q - p && g_ascii_strncasecmp ("all", p, q - p) == 0)
        {
          invert = TRUE;
        }
      else if (4 == q - p && g_ascii_strncasecmp ("help", p, q - p) == 0)
        {
          help = TRUE;
        }
      else
        {
          for (i = 0; i < nkeys; i++)
            {
              if (strlen (keys[i].key) == q - p &&
                  g_ascii_strncasecmp (keys[i].key, p, q - p) == 0)
                {
                  result |= keys[i].value;
                  break;
                }
            }
          if (i == nkeys)
            fprintf (stderr, "Unrecognized value \"%.*s\". Try %s=help\n", (int) (q - p), p, variable);
         }

      p = q;
      if (*p)
        p++;
    }

  if (help)
    {
      int max_width = 4;
      for (i = 0; i < nkeys; i++)
        max_width = MAX (max_width, strlen (keys[i].key));
      max_width += 4;

      fprintf (stderr, "Supported %s values:\n", variable);
      for (i = 0; i < nkeys; i++) {
        fprintf (stderr, "  %s%*s%s\n", keys[i].key, (int)(max_width - strlen (keys[i].key)), " ", keys[i].help);
      }
      fprintf (stderr, "  %s%*s%s\n", "all", max_width - 3, " ", "Enable all values. Other given values are subtracted");
      fprintf (stderr, "  %s%*s%s\n", "help", max_width - 4, " ", "Print this help");
      fprintf (stderr, "\nMultiple values can be given, separated by : or space.\n");
    }

  if (invert)
    {
      guint all_flags = 0;

      for (i = 0; i < nkeys; i++)
        {
          all_flags |= keys[i].value;
        }

      result = all_flags & (~result);
    }

  return result;
}

void
gdk_pre_parse (void)
{
  gdk_initialized = TRUE;

  gdk_ensure_resources ();

  _gdk_debug_flags = gdk_parse_debug_var ("GDK_DEBUG",
                                          gdk_debug_keys,
                                          G_N_ELEMENTS (gdk_debug_keys));

  /* These are global */
  if (_gdk_debug_flags & GDK_DEBUG_GL_EGL)
    gdk_gl_backend_use (GDK_GL_EGL);
  else if (_gdk_debug_flags & GDK_DEBUG_GL_GLX)
    gdk_gl_backend_use (GDK_GL_GLX);
  else if (_gdk_debug_flags & GDK_DEBUG_GL_WGL)
    gdk_gl_backend_use (GDK_GL_WGL);

#ifndef G_HAS_CONSTRUCTORS
  stash_and_unset_environment ();
#endif
}

/*< private >
 * gdk_display_open_default:
 *
 * Opens the default display specified by command line arguments or
 * environment variables, sets it as the default display, and returns
 * it. If the default display has previously been set, simply returns
 * that. An internal function that should not be used by applications.
 *
 * Returns: (nullable) (transfer none): the default display, if it
 *   could be opened, otherwise %NULL.
 */
GdkDisplay *
gdk_display_open_default (void)
{
  GdkDisplay *display;

  g_return_val_if_fail (gdk_initialized, NULL);

  display = gdk_display_get_default ();
  if (display)
    return display;

  display = gdk_display_open (NULL);

  return display;
}

/*< private >
 * gdk_get_startup_notification_id:
 *
 * Returns the original value of the XDG_ACTIVATION_TOKEN environment
 * variable if it was defined and valid, otherwise it returns the original
 * value of the DESKTOP_STARTUP_ID environment variable if it was defined
 * and valid, or %NULL if neither of them were defined and valid.
 *
 * Returns: (nullable) (transfer none): the original value of the
 *   XDG_ACTIVATION_TOKEN or DESKTOP_STARTUP_ID environment variable
 */
const char *
gdk_get_startup_notification_id (void)
{
  if (xdg_activation_token)
    return xdg_activation_token;

  return startup_notification_id;
}

gboolean
gdk_running_in_sandbox (void)
{
  return g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

gboolean
gdk_should_use_portal (void)
{
  if (gdk_display_get_debug_flags (NULL) & GDK_DEBUG_PORTALS)
    return TRUE;

  if (gdk_display_get_debug_flags (NULL) & GDK_DEBUG_NO_PORTALS)
    return FALSE;

  if (gdk_running_in_sandbox ())
    return TRUE;

  return FALSE;
}

PangoDirection
gdk_unichar_direction (gunichar ch)
{
  FriBidiCharType fribidi_ch_type;

  G_STATIC_ASSERT (sizeof (FriBidiChar) == sizeof (gunichar));

  fribidi_ch_type = fribidi_get_bidi_type (ch);

  if (!FRIBIDI_IS_STRONG (fribidi_ch_type))
    return PANGO_DIRECTION_NEUTRAL;
  else if (FRIBIDI_IS_RTL (fribidi_ch_type))
    return PANGO_DIRECTION_RTL;
  else
    return PANGO_DIRECTION_LTR;
}

PangoDirection
gdk_find_base_dir (const char *text,
                   int          length)
{
  PangoDirection dir = PANGO_DIRECTION_NEUTRAL;
  const char *p;

  g_return_val_if_fail (text != NULL || length == 0, PANGO_DIRECTION_NEUTRAL);

  p = text;
  while ((length < 0 || p < text + length) && *p)
    {
      gunichar wc = g_utf8_get_char (p);

      dir = gdk_unichar_direction (wc);

      if (dir != PANGO_DIRECTION_NEUTRAL)
        break;

      p = g_utf8_next_char (p);
    }

  return dir;
}

void
gdk_source_set_static_name_by_id (guint           tag,
                                  const char     *name)
{
  GSource *source;

  g_return_if_fail (tag > 0);

  source = g_main_context_find_source_by_id (NULL, tag);
  if (source == NULL)
    return;

  g_source_set_static_name (source, name);
}
