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

#include "gdkprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkpopupprivate.h"

/**
 * GdkPopup:
 *
 * A `GdkPopup` is a surface that is attached to another surface.
 *
 * The `GdkPopup` is positioned relative to its parent surface.
 *
 * `GdkPopup`s are typically used to implement menus and similar popups.
 * They can be modal, which is indicated by the [property@Gdk.Popup:autohide]
 * property.
 */

G_DEFINE_INTERFACE (GdkPopup, gdk_popup, GDK_TYPE_SURFACE)

static gboolean
gdk_popup_default_present (GdkPopup       *popup,
                           int             width,
                           int             height,
                           GdkPopupLayout *layout)
{
  return FALSE;
}

static GdkGravity
gdk_popup_default_get_surface_anchor (GdkPopup *popup)
{
  return GDK_GRAVITY_STATIC;
}

static GdkGravity
gdk_popup_default_get_rect_anchor (GdkPopup *popup)
{
  return GDK_GRAVITY_STATIC;
}

static int
gdk_popup_default_get_position_x (GdkPopup *popup)
{
  return 0;
}

static int
gdk_popup_default_get_position_y (GdkPopup *popup)
{
  return 0;
}

static void
gdk_popup_default_init (GdkPopupInterface *iface)
{
  iface->present = gdk_popup_default_present;
  iface->get_surface_anchor = gdk_popup_default_get_surface_anchor;
  iface->get_rect_anchor = gdk_popup_default_get_rect_anchor;
  iface->get_position_x = gdk_popup_default_get_position_x;
  iface->get_position_y = gdk_popup_default_get_position_y;

  /**
   * GdkPopup:parent: (attributes org.gtk.Property.get=gdk_popup_get_parent)
   *
   * The parent surface.
   */
  g_object_interface_install_property (iface,
      g_param_spec_object ("parent", NULL, NULL,
                           GDK_TYPE_SURFACE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * GdkPopup:autohide: (attributes org.gtk.Property.get=gdk_popup_get_autohide)
   *
   * Whether to hide on outside clicks.
   */
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("autohide", NULL, NULL,
                           FALSE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

/**
 * gdk_popup_present:
 * @popup: the `GdkPopup` to show
 * @width: the unconstrained popup width to layout
 * @height: the unconstrained popup height to layout
 * @layout: the `GdkPopupLayout` object used to layout
 *
 * Present @popup after having processed the `GdkPopupLayout` rules.
 *
 * If the popup was previously now showing, it will be showed,
 * otherwise it will change position according to @layout.
 *
 * After calling this function, the result should be handled in response
 * to the [signal@Gdk.Surface::layout] signal being emitted. The resulting
 * popup position can be queried using [method@Gdk.Popup.get_position_x],
 * [method@Gdk.Popup.get_position_y], and the resulting size will be sent as
 * parameters in the layout signal. Use [method@Gdk.Popup.get_rect_anchor]
 * and [method@Gdk.Popup.get_surface_anchor] to get the resulting anchors.
 *
 * Presenting may fail, for example if the @popup is set to autohide
 * and is immediately hidden upon being presented. If presenting failed,
 * the [signal@Gdk.Surface::layout] signal will not me emitted.
 *
 * Returns: %FALSE if it failed to be presented, otherwise %TRUE.
 */
gboolean
gdk_popup_present (GdkPopup       *popup,
                   int             width,
                   int             height,
                   GdkPopupLayout *layout)
{
  g_return_val_if_fail (GDK_IS_POPUP (popup), FALSE);
  g_return_val_if_fail (width > 0, FALSE);
  g_return_val_if_fail (height > 0, FALSE);
  g_return_val_if_fail (layout != NULL, FALSE);

  return GDK_POPUP_GET_IFACE (popup)->present (popup, width, height, layout);
}

/**
 * gdk_popup_get_surface_anchor:
 * @popup: a `GdkPopup`
 *
 * Gets the current popup surface anchor.
 *
 * The value returned may change after calling [method@Gdk.Popup.present],
 * or after the [signal@Gdk.Surface::layout] signal is emitted.
 *
 * Returns: the current surface anchor value of @popup
 */
GdkGravity
gdk_popup_get_surface_anchor (GdkPopup *popup)
{
  g_return_val_if_fail (GDK_IS_POPUP (popup), GDK_GRAVITY_STATIC);

  return GDK_POPUP_GET_IFACE (popup)->get_surface_anchor (popup);
}

/**
 * gdk_popup_get_rect_anchor:
 * @popup: a `GdkPopup`
 *
 * Gets the current popup rectangle anchor.
 *
 * The value returned may change after calling [method@Gdk.Popup.present],
 * or after the [signal@Gdk.Surface::layout] signal is emitted.
 *
 * Returns: the current rectangle anchor value of @popup
 */
GdkGravity
gdk_popup_get_rect_anchor (GdkPopup *popup)
{
  g_return_val_if_fail (GDK_IS_POPUP (popup), GDK_GRAVITY_STATIC);

  return GDK_POPUP_GET_IFACE (popup)->get_rect_anchor (popup);
}

/**
 * gdk_popup_get_parent: (attributes org.gtk.Method.get_property=parent)
 * @popup: a `GdkPopup`
 *
 * Returns the parent surface of a popup.
 *
 * Returns: (transfer none) (nullable): the parent surface
 */
GdkSurface *
gdk_popup_get_parent (GdkPopup *popup)
{
  GdkSurface *surface;

  g_return_val_if_fail (GDK_IS_POPUP (popup), NULL);

  g_object_get (popup, "parent", &surface, NULL);

  if (surface)
    g_object_unref (surface);

  return surface;
}

/**
 * gdk_popup_get_position_x:
 * @popup: a `GdkPopup`
 *
 * Obtains the position of the popup relative to its parent.
 *
 * Returns: the X coordinate of @popup position
 */
int
gdk_popup_get_position_x (GdkPopup *popup)
{
  g_return_val_if_fail (GDK_IS_POPUP (popup), 0);

  return GDK_POPUP_GET_IFACE (popup)->get_position_x (popup);
}

/**
 * gdk_popup_get_position_y:
 * @popup: a `GdkPopup`
 *
 * Obtains the position of the popup relative to its parent.
 *
 * Returns: the Y coordinate of @popup position
 */
int
gdk_popup_get_position_y (GdkPopup *popup)
{
  g_return_val_if_fail (GDK_IS_POPUP (popup), 0);

  return GDK_POPUP_GET_IFACE (popup)->get_position_y (popup);
}

/**
 * gdk_popup_get_autohide: (attributes org.gtk.Method.get_property=autohide)
 * @popup: a `GdkPopup`
 *
 * Returns whether this popup is set to hide on outside clicks.
 *
 * Returns: %TRUE if @popup will autohide
 */
gboolean
gdk_popup_get_autohide (GdkPopup *popup)
{
  gboolean autohide;

  g_return_val_if_fail (GDK_IS_POPUP (popup), FALSE);

  g_object_get (popup, "autohide", &autohide, NULL);

  return autohide;
}

guint
gdk_popup_install_properties (GObjectClass *object_class,
                              guint         first_prop)
{
  /**
   * GdkToplevel:parent:
   *
   * The parent surface of the toplevel.
   */
  g_object_class_override_property (object_class, first_prop + GDK_POPUP_PROP_PARENT, "parent");

  /**
   * GdkToplevel:autohide:
   *
   * Whether the toplevel should be modal with respect to its parent.
   */
  g_object_class_override_property (object_class, first_prop + GDK_POPUP_PROP_AUTOHIDE, "autohide");

  return GDK_POPUP_NUM_PROPERTIES;
}
