/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#define VK_USE_PLATFORM_WAYLAND_KHR

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysmacros.h>

#ifdef HAVE_LINUX_MEMFD_H
#include <linux/memfd.h>
#endif

#include <sys/mman.h>
#include <sys/syscall.h>

#include <glib.h>
#include <gio/gio.h>
#include "gdkwayland.h"
#include "gdkdisplay.h"
#include "gdkdisplay-wayland.h"
#include "gdkmonitor-wayland.h"
#include "gdkseat-wayland.h"
#include "gdksurface-wayland.h"
#include "gdksurfaceprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkkeysprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkcairocontext-wayland.h"
#include "gdkglcontext-wayland.h"
#include "gdkvulkancontext-wayland.h"
#include "gdkwaylandmonitor.h"
#include "gdkprofilerprivate.h"
#include "gdktoplevel-wayland-private.h"
#include <wayland/pointer-gestures-unstable-v1-client-protocol.h>
#include "tablet-unstable-v2-client-protocol.h"
#include <wayland/xdg-shell-unstable-v6-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v1-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v2-client-protocol.h>
#include <wayland/server-decoration-client-protocol.h>
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "presentation-time-client-protocol.h"

#include "wm-button-layout-translation.h"

#include "gdk/gdkprivate.h"

/* Keep g_assert() defined even if we disable it globally,
 * as we use it in many places as a handy mechanism to check
 * for non-NULL
 */
#ifdef G_DISABLE_ASSERT
# undef g_assert
# define g_assert(expr)                  G_STMT_START { \
                                           if G_LIKELY (expr) ; else \
                                             g_assertion_message_expr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                       #expr); \
                                         } G_STMT_END
#endif

/**
 * GdkWaylandDisplay:
 *
 * The Wayland implementation of `GdkDisplay`.
 *
 * Beyond the regular [class@Gdk.Display] API, the Wayland implementation
 * provides access to Wayland objects such as the `wl_display` with
 * [method@GdkWayland.WaylandDisplay.get_wl_display], the `wl_compositor` with
 * [method@GdkWayland.WaylandDisplay.get_wl_compositor].
 *
 * You can find out what Wayland globals are supported by a display
 * with [method@GdkWayland.WaylandDisplay.query_registry].
 */

#define MIN_SYSTEM_BELL_DELAY_MS 20

#define GTK_SHELL1_VERSION       5
#define OUTPUT_VERSION_WITH_DONE 2
#define NO_XDG_OUTPUT_DONE_SINCE_VERSION 3
#define OUTPUT_VERSION           3
#define XDG_WM_DIALOG_VERSION    1

#ifdef HAVE_TOPLEVEL_STATE_SUSPENDED
#define XDG_WM_BASE_VERSION      6
#else
#define XDG_WM_BASE_VERSION      5
#endif

static void _gdk_wayland_display_load_cursor_theme (GdkWaylandDisplay *display_wayland);

G_DEFINE_TYPE (GdkWaylandDisplay, gdk_wayland_display, GDK_TYPE_DISPLAY)

static void
async_roundtrip_callback (void               *data,
                          struct wl_callback *callback,
                          uint32_t            time)
{
  GdkWaylandDisplay *display_wayland = data;

  display_wayland->async_roundtrips =
    g_list_remove (display_wayland->async_roundtrips, callback);
  wl_callback_destroy (callback);
}

static const struct wl_callback_listener async_roundtrip_listener = {
  async_roundtrip_callback
};

static void
_gdk_wayland_display_async_roundtrip (GdkWaylandDisplay *display_wayland)
{
  struct wl_callback *callback;

  callback = wl_display_sync (display_wayland->wl_display);
  wl_callback_add_listener (callback,
                            &async_roundtrip_listener,
                            display_wayland);
  display_wayland->async_roundtrips =
    g_list_append (display_wayland->async_roundtrips, callback);
}

static void
xdg_wm_base_ping (void               *data,
                  struct xdg_wm_base *xdg_wm_base,
                  uint32_t            serial)
{
  GDK_DEBUG (EVENTS, "ping, shell %p, serial %u", xdg_wm_base, serial);

  xdg_wm_base_pong (xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
  xdg_wm_base_ping,
};

static void
zxdg_shell_v6_ping (void                 *data,
                    struct zxdg_shell_v6 *xdg_shell,
                    uint32_t              serial)
{
  GDK_DISPLAY_DEBUG (GDK_DISPLAY (data), EVENTS,
                     "ping, shell %p, serial %u", xdg_shell, serial);

  zxdg_shell_v6_pong (xdg_shell, serial);
}

static const struct zxdg_shell_v6_listener zxdg_shell_v6_listener = {
  zxdg_shell_v6_ping,
};

static gboolean
is_known_global (gpointer key, gpointer value, gpointer user_data)
{
  const char *required_global = user_data;
  const char *known_global = value;

  return g_strcmp0 (required_global, known_global) == 0;
}

static gboolean
has_required_globals (GdkWaylandDisplay *display_wayland,
                      const char *required_globals[])
{
  int i = 0;

  while (required_globals[i])
    {
      if (g_hash_table_find (display_wayland->known_globals,
                             is_known_global,
                             (gpointer)required_globals[i]) == NULL)
        return FALSE;

      i++;
    }

  return TRUE;
}

typedef struct _OnHasGlobalsClosure OnHasGlobalsClosure;

typedef void (*HasGlobalsCallback) (GdkWaylandDisplay *display_wayland,
                                    OnHasGlobalsClosure *closure);

struct _OnHasGlobalsClosure
{
  HasGlobalsCallback handler;
  const char **required_globals;
};

static void
process_on_globals_closures (GdkWaylandDisplay *display_wayland)
{
  GList *iter;

  iter = display_wayland->on_has_globals_closures;
  while (iter != NULL)
    {
      GList *next = iter->next;
      OnHasGlobalsClosure *closure = iter->data;

      if (has_required_globals (display_wayland,
                                closure->required_globals))
        {
          closure->handler (display_wayland, closure);
          g_free (closure);
          display_wayland->on_has_globals_closures =
            g_list_delete_link (display_wayland->on_has_globals_closures, iter);
        }

      iter = next;
    }
}

typedef struct
{
  OnHasGlobalsClosure base;
  uint32_t id;
  uint32_t version;
} SeatAddedClosure;

static void
_gdk_wayland_display_add_seat (GdkWaylandDisplay *display_wayland,
                               uint32_t id,
                               uint32_t version)
{
  struct wl_seat *seat;

  seat = wl_registry_bind (display_wayland->wl_registry,
                           id, &wl_seat_interface,
                           MIN (version, 8));
  _gdk_wayland_display_create_seat (display_wayland, id, seat);
  _gdk_wayland_display_async_roundtrip (display_wayland);
}

static void
seat_added_closure_run (GdkWaylandDisplay *display_wayland,
                        OnHasGlobalsClosure *closure)
{
  SeatAddedClosure *seat_added_closure = (SeatAddedClosure*)closure;

  _gdk_wayland_display_add_seat (display_wayland,
                                 seat_added_closure->id,
                                 seat_added_closure->version);
}

static void
postpone_on_globals_closure (GdkWaylandDisplay *display_wayland,
                             OnHasGlobalsClosure *closure)
{
  display_wayland->on_has_globals_closures =
    g_list_append (display_wayland->on_has_globals_closures, closure);
}

static void
wl_shm_format (void          *data,
               struct wl_shm *wl_shm,
               uint32_t       format)
{
  GDK_DEBUG (MISC, "supported shm pixel format %.4s (0x%X)",
             format == 0 ? "ARGB8888"
               : (format == 1 ? "XRGB8888"
               : (char *) &format), format);
}

static const struct wl_shm_listener wl_shm_listener = {
  wl_shm_format
};

static void
linux_dmabuf_done (void *data,
                   struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
  GDK_DEBUG (MISC, "dmabuf feedback done");
}

static void
linux_dmabuf_format_table (void *data,
                           struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                           int32_t fd,
                           uint32_t size)
{
  GdkWaylandDisplay *display_wayland = data;

  display_wayland->linux_dmabuf_n_formats = size / 16;
  display_wayland->linux_dmabuf_formats = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  GDK_DEBUG (MISC, "got dmabuf format table (%lu entries)", display_wayland->linux_dmabuf_n_formats);
}

static void
linux_dmabuf_main_device (void *data,
                          struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                          struct wl_array *device)
{
  dev_t dev G_GNUC_UNUSED;

  memcpy (&dev, device->data, sizeof (dev_t));

  GDK_DEBUG (MISC, "got dmabuf main device: %u %u", major (dev), minor (dev));
}

static void
linux_dmabuf_tranche_done (void *data,
                           struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
  GDK_DEBUG (MISC, "dmabuf feedback tranche done");
}

static void
linux_dmabuf_tranche_target_device (void *data,
                                    struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                    struct wl_array *device)
{
  dev_t dev G_GNUC_UNUSED;

  memcpy (&dev, device->data, sizeof (dev_t));

  GDK_DEBUG (MISC, "got dmabuf tranche target device: %u %u", major (dev), minor (dev));
}

static void
linux_dmabuf_tranche_formats (void *data,
                              struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                              struct wl_array *indices)
{
  GdkWaylandDisplay *display_wayland = data;

  GDK_DEBUG (MISC, "got dmabuf tranche formats (%lu entries):", indices->size / sizeof (guint16));
  guint16 *pos;

  wl_array_for_each (pos, indices)
    {
      LinuxDmabufFormat *fmt G_GNUC_UNUSED = &display_wayland->linux_dmabuf_formats[*pos];
      uint32_t f G_GNUC_UNUSED = fmt->fourcc;
      uint64_t m G_GNUC_UNUSED = fmt->modifier;
      GDK_DEBUG (MISC, "  %.4s:%#" G_GINT64_MODIFIER "x", (char *) &f, m);
    }
}

static void
linux_dmabuf_tranche_flags (void *data,
                            struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                            uint32_t flags)
{
  GDK_DEBUG (MISC,
             "got dmabuf tranche flags: %s",
             flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT ? "scanout" : "");
}

static const struct zwp_linux_dmabuf_feedback_v1_listener linux_dmabuf_feedback_listener = {
  linux_dmabuf_done,
  linux_dmabuf_format_table,
  linux_dmabuf_main_device,
  linux_dmabuf_tranche_done,
  linux_dmabuf_tranche_target_device,
  linux_dmabuf_tranche_formats,
  linux_dmabuf_tranche_flags,
};

static void
server_decoration_manager_default_mode (void                                          *data,
                                        struct org_kde_kwin_server_decoration_manager *manager,
                                        uint32_t                                       mode)
{
  g_assert (mode <= ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER);
  const char *modes[] = {
    [ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_NONE]   = "none",
    [ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_CLIENT] = "client",
    [ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER] = "server",
  };
  GdkWaylandDisplay *display_wayland = data;
  GDK_DISPLAY_DEBUG (GDK_DISPLAY (data), MISC, "Compositor prefers decoration mode '%s'", modes[mode]);
  display_wayland->server_decoration_mode = mode;
}

static const struct org_kde_kwin_server_decoration_manager_listener server_decoration_listener = {
  .default_mode = server_decoration_manager_default_mode
};

/*
 * gdk_wayland_display_prefers_ssd:
 * @display: (type GdkWaylandDisplay): a `GdkDisplay`
 *
 * Checks whether the Wayland compositor prefers to draw the window
 * decorations or if it leaves decorations to the application.
 *
 * Returns: %TRUE if the compositor prefers server-side decorations
 */
gboolean
gdk_wayland_display_prefers_ssd (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (display_wayland->server_decoration_manager)
    return display_wayland->server_decoration_mode == ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER;

  return FALSE;
}

static void gdk_wayland_display_set_has_gtk_shell (GdkWaylandDisplay *display_wayland);
static void gdk_wayland_display_add_output        (GdkWaylandDisplay *display_wayland,
                                                   guint32            id,
                                                   struct wl_output  *output,
                                                   guint32            version);
static void gdk_wayland_display_remove_output     (GdkWaylandDisplay *display_wayland,
                                                   guint32            id);
static void gdk_wayland_display_init_xdg_output   (GdkWaylandDisplay *display_wayland);
static void gdk_wayland_display_get_xdg_output    (GdkWaylandMonitor *monitor);

static void
gdk_registry_handle_global (void               *data,
                            struct wl_registry *registry,
                            uint32_t            id,
                            const char         *interface,
                            uint32_t            version)
{
  GdkWaylandDisplay *display_wayland = data;
  struct wl_output *output;

  GDK_DEBUG (MISC, "add global %u, interface %s, version %u", id, interface, version);

  if (strcmp (interface, "wl_compositor") == 0)
    {
      display_wayland->compositor =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wl_compositor_interface, MIN (version, 5));
    }
  else if (strcmp (interface, "wl_shm") == 0)
    {
      display_wayland->shm =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_shm_interface, 1);
      wl_shm_add_listener (display_wayland->shm, &wl_shm_listener, display_wayland);
    }
  else if (strcmp (interface, "zwp_linux_dmabuf_v1") == 0 && version >= 4)
    {
      display_wayland->linux_dmabuf =
        wl_registry_bind (display_wayland->wl_registry, id, &zwp_linux_dmabuf_v1_interface, version);
      display_wayland->linux_dmabuf_feedback =
        zwp_linux_dmabuf_v1_get_default_feedback (display_wayland->linux_dmabuf);
     zwp_linux_dmabuf_feedback_v1_add_listener (display_wayland->linux_dmabuf_feedback,
                                                &linux_dmabuf_feedback_listener, display_wayland);
      _gdk_wayland_display_async_roundtrip (display_wayland);
    }
  else if (strcmp (interface, "xdg_wm_base") == 0)
    {
      display_wayland->xdg_wm_base_id = id;
      display_wayland->xdg_wm_base_version = version;
    }
  else if (strcmp (interface, "zxdg_shell_v6") == 0)
    {
      display_wayland->zxdg_shell_v6_id = id;
    }
  else if (strcmp (interface, "xdg_wm_dialog_v1") == 0)
    {
      display_wayland->xdg_wm_dialog =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &xdg_wm_dialog_v1_interface,
                          MIN (version, XDG_WM_DIALOG_VERSION));
    }
  else if (strcmp (interface, "gtk_shell1") == 0)
    {
      display_wayland->gtk_shell =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &gtk_shell1_interface,
                          MIN (version, GTK_SHELL1_VERSION));
      gdk_wayland_display_set_has_gtk_shell (display_wayland);
    }
  else if (strcmp (interface, "wl_output") == 0)
    {
      output =
       wl_registry_bind (display_wayland->wl_registry, id, &wl_output_interface,
                         MIN (version, OUTPUT_VERSION));
      gdk_wayland_display_add_output (display_wayland, id, output,
                                      MIN (version, OUTPUT_VERSION));
      _gdk_wayland_display_async_roundtrip (display_wayland);
    }
  else if (strcmp (interface, "wl_seat") == 0)
    {
      SeatAddedClosure *closure;
      static const char *required_device_manager_globals[] = {
        "wl_compositor",
        "wl_data_device_manager",
        NULL
      };

      closure = g_new0 (SeatAddedClosure, 1);
      closure->base.handler = seat_added_closure_run;
      closure->base.required_globals = required_device_manager_globals;
      closure->id = id;
      closure->version = version;
      postpone_on_globals_closure (display_wayland, &closure->base);
    }
  else if (strcmp (interface, "wl_data_device_manager") == 0)
    {
      display_wayland->data_device_manager =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_data_device_manager_interface,
                          MIN (version, 3));
    }
  else if (strcmp (interface, "wl_subcompositor") == 0)
    {
      display_wayland->subcompositor =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_subcompositor_interface, 1);
    }
  else if (strcmp (interface, "zwp_pointer_gestures_v1") == 0)
    {
      display_wayland->pointer_gestures =
        wl_registry_bind (display_wayland->wl_registry,
                          id, &zwp_pointer_gestures_v1_interface,
                          MIN (version, GDK_ZWP_POINTER_GESTURES_V1_VERSION));
    }
  else if (strcmp (interface, "zwp_primary_selection_device_manager_v1") == 0)
    {
      display_wayland->primary_selection_manager =
        wl_registry_bind(display_wayland->wl_registry, id,
                         &zwp_primary_selection_device_manager_v1_interface, 1);
    }
  else if (strcmp (interface, "zwp_tablet_manager_v2") == 0)
    {
      display_wayland->tablet_manager =
        wl_registry_bind(display_wayland->wl_registry, id,
                         &zwp_tablet_manager_v2_interface, 1);
    }
  else if (strcmp (interface, "zxdg_exporter_v1") == 0)
    {
      display_wayland->xdg_exporter =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_exporter_v1_interface, 1);
    }
  else if (strcmp (interface, "zxdg_exporter_v2") == 0)
    {
      display_wayland->xdg_exporter_v2 =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_exporter_v2_interface, 1);
    }
  else if (strcmp (interface, "zxdg_importer_v1") == 0)
    {
      display_wayland->xdg_importer =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_importer_v1_interface, 1);
    }
  else if (strcmp (interface, "zxdg_importer_v2") == 0)
    {
      display_wayland->xdg_importer_v2 =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_importer_v2_interface, 1);
    }
  else if (strcmp (interface, "zwp_keyboard_shortcuts_inhibit_manager_v1") == 0)
    {
      display_wayland->keyboard_shortcuts_inhibit =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1);
    }
  else if (strcmp (interface, "org_kde_kwin_server_decoration_manager") == 0)
    {
      display_wayland->server_decoration_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &org_kde_kwin_server_decoration_manager_interface, 1);
      org_kde_kwin_server_decoration_manager_add_listener (display_wayland->server_decoration_manager,
                                                           &server_decoration_listener,
                                                           display_wayland);
    }
  else if (strcmp(interface, "zxdg_output_manager_v1") == 0)
    {
      display_wayland->xdg_output_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_output_manager_v1_interface,
                          MIN (version, 3));
      gdk_wayland_display_init_xdg_output (display_wayland);
      _gdk_wayland_display_async_roundtrip (display_wayland);
    }
  else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0)
    {
      display_wayland->idle_inhibit_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zwp_idle_inhibit_manager_v1_interface, 1);
    }
  else if (strcmp (interface, "xdg_activation_v1") == 0)
    {
      display_wayland->xdg_activation =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &xdg_activation_v1_interface, 1);
    }
  else if (strcmp (interface, "wp_fractional_scale_manager_v1") == 0)
    {
      display_wayland->fractional_scale =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_fractional_scale_manager_v1_interface, 1);
    }
  else if (strcmp (interface, "wp_viewporter") == 0)
    {
      display_wayland->viewporter =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_viewporter_interface, 1);
    }
  else if (strcmp (interface, "wp_presentation") == 0)
    {
      display_wayland->presentation =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_presentation_interface,
                          MIN (version, 1));
    }


  g_hash_table_insert (display_wayland->known_globals,
                       GUINT_TO_POINTER (id), g_strdup (interface));
}

static void
gdk_registry_handle_global_remove (void               *data,
                                   struct wl_registry *registry,
                                   uint32_t            id)
{
  GdkWaylandDisplay *display_wayland = data;

  GDK_DEBUG (MISC, "remove global %u", id);

  _gdk_wayland_display_remove_seat (display_wayland, id);
  gdk_wayland_display_remove_output (display_wayland, id);

  g_hash_table_remove (display_wayland->known_globals, GUINT_TO_POINTER (id));

  /* FIXME: the object needs to be destroyed here, we're leaking */
}

static const struct wl_registry_listener registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

G_GNUC_PRINTF (1, 0)
static void
log_handler (const char *format, va_list args)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
}

static void
load_cursor_theme_closure_run (GdkWaylandDisplay *display_wayland,
                               OnHasGlobalsClosure *closure)
{
  _gdk_wayland_display_load_cursor_theme (display_wayland);
}

static void
_gdk_wayland_display_prepare_cursor_themes (GdkWaylandDisplay *display_wayland)
{
  OnHasGlobalsClosure *closure;
  static const char *required_cursor_theme_globals[] = {
      "wl_shm",
      NULL
  };

  closure = g_new0 (OnHasGlobalsClosure, 1);
  closure->handler = load_cursor_theme_closure_run;
  closure->required_globals = required_cursor_theme_globals;
  postpone_on_globals_closure (display_wayland, closure);
}

static void init_settings (GdkDisplay *display);

GdkDisplay *
_gdk_wayland_display_open (const char *display_name)
{
  struct wl_display *wl_display;
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;

  GDK_DEBUG (MISC, "opening display %s", display_name ? display_name : "");

  wl_log_set_handler_client (log_handler);

  wl_display = wl_display_connect (display_name);
  if (!wl_display)
    return NULL;

  display = g_object_new (GDK_TYPE_WAYLAND_DISPLAY, NULL);
  display_wayland = GDK_WAYLAND_DISPLAY (display);
  display_wayland->wl_display = wl_display;
  display_wayland->event_source = _gdk_wayland_display_event_source_new (display);

  init_settings (display);

  display_wayland->known_globals =
    g_hash_table_new_full (NULL, NULL, NULL, g_free);

  _gdk_wayland_display_init_cursors (display_wayland);
  _gdk_wayland_display_prepare_cursor_themes (display_wayland);

  display_wayland->wl_registry = wl_display_get_registry (display_wayland->wl_display);
  wl_registry_add_listener (display_wayland->wl_registry, &registry_listener, display_wayland);
  if (wl_display_roundtrip (display_wayland->wl_display) < 0)
    {
      g_object_unref (display);
      return NULL;
    }

  process_on_globals_closures (display_wayland);

  /* Wait for initializing to complete. This means waiting for all
   * asynchronous roundtrips that were triggered during initial roundtrip.
   */
  while (display_wayland->async_roundtrips != NULL)
    {
      if (wl_display_dispatch (display_wayland->wl_display) < 0)
        {
          g_object_unref (display);
          return NULL;
        }
    }

  /* Check that we got all the required globals */
  if (display_wayland->compositor == NULL ||
      display_wayland->shm == NULL ||
      display_wayland->data_device_manager == NULL)
    {
      g_warning ("The Wayland compositor does not provide one or more of the required interfaces, "
                 "not using Wayland display");
      g_object_unref (display);

      return NULL;
    }

  if (display_wayland->xdg_wm_base_id)
    {
      display_wayland->shell_variant = GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL;
      display_wayland->xdg_wm_base =
        wl_registry_bind (display_wayland->wl_registry,
                          display_wayland->xdg_wm_base_id,
                          &xdg_wm_base_interface,
                          MIN (display_wayland->xdg_wm_base_version,
                               XDG_WM_BASE_VERSION));
      xdg_wm_base_add_listener (display_wayland->xdg_wm_base,
                                &xdg_wm_base_listener,
                                display_wayland);
    }
  else if (display_wayland->zxdg_shell_v6_id)
    {
      display_wayland->shell_variant = GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6;
      display_wayland->zxdg_shell_v6 =
        wl_registry_bind (display_wayland->wl_registry,
                          display_wayland->zxdg_shell_v6_id,
                          &zxdg_shell_v6_interface, 1);
      zxdg_shell_v6_add_listener (display_wayland->zxdg_shell_v6,
                                  &zxdg_shell_v6_listener,
                                  display_wayland);
    }
  else
    {
      g_warning ("The Wayland compositor does not provide any supported shell interface, "
                 "not using Wayland display");
      g_object_unref (display);

      return NULL;
    }

  gdk_display_emit_opened (display);

  return display;
}

static void
destroy_toplevel (gpointer data)
{
  _gdk_surface_destroy (GDK_SURFACE (data), FALSE);
}

static void
gdk_wayland_display_dispose (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

  g_list_free_full (display_wayland->toplevels, destroy_toplevel);

  if (display_wayland->event_source)
    {
      g_source_destroy (display_wayland->event_source);
      g_source_unref (display_wayland->event_source);
      display_wayland->event_source = NULL;
    }

  g_list_free_full (display_wayland->async_roundtrips, (GDestroyNotify) wl_callback_destroy);

  if (display_wayland->known_globals)
    {
      g_hash_table_destroy (display_wayland->known_globals);
      display_wayland->known_globals = NULL;
    }

  g_list_free_full (display_wayland->on_has_globals_closures, g_free);

  g_clear_pointer (&display_wayland->cursor_theme, wl_cursor_theme_destroy);
  g_clear_pointer (&display_wayland->compositor, wl_compositor_destroy);
  g_clear_pointer (&display_wayland->xdg_wm_base, xdg_wm_base_destroy);
  g_clear_pointer (&display_wayland->zxdg_shell_v6, zxdg_shell_v6_destroy);
  g_clear_pointer (&display_wayland->gtk_shell, gtk_shell1_destroy);
  g_clear_pointer (&display_wayland->data_device_manager, wl_data_device_manager_destroy);
  g_clear_pointer (&display_wayland->subcompositor, wl_subcompositor_destroy);
  g_clear_pointer (&display_wayland->pointer_gestures, zwp_pointer_gestures_v1_destroy);
  g_clear_pointer (&display_wayland->primary_selection_manager, zwp_primary_selection_device_manager_v1_destroy);
  g_clear_pointer (&display_wayland->tablet_manager, zwp_tablet_manager_v2_destroy);
  g_clear_pointer (&display_wayland->xdg_exporter, zxdg_exporter_v1_destroy);
  g_clear_pointer (&display_wayland->xdg_exporter_v2, zxdg_exporter_v2_destroy);
  g_clear_pointer (&display_wayland->xdg_importer, zxdg_importer_v1_destroy);
  g_clear_pointer (&display_wayland->xdg_importer_v2, zxdg_importer_v2_destroy);
  g_clear_pointer (&display_wayland->keyboard_shortcuts_inhibit, zwp_keyboard_shortcuts_inhibit_manager_v1_destroy);
  g_clear_pointer (&display_wayland->server_decoration_manager, org_kde_kwin_server_decoration_manager_destroy);
  g_clear_pointer (&display_wayland->xdg_output_manager, zxdg_output_manager_v1_destroy);
  g_clear_pointer (&display_wayland->idle_inhibit_manager, zwp_idle_inhibit_manager_v1_destroy);
  g_clear_pointer (&display_wayland->xdg_activation, xdg_activation_v1_destroy);
  g_clear_pointer (&display_wayland->fractional_scale, wp_fractional_scale_manager_v1_destroy);
  g_clear_pointer (&display_wayland->viewporter, wp_viewporter_destroy);
  g_clear_pointer (&display_wayland->presentation, wp_presentation_destroy);
  g_clear_pointer (&display_wayland->linux_dmabuf, zwp_linux_dmabuf_v1_destroy);
  g_clear_pointer (&display_wayland->linux_dmabuf_feedback, zwp_linux_dmabuf_feedback_v1_destroy);
  if (display_wayland->linux_dmabuf_formats)
    munmap (display_wayland->linux_dmabuf_formats, display_wayland->linux_dmabuf_n_formats * 16);

  g_clear_pointer (&display_wayland->shm, wl_shm_destroy);
  g_clear_pointer (&display_wayland->wl_registry, wl_registry_destroy);

  g_list_store_remove_all (display_wayland->monitors);

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->dispose (object);

  g_clear_pointer (&display_wayland->wl_display, wl_display_disconnect);
}

static void
gdk_wayland_display_finalize (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

  _gdk_wayland_display_finalize_cursors (display_wayland);

  g_object_unref (display_wayland->monitors);

  g_free (display_wayland->startup_notification_id);
  g_free (display_wayland->cursor_theme_name);
  xkb_context_unref (display_wayland->xkb_context);

  if (display_wayland->settings)
    g_hash_table_destroy (display_wayland->settings);

  g_clear_object (&display_wayland->settings_portal);

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->finalize (object);
}

static const char *
gdk_wayland_display_get_name (GdkDisplay *display)
{
  const char *name;

  name = g_getenv ("WAYLAND_DISPLAY");
  if (name == NULL)
    name = "wayland-0";

  return name;
}

void
gdk_wayland_display_system_bell (GdkDisplay *display,
                                 GdkSurface  *window)
{
  GdkWaylandDisplay *display_wayland;
  struct gtk_surface1 *gtk_surface;
  gint64 now_ms;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (!display_wayland->gtk_shell)
    return;

  if (window && GDK_IS_WAYLAND_TOPLEVEL (window))
    gtk_surface = gdk_wayland_toplevel_get_gtk_surface (GDK_WAYLAND_TOPLEVEL (window));
  else
    gtk_surface = NULL;

  now_ms = g_get_monotonic_time () / 1000;
  if (now_ms - display_wayland->last_bell_time_ms < MIN_SYSTEM_BELL_DELAY_MS)
    return;

  display_wayland->last_bell_time_ms = now_ms;

  gtk_shell1_system_bell (display_wayland->gtk_shell, gtk_surface);
}

static void
gdk_wayland_display_beep (GdkDisplay *display)
{
  gdk_wayland_display_system_bell (display, NULL);
}

static void
gdk_wayland_display_sync (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_wayland = GDK_WAYLAND_DISPLAY (display);

  wl_display_roundtrip (display_wayland->wl_display);
}

static void
gdk_wayland_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (!display->closed)
    wl_display_flush (GDK_WAYLAND_DISPLAY (display)->wl_display);
}

static void
gdk_wayland_display_make_default (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  const char *startup_id;

  g_free (display_wayland->startup_notification_id);
  display_wayland->startup_notification_id = NULL;

  startup_id = gdk_get_startup_notification_id ();
  if (startup_id)
    display_wayland->startup_notification_id = g_strdup (startup_id);
}

static gboolean
gdk_wayland_display_has_pending (GdkDisplay *display)
{
  return FALSE;
}

static gulong
gdk_wayland_display_get_next_serial (GdkDisplay *display)
{
  static gulong serial = 0;
  return ++serial;
}

/**
 * gdk_wayland_display_get_startup_notification_id:
 * @display: (type GdkWaylandDisplay): a `GdkDisplay`
 *
 * Gets the startup notification ID for a Wayland display, or %NULL
 * if no ID has been defined.
 *
 * Returns: (nullable): the startup notification ID for @display
 *
 * Deprecated: 4.10.
 */
const char *
gdk_wayland_display_get_startup_notification_id (GdkDisplay  *display)
{
  return GDK_WAYLAND_DISPLAY (display)->startup_notification_id;
}

/**
 * gdk_wayland_display_set_startup_notification_id:
 * @display: (type GdkWaylandDisplay): a `GdkDisplay`
 * @startup_id: the startup notification ID (must be valid utf8)
 *
 * Sets the startup notification ID for a display.
 *
 * This is usually taken from the value of the `DESKTOP_STARTUP_ID`
 * environment variable, but in some cases (such as the application not
 * being launched using exec()) it can come from other sources.
 *
 * The startup ID is also what is used to signal that the startup is
 * complete (for example, when opening a window or when calling
 * [method@Gdk.Display.notify_startup_complete]).
 *
 * Deprecated: 4.10. Use [method@Gdk.Toplevel.set_startup_id]
 */
void
gdk_wayland_display_set_startup_notification_id (GdkDisplay *display,
                                                 const char *startup_id)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_free (display_wayland->startup_notification_id);
  display_wayland->startup_notification_id = g_strdup (startup_id);
}

static void
gdk_wayland_display_notify_startup_complete (GdkDisplay  *display,
					     const char *startup_id)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  char *free_this = NULL;

  /* Will be signaled with focus activation */
  if (display_wayland->xdg_activation)
    return;

  if (startup_id == NULL)
    {
      startup_id = free_this = display_wayland->startup_notification_id;
      display_wayland->startup_notification_id = NULL;

      if (startup_id == NULL)
        return;
    }

  if (!display_wayland->xdg_activation && display_wayland->gtk_shell)
    gtk_shell1_set_startup_id (display_wayland->gtk_shell, startup_id);

  g_free (free_this);
}

static GdkKeymap *
_gdk_wayland_display_get_keymap (GdkDisplay *display)
{
  GdkSeat *seat;
  GdkDevice *core_keyboard = NULL;
  static GdkKeymap *tmp_keymap = NULL;

  seat = gdk_display_get_default_seat (display);
  if (seat)
    core_keyboard = gdk_seat_get_keyboard (seat);

  if (core_keyboard && tmp_keymap)
    {
      g_object_unref (tmp_keymap);
      tmp_keymap = NULL;
    }

  if (core_keyboard)
    return _gdk_wayland_device_get_keymap (core_keyboard);

  if (!tmp_keymap)
    tmp_keymap = _gdk_wayland_keymap_new (display);

  return tmp_keymap;
}

static GListModel *
gdk_wayland_display_get_monitors (GdkDisplay *display)
{
  GdkWaylandDisplay *self = GDK_WAYLAND_DISPLAY (display);

  return G_LIST_MODEL (self->monitors);
}

static GdkMonitor *
gdk_wayland_display_get_monitor_at_surface (GdkDisplay *display,
                                           GdkSurface  *window)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  struct wl_output *output;
  guint i, n;

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (window), NULL);

  output = gdk_wayland_surface_get_wl_output (window);
  if (output == NULL)
    return NULL;

  n = g_list_model_get_n_items (G_LIST_MODEL (display_wayland->monitors));
  for (i = 0; i < n; i++)
    {
      GdkMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (display_wayland->monitors), i);

      g_object_unref (monitor);

      if (gdk_wayland_monitor_get_wl_output (monitor) == output)
        return monitor;
    }

  return NULL;
}

static gboolean gdk_wayland_display_get_setting (GdkDisplay *display,
                                                 const char *name,
                                                 GValue     *value);

static void
gdk_wayland_display_class_init (GdkWaylandDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_wayland_display_dispose;
  object_class->finalize = gdk_wayland_display_finalize;

  display_class->toplevel_type = GDK_TYPE_WAYLAND_TOPLEVEL;
  display_class->popup_type = GDK_TYPE_WAYLAND_POPUP;
  display_class->cairo_context_type = GDK_TYPE_WAYLAND_CAIRO_CONTEXT;

#ifdef GDK_RENDERING_VULKAN
  display_class->vk_context_type = GDK_TYPE_WAYLAND_VULKAN_CONTEXT;
  display_class->vk_extension_name = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#endif

  display_class->get_name = gdk_wayland_display_get_name;
  display_class->beep = gdk_wayland_display_beep;
  display_class->sync = gdk_wayland_display_sync;
  display_class->flush = gdk_wayland_display_flush;
  display_class->make_default = gdk_wayland_display_make_default;
  display_class->has_pending = gdk_wayland_display_has_pending;
  display_class->queue_events = _gdk_wayland_display_queue_events;
  display_class->get_app_launch_context = _gdk_wayland_display_get_app_launch_context;
  display_class->get_next_serial = gdk_wayland_display_get_next_serial;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  display_class->get_startup_notification_id = gdk_wayland_display_get_startup_notification_id;
G_GNUC_END_IGNORE_DEPRECATIONS
  display_class->notify_startup_complete = gdk_wayland_display_notify_startup_complete;
  display_class->get_keymap = _gdk_wayland_display_get_keymap;

  display_class->init_gl = gdk_wayland_display_init_gl;

  display_class->get_monitors = gdk_wayland_display_get_monitors;
  display_class->get_monitor_at_surface = gdk_wayland_display_get_monitor_at_surface;
  display_class->get_setting = gdk_wayland_display_get_setting;
  display_class->set_cursor_theme = gdk_wayland_display_set_cursor_theme;
}

static void
gdk_wayland_display_init (GdkWaylandDisplay *display)
{
  display->xkb_context = xkb_context_new (0);

  display->monitors = g_list_store_new (GDK_TYPE_MONITOR);
}

GList *
gdk_wayland_display_get_toplevel_surfaces (GdkDisplay *display)
{
  return GDK_WAYLAND_DISPLAY (display)->toplevels;
}

static struct wl_cursor_theme *
try_load_theme (GdkWaylandDisplay *display_wayland,
                const char        *dir,
                gboolean           dotdir,
                const char        *name,
                int                size)
{
  struct wl_cursor_theme *theme = NULL;
  char *path;

  path = g_build_filename (dir, dotdir ? ".icons" : "icons", name, "cursors", NULL);

  if (g_file_test (path, G_FILE_TEST_IS_DIR))
    theme = wl_cursor_theme_create (path, size, display_wayland->shm);

  g_free (path);

  return theme;
}

static struct wl_cursor_theme *
get_cursor_theme (GdkWaylandDisplay *display_wayland,
                  const char *name,
                  int size)
{
  const char * const *xdg_data_dirs;
  struct wl_cursor_theme *theme = NULL;
  int i;

  theme = try_load_theme (display_wayland, g_get_user_data_dir (), FALSE, name, size);
  if (theme)
    return theme;

  theme = try_load_theme (display_wayland, g_get_home_dir (), TRUE, name, size);
  if (theme)
    return theme;

  xdg_data_dirs = g_get_system_data_dirs ();
  for (i = 0; xdg_data_dirs[i]; i++)
    {
      theme = try_load_theme (display_wayland, xdg_data_dirs[i], FALSE, name, size);
      if (theme)
        return theme;
    }

  /* This may fall back to builtin cursors */
  return wl_cursor_theme_create ("/usr/share/icons/default/cursors", size, display_wayland->shm);
}

/**
 * gdk_wayland_display_set_cursor_theme:
 * @display: (type GdkWaylandDisplay): a `GdkDisplay`
 * @name: the new cursor theme
 * @size: the size to use for cursors
 *
 * Sets the cursor theme for the given @display.
 */
void
gdk_wayland_display_set_cursor_theme (GdkDisplay *display,
                                      const char *name,
                                      int         size)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY(display);
  struct wl_cursor_theme *theme;

  g_assert (display_wayland);
  g_assert (display_wayland->shm);

  if (size == 0)
    size = 24;

  if (g_strcmp0 (name, display_wayland->cursor_theme_name) == 0 &&
      display_wayland->cursor_theme_size == size)
    return;

  theme = get_cursor_theme (display_wayland, name, size);

  if (theme == NULL)
    {
      g_warning ("Failed to load cursor theme %s", name);
      return;
    }

  if (display_wayland->cursor_theme)
    {
      wl_cursor_theme_destroy (display_wayland->cursor_theme);
      display_wayland->cursor_theme = NULL;
    }
  display_wayland->cursor_theme = theme;
  if (display_wayland->cursor_theme_name != NULL)
    g_free (display_wayland->cursor_theme_name);
  display_wayland->cursor_theme_name = g_strdup (name);
  display_wayland->cursor_theme_size = size;
}

struct wl_cursor_theme *
_gdk_wayland_display_get_cursor_theme (GdkWaylandDisplay *display_wayland)
{
  g_assert (display_wayland->cursor_theme_name);

  return display_wayland->cursor_theme;
}

static void
_gdk_wayland_display_load_cursor_theme (GdkWaylandDisplay *display_wayland)
{
  guint size;
  const char *name;
  GValue v = G_VALUE_INIT;
  gint64 before G_GNUC_UNUSED;

  before = GDK_PROFILER_CURRENT_TIME;

  g_assert (display_wayland);
  g_assert (display_wayland->shm);

  g_value_init (&v, G_TYPE_INT);
  if (gdk_display_get_setting (GDK_DISPLAY (display_wayland), "gtk-cursor-theme-size", &v))
    size = g_value_get_int (&v);
  else
    size = 32;
  g_value_unset (&v);

  g_value_init (&v, G_TYPE_STRING);
  if (gdk_display_get_setting (GDK_DISPLAY (display_wayland), "gtk-cursor-theme-name", &v))
    name = g_value_get_string (&v);
  else
    name = "default";

  gdk_wayland_display_set_cursor_theme (GDK_DISPLAY (display_wayland), name, size);
  g_value_unset (&v);

  gdk_profiler_end_mark (before, "Wayland cursor theme load", NULL);

}

/**
 * gdk_wayland_display_get_wl_display: (skip)
 * @display: (type GdkWaylandDisplay): a `GdkDisplay`
 *
 * Returns the Wayland `wl_display` of a `GdkDisplay`.
 *
 * Returns: (transfer none): a Wayland `wl_display`
 */
struct wl_display *
gdk_wayland_display_get_wl_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->wl_display;
}

/**
 * gdk_wayland_display_get_wl_compositor: (skip)
 * @display: (type GdkWaylandDisplay): a `GdkDisplay`
 *
 * Returns the Wayland `wl_compositor` of a `GdkDisplay`.
 *
 * Returns: (transfer none): a Wayland `wl_compositor`
 */
struct wl_compositor *
gdk_wayland_display_get_wl_compositor (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->compositor;
}

static const cairo_user_data_key_t gdk_wayland_shm_surface_cairo_key;

typedef struct _GdkWaylandCairoSurfaceData {
  gpointer buf;
  size_t buf_length;
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;
  GdkWaylandDisplay *display;
  GdkFractionalScale scale;
} GdkWaylandCairoSurfaceData;

static int
open_shared_memory (void)
{
  static gboolean force_shm_open = FALSE;
  int ret = -1;

#if !defined (__NR_memfd_create)
  force_shm_open = TRUE;
#endif

  do
    {
#if defined (__NR_memfd_create)
      if (!force_shm_open)
        {
          int options = MFD_CLOEXEC;
#if defined (MFD_ALLOW_SEALING)
          options |= MFD_ALLOW_SEALING;
#endif
          ret = syscall (__NR_memfd_create, "gdk-wayland", options);

          /* fall back to shm_open until debian stops shipping 3.16 kernel
           * See bug 766341
           */
          if (ret < 0 && errno == ENOSYS)
            force_shm_open = TRUE;
#if defined (F_ADD_SEALS) && defined (F_SEAL_SHRINK)
          if (ret >= 0)
            fcntl (ret, F_ADD_SEALS, F_SEAL_SHRINK);
#endif
        }
#endif

      if (force_shm_open)
        {
#if defined (__FreeBSD__)
          ret = shm_open (SHM_ANON, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
#else
          char name[NAME_MAX - 1] = "";

          sprintf (name, "/gdk-wayland-%x", g_random_int ());

          ret = shm_open (name, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);

          if (ret >= 0)
            shm_unlink (name);
          else if (errno == EEXIST)
            continue;
#endif
        }
    }
  while (ret < 0 && errno == EINTR);

  if (ret < 0)
    g_critical (G_STRLOC ": creating shared memory file (using %s) failed: %m",
                force_shm_open? "shm_open" : "memfd_create");

  return ret;
}

static struct wl_shm_pool *
create_shm_pool (struct wl_shm  *shm,
                 int             size,
                 size_t         *buf_length,
                 void          **data_out)
{
  struct wl_shm_pool *pool;
  int fd;
  void *data;

  fd = open_shared_memory ();

  if (fd < 0)
    goto fail;

  if (ftruncate (fd, size) < 0)
    {
      g_critical (G_STRLOC ": Truncating shared memory file failed: %m");
      close (fd);
      goto fail;
    }

  data = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (data == MAP_FAILED)
    {
      g_critical (G_STRLOC ": mmap'ping shared memory file failed: %m");
      close (fd);
      goto fail;
    }

  pool = wl_shm_create_pool (shm, fd, size);

  close (fd);

  *data_out = data;
  *buf_length = size;

  return pool;

fail:
  *data_out = NULL;
  *buf_length = 0;
  return NULL;
}

static void
gdk_wayland_cairo_surface_destroy (void *p)
{
  GdkWaylandCairoSurfaceData *data = p;

  if (data->buffer)
    wl_buffer_destroy (data->buffer);

  if (data->pool)
    wl_shm_pool_destroy (data->pool);

  munmap (data->buf, data->buf_length);
  g_free (data);
}

cairo_surface_t *
gdk_wayland_display_create_shm_surface (GdkWaylandDisplay        *display,
                                        int                       width,
                                        int                       height,
                                        const GdkFractionalScale *scale)
{
  GdkWaylandCairoSurfaceData *data;
  cairo_surface_t *surface = NULL;
  cairo_status_t status;
  int scaled_width, scaled_height;
  int stride;

  data = g_new (GdkWaylandCairoSurfaceData, 1);
  data->display = display;
  data->buffer = NULL;
  data->scale = *scale;

  scaled_width = gdk_fractional_scale_scale (scale, width);
  scaled_height = gdk_fractional_scale_scale (scale, height);
  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, scaled_width);

  data->pool = create_shm_pool (display->shm,
                                scaled_height * stride,
                                &data->buf_length,
                                &data->buf);
  if (G_UNLIKELY (data->pool == NULL))
    g_error ("Unable to create shared memory pool");

  surface = cairo_image_surface_create_for_data (data->buf,
                                                 CAIRO_FORMAT_ARGB32,
                                                 scaled_width,
                                                 scaled_height,
                                                 stride);

  data->buffer = wl_shm_pool_create_buffer (data->pool, 0,
                                            scaled_width, scaled_height,
                                            stride, WL_SHM_FORMAT_ARGB8888);

  cairo_surface_set_user_data (surface, &gdk_wayland_shm_surface_cairo_key,
                               data, gdk_wayland_cairo_surface_destroy);

  cairo_surface_set_device_scale (surface,
                                  gdk_fractional_scale_to_double (scale),
                                  gdk_fractional_scale_to_double (scale));

  status = cairo_surface_status (surface);
  if (status != CAIRO_STATUS_SUCCESS)
    {
      g_critical (G_STRLOC ": Unable to create Cairo image surface: %s",
                  cairo_status_to_string (status));
    }

  return surface;
}

struct wl_buffer *
_gdk_wayland_shm_surface_get_wl_buffer (cairo_surface_t *surface)
{
  GdkWaylandCairoSurfaceData *data = cairo_surface_get_user_data (surface, &gdk_wayland_shm_surface_cairo_key);
  return data->buffer;
}

gboolean
_gdk_wayland_is_shm_surface (cairo_surface_t *surface)
{
  return cairo_surface_get_user_data (surface, &gdk_wayland_shm_surface_cairo_key) != NULL;
}

typedef enum
{
  GSD_FONT_ANTIALIASING_MODE_NONE,
  GSD_FONT_ANTIALIASING_MODE_GRAYSCALE,
  GSD_FONT_ANTIALIASING_MODE_RGBA
} GsdFontAntialiasingMode;

static int
get_antialiasing (const char *s)
{
  const char *names[] = { "none", "grayscale", "rgba" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

typedef enum
{
  GSD_FONT_HINTING_NONE,
  GSD_FONT_HINTING_SLIGHT,
  GSD_FONT_HINTING_MEDIUM,
  GSD_FONT_HINTING_FULL
} GsdFontHinting;

static int
get_hinting (const char *s)
{
  const char *names[] = { "none", "slight", "medium", "full" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

typedef enum
{
  GSD_FONT_RGBA_ORDER_RGBA,
  GSD_FONT_RGBA_ORDER_RGB,
  GSD_FONT_RGBA_ORDER_BGR,
  GSD_FONT_RGBA_ORDER_VRGB,
  GSD_FONT_RGBA_ORDER_VBGR
} GsdFontRgbaOrder;

static int
get_order (const char *s)
{
  const char *names[] = { "rgba", "rgb", "bgr", "vrgb", "vbgr" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

static double
get_dpi_from_gsettings (GdkWaylandDisplay *display_wayland)
{
  GSettings *settings;
  double factor;

  settings = g_hash_table_lookup (display_wayland->settings,
                                  "org.gnome.desktop.interface");
  if (settings != NULL)
    factor = g_settings_get_double (settings, "text-scaling-factor");
  else
    factor = 1.0;

  return 96.0 * factor;
}

/* When using the Settings portal, we cache the value in
 * the fallback member, and we ignore the valid field
 */
typedef struct _TranslationEntry TranslationEntry;
struct _TranslationEntry {
  gboolean valid;
  const char *schema;
  const char *key;
  const char *setting;
  GType type;
  union {
    const char *s;
    int          i;
    gboolean     b;
  } fallback;
};

static TranslationEntry * find_translation_entry_by_schema (const char *schema,
                                                            const char *key);

static void
update_xft_settings (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GSettings *settings;
  GsdFontAntialiasingMode antialiasing;
  GsdFontHinting hinting;
  GsdFontRgbaOrder order;
  gboolean use_rgba = FALSE;
  GsdXftSettings xft_settings;
  double dpi;

  if (display_wayland->settings_portal)
    {
      TranslationEntry *entry;

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-antialiasing");
      g_assert (entry);

      if (entry->valid)
        {
          antialiasing = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-hinting");
          g_assert (entry);
          hinting = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-rgba-order");
          g_assert (entry);
          order = entry->fallback.i;
        }
      else
        {
          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "antialiasing");
          g_assert (entry);
          antialiasing = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "hinting");
          g_assert (entry);
          hinting = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "rgba-order");
          g_assert (entry);
          order = entry->fallback.i;
        }

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "text-scaling-factor");
      g_assert (entry);
      dpi = 96.0 * entry->fallback.i / 65536.0 * 1024; /* Xft wants 1/1024th of an inch */
    }
  else
    {
      TranslationEntry *entry;

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-antialiasing");

      if (entry && entry->valid)
        {
          settings = g_hash_table_lookup (display_wayland->settings,
                                          "org.gnome.desktop.interface");
          antialiasing = g_settings_get_enum (settings, "font-antialiasing");
          hinting = g_settings_get_enum (settings, "font-hinting");
          order = g_settings_get_enum (settings, "font-rgba-order");
        }
      else if (g_hash_table_contains (display_wayland->settings,
                                      "org.gnome.settings-daemon.plugins.xsettings"))
        {
          settings = g_hash_table_lookup (display_wayland->settings,
                                          "org.gnome.settings-daemon.plugins.xsettings");
          antialiasing = g_settings_get_enum (settings, "antialiasing");
          hinting = g_settings_get_enum (settings, "hinting");
          order = g_settings_get_enum (settings, "rgba-order");
        }
      else
        {
          antialiasing = GSD_FONT_ANTIALIASING_MODE_GRAYSCALE;
          hinting = GSD_FONT_HINTING_MEDIUM;
          order = GSD_FONT_RGBA_ORDER_RGB;
        }

      dpi = get_dpi_from_gsettings (display_wayland) * 1024;
    }

  xft_settings.hinting = (hinting != GSD_FONT_HINTING_NONE);
  xft_settings.dpi = dpi;

  switch (hinting)
    {
    case GSD_FONT_HINTING_NONE:
      xft_settings.hintstyle = "hintnone";
      break;
    case GSD_FONT_HINTING_SLIGHT:
      xft_settings.hintstyle = "hintslight";
      break;
    case GSD_FONT_HINTING_MEDIUM:
      xft_settings.hintstyle = "hintmedium";
      break;
    case GSD_FONT_HINTING_FULL:
    default:
      xft_settings.hintstyle = "hintfull";
      break;
    }

  switch (order)
    {
    case GSD_FONT_RGBA_ORDER_RGBA:
      xft_settings.rgba = "rgba";
      break;
    default:
    case GSD_FONT_RGBA_ORDER_RGB:
      xft_settings.rgba = "rgb";
      break;
    case GSD_FONT_RGBA_ORDER_BGR:
      xft_settings.rgba = "bgr";
      break;
    case GSD_FONT_RGBA_ORDER_VRGB:
      xft_settings.rgba = "vrgb";
      break;
    case GSD_FONT_RGBA_ORDER_VBGR:
      xft_settings.rgba = "vbgr";
      break;
    }

  switch (antialiasing)
   {
   default:
   case GSD_FONT_ANTIALIASING_MODE_NONE:
     xft_settings.antialias = FALSE;
     break;
   case GSD_FONT_ANTIALIASING_MODE_GRAYSCALE:
     xft_settings.antialias = TRUE;
     break;
   case GSD_FONT_ANTIALIASING_MODE_RGBA:
     xft_settings.antialias = TRUE;
     use_rgba = TRUE;
   }

  if (!use_rgba)
    xft_settings.rgba = "none";

  if (display_wayland->xft_settings.antialias != xft_settings.antialias)
    {
      display_wayland->xft_settings.antialias = xft_settings.antialias;
      gdk_display_setting_changed (display, "gtk-xft-antialias");
    }

  if (display_wayland->xft_settings.hinting != xft_settings.hinting)
    {
      display_wayland->xft_settings.hinting = xft_settings.hinting;
      gdk_display_setting_changed (display, "gtk-xft-hinting");
    }

  if (display_wayland->xft_settings.hintstyle != xft_settings.hintstyle)
    {
      display_wayland->xft_settings.hintstyle = xft_settings.hintstyle;
      gdk_display_setting_changed (display, "gtk-xft-hintstyle");
    }

  if (display_wayland->xft_settings.rgba != xft_settings.rgba)
    {
      display_wayland->xft_settings.rgba = xft_settings.rgba;
      gdk_display_setting_changed (display, "gtk-xft-rgba");
    }

  if (display_wayland->xft_settings.dpi != xft_settings.dpi)
    {
      display_wayland->xft_settings.dpi = xft_settings.dpi;
      gdk_display_setting_changed (display, "gtk-xft-dpi");
    }
}

static TranslationEntry translations[] = {
  { FALSE, "org.gnome.desktop.interface", "gtk-theme", "gtk-theme-name" , G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "icon-theme", "gtk-icon-theme-name", G_TYPE_STRING, { .s = "gnome" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-theme", "gtk-cursor-theme-name", G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-size", "gtk-cursor-theme-size", G_TYPE_INT, { .i = 32 } },
  { FALSE, "org.gnome.desktop.interface", "font-name", "gtk-font-name", G_TYPE_STRING, { .s = "Cantarell 11" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink", "gtk-cursor-blink", G_TYPE_BOOLEAN,  { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-time", "gtk-cursor-blink-time", G_TYPE_INT, { .i = 1200 } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-timeout", "gtk-cursor-blink-timeout", G_TYPE_INT, { .i = 3600 } },
  { FALSE, "org.gnome.desktop.interface", "gtk-im-module", "gtk-im-module", G_TYPE_STRING, { .s = "simple" } },
  { FALSE, "org.gnome.desktop.interface", "enable-animations", "gtk-enable-animations", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "gtk-enable-primary-paste", "gtk-enable-primary-paste", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "overlay-scrolling", "gtk-overlay-scrolling", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.desktop.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.desktop.sound", "theme-name", "gtk-sound-theme-name", G_TYPE_STRING, { .s = "freedesktop" } },
  { FALSE, "org.gnome.desktop.sound", "event-sounds", "gtk-enable-event-sounds", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.sound", "input-feedback-sounds", "gtk-enable-input-feedback-sounds", G_TYPE_BOOLEAN, { . b = FALSE } },
  { FALSE, "org.gnome.desktop.privacy", "recent-files-max-age", "gtk-recent-files-max-age", G_TYPE_INT, { .i = 30 } },
  { FALSE, "org.gnome.desktop.privacy", "remember-recent-files",    "gtk-recent-files-enabled", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.wm.preferences", "button-layout",    "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { FALSE, "org.gnome.desktop.interface", "font-antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.interface", "text-scaling-factor", "gtk-xft-dpi" , G_TYPE_NONE, { .i = 0 } }, /* We store the factor as 16.16 */
  { FALSE, "org.gnome.desktop.wm.preferences", "action-double-click-titlebar", "gtk-titlebar-double-click", G_TYPE_STRING, { .s = "toggle-maximize" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-middle-click-titlebar", "gtk-titlebar-middle-click", G_TYPE_STRING, { .s = "none" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-right-click-titlebar", "gtk-titlebar-right-click", G_TYPE_STRING, { .s = "menu" } },
  { FALSE, "org.gnome.desktop.a11y", "always-show-text-caret", "gtk-keynav-use-caret", G_TYPE_BOOLEAN, { .b = FALSE } },
  { FALSE, "org.gnome.desktop.a11y.interface", "high-contrast", "high-contrast", G_TYPE_NONE, { .b = FALSE } },
  { FALSE, "org.gnome.desktop.a11y.interface", "show-status-shapes", "gtk-show-status-shapes", G_TYPE_BOOLEAN, { .b = FALSE } },
  /* Note, this setting doesn't exist, the portal and gsd fake it */
  { FALSE, "org.gnome.fontconfig", "serial", "gtk-fontconfig-timestamp", G_TYPE_NONE, { .i = 0 } },
};


static TranslationEntry *
find_translation_entry_by_schema (const char *schema,
                                  const char *key)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (schema, translations[i].schema) &&
          g_str_equal (key, translations[i].key))
        return &translations[i];
    }

  return NULL;
}

static TranslationEntry *
find_translation_entry_by_key (GSettings  *settings,
                               const char *key)
{
  char *schema;
  TranslationEntry *entry;

  g_object_get (settings, "schema", &schema, NULL);
  entry = find_translation_entry_by_schema (schema, key);
  g_free (schema);

  return entry;
}

static TranslationEntry *
find_translation_entry_by_setting (const char *setting)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (setting, translations[i].setting))
        return &translations[i];
    }

  return NULL;
}

static void
settings_changed (GSettings  *settings,
                  const char *key,
                  GdkDisplay *display)
{
  TranslationEntry *entry;

  entry = find_translation_entry_by_key (settings, key);

  if (entry != NULL)
    {
      if (entry->type != G_TYPE_NONE)
        gdk_display_setting_changed (display, entry->setting);
      else if (strcmp (key, "high-contrast") == 0)
        gdk_display_setting_changed (display, "gtk-theme-name");
      else
        update_xft_settings (display);
    }
}

static void
apply_portal_setting (TranslationEntry *entry,
                      GVariant         *value,
                      GdkDisplay       *display)
{
  switch (entry->type)
    {
    case G_TYPE_STRING:
      entry->fallback.s = g_intern_string (g_variant_get_string (value, NULL));
      break;
    case G_TYPE_INT:
      entry->fallback.i = g_variant_get_int32 (value);
      break;
    case G_TYPE_BOOLEAN:
      entry->fallback.b = g_variant_get_boolean (value);
      break;
    case G_TYPE_NONE:
      if (strcmp (entry->key, "serial") == 0)
        {
          entry->fallback.i = g_variant_get_int32 (value);
          break;
        }
      if (strcmp (entry->key, "antialiasing") == 0 ||
          strcmp (entry->key, "font-antialiasing") == 0)
        entry->fallback.i = get_antialiasing (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "hinting") == 0 ||
               strcmp (entry->key, "font-hinting") == 0)
        entry->fallback.i = get_hinting (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "rgba-order") == 0 ||
               strcmp (entry->key, "font-rgba-order") == 0)
        entry->fallback.i = get_order (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "text-scaling-factor") == 0)
        entry->fallback.i = (int) (g_variant_get_double (value) * 65536.0);
      update_xft_settings (display);
      break;
    default:
      break;
    }
}

static void
settings_portal_changed (GDBusProxy *proxy,
                         const char *sender_name,
                         const char *signal_name,
                         GVariant   *parameters,
                         GdkDisplay *display)
{
  if (strcmp (signal_name, "SettingChanged") == 0)
    {
      const char *namespace;
      const char *name;
      GVariant *value;
      TranslationEntry *entry;

      g_variant_get (parameters, "(&s&sv)", &namespace, &name, &value);

      entry = find_translation_entry_by_schema (namespace, name);
      if (entry != NULL)
        {
          char *a = g_variant_print (value, FALSE);
          g_debug ("Using changed portal setting %s %s: %s", namespace, name, a);
          g_free (a);
          entry->valid = TRUE;
          apply_portal_setting (entry, value, display);
          gdk_display_setting_changed (display, entry->setting);
        }
      else
        g_debug ("Ignoring portal setting %s %s", namespace, name);

      g_variant_unref (value);
    }
}

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

static void
init_settings (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  GSettings *settings;
  int i;

  if (gdk_should_use_portal ())
    {
      GVariant *ret;
      GError *error = NULL;
      const char *schema_str;
      GVariant *val;
      GVariantIter *iter;
      const char *patterns[] = { "org.gnome.*", NULL }; 

      display_wayland->settings_portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                                        NULL,
                                                                        PORTAL_BUS_NAME,
                                                                        PORTAL_OBJECT_PATH,
                                                                        PORTAL_SETTINGS_INTERFACE,
                                                                        NULL,
                                                                        &error);
      if (error)
        {
          g_warning ("Settings portal not found: %s", error->message);
          g_error_free (error);

          goto fallback;
        }

      ret = g_dbus_proxy_call_sync (display_wayland->settings_portal,
                                    "ReadAll",
                                    g_variant_new ("(^as)", patterns),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    &error);

      if (error)
        {
          g_warning ("Failed to read portal settings: %s", error->message);
          g_error_free (error);
          g_clear_object (&display_wayland->settings_portal);

          goto fallback;
        }

      g_variant_get (ret, "(a{sa{sv}})", &iter);

      if (g_variant_iter_n_children (iter) == 0)
        {
          g_debug ("Received no portal settings");
          g_clear_pointer (&iter, g_variant_iter_free);
          g_clear_pointer (&ret, g_variant_unref);

          goto fallback;
        }

      while (g_variant_iter_loop (iter, "{s@a{sv}}", &schema_str, &val))
        {
          GVariantIter *iter2 = g_variant_iter_new (val);
          const char *key;
          GVariant *v;

          while (g_variant_iter_loop (iter2, "{sv}", &key, &v))
            {
              TranslationEntry *entry = find_translation_entry_by_schema (schema_str, key);
              if (entry)
                {
                  char *a = g_variant_print (v, FALSE);
                  g_debug ("Using portal setting for %s %s: %s\n", schema_str, key, a);
                  g_free (a);
                  entry->valid = TRUE;
                  apply_portal_setting (entry, v, display);
                }
              else
                {
                  g_debug ("Ignoring portal setting for %s %s", schema_str, key);
                }
            }
          g_variant_iter_free (iter2);
        }
      g_variant_iter_free (iter);

      g_variant_unref (ret);

      g_signal_connect (display_wayland->settings_portal, "g-signal",
                        G_CALLBACK (settings_portal_changed), display_wayland);

      return;

fallback:
      g_debug ("Failed to use Settings portal; falling back to gsettings");
    }

  g_intern_static_string ("antialiasing");
  g_intern_static_string ("hinting");
  g_intern_static_string ("rgba-order");
  g_intern_static_string ("text-scaling-factor");

  display_wayland->settings = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  source = g_settings_schema_source_get_default ();
  if (source == NULL)
    return;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      schema = g_settings_schema_source_lookup (source, translations[i].schema, TRUE);
      if (!schema)
        continue;

      g_intern_static_string (translations[i].key);

      if (g_hash_table_lookup (display_wayland->settings, (gpointer)translations[i].schema) == NULL)
        {
          settings = g_settings_new_full (schema, NULL, NULL);
          g_signal_connect (settings, "changed",
                            G_CALLBACK (settings_changed), display);
          g_hash_table_insert (display_wayland->settings, (gpointer)translations[i].schema, settings);
        }

      if (g_settings_schema_has_key (schema, translations[i].key))
        translations[i].valid = TRUE;

      g_settings_schema_unref (schema);
    }

  update_xft_settings (display);
}

static void
gtk_shell_handle_capabilities (void              *data,
                               struct gtk_shell1 *shell,
                               uint32_t           capabilities)
{
  GdkDisplay *display = data;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (data);

  display_wayland->shell_capabilities = capabilities;

  gdk_display_setting_changed (display, "gtk-shell-shows-app-menu");
  gdk_display_setting_changed (display, "gtk-shell-shows-menubar");
  gdk_display_setting_changed (display, "gtk-shell-shows-desktop");
}

struct gtk_shell1_listener gdk_display_gtk_shell_listener = {
  gtk_shell_handle_capabilities
};

static void
gdk_wayland_display_set_has_gtk_shell (GdkWaylandDisplay *display_wayland)
{
  gtk_shell1_add_listener (display_wayland->gtk_shell,
                           &gdk_display_gtk_shell_listener,
                           display_wayland);
}

static void
set_value_from_entry (GdkDisplay       *display,
                      TranslationEntry *entry,
                      GValue           *value)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GSettings *settings;

  if (display_wayland->settings_portal)
    {
      switch (entry->type)
        {
        case G_TYPE_STRING:
          g_value_set_string (value, entry->fallback.s);
          break;
        case G_TYPE_INT:
          g_value_set_int (value, entry->fallback.i);
          break;
        case G_TYPE_BOOLEAN:
          g_value_set_boolean (value, entry->fallback.b);
          break;
        case G_TYPE_NONE:
          if (g_str_equal (entry->setting, "gtk-fontconfig-timestamp"))
            g_value_set_uint (value, (guint)entry->fallback.i);
          else if (g_str_equal (entry->setting, "gtk-xft-antialias"))
            g_value_set_int (value, display_wayland->xft_settings.antialias);
          else if (g_str_equal (entry->setting, "gtk-xft-hinting"))
            g_value_set_int (value, display_wayland->xft_settings.hinting);
          else if (g_str_equal (entry->setting, "gtk-xft-hintstyle"))
            g_value_set_static_string (value, display_wayland->xft_settings.hintstyle);
          else if (g_str_equal (entry->setting, "gtk-xft-rgba"))
            g_value_set_static_string (value, display_wayland->xft_settings.rgba);
          else if (g_str_equal (entry->setting, "gtk-xft-dpi"))
            g_value_set_int (value, display_wayland->xft_settings.dpi);
          else
            g_assert_not_reached ();
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      return;
    }

  settings = (GSettings *)g_hash_table_lookup (display_wayland->settings, entry->schema);
  switch (entry->type)
    {
    case G_TYPE_STRING:
      if (settings && entry->valid)
        {
          char *s;
          s = g_settings_get_string (settings, entry->key);
          g_value_set_string (value, s);
          g_free (s);
        }
      else
        {
          g_value_set_static_string (value, entry->fallback.s);
        }
      break;
    case G_TYPE_INT:
      g_value_set_int (value, settings && entry->valid
                              ? g_settings_get_int (settings, entry->key)
                              : entry->fallback.i);
      break;
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, settings && entry->valid
                                  ? g_settings_get_boolean (settings, entry->key)
                                  : entry->fallback.b);
      break;
    case G_TYPE_NONE:
      if (g_str_equal (entry->setting, "gtk-fontconfig-timestamp"))
        g_value_set_uint (value, (guint)entry->fallback.i);
      else if (g_str_equal (entry->setting, "gtk-xft-antialias"))
        g_value_set_int (value, display_wayland->xft_settings.antialias);
      else if (g_str_equal (entry->setting, "gtk-xft-hinting"))
        g_value_set_int (value, display_wayland->xft_settings.hinting);
      else if (g_str_equal (entry->setting, "gtk-xft-hintstyle"))
        g_value_set_static_string (value, display_wayland->xft_settings.hintstyle);
      else if (g_str_equal (entry->setting, "gtk-xft-rgba"))
        g_value_set_static_string (value, display_wayland->xft_settings.rgba);
      else if (g_str_equal (entry->setting, "gtk-xft-dpi"))
        g_value_set_int (value, display_wayland->xft_settings.dpi);
      else
        g_assert_not_reached ();
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
set_decoration_layout_from_entry (GdkDisplay       *display,
                                  TranslationEntry *entry,
                                  GValue           *value)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GSettings *settings = NULL;

  if (display_wayland->settings_portal)
    {
      g_value_set_string (value, entry->fallback.s);
      return;
    }

  settings = (GSettings *)g_hash_table_lookup (display_wayland->settings, entry->schema);

  if (settings)
    {
      char *s = g_settings_get_string (settings, entry->key);

      translate_wm_button_layout_to_gtk (s);
      g_value_set_string (value, s);

      g_free (s);
    }
  else
    {
      g_value_set_static_string (value, entry->fallback.s);
    }
}

static void
set_theme_from_entry (GdkDisplay       *display,
                      TranslationEntry *entry,
                      GValue           *value)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GSettings *settings = NULL;
  GSettingsSchema *schema = NULL;
  gboolean hc = FALSE;

  if (display_wayland->settings_portal == NULL)
    {
      settings = (GSettings *)g_hash_table_lookup (display_wayland->settings,
                                                   "org.gnome.desktop.a11y.interface");
    }

  if (settings)
    g_object_get (settings, "settings-schema", &schema, NULL);

  if (schema && g_settings_schema_has_key (schema, "high-contrast"))
    hc = g_settings_get_boolean (settings, "high-contrast");

  g_clear_pointer (&schema, g_settings_schema_unref);

  if (hc)
    g_value_set_static_string (value, "HighContrast");
  else
    set_value_from_entry (display, entry, value);
}

static gboolean
set_capability_setting (GdkDisplay                *display,
                        GValue                    *value,
                        enum gtk_shell1_capability test)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  int testbit = 1 << (test - 1);

  g_value_set_boolean (value, (display_wayland->shell_capabilities & testbit) == testbit);

  return TRUE;
}

static gboolean
gdk_wayland_display_get_setting (GdkDisplay *display,
                                 const char *name,
                                 GValue     *value)
{
  TranslationEntry *entry;

  if (gdk_display_get_debug_flags (display) & GDK_DEBUG_DEFAULT_SETTINGS)
      return FALSE;

  if (GDK_WAYLAND_DISPLAY (display)->settings != NULL &&
      g_hash_table_size (GDK_WAYLAND_DISPLAY (display)->settings) == 0)
    return FALSE;

  entry = find_translation_entry_by_setting (name);
  if (entry != NULL)
    {
      if (strcmp (name, "gtk-decoration-layout") == 0)
        set_decoration_layout_from_entry (display, entry, value);
      else if (strcmp (name, "gtk-theme-name") == 0)
        set_theme_from_entry (display, entry, value);
      else
        set_value_from_entry (display, entry, value);
      return TRUE;
   }

  if (strcmp (name, "gtk-shell-shows-app-menu") == 0)
    return set_capability_setting (display, value, GTK_SHELL1_CAPABILITY_GLOBAL_APP_MENU);

  if (strcmp (name, "gtk-shell-shows-menubar") == 0)
    return set_capability_setting (display, value, GTK_SHELL1_CAPABILITY_GLOBAL_MENU_BAR);

  if (strcmp (name, "gtk-shell-shows-desktop") == 0)
    return set_capability_setting (display, value, GTK_SHELL1_CAPABILITY_DESKTOP_ICONS);

  if (strcmp (name, "gtk-dialogs-use-header") == 0)
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  return FALSE;
}

static const char *
subpixel_to_string (int layout)
{
  int i;
  struct { int layout; const char *name; } layouts[] = {
    { WL_OUTPUT_SUBPIXEL_UNKNOWN, "unknown" },
    { WL_OUTPUT_SUBPIXEL_NONE, "none" },
    { WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB, "rgb" },
    { WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR, "bgr" },
    { WL_OUTPUT_SUBPIXEL_VERTICAL_RGB, "vrgb" },
    { WL_OUTPUT_SUBPIXEL_VERTICAL_BGR, "vbgr" },
    { 0xffffffff, NULL }
  };

  for (i = 0; layouts[i].name; i++)
    {
      if (layouts[i].layout == layout)
        return layouts[i].name;
    }
  return NULL;
}

static const char *
transform_to_string (int transform)
{
  int i;
  struct { int transform; const char *name; } transforms[] = {
    { WL_OUTPUT_TRANSFORM_NORMAL, "normal" },
    { WL_OUTPUT_TRANSFORM_90, "90" },
    { WL_OUTPUT_TRANSFORM_180, "180" },
    { WL_OUTPUT_TRANSFORM_270, "270" },
    { WL_OUTPUT_TRANSFORM_FLIPPED, "flipped" },
    { WL_OUTPUT_TRANSFORM_FLIPPED_90, "flipped 90" },
    { WL_OUTPUT_TRANSFORM_FLIPPED_180, "flipped 180" },
    { WL_OUTPUT_TRANSFORM_FLIPPED_270, "flipped 270" },
    { 0xffffffff, NULL }
  };

  for (i = 0; transforms[i].name; i++)
    {
      if (transforms[i].transform == transform)
        return transforms[i].name;
    }
  return NULL;
}

static void
update_scale (GdkDisplay *display)
{
  GList *seats;
  GList *l;

  g_list_foreach (gdk_wayland_display_get_toplevel_surfaces (display),
                  (GFunc)gdk_wayland_surface_update_scale,
                  NULL);
  seats = gdk_display_list_seats (display);
  for (l = seats; l; l = l->next)
    {
      GdkSeat *seat = l->data;

      gdk_wayland_seat_update_cursor_scale (GDK_WAYLAND_SEAT (seat));
    }
  g_list_free (seats);
}

static void
gdk_wayland_display_init_xdg_output (GdkWaylandDisplay *self)
{
  guint i, n;

  GDK_DEBUG (MISC, "init xdg-output support, %d monitor(s) already present",
                   g_list_model_get_n_items (G_LIST_MODEL (self->monitors)));

  n = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));
  for (i = 0; i < n; i++)
    {
      GdkWaylandMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), i);
      gdk_wayland_display_get_xdg_output (monitor);
      g_object_unref (monitor);
    }
}

static gboolean
display_has_xdg_output_support (GdkWaylandDisplay *display_wayland)
{
  return (display_wayland->xdg_output_manager != NULL);
}

static gboolean
monitor_has_xdg_output (GdkWaylandMonitor *monitor)
{
  return (monitor->xdg_output != NULL);
}

static gboolean
should_update_monitor (GdkWaylandMonitor *monitor)
{
  return (GDK_MONITOR (monitor)->geometry.width != 0 &&
          monitor->version < OUTPUT_VERSION_WITH_DONE);
}

static gboolean
should_expect_xdg_output_done (GdkWaylandMonitor *monitor)
{
  GdkDisplay *display = GDK_MONITOR (monitor)->display;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  return (monitor_has_xdg_output (monitor) &&
          zxdg_output_manager_v1_get_version (display_wayland->xdg_output_manager) < NO_XDG_OUTPUT_DONE_SINCE_VERSION);
}

static void
apply_monitor_change (GdkWaylandMonitor *monitor)
{
  GDK_DEBUG (MISC, "monitor %d changed position %d %d, size %d %d",
                   monitor->id,
                   monitor->output_geometry.x, monitor->output_geometry.y,
                   monitor->output_geometry.width, monitor->output_geometry.height);

  GdkRectangle logical_geometry;
  gboolean needs_scaling = FALSE;
  double scale;

  if (monitor_has_xdg_output (monitor) &&
      monitor->xdg_output_geometry.width != 0  &&
      monitor->xdg_output_geometry.height != 0)
    {
      logical_geometry = monitor->xdg_output_geometry;
      needs_scaling = logical_geometry.width == monitor->output_geometry.width &&
                      logical_geometry.height == monitor->output_geometry.height;
    }
  else
    {
      logical_geometry = monitor->output_geometry;
      needs_scaling = TRUE;
    }

  if (needs_scaling)
    {
      int scale_factor = gdk_monitor_get_scale_factor (GDK_MONITOR (monitor));

      logical_geometry.y /= scale_factor;
      logical_geometry.x /= scale_factor;
      logical_geometry.width /= scale_factor;
      logical_geometry.height /= scale_factor;

      scale = scale_factor;
    }
  else
    {
      scale = MAX (monitor->output_geometry.width / (double) logical_geometry.width,
                   monitor->output_geometry.height / (double) logical_geometry.height);
    }

  gdk_monitor_set_geometry (GDK_MONITOR (monitor), &logical_geometry);
  gdk_monitor_set_connector (GDK_MONITOR (monitor), monitor->name);
  gdk_monitor_set_description (GDK_MONITOR (monitor), monitor->description);
  gdk_monitor_set_scale (GDK_MONITOR (monitor), scale);

  monitor->wl_output_done = FALSE;
  monitor->xdg_output_done = FALSE;

  update_scale (GDK_MONITOR (monitor)->display);
}

static void
xdg_output_handle_logical_position (void                  *data,
                                    struct zxdg_output_v1 *xdg_output,
                                    int32_t                x,
                                    int32_t                y)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle logical position xdg-output %d, position %d %d",
                   monitor->id, x, y);

  monitor->xdg_output_geometry.x = x;
  monitor->xdg_output_geometry.y = y;
}

static void
xdg_output_handle_logical_size (void                  *data,
                                struct zxdg_output_v1 *xdg_output,
                                int32_t                width,
                                int32_t                height)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle logical size xdg-output %d, size %d %d",
                   monitor->id, width, height);

  monitor->xdg_output_geometry.width = width;
  monitor->xdg_output_geometry.height = height;
}

static void
xdg_output_handle_done (void                  *data,
                        struct zxdg_output_v1 *xdg_output)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle done xdg-output %d", monitor->id);

  monitor->xdg_output_done = TRUE;
  if (monitor->wl_output_done && should_expect_xdg_output_done (monitor))
    apply_monitor_change (monitor);
}

static void
xdg_output_handle_name (void                  *data,
                        struct zxdg_output_v1 *xdg_output,
                        const char            *name)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle name xdg-output %d: %s", monitor->id, name);

  monitor->name = g_strdup (name);
}

static void
xdg_output_handle_description (void                  *data,
                               struct zxdg_output_v1 *xdg_output,
                               const char            *description)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle description xdg-output %d: %s", monitor->id, description);

  monitor->description = g_strdup (description);
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    xdg_output_handle_logical_position,
    xdg_output_handle_logical_size,
    xdg_output_handle_done,
    xdg_output_handle_name,
    xdg_output_handle_description,
};

static void
gdk_wayland_display_get_xdg_output (GdkWaylandMonitor *monitor)
{
  GdkDisplay *display = GDK_MONITOR (monitor)->display;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  GDK_DEBUG (MISC, "get xdg-output for monitor %d", monitor->id);

  monitor->xdg_output =
    zxdg_output_manager_v1_get_xdg_output (display_wayland->xdg_output_manager,
                                           monitor->output);

  zxdg_output_v1_add_listener (monitor->xdg_output,
                               &xdg_output_listener,
                               monitor);
}

static void
output_handle_geometry (void             *data,
                        struct wl_output *wl_output,
                        int               x,
                        int               y,
                        int               physical_width,
                        int               physical_height,
                        int               subpixel,
                        const char       *make,
                        const char       *model,
                        int32_t           transform)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_DEBUG (MISC, "handle geometry output %d, position %d %d, phys. size %d %d, subpixel layout %s, manufacturer %s, model %s, transform %s",
                   monitor->id, x, y,
                   physical_width, physical_height,
                   subpixel_to_string (subpixel),
                   make, model,
                   transform_to_string (transform));

  monitor->output_geometry.x = x;
  monitor->output_geometry.y = y;

  switch (transform)
    {
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      gdk_monitor_set_physical_size (GDK_MONITOR (monitor),
				     physical_height, physical_width);
      break;
    default:
      gdk_monitor_set_physical_size (GDK_MONITOR (monitor),
				     physical_width, physical_height);
    }

  gdk_monitor_set_subpixel_layout (GDK_MONITOR (monitor), subpixel);
  gdk_monitor_set_manufacturer (GDK_MONITOR (monitor), make);
  gdk_monitor_set_model (GDK_MONITOR (monitor), model);

  if (should_update_monitor (monitor) || !monitor_has_xdg_output (monitor))
    apply_monitor_change (monitor);
}

static void
output_handle_done (void             *data,
                    struct wl_output *wl_output)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_DEBUG (MISC, "handle done output %d", monitor->id);

  monitor->wl_output_done = TRUE;

  if (!should_expect_xdg_output_done (monitor) || monitor->xdg_output_done)
    apply_monitor_change (monitor);
}

static void
output_handle_scale (void             *data,
                     struct wl_output *wl_output,
                     int32_t           scale)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_DEBUG (MISC, "handle scale output %d, scale %d", monitor->id, scale);

  /* Set the scale from wl_output protocol, regardless of xdg-output support */
  gdk_monitor_set_scale_factor (GDK_MONITOR (monitor), scale);

  if (should_update_monitor (monitor))
    apply_monitor_change (monitor);
}

static void
output_handle_mode (void             *data,
                    struct wl_output *wl_output,
                    uint32_t          flags,
                    int               width,
                    int               height,
                    int               refresh)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_DEBUG (MISC, "handle mode output %d, size %d %d, rate %d",
                   monitor->id, width, height, refresh);

  if ((flags & WL_OUTPUT_MODE_CURRENT) == 0)
    return;

  monitor->output_geometry.width = width;
  monitor->output_geometry.height = height;
  gdk_monitor_set_refresh_rate (GDK_MONITOR (monitor), refresh);

  if (should_update_monitor (monitor) || !monitor_has_xdg_output (monitor))
    apply_monitor_change (monitor);
}

static const struct wl_output_listener output_listener =
{
  output_handle_geometry,
  output_handle_mode,
  output_handle_done,
  output_handle_scale,
};

static void
gdk_wayland_display_add_output (GdkWaylandDisplay *display_wayland,
                                guint32            id,
                                struct wl_output  *output,
                                guint32            version)
{
  GdkWaylandMonitor *monitor;

  monitor = g_object_new (GDK_TYPE_WAYLAND_MONITOR,
                          "display", GDK_DISPLAY (display_wayland),
                          NULL);

  monitor->id = id;
  monitor->output = output;
  monitor->version = version;

  wl_output_add_listener (output, &output_listener, monitor);

  GDK_DEBUG (MISC, "add output %u, version %u", id, version);

  if (display_has_xdg_output_support (display_wayland))
    gdk_wayland_display_get_xdg_output (monitor);

  g_list_store_append (display_wayland->monitors, monitor);

  g_object_unref (monitor);
}

static GdkWaylandMonitor *
get_monitor_for_output (GdkWaylandDisplay *self,
                        struct wl_output  *output)
{
  guint i, n;

  n = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));
  for (i = 0; i < n; i++)
    {
      GdkWaylandMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), i);

      g_object_unref (monitor);

      if (monitor->output == output)
        return monitor;
    }

  return NULL;
}

GdkMonitor *
gdk_wayland_display_get_monitor_for_output (GdkDisplay       *display,
                                            struct wl_output *output)
{
  return (GdkMonitor *)get_monitor_for_output (GDK_WAYLAND_DISPLAY (display), output);
}

static void
gdk_wayland_display_remove_output (GdkWaylandDisplay *self,
                                   guint32            id)
{
  guint i, n;

  GDK_DEBUG (MISC, "remove output %u", id);

  n = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));
  for (i = 0; i < n; i++)
    {
      GdkWaylandMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), i);

      if (monitor->id == id)
        {
          g_list_store_remove (self->monitors, i);
          gdk_monitor_invalidate (GDK_MONITOR (monitor));
          update_scale (GDK_DISPLAY (self));
          g_object_unref (monitor);
          break;
        }

      g_object_unref (monitor);
    }
}

int
gdk_wayland_display_get_output_refresh_rate (GdkWaylandDisplay *display_wayland,
                                             struct wl_output  *output)
{
  GdkWaylandMonitor *monitor;

  monitor = get_monitor_for_output (display_wayland, output);
  if (monitor != NULL)
    return gdk_monitor_get_refresh_rate (GDK_MONITOR (monitor));

  return 0;
}

guint32
gdk_wayland_display_get_output_scale (GdkWaylandDisplay *display_wayland,
				      struct wl_output  *output)
{
  GdkWaylandMonitor *monitor;

  monitor = get_monitor_for_output (display_wayland, output);
  if (monitor != NULL)
    return gdk_monitor_get_scale_factor (GDK_MONITOR (monitor));

  return 0;
}

/**
 * gdk_wayland_display_query_registry:
 * @display: (type GdkWaylandDisplay): a `GdkDisplay`
 * @global: global interface to query in the registry
 *
 * Returns %TRUE if the interface was found in the display
 * `wl_registry.global` handler.
 *
 * Returns: %TRUE if the global is offered by the compositor
 */
gboolean
gdk_wayland_display_query_registry (GdkDisplay *display,
				    const char *global)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GHashTableIter iter;
  char *value;

  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);
  g_return_val_if_fail (global != NULL, FALSE);

  g_hash_table_iter_init (&iter, display_wayland->known_globals);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &value))
    {
      if (strcmp (value, global) == 0)
        return TRUE;
    }

  return FALSE;
}

void
gdk_wayland_display_dispatch_queue (GdkDisplay            *display,
                                    struct wl_event_queue *event_queue)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (wl_display_dispatch_queue (display_wayland->wl_display, event_queue) == -1)
    {
      g_message ("Error %d (%s) dispatching to Wayland display.",
                 errno, g_strerror (errno));
      _exit (1);
    }
}
