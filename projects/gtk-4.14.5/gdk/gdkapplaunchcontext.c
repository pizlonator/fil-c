/* gdkapplaunchcontext.c - Gtk+ implementation for GAppLaunchContext

   Copyright (C) 2007 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library. If not, see <http://www.gnu.org/licenses/>.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include "config.h"

#include "gdkapplaunchcontextprivate.h"
#include "gdkdisplay.h"
#include <glib/gi18n-lib.h>


/**
 * GdkAppLaunchContext:
 *
 * `GdkAppLaunchContext` handles launching an application in a graphical context.
 *
 * It is an implementation of `GAppLaunchContext` that provides startup
 * notification and allows to launch applications on a specific workspace.
 *
 * ## Launching an application
 *
 * ```c
 * GdkAppLaunchContext *context;
 *
 * context = gdk_display_get_app_launch_context (display);
 *
 * gdk_app_launch_context_set_timestamp (gdk_event_get_time (event));
 *
 * if (!g_app_info_launch_default_for_uri ("http://www.gtk.org", context, &error))
 *   g_warning ("Launching failed: %s\n", error->message);
 *
 * g_object_unref (context);
 * ```
 */

static void    gdk_app_launch_context_finalize    (GObject           *object);
static char * gdk_app_launch_context_get_display_name (GAppLaunchContext *context,
                                                        GAppInfo          *info,
                                                        GList             *files);
static char * gdk_app_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                                             GAppInfo          *info,
                                                             GList             *files);
static void    gdk_app_launch_context_launch_failed (GAppLaunchContext *context,
                                                     const char        *startup_notify_id);


enum
{
  PROP_0,
  PROP_DISPLAY
};

G_DEFINE_TYPE (GdkAppLaunchContext, gdk_app_launch_context, G_TYPE_APP_LAUNCH_CONTEXT)

static void
gdk_app_launch_context_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GdkAppLaunchContext *context = GDK_APP_LAUNCH_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, context->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_app_launch_context_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GdkAppLaunchContext *context = GDK_APP_LAUNCH_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      context->display = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_app_launch_context_class_init (GdkAppLaunchContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GAppLaunchContextClass *context_class = G_APP_LAUNCH_CONTEXT_CLASS (klass);

  gobject_class->set_property = gdk_app_launch_context_set_property,
  gobject_class->get_property = gdk_app_launch_context_get_property;

  gobject_class->finalize = gdk_app_launch_context_finalize;

  context_class->get_display = gdk_app_launch_context_get_display_name;
  context_class->get_startup_notify_id = gdk_app_launch_context_get_startup_notify_id;
  context_class->launch_failed = gdk_app_launch_context_launch_failed;

  /**
   * GdkAppLaunchContext:display: (attributes org.gtk.Property.get=gdk_app_launch_context_get_display)
   *
   * The display that the `GdkAppLaunchContext` is on.
   */
  g_object_class_install_property (gobject_class, PROP_DISPLAY,
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gdk_app_launch_context_init (GdkAppLaunchContext *context)
{
  context->workspace = -1;
}

static void
gdk_app_launch_context_finalize (GObject *object)
{
  GdkAppLaunchContext *context = GDK_APP_LAUNCH_CONTEXT (object);

  if (context->display)
    g_object_unref (context->display);

  if (context->icon)
    g_object_unref (context->icon);

  g_free (context->icon_name);

  G_OBJECT_CLASS (gdk_app_launch_context_parent_class)->finalize (object);
}

static char *
gdk_app_launch_context_get_display_name (GAppLaunchContext *context,
                                         GAppInfo          *info,
                                         GList             *files)
{
  GdkAppLaunchContext *ctx = GDK_APP_LAUNCH_CONTEXT (context);
  GdkDisplay *display;

  if (ctx->display)
    display = ctx->display;
  else
    display = gdk_display_get_default ();

  return g_strdup (gdk_display_get_name (display));
}

/**
 * gdk_app_launch_context_get_display: (attributes org.gtk.Method.get_property=display)
 * @context: a `GdkAppLaunchContext`
 *
 * Gets the `GdkDisplay` that @context is for.
 *
 * Returns: (transfer none): the display of @context
 */
GdkDisplay *
gdk_app_launch_context_get_display (GdkAppLaunchContext *context)
{
  g_return_val_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context), NULL);

  return context->display;
}

/**
 * gdk_app_launch_context_set_desktop:
 * @context: a `GdkAppLaunchContext`
 * @desktop: the number of a workspace, or -1
 *
 * Sets the workspace on which applications will be launched.
 *
 * This only works when running under a window manager that
 * supports multiple workspaces, as described in the
 * [Extended Window Manager Hints](http://www.freedesktop.org/Standards/wm-spec).
 * Specifically this sets the `_NET_WM_DESKTOP` property described
 * in that spec.
 *
 * This only works when using the X11 backend.
 *
 * When the workspace is not specified or @desktop is set to -1,
 * it is up to the window manager to pick one, typically it will
 * be the current workspace.
 */
void
gdk_app_launch_context_set_desktop (GdkAppLaunchContext *context,
                                    int                  desktop)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));

  context->workspace = desktop;
}

/**
 * gdk_app_launch_context_set_timestamp:
 * @context: a `GdkAppLaunchContext`
 * @timestamp: a timestamp
 *
 * Sets the timestamp of @context.
 *
 * The timestamp should ideally be taken from the event that
 * triggered the launch.
 *
 * Window managers can use this information to avoid moving the
 * focus to the newly launched application when the user is busy
 * typing in another window. This is also known as 'focus stealing
 * prevention'.
 */
void
gdk_app_launch_context_set_timestamp (GdkAppLaunchContext *context,
                                      guint32              timestamp)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));

  context->timestamp = timestamp;
}

/**
 * gdk_app_launch_context_set_icon:
 * @context: a `GdkAppLaunchContext`
 * @icon: (nullable): a `GIcon`
 *
 * Sets the icon for applications that are launched with this
 * context.
 *
 * Window Managers can use this information when displaying startup
 * notification.
 *
 * See also [method@Gdk.AppLaunchContext.set_icon_name].
 */
void
gdk_app_launch_context_set_icon (GdkAppLaunchContext *context,
                                 GIcon               *icon)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));
  g_return_if_fail (icon == NULL || G_IS_ICON (icon));

  if (context->icon)
    {
      g_object_unref (context->icon);
      context->icon = NULL;
    }

  if (icon)
    context->icon = g_object_ref (icon);
}

/**
 * gdk_app_launch_context_set_icon_name:
 * @context: a `GdkAppLaunchContext`
 * @icon_name: (nullable): an icon name
 *
 * Sets the icon for applications that are launched with this context.
 *
 * The @icon_name will be interpreted in the same way as the Icon field
 * in desktop files. See also [method@Gdk.AppLaunchContext.set_icon].
 *
 * If both @icon and @icon_name are set, the @icon_name takes priority.
 * If neither @icon or @icon_name is set, the icon is taken from either
 * the file that is passed to launched application or from the `GAppInfo`
 * for the launched application itself.
 */
void
gdk_app_launch_context_set_icon_name (GdkAppLaunchContext *context,
                                      const char          *icon_name)
{
  g_return_if_fail (GDK_IS_APP_LAUNCH_CONTEXT (context));

  g_free (context->icon_name);
  context->icon_name = g_strdup (icon_name);
}

static char *
gdk_app_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                              GAppInfo          *info,
                                              GList             *files)
{
 return NULL;
}

static void
gdk_app_launch_context_launch_failed (GAppLaunchContext *context,
                                      const char        *startup_notify_id)
{
}
