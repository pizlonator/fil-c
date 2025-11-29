/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtk_text_view_child.c Copyright (C) 2019 Red Hat, Inc.
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
 */

#include "config.h"

#include "gtkcssnodeprivate.h"
#include "gtkprivate.h"
#include "gtktextview.h"
#include "gtktextviewchildprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

typedef struct
{
  GList      link;
  GtkWidget *widget;
  int        x;
  int        y;
} Overlay;

struct _GtkTextViewChild
{
  GtkWidget          parent_instance;
  GtkTextWindowType  window_type;
  GQueue             overlays;
  int                xoffset;
  int                yoffset;
  GtkWidget         *child;
};

enum {
  PROP_0,
  PROP_WINDOW_TYPE,
  N_PROPS
};

G_DEFINE_TYPE (GtkTextViewChild, gtk_text_view_child, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS];

static Overlay *
overlay_new (GtkWidget *widget,
             int        x,
             int        y)
{
  Overlay *overlay;

  overlay = g_new0 (Overlay, 1);
  overlay->link.data = overlay;
  overlay->widget = g_object_ref (widget);
  overlay->x = x;
  overlay->y = y;

  return overlay;
}

static void
overlay_free (Overlay *overlay)
{
  g_assert (overlay->link.prev == NULL);
  g_assert (overlay->link.next == NULL);

  g_object_unref (overlay->widget);
  g_free (overlay);
}

static void
gtk_text_view_child_remove_overlay (GtkTextViewChild *self,
                                    Overlay          *overlay)
{
  g_queue_unlink (&self->overlays, &overlay->link);
  gtk_widget_unparent (overlay->widget);
  overlay_free (overlay);
}

static Overlay *
gtk_text_view_child_get_overlay (GtkTextViewChild *self,
                                 GtkWidget        *widget)
{
  GList *iter;

  for (iter = self->overlays.head; iter; iter = iter->next)
    {
      Overlay *overlay = iter->data;

      if (overlay->widget == widget)
        return overlay;
    }

  return NULL;
}

void
gtk_text_view_child_add (GtkTextViewChild *self,
                         GtkWidget        *widget)
{
  if (self->child != NULL)
    {
      g_warning ("%s allows a single child and already contains a %s",
                 G_OBJECT_TYPE_NAME (self),
                 G_OBJECT_TYPE_NAME (widget));
      return;
    }

  self->child = g_object_ref (widget);
  gtk_widget_set_parent (widget, GTK_WIDGET (self));
}

void
gtk_text_view_child_remove (GtkTextViewChild *self,
                            GtkWidget        *widget)
{
  if (widget == self->child)
    {
      self->child = NULL;
      gtk_widget_unparent (widget);
      g_object_unref (widget);
    }
  else
    {
      Overlay *overlay = gtk_text_view_child_get_overlay (self, widget);

      if (overlay != NULL)
        gtk_text_view_child_remove_overlay (self, overlay);
    }
}

static void
gtk_text_view_child_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             int             for_size,
                             int            *min_size,
                             int            *nat_size,
                             int            *min_baseline,
                             int            *nat_baseline)
{
  GtkTextViewChild *self = GTK_TEXT_VIEW_CHILD (widget);
  const GList *iter;
  int real_min_size = 0;
  int real_nat_size = 0;

  if (self->child != NULL)
    gtk_widget_measure (self->child,
                        orientation,
                        for_size,
                        &real_min_size,
                        &real_nat_size,
                        NULL,
                        NULL);

  for (iter = self->overlays.head; iter; iter = iter->next)
    {
      Overlay *overlay = iter->data;
      int child_min_size = 0;
      int child_nat_size = 0;

      gtk_widget_measure (overlay->widget,
                          orientation,
                          for_size,
                          &child_min_size,
                          &child_nat_size,
                          NULL,
                          NULL);

      if (child_min_size > real_min_size)
        real_min_size = child_min_size;

      if (child_nat_size > real_nat_size)
        real_nat_size = child_nat_size;
    }

  if (min_size)
    *min_size = real_min_size;

  if (nat_size)
    *nat_size = real_nat_size;

  if (min_baseline)
    *min_baseline = -1;

  if (nat_baseline)
    *nat_baseline = -1;
}

static void
gtk_text_view_child_size_allocate (GtkWidget *widget,
                                   int        width,
                                   int        height,
                                   int        baseline)
{
  GtkTextViewChild *self = GTK_TEXT_VIEW_CHILD (widget);
  GtkRequisition min_req;
  GdkRectangle rect;
  const GList *iter;

  GTK_WIDGET_CLASS (gtk_text_view_child_parent_class)->size_allocate (widget, width, height, baseline);

  if (self->child != NULL)
    {
      rect.x = 0;
      rect.y = 0;
      rect.width = width;
      rect.height = height;

      gtk_widget_size_allocate (self->child, &rect, baseline);
    }

  for (iter = self->overlays.head; iter; iter = iter->next)
    {
      Overlay *overlay = iter->data;

      gtk_widget_get_preferred_size (overlay->widget, &min_req, NULL);

      rect.width = min_req.width;
      rect.height = min_req.height;

      if (self->window_type == GTK_TEXT_WINDOW_TEXT ||
          self->window_type == GTK_TEXT_WINDOW_TOP ||
          self->window_type == GTK_TEXT_WINDOW_BOTTOM)
        rect.x = overlay->x - self->xoffset;
      else
        rect.x = overlay->x;

      if (self->window_type == GTK_TEXT_WINDOW_TEXT ||
          self->window_type == GTK_TEXT_WINDOW_RIGHT ||
          self->window_type == GTK_TEXT_WINDOW_LEFT)
        rect.y = overlay->y - self->yoffset;
      else
        rect.y = overlay->y;

      gtk_widget_size_allocate (overlay->widget, &rect, -1);
    }
}

static void
gtk_text_view_child_snapshot (GtkWidget   *widget,
                              GtkSnapshot *snapshot)
{
  GtkTextViewChild *self = GTK_TEXT_VIEW_CHILD (widget);
  const GList *iter;

  if (self->child)
    gtk_widget_snapshot_child (widget, self->child, snapshot);

  for (iter = self->overlays.head; iter; iter = iter->next)
    {
      Overlay *overlay = iter->data;

      gtk_widget_snapshot_child (widget, overlay->widget, snapshot);
    }
}

static void
gtk_text_view_child_constructed (GObject *object)
{
  GtkTextViewChild *self = GTK_TEXT_VIEW_CHILD (object);
  GtkCssNode *css_node;

  G_OBJECT_CLASS (gtk_text_view_child_parent_class)->constructed (object);

  css_node = gtk_widget_get_css_node (GTK_WIDGET (self));

  switch (self->window_type)
    {
    case GTK_TEXT_WINDOW_LEFT:
      gtk_css_node_set_name (css_node, g_quark_from_static_string ("border"));
      gtk_css_node_add_class (css_node, g_quark_from_static_string ("left"));
      break;

    case GTK_TEXT_WINDOW_RIGHT:
      gtk_css_node_set_name (css_node, g_quark_from_static_string ("border"));
      gtk_css_node_add_class (css_node, g_quark_from_static_string ("right"));
      break;

    case GTK_TEXT_WINDOW_TOP:
      gtk_css_node_set_name (css_node, g_quark_from_static_string ("border"));
      gtk_css_node_add_class (css_node, g_quark_from_static_string ("top"));
      break;

    case GTK_TEXT_WINDOW_BOTTOM:
      gtk_css_node_set_name (css_node, g_quark_from_static_string ("border"));
      gtk_css_node_add_class (css_node, g_quark_from_static_string ("bottom"));
      break;

    case GTK_TEXT_WINDOW_TEXT:
      gtk_css_node_set_name (css_node, g_quark_from_static_string ("child"));
      break;

    case GTK_TEXT_WINDOW_WIDGET:
    default:
      break;
    }
}

static void
gtk_text_view_child_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkTextViewChild *self = GTK_TEXT_VIEW_CHILD (object);

  switch (prop_id)
    {
    case PROP_WINDOW_TYPE:
      g_value_set_enum (value, self->window_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_text_view_child_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkTextViewChild *self = GTK_TEXT_VIEW_CHILD (object);

  switch (prop_id)
    {
    case PROP_WINDOW_TYPE:
      self->window_type = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_text_view_child_dispose (GObject *object)
{
  GtkTextViewChild *self = GTK_TEXT_VIEW_CHILD (object);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_text_view_child_remove (self, child);

  G_OBJECT_CLASS (gtk_text_view_child_parent_class)->dispose (object);
}

static void
gtk_text_view_child_class_init (GtkTextViewChildClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_text_view_child_dispose;
  object_class->constructed = gtk_text_view_child_constructed;
  object_class->get_property = gtk_text_view_child_get_property;
  object_class->set_property = gtk_text_view_child_set_property;

  widget_class->measure = gtk_text_view_child_measure;
  widget_class->size_allocate = gtk_text_view_child_size_allocate;
  widget_class->snapshot = gtk_text_view_child_snapshot;

  /**
   * GtkTextViewChild:window-type:
   *
   * The "window-type" property is the `GtkTextWindowType` of the
   * `GtkTextView` that the child is attached.
   */
  properties[PROP_WINDOW_TYPE] =
    g_param_spec_enum ("window-type", NULL, NULL,
                       GTK_TYPE_TEXT_WINDOW_TYPE,
                       GTK_TEXT_WINDOW_TEXT,
                       GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_text_view_child_init (GtkTextViewChild *self)
{
  self->window_type = GTK_TEXT_WINDOW_TEXT;

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

GtkWidget *
gtk_text_view_child_new (GtkTextWindowType window_type)
{
  g_return_val_if_fail (window_type == GTK_TEXT_WINDOW_LEFT ||
                        window_type == GTK_TEXT_WINDOW_RIGHT ||
                        window_type == GTK_TEXT_WINDOW_TOP ||
                        window_type == GTK_TEXT_WINDOW_BOTTOM ||
                        window_type == GTK_TEXT_WINDOW_TEXT,
                        NULL);

  return g_object_new (GTK_TYPE_TEXT_VIEW_CHILD,
                       "window-type", window_type,
                       NULL);
}

void
gtk_text_view_child_add_overlay (GtkTextViewChild *self,
                                 GtkWidget        *widget,
                                 int               xpos,
                                 int               ypos)
{
  Overlay *overlay;

  g_return_if_fail (GTK_IS_TEXT_VIEW_CHILD (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  overlay = overlay_new (widget, xpos, ypos);
  g_queue_push_tail (&self->overlays, &overlay->link);
  gtk_widget_set_parent (widget, GTK_WIDGET (self));
}

void
gtk_text_view_child_move_overlay (GtkTextViewChild *self,
                                  GtkWidget        *widget,
                                  int               xpos,
                                  int               ypos)
{
  Overlay *overlay;

  g_return_if_fail (GTK_IS_TEXT_VIEW_CHILD (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  overlay = gtk_text_view_child_get_overlay (self, widget);

  if (overlay != NULL)
    {
      overlay->x = xpos;
      overlay->y = ypos;

      if (gtk_widget_get_visible (GTK_WIDGET (self)) &&
          gtk_widget_get_visible (widget))
        gtk_widget_queue_allocate (GTK_WIDGET (self));
    }
}

GtkTextWindowType
gtk_text_view_child_get_window_type (GtkTextViewChild *self)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW_CHILD (self), 0);

  return self->window_type;
}

void
gtk_text_view_child_set_offset (GtkTextViewChild *self,
                                int               xoffset,
                                int               yoffset)
{
  gboolean changed = FALSE;

  g_return_if_fail (GTK_IS_TEXT_VIEW_CHILD (self));

  if (self->window_type == GTK_TEXT_WINDOW_TEXT ||
      self->window_type == GTK_TEXT_WINDOW_TOP ||
      self->window_type == GTK_TEXT_WINDOW_BOTTOM)
    {
      if (self->xoffset != xoffset)
        {
          self->xoffset = xoffset;
          changed = TRUE;
        }
    }

  if (self->window_type == GTK_TEXT_WINDOW_TEXT ||
      self->window_type == GTK_TEXT_WINDOW_LEFT ||
      self->window_type == GTK_TEXT_WINDOW_RIGHT)
    {
      if (self->yoffset != yoffset)
        {
          self->yoffset = yoffset;
          changed = TRUE;
        }
    }

  if (changed)
    gtk_widget_queue_draw (GTK_WIDGET (self));
}
