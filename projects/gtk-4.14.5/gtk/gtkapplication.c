/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplication.h"
#include "gdkprofilerprivate.h"

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gtkapplicationprivate.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkicontheme.h"
#include "gtkbuilder.h"
#include "gtkprivate.h"

/* NB: please do not add backend-specific GDK headers here.  This should
 * be abstracted via GtkApplicationImpl.
 */

/**
 * GtkApplication:
 *
 * `GtkApplication` is a high-level API for writing applications.
 *
 * It supports many aspects of writing a GTK application in a convenient
 * fashion, without enforcing a one-size-fits-all model.
 *
 * Currently, `GtkApplication` handles GTK initialization, application
 * uniqueness, session management, provides some basic scriptability and
 * desktop shell integration by exporting actions and menus and manages a
 * list of toplevel windows whose life-cycle is automatically tied to the
 * life-cycle of your application.
 *
 * While `GtkApplication` works fine with plain [class@Gtk.Window]s, it is
 * recommended to use it together with [class@Gtk.ApplicationWindow].
 *
 * ## Automatic resources
 *
 * `GtkApplication` will automatically load menus from the `GtkBuilder`
 * resource located at "gtk/menus.ui", relative to the application's
 * resource base path (see [method@Gio.Application.set_resource_base_path]).
 * The menu with the ID "menubar" is taken as the application's
 * menubar. Additional menus (most interesting submenus) can be named
 * and accessed via [method@Gtk.Application.get_menu_by_id] which allows for
 * dynamic population of a part of the menu structure.
 *
 * Note that automatic resource loading uses the resource base path
 * that is set at construction time and will not work if the resource
 * base path is changed at a later time.
 *
 * It is also possible to provide the menubar manually using
 * [method@Gtk.Application.set_menubar].
 *
 * `GtkApplication` will also automatically setup an icon search path for
 * the default icon theme by appending "icons" to the resource base
 * path. This allows your application to easily store its icons as
 * resources. See [method@Gtk.IconTheme.add_resource_path] for more
 * information.
 *
 * If there is a resource located at `gtk/help-overlay.ui` which
 * defines a [class@Gtk.ShortcutsWindow] with ID `help_overlay` then
 * `GtkApplication` associates an instance of this shortcuts window with
 * each [class@Gtk.ApplicationWindow] and sets up the keyboard accelerator
 * <kbd>Control</kbd>+<kbd>?</kbd> to open it. To create a menu item that
 * displays the shortcuts window, associate the item with the action
 * `win.show-help-overlay`.
 *
 * ## A simple application
 *
 * [A simple example](https://gitlab.gnome.org/GNOME/gtk/tree/main/examples/bp/bloatpad.c)
 * is available in the GTK source code repository
 *
 * `GtkApplication` optionally registers with a session manager of the
 * users session (if you set the [property@Gtk.Application:register-session]
 * property) and offers various functionality related to the session
 * life-cycle.
 *
 * An application can block various ways to end the session with
 * the [method@Gtk.Application.inhibit] function. Typical use cases for
 * this kind of inhibiting are long-running, uninterruptible operations,
 * such as burning a CD or performing a disk backup. The session
 * manager may not honor the inhibitor, but it can be expected to
 * inform the user about the negative consequences of ending the
 * session while inhibitors are present.
 *
 * ## See Also
 *
 * [HowDoI: Using GtkApplication](https://wiki.gnome.org/HowDoI/GtkApplication),
 * [Getting Started with GTK: Basics](getting_started.html#basics)
 */

enum {
  WINDOW_ADDED,
  WINDOW_REMOVED,
  QUERY_END,
  LAST_SIGNAL
};

static guint gtk_application_signals[LAST_SIGNAL];

enum {
  PROP_ZERO,
  PROP_REGISTER_SESSION,
  PROP_SCREENSAVER_ACTIVE,
  PROP_MENUBAR,
  PROP_ACTIVE_WINDOW,
  NUM_PROPERTIES
};

static GParamSpec *gtk_application_props[NUM_PROPERTIES];

typedef struct
{
  GtkApplicationImpl *impl;
  GtkApplicationAccels *accels;

  GList *windows;

  GMenuModel      *menubar;
  guint            last_window_id;

  gboolean         register_session;
  gboolean         screensaver_active;
  GtkActionMuxer  *muxer;
  GtkBuilder      *menus_builder;
  char            *help_overlay_path;
} GtkApplicationPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

static void
gtk_application_window_active_cb (GtkWindow      *window,
                                  GParamSpec     *pspec,
                                  GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);
  GList *link;

  if (!gtk_window_is_active (window))
    return;

  /* Keep the window list sorted by most-recently-focused. */
  link = g_list_find (priv->windows, window);
  if (link != NULL && link != priv->windows)
    {
      priv->windows = g_list_remove_link (priv->windows, link);
      priv->windows = g_list_concat (link, priv->windows);
    }

  if (priv->impl)
    gtk_application_impl_active_window_changed (priv->impl, window);

  g_object_notify_by_pspec (G_OBJECT (application), gtk_application_props[PROP_ACTIVE_WINDOW]);
}

static void
gtk_application_load_resources (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);
  const char *base_path;
  const char *optional_slash = "/";

  base_path = g_application_get_resource_base_path (G_APPLICATION (application));

  if (base_path == NULL)
    return;

  if (base_path[strlen (base_path) - 1] == '/')
    optional_slash = "";

  /* Expand the icon search path */
  {
    GtkIconTheme *default_theme;
    char *iconspath;

    default_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    iconspath = g_strconcat (base_path, optional_slash, "icons/", NULL);
    gtk_icon_theme_add_resource_path (default_theme, iconspath);
    g_free (iconspath);
  }

  /* Load the menus */
  {
    char *menuspath;

    menuspath = g_strconcat (base_path, optional_slash, "gtk/menus.ui", NULL);
    if (g_resources_get_info (menuspath, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL))
      priv->menus_builder = gtk_builder_new_from_resource (menuspath);
    g_free (menuspath);

    if (priv->menus_builder)
      {
        GObject *menu;

        menu = gtk_builder_get_object (priv->menus_builder, "menubar");
        if (menu != NULL && G_IS_MENU_MODEL (menu))
          gtk_application_set_menubar (application, G_MENU_MODEL (menu));
      }
  }

  /* Help overlay */
  {
    char *path;

    path = g_strconcat (base_path, optional_slash, "gtk/help-overlay.ui", NULL);
    if (g_resources_get_info (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL))
      {
        const char * const accels[] = { "<Control>question", NULL };

        priv->help_overlay_path = path;
        gtk_application_set_accels_for_action (application, "win.show-help-overlay", accels);
      }
    else
      {
        g_free (path);
      }
  }
}


static void
gtk_application_startup (GApplication *g_application)
{
  GtkApplication *application = GTK_APPLICATION (g_application);
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);
  gint64 before G_GNUC_UNUSED;
  gint64 before2 G_GNUC_UNUSED;

  before = GDK_PROFILER_CURRENT_TIME;

  G_APPLICATION_CLASS (gtk_application_parent_class)->startup (g_application);

  gtk_action_muxer_insert (priv->muxer, "app", G_ACTION_GROUP (application));

  before2 = GDK_PROFILER_CURRENT_TIME;
  gtk_init ();
  gdk_profiler_end_mark (before2, "gtk_init", NULL);

  priv->impl = gtk_application_impl_new (application, gdk_display_get_default ());
  gtk_application_impl_startup (priv->impl, priv->register_session);

  gtk_application_load_resources (application);

  gdk_profiler_end_mark (before, "Application startup", NULL);
}

static void
gtk_application_shutdown (GApplication *g_application)
{
  GtkApplication *application = GTK_APPLICATION (g_application);
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  if (priv->impl == NULL)
    return;

  gtk_application_impl_shutdown (priv->impl);
  g_clear_object (&priv->impl);

  gtk_action_muxer_remove (priv->muxer, "app");

  gtk_main_sync ();

  G_APPLICATION_CLASS (gtk_application_parent_class)->shutdown (g_application);
}

static gboolean
gtk_application_local_command_line (GApplication   *application,
                                    char         ***arguments,
                                    int            *exit_status)
{
  /* We need to call setlocale() here so --help output works */
  setlocale_initialization ();

  return G_APPLICATION_CLASS (gtk_application_parent_class)->local_command_line (application, arguments, exit_status);
}

static void
gtk_application_add_platform_data (GApplication    *application,
                                   GVariantBuilder *builder)
{
  GdkDisplay *display;

  /* This is slightly evil.
   *
   * We don't have an impl here because we're remote so we can't figure
   * out what to do on a per-display-server basis.
   *
   * So we do all the things... which currently is just one thing.
   */
  display = gdk_display_get_default ();
  if (display)
    {
      const char *startup_id;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      startup_id = gdk_display_get_startup_notification_id (display);
G_GNUC_END_IGNORE_DEPRECATIONS
      if (startup_id && g_utf8_validate (startup_id, -1, NULL))
        {
          g_variant_builder_add (builder, "{sv}", "activation-token",
                                 g_variant_new_string (startup_id));
          g_variant_builder_add (builder, "{sv}", "desktop-startup-id",
                                 g_variant_new_string (startup_id));
        }
    }
}

static void
gtk_application_before_emit (GApplication *g_application,
                             GVariant     *platform_data)
{
  GtkApplication *application = GTK_APPLICATION (g_application);
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  gtk_application_impl_before_emit (priv->impl, platform_data);
}

static void
gtk_application_after_emit (GApplication *application,
                            GVariant     *platform_data)
{
}

static void
gtk_application_init (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  priv->muxer = gtk_action_muxer_new (NULL);

  priv->accels = gtk_application_accels_new ();
}

static void
gtk_application_window_added (GtkApplication *application,
                              GtkWindow      *window)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  if (GTK_IS_APPLICATION_WINDOW (window))
    {
      gtk_application_window_set_id (GTK_APPLICATION_WINDOW (window), ++priv->last_window_id);
      if (priv->help_overlay_path)
        {
          GtkBuilder *builder;
          GtkWidget *help_overlay;

          builder = gtk_builder_new_from_resource (priv->help_overlay_path);
          help_overlay = GTK_WIDGET (gtk_builder_get_object (builder, "help_overlay"));
          if (GTK_IS_SHORTCUTS_WINDOW (help_overlay))
            gtk_application_window_set_help_overlay (GTK_APPLICATION_WINDOW (window),
                                                     GTK_SHORTCUTS_WINDOW (help_overlay));
          g_object_unref (builder);
        }
    }

  priv->windows = g_list_prepend (priv->windows, window);
  gtk_window_set_application (window, application);
  g_application_hold (G_APPLICATION (application));

  g_signal_connect (window, "notify::is-active",
                    G_CALLBACK (gtk_application_window_active_cb),
                    application);

  gtk_application_impl_window_added (priv->impl, window);

  gtk_application_impl_active_window_changed (priv->impl, window);

  g_object_notify_by_pspec (G_OBJECT (application), gtk_application_props[PROP_ACTIVE_WINDOW]);
}

static void
gtk_application_window_removed (GtkApplication *application,
                                GtkWindow      *window)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);
  gpointer old_active;

  old_active = priv->windows;

  if (priv->impl)
    gtk_application_impl_window_removed (priv->impl, window);

  g_signal_handlers_disconnect_by_func (window,
                                        gtk_application_window_active_cb,
                                        application);

  g_application_release (G_APPLICATION (application));
  priv->windows = g_list_remove (priv->windows, window);
  gtk_window_set_application (window, NULL);

  if (priv->windows != old_active && priv->impl)
    {
      gtk_application_impl_active_window_changed (priv->impl, priv->windows ? priv->windows->data : NULL);
      g_object_notify_by_pspec (G_OBJECT (application), gtk_application_props[PROP_ACTIVE_WINDOW]);
    }
}

static void
gtk_application_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  switch (prop_id)
    {
    case PROP_REGISTER_SESSION:
      g_value_set_boolean (value, priv->register_session);
      break;

    case PROP_SCREENSAVER_ACTIVE:
      g_value_set_boolean (value, priv->screensaver_active);
      break;

    case PROP_MENUBAR:
      g_value_set_object (value, gtk_application_get_menubar (application));
      break;

    case PROP_ACTIVE_WINDOW:
      g_value_set_object (value, gtk_application_get_active_window (application));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_application_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  switch (prop_id)
    {
    case PROP_REGISTER_SESSION:
      priv->register_session = g_value_get_boolean (value);
      break;

    case PROP_MENUBAR:
      gtk_application_set_menubar (application, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_application_finalize (GObject *object)
{
  GtkApplication *application = GTK_APPLICATION (object);
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_clear_object (&priv->menus_builder);
  g_clear_object (&priv->menubar);
  g_clear_object (&priv->muxer);
  g_clear_object (&priv->accels);

  g_free (priv->help_overlay_path);

  G_OBJECT_CLASS (gtk_application_parent_class)->finalize (object);
}

static gboolean
gtk_application_dbus_register (GApplication     *application,
                               GDBusConnection  *connection,
                               const char       *object_path,
                               GError          **error)
{
  return TRUE;
}

static void
gtk_application_dbus_unregister (GApplication     *application,
                                 GDBusConnection  *connection,
                                 const char       *object_path)
{
}

static void
gtk_application_class_init (GtkApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->get_property = gtk_application_get_property;
  object_class->set_property = gtk_application_set_property;
  object_class->finalize = gtk_application_finalize;

  application_class->local_command_line = gtk_application_local_command_line;
  application_class->add_platform_data = gtk_application_add_platform_data;
  application_class->before_emit = gtk_application_before_emit;
  application_class->after_emit = gtk_application_after_emit;
  application_class->startup = gtk_application_startup;
  application_class->shutdown = gtk_application_shutdown;
  application_class->dbus_register = gtk_application_dbus_register;
  application_class->dbus_unregister = gtk_application_dbus_unregister;

  class->window_added = gtk_application_window_added;
  class->window_removed = gtk_application_window_removed;

  /**
   * GtkApplication::window-added:
   * @application: the `GtkApplication` which emitted the signal
   * @window: the newly-added [class@Gtk.Window]
   *
   * Emitted when a [class@Gtk.Window] is added to `application` through
   * [method@Gtk.Application.add_window].
   */
  gtk_application_signals[WINDOW_ADDED] =
    g_signal_new (I_("window-added"), GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, window_added),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_WINDOW);

  /**
   * GtkApplication::window-removed:
   * @application: the `GtkApplication` which emitted the signal
   * @window: the [class@Gtk.Window] that is being removed
   *
   * Emitted when a [class@Gtk.Window] is removed from `application`.
   *
   * This can happen as a side-effect of the window being destroyed
   * or explicitly through [method@Gtk.Application.remove_window].
   */
  gtk_application_signals[WINDOW_REMOVED] =
    g_signal_new (I_("window-removed"), GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, window_removed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_WINDOW);

  /**
   * GtkApplication::query-end:
   * @application: the `GtkApplication` which emitted the signal
   *
   * Emitted when the session manager is about to end the session.
   *
   * This signal is only emitted if [property@Gtk.Application:register-session]
   * is `TRUE`. Applications can connect to this signal and call
   * [method@Gtk.Application.inhibit] with `GTK_APPLICATION_INHIBIT_LOGOUT`
   * to delay the end of the session until state has been saved.
   */
  gtk_application_signals[QUERY_END] =
    g_signal_new (I_("query-end"), GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkApplication:register-session:
   *
   * Set this property to `TRUE` to register with the session manager.
   *
   * This will make GTK track the session state (such as the
   * [property@Gtk.Application:screensaver-active] property).
   */
  gtk_application_props[PROP_REGISTER_SESSION] =
    g_param_spec_boolean ("register-session", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);

  /**
   * GtkApplication:screensaver-active:
   *
   * This property is `TRUE` if GTK believes that the screensaver is
   * currently active.
   *
   * GTK only tracks session state (including this) when
   * [property@Gtk.Application:register-session] is set to %TRUE.
   *
   * Tracking the screensaver state is currently only supported on
   * Linux.
   */
  gtk_application_props[PROP_SCREENSAVER_ACTIVE] =
    g_param_spec_boolean ("screensaver-active", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE|G_PARAM_STATIC_STRINGS);

  /**
   * GtkApplication:menubar: (attributes org.gtk.Property.get=gtk_application_get_menubar org.gtk.Property.set=gtk_application_set_menubar)
   *
   * The `GMenuModel` to be used for the application's menu bar.
   */
  gtk_application_props[PROP_MENUBAR] =
    g_param_spec_object ("menubar", NULL, NULL,
                         G_TYPE_MENU_MODEL,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);

  /**
   * GtkApplication:active-window: (attributes org.gtk.Property.get=gtk_application_get_active_window)
   *
   * The currently focused window of the application.
   */
  gtk_application_props[PROP_ACTIVE_WINDOW] =
    g_param_spec_object ("active-window", NULL, NULL,
                         GTK_TYPE_WINDOW,
                         G_PARAM_READABLE|G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, gtk_application_props);
}

/**
 * gtk_application_new:
 * @application_id: (nullable): The application ID
 * @flags: the application flags
 *
 * Creates a new `GtkApplication` instance.
 *
 * When using `GtkApplication`, it is not necessary to call [func@Gtk.init]
 * manually. It is called as soon as the application gets registered as
 * the primary instance.
 *
 * Concretely, [func@Gtk.init] is called in the default handler for the
 * `GApplication::startup` signal. Therefore, `GtkApplication` subclasses should
 * always chain up in their `GApplication::startup` handler before using any GTK
 * API.
 *
 * Note that commandline arguments are not passed to [func@Gtk.init].
 *
 * If `application_id` is not %NULL, then it must be valid. See
 * `g_application_id_is_valid()`.
 *
 * If no application ID is given then some features (most notably application
 * uniqueness) will be disabled.
 *
 * Returns: a new `GtkApplication` instance
 */
GtkApplication *
gtk_application_new (const char        *application_id,
                     GApplicationFlags  flags)
{
  g_return_val_if_fail (application_id == NULL || g_application_id_is_valid (application_id), NULL);

  return g_object_new (GTK_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

/**
 * gtk_application_add_window:
 * @application: a `GtkApplication`
 * @window: a `GtkWindow`
 *
 * Adds a window to `application`.
 *
 * This call can only happen after the `application` has started;
 * typically, you should add new application windows in response
 * to the emission of the `GApplication::activate` signal.
 *
 * This call is equivalent to setting the [property@Gtk.Window:application]
 * property of `window` to `application`.
 *
 * Normally, the connection between the application and the window
 * will remain until the window is destroyed, but you can explicitly
 * remove it with [method@Gtk.Application.remove_window].
 *
 * GTK will keep the `application` running as long as it has
 * any windows.
 **/
void
gtk_application_add_window (GtkApplication *application,
                            GtkWindow      *window)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (!g_application_get_is_registered (G_APPLICATION (application)))
    {
      g_critical ("New application windows must be added after the "
                  "GApplication::startup signal has been emitted.");
      return;
    }

  if (!g_list_find (priv->windows, window))
    g_signal_emit (application,
                   gtk_application_signals[WINDOW_ADDED], 0, window);
}

/**
 * gtk_application_remove_window:
 * @application: a `GtkApplication`
 * @window: a `GtkWindow`
 *
 * Remove a window from `application`.
 *
 * If `window` belongs to `application` then this call is equivalent to
 * setting the [property@Gtk.Window:application] property of `window` to
 * `NULL`.
 *
 * The application may stop running as a result of a call to this
 * function, if `window` was the last window of the `application`.
 **/
void
gtk_application_remove_window (GtkApplication *application,
                               GtkWindow      *window)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (g_list_find (priv->windows, window))
    g_signal_emit (application,
                   gtk_application_signals[WINDOW_REMOVED], 0, window);
}

/**
 * gtk_application_get_windows:
 * @application: a `GtkApplication`
 *
 * Gets a list of the [class@Gtk.Window] instances associated with `application`.
 *
 * The list is sorted by most recently focused window, such that the first
 * element is the currently focused window. (Useful for choosing a parent
 * for a transient window.)
 *
 * The list that is returned should not be modified in any way. It will
 * only remain valid until the next focus change or window creation or
 * deletion.
 *
 * Returns: (element-type GtkWindow) (transfer none): a `GList` of `GtkWindow`
 *   instances
 **/
GList *
gtk_application_get_windows (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return priv->windows;
}

/**
 * gtk_application_get_window_by_id:
 * @application: a `GtkApplication`
 * @id: an identifier number
 *
 * Returns the [class@Gtk.ApplicationWindow] with the given ID.
 *
 * The ID of a `GtkApplicationWindow` can be retrieved with
 * [method@Gtk.ApplicationWindow.get_id].
 *
 * Returns: (nullable) (transfer none): the window for the given `id`
 */
GtkWindow *
gtk_application_get_window_by_id (GtkApplication *application,
                                  guint           id)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);
  GList *l;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  for (l = priv->windows; l != NULL; l = l->next)
    {
      if (GTK_IS_APPLICATION_WINDOW (l->data) &&
          gtk_application_window_get_id (GTK_APPLICATION_WINDOW (l->data)) == id)
        return l->data;
    }

  return NULL;
}

/**
 * gtk_application_get_active_window: (attributes org.gtk.Method.get_property=active-window)
 * @application: a `GtkApplication`
 *
 * Gets the “active” window for the application.
 *
 * The active window is the one that was most recently focused (within
 * the application).  This window may not have the focus at the moment
 * if another application has it — this is just the most
 * recently-focused window within this application.
 *
 * Returns: (transfer none) (nullable): the active window
 **/
GtkWindow *
gtk_application_get_active_window (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return priv->windows ? priv->windows->data : NULL;
}

static void
gtk_application_update_accels (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);
  GList *l;

  for (l = priv->windows; l != NULL; l = l->next)
    _gtk_window_notify_keys_changed (l->data);
}

/**
 * gtk_application_set_menubar: (attributes org.gtk.Method.set_property=menubar)
 * @application: a `GtkApplication`
 * @menubar: (nullable): a `GMenuModel`
 *
 * Sets or unsets the menubar for windows of `application`.
 *
 * This is a menubar in the traditional sense.
 *
 * This can only be done in the primary instance of the application,
 * after it has been registered. `GApplication::startup` is a good place
 * to call this.
 *
 * Depending on the desktop environment, this may appear at the top of
 * each window, or at the top of the screen.  In some environments, if
 * both the application menu and the menubar are set, the application
 * menu will be presented as if it were the first item of the menubar.
 * Other environments treat the two as completely separate — for example,
 * the application menu may be rendered by the desktop shell while the
 * menubar (if set) remains in each individual window.
 *
 * Use the base `GActionMap` interface to add actions, to respond to the
 * user selecting these menu items.
 */
void
gtk_application_set_menubar (GtkApplication *application,
                             GMenuModel     *menubar)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (g_application_get_is_registered (G_APPLICATION (application)));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));
  g_return_if_fail (menubar == NULL || G_IS_MENU_MODEL (menubar));

  if (g_set_object (&priv->menubar, menubar))
    {
      gtk_application_impl_set_menubar (priv->impl, menubar);

      g_object_notify_by_pspec (G_OBJECT (application), gtk_application_props[PROP_MENUBAR]);
    }
}

/**
 * gtk_application_get_menubar: (attributes org.gtk.Method.get_property=menubar)
 * @application: a `GtkApplication`
 *
 * Returns the menu model that has been set with
 * [method@Gtk.Application.set_menubar].
 *
 * Returns: (nullable) (transfer none): the menubar for windows of `application`
 */
GMenuModel *
gtk_application_get_menubar (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return priv->menubar;
}

/**
 * GtkApplicationInhibitFlags:
 * @GTK_APPLICATION_INHIBIT_LOGOUT: Inhibit ending the user session
 *   by logging out or by shutting down the computer
 * @GTK_APPLICATION_INHIBIT_SWITCH: Inhibit user switching
 * @GTK_APPLICATION_INHIBIT_SUSPEND: Inhibit suspending the
 *   session or computer
 * @GTK_APPLICATION_INHIBIT_IDLE: Inhibit the session being
 *   marked as idle (and possibly locked)
 *
 * Types of user actions that may be blocked by `GtkApplication`.
 *
 * See [method@Gtk.Application.inhibit].
 */

/**
 * gtk_application_inhibit:
 * @application: the `GtkApplication`
 * @window: (nullable): a `GtkWindow`
 * @flags: what types of actions should be inhibited
 * @reason: (nullable): a short, human-readable string that explains
 *   why these operations are inhibited
 *
 * Inform the session manager that certain types of actions should be
 * inhibited.
 *
 * This is not guaranteed to work on all platforms and for all types of
 * actions.
 *
 * Applications should invoke this method when they begin an operation
 * that should not be interrupted, such as creating a CD or DVD. The
 * types of actions that may be blocked are specified by the `flags`
 * parameter. When the application completes the operation it should
 * call [method@Gtk.Application.uninhibit] to remove the inhibitor. Note
 * that an application can have multiple inhibitors, and all of them must
 * be individually removed. Inhibitors are also cleared when the
 * application exits.
 *
 * Applications should not expect that they will always be able to block
 * the action. In most cases, users will be given the option to force
 * the action to take place.
 *
 * The `reason` message should be short and to the point.
 *
 * If `window` is given, the session manager may point the user to
 * this window to find out more about why the action is inhibited.
 *
 * Returns: A non-zero cookie that is used to uniquely identify this
 *   request. It should be used as an argument to [method@Gtk.Application.uninhibit]
 *   in order to remove the request. If the platform does not support
 *   inhibiting or the request failed for some reason, 0 is returned.
 */
guint
gtk_application_inhibit (GtkApplication             *application,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const char                 *reason)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_val_if_fail (GTK_IS_APPLICATION (application), 0);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), 0);
  g_return_val_if_fail (window == NULL || GTK_IS_WINDOW (window), 0);

  return gtk_application_impl_inhibit (priv->impl, window, flags, reason);
}

/**
 * gtk_application_uninhibit:
 * @application: the `GtkApplication`
 * @cookie: a cookie that was returned by [method@Gtk.Application.inhibit]
 *
 * Removes an inhibitor that has been previously established.
 *
 * See [method@Gtk.Application.inhibit].
 *
 * Inhibitors are also cleared when the application exits.
 */
void
gtk_application_uninhibit (GtkApplication *application,
                           guint           cookie)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));
  g_return_if_fail (cookie > 0);

  gtk_application_impl_uninhibit (priv->impl, cookie);
}

GtkActionMuxer *
gtk_application_get_parent_muxer_for_window (GtkWindow *window)
{
  GtkApplication *application = gtk_window_get_application (window);
  GtkApplicationPrivate *priv;

  if (!application)
    return NULL;

  priv = gtk_application_get_instance_private (application);

  return priv->muxer;
}

GtkApplicationAccels *
gtk_application_get_application_accels (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  return priv->accels;
}

/**
 * gtk_application_list_action_descriptions:
 * @application: a `GtkApplication`
 *
 * Lists the detailed action names which have associated accelerators.
 *
 * See [method@Gtk.Application.set_accels_for_action].
 *
 * Returns: (transfer full) (array zero-terminated=1): the detailed action names
 */
char **
gtk_application_list_action_descriptions (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return gtk_application_accels_list_action_descriptions (priv->accels);
}

/**
 * gtk_application_set_accels_for_action:
 * @application: a `GtkApplication`
 * @detailed_action_name: a detailed action name, specifying an action
 *   and target to associate accelerators with
 * @accels: (array zero-terminated=1): a list of accelerators in the format
 *   understood by [func@Gtk.accelerator_parse]
 *
 * Sets zero or more keyboard accelerators that will trigger the
 * given action.
 *
 * The first item in `accels` will be the primary accelerator, which may be
 * displayed in the UI.
 *
 * To remove all accelerators for an action, use an empty, zero-terminated
 * array for `accels`.
 *
 * For the `detailed_action_name`, see `g_action_parse_detailed_name()` and
 * `g_action_print_detailed_name()`.
 */
void
gtk_application_set_accels_for_action (GtkApplication      *application,
                                       const char          *detailed_action_name,
                                       const char * const *accels)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);
  char *action_and_target;

  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (detailed_action_name != NULL);
  g_return_if_fail (accels != NULL);

  gtk_application_accels_set_accels_for_action (priv->accels,
                                                detailed_action_name,
                                                accels);

  action_and_target = gtk_normalise_detailed_action_name (detailed_action_name);
  gtk_action_muxer_set_primary_accel (priv->muxer, action_and_target, accels[0]);
  g_free (action_and_target);

  gtk_application_update_accels (application);
}

/**
 * gtk_application_get_accels_for_action:
 * @application: a `GtkApplication`
 * @detailed_action_name: a detailed action name, specifying an action
 *   and target to obtain accelerators for
 *
 * Gets the accelerators that are currently associated with
 * the given action.
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8):
 *   accelerators for `detailed_action_name`
 */
char **
gtk_application_get_accels_for_action (GtkApplication *application,
                                       const char     *detailed_action_name)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (detailed_action_name != NULL, NULL);

  return gtk_application_accels_get_accels_for_action (priv->accels,
                                                       detailed_action_name);
}

/**
 * gtk_application_get_actions_for_accel:
 * @application: a `GtkApplication`
 * @accel: an accelerator that can be parsed by [func@Gtk.accelerator_parse]
 *
 * Returns the list of actions (possibly empty) that `accel` maps to.
 *
 * Each item in the list is a detailed action name in the usual form.
 *
 * This might be useful to discover if an accel already exists in
 * order to prevent installation of a conflicting accelerator (from
 * an accelerator editor or a plugin system, for example). Note that
 * having more than one action per accelerator may not be a bad thing
 * and might make sense in cases where the actions never appear in the
 * same context.
 *
 * In case there are no actions for a given accelerator, an empty array
 * is returned. `NULL` is never returned.
 *
 * It is a programmer error to pass an invalid accelerator string.
 *
 * If you are unsure, check it with [func@Gtk.accelerator_parse] first.
 *
 * Returns: (transfer full): a %NULL-terminated array of actions for `accel`
 */
char **
gtk_application_get_actions_for_accel (GtkApplication *application,
                                       const char     *accel)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (accel != NULL, NULL);

  return gtk_application_accels_get_actions_for_accel (priv->accels, accel);
}

GtkActionMuxer *
gtk_application_get_action_muxer (GtkApplication *application)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  g_assert (priv->muxer);

  return priv->muxer;
}

void
gtk_application_insert_action_group (GtkApplication *application,
                                     const char     *name,
                                     GActionGroup   *action_group)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  gtk_action_muxer_insert (priv->muxer, name, action_group);
}

void
gtk_application_handle_window_realize (GtkApplication *application,
                                       GtkWindow      *window)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  if (priv->impl)
    gtk_application_impl_handle_window_realize (priv->impl, window);
}

void
gtk_application_handle_window_map (GtkApplication *application,
                                   GtkWindow      *window)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  if (priv->impl)
    gtk_application_impl_handle_window_map (priv->impl, window);
}

/**
 * gtk_application_get_menu_by_id:
 * @application: a `GtkApplication`
 * @id: the id of the menu to look up
 *
 * Gets a menu from automatically loaded resources.
 *
 * See [the section on Automatic resources](class.Application.html#automatic-resources)
 * for more information.
 *
 * Returns: (nullable) (transfer none): Gets the menu with the
 *   given id from the automatically loaded resources
 */
GMenu *
gtk_application_get_menu_by_id (GtkApplication *application,
                                const char     *id)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);
  GObject *object;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  if (!priv->menus_builder)
    return NULL;

  object = gtk_builder_get_object (priv->menus_builder, id);

  if (!object || !G_IS_MENU (object))
    return NULL;

  return G_MENU (object);
}

void
gtk_application_set_screensaver_active (GtkApplication *application,
                                        gboolean        active)
{
  GtkApplicationPrivate *priv = gtk_application_get_instance_private (application);

  if (priv->screensaver_active != active)
    {
      priv->screensaver_active = active;
      g_object_notify (G_OBJECT (application), "screensaver-active");
    }
}
