/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 Red Hat
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
 *
 */

#include "config.h"

#include "gdktoplevellayout.h"

#include "gdkmonitor.h"

/**
 * GdkToplevelLayout:
 *
 * The `GdkToplevelLayout` struct contains information that
 * is necessary to present a sovereign window on screen.
 *
 * The `GdkToplevelLayout` struct is necessary for using
 * [method@Gdk.Toplevel.present].
 *
 * Toplevel surfaces are sovereign windows that can be presented
 * to the user in various states (maximized, on all workspaces,
 * etc).
 */
struct _GdkToplevelLayout
{
  /* < private >*/
  grefcount ref_count;

  guint resizable  : 1;

  guint maximized_valid : 1;
  guint maximized  : 1;
  guint fullscreen_valid : 1;
  guint fullscreen : 1;
  GdkMonitor *fullscreen_monitor;
};

G_DEFINE_BOXED_TYPE (GdkToplevelLayout, gdk_toplevel_layout,
                     gdk_toplevel_layout_ref,
                     gdk_toplevel_layout_unref)

/**
 * gdk_toplevel_layout_new: (constructor)
 *
 * Create a toplevel layout description.
 *
 * Used together with gdk_toplevel_present() to describe
 * how a toplevel surface should be placed and behave on-screen.
 *
 * The size is in ”application pixels”, not
 * ”device pixels” (see gdk_surface_get_scale_factor()).
 *
 * Returns: (transfer full): newly created instance of `GdkToplevelLayout`
 */
GdkToplevelLayout *
gdk_toplevel_layout_new (void)
{
  GdkToplevelLayout *layout;

  layout = g_new0 (GdkToplevelLayout, 1);
  g_ref_count_init (&layout->ref_count);
  layout->resizable = TRUE;
  layout->maximized_valid = FALSE;
  layout->maximized = FALSE;
  layout->fullscreen_valid = FALSE;
  layout->fullscreen = FALSE;
  layout->fullscreen_monitor = NULL;

  return layout;
}

/**
 * gdk_toplevel_layout_ref:
 * @layout: a `GdkToplevelLayout`
 *
 * Increases the reference count of @layout.
 *
 * Returns: the same @layout
 */
GdkToplevelLayout *
gdk_toplevel_layout_ref (GdkToplevelLayout *layout)
{
  g_ref_count_inc (&layout->ref_count);
  return layout;
}

/**
 * gdk_toplevel_layout_unref:
 * @layout: a `GdkToplevelLayout`
 *
 * Decreases the reference count of @layout.
 */
void
gdk_toplevel_layout_unref (GdkToplevelLayout *layout)
{
  if (g_ref_count_dec (&layout->ref_count))
    {
      g_clear_object (&layout->fullscreen_monitor);
      g_free (layout);
    }
}

/**
 * gdk_toplevel_layout_copy:
 * @layout: a `GdkToplevelLayout`
 *
 * Create a new `GdkToplevelLayout` and copy the contents of @layout into it.
 *
 * Returns: (transfer full): a copy of @layout.
 */
GdkToplevelLayout *
gdk_toplevel_layout_copy (GdkToplevelLayout *layout)
{
  GdkToplevelLayout *new_layout;

  new_layout = g_new0 (GdkToplevelLayout, 1);
  g_ref_count_init (&new_layout->ref_count);

  new_layout->resizable = layout->resizable;
  new_layout->maximized_valid = layout->maximized_valid;
  new_layout->maximized = layout->maximized;
  new_layout->fullscreen_valid = layout->fullscreen_valid;
  new_layout->fullscreen = layout->fullscreen;
  if (layout->fullscreen_monitor)
    new_layout->fullscreen_monitor = g_object_ref (layout->fullscreen_monitor);

  return new_layout;
}

/**
 * gdk_toplevel_layout_equal:
 * @layout: a `GdkToplevelLayout`
 * @other: another `GdkToplevelLayout`
 *
 * Check whether @layout and @other has identical layout properties.
 *
 * Returns: %TRUE if @layout and @other have identical layout properties,
 *   otherwise %FALSE.
 */
gboolean
gdk_toplevel_layout_equal (GdkToplevelLayout *layout,
                           GdkToplevelLayout *other)
{
  g_return_val_if_fail (layout, FALSE);
  g_return_val_if_fail (other, FALSE);

  return layout->resizable == other->resizable &&
         layout->maximized_valid == other->maximized_valid &&
         layout->maximized == other->maximized &&
         layout->fullscreen_valid == other->fullscreen_valid &&
         layout->fullscreen == other->fullscreen &&
         layout->fullscreen_monitor == other->fullscreen_monitor;
}

/**
 * gdk_toplevel_layout_set_resizable:
 * @layout: a `GdkToplevelLayout`
 * @resizable: %TRUE to allow resizing
 *
 * Sets whether the layout should allow the user
 * to resize the surface after it has been presented.
 */
void
gdk_toplevel_layout_set_resizable (GdkToplevelLayout *layout,
                                   gboolean           resizable)
{
  layout->resizable = resizable;
}

/**
 * gdk_toplevel_layout_get_resizable:
 * @layout: a `GdkToplevelLayout`
 *
 * Returns whether the layout should allow the user
 * to resize the surface.
 *
 * Returns: %TRUE if the layout is resizable
 */
gboolean
gdk_toplevel_layout_get_resizable (GdkToplevelLayout *layout)
{
  return layout->resizable;
}

/**
 * gdk_toplevel_layout_set_maximized:
 * @layout: a `GdkToplevelLayout`
 * @maximized: %TRUE to maximize
 *
 * Sets whether the layout should cause the surface
 * to be maximized when presented.
 */
void
gdk_toplevel_layout_set_maximized (GdkToplevelLayout *layout,
                                   gboolean           maximized)
{
  layout->maximized_valid = TRUE;
  layout->maximized = maximized;
}

/**
 * gdk_toplevel_layout_get_maximized:
 * @layout: a `GdkToplevelLayout`
 * @maximized: (out): set to %TRUE if the toplevel should be maximized
 *
 * If the layout specifies whether to the toplevel should go maximized,
 * the value pointed to by @maximized is set to %TRUE if it should go
 * fullscreen, or %FALSE, if it should go unmaximized.
 *
 * Returns: whether the @layout specifies the maximized state for the toplevel
 */
gboolean
gdk_toplevel_layout_get_maximized (GdkToplevelLayout *layout,
                                   gboolean          *maximized)
{
  if (layout->maximized_valid)
    {
      *maximized = layout->maximized;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_toplevel_layout_set_fullscreen:
 * @layout: a `GdkToplevelLayout`
 * @fullscreen: %TRUE to fullscreen the surface
 * @monitor: (nullable): the monitor to fullscreen on
 *
 * Sets whether the layout should cause the surface
 * to be fullscreen when presented.
 */
void
gdk_toplevel_layout_set_fullscreen (GdkToplevelLayout *layout,
                                    gboolean           fullscreen,
                                    GdkMonitor        *monitor)
{
  layout->fullscreen_valid = TRUE;
  layout->fullscreen = fullscreen;
  if (monitor)
    layout->fullscreen_monitor = g_object_ref (monitor);
}

/**
 * gdk_toplevel_layout_get_fullscreen:
 * @layout: a ``GdkToplevelLayout`
 * @fullscreen: (out): location to store whether the toplevel should be fullscreen
 *
 * If the layout specifies whether to the toplevel should go fullscreen,
 * the value pointed to by @fullscreen is set to %TRUE if it should go
 * fullscreen, or %FALSE, if it should go unfullscreen.
 *
 * Returns: whether the @layout specifies the fullscreen state for the toplevel
 */
gboolean
gdk_toplevel_layout_get_fullscreen (GdkToplevelLayout *layout,
                                    gboolean          *fullscreen)
{
  if (layout->fullscreen_valid)
    {
      *fullscreen = layout->fullscreen;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_toplevel_layout_get_fullscreen_monitor:
 * @layout: a `GdkToplevelLayout`
 *
 * Returns the monitor that the layout is fullscreening
 * the surface on.
 *
 * Returns: (nullable) (transfer none): the monitor on which @layout fullscreens
 */
GdkMonitor *
gdk_toplevel_layout_get_fullscreen_monitor (GdkToplevelLayout *layout)
{
  return layout->fullscreen_monitor;
}
