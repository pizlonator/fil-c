/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gdktoplevelprivate.h"

#include "gdkdisplay.h"
#include "gdkenumtypes.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate.h"

#include <graphene-gobject.h>
#include <math.h>

/**
 * GdkToplevel:
 *
 * A `GdkToplevel` is a freestanding toplevel surface.
 *
 * The `GdkToplevel` interface provides useful APIs for interacting with
 * the windowing system, such as controlling maximization and size of the
 * surface, setting icons and transient parents for dialogs.
 */

G_DEFINE_INTERFACE (GdkToplevel, gdk_toplevel, GDK_TYPE_SURFACE)

enum
{
  COMPUTE_SIZE,

  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

static void
gdk_toplevel_default_present (GdkToplevel       *toplevel,
                              GdkToplevelLayout *layout)
{
}

static gboolean
gdk_toplevel_default_minimize (GdkToplevel *toplevel)
{
  return FALSE;
}

static gboolean
gdk_toplevel_default_lower (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
gdk_toplevel_default_focus (GdkToplevel *toplevel,
                            guint32      timestamp)
{
}

static gboolean
gdk_toplevel_default_show_window_menu (GdkToplevel *toplevel,
                                       GdkEvent    *event)
{
  return FALSE;
}

static gboolean
gdk_toplevel_default_titlebar_gesture (GdkToplevel        *toplevel,
                                       GdkTitlebarGesture  gesture)
{
  return FALSE;
}

static gboolean
gdk_toplevel_default_supports_edge_constraints (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
gdk_toplevel_default_inhibit_system_shortcuts (GdkToplevel *toplevel,
                                               GdkEvent    *event)
{
}

static void
gdk_toplevel_default_restore_system_shortcuts (GdkToplevel *toplevel)
{
}

void
gdk_toplevel_notify_compute_size (GdkToplevel     *toplevel,
                                  GdkToplevelSize *size)
{
  g_signal_emit (toplevel, signals[COMPUTE_SIZE], 0, size);
  gdk_toplevel_size_validate (size);
}

static void
gdk_toplevel_default_export_handle (GdkToplevel          *toplevel,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data)
{
  GTask *task;

  task = g_task_new (toplevel, cancellable, callback, user_data);
  g_task_return_pointer (task, NULL, NULL);
  g_object_unref (task);
}

static char *
gdk_toplevel_default_export_handle_finish (GdkToplevel   *toplevel,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gdk_toplevel_default_unexport_handle (GdkToplevel *toplevel,
                                      const char  *handle)
{
}

static void
gdk_toplevel_default_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_toplevel_default_present;
  iface->minimize = gdk_toplevel_default_minimize;
  iface->lower = gdk_toplevel_default_lower;
  iface->focus = gdk_toplevel_default_focus;
  iface->show_window_menu = gdk_toplevel_default_show_window_menu;
  iface->supports_edge_constraints = gdk_toplevel_default_supports_edge_constraints;
  iface->inhibit_system_shortcuts = gdk_toplevel_default_inhibit_system_shortcuts;
  iface->restore_system_shortcuts = gdk_toplevel_default_restore_system_shortcuts;
  iface->titlebar_gesture = gdk_toplevel_default_titlebar_gesture;
  iface->export_handle = gdk_toplevel_default_export_handle;
  iface->export_handle_finish = gdk_toplevel_default_export_handle_finish;
  iface->unexport_handle = gdk_toplevel_default_unexport_handle;

  /**
   * GdkToplevel:state: (attributes org.gtk.Property.get=gdk_toplevel_get_state)
   *
   * The state of the toplevel.
   */
  g_object_interface_install_property (iface,
      g_param_spec_flags ("state", NULL, NULL,
                          GDK_TYPE_TOPLEVEL_STATE, 0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GdkToplevel:title: (attributes org.gtk.Property.set=gdk_toplevel_set_title)
   *
   * The title of the surface.
   */
  g_object_interface_install_property (iface,
      g_param_spec_string ("title", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel:startup-id: (attributes org.gtk.Property.set=gdk_toplevel_set_startup_id)
   *
   * The startup ID of the surface.
   *
   * See [class@Gdk.AppLaunchContext] for more information about
   * startup feedback.
   */
  g_object_interface_install_property (iface,
      g_param_spec_string ("startup-id", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel:transient-for: (attributes org.gtk.Property.set=gdk_toplevel_set_transient_for)
   *
   * The transient parent of the surface.
   */
  g_object_interface_install_property (iface,
      g_param_spec_object ("transient-for", NULL, NULL,
                           GDK_TYPE_SURFACE,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel:modal: (attributes org.gtk.Property.set=gdk_toplevel_set_modal)
   *
   * Whether the surface is modal.
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("modal", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel:icon-list: (attributes org.gtk.Property.set=gdk_toplevel_set_icon_list)
   *
   * A list of textures to use as icon.
   */
  g_object_interface_install_property (iface,
      g_param_spec_pointer ("icon-list", NULL, NULL,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel:decorated: (attributes org.gtk.Property.set=gdk_toplevel_set_decorated)
   *
   * Whether the window manager should add decorations.
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("decorated", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel:deletable: (attributes org.gtk.Property.set=gdk_toplevel_set_deletable)
   *
   * Whether the window manager should allow to close the surface.
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("deletable", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel:fullscreen-mode:
   *
   * The fullscreen mode of the surface.
   */
  g_object_interface_install_property (iface,
      g_param_spec_enum ("fullscreen-mode", NULL, NULL,
                         GDK_TYPE_FULLSCREEN_MODE,
                         GDK_FULLSCREEN_ON_CURRENT_MONITOR,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel:shortcuts-inhibited:
   *
   * Whether the surface should inhibit keyboard shortcuts.
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("shortcuts-inhibited", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GdkToplevel::compute-size:
   * @toplevel: a `GdkToplevel`
   * @size: (type Gdk.ToplevelSize): a `GdkToplevelSize`
   *
   * Emitted when the size for the surface needs to be computed, when
   * it is present.
   *
   * This signal will normally be emitted during or after a call to
   * [method@Gdk.Toplevel.present], depending on the configuration
   * received by the windowing system. It may also be emitted at any
   * other point in time, in response to the windowing system
   * spontaneously changing the configuration of the toplevel surface.
   *
   * It is the responsibility of the toplevel user to handle this signal
   * and compute the desired size of the toplevel, given the information
   * passed via the [struct@Gdk.ToplevelSize] object. Failing to do so
   * will result in an arbitrary size being used as a result.
   */
  signals[COMPUTE_SIZE] =
    g_signal_new (I_("compute-size"),
                  GDK_TYPE_TOPLEVEL,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_TOPLEVEL_SIZE | G_SIGNAL_TYPE_STATIC_SCOPE);
}

guint
gdk_toplevel_install_properties (GObjectClass *object_class,
                                 guint         first_prop)
{
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_STATE, "state");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_TITLE, "title");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_STARTUP_ID, "startup-id");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_TRANSIENT_FOR, "transient-for");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_MODAL, "modal");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_ICON_LIST, "icon-list");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_DECORATED, "decorated");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_DELETABLE, "deletable");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE, "fullscreen-mode");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED, "shortcuts-inhibited");

  return GDK_TOPLEVEL_NUM_PROPERTIES;
}

/**
 * gdk_toplevel_present:
 * @toplevel: the `GdkToplevel` to show
 * @layout: the `GdkToplevelLayout` object used to layout
 *
 * Present @toplevel after having processed the `GdkToplevelLayout` rules.
 *
 * If the toplevel was previously not showing, it will be showed,
 * otherwise it will change layout according to @layout.
 *
 * GDK may emit the [signal@Gdk.Toplevel::compute-size] signal to let
 * the user of this toplevel compute the preferred size of the toplevel
 * surface.
 *
 * Presenting is asynchronous and the specified layout parameters are not
 * guaranteed to be respected.
 */
void
gdk_toplevel_present (GdkToplevel       *toplevel,
                      GdkToplevelLayout *layout)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));
  g_return_if_fail (layout != NULL);

  GDK_TOPLEVEL_GET_IFACE (toplevel)->present (toplevel, layout);
}

/**
 * gdk_toplevel_minimize:
 * @toplevel: a `GdkToplevel`
 *
 * Asks to minimize the @toplevel.
 *
 * The windowing system may choose to ignore the request.
 *
 * Returns: %TRUE if the surface was minimized
 */
gboolean
gdk_toplevel_minimize (GdkToplevel *toplevel)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->minimize (toplevel);
}

/**
 * gdk_toplevel_lower:
 * @toplevel: a `GdkToplevel`
 *
 * Asks to lower the @toplevel below other windows.
 *
 * The windowing system may choose to ignore the request.
 *
 * Returns: %TRUE if the surface was lowered
 */
gboolean
gdk_toplevel_lower (GdkToplevel *toplevel)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->lower (toplevel);
}

/**
 * gdk_toplevel_focus:
 * @toplevel: a `GdkToplevel`
 * @timestamp: timestamp of the event triggering the surface focus
 *
 * Sets keyboard focus to @surface.
 *
 * In most cases, [gtk_window_present_with_time()](../gtk4/method.Window.present_with_time.html)
 * should be used on a [GtkWindow](../gtk4/class.Window.html), rather than
 * calling this function.
 */
void
gdk_toplevel_focus (GdkToplevel *toplevel,
                   guint32       timestamp)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  GDK_TOPLEVEL_GET_IFACE (toplevel)->focus (toplevel, timestamp);
}

/**
 * gdk_toplevel_get_state: (attributes org.gtk.Method.get_property=state)
 * @toplevel: a `GdkToplevel`
 *
 * Gets the bitwise or of the currently active surface state flags,
 * from the `GdkToplevelState` enumeration.
 *
 * Returns: surface state bitfield
 */
GdkToplevelState
gdk_toplevel_get_state (GdkToplevel *toplevel)
{
  GdkToplevelState state;

  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), 0);

  g_object_get (toplevel, "state", &state, NULL);

  return state;
}

/**
 * gdk_toplevel_set_title: (attributes org.gtk.Method.set_property=title)
 * @toplevel: a `GdkToplevel`
 * @title: title of @surface
 *
 * Sets the title of a toplevel surface.
 *
 * The title maybe be displayed in the titlebar,
 * in lists of windows, etc.
 */
void
gdk_toplevel_set_title (GdkToplevel *toplevel,
                        const char  *title)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "title", title, NULL);
}

/**
 * gdk_toplevel_set_startup_id: (attributes org.gtk.Method.set_property=startup-id)
 * @toplevel: a `GdkToplevel`
 * @startup_id: a string with startup-notification identifier
 *
 * Sets the startup notification ID.
 *
 * When using GTK, typically you should use
 * [gtk_window_set_startup_id()](../gtk4/method.Window.set_startup_id.html)
 * instead of this low-level function.
 */
void
gdk_toplevel_set_startup_id (GdkToplevel *toplevel,
                             const char  *startup_id)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "startup-id", startup_id, NULL);
}

/**
 * gdk_toplevel_set_transient_for: (attributes org.gtk.Method.set_property=transient-for)
 * @toplevel: a `GdkToplevel`
 * @parent: another toplevel `GdkSurface`
 *
 * Sets a transient-for parent.
 *
 * Indicates to the window manager that @surface is a transient
 * dialog associated with the application surface @parent. This
 * allows the window manager to do things like center @surface
 * on @parent and keep @surface above @parent.
 *
 * See [gtk_window_set_transient_for()](../gtk4/method.Window.set_transient_for.html)
 * if you’re using [GtkWindow](../gtk4/class.Window.html).
 */
void
gdk_toplevel_set_transient_for (GdkToplevel *toplevel,
                                GdkSurface  *parent)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "transient-for", parent, NULL);
}

/**
 * gdk_toplevel_set_modal: (attributes org.gtk.Method.set_property=modal)
 * @toplevel: a `GdkToplevel`
 * @modal: %TRUE if the surface is modal, %FALSE otherwise.
 *
 * Sets the toplevel to be modal.
 *
 * The application can use this hint to tell the
 * window manager that a certain surface has modal
 * behaviour. The window manager can use this information
 * to handle modal surfaces in a special way.
 *
 * You should only use this on surfaces for which you have
 * previously called [method@Gdk.Toplevel.set_transient_for].
 */
void
gdk_toplevel_set_modal (GdkToplevel *toplevel,
                        gboolean     modal)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "modal", modal, NULL);
}

/**
 * gdk_toplevel_set_icon_list: (attributes org.gtk.Method.set_property=icon-list)
 * @toplevel: a `GdkToplevel`
 * @surfaces: (transfer none) (element-type GdkTexture):
 *   A list of textures to use as icon, of different sizes
 *
 * Sets a list of icons for the surface.
 *
 * One of these will be used to represent the surface in iconic form.
 * The icon may be shown in window lists or task bars. Which icon
 * size is shown depends on the window manager. The window manager
 * can scale the icon but setting several size icons can give better
 * image quality.
 *
 * Note that some platforms don't support surface icons.
 */
void
gdk_toplevel_set_icon_list (GdkToplevel *toplevel,
                            GList       *surfaces)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "icon-list", surfaces, NULL);
}

/**
 * gdk_toplevel_show_window_menu:
 * @toplevel: a `GdkToplevel`
 * @event: a `GdkEvent` to show the menu for
 *
 * Asks the windowing system to show the window menu.
 *
 * The window menu is the menu shown when right-clicking the titlebar
 * on traditional windows managed by the window manager. This is useful
 * for windows using client-side decorations, activating it with a
 * right-click on the window decorations.
 *
 * Returns: %TRUE if the window menu was shown and %FALSE otherwise.
 */
gboolean
gdk_toplevel_show_window_menu (GdkToplevel *toplevel,
                               GdkEvent    *event)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->show_window_menu (toplevel, event);
}

/**
 * gdk_toplevel_set_decorated: (attributes org.gtk.Method.set_property=decorated)
 * @toplevel: a `GdkToplevel`
 * @decorated: %TRUE to request decorations
 *
 * Sets the toplevel to be decorated.
 *
 * Setting @decorated to %FALSE hints the desktop environment
 * that the surface has its own, client-side decorations and
 * does not need to have window decorations added.
 */
void
gdk_toplevel_set_decorated (GdkToplevel *toplevel,
                            gboolean     decorated)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "decorated", decorated, NULL);
}

/**
 * gdk_toplevel_set_deletable: (attributes org.gtk.Method.set_property=deletable)
 * @toplevel: a `GdkToplevel`
 * @deletable: %TRUE to request a delete button
 *
 * Sets the toplevel to be deletable.
 *
 * Setting @deletable to %TRUE hints the desktop environment
 * that it should offer the user a way to close the surface.
 */
void
gdk_toplevel_set_deletable (GdkToplevel *toplevel,
                            gboolean     deletable)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "deletable", deletable, NULL);
}

/**
 * gdk_toplevel_supports_edge_constraints:
 * @toplevel: a `GdkToplevel`
 *
 * Returns whether the desktop environment supports
 * tiled window states.
 *
 * Returns: %TRUE if the desktop environment supports tiled window states
 */
gboolean
gdk_toplevel_supports_edge_constraints (GdkToplevel *toplevel)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->supports_edge_constraints (toplevel);
}

/**
 * gdk_toplevel_inhibit_system_shortcuts:
 * @toplevel: a `GdkToplevel`
 * @event: (nullable): the `GdkEvent` that is triggering the inhibit
 *   request, or %NULL if none is available
 *
 * Requests that the @toplevel inhibit the system shortcuts.
 *
 * This is asking the desktop environment/windowing system to let all
 * keyboard events reach the surface, as long as it is focused, instead
 * of triggering system actions.
 *
 * If granted, the rerouting remains active until the default shortcuts
 * processing is restored with [method@Gdk.Toplevel.restore_system_shortcuts],
 * or the request is revoked by the desktop environment, windowing system
 * or the user.
 *
 * A typical use case for this API is remote desktop or virtual machine
 * viewers which need to inhibit the default system keyboard shortcuts
 * so that the remote session or virtual host gets those instead of the
 * local environment.
 *
 * The windowing system or desktop environment may ask the user to grant
 * or deny the request or even choose to ignore the request entirely.
 *
 * The caller can be notified whenever the request is granted or revoked
 * by listening to the [property@Gdk.Toplevel:shortcuts-inhibited] property.
 */
void
gdk_toplevel_inhibit_system_shortcuts (GdkToplevel *toplevel,
                                       GdkEvent    *event)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  GDK_TOPLEVEL_GET_IFACE (toplevel)->inhibit_system_shortcuts (toplevel,
                                                               event);
}

/**
 * gdk_toplevel_restore_system_shortcuts:
 * @toplevel: a `GdkToplevel`
 *
 * Restore default system keyboard shortcuts which were previously
 * inhibited.
 *
 * This undoes the effect of [method@Gdk.Toplevel.inhibit_system_shortcuts].
 */
void
gdk_toplevel_restore_system_shortcuts (GdkToplevel *toplevel)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  GDK_TOPLEVEL_GET_IFACE (toplevel)->restore_system_shortcuts (toplevel);
}

/**
 * gdk_toplevel_begin_resize:
 * @toplevel: a `GdkToplevel`
 * @edge: the edge or corner from which the drag is started
 * @device: (nullable): the device used for the operation
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @x: surface X coordinate of mouse click that began the drag
 * @y: surface Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag (use
 *   [method@Gdk.Event.get_time])
 *
 * Begins an interactive resize operation.
 *
 * You might use this function to implement a “window resize grip.”
 */
void
gdk_toplevel_begin_resize (GdkToplevel    *toplevel,
                           GdkSurfaceEdge  edge,
                           GdkDevice      *device,
                           int             button,
                           double          x,
                           double          y,
                           guint32         timestamp)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  if (device == NULL)
    {
      GdkSeat *seat = gdk_display_get_default_seat (gdk_surface_get_display (GDK_SURFACE (toplevel)));
      if (button == 0)
        device = gdk_seat_get_keyboard (seat);
      else
        device = gdk_seat_get_pointer (seat);
    }

  GDK_TOPLEVEL_GET_IFACE (toplevel)->begin_resize (toplevel,
                                                   edge,
                                                   device,
                                                   button,
                                                   x, y,
                                                   timestamp);
}

/**
 * gdk_toplevel_begin_move:
 * @toplevel: a `GdkToplevel`
 * @device: the device used for the operation
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @x: surface X coordinate of mouse click that began the drag
 * @y: surface Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag (use
 *   [method@Gdk.Event.get_time])
 *
 * Begins an interactive move operation.
 *
 * You might use this function to implement draggable titlebars.
 */
void
gdk_toplevel_begin_move (GdkToplevel *toplevel,
                         GdkDevice   *device,
                         int          button,
                         double       x,
                         double       y,
                         guint32      timestamp)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  if (device == NULL)
    {
      GdkSeat *seat = gdk_display_get_default_seat (gdk_surface_get_display (GDK_SURFACE (toplevel)));
      if (button == 0)
        device = gdk_seat_get_keyboard (seat);
      else
        device = gdk_seat_get_pointer (seat);
    }

  GDK_TOPLEVEL_GET_IFACE (toplevel)->begin_move (toplevel,
                                                 device,
                                                 button,
                                                 x, y,
                                                 timestamp);
}

/**
 * gdk_toplevel_titlebar_gesture:
 * @toplevel: a `GdkToplevel`
 * @gesture: a `GdkTitlebarGesture`
 *
 * Since: 4.4
 */
gboolean
gdk_toplevel_titlebar_gesture (GdkToplevel        *toplevel,
                               GdkTitlebarGesture  gesture)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->titlebar_gesture (toplevel,
                                                              gesture);
}

/*< private >
 * gdk_toplevel_export_handle:
 * @toplevel: a `GdkToplevel`
 * @cancellable: (nullable): a `GCancellable`
 * @callback: ithe callback to call when the handle has been exported
 * @user_data: (closure callback): data to pass to @callback
 *
 * This function asynchronously obtains a handle for a toplevel surface
 * that can be passed to other processes.
 *
 * When a handle has been obtained, @callback will be called, and can
 * receive the handle via [method@Gdk.Toplevel.export_handle_finish].
 *
 * It is an error to call this function on a surface that is already
 * exported.
 *
 * When the handle is no longer needed, [method@Gdk.Toplevel.unexport_handle]
 * should be called to clean up resources.
 *
 * Since: 4.10
 */
void
gdk_toplevel_export_handle (GdkToplevel         *toplevel,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GDK_TOPLEVEL_GET_IFACE (toplevel)->export_handle (toplevel, cancellable, callback, user_data);
}

/*< private >
 * gdk_toplevel_export_handle_finish:
 * @toplevel: a `GdkToplevel`
 * @result: the `GAsyncResult`
 * @error: return location for an error
 *
 * Finishes the [method@Gdk.Toplevel.export_handle] call and
 * returns the resulting handle.
 *
 * Returns: (nullable) (transfer full): the exported handle,
 *   or `NULL` and @error is set
 *
 * Since: 4.10
 */
char *
gdk_toplevel_export_handle_finish (GdkToplevel   *toplevel,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  return GDK_TOPLEVEL_GET_IFACE (toplevel)->export_handle_finish (toplevel, result, error);
}

/*< private >
 * gdk_toplevel_unexport_handle:
 * @toplevel: a `GdkToplevel`
 * @handle: the handle to unexport
 *
 * Destroys the handle that was obtained with [method@Gdk.Toplevel.export_handle].
 *
 * It is an error to call this function on a surface that
 * does not have a handle.
 *
 * Since: 4.10
 */
void
gdk_toplevel_unexport_handle (GdkToplevel *toplevel,
                              const char  *handle)
{
  GDK_TOPLEVEL_GET_IFACE (toplevel)->unexport_handle (toplevel, handle);
}
