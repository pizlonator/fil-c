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

#include "gdkpopuplayout.h"

#include "gdkrectangle.h"
#include "gdksurface.h"

/**
 * GdkPopupLayout:
 *
 * The `GdkPopupLayout` struct contains information that is
 * necessary position a [iface@Gdk.Popup] relative to its parent.
 *
 * The positioning requires a negotiation with the windowing system,
 * since it depends on external constraints, such as the position of
 * the parent surface, and the screen dimensions.
 *
 * The basic ingredients are a rectangle on the parent surface,
 * and the anchor on both that rectangle and the popup. The anchors
 * specify a side or corner to place next to each other.
 *
 * ![Popup anchors](popup-anchors.png)
 *
 * For cases where placing the anchors next to each other would make
 * the popup extend offscreen, the layout includes some hints for how
 * to resolve this problem. The hints may suggest to flip the anchor
 * position to the other side, or to 'slide' the popup along a side,
 * or to resize it.
 *
 * ![Flipping popups](popup-flip.png)
 *
 * ![Sliding popups](popup-slide.png)
 *
 * These hints may be combined.
 *
 * Ultimatively, it is up to the windowing system to determine the position
 * and size of the popup. You can learn about the result by calling
 * [method@Gdk.Popup.get_position_x], [method@Gdk.Popup.get_position_y],
 * [method@Gdk.Popup.get_rect_anchor] and [method@Gdk.Popup.get_surface_anchor]
 * after the popup has been presented. This can be used to adjust the rendering.
 * For example, [GtkPopover](../gtk4/class.Popover.html) changes its arrow position
 * accordingly. But you have to be careful avoid changing the size of the popover,
 * or it has to be presented again.
 */

struct _GdkPopupLayout
{
  /* < private >*/
  grefcount ref_count;

  GdkRectangle anchor_rect;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;
  int dx;
  int dy;
  int shadow_left;
  int shadow_right;
  int shadow_top;
  int shadow_bottom;
};

G_DEFINE_BOXED_TYPE (GdkPopupLayout, gdk_popup_layout,
                     gdk_popup_layout_ref,
                     gdk_popup_layout_unref)

/**
 * gdk_popup_layout_new: (constructor)
 * @anchor_rect:  (not nullable): the anchor `GdkRectangle` to align @surface with
 * @rect_anchor: the point on @anchor_rect to align with @surface's anchor point
 * @surface_anchor: the point on @surface to align with @rect's anchor point
 *
 * Create a popup layout description.
 *
 * Used together with [method@Gdk.Popup.present] to describe how a popup
 * surface should be placed and behave on-screen.
 *
 * @anchor_rect is relative to the top-left corner of the surface's parent.
 * @rect_anchor and @surface_anchor determine anchor points on @anchor_rect
 * and surface to pin together.
 *
 * The position of @anchor_rect's anchor point can optionally be offset using
 * [method@Gdk.PopupLayout.set_offset], which is equivalent to offsetting the
 * position of surface.
 *
 * Returns: (transfer full): newly created instance of `GdkPopupLayout`
 */
GdkPopupLayout *
gdk_popup_layout_new (const GdkRectangle *anchor_rect,
                      GdkGravity          rect_anchor,
                      GdkGravity          surface_anchor)
{
  GdkPopupLayout *layout;

  layout = g_new0 (GdkPopupLayout, 1);
  g_ref_count_init (&layout->ref_count);
  layout->anchor_rect = *anchor_rect;
  layout->rect_anchor = rect_anchor;
  layout->surface_anchor = surface_anchor;

  return layout;
}

/**
 * gdk_popup_layout_ref:
 * @layout: a `GdkPopupLayout`
 *
 * Increases the reference count of @value.
 *
 * Returns: the same @layout
 */
GdkPopupLayout *
gdk_popup_layout_ref (GdkPopupLayout *layout)
{
  g_ref_count_inc (&layout->ref_count);
  return layout;
}

/**
 * gdk_popup_layout_unref:
 * @layout: a `GdkPopupLayout`
 *
 * Decreases the reference count of @value.
 */
void
gdk_popup_layout_unref (GdkPopupLayout *layout)
{
  if (g_ref_count_dec (&layout->ref_count))
    g_free (layout);
}

/**
 * gdk_popup_layout_copy:
 * @layout: a `GdkPopupLayout`
 *
 * Makes a copy of @layout.
 *
 * Returns: (transfer full): a copy of @layout.
 */
GdkPopupLayout *
gdk_popup_layout_copy (GdkPopupLayout *layout)
{
  GdkPopupLayout *new_layout;

  new_layout = g_new0 (GdkPopupLayout, 1);
  g_ref_count_init (&new_layout->ref_count);

  new_layout->anchor_rect = layout->anchor_rect;
  new_layout->rect_anchor = layout->rect_anchor;
  new_layout->surface_anchor = layout->surface_anchor;
  new_layout->anchor_hints = layout->anchor_hints;
  new_layout->dx = layout->dx;
  new_layout->dy = layout->dy;
  new_layout->shadow_left = layout->shadow_left;
  new_layout->shadow_right = layout->shadow_right;
  new_layout->shadow_top = layout->shadow_top;
  new_layout->shadow_bottom = layout->shadow_bottom;

  return new_layout;
}

/**
 * gdk_popup_layout_equal:
 * @layout: a `GdkPopupLayout`
 * @other: another `GdkPopupLayout`
 *
 * Check whether @layout and @other has identical layout properties.
 *
 * Returns: %TRUE if @layout and @other have identical layout properties,
 *   otherwise %FALSE.
 */
gboolean
gdk_popup_layout_equal (GdkPopupLayout *layout,
                        GdkPopupLayout *other)
{
  g_return_val_if_fail (layout, FALSE);
  g_return_val_if_fail (other, FALSE);

  return (gdk_rectangle_equal (&layout->anchor_rect, &other->anchor_rect) &&
          layout->rect_anchor == other->rect_anchor &&
          layout->surface_anchor == other->surface_anchor &&
          layout->anchor_hints == other->anchor_hints &&
          layout->dx == other->dx &&
          layout->dy == other->dy &&
          layout->shadow_left == other->shadow_left &&
          layout->shadow_right == other->shadow_right &&
          layout->shadow_top == other->shadow_top &&
          layout->shadow_bottom == other->shadow_bottom);
}

/**
 * gdk_popup_layout_set_anchor_rect:
 * @layout: a `GdkPopupLayout`
 * @anchor_rect: the new anchor rectangle
 *
 * Set the anchor rectangle.
 */
void
gdk_popup_layout_set_anchor_rect (GdkPopupLayout     *layout,
                                  const GdkRectangle *anchor_rect)
{
  layout->anchor_rect = *anchor_rect;
}

/**
 * gdk_popup_layout_get_anchor_rect:
 * @layout: a `GdkPopupLayout`
 *
 * Get the anchor rectangle.
 *
 * Returns: The anchor rectangle
 */
const GdkRectangle *
gdk_popup_layout_get_anchor_rect (GdkPopupLayout *layout)
{
  return &layout->anchor_rect;
}

/**
 * gdk_popup_layout_set_rect_anchor:
 * @layout: a `GdkPopupLayout`
 * @anchor: the new rect anchor
 *
 * Set the anchor on the anchor rectangle.
 */
void
gdk_popup_layout_set_rect_anchor (GdkPopupLayout *layout,
                                  GdkGravity      anchor)
{
  layout->rect_anchor = anchor;
}

/**
 * gdk_popup_layout_get_rect_anchor:
 * @layout: a `GdkPopupLayout`
 *
 * Returns the anchor position on the anchor rectangle.
 *
 * Returns: the anchor on the anchor rectangle.
 */
GdkGravity
gdk_popup_layout_get_rect_anchor (GdkPopupLayout *layout)
{
  return layout->rect_anchor;
}

/**
 * gdk_popup_layout_set_surface_anchor:
 * @layout: a `GdkPopupLayout`
 * @anchor: the new popup surface anchor
 *
 * Set the anchor on the popup surface.
 */
void
gdk_popup_layout_set_surface_anchor (GdkPopupLayout *layout,
                                     GdkGravity      anchor)
{
  layout->surface_anchor = anchor;
}

/**
 * gdk_popup_layout_get_surface_anchor:
 * @layout: a `GdkPopupLayout`
 *
 * Returns the anchor position on the popup surface.
 *
 * Returns: the anchor on the popup surface.
 */
GdkGravity
gdk_popup_layout_get_surface_anchor (GdkPopupLayout *layout)
{
  return layout->surface_anchor;
}

/**
 * gdk_popup_layout_set_anchor_hints:
 * @layout: a `GdkPopupLayout`
 * @anchor_hints: the new `GdkAnchorHints`
 *
 * Set new anchor hints.
 *
 * The set @anchor_hints determines how @surface will be moved
 * if the anchor points cause it to move off-screen. For example,
 * %GDK_ANCHOR_FLIP_X will replace %GDK_GRAVITY_NORTH_WEST with
 * %GDK_GRAVITY_NORTH_EAST and vice versa if @surface extends
 * beyond the left or right edges of the monitor.
 */
void
gdk_popup_layout_set_anchor_hints (GdkPopupLayout *layout,
                                   GdkAnchorHints  anchor_hints)
{
  layout->anchor_hints = anchor_hints;
}

/**
 * gdk_popup_layout_get_anchor_hints:
 * @layout: a `GdkPopupLayout`
 *
 * Get the `GdkAnchorHints`.
 *
 * Returns: the `GdkAnchorHints`
 */
GdkAnchorHints
gdk_popup_layout_get_anchor_hints (GdkPopupLayout *layout)
{
  return layout->anchor_hints;
}

/**
 * gdk_popup_layout_set_offset:
 * @layout: a `GdkPopupLayout`
 * @dx: x delta to offset the anchor rectangle with
 * @dy: y delta to offset the anchor rectangle with
 *
 * Offset the position of the anchor rectangle with the given delta.
 */
void
gdk_popup_layout_set_offset (GdkPopupLayout *layout,
                             int             dx,
                             int             dy)
{
  layout->dx = dx;
  layout->dy = dy;
}

/**
 * gdk_popup_layout_get_offset:
 * @layout: a `GdkPopupLayout`
 * @dx: (out): return location for the delta X coordinate
 * @dy: (out): return location for the delta Y coordinate
 *
 * Retrieves the offset for the anchor rectangle.
 */
void
gdk_popup_layout_get_offset (GdkPopupLayout *layout,
                             int            *dx,
                             int            *dy)
{
  if (dx)
    *dx = layout->dx;
  if (dy)
    *dy = layout->dy;
}

/**
 * gdk_popup_layout_set_shadow_width:
 * @layout: a `GdkPopupLayout`
 * @left: width of the left part of the shadow
 * @right: width of the right part of the shadow
 * @top: height of the top part of the shadow
 * @bottom: height of the bottom part of the shadow
 *
 * Sets the shadow width of the popup.
 *
 * The shadow width corresponds to the part of the computed
 * surface size that would consist of the shadow margin
 * surrounding the window, would there be any.
 *
 * Since: 4.2
 */
void
gdk_popup_layout_set_shadow_width (GdkPopupLayout *layout,
                                   int             left,
                                   int             right,
                                   int             top,
                                   int             bottom)
{
  layout->shadow_left = left;
  layout->shadow_right = right;
  layout->shadow_top = top;
  layout->shadow_bottom = bottom;
}

/**
 * gdk_popup_layout_get_shadow_width:
 * @layout: a `GdkPopupLayout`
 * @left: (out): return location for the left shadow width
 * @right: (out): return location for the right shadow width
 * @top: (out): return location for the top shadow width
 * @bottom: (out): return location for the bottom shadow width
 *
 * Obtains the shadow widths of this layout.
 *
 * Since: 4.2
 */
void
gdk_popup_layout_get_shadow_width (GdkPopupLayout *layout,
                                   int            *left,
                                   int            *right,
                                   int            *top,
                                   int            *bottom)
{
  if (left)
    *left = layout->shadow_left;
  if (right)
    *right = layout->shadow_right;
  if (top)
    *top = layout->shadow_top;
  if (bottom)
    *bottom = layout->shadow_bottom;
}
