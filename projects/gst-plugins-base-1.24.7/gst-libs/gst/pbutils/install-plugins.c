/* GStreamer base utils library plugin install support for applications
 * Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2006 Ryan Lortie <desrt desrt ca>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gstpbutilsinstallplugins
 * @title: Install-plugins
 * @short_description: Missing plugin installation support for applications
 *
 * ## Overview
 *
 * Using this API, applications can request the installation of missing
 * GStreamer plugins. These may be missing decoders/demuxers or
 * encoders/muxers for a certain format, sources or sinks for a certain URI
 * protocol (e.g. 'http'), or certain elements known by their element
 * factory name ('audioresample').
 *
 * Whether plugin installation is supported or not depends on the operating
 * system and/or distribution in question. The vendor of the operating
 * system needs to make sure the necessary hooks and mechanisms are in
 * place for plugin installation to work. See below for more detailed
 * information.
 *
 * From the application perspective, plugin installation is usually
 * triggered either
 *
 * -   when the application itself has found that it wants or needs to
 *     install a certain element
 * -   when the application has been notified by an element (such as
 *     playbin or decodebin) that one or more plugins are missing *and* the
 *     application has decided that it wants to install one or more of
 *     those missing plugins
 *
 * The install functions in this section all take one or more 'detail
 * strings'. These detail strings contain information about the type of
 * plugin that needs to be installed (decoder, encoder, source, sink, or
 * named element), and some additional information such GStreamer version
 * used and a human-readable description of the component to install for
 * user dialogs.
 *
 * Applications should not concern themselves with the composition of the
 * string itself. They should regard the string as if it was a shared
 * secret between GStreamer and the plugin installer application.
 *
 * Detail strings can be obtained using the function
 * gst_missing_plugin_message_get_installer_detail() on a
 * missing-plugin message. Such a message will either have been found by
 * the application on a pipeline's #GstBus, or the application will have
 * created it itself using gst_missing_element_message_new(),
 * gst_missing_decoder_message_new(),
 * gst_missing_encoder_message_new(),
 * gst_missing_uri_sink_message_new(), or
 * gst_missing_uri_source_message_new().
 *
 * For each GStreamer element/plugin/component that should be installed,
 * the application needs one of those 'installer detail' string mentioned
 * in the previous section. This string can be obtained, as already
 * mentioned above, from a missing-plugin message using the function
 * gst_missing_plugin_message_get_installer_detail(). The
 * missing-plugin message is either posted by another element and then
 * found on the bus by the application, or the application has created it
 * itself as described above.
 *
 * The application will then call gst_install_plugins_async(), passing a
 * NULL-terminated array of installer detail strings, and a function that
 * should be called when the installation of the plugins has finished
 * (successfully or not). Optionally, a #GstInstallPluginsContext created
 * with gst_install_plugins_context_new() may be passed as well. This
 * way additional optional arguments like the application window's XID can
 * be passed to the external installer application.
 *
 * gst_install_plugins_async() will return almost immediately, with the
 * return code indicating whether plugin installation was started or not.
 * If the necessary hooks for plugin installation are in place and an
 * external installer application has in fact been called, the passed in
 * function will be called with a result code as soon as the external
 * installer has finished. If the result code indicates that new plugins
 * have been installed, the application will want to call
 * gst_update_registry() so the run-time plugin registry is updated and
 * the new plugins are made available to the application.
 *
 * > A Gtk/GLib main loop must be running in order for the result function
 * > to be called when the external installer has finished. If this is not
 * > the case, make sure to regularly call in your code:
 * >
 * > g_main_context_iteration (NULL,FALSE);
 *
 * ## 1. Installer hook
 *
 * When GStreamer applications initiate plugin installation via
 * gst_install_plugins_async() or gst_install_plugins_sync(), a
 * pre-defined helper application will be called.
 *
 * The exact path of the helper application to be called is set at compile
 * time, usually by the build system based on the install prefix.
 * For a normal package build into the `/usr` prefix, this will usually
 * default to `/usr/libexec/gst-install-plugins-helper` or
 * `/usr/lib/gst-install-plugins-helper`.
 *
 * Vendors/distros who want to support GStreamer plugin installation should
 * either provide such a helper script/application or use the meson option
 * `-Dinstall_plugins_helper'=/path/to/installer` to make GStreamer call an
 * installer of their own directly.
 *
 * It is strongly recommended that vendors provide a small helper
 * application as interlocutor to the real installer though, even more so
 * if command line argument munging is required to transform the command
 * line arguments passed by GStreamer to the helper application into
 * arguments that are understood by the real installer.
 *
 * The helper application path defined at compile time can be overridden at
 * runtime by setting the GST_INSTALL_PLUGINS_HELPER environment
 * variable. This can be useful for testing/debugging purposes.
 *
 * ## 2. Arguments passed to the install helper
 *
 * GStreamer will pass the following arguments to the install helper (this
 * is in addition to the path of the executable itself, which is by
 * convention argv[0]):
 *
 * -   none to many optional arguments in the form of `--foo-bar=val`.
 *     Example: `--transient-for=XID` where XID is the X Window ID of the
 *     main window of the calling application (so the installer can make
 *     itself transient to that window). Unknown optional arguments should
 *     be ignored by the installer.
 *
 * -   one 'installer detail string' argument for each plugin to be
 *     installed; these strings will have a `gstreamer` prefix; the exact
 *     format of the detail string is explained below
 *
 * ## 3. Detail string describing the missing plugin
 *
 * The string is in UTF-8 encoding and is made up of several fields,
 * separated by '|' characters (but neither the first nor the last
 * character is a '|'). The fields are:
 *
 * -   plugin system identifier, ie. "gstreamer"
 *     This identifier determines the format of the rest of the detail
 *     string. Automatic plugin installers should not process detail
 *     strings with unknown identifiers. This allows other plugin-based
 *     libraries to use the same mechanism for their automatic plugin
 *     installation needs, or for the format to be changed should it turn
 *     out to be insufficient.
 * -   plugin system version, e.g. "1.0"
 *     This is required so that when there is GStreamer-2.0 at some point
 *     in future, the different major versions can still co-exist and use
 *     the same plugin install mechanism in the same way.
 * -   application identifier, e.g. "totem"
 *     This may also be in the form of "pid/12345" if the program name
 *     can't be obtained for some reason.
 * -   human-readable localised description of the required component, e.g.
 *     "Vorbis audio decoder"
 * -   identifier string for the required component (see below for details
 *     about how to map this to the package/plugin that needs installing),
 *     e.g.
 *     -   urisource-$(PROTOCOL_REQUIRED), e.g. urisource-http or
 *         urisource-mms
 *     -   element-$(ELEMENT_REQUIRED), e.g. element-videoconvert
 *     -   decoder-$(CAPS_REQUIRED), e.g. (do read below for more
 *         details!):
 *         -   decoder-audio/x-vorbis
 *         -   decoder-application/ogg
 *         -   decoder-audio/mpeg, mpegversion=(int)4
 *         -   decoder-video/mpeg, systemstream=(boolean)true,
 *             mpegversion=(int)2
 *     -   encoder-$(CAPS_REQUIRED), e.g. encoder-audio/x-vorbis
 * -   optional further fields not yet specified
 *
 * An entire ID string might then look like this, for example: `
 * gstreamer|1.0|totem|Vorbis audio decoder|decoder-audio/x-vorbis`
 *
 * Plugin installers parsing this ID string should expect further fields
 * also separated by '|' symbols and either ignore them, warn the user, or
 * error out when encountering them.
 *
 * Those unfamiliar with the GStreamer 'caps' system should note a few
 * things about the caps string used in the above decoder/encoder case:
 *
 * -   the first part ("video/mpeg") of the caps string is a GStreamer
 *     media type and *not* a MIME type. Wherever possible, the GStreamer
 *     media type will be the same as the corresponding MIME type, but
 *     often it is not.
 * -   a caps string may or may not have additional comma-separated fields
 *     of various types (as seen in the examples above)
 * -   the caps string of a 'required' component (as above) will always
 *     have fields with fixed values, whereas an introspected string (see
 *     below) may have fields with non-fixed values. Compare for example:
 *     -   `audio/mpeg, mpegversion=(int)4` vs.
 *         `audio/mpeg, mpegversion=(int){2, 4}`
 *     -   `video/mpeg, mpegversion=(int)2` vs.
 *         `video/mpeg, systemstream=(boolean){ true, false}, mpegversion=(int)[1, 2]`
 *
 * ## 4. Exit codes the installer should return
 *
 * The installer should return one of the following exit codes when it
 * exits:
 *
 * -   0 if all of the requested plugins could be installed
 *     (#GST_INSTALL_PLUGINS_SUCCESS)
 * -   1 if no appropriate installation candidate for any of the requested
 *     plugins could be found. Only return this if nothing has been
 *     installed (#GST_INSTALL_PLUGINS_NOT_FOUND)
 * -   2 if an error occurred during the installation. The application will
 *     assume that the user will already have seen an error message by the
 *     installer in this case and will usually not show another one
 *     (#GST_INSTALL_PLUGINS_ERROR)
 * -   3 if some of the requested plugins could be installed, but not all
 *     (#GST_INSTALL_PLUGINS_PARTIAL_SUCCESS)
 * -   4 if the user aborted the installation
 *     (#GST_INSTALL_PLUGINS_USER_ABORT)
 *
 * ## 5. How to map the required detail string to packages
 *
 * It is up to the vendor to find mechanism to map required components from
 * the detail string to the actual packages/plugins to install. This could
 * be a hardcoded list of mappings, for example, or be part of the
 * packaging system metadata.
 *
 * GStreamer plugin files can be introspected for this information. The
 * `gst-inspect` utility has a special command line option that will output
 * information similar to what is required. For example `
 * $ gst-inspect-1.0 --print-plugin-auto-install-info /path/to/libgstvorbis.so
 * should output something along the lines of
 * `decoder-audio/x-vorbis`, `element-vorbisdec` `element-vorbisenc`
 * `element-vorbisparse`, `element-vorbistag`, `encoder-audio/x-vorbis`
 *
 * Note that in the encoder and decoder case the introspected caps can be
 * more complex with additional fields, e.g.
 * `audio/mpeg,mpegversion=(int){2,4}`, so they will not always exactly
 * match the caps wanted by the application. It is up to the installer to
 * deal with this (either by doing proper caps intersection using the
 * GStreamer #GstCaps API, or by only taking into account the media type).
 *
 * Another potential source of problems are plugins such as ladspa or
 * libvisual where the list of elements depends on the installed
 * ladspa/libvisual plugins at the time. This is also up to the
 * distribution to handle (but usually not relevant for playback
 * applications).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "install-plugins.h"

#include <gst/gstinfo.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <string.h>

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_pb_utils_install_plugins_ensure_debug_category()

static GstDebugCategory *
gst_pb_utils_install_plugins_ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    GstDebugCategory *cat = NULL;

    GST_DEBUG_CATEGORY_INIT (cat, "install-plugins", 0,
        "GstPbUtils plugins installation helper");

    g_once_init_leave (&cat_gonce, (gsize) cat);
  }

  return (GstDebugCategory *) cat_gonce;
}
#endif /* GST_DISABLE_GST_DEBUG */

/* best effort to make things compile and possibly even work on win32 */
#ifndef WEXITSTATUS
# define WEXITSTATUS(status) ((((guint)(status)) & 0xff00) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(status) ((((guint)(status)) & 0x7f) == 0)
#endif

static gboolean install_in_progress;    /* FALSE */

/* private struct */
struct _GstInstallPluginsContext
{
  gchar *confirm_search;
  gchar *desktop_id;
  gchar *startup_notification_id;
  guint xid;
};

/**
 * gst_install_plugins_context_set_confirm_search:
 * @ctx: a #GstInstallPluginsContext
 * @confirm_search: whether to ask for confirmation before searching for plugins
 *
 * This function is used to tell the external installer process whether it
 * should ask for confirmation or not before searching for missing plugins.
 *
 * If set, this option will be passed to the installer via a
 * --interaction=[show-confirm-search|hide-confirm-search] command line option.
 *
 * Since: 1.6
 */
void
gst_install_plugins_context_set_confirm_search (GstInstallPluginsContext * ctx,
    gboolean confirm_search)
{
  g_return_if_fail (ctx != NULL);

  if (confirm_search)
    ctx->confirm_search = g_strdup ("show-confirm-search");
  else
    ctx->confirm_search = g_strdup ("hide-confirm-search");
}

/**
 * gst_install_plugins_context_set_desktop_id:
 * @ctx: a #GstInstallPluginsContext
 * @desktop_id: the desktop file ID of the calling application
 *
 * This function is used to pass the calling application's desktop file ID to
 * the external installer process.
 *
 * A desktop file ID is the basename of the desktop file, including the
 * .desktop extension.
 *
 * If set, the desktop file ID will be passed to the installer via a
 * --desktop-id= command line option.
 *
 * Since: 1.6
 */
void
gst_install_plugins_context_set_desktop_id (GstInstallPluginsContext * ctx,
    const gchar * desktop_id)
{
  g_return_if_fail (ctx != NULL);

  ctx->desktop_id = g_strdup (desktop_id);
}

/**
 * gst_install_plugins_context_set_startup_notification_id:
 * @ctx: a #GstInstallPluginsContext
 * @startup_id: the startup notification ID
 *
 * Sets the startup notification ID for the launched process.
 *
 * This is typically used to to pass the current X11 event timestamp to the
 * external installer process.
 *
 * Startup notification IDs are defined in the
 * [FreeDesktop.Org Startup Notifications standard](http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt).
 *
 * If set, the ID will be passed to the installer via a
 * --startup-notification-id= command line option.
 *
 * GTK+/GNOME applications should be able to create a startup notification ID
 * like this:
 * |[
 *   timestamp = gtk_get_current_event_time ();
 *   startup_id = g_strdup_printf ("_TIME%u", timestamp);
 * ...
 * ]|
 *
 * Since: 1.6
 */
void gst_install_plugins_context_set_startup_notification_id
    (GstInstallPluginsContext * ctx, const gchar * startup_id)
{
  g_return_if_fail (ctx != NULL);

  ctx->startup_notification_id = g_strdup (startup_id);
}

/**
 * gst_install_plugins_context_set_xid:
 * @ctx: a #GstInstallPluginsContext
 * @xid: the XWindow ID (XID) of the top-level application
 *
 * This function is for X11-based applications (such as most Gtk/Qt
 * applications on linux/unix) only. You can use it to tell the external
 * installer the XID of your main application window. That way the installer
 * can make its own window transient to your application window during the
 * installation.
 *
 * If set, the XID will be passed to the installer via a --transient-for=XID
 * command line option.
 *
 * Gtk+/Gnome application should be able to obtain the XID of the top-level
 * window like this:
 * |[
 * ##include <gtk/gtk.h>
 * ##ifdef GDK_WINDOWING_X11
 * ##include <gdk/gdkx.h>
 * ##endif
 * ...
 * ##ifdef GDK_WINDOWING_X11
 *   xid = GDK_WINDOW_XWINDOW (GTK_WIDGET (application_window)->window);
 * ##endif
 * ...
 * ]|
 *
 */
void
gst_install_plugins_context_set_xid (GstInstallPluginsContext * ctx, guint xid)
{
  g_return_if_fail (ctx != NULL);

  ctx->xid = xid;
}

/**
 * gst_install_plugins_context_new:
 *
 * Creates a new #GstInstallPluginsContext.
 *
 * Returns: a new #GstInstallPluginsContext. Free with
 * gst_install_plugins_context_free() when no longer needed
 */
GstInstallPluginsContext *
gst_install_plugins_context_new (void)
{
  return g_new0 (GstInstallPluginsContext, 1);
}

/**
 * gst_install_plugins_context_free:
 * @ctx: a #GstInstallPluginsContext
 *
 * Frees a #GstInstallPluginsContext.
 */
void
gst_install_plugins_context_free (GstInstallPluginsContext * ctx)
{
  g_return_if_fail (ctx != NULL);

  g_free (ctx->confirm_search);
  g_free (ctx->desktop_id);
  g_free (ctx->startup_notification_id);
  g_free (ctx);
}

/**
 * gst_install_plugins_context_copy:
 * @ctx: a #GstInstallPluginsContext
 *
 * Copies a #GstInstallPluginsContext.
 *
 * Returns: (transfer full): A copy of @ctx
 *
 * Since: 1.12.1
 */
GstInstallPluginsContext *
gst_install_plugins_context_copy (GstInstallPluginsContext * ctx)
{
  GstInstallPluginsContext *ret;

  ret = gst_install_plugins_context_new ();
  ret->confirm_search = g_strdup (ctx->confirm_search);
  ret->desktop_id = g_strdup (ctx->desktop_id);
  ret->startup_notification_id = g_strdup (ctx->startup_notification_id);
  ret->xid = ctx->xid;

  return ret;
}

G_DEFINE_BOXED_TYPE (GstInstallPluginsContext, gst_install_plugins_context,
    (GBoxedCopyFunc) gst_install_plugins_context_copy,
    (GBoxedFreeFunc) gst_install_plugins_context_free);

static const gchar *
gst_install_plugins_get_helper (void)
{
  const gchar *helper;

  helper = g_getenv ("GST_INSTALL_PLUGINS_HELPER");
  if (helper == NULL)
    helper = GST_INSTALL_PLUGINS_HELPER;

  GST_LOG ("Using plugin install helper '%s'", helper);
  return helper;
}

static gboolean
ptr_array_contains_string (GPtrArray * arr, const gchar * s)
{
  gint i;

  for (i = 0; i < arr->len; ++i) {
    if (strcmp ((const char *) g_ptr_array_index (arr, i), s) == 0)
      return TRUE;
  }
  return FALSE;
}

static gboolean
gst_install_plugins_spawn_child (const gchar * const *details,
    GstInstallPluginsContext * ctx, GPid * child_pid, gint * exit_status)
{
  GPtrArray *arr;
  gboolean ret;
  GError *err = NULL;
  gchar **argv;

  arr = g_ptr_array_new_with_free_func (g_free);

  /* argv[0] = helper path */
  g_ptr_array_add (arr, g_strdup (gst_install_plugins_get_helper ()));

  /* add any additional command line args from the context */
  if (ctx != NULL && ctx->confirm_search) {
    g_ptr_array_add (arr, g_strdup_printf ("--interaction=%s",
            ctx->confirm_search));
  }
  if (ctx != NULL && ctx->desktop_id != NULL) {
    g_ptr_array_add (arr, g_strdup_printf ("--desktop-id=%s", ctx->desktop_id));
  }
  if (ctx != NULL && ctx->startup_notification_id != NULL) {
    g_ptr_array_add (arr, g_strdup_printf ("--startup-notification-id=%s",
            ctx->startup_notification_id));
  }
  if (ctx != NULL && ctx->xid != 0) {
    g_ptr_array_add (arr, g_strdup_printf ("--transient-for=%u", ctx->xid));
  }

  /* finally, add the detail strings, but without duplicates */
  while (details != NULL && details[0] != NULL) {
    if (!ptr_array_contains_string (arr, details[0]))
      g_ptr_array_add (arr, g_strdup (details[0]));
    ++details;
  }

  /* and NULL-terminate */
  g_ptr_array_add (arr, NULL);

  argv = (gchar **) arr->pdata;

  if (child_pid == NULL && exit_status != NULL) {
    install_in_progress = TRUE;
    ret = g_spawn_sync (NULL, argv, NULL, (GSpawnFlags) 0, NULL, NULL,
        NULL, NULL, exit_status, &err);
    install_in_progress = FALSE;
  } else if (child_pid != NULL && exit_status == NULL) {
    install_in_progress = TRUE;
    ret = g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL,
        NULL, child_pid, &err);
  } else {
    g_return_val_if_reached (FALSE);
  }

  if (!ret) {
    GST_ERROR ("Error spawning plugin install helper: %s", err->message);
    g_error_free (err);
  }

  g_ptr_array_unref (arr);
  return ret;
}

static GstInstallPluginsReturn
gst_install_plugins_return_from_status (gint status)
{
  GstInstallPluginsReturn ret;

  /* did we exit cleanly? */
  if (!WIFEXITED (status)) {
    ret = GST_INSTALL_PLUGINS_CRASHED;
  } else {
    ret = (GstInstallPluginsReturn) WEXITSTATUS (status);

    /* did the helper return an invalid status code? */
    if (((guint) ret) >= GST_INSTALL_PLUGINS_STARTED_OK &&
        ret != GST_INSTALL_PLUGINS_INTERNAL_FAILURE) {
      ret = GST_INSTALL_PLUGINS_INVALID;
    }
  }

  GST_LOG ("plugin installer exited with status 0x%04x = %s", status,
      gst_install_plugins_return_get_name (ret));

  return ret;
}

typedef struct
{
  GstInstallPluginsResultFunc func;
  gpointer user_data;
} GstInstallPluginsAsyncHelper;

static void
gst_install_plugins_installer_exited (GPid pid, gint status, gpointer data)
{
  GstInstallPluginsAsyncHelper *helper;
  GstInstallPluginsReturn ret;

  install_in_progress = FALSE;

  helper = (GstInstallPluginsAsyncHelper *) data;
  ret = gst_install_plugins_return_from_status (status);

  GST_LOG ("calling plugin install result function %p", helper->func);
  helper->func (ret, helper->user_data);

  g_free (helper);
}

/**
 * gst_install_plugins_async:
 * @details: (array zero-terminated=1) (transfer none): NULL-terminated array
 *     of installer string details (see below)
 * @ctx: (allow-none): a #GstInstallPluginsContext, or NULL
 * @func: (scope async): the function to call when the installer program returns
 * @user_data: (closure): the user data to pass to @func when called, or NULL
 *
 * Requests plugin installation without blocking. Once the plugins have been
 * installed or installation has failed, @func will be called with the result
 * of the installation and your provided @user_data pointer.
 *
 * This function requires a running GLib/Gtk main loop. If you are not
 * running a GLib/Gtk main loop, make sure to regularly call
 * g_main_context_iteration(NULL,FALSE).
 *
 * The installer strings that make up @detail are typically obtained by
 * calling gst_missing_plugin_message_get_installer_detail() on missing-plugin
 * messages that have been caught on a pipeline's bus or created by the
 * application via the provided API, such as gst_missing_element_message_new().
 *
 * It is possible to request the installation of multiple missing plugins in
 * one go (as might be required if there is a demuxer for a certain format
 * installed but no suitable video decoder and no suitable audio decoder).
 *
 * Returns: result code whether an external installer could be started
 */

GstInstallPluginsReturn
gst_install_plugins_async (const gchar * const *details,
    GstInstallPluginsContext * ctx, GstInstallPluginsResultFunc func,
    gpointer user_data)
{
  GstInstallPluginsAsyncHelper *helper;
  GPid pid;

  g_return_val_if_fail (details != NULL, GST_INSTALL_PLUGINS_INTERNAL_FAILURE);
  g_return_val_if_fail (func != NULL, GST_INSTALL_PLUGINS_INTERNAL_FAILURE);

  if (install_in_progress)
    return GST_INSTALL_PLUGINS_INSTALL_IN_PROGRESS;

  /* if we can't access our helper, don't bother */
  if (!g_file_test (gst_install_plugins_get_helper (),
          G_FILE_TEST_IS_EXECUTABLE))
    return GST_INSTALL_PLUGINS_HELPER_MISSING;

  if (!gst_install_plugins_spawn_child (details, ctx, &pid, NULL))
    return GST_INSTALL_PLUGINS_INTERNAL_FAILURE;

  helper = g_new (GstInstallPluginsAsyncHelper, 1);
  helper->func = func;
  helper->user_data = user_data;

  g_child_watch_add (pid, gst_install_plugins_installer_exited, helper);

  return GST_INSTALL_PLUGINS_STARTED_OK;
}

/**
 * gst_install_plugins_sync:
 * @details: (array zero-terminated=1) (transfer none): NULL-terminated array
 *     of installer string details
 * @ctx: (allow-none): a #GstInstallPluginsContext, or NULL
 *
 * Requests plugin installation and block until the plugins have been
 * installed or installation has failed.
 *
 * This function should almost never be used, it only exists for cases where
 * a non-GLib main loop is running and the user wants to run it in a separate
 * thread and marshal the result back asynchronously into the main thread
 * using the other non-GLib main loop. You should almost always use
 * gst_install_plugins_async() instead of this function.
 *
 * Returns: the result of the installation.
 */
GstInstallPluginsReturn
gst_install_plugins_sync (const gchar * const *details,
    GstInstallPluginsContext * ctx)
{
  gint status;

  g_return_val_if_fail (details != NULL, GST_INSTALL_PLUGINS_INTERNAL_FAILURE);

  if (install_in_progress)
    return GST_INSTALL_PLUGINS_INSTALL_IN_PROGRESS;

  /* if we can't access our helper, don't bother */
  if (!g_file_test (gst_install_plugins_get_helper (),
          G_FILE_TEST_IS_EXECUTABLE))
    return GST_INSTALL_PLUGINS_HELPER_MISSING;

  if (!gst_install_plugins_spawn_child (details, ctx, NULL, &status))
    return GST_INSTALL_PLUGINS_INTERNAL_FAILURE;

  return gst_install_plugins_return_from_status (status);
}

/**
 * gst_install_plugins_return_get_name:
 * @ret: the return status code
 *
 * Convenience function to return the descriptive string associated
 * with a status code.  This function returns English strings and
 * should not be used for user messages. It is here only to assist
 * in debugging.
 *
 * Returns: a descriptive string for the status code in @ret
 */
const gchar *
gst_install_plugins_return_get_name (GstInstallPluginsReturn ret)
{
  switch (ret) {
    case GST_INSTALL_PLUGINS_SUCCESS:
      return "success";
    case GST_INSTALL_PLUGINS_NOT_FOUND:
      return "not-found";
    case GST_INSTALL_PLUGINS_ERROR:
      return "install-error";
    case GST_INSTALL_PLUGINS_CRASHED:
      return "installer-exit-unclean";
    case GST_INSTALL_PLUGINS_PARTIAL_SUCCESS:
      return "partial-success";
    case GST_INSTALL_PLUGINS_USER_ABORT:
      return "user-abort";
    case GST_INSTALL_PLUGINS_STARTED_OK:
      return "started-ok";
    case GST_INSTALL_PLUGINS_INTERNAL_FAILURE:
      return "internal-failure";
    case GST_INSTALL_PLUGINS_HELPER_MISSING:
      return "helper-missing";
    case GST_INSTALL_PLUGINS_INSTALL_IN_PROGRESS:
      return "install-in-progress";
    case GST_INSTALL_PLUGINS_INVALID:
      return "invalid";
    default:
      break;
  }
  return "(UNKNOWN)";
}

/**
 * gst_install_plugins_installation_in_progress:
 *
 * Checks whether plugin installation (initiated by this application only)
 * is currently in progress.
 *
 * Returns: TRUE if plugin installation is in progress, otherwise FALSE
 */
gboolean
gst_install_plugins_installation_in_progress (void)
{
  return install_in_progress;
}

/**
 * gst_install_plugins_supported:
 *
 * Checks whether plugin installation is likely to be supported by the
 * current environment. This currently only checks whether the helper script
 * that is to be provided by the distribution or operating system vendor
 * exists.
 *
 * Returns: TRUE if plugin installation is likely to be supported.
 */
gboolean
gst_install_plugins_supported (void)
{
  return g_file_test (gst_install_plugins_get_helper (),
      G_FILE_TEST_IS_EXECUTABLE);
}
