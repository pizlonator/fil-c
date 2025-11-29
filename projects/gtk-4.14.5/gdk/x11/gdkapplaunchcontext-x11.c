/* gdkapplaunchcontext-x11.c - Gtk+ implementation for GAppLaunchContext

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

#include "gdkx11applaunchcontext.h"
#include "gdkapplaunchcontextprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate.h"

#include <glib.h>
#include <gio/gdesktopappinfo.h>

#include <string.h>
#include <unistd.h>

static char *
get_display_name (GFile     *file,
                  GFileInfo *info)
{
  char *name, *tmp;

  name = NULL;
  if (info)
    name = g_strdup (g_file_info_get_display_name (info));

  if (name == NULL)
    {
      name = g_file_get_basename (file);
      if (name == NULL)
        name = g_file_get_uri (file);

      if (!g_utf8_validate (name, -1, NULL))
        {
          tmp = name;
          name =
            g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
          g_free (tmp);
        }
    }

  return name;
}

static GIcon *
get_icon (GFile     *file,
          GFileInfo *info)
{
  GIcon *icon;

  icon = NULL;

  if (info)
    {
      icon = g_file_info_get_icon (info);
      if (icon)
        g_object_ref (icon);
    }

  return icon;
}

static char *
gicon_to_string (GIcon *icon)
{
  GFile *file;
  const char *const *names;

  if (G_IS_FILE_ICON (icon))
    {
      file = g_file_icon_get_file (G_FILE_ICON (icon));
      if (file)
        return g_file_get_path (file);
    }
  else if (G_IS_THEMED_ICON (icon))
    {
      names = g_themed_icon_get_names (G_THEMED_ICON (icon));
      if (names)
        return g_strdup (names[0]);
    }
  else if (G_IS_EMBLEMED_ICON (icon))
    {
      GIcon *base;

      base = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (icon));

      return gicon_to_string (base);
    }

  return NULL;
}

static void
end_startup_notification (GdkDisplay *display,
                          const char *startup_id)
{
  gdk_x11_display_broadcast_startup_message (display, "remove",
                                             "ID", startup_id,
                                             NULL);
}


/* This should be fairly long, as it's confusing to users if a startup
 * ends when it shouldn’t (it appears that the startup failed, and
 * they have to relaunch the app). Also the timeout only matters when
 * there are bugs and apps don’t end their own startup sequence.
 *
 * This timeout is a "last resort" timeout that ignores whether the
 * startup sequence has shown activity or not.  Metacity and the
 * tasklist have smarter, and correspondingly able-to-be-shorter
 * timeouts. The reason our timeout is dumb is that we don’t monitor
 * the sequence (don’t use an SnMonitorContext)
 */
#define STARTUP_TIMEOUT_LENGTH_SECONDS 30
#define STARTUP_TIMEOUT_LENGTH (STARTUP_TIMEOUT_LENGTH_SECONDS * 1000) /* ms */

typedef struct
{
  GdkDisplay *display;
  char *startup_id;
  gint64 time;
} StartupNotificationData;

static void
free_startup_notification_data (gpointer data)
{
  StartupNotificationData *sn_data = data;

  g_object_unref (sn_data->display);
  g_free (sn_data->startup_id);
  g_free (sn_data);
}

typedef struct
{
  GSList *contexts;
  guint timeout_id;
} StartupTimeoutData;

static void
free_startup_timeout (void *data)
{
  StartupTimeoutData *std;

  std = data;

  g_slist_free_full (std->contexts, free_startup_notification_data);

  if (std->timeout_id != 0)
    {
      g_source_remove (std->timeout_id);
      std->timeout_id = 0;
    }

  g_free (std);
}

static gboolean
startup_timeout (void *data)
{
  StartupTimeoutData *std;
  GSList *tmp;
  gint64 now;
  int min_timeout;

  std = data;

  min_timeout = STARTUP_TIMEOUT_LENGTH;

  now = g_get_monotonic_time ();

  tmp = std->contexts;
  while (tmp != NULL)
    {
      StartupNotificationData *sn_data;
      GSList *next;
      double elapsed;

      sn_data = tmp->data;
      next = tmp->next;

      elapsed = (now - sn_data->time) / 1000.0;

      if (elapsed >= STARTUP_TIMEOUT_LENGTH)
        {
          std->contexts = g_slist_remove (std->contexts, sn_data);
          end_startup_notification (sn_data->display, sn_data->startup_id);
          free_startup_notification_data (sn_data);
        }
      else
        {
          min_timeout = MIN (min_timeout, (STARTUP_TIMEOUT_LENGTH - elapsed));
        }

      tmp = next;
    }

  if (std->contexts == NULL)
    std->timeout_id = 0;
  else {
    std->timeout_id = g_timeout_add_seconds ((min_timeout + 500)/1000, startup_timeout, std);
    gdk_source_set_static_name_by_id (std->timeout_id, "[gtk] startup_timeout");
  }

  /* always remove this one, but we may have reinstalled another one. */
  return G_SOURCE_REMOVE;
}


static void
add_startup_timeout (GdkX11Screen *screen,
                     const char   *startup_id)
{
  StartupTimeoutData *data;
  StartupNotificationData *sn_data;

  data = g_object_get_data (G_OBJECT (screen), "appinfo-startup-data");

  if (data == NULL)
    {
      data = g_new (StartupTimeoutData, 1);
      data->contexts = NULL;
      data->timeout_id = 0;

      g_object_set_data_full (G_OBJECT (screen), "appinfo-startup-data",
                              data, free_startup_timeout);
    }

  sn_data = g_new (StartupNotificationData, 1);
  sn_data->display = g_object_ref (GDK_SCREEN_DISPLAY (screen));
  sn_data->startup_id = g_strdup (startup_id);
  sn_data->time = g_get_monotonic_time ();

  data->contexts = g_slist_prepend (data->contexts, sn_data);

  if (data->timeout_id == 0) {
    data->timeout_id = g_timeout_add_seconds (STARTUP_TIMEOUT_LENGTH_SECONDS,
                                              startup_timeout, data);
    gdk_source_set_static_name_by_id (data->timeout_id, "[gtk] startup_timeout");
  }
}


static char *
gdk_x11_app_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                                  GAppInfo          *info,
                                                  GList             *files)
{
  static int sequence = 0;
  GdkDisplay *display;
  GdkX11Screen *screen;
  int files_count;
  char *description;
  char *icon_name;
  const char *binary_name;
  const char *application_id;
  char *screen_str;
  char *workspace_str;
  GIcon *icon;
  guint32 timestamp;
  char *startup_id;
  GFileInfo *fileinfo;
  GdkAppLaunchContext *ctx;

  if (!info)
    return NULL;

  ctx = GDK_APP_LAUNCH_CONTEXT (context);

  display = ctx->display;
  screen = GDK_X11_DISPLAY (display)->screen;

  fileinfo = NULL;

  files_count = g_list_length (files);
  if (files_count == 0)
    {
      description = g_strdup_printf (_("Starting “%s”"), g_app_info_get_name (info));
    }
  else if (files_count == 1)
    {
      char *display_name;

      if (g_file_is_native (files->data))
        fileinfo = g_file_query_info (files->data,
                                      G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                      G_FILE_ATTRIBUTE_STANDARD_ICON,
                                      0, NULL, NULL);

      display_name = get_display_name (files->data, fileinfo);
      description = g_strdup_printf (_("Opening “%s”"), display_name);
      g_free (display_name);
    }
  else
    description = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                                "Opening %d Item",
                                                "Opening %d Items",
                                                files_count), files_count);

  icon_name = NULL;
  if (ctx->icon_name)
    icon_name = g_strdup (ctx->icon_name);
  else
    {
      icon = NULL;

      if (ctx->icon != NULL)
        icon = g_object_ref (ctx->icon);
      else if (files_count == 1)
        icon = get_icon (files->data, fileinfo);

      if (icon == NULL)
        {
          icon = g_app_info_get_icon (info);
          if (icon != NULL)
            g_object_ref (icon);
        }

      if (icon != NULL)
        {
          icon_name = gicon_to_string (icon);
          g_object_unref (icon);
        }
    }

  binary_name = g_app_info_get_executable (info);

  timestamp = ctx->timestamp;
  if (timestamp == GDK_CURRENT_TIME)
    timestamp = gdk_x11_display_get_user_time (display);

  screen_str = g_strdup_printf ("%d", gdk_x11_screen_get_screen_number (screen));
  if (ctx->workspace > -1)
    workspace_str = g_strdup_printf ("%d", ctx->workspace);
  else
    workspace_str = NULL;

  if (G_IS_DESKTOP_APP_INFO (info))
    application_id = g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (info));
  else
    application_id = NULL;

  startup_id = g_strdup_printf ("%s-%lu-%s-%s-%d_TIME%lu",
                                g_get_prgname (),
                                (unsigned long)getpid (),
                                g_get_host_name (),
                                binary_name,
                                sequence++,
                                (unsigned long)timestamp);

  gdk_x11_display_broadcast_startup_message (display, "new",
                                             "ID", startup_id,
                                             "NAME", g_app_info_get_name (info),
                                             "SCREEN", screen_str,
                                             "BIN", binary_name,
                                             "ICON", icon_name,
                                             "DESKTOP", workspace_str,
                                             "DESCRIPTION", description,
                                             "WMCLASS", NULL, /* FIXME */
                                             "APPLICATION_ID", application_id,
                                             NULL);

  g_free (description);
  g_free (screen_str);
  g_free (workspace_str);
  g_free (icon_name);
  if (fileinfo)
    g_object_unref (fileinfo);

  add_startup_timeout (screen, startup_id);

  return startup_id;
}


static void
gdk_x11_app_launch_context_launch_failed (GAppLaunchContext *context,
                                          const char        *startup_notify_id)
{
  GdkAppLaunchContext *ctx;
  GdkX11Screen *screen;
  StartupTimeoutData *data;
  StartupNotificationData *sn_data;
  GSList *l;

  ctx = GDK_APP_LAUNCH_CONTEXT (context);

  screen = GDK_X11_DISPLAY (ctx->display)->screen;

  data = g_object_get_data (G_OBJECT (screen), "appinfo-startup-data");

  if (data)
    {
      for (l = data->contexts; l != NULL; l = l->next)
        {
          sn_data = l->data;
          if (strcmp (startup_notify_id, sn_data->startup_id) == 0)
            {
              data->contexts = g_slist_remove (data->contexts, sn_data);
              end_startup_notification (sn_data->display, sn_data->startup_id);
              free_startup_notification_data (sn_data);

              break;
            }
        }

      if (data->contexts == NULL)
        {
          g_source_remove (data->timeout_id);
          data->timeout_id = 0;
        }
    }
}

struct _GdkX11AppLaunchContext
{
  GdkAppLaunchContext parent_instance;
};

struct _GdkX11AppLaunchContextClass
{
  GdkAppLaunchContextClass parent_class;
};


G_DEFINE_TYPE (GdkX11AppLaunchContext, gdk_x11_app_launch_context, GDK_TYPE_APP_LAUNCH_CONTEXT)

static void
gdk_x11_app_launch_context_class_init (GdkX11AppLaunchContextClass *klass)
{
  GAppLaunchContextClass *ctx_class = G_APP_LAUNCH_CONTEXT_CLASS (klass);

  ctx_class->get_startup_notify_id = gdk_x11_app_launch_context_get_startup_notify_id;
  ctx_class->launch_failed = gdk_x11_app_launch_context_launch_failed;
}

static void
gdk_x11_app_launch_context_init (GdkX11AppLaunchContext *ctx)
{
}

GdkAppLaunchContext *
_gdk_x11_display_get_app_launch_context (GdkDisplay *display)
{
  GdkAppLaunchContext *ctx;
  const char *display_name;

  ctx = g_object_new (GDK_TYPE_X11_APP_LAUNCH_CONTEXT,
                      "display", display,
                      NULL);

  display_name = gdk_display_get_name (display);
  if (display_name)
    g_app_launch_context_setenv (G_APP_LAUNCH_CONTEXT (ctx),
                                 "DISPLAY", display_name);

  return ctx;
}
