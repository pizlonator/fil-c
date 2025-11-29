/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "gdkconfig.h"
#include "gdkdisplaymanagerprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkkeysprivate.h"
#include <glib/gi18n-lib.h>

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#include "x11/gdkprivate-x11.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkprivate-broadway.h"
#endif

#ifdef GDK_WINDOWING_MACOS
#include "macos/gdkmacosdisplay-private.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#include "win32/gdkprivate-win32.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkprivate-wayland.h"
#endif

/**
 * GdkDisplayManager:
 *
 * A singleton object that offers notification when displays appear or
 * disappear.
 *
 * You can use [func@Gdk.DisplayManager.get] to obtain the `GdkDisplayManager`
 * singleton, but that should be rarely necessary. Typically, initializing
 * GTK opens a display that you can work with without ever accessing the
 * `GdkDisplayManager`.
 *
 * The GDK library can be built with support for multiple backends.
 * The `GdkDisplayManager` object determines which backend is used
 * at runtime.
 *
 * In the rare case that you need to influence which of the backends
 * is being used, you can use [func@Gdk.set_allowed_backends]. Note
 * that you need to call this function before initializing GTK.
 *
 * ## Backend-specific code
 *
 * When writing backend-specific code that is supposed to work with
 * multiple GDK backends, you have to consider both compile time and
 * runtime. At compile time, use the `GDK_WINDOWING_X11`, `GDK_WINDOWING_WIN32`
 * macros, etc. to find out which backends are present in the GDK library
 * you are building your application against. At runtime, use type-check
 * macros like GDK_IS_X11_DISPLAY() to find out which backend is in use:
 *
 * ```c
 * #ifdef GDK_WINDOWING_X11
 *   if (GDK_IS_X11_DISPLAY (display))
 *     {
 *       // make X11-specific calls here
 *     }
 *   else
 * #endif
 * #ifdef GDK_WINDOWING_MACOS
 *   if (GDK_IS_MACOS_DISPLAY (display))
 *     {
 *       // make Quartz-specific calls here
*     }
 *   else
 * #endif
 *   g_error ("Unsupported GDK backend");
 * ```
 */

enum {
  PROP_0,
  PROP_DEFAULT_DISPLAY
};

enum {
  DISPLAY_OPENED,
  LAST_SIGNAL
};

static void gdk_display_manager_set_property (GObject                *object,
                                              guint                   prop_id,
                                              const GValue           *value,
                                              GParamSpec             *pspec);
static void gdk_display_manager_get_property (GObject                *object,
                                              guint                   prop_id,
                                              GValue                 *value,
                                              GParamSpec             *pspec);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkDisplayManager, gdk_display_manager, G_TYPE_OBJECT)

static void
gdk_display_manager_class_init (GdkDisplayManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_display_manager_set_property;
  object_class->get_property = gdk_display_manager_get_property;

  /**
   * GdkDisplayManager::display-opened:
   * @manager: the object on which the signal is emitted
   * @display: the opened display
   *
   * Emitted when a display is opened.
   */
  signals[DISPLAY_OPENED] =
    g_signal_new (g_intern_static_string ("display-opened"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDisplayManagerClass, display_opened),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  GDK_TYPE_DISPLAY);

  /**
   * GdkDisplayManager:default-display: (attributes org.gtk.Property.get=gdk_display_manager_get_default_display)
   *
   * The default display.
   */
  g_object_class_install_property (object_class,
                                   PROP_DEFAULT_DISPLAY,
                                   g_param_spec_object ("default-display", NULL, NULL,
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
}

static void
gdk_display_manager_init (GdkDisplayManager *manager)
{
}

static void
gdk_display_manager_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DEFAULT_DISPLAY:
      gdk_display_manager_set_default_display (GDK_DISPLAY_MANAGER (object),
                                               g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_display_manager_get_property (GObject      *object,
                                  guint         prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DEFAULT_DISPLAY:
      g_value_set_object (value,
                          gdk_display_manager_get_default_display (GDK_DISPLAY_MANAGER (object)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static const char *allowed_backends;

/**
 * gdk_set_allowed_backends:
 * @backends: a comma-separated list of backends
 *
 * Sets a list of backends that GDK should try to use.
 *
 * This can be useful if your application does not
 * work with certain GDK backends.
 *
 * By default, GDK tries all included backends.
 *
 * For example:
 *
 * ```c
 * gdk_set_allowed_backends ("wayland,macos,*");
 * ```
 *
 * instructs GDK to try the Wayland backend first, followed by the
 * MacOs backend, and then all others.
 *
 * If the `GDK_BACKEND` environment variable is set, it determines
 * what backends are tried in what order, while still respecting the
 * set of allowed backends that are specified by this function.
 *
 * The possible backend names are:
 *
 *   - `broadway`
 *   - `macos`
 *   - `wayland`.
 *   - `win32`
 *   - `x11`
 *
 * You can also include a `*` in the list to try all remaining backends.
 *
 * This call must happen prior to functions that open a display, such
 * as [func@Gdk.Display.open], `gtk_init()`, or `gtk_init_check()`
 * in order to take effect.
 */
void
gdk_set_allowed_backends (const char *backends)
{
  allowed_backends = g_strdup (backends);
}

typedef struct _GdkBackend GdkBackend;

struct _GdkBackend {
  const char *name;
  GdkDisplay * (* open_display) (const char *name);
};

static GdkBackend gdk_backends[] = {
#ifdef GDK_WINDOWING_MACOS
  { "macos",   _gdk_macos_display_open },
#endif
#ifdef GDK_WINDOWING_WIN32
  { "win32",    _gdk_win32_display_open },
#endif
#ifdef GDK_WINDOWING_WAYLAND
  { "wayland",  _gdk_wayland_display_open },
#endif
#ifdef GDK_WINDOWING_X11
  { "x11",      gdk_x11_display_open },
#endif
#ifdef GDK_WINDOWING_BROADWAY
  { "broadway", _gdk_broadway_display_open },
#endif
  /* NULL-terminating this array so we can use commas above */
  { NULL, NULL }
};

/**
 * gdk_display_manager_get:
 *
 * Gets the singleton `GdkDisplayManager` object.
 *
 * When called for the first time, this function consults the
 * `GDK_BACKEND` environment variable to find out which of the
 * supported GDK backends to use (in case GDK has been compiled
 * with multiple backends).
 *
 * Applications can use [func@set_allowed_backends] to limit what
 * backends will be used.
 *
 * Returns: (transfer none): The global `GdkDisplayManager` singleton
 */
GdkDisplayManager*
gdk_display_manager_get (void)
{
  static GdkDisplayManager *manager = NULL;

  if (manager == NULL)
    manager = g_object_new (GDK_TYPE_DISPLAY_MANAGER, NULL);

  return manager;
}

/**
 * gdk_display_manager_get_default_display: (attributes org.gtk.Method.get_property=default-display)
 * @manager: a `GdkDisplayManager`
 *
 * Gets the default `GdkDisplay`.
 *
 * Returns: (nullable) (transfer none): a `GdkDisplay`
 */
GdkDisplay *
gdk_display_manager_get_default_display (GdkDisplayManager *manager)
{
  return manager->default_display;
}

/**
 * gdk_display_get_default:
 *
 * Gets the default `GdkDisplay`.
 *
 * This is a convenience function for:
 *
 *     gdk_display_manager_get_default_display (gdk_display_manager_get ())
 *
 * Returns: (nullable) (transfer none): a `GdkDisplay`, or %NULL if
 *   there is no default display
 */
GdkDisplay *
gdk_display_get_default (void)
{
  return gdk_display_manager_get_default_display (gdk_display_manager_get ());
}

/**
 * gdk_display_manager_set_default_display:
 * @manager: a `GdkDisplayManager`
 * @display: a `GdkDisplay`
 *
 * Sets @display as the default display.
 */
void
gdk_display_manager_set_default_display (GdkDisplayManager *manager,
                                         GdkDisplay        *display)
{
  manager->default_display = display;

  if (display)
    GDK_DISPLAY_GET_CLASS (display)->make_default (display);

  g_object_notify (G_OBJECT (manager), "default-display");
}

/**
 * gdk_display_manager_list_displays:
 * @manager: a `GdkDisplayManager`
 *
 * List all currently open displays.
 *
 * Returns: (transfer container) (element-type GdkDisplay): a newly
 *   allocated `GSList` of `GdkDisplay` objects
 */
GSList *
gdk_display_manager_list_displays (GdkDisplayManager *manager)
{
  return g_slist_copy (manager->displays);
}

/**
 * gdk_display_manager_open_display:
 * @manager: a `GdkDisplayManager`
 * @name: (nullable): the name of the display to open
 *
 * Opens a display.
 *
 * Returns: (nullable) (transfer none): a `GdkDisplay`, or %NULL
 *   if the display could not be opened
 */
GdkDisplay *
gdk_display_manager_open_display (GdkDisplayManager *manager,
                                  const char        *name)
{
  const char *backend_list;
  GdkDisplay *display;
  char **backends;
  int i, j;
  gboolean allow_any;

  if (allowed_backends == NULL)
    allowed_backends = "*";
  allow_any = strstr (allowed_backends, "*") != NULL;

  backend_list = g_getenv ("GDK_BACKEND");
  if (backend_list == NULL)
    backend_list = allowed_backends;
  else if (g_strcmp0 (backend_list, "help") == 0)
    {
      fprintf (stderr, "Supported GDK backends:");
      for (i = 0; gdk_backends[i].name != NULL; i++)
        fprintf (stderr, " %s", gdk_backends[i].name);
      fprintf (stderr, "\n");

      backend_list = allowed_backends;
    }
  backends = g_strsplit (backend_list, ",", 0);

  display = NULL;

  for (i = 0; display == NULL && backends[i] != NULL; i++)
    {
      const char *backend = backends[i];
      gboolean any = g_str_equal (backend, "*");
      gboolean found = FALSE;

      if (!allow_any && !any && !strstr (allowed_backends, backend))
        {
          GDK_DEBUG (MISC, "Skipping %s backend", backend);
          continue;
        }

      for (j = 0; gdk_backends[j].name != NULL; j++)
        {
          if (g_str_equal (backend, gdk_backends[j].name))
            found = TRUE;

          if ((any && allow_any) ||
              (any && strstr (allowed_backends, gdk_backends[j].name)) ||
              g_str_equal (backend, gdk_backends[j].name))
            {
              GDK_DEBUG (MISC, "Trying %s backend", gdk_backends[j].name);
              display = gdk_backends[j].open_display (name);
              if (display)
                {
                  GDK_DEBUG (MISC, "Using %s display %s", gdk_backends[j].name, gdk_display_get_name (display));
                  break;
                }
            }
        }

      if (!found && !any && !display)
        g_warning ("No such backend: %s", backend);
    }

  g_strfreev (backends);

  return display;
}

void
_gdk_display_manager_add_display (GdkDisplayManager *manager,
                                  GdkDisplay        *display)
{
  if (manager->displays == NULL)
    gdk_display_manager_set_default_display (manager, display);

  manager->displays = g_slist_prepend (manager->displays, display);

  g_signal_emit (manager, signals[DISPLAY_OPENED], 0, display);
}

/* NB: This function can be called multiple times per display. */
void
_gdk_display_manager_remove_display (GdkDisplayManager *manager,
                                     GdkDisplay        *display)
{
  manager->displays = g_slist_remove (manager->displays, display);

  if (manager->default_display == display)
    {
      if (manager->displays)
        gdk_display_manager_set_default_display (manager, manager->displays->data);
      else
        gdk_display_manager_set_default_display (manager, NULL);
    }
}
