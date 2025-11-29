/*
 * Copyright © 2019 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtklistbaseprivate.h"

#include "gtkadjustment.h"
#include "gtkbitset.h"
#include "gtkcssboxesprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkdragsourceprivate.h"
#include "gtkdropcontrollermotion.h"
#include "gtkgesturedrag.h"
#include "gtkgizmoprivate.h"
#include "gtklistitemwidgetprivate.h"
#include "gtkmultiselection.h"
#include "gtkorientable.h"
#include "gtkscrollable.h"
#include "gtkscrollinfoprivate.h"
#include "gtksingleselection.h"
#include "gtksnapshot.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

/* Allow shadows to overdraw without immediately culling the widget at the viewport
 * boundary.
 * Choose this so that roughly 1 extra widget gets drawn on each side of the viewport,
 * but not more. Icons are 16px, text height is somewhere there, too.
 */
#define GTK_LIST_BASE_CHILD_MAX_OVERDRAW 10

typedef struct _RubberbandData RubberbandData;

struct _RubberbandData
{
  GtkWidget *widget;                            /* The rubberband widget */

  GtkListItemTracker *start_tracker;            /* The item we started dragging on */
  double              start_align_across;       /* alignment in horizontal direction */
  double              start_align_along;        /* alignment in vertical direction */

  double pointer_x, pointer_y;                  /* mouse coordinates in widget space */
};

typedef struct _GtkListBasePrivate GtkListBasePrivate;

struct _GtkListBasePrivate
{
  GtkListItemManager *item_manager;
  GtkSelectionModel *model;
  GtkOrientation orientation;
  GtkAdjustment *adjustment[2];
  GtkScrollablePolicy scroll_policy[2];
  GtkListTabBehavior tab_behavior;

  GtkListItemTracker *anchor;
  double anchor_align_along;
  double anchor_align_across;
  GtkPackType anchor_side_along;
  GtkPackType anchor_side_across;
  guint center_widgets;
  guint above_below_widgets;
  /* the last item that was selected - basically the location to extend selections from */
  GtkListItemTracker *selected;
  /* the item that has input focus */
  GtkListItemTracker *focus;

  gboolean enable_rubberband;
  GtkGesture *drag_gesture;
  RubberbandData *rubberband;

  guint autoscroll_id;
  double autoscroll_delta_x;
  double autoscroll_delta_y;
};

enum
{
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_ORIENTATION,
  PROP_VADJUSTMENT,
  PROP_VSCROLL_POLICY,

  N_PROPS
};

/* HACK: We want the g_class argument in our instance init func and G_DEFINE_TYPE() won't let us */
static void gtk_list_base_init_real (GtkListBase *self, GtkListBaseClass *g_class);
#define g_type_register_static_simple(a,b,c,d,e,evil,f) g_type_register_static_simple(a,b,c,d,e, (GInstanceInitFunc) gtk_list_base_init_real, f);
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkListBase, gtk_list_base, GTK_TYPE_WIDGET,
                                                              G_ADD_PRIVATE (GtkListBase)
                                                              G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                                                              G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))
#undef g_type_register_static_simple
G_GNUC_UNUSED static void gtk_list_base_init (GtkListBase *self) { }

static GParamSpec *properties[N_PROPS] = { NULL, };

/*
 * gtk_list_base_get_position_from_allocation:
 * @self: a `GtkListBase`
 * @across: position in pixels in the direction cross to the list
 * @along:  position in pixels in the direction of the list
 * @pos: (out): set to the looked up position
 * @area: (out caller-allocates) (optional): set to the area occupied
 *   by the returned position
 *
 * Given a coordinate in list coordinates, determine the position of the
 * item that occupies that position.
 *
 * It is possible for @area to not include the point given by (across, along).
 * This will happen for example in the last row of a gridview, where the
 * last item will be returned for the whole width, even if there are empty
 * cells.
 *
 * Returns: %TRUE on success or %FALSE if no position occupies the given offset.
 **/
static guint
gtk_list_base_get_position_from_allocation (GtkListBase           *self,
                                            int                    across,
                                            int                    along,
                                            guint                 *pos,
                                            cairo_rectangle_int_t *area)
{
  return GTK_LIST_BASE_GET_CLASS (self)->get_position_from_allocation (self, across, along, pos, area);
}

static gboolean
gtk_list_base_adjustment_is_flipped (GtkListBase    *self,
                                     GtkOrientation  orientation)
{
  if (orientation == GTK_ORIENTATION_VERTICAL)
    return FALSE;

  return gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
}

static void
gtk_list_base_get_adjustment_values (GtkListBase    *self,
                                     GtkOrientation  orientation,
                                     int            *value,
                                     int            *size,
                                     int            *page_size)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  int val, upper, ps;

  val = gtk_adjustment_get_value (priv->adjustment[orientation]);
  upper = gtk_adjustment_get_upper (priv->adjustment[orientation]);
  ps = gtk_adjustment_get_page_size (priv->adjustment[orientation]);
  if (gtk_list_base_adjustment_is_flipped (self, orientation))
    val = upper - ps - val;

  if (value)
    *value = val;
  if (size)
    *size = upper;
  if (page_size)
    *page_size = ps;
}

static void
gtk_list_base_adjustment_value_changed_cb (GtkAdjustment *adjustment,
                                           GtkListBase   *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  cairo_rectangle_int_t area, cell_area;
  int along, across, total_size;
  double align_across, align_along;
  GtkPackType side_across, side_along;
  guint pos;

  gtk_list_base_get_adjustment_values (self, OPPOSITE_ORIENTATION (priv->orientation), &area.x, &total_size, &area.width);
  if (total_size == area.width)
    align_across = 0.5;
  else if (adjustment != priv->adjustment[priv->orientation])
    align_across = CLAMP (priv->anchor_align_across, 0, 1);
  else
    align_across = (double) area.x / (total_size - area.width);
  across = area.x + round (align_across * area.width);
  across = CLAMP (across, 0, total_size - 1);

  gtk_list_base_get_adjustment_values (self, priv->orientation, &area.y, &total_size, &area.height);
  if (total_size == area.height)
    align_along = 0.5;
  else if (adjustment != priv->adjustment[OPPOSITE_ORIENTATION(priv->orientation)])
    align_along = CLAMP (priv->anchor_align_along, 0, 1);
  else
    align_along = (double) area.y / (total_size - area.height);
  along = area.y + round (align_along * area.height);
  along = CLAMP (along, 0, total_size - 1);

  if (!gtk_list_base_get_position_from_allocation (self,
                                                   across, along,
                                                   &pos,
                                                   &cell_area))
    {
      /* If we get here with n-items == 0, then somebody cleared the list but
       * GC hasn't run. So no item to be found. */
      if (gtk_list_base_get_n_items (self) == 0)
        return;

      g_warning ("%s failed to scroll to given position. Ignoring...", G_OBJECT_TYPE_NAME (self));
      return;
    }

  /* find an anchor that is in the visible area */
  if (cell_area.x < area.x && cell_area.x + cell_area.width <= area.x + area.width)
    side_across = GTK_PACK_END;
  else if (cell_area.x >= area.x && cell_area.x + cell_area.width > area.x + area.width)
    side_across = GTK_PACK_START;
  else if (cell_area.x + cell_area.width / 2 > across)
    side_across = GTK_PACK_END;
  else
    side_across = GTK_PACK_START;

  if (cell_area.y < area.y && cell_area.y + cell_area.height <= area.y + area.height)
    side_along = GTK_PACK_END;
  else if (cell_area.y >= area.y && cell_area.y + cell_area.height > area.y + area.height)
    side_along = GTK_PACK_START;
  else if (cell_area.y + cell_area.height / 2 > along)
    side_along = GTK_PACK_END;
  else
    side_along = GTK_PACK_START;

  /* Compute the align based on side to keep the values identical */
  if (side_across == GTK_PACK_START)
    align_across = (double) (cell_area.x - area.x) / area.width;
  else
    align_across = (double) (cell_area.x + cell_area.width - area.x) / area.width;
  if (side_along == GTK_PACK_START)
    align_along = (double) (cell_area.y - area.y) / area.height;
  else
    align_along = (double) (cell_area.y + cell_area.height - area.y) / area.height;

  gtk_list_base_set_anchor (self,
                            pos,
                            align_across, side_across,
                            align_along, side_along);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
gtk_list_base_clear_adjustment (GtkListBase    *self,
                                GtkOrientation  orientation)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->adjustment[orientation] == NULL)
    return;

  g_signal_handlers_disconnect_by_func (priv->adjustment[orientation],
                                        gtk_list_base_adjustment_value_changed_cb,
                                        self);
  g_clear_object (&priv->adjustment[orientation]);
}

/*
 * gtk_list_base_move_focus_along:
 * @self: a `GtkListBase`
 * @pos: position from which to move focus
 * @steps: steps to move focus - negative numbers move focus backwards
 *
 * Moves focus @steps in the direction of the list.
 * If focus cannot be moved, @pos is returned.
 * If focus should be moved out of the widget, %GTK_INVALID_LIST_POSITION
 * is returned.
 *
 * Returns: new focus position
 **/
static guint
gtk_list_base_move_focus_along (GtkListBase *self,
                                guint        pos,
                                int          steps)
{
  return GTK_LIST_BASE_GET_CLASS (self)->move_focus_along (self, pos, steps);
}

/*
 * gtk_list_base_move_focus_across:
 * @self: a `GtkListBase`
 * @pos: position from which to move focus
 * @steps: steps to move focus - negative numbers move focus backwards
 *
 * Moves focus @steps in the direction across the list.
 * If focus cannot be moved, @pos is returned.
 * If focus should be moved out of the widget, %GTK_INVALID_LIST_POSITION
 * is returned.
 *
 * Returns: new focus position
 **/
static guint
gtk_list_base_move_focus_across (GtkListBase *self,
                                 guint        pos,
                                 int          steps)
{
  return GTK_LIST_BASE_GET_CLASS (self)->move_focus_across (self, pos, steps);
}

static guint
gtk_list_base_move_focus (GtkListBase    *self,
                          guint           pos,
                          GtkOrientation  orientation,
                          int             steps)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    steps = -steps;

  if (orientation == priv->orientation)
    return gtk_list_base_move_focus_along (self, pos, steps);
  else
    return gtk_list_base_move_focus_across (self, pos, steps);
}

/*
 * gtk_list_base_get_allocation:
 * @self: a `GtkListBase`
 * @pos: item to get the area of
 * @area: (out caller-allocates): set to the area
 *   occupied by the item
 *
 * Computes the allocation of the item in the given position
 *
 * Returns: %TRUE if the item exists and has an allocation, %FALSE otherwise
 **/
static gboolean
gtk_list_base_get_allocation (GtkListBase  *self,
                              guint         pos,
                              GdkRectangle *area)
{
  return GTK_LIST_BASE_GET_CLASS (self)->get_allocation (self, pos, area);
}

/*
 * gtk_list_base_select_item:
 * @self: a `GtkListBase`
 * @pos: item to select
 * @modify: %TRUE if the selection should be modified, %FALSE
 *   if a new selection should be done. This is usually set
 *   to %TRUE if the user keeps the `<Shift>` key pressed.
 * @extend_pos: %TRUE if the selection should be extended.
 *   Selections are usually extended from the last selected
 *   position if the user presses the `<Ctrl>` key.
 *
 * Selects the item at @pos according to how GTK list widgets modify
 * selections, both when clicking rows with the mouse or when using
 * the keyboard.
 **/
static void
gtk_list_base_select_item (GtkListBase *self,
                           guint        pos,
                           gboolean     modify,
                           gboolean     extend)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkSelectionModel *model;
  gboolean success = FALSE;
  guint n_items;

  model = gtk_list_item_manager_get_model (priv->item_manager);
  if (model == NULL)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  if (pos >= n_items)
    return;

  if (extend)
    {
      guint extend_pos = gtk_list_item_tracker_get_position (priv->item_manager, priv->selected);

      if (extend_pos < n_items)
        {
          guint max = MAX (extend_pos, pos);
          guint min = MIN (extend_pos, pos);

          if (modify)
            {
              if (gtk_selection_model_is_selected (model, extend_pos))
                {
                  success = gtk_selection_model_select_range (model,
                                                              min,
                                                              max - min + 1,
                                                              FALSE);
                }
              else
                {
                  success = gtk_selection_model_unselect_range (model,
                                                                min,
                                                                max - min + 1);
                }
            }
          else
            {
              success = gtk_selection_model_select_range (model,
                                                          min,
                                                          max - min + 1,
                                                          TRUE);
            }
        }
      /* If there's no range to select or selecting ranges isn't supported
       * by the model, fall through to normal setting.
       */
    }

  if (success)
    return;

  if (modify)
    {
      if (gtk_selection_model_is_selected (model, pos))
        gtk_selection_model_unselect_item (model, pos);
      else
        gtk_selection_model_select_item (model, pos, FALSE);
    }
  else
    {
      gtk_selection_model_select_item (model, pos, TRUE);
    }

  gtk_list_item_tracker_set_position (priv->item_manager,
                                      priv->selected,
                                      pos,
                                      0, 0);
}

/*
 * gtk_list_base_grab_focus_on_item:
 * @self: a `GtkListBase`
 * @pos: position of the item to focus
 * @select: %TRUE to select the item
 * @modify: if selecting, %TRUE to modify the selected
 *   state, %FALSE to always select
 * @extend: if selecting, %TRUE to extend the selection,
 *   %FALSE to only operate on this item
 *
 * Tries to grab focus on the given item. If there is no item
 * at this position or grabbing focus failed, %FALSE will be
 * returned.
 *
 * Returns: %TRUE if focusing the item succeeded
 **/
static gboolean
gtk_list_base_grab_focus_on_item (GtkListBase *self,
                                  guint        pos,
                                  gboolean     select,
                                  gboolean     modify,
                                  gboolean     extend)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkListTile *tile;
  gboolean success;

  tile = gtk_list_item_manager_get_nth (priv->item_manager, pos, NULL);
  if (tile == NULL)
    return FALSE;

  if (!tile->widget)
    {
      GtkListItemTracker *tracker = gtk_list_item_tracker_new (priv->item_manager);

      /* We need a tracker here to create the widget.
       * That needs to have happened or we can't grab it.
       * And we can't use a different tracker, because they manage important rows,
       * so we create a temporary one. */
      gtk_list_item_tracker_set_position (priv->item_manager, tracker, pos, 0, 0);

      tile = gtk_list_item_manager_get_nth (priv->item_manager, pos, NULL);
      g_assert (tile->widget);

      success = gtk_widget_grab_focus (tile->widget);

      gtk_list_item_tracker_free (priv->item_manager, tracker);
    }
  else
    {
      success = gtk_widget_grab_focus (tile->widget);
    }

  if (!success)
    return FALSE;

  if (select)
    {
      tile = gtk_list_item_manager_get_nth (priv->item_manager, pos, NULL);

      /* We do this convoluted calling into the widget because that way
       * GtkListItem::selectable gets respected, which is what one would expect.
       */
      g_assert (tile->widget);
      gtk_widget_activate_action (tile->widget, "listitem.select", "(bb)", modify, extend);
    }

  return TRUE;
}

guint
gtk_list_base_get_n_items (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->model == NULL)
    return 0;

  return g_list_model_get_n_items (G_LIST_MODEL (priv->model));
}

static guint
gtk_list_base_get_focus_position (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return gtk_list_item_tracker_get_position (priv->item_manager, priv->focus);
}

static gboolean
gtk_list_base_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  guint old, pos, n_items;
  GtkWidget *focus_child;
  GtkListTile *tile;

  focus_child = gtk_widget_get_focus_child (widget);
  /* focus is moving around fine inside the focus child, don't disturb it */
  if (focus_child && gtk_widget_child_focus (focus_child, direction))
    return TRUE;

  pos = gtk_list_base_get_focus_position (self);
  n_items = gtk_list_base_get_n_items (self);
  old = pos;

  if (pos >= n_items)
    {
      if (n_items == 0)
        return FALSE;

      pos = 0;
    }
  else if (focus_child == NULL)
    {
      /* Focus was outside the list, just grab the old focus item
       * while keeping the selection intact.
       */
      old = GTK_INVALID_LIST_POSITION;
      if (priv->tab_behavior == GTK_LIST_TAB_ALL)
        {
          if (direction == GTK_DIR_TAB_FORWARD)
            pos = 0;
          else if (direction == GTK_DIR_TAB_BACKWARD)
            pos = n_items - 1;
        }
    }
  else
    {
      switch (direction)
        {
        case GTK_DIR_TAB_FORWARD:
          if (priv->tab_behavior == GTK_LIST_TAB_ALL)
            {
              pos++;
              if (pos >= n_items)
                return FALSE;
            }
          else
            {
              return FALSE;
            }
          break;

        case GTK_DIR_TAB_BACKWARD:
          if (priv->tab_behavior == GTK_LIST_TAB_ALL)
            {
              if (pos == 0)
                return FALSE;
              pos--;
            }
          else
            {
              return FALSE;
            }
          break;

        case GTK_DIR_UP:
          pos = gtk_list_base_move_focus (self, pos, GTK_ORIENTATION_VERTICAL, -1);
          break;

        case GTK_DIR_DOWN:
          pos = gtk_list_base_move_focus (self, pos, GTK_ORIENTATION_VERTICAL, 1);
          break;

        case GTK_DIR_LEFT:
          pos = gtk_list_base_move_focus (self, pos, GTK_ORIENTATION_HORIZONTAL, -1);
          break;

        case GTK_DIR_RIGHT:
          pos = gtk_list_base_move_focus (self, pos, GTK_ORIENTATION_HORIZONTAL, 1);
          break;

        default:
          g_assert_not_reached ();
          return TRUE;
        }
    }

  if (old == pos)
    return TRUE;

  tile = gtk_list_item_manager_get_nth (priv->item_manager, pos, NULL);
  if (tile == NULL)
    return FALSE;

  /* This shouldn't really happen, but if it does, oh well */
  if (tile->widget == NULL)
    return gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), pos, TRUE, FALSE, FALSE);

  return gtk_widget_child_focus (tile->widget, direction);
}

static gboolean
gtk_list_base_grab_focus (GtkWidget *widget)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  guint pos;

  pos = gtk_list_item_tracker_get_position (priv->item_manager, priv->focus);
  if (gtk_list_base_grab_focus_on_item (self, pos, FALSE, FALSE, FALSE))
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_list_base_parent_class)->grab_focus (widget);
}

static void
gtk_list_base_dispose (GObject *object)
{
  GtkListBase *self = GTK_LIST_BASE (object);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  gtk_list_base_clear_adjustment (self, GTK_ORIENTATION_HORIZONTAL);
  gtk_list_base_clear_adjustment (self, GTK_ORIENTATION_VERTICAL);

  if (priv->anchor)
    {
      gtk_list_item_tracker_free (priv->item_manager, priv->anchor);
      priv->anchor = NULL;
    }
  if (priv->selected)
    {
      gtk_list_item_tracker_free (priv->item_manager, priv->selected);
      priv->selected = NULL;
    }
  if (priv->focus)
    {
      gtk_list_item_tracker_free (priv->item_manager, priv->focus);
      priv->focus = NULL;
    }
  g_clear_object (&priv->item_manager);

  g_clear_object (&priv->model);

  G_OBJECT_CLASS (gtk_list_base_parent_class)->dispose (object);
}

static void
gtk_list_base_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListBase *self = GTK_LIST_BASE (object);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, priv->adjustment[GTK_ORIENTATION_HORIZONTAL]);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, priv->scroll_policy[GTK_ORIENTATION_HORIZONTAL]);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, priv->adjustment[GTK_ORIENTATION_VERTICAL]);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, priv->scroll_policy[GTK_ORIENTATION_VERTICAL]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_base_set_adjustment (GtkListBase    *self,
                              GtkOrientation  orientation,
                              GtkAdjustment  *adjustment)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->adjustment[orientation] == adjustment)
    return;

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  else
    gtk_adjustment_configure (adjustment, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  g_object_ref_sink (adjustment);

  gtk_list_base_clear_adjustment (self, orientation);

  priv->adjustment[orientation] = adjustment;

  g_signal_connect (adjustment, "value-changed",
		    G_CALLBACK (gtk_list_base_adjustment_value_changed_cb),
		    self);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
gtk_list_base_set_scroll_policy (GtkListBase         *self,
                                 GtkOrientation       orientation,
                                 GtkScrollablePolicy  scroll_policy)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->scroll_policy[orientation] == scroll_policy)
    return;

  priv->scroll_policy[orientation] = scroll_policy;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self),
                            orientation == GTK_ORIENTATION_HORIZONTAL
                            ? properties[PROP_HSCROLL_POLICY]
                            : properties[PROP_VSCROLL_POLICY]);
}

static void
gtk_list_base_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkListBase *self = GTK_LIST_BASE (object);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      gtk_list_base_set_adjustment (self, GTK_ORIENTATION_HORIZONTAL, g_value_get_object (value));
      break;

    case PROP_HSCROLL_POLICY:
      gtk_list_base_set_scroll_policy (self, GTK_ORIENTATION_HORIZONTAL, g_value_get_enum (value));
      break;

    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (priv->orientation != orientation)
          {
            priv->orientation = orientation;
            gtk_widget_update_orientation (GTK_WIDGET (self), priv->orientation);
            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ORIENTATION]);
          }
      }
      break;

    case PROP_VADJUSTMENT:
      gtk_list_base_set_adjustment (self, GTK_ORIENTATION_VERTICAL, g_value_get_object (value));
      break;

    case PROP_VSCROLL_POLICY:
      gtk_list_base_set_scroll_policy (self, GTK_ORIENTATION_VERTICAL, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_base_compute_scroll_align (int            cell_start,
                                    int            cell_size,
                                    int            visible_start,
                                    int            visible_size,
                                    double         current_align,
                                    GtkPackType    current_side,
                                    double        *new_align,
                                    GtkPackType   *new_side)
{
  int cell_end, visible_end;

  visible_end = visible_start + visible_size;
  cell_end = cell_start + cell_size;

  if (cell_size <= visible_size)
    {
      if (cell_start < visible_start)
        {
          *new_align = 0.0;
          *new_side = GTK_PACK_START;
        }
      else if (cell_end > visible_end)
        {
          *new_align = 1.0;
          *new_side = GTK_PACK_END;
        }
      else
        {
          /* XXX: start or end here? */
          *new_side = GTK_PACK_START;
          *new_align = (double) (cell_start - visible_start) / visible_size;
        }
    }
  else
    {
      /* This is the unlikely case of the cell being higher than the visible area */
      if (cell_start > visible_start)
        {
          *new_align = 0.0;
          *new_side = GTK_PACK_START;
        }
      else if (cell_end < visible_end)
        {
          *new_align = 1.0;
          *new_side = GTK_PACK_END;
        }
      else
        {
          /* the cell already covers the whole screen */
          *new_align = current_align;
          *new_side = current_side;
        }
    }
}

static void
gtk_list_base_scroll_to_item (GtkListBase   *self,
                              guint          pos,
                              GtkScrollInfo *scroll)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  double align_along, align_across;
  GtkPackType side_along, side_across;
  GdkRectangle area, viewport;
  int x, y;

  if (!gtk_list_base_get_allocation (GTK_LIST_BASE (self), pos, &area))
    {
      g_clear_pointer (&scroll, gtk_scroll_info_unref);
      return;
    }

  gtk_list_base_get_adjustment_values (GTK_LIST_BASE (self),
                                       gtk_list_base_get_orientation (GTK_LIST_BASE (self)),
                                       &viewport.y, NULL, &viewport.height);
  gtk_list_base_get_adjustment_values (GTK_LIST_BASE (self),
                                       gtk_list_base_get_opposite_orientation (GTK_LIST_BASE (self)),
                                       &viewport.x, NULL, &viewport.width);

  gtk_scroll_info_compute_scroll (scroll, &area, &viewport, &x, &y);

  gtk_list_base_compute_scroll_align (area.y, area.height,
                                      y, viewport.height,
                                      priv->anchor_align_along, priv->anchor_side_along,
                                      &align_along, &side_along);

  gtk_list_base_compute_scroll_align (area.x, area.width,
                                      x, viewport.width,
                                      priv->anchor_align_across, priv->anchor_side_across,
                                      &align_across, &side_across);

  gtk_list_base_set_anchor (self,
                            pos,
                            align_across, side_across,
                            align_along, side_along);

  g_clear_pointer (&scroll, gtk_scroll_info_unref);
}

static void
gtk_list_base_scroll_to_item_action (GtkWidget  *widget,
                                     const char *action_name,
                                     GVariant   *parameter)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  guint pos;

  if (!g_variant_check_format_string (parameter, "u", FALSE))
    return;

  g_variant_get (parameter, "u", &pos);

  gtk_list_base_scroll_to_item (self, pos, NULL);
}

static void
gtk_list_base_set_focus_child (GtkWidget *widget,
                               GtkWidget *child)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  guint pos;

  GTK_WIDGET_CLASS (gtk_list_base_parent_class)->set_focus_child (widget, child);

  if (!GTK_IS_LIST_ITEM_BASE (child))
    return;

  pos = gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (child));

  if (pos != gtk_list_item_tracker_get_position (priv->item_manager, priv->focus))
    {
      gtk_list_base_scroll_to_item (self, pos, NULL);
      gtk_list_item_tracker_set_position (priv->item_manager,
                                          priv->focus,
                                          pos,
                                          0,
                                          0);
    }
}

static void
gtk_list_base_select_item_action (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *parameter)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  guint pos;
  gboolean modify, extend;

  g_variant_get (parameter, "(ubb)", &pos, &modify, &extend);

  gtk_list_base_select_item (self, pos, modify, extend);
}

static void
gtk_list_base_select_all (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *parameter)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkSelectionModel *selection_model;

  selection_model = gtk_list_item_manager_get_model (priv->item_manager);
  if (selection_model == NULL)
    return;

  gtk_selection_model_select_all (selection_model);
}

static void
gtk_list_base_unselect_all (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *parameter)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkSelectionModel *selection_model;

  selection_model = gtk_list_item_manager_get_model (priv->item_manager);
  if (selection_model == NULL)
    return;

  gtk_selection_model_unselect_all (selection_model);
}

static gboolean
gtk_list_base_move_cursor_to_start (GtkWidget *widget,
                                    GVariant  *args,
                                    gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  gboolean select, modify, extend;

  if (gtk_list_base_get_n_items (self) == 0)
    return TRUE;

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), 0, select, modify, extend);

  return TRUE;
}

static gboolean
gtk_list_base_move_cursor_page_up (GtkWidget *widget,
                                   GVariant  *args,
                                   gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  gboolean select, modify, extend;
  cairo_rectangle_int_t area, new_area;
  int page_size;
  guint pos, new_pos;

  pos = gtk_list_base_get_focus_position (self);
  page_size = gtk_adjustment_get_page_size (priv->adjustment[priv->orientation]);
  if (!gtk_list_base_get_allocation (self, pos, &area))
    return TRUE;
  if (!gtk_list_base_get_position_from_allocation (self,
                                                   area.x + area.width / 2,
                                                   MAX (0, area.y + area.height - page_size),
                                                   &new_pos,
                                                   &new_area))
    return TRUE;

  /* We want the whole row to be visible */
  if (new_area.y < MAX (0, area.y + area.height - page_size))
    new_pos = gtk_list_base_move_focus_along (self, new_pos, 1);
  /* But we definitely want to move if we can */
  if (new_pos >= pos)
    {
      new_pos = gtk_list_base_move_focus_along (self, new_pos, -1);
      if (new_pos == pos)
        return TRUE;
    }

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), new_pos, select, modify, extend);

  return TRUE;
}

static gboolean
gtk_list_base_move_cursor_page_down (GtkWidget *widget,
                                     GVariant  *args,
                                     gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  gboolean select, modify, extend;
  cairo_rectangle_int_t area, new_area;
  int page_size, end;
  guint pos, new_pos;

  pos = gtk_list_base_get_focus_position (self);
  page_size = gtk_adjustment_get_page_size (priv->adjustment[priv->orientation]);
  end = gtk_adjustment_get_upper (priv->adjustment[priv->orientation]);
  if (end == 0)
    return TRUE;

  if (!gtk_list_base_get_allocation (self, pos, &area))
    return TRUE;

  if (!gtk_list_base_get_position_from_allocation (self,
                                                   area.x + area.width / 2,
                                                   MIN (end, area.y + page_size) - 1,
                                                   &new_pos,
                                                   &new_area))
    return TRUE;

  /* We want the whole row to be visible */
  if (new_area.y + new_area.height > MIN (end, area.y + page_size))
    new_pos = gtk_list_base_move_focus_along (self, new_pos, -1);
  /* But we definitely want to move if we can */
  if (new_pos <= pos)
    {
      new_pos = gtk_list_base_move_focus_along (self, new_pos, 1);
      if (new_pos == pos)
        return TRUE;
    }

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), new_pos, select, modify, extend);

  return TRUE;
}

static gboolean
gtk_list_base_move_cursor_to_end (GtkWidget *widget,
                                  GVariant  *args,
                                  gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  gboolean select, modify, extend;
  guint n_items;

  n_items = gtk_list_base_get_n_items (self);
  if (n_items == 0)
    return TRUE;

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), n_items - 1, select, modify, extend);

  return TRUE;
}

static gboolean
gtk_list_base_move_cursor (GtkWidget *widget,
                           GVariant  *args,
                           gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  int amount;
  guint orientation;
  guint old_pos, new_pos;
  gboolean select, modify, extend;

  g_variant_get (args, "(ubbbi)", &orientation, &select, &modify, &extend, &amount);

  old_pos = gtk_list_base_get_focus_position (self);
  new_pos = gtk_list_base_move_focus (self, old_pos, orientation, amount);

  if (old_pos != new_pos)
    gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), new_pos, select, modify, extend);

  return TRUE;
}

static void
gtk_list_base_add_move_binding (GtkWidgetClass *widget_class,
                                guint           keyval,
                                GtkOrientation  orientation,
                                int             amount)
{
  gtk_widget_class_add_binding (widget_class,
                                keyval,
                                0,
                                gtk_list_base_move_cursor,
                                "(ubbbi)", orientation, TRUE, FALSE, FALSE, amount);
  gtk_widget_class_add_binding (widget_class,
                                keyval,
                                GDK_CONTROL_MASK,
                                gtk_list_base_move_cursor,
                                "(ubbbi)", orientation, FALSE, FALSE, FALSE, amount);
  gtk_widget_class_add_binding (widget_class,
                                keyval,
                                GDK_SHIFT_MASK,
                                gtk_list_base_move_cursor,
                                "(ubbbi)", orientation, TRUE, FALSE, TRUE, amount);
  gtk_widget_class_add_binding (widget_class,
                                keyval,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                gtk_list_base_move_cursor,
                                "(ubbbi)", orientation, TRUE, TRUE, TRUE, amount);
}

static void
gtk_list_base_add_custom_move_binding (GtkWidgetClass  *widget_class,
                                       guint            keyval,
                                       GtkShortcutFunc  callback)
{
  gtk_widget_class_add_binding (widget_class,
                                keyval,
                                0,
                                callback,
                                "(bbb)", TRUE, FALSE, FALSE);
  gtk_widget_class_add_binding (widget_class,
                                keyval,
                                GDK_CONTROL_MASK,
                                callback,
                                "(bbb)", FALSE, FALSE, FALSE);
  gtk_widget_class_add_binding (widget_class,
                                keyval,
                                GDK_SHIFT_MASK,
                                callback,
                                "(bbb)", TRUE, FALSE, TRUE);
  gtk_widget_class_add_binding (widget_class,
                                keyval,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                callback,
                                "(bbb)", TRUE, TRUE, TRUE);
}

static void
gtk_list_base_class_init (GtkListBaseClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gpointer iface;

  widget_class->focus = gtk_list_base_focus;
  widget_class->grab_focus = gtk_list_base_grab_focus;
  widget_class->set_focus_child = gtk_list_base_set_focus_child;

  gobject_class->dispose = gtk_list_base_dispose;
  gobject_class->get_property = gtk_list_base_get_property;
  gobject_class->set_property = gtk_list_base_set_property;

  /* GtkScrollable implementation */
  iface = g_type_default_interface_peek (GTK_TYPE_SCROLLABLE);
  properties[PROP_HADJUSTMENT] =
      g_param_spec_override ("hadjustment",
                             g_object_interface_find_property (iface, "hadjustment"));
  properties[PROP_HSCROLL_POLICY] =
      g_param_spec_override ("hscroll-policy",
                             g_object_interface_find_property (iface, "hscroll-policy"));
  properties[PROP_VADJUSTMENT] =
      g_param_spec_override ("vadjustment",
                             g_object_interface_find_property (iface, "vadjustment"));
  properties[PROP_VSCROLL_POLICY] =
      g_param_spec_override ("vscroll-policy",
                             g_object_interface_find_property (iface, "vscroll-policy"));

  /**
   * GtkListBase:orientation:
   *
   * The orientation of the list. See GtkOrientable:orientation
   * for details.
   */
  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation", NULL, NULL,
                       GTK_TYPE_ORIENTATION,
                       GTK_ORIENTATION_VERTICAL,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkListBase|list.scroll-to-item:
   * @position: position of item to scroll to
   *
   * Moves the visible area to the item given in @position with the minimum amount
   * of scrolling required. If the item is already visible, nothing happens.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.scroll-to-item",
                                   "u",
                                   gtk_list_base_scroll_to_item_action);

  /**
   * GtkListBase|list.select-item:
   * @position: position of item to select
   * @modify: %TRUE to toggle the existing selection, %FALSE to select
   * @extend: %TRUE to extend the selection
   *
   * Changes selection.
   *
   * If @extend is %TRUE and the model supports selecting ranges, the
   * affected items are all items from the last selected item to the item
   * in @position.
   * If @extend is %FALSE or selecting ranges is not supported, only the
   * item in @position is affected.
   *
   * If @modify is %TRUE, the affected items will be set to the same state.
   * If @modify is %FALSE, the affected items will be selected and
   * all other items will be deselected.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.select-item",
                                   "(ubb)",
                                   gtk_list_base_select_item_action);

  /**
   * GtkListBase|list.select-all:
   *
   * If the selection model supports it, select all items in the model.
   * If not, do nothing.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.select-all",
                                   NULL,
                                   gtk_list_base_select_all);

  /**
   * GtkListBase|list.unselect-all:
   *
   * If the selection model supports it, unselect all items in the model.
   * If not, do nothing.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.unselect-all",
                                   NULL,
                                   gtk_list_base_unselect_all);

  gtk_list_base_add_move_binding (widget_class, GDK_KEY_Up, GTK_ORIENTATION_VERTICAL, -1);
  gtk_list_base_add_move_binding (widget_class, GDK_KEY_KP_Up, GTK_ORIENTATION_VERTICAL, -1);
  gtk_list_base_add_move_binding (widget_class, GDK_KEY_Down, GTK_ORIENTATION_VERTICAL, 1);
  gtk_list_base_add_move_binding (widget_class, GDK_KEY_KP_Down, GTK_ORIENTATION_VERTICAL, 1);
  gtk_list_base_add_move_binding (widget_class, GDK_KEY_Left, GTK_ORIENTATION_HORIZONTAL, -1);
  gtk_list_base_add_move_binding (widget_class, GDK_KEY_KP_Left, GTK_ORIENTATION_HORIZONTAL, -1);
  gtk_list_base_add_move_binding (widget_class, GDK_KEY_Right, GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_list_base_add_move_binding (widget_class, GDK_KEY_KP_Right, GTK_ORIENTATION_HORIZONTAL, 1);

  gtk_list_base_add_custom_move_binding (widget_class, GDK_KEY_Home, gtk_list_base_move_cursor_to_start);
  gtk_list_base_add_custom_move_binding (widget_class, GDK_KEY_KP_Home, gtk_list_base_move_cursor_to_start);
  gtk_list_base_add_custom_move_binding (widget_class, GDK_KEY_End, gtk_list_base_move_cursor_to_end);
  gtk_list_base_add_custom_move_binding (widget_class, GDK_KEY_KP_End, gtk_list_base_move_cursor_to_end);
  gtk_list_base_add_custom_move_binding (widget_class, GDK_KEY_Page_Up, gtk_list_base_move_cursor_page_up);
  gtk_list_base_add_custom_move_binding (widget_class, GDK_KEY_KP_Page_Up, gtk_list_base_move_cursor_page_up);
  gtk_list_base_add_custom_move_binding (widget_class, GDK_KEY_Page_Down, gtk_list_base_move_cursor_page_down);
  gtk_list_base_add_custom_move_binding (widget_class, GDK_KEY_KP_Page_Down, gtk_list_base_move_cursor_page_down);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_a, GDK_CONTROL_MASK, "list.select-all", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_slash, GDK_CONTROL_MASK, "list.select-all", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_A, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "list.unselect-all", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_backslash, GDK_CONTROL_MASK, "list.unselect-all", NULL);
}

static gboolean
autoscroll_cb (GtkWidget     *widget,
               GdkFrameClock *frame_clock,
               gpointer       data)
{
  GtkListBase *self = data;
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  double value;
  double delta_x, delta_y;

  value = gtk_adjustment_get_value (priv->adjustment[GTK_ORIENTATION_HORIZONTAL]);
  gtk_adjustment_set_value (priv->adjustment[GTK_ORIENTATION_HORIZONTAL], value + priv->autoscroll_delta_x);

  delta_x = gtk_adjustment_get_value (priv->adjustment[GTK_ORIENTATION_HORIZONTAL]) - value;

  value = gtk_adjustment_get_value (priv->adjustment[GTK_ORIENTATION_VERTICAL]);
  gtk_adjustment_set_value (priv->adjustment[GTK_ORIENTATION_VERTICAL], value + priv->autoscroll_delta_y);

  delta_y = gtk_adjustment_get_value (priv->adjustment[GTK_ORIENTATION_VERTICAL]) - value;

  if (delta_x != 0 || delta_y != 0)
    {
      return G_SOURCE_CONTINUE;
    }
  else
    {
      priv->autoscroll_id = 0;
      return G_SOURCE_REMOVE;
    }
}

static void
add_autoscroll (GtkListBase *self,
                double       delta_x,
                double       delta_y)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (gtk_list_base_adjustment_is_flipped (self, GTK_ORIENTATION_HORIZONTAL))
    priv->autoscroll_delta_x = -delta_x;
  else
    priv->autoscroll_delta_x = delta_x;
  if (gtk_list_base_adjustment_is_flipped (self, GTK_ORIENTATION_VERTICAL))
    priv->autoscroll_delta_y = -delta_y;
  else
    priv->autoscroll_delta_y = delta_y;

  if (priv->autoscroll_id == 0)
    priv->autoscroll_id = gtk_widget_add_tick_callback (GTK_WIDGET (self), autoscroll_cb, self, NULL);
}

static void
remove_autoscroll (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->autoscroll_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), priv->autoscroll_id);
      priv->autoscroll_id = 0;
    }
}

#define SCROLL_EDGE_SIZE 30

static void
update_autoscroll (GtkListBase *self,
                   double       x,
                   double       y)
{
  double width, height;
  double delta_x, delta_y;

  width = gtk_widget_get_width (GTK_WIDGET (self));

  if (x < SCROLL_EDGE_SIZE)
    delta_x = - (SCROLL_EDGE_SIZE - x)/3.0;
  else if (width - x < SCROLL_EDGE_SIZE)
    delta_x = (SCROLL_EDGE_SIZE - (width - x))/3.0;
  else
    delta_x = 0;

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    delta_x = - delta_x;

  height = gtk_widget_get_height (GTK_WIDGET (self));

  if (y < SCROLL_EDGE_SIZE)
    delta_y = - (SCROLL_EDGE_SIZE - y)/3.0;
  else if (height - y < SCROLL_EDGE_SIZE)
    delta_y = (SCROLL_EDGE_SIZE - (height - y))/3.0;
  else
    delta_y = 0;

  if (delta_x != 0 || delta_y != 0)
    add_autoscroll (self, delta_x, delta_y);
  else
    remove_autoscroll (self);
}

/*
 * gtk_list_base_size_allocate_child:
 * @self: The listbase
 * @boxes: The CSS boxes of @self to allow for proper
 *     clipping
 * @child: The child
 * @x: top left coordinate in the across direction
 * @y: top right coordinate in the along direction
 * @width: size in the across direction
 * @height: size in the along direction
 *
 * Allocates a child widget in the list coordinate system,
 * but with the coordinates already offset by the scroll
 * offset.
 **/
static void
gtk_list_base_size_allocate_child (GtkListBase *self,
                                   GtkCssBoxes *boxes,
                                   GtkWidget   *child,
                                   int          x,
                                   int          y,
                                   int          width,
                                   int          height)
{
  GtkAllocation child_allocation;
  int self_width;

  self_width = gtk_widget_get_width (GTK_WIDGET (self));

  if (gtk_list_base_get_orientation (GTK_LIST_BASE (self)) == GTK_ORIENTATION_VERTICAL)
    {
      if (_gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR)
        {
          child_allocation.x = x;
          child_allocation.y = y;
        }
      else
        {
          child_allocation.x = self_width - x - width;
          child_allocation.y = y;
        }
      child_allocation.width = width;
      child_allocation.height = height;
    }
  else
    {
      if (_gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR)
        {
          child_allocation.x = y;
          child_allocation.y = x;
        }
      else
        {
          child_allocation.x = self_width - y - height;
          child_allocation.y = x;
        }
      child_allocation.width = height;
      child_allocation.height = width;
    }

  if (!graphene_rect_intersection (gtk_css_boxes_get_padding_rect (boxes),
                                   &GRAPHENE_RECT_INIT(
                                     child_allocation.x + GTK_LIST_BASE_CHILD_MAX_OVERDRAW,
                                     child_allocation.y + GTK_LIST_BASE_CHILD_MAX_OVERDRAW,
                                     child_allocation.width + 2 * GTK_LIST_BASE_CHILD_MAX_OVERDRAW,
                                     child_allocation.height + 2 * GTK_LIST_BASE_CHILD_MAX_OVERDRAW
                                   ),
                                   NULL))
    {
      /* child is fully outside the viewport, hide it and don't allocate it */
      gtk_widget_set_child_visible (child, FALSE);
      return;
    }

  gtk_widget_set_child_visible (child, TRUE);

  gtk_widget_size_allocate (child, &child_allocation, -1);
}

static void
gtk_list_base_allocate_children (GtkListBase *self,
                                 GtkCssBoxes *boxes)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkListTile *tile;
  int dx, dy;

  gtk_list_base_get_adjustment_values (self, OPPOSITE_ORIENTATION (priv->orientation), &dx, NULL, NULL);
  gtk_list_base_get_adjustment_values (self, priv->orientation, &dy, NULL, NULL);

  for (tile = gtk_list_item_manager_get_first (priv->item_manager);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      if (tile->widget)
        {
          gtk_list_base_size_allocate_child (GTK_LIST_BASE (self),
                                             boxes,
                                             tile->widget,
                                             tile->area.x - dx,
                                             tile->area.y - dy,
                                             tile->area.width,
                                             tile->area.height);
        }
    }
}

static void
gtk_list_base_widget_to_list (GtkListBase *self,
                              double       x_widget,
                              double       y_widget,
                              int         *across_out,
                              int         *along_out)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x_widget = gtk_widget_get_width (widget) - x_widget;

  gtk_list_base_get_adjustment_values (self, OPPOSITE_ORIENTATION (priv->orientation), across_out, NULL, NULL);
  gtk_list_base_get_adjustment_values (self, priv->orientation, along_out, NULL, NULL);

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      *across_out += x_widget;
      *along_out += y_widget;
    }
  else
    {
      *across_out += y_widget;
      *along_out += x_widget;
    }
}

static GtkBitset *
gtk_list_base_get_items_in_rect (GtkListBase        *self,
                                 const GdkRectangle *rect)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GdkRectangle bounds;

  gtk_list_item_manager_get_tile_bounds (priv->item_manager, &bounds);
  if (!gdk_rectangle_intersect (&bounds, rect, &bounds))
    return gtk_bitset_new_empty ();

  return GTK_LIST_BASE_GET_CLASS (self)->get_items_in_rect (self, &bounds);
}

static gboolean
gtk_list_base_get_rubberband_coords (GtkListBase  *self,
                                     GdkRectangle *rect)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  int x1, x2, y1, y2;

  if (!priv->rubberband)
    return FALSE;

  if (priv->rubberband->start_tracker == NULL)
    {
      x1 = 0;
      y1 = 0;
    }
  else
    {
      GdkRectangle area;
      guint pos = gtk_list_item_tracker_get_position (priv->item_manager, priv->rubberband->start_tracker);

      if (gtk_list_base_get_allocation (self, pos, &area))
        {
          x1 = area.x + area.width * priv->rubberband->start_align_across;
          y1 = area.y + area.height * priv->rubberband->start_align_along;
        }
      else
        {
          x1 = 0;
          y1 = 0;
        }
    }

  gtk_list_base_widget_to_list (self,
                                priv->rubberband->pointer_x, priv->rubberband->pointer_y,
                                &x2, &y2);

  rect->x = MIN (x1, x2);
  rect->y = MIN (y1, y2);
  rect->width = ABS (x1 - x2) + 1;
  rect->height = ABS (y1 - y2) + 1;

  return TRUE;
}

static void
gtk_list_base_allocate_rubberband (GtkListBase *self,
                                   GtkCssBoxes *boxes)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkRequisition min_size;
  GdkRectangle rect;
  int offset_x, offset_y;

  if (!gtk_list_base_get_rubberband_coords (self, &rect))
    return;

  gtk_widget_get_preferred_size (priv->rubberband->widget, &min_size, NULL);
  rect.width = MAX (min_size.width, rect.width);
  rect.height = MAX (min_size.height, rect.height);

  gtk_list_base_get_adjustment_values (self, OPPOSITE_ORIENTATION (priv->orientation), &offset_x, NULL, NULL);
  gtk_list_base_get_adjustment_values (self, priv->orientation, &offset_y, NULL, NULL);
  rect.x -= offset_x;
  rect.y -= offset_y;

  gtk_list_base_size_allocate_child (self,
                                     boxes,
                                     priv->rubberband->widget,
                                     rect.x, rect.y, rect.width, rect.height);
}

static void
gtk_list_base_start_rubberband (GtkListBase *self,
                                double       x,
                                double       y)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  cairo_rectangle_int_t item_area;
  int list_x, list_y;
  guint pos;

  if (priv->rubberband)
    return;

  gtk_list_base_widget_to_list (self, x, y, &list_x, &list_y);
  if (!gtk_list_base_get_position_from_allocation (self, list_x, list_y, &pos, &item_area))
    {
      g_warning ("Could not start rubberbanding: No item\n");
      return;
    }

  priv->rubberband = g_new0 (RubberbandData, 1);

  priv->rubberband->start_tracker = gtk_list_item_tracker_new (priv->item_manager);
  gtk_list_item_tracker_set_position (priv->item_manager, priv->rubberband->start_tracker, pos, 0, 0);
  priv->rubberband->start_align_across = (double) (list_x - item_area.x) / item_area.width;
  priv->rubberband->start_align_along = (double) (list_y - item_area.y) / item_area.height;

  priv->rubberband->pointer_x = x;
  priv->rubberband->pointer_y = y;

  priv->rubberband->widget = gtk_gizmo_new ("rubberband",
                                            NULL, NULL, NULL, NULL, NULL, NULL);
  gtk_widget_set_parent (priv->rubberband->widget, GTK_WIDGET (self));
}

static void
gtk_list_base_apply_rubberband_selection (GtkListBase *self,
                                          gboolean     modify,
                                          gboolean     extend)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkSelectionModel *model;

  if (!priv->rubberband)
    return;

  model = gtk_list_item_manager_get_model (priv->item_manager);
  if (model != NULL)
    {
      GtkBitset *selected, *mask;
      GdkRectangle rect;
      GtkBitset *rubberband_selection;

      if (!gtk_list_base_get_rubberband_coords (self, &rect))
        return;

      rubberband_selection = gtk_list_base_get_items_in_rect (self, &rect);

      if (modify && extend) /* Ctrl + Shift */
        {
          if (gtk_bitset_is_empty (rubberband_selection))
            {
              selected = gtk_bitset_ref (rubberband_selection);
              mask = gtk_bitset_ref (rubberband_selection);
            }
          else
            {
              GtkBitset *current;
              guint min = gtk_bitset_get_minimum (rubberband_selection);
              guint max = gtk_bitset_get_maximum (rubberband_selection);
              /* toggle the rubberband, keep the rest */
              current = gtk_selection_model_get_selection_in_range (model, min, max - min + 1);
              selected = gtk_bitset_copy (current);
              gtk_bitset_unref (current);
              gtk_bitset_intersect (selected, rubberband_selection);
              gtk_bitset_difference (selected, rubberband_selection);

              mask = gtk_bitset_ref (rubberband_selection);
            }
        }
      else if (modify) /* Ctrl */
        {
          /* select the rubberband, keep the rest */
          selected = gtk_bitset_ref (rubberband_selection);
          mask = gtk_bitset_ref (rubberband_selection);
        }
      else if (extend) /* Shift */
        {
          /* unselect the rubberband, keep the rest */
          selected = gtk_bitset_new_empty ();
          mask = gtk_bitset_ref (rubberband_selection);
        }
      else /* no modifier */
        {
          /* select the rubberband, clear the rest */
          selected = gtk_bitset_ref (rubberband_selection);
          mask = gtk_bitset_new_empty ();
          gtk_bitset_add_range (mask, 0, g_list_model_get_n_items (G_LIST_MODEL (model)));
        }

      gtk_selection_model_set_selection (model, selected, mask);

      gtk_bitset_unref (selected);
      gtk_bitset_unref (mask);
      gtk_bitset_unref (rubberband_selection);
    }
}

static void
gtk_list_base_stop_rubberband (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkListTile *tile;

  if (!priv->rubberband)
    return;

  for (tile = gtk_list_item_manager_get_first (priv->item_manager);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      if (tile->widget)
        gtk_widget_unset_state_flags (tile->widget, GTK_STATE_FLAG_ACTIVE);
    }

  gtk_list_item_tracker_free (priv->item_manager, priv->rubberband->start_tracker);
  g_clear_pointer (&priv->rubberband->widget, gtk_widget_unparent);
  g_free (priv->rubberband);
  priv->rubberband = NULL;

  remove_autoscroll (self);
}

static void
gtk_list_base_update_rubberband_selection (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkListTile *tile;
  GdkRectangle rect;
  guint pos;
  GtkBitset *rubberband_selection;

  if (!gtk_list_base_get_rubberband_coords (self, &rect))
    return;

  rubberband_selection = gtk_list_base_get_items_in_rect (self, &rect);

  pos = 0;
  for (tile = gtk_list_item_manager_get_first (priv->item_manager);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      if (tile->widget)
        {
          if (gtk_bitset_contains (rubberband_selection, pos))
            gtk_widget_set_state_flags (tile->widget, GTK_STATE_FLAG_ACTIVE, FALSE);
          else
            gtk_widget_unset_state_flags (tile->widget, GTK_STATE_FLAG_ACTIVE);
        }

      pos += tile->n_items;
    }

  gtk_bitset_unref (rubberband_selection);
}

static void
gtk_list_base_update_rubberband (GtkListBase *self,
                                 double       x,
                                 double       y)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (!priv->rubberband)
    return;

  priv->rubberband->pointer_x = x;
  priv->rubberband->pointer_y = y;

  gtk_list_base_update_rubberband_selection (self);

  update_autoscroll (self, x, y);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
get_selection_modifiers (GtkGesture *gesture,
                         gboolean   *modify,
                         gboolean   *extend)
{
  GdkEventSequence *sequence;
  GdkEvent *event;
  GdkModifierType state;

  *modify = FALSE;
  *extend = FALSE;

  sequence = gtk_gesture_get_last_updated_sequence (gesture);
  event = gtk_gesture_get_last_event (gesture, sequence);
  state = gdk_event_get_modifier_state (event);
  if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
    *modify = TRUE;
  if ((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
    *extend = TRUE;
}

static void
gtk_list_base_drag_update (GtkGestureDrag *gesture,
                           double          offset_x,
                           double          offset_y,
                           GtkListBase    *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  double start_x, start_y;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

  if (!priv->rubberband)
    {
      if (!gtk_drag_check_threshold_double (GTK_WIDGET (self), 0, 0, offset_x, offset_y))
        return;

      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
      gtk_list_base_start_rubberband (self, start_x, start_y);
    }
  gtk_list_base_update_rubberband (self, start_x + offset_x, start_y + offset_y);
}

static void
gtk_list_base_drag_end (GtkGestureDrag *gesture,
                        double          offset_x,
                        double          offset_y,
                        GtkListBase    *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GdkEventSequence *sequence;
  gboolean modify, extend;

  if (!priv->rubberband)
    return;

  sequence = gtk_gesture_get_last_updated_sequence (GTK_GESTURE (gesture));
  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    {
      gtk_list_base_stop_rubberband (self);
      return;
    }

  gtk_list_base_drag_update (gesture, offset_x, offset_y, self);
  get_selection_modifiers (GTK_GESTURE (gesture), &modify, &extend);
  gtk_list_base_apply_rubberband_selection (self, modify, extend);
  gtk_list_base_stop_rubberband (self);
}

void
gtk_list_base_set_enable_rubberband (GtkListBase *self,
                                     gboolean     enable)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->enable_rubberband == enable)
    return;

  priv->enable_rubberband = enable;

  if (enable)
    {
      priv->drag_gesture = gtk_gesture_drag_new ();
      g_signal_connect (priv->drag_gesture, "drag-update", G_CALLBACK (gtk_list_base_drag_update), self);
      g_signal_connect (priv->drag_gesture, "drag-end", G_CALLBACK (gtk_list_base_drag_end), self);
      gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (priv->drag_gesture));
    }
  else
    {
      gtk_widget_remove_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (priv->drag_gesture));
      priv->drag_gesture = NULL;
    }
}

gboolean
gtk_list_base_get_enable_rubberband (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->enable_rubberband;
}

static void
gtk_list_base_drag_motion (GtkDropControllerMotion *motion,
                           double                   x,
                           double                   y,
                           gpointer                 unused)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));

  update_autoscroll (GTK_LIST_BASE (widget), x, y);
}

static void
gtk_list_base_drag_leave (GtkDropControllerMotion *motion,
                          gpointer                 unused)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));

  remove_autoscroll (GTK_LIST_BASE (widget));
}

static GtkListTile *
gtk_list_base_split_func (GtkWidget   *widget,
                          GtkListTile *tile,
                          guint        n_items)
{
  return GTK_LIST_BASE_GET_CLASS (widget)->split (GTK_LIST_BASE (widget), tile, n_items);
}

static GtkListItemBase *
gtk_list_base_create_list_widget_func (GtkWidget *widget)
{
  return GTK_LIST_BASE_GET_CLASS (widget)->create_list_widget (GTK_LIST_BASE (widget));
}

static void
gtk_list_base_prepare_section_func (GtkWidget   *widget,
                                    GtkListTile *tile,
                                    guint        pos)
{
  GTK_LIST_BASE_GET_CLASS (widget)->prepare_section (GTK_LIST_BASE (widget), tile, pos);
}

static GtkListHeaderBase *
gtk_list_base_create_header_widget_func (GtkWidget *widget)
{
  return GTK_LIST_BASE_GET_CLASS (widget)->create_header_widget (GTK_LIST_BASE (widget));
}

static void
gtk_list_base_init_real (GtkListBase      *self,
                         GtkListBaseClass *g_class)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkEventController *controller;

  priv->item_manager = gtk_list_item_manager_new (GTK_WIDGET (self),
                                                  gtk_list_base_split_func,
                                                  gtk_list_base_create_list_widget_func,
                                                  gtk_list_base_prepare_section_func,
                                                  gtk_list_base_create_header_widget_func);
  priv->anchor = gtk_list_item_tracker_new (priv->item_manager);
  priv->anchor_side_along = GTK_PACK_START;
  priv->anchor_side_across = GTK_PACK_START;
  priv->selected = gtk_list_item_tracker_new (priv->item_manager);
  priv->focus = gtk_list_item_tracker_new (priv->item_manager);

  priv->adjustment[GTK_ORIENTATION_HORIZONTAL] = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  g_object_ref_sink (priv->adjustment[GTK_ORIENTATION_HORIZONTAL]);
  priv->adjustment[GTK_ORIENTATION_VERTICAL] = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  g_object_ref_sink (priv->adjustment[GTK_ORIENTATION_VERTICAL]);

  priv->tab_behavior = GTK_LIST_TAB_ALL;
  priv->orientation = GTK_ORIENTATION_VERTICAL;

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  controller = gtk_drop_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (gtk_list_base_drag_motion), NULL);
  g_signal_connect (controller, "leave", G_CALLBACK (gtk_list_base_drag_leave), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

static void
gtk_list_base_set_adjustment_values (GtkListBase    *self,
                                     GtkOrientation  orientation,
                                     int             value,
                                     int             size,
                                     int             page_size)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  size = MAX (size, page_size);
  value = MAX (value, 0);
  value = MIN (value, size - page_size);

  g_signal_handlers_block_by_func (priv->adjustment[orientation],
                                   gtk_list_base_adjustment_value_changed_cb,
                                   self);
  gtk_adjustment_configure (priv->adjustment[orientation],
                            gtk_list_base_adjustment_is_flipped (self, orientation)
                              ? size - page_size - value
                              : value,
                            0,
                            size,
                            page_size * 0.1,
                            page_size * 0.9,
                            page_size);
  g_signal_handlers_unblock_by_func (priv->adjustment[orientation],
                                     gtk_list_base_adjustment_value_changed_cb,
                                     self);
}

static void
gtk_list_base_update_adjustments (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GdkRectangle bounds;
  int value_along, value_across;
  int page_along, page_across;
  guint pos;

  gtk_list_item_manager_get_tile_bounds (priv->item_manager, &bounds);
  g_assert (bounds.x == 0);
  g_assert (bounds.y == 0);

  page_across = gtk_widget_get_size (GTK_WIDGET (self), OPPOSITE_ORIENTATION (priv->orientation));
  page_along = gtk_widget_get_size (GTK_WIDGET (self), priv->orientation);

  pos = gtk_list_item_tracker_get_position (priv->item_manager, priv->anchor);
  if (pos == GTK_INVALID_LIST_POSITION)
    {
      value_across = 0;
      value_along = 0;
    }
  else
    {
      GdkRectangle area;

      if (gtk_list_base_get_allocation (self, pos, &area))
        {
          value_across = area.x;
          value_along = area.y;
          if (priv->anchor_side_across == GTK_PACK_END)
            value_across += area.width;
          if (priv->anchor_side_along == GTK_PACK_END)
            value_along += area.height;
          value_across -= priv->anchor_align_across * page_across;
          value_along -= priv->anchor_align_along * page_along;
        }
      else
        {
          value_across = 0;
          value_along = 0;
        }
    }

  gtk_list_base_set_adjustment_values (self,
                                       OPPOSITE_ORIENTATION (priv->orientation),
                                       value_across,
                                       bounds.width,
                                       page_across);
  gtk_list_base_set_adjustment_values (self,
                                       priv->orientation,
                                       value_along,
                                       bounds.height,
                                       page_along);
}

void
gtk_list_base_allocate (GtkListBase *self)
{
  GtkCssBoxes boxes;

  gtk_css_boxes_init (&boxes, GTK_WIDGET (self));

  gtk_list_base_update_adjustments (self);

  gtk_list_base_allocate_children (self, &boxes);
  gtk_list_base_allocate_rubberband (self, &boxes);
}

GtkScrollablePolicy
gtk_list_base_get_scroll_policy (GtkListBase    *self,
                                 GtkOrientation  orientation)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->scroll_policy[orientation];
}

GtkOrientation
gtk_list_base_get_orientation (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->orientation;
}

void
gtk_list_base_get_border_spacing (GtkListBase *self,
                                  int         *xspacing,
                                  int         *yspacing)
{
  GtkCssStyle *style = gtk_css_node_get_style (gtk_widget_get_css_node (GTK_WIDGET (self)));
  GtkCssValue *border_spacing = style->size->border_spacing;

  if (gtk_list_base_get_orientation (self) == GTK_ORIENTATION_HORIZONTAL)
    {
      if (xspacing)
        *xspacing = _gtk_css_position_value_get_y (border_spacing, 0);
      if (yspacing)
        *yspacing = _gtk_css_position_value_get_x (border_spacing, 0);
    }
  else
    {
      if (xspacing)
        *xspacing = _gtk_css_position_value_get_x (border_spacing, 0);
      if (yspacing)
        *yspacing = _gtk_css_position_value_get_y (border_spacing, 0);
    }
}

GtkListItemManager *
gtk_list_base_get_manager (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->item_manager;
}

guint
gtk_list_base_get_anchor (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return gtk_list_item_tracker_get_position (priv->item_manager,
                                             priv->anchor);
}

/*
 * gtk_list_base_set_anchor:
 * @self: a `GtkListBase`
 * @anchor_pos: position of the item to anchor
 * @anchor_align_across: how far in the across direction to anchor
 * @anchor_side_across: if the anchor should side to start or end of item
 * @anchor_align_along: how far in the along direction to anchor
 * @anchor_side_along: if the anchor should side to start or end of item
 *
 * Sets the anchor.
 * The anchor is the item that is always kept on screen.
 *
 * In each dimension, anchoring uses 2 variables: The side of the
 * item that gets anchored - either start or end - and where in
 * the widget's allocation it should get anchored - here 0.0 means
 * the start of the widget and 1.0 is the end of the widget.
 * It is allowed to use values outside of this range. In particular,
 * this is necessary when the items are larger than the list's
 * allocation.
 *
 * Using this information, the adjustment's value and in turn widget
 * offsets will then be computed. If the anchor is too far off, it
 * will be clamped so that there are always visible items on screen.
 *
 * Making anchoring this complicated ensures that one item - one
 * corner of one item to be exact - always stays at the same place
 * (usually this item is the focused item). So when the list undergoes
 * heavy changes (like sorting, filtering, removals, additions), this
 * item will stay in place while everything around it will shuffle
 * around.
 *
 * The anchor will also ensure that enough widgets are created according
 * to gtk_list_base_set_anchor_max_widgets().
 **/
void
gtk_list_base_set_anchor (GtkListBase *self,
                          guint        anchor_pos,
                          double       anchor_align_across,
                          GtkPackType  anchor_side_across,
                          double       anchor_align_along,
                          GtkPackType  anchor_side_along)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  guint items_before;

  items_before = round (priv->center_widgets * CLAMP (anchor_align_along, 0, 1));
  gtk_list_item_tracker_set_position (priv->item_manager,
                                      priv->anchor,
                                      anchor_pos,
                                      items_before + priv->above_below_widgets,
                                      priv->center_widgets - items_before + priv->above_below_widgets);

  priv->anchor_align_across = anchor_align_across;
  priv->anchor_side_across = anchor_side_across;
  priv->anchor_align_along = anchor_align_along;
  priv->anchor_side_along = anchor_side_along;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

/**
 * gtk_list_base_set_anchor_max_widgets:
 * @self: a `GtkListBase`
 * @center: the number of widgets in the middle
 * @above_below: extra widgets above and below
 *
 * Sets how many widgets should be kept alive around the anchor.
 * The number of these widgets determines how many items can be
 * displayed and must be chosen to be large enough to cover the
 * allocation but should be kept as small as possible for
 * performance reasons.
 *
 * There will be @center widgets allocated around the anchor
 * evenly distributed according to the anchor's alignment - if
 * the anchor is at the start, all these widgets will be allocated
 * behind it, if it's at the end, all the widgets will be allocated
 * in front of it.
 *
 * Additionally, there will be @above_below widgets allocated both
 * before and after the center widgets, so the total number of
 * widgets kept alive is 2 * above_below + center + 1.
 **/
void
gtk_list_base_set_anchor_max_widgets (GtkListBase *self,
                                      guint        n_center,
                                      guint        n_above_below)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  priv->center_widgets = n_center;
  priv->above_below_widgets = n_above_below;

  gtk_list_base_set_anchor (self,
                            gtk_list_item_tracker_get_position (priv->item_manager, priv->anchor),
                            priv->anchor_align_across,
                            priv->anchor_side_across,
                            priv->anchor_align_along,
                            priv->anchor_side_along);
}

GtkSelectionModel *
gtk_list_base_get_model (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->model;
}

gboolean
gtk_list_base_set_model (GtkListBase       *self,
                         GtkSelectionModel *model)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->model == model)
    return FALSE;

  g_clear_object (&priv->model);

  if (model)
    {
      priv->model = g_object_ref (model);
      gtk_list_item_manager_set_model (priv->item_manager, model);
      gtk_list_base_set_anchor (self, 0, 0.0, GTK_PACK_START, 0.0, GTK_PACK_START);
    }
  else
    {
      gtk_list_item_manager_set_model (priv->item_manager, NULL);
    }

  return TRUE;
}

void
gtk_list_base_set_tab_behavior (GtkListBase        *self,
                                GtkListTabBehavior  behavior)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  priv->tab_behavior = behavior;
}

GtkListTabBehavior
gtk_list_base_get_tab_behavior (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->tab_behavior;
}

void
gtk_list_base_scroll_to (GtkListBase        *self,
                         guint               pos,
                         GtkListScrollFlags  flags,
                         GtkScrollInfo      *scroll)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (flags & GTK_LIST_SCROLL_FOCUS)
    {
      GtkListItemTracker *old_focus;

      /* We need a tracker here to keep the focus widget around,
       * because we need to update the focus tracker before grabbing
       * focus, because otherwise gtk_list_base_set_focus_child() will
       * scroll to the item, and we want to avoid that.
       */
      old_focus = gtk_list_item_tracker_new (priv->item_manager);
      gtk_list_item_tracker_set_position (priv->item_manager, old_focus, gtk_list_base_get_focus_position (self), 0, 0);

      gtk_list_item_tracker_set_position (priv->item_manager, priv->focus, pos, 0, 0);

      /* XXX: Is this the proper check? */
      if (gtk_widget_get_state_flags (GTK_WIDGET (self)) & GTK_STATE_FLAG_FOCUS_WITHIN)
        {
          GtkListTile *tile = gtk_list_item_manager_get_nth (priv->item_manager, pos, NULL);

          gtk_widget_grab_focus (tile->widget);
        }

      gtk_list_item_tracker_free (priv->item_manager, old_focus);
    }

  if (flags & GTK_LIST_SCROLL_SELECT)
    gtk_list_base_select_item (self, pos, FALSE, FALSE);

  gtk_list_base_scroll_to_item (self, pos, scroll);
}

