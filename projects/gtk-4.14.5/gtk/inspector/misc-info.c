/*
 * Copyright (c) 2014 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>

#include "misc-info.h"

#include "measuregraph.h"
#include "window.h"
#include "type-info.h"

#include "gtktypebuiltins.h"
#include "gtkbuildable.h"
#include "gtklabel.h"
#include "gtkframe.h"
#include "gtkbutton.h"
#include "gtkmenubutton.h"
#include "gtkwidgetprivate.h"
#include "gtkbinlayout.h"
#include "gtkwidgetprivate.h"

struct _GtkInspectorMiscInfo
{
  GtkWidget parent;

  GObject *object;

  GtkWidget *swin;
  GtkWidget *address;
  GtkWidget *type;
  GtkWidget *type_popover;
  GtkWidget *refcount_row;
  GtkWidget *refcount;
  GtkWidget *state_row;
  GtkWidget *state;
  GtkWidget *direction_row;
  GtkWidget *direction;
  GtkWidget *buildable_id_row;
  GtkWidget *buildable_id;
  GtkWidget *mnemonic_label_row;
  GtkWidget *mnemonic_label;
  GtkWidget *request_mode_row;
  GtkWidget *request_mode;
  GtkWidget *measure_info_row;
  GtkWidget *measure_row;
  GtkWidget *measure_expand_toggle;
  GtkWidget *measure_picture;
  GdkPaintable *measure_graph;
  GtkWidget *bounds_row;
  GtkWidget *bounds;
  GtkWidget *baseline_row;
  GtkWidget *baseline;
  GtkWidget *surface_row;
  GtkWidget *surface;
  GtkWidget *surface_button;
  GtkWidget *renderer_row;
  GtkWidget *renderer;
  GtkWidget *renderer_button;
  GtkWidget *frame_clock_row;
  GtkWidget *frame_clock;
  GtkWidget *frame_clock_button;
  GtkWidget *tick_callback_row;
  GtkWidget *tick_callback;
  GtkWidget *framerate_row;
  GtkWidget *framerate;
  GtkWidget *scale_row;
  GtkWidget *scale;
  GtkWidget *framecount_row;
  GtkWidget *framecount;
  GtkWidget *mapped_row;
  GtkWidget *mapped;
  GtkWidget *realized_row;
  GtkWidget *realized;
  GtkWidget *is_toplevel_row;
  GtkWidget *is_toplevel;
  GtkWidget *child_visible_row;
  GtkWidget *child_visible;

  guint update_source_id;
  gint64 last_frame;
};

typedef struct _GtkInspectorMiscInfoClass
{
  GtkWidgetClass parent_class;
} GtkInspectorMiscInfoClass;

G_DEFINE_TYPE (GtkInspectorMiscInfo, gtk_inspector_misc_info, GTK_TYPE_WIDGET)

static char *
format_state_flags (GtkStateFlags state)
{
  GFlagsClass *fclass;
  GString *str;
  int i;

  str = g_string_new ("");

  if (state)
    {
      fclass = g_type_class_ref (GTK_TYPE_STATE_FLAGS);
      for (i = 0; i < fclass->n_values; i++)
        {
          if (state & fclass->values[i].value)
            {
              if (str->len)
                g_string_append (str, " | ");
              g_string_append (str, fclass->values[i].value_nick);
            }
        }
      g_type_class_unref (fclass);
    }
  else
    g_string_append (str, "normal");

  return g_string_free (str, FALSE);
}

static void
state_flags_changed (GtkWidget *w, GtkStateFlags old_flags, GtkInspectorMiscInfo *sl)
{
  char *s;

  s = format_state_flags (gtk_widget_get_state_flags (w));
  gtk_label_set_label (GTK_LABEL (sl->state), s);
  g_free (s);
}

static void
update_measure_picture (GtkPicture      *picture,
                        GtkToggleButton *toggle)
{
  GdkPaintable *paintable = gtk_picture_get_paintable (picture);

  if (gtk_toggle_button_get_active (toggle) ||
      (gdk_paintable_get_intrinsic_width (paintable) <= 200 &&
       gdk_paintable_get_intrinsic_height (paintable) <= 100))
    {
      gtk_picture_set_can_shrink (picture, FALSE);
      gtk_widget_set_size_request (GTK_WIDGET (picture), -1, -1);
    }
  else
    {
      gtk_picture_set_can_shrink (picture, TRUE);
      gtk_widget_set_size_request (GTK_WIDGET (picture),
                                   -1,
                                   MIN (100, 200 / gdk_paintable_get_intrinsic_aspect_ratio (paintable)));
    }
}

static void
measure_graph_measure (GtkInspectorMiscInfo *sl)
{
  if (gtk_widget_get_visible (sl->measure_row))
    gtk_inspector_measure_graph_measure (GTK_INSPECTOR_MEASURE_GRAPH (sl->measure_graph), GTK_WIDGET (sl->object));

  update_measure_picture (GTK_PICTURE (sl->measure_picture), GTK_TOGGLE_BUTTON (sl->measure_expand_toggle));
}

static void
update_allocation (GtkWidget            *w,
                   GtkInspectorMiscInfo *sl)
{
  graphene_rect_t bounds;
  char *size_label;
  GEnumClass *class;
  GEnumValue *value;

  if (!gtk_widget_compute_bounds (w, gtk_widget_get_parent (w), &bounds))
    graphene_rect_init (&bounds, 0, 0, 0, 0);

  size_label = g_strdup_printf ("%g × %g +%g +%g",
                                bounds.size.width, bounds.size.height,
                                bounds.origin.x, bounds.origin.y);

  gtk_label_set_label (GTK_LABEL (sl->bounds), size_label);
  g_free (size_label);

  size_label = g_strdup_printf ("%d", gtk_widget_get_baseline (w));
  gtk_label_set_label (GTK_LABEL (sl->baseline), size_label);
  g_free (size_label);

  class = G_ENUM_CLASS (g_type_class_ref (GTK_TYPE_SIZE_REQUEST_MODE));
  value = g_enum_get_value (class, gtk_widget_get_request_mode (w));
  gtk_label_set_label (GTK_LABEL (sl->request_mode), value->value_nick);
  g_type_class_unref (class);

  measure_graph_measure (sl);
}

static void
disconnect_each_other (gpointer  still_alive,
                       GObject  *for_science)
{
  if (GTK_INSPECTOR_IS_MISC_INFO (still_alive))
    {
      GtkInspectorMiscInfo *self = GTK_INSPECTOR_MISC_INFO (still_alive);
      self->object = NULL;
    }

  g_signal_handlers_disconnect_matched (still_alive, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, for_science);
  g_object_weak_unref (still_alive, disconnect_each_other, for_science);
}

static void
show_mnemonic_label (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GtkInspectorWindow *iw;
  GtkWidget *widget;

  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (sl), GTK_TYPE_INSPECTOR_WINDOW));

  widget = g_object_get_data (G_OBJECT (button), "mnemonic-label");
  if (widget)
    gtk_inspector_window_push_object (iw, G_OBJECT (widget), CHILD_KIND_OTHER, 0);
}

static void
show_surface (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GtkInspectorWindow *iw;
  GObject *surface;

  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (sl), GTK_TYPE_INSPECTOR_WINDOW));

  surface = (GObject *)gtk_native_get_surface (GTK_NATIVE (sl->object));
  if (surface)
    gtk_inspector_window_push_object (iw, surface, CHILD_KIND_OTHER, 0);
}

static void
show_renderer (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GtkInspectorWindow *iw;
  GObject *renderer;

  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (sl), GTK_TYPE_INSPECTOR_WINDOW));

  renderer = (GObject *)gtk_native_get_renderer (GTK_NATIVE (sl->object));
  if (renderer)
    gtk_inspector_window_push_object (iw, renderer, CHILD_KIND_OTHER, 0);
}

static void
show_frame_clock (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GtkInspectorWindow *iw;
  GObject *clock;

  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (sl), GTK_TYPE_INSPECTOR_WINDOW));

  clock = (GObject *)gtk_widget_get_frame_clock (GTK_WIDGET (sl->object));
  if (clock)
    gtk_inspector_window_push_object (iw, clock, CHILD_KIND_OTHER, 0);
}

static void
update_surface (GtkInspectorMiscInfo *sl)
{
  gtk_widget_set_visible (sl->surface_row, GTK_IS_NATIVE (sl->object));
  if (GTK_IS_NATIVE (sl->object))
    {
      GObject *obj;
      char *tmp;

      obj = (GObject *)gtk_native_get_surface (GTK_NATIVE (sl->object));
      tmp = g_strdup_printf ("%p", obj);
      gtk_label_set_label (GTK_LABEL (sl->surface), tmp);
      g_free (tmp);
    }
}

static void
update_renderer (GtkInspectorMiscInfo *sl)
{
  gtk_widget_set_visible (sl->renderer_row, GTK_IS_NATIVE (sl->object));
  if (GTK_IS_NATIVE (sl->object))
    {
      GObject *obj;
      char *tmp;

      obj = (GObject *)gtk_native_get_surface (GTK_NATIVE (sl->object));
      tmp = g_strdup_printf ("%p", obj);
      gtk_label_set_label (GTK_LABEL (sl->renderer), tmp);
      g_free (tmp);
    }
}

static void
update_frame_clock (GtkInspectorMiscInfo *sl)
{
  gtk_widget_set_visible (sl->frame_clock_row, GTK_IS_ROOT (sl->object));
  if (GTK_IS_ROOT (sl->object))
    {
      GObject *clock;

      clock = (GObject *)gtk_widget_get_frame_clock (GTK_WIDGET (sl->object));
      gtk_widget_set_sensitive (sl->frame_clock_button, clock != NULL);
      if (clock)
        {
          char *tmp = g_strdup_printf ("%p", clock);
          gtk_label_set_label (GTK_LABEL (sl->frame_clock), tmp);
          g_free (tmp);
        }
      else
        {
          gtk_label_set_label (GTK_LABEL (sl->frame_clock), "NULL");
        }
    }
}

static void
update_direction (GtkInspectorMiscInfo *sl)
{
  GtkWidget *widget = GTK_WIDGET (sl->object);

  switch (widget->priv->direction)
    {
    case GTK_TEXT_DIR_LTR:
      gtk_label_set_label (GTK_LABEL (sl->direction), "Left-to-Right");
      break;
    case GTK_TEXT_DIR_RTL:
      gtk_label_set_label (GTK_LABEL (sl->direction), "Right-to-Left");
      break;
    case GTK_TEXT_DIR_NONE:
      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
        gtk_label_set_label (GTK_LABEL (sl->direction), "Left-to-Right (inherited)");
      else
        gtk_label_set_label (GTK_LABEL (sl->direction), "Right-to-Left (inherited)");
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
update_info (gpointer data)
{
  GtkInspectorMiscInfo *sl = data;
  char *tmp;
  GType gtype;

  tmp = g_strdup_printf ("%p", sl->object);
  gtk_label_set_text (GTK_LABEL (sl->address), tmp);
  g_free (tmp);

  gtype = G_TYPE_FROM_INSTANCE (sl->object);

  gtk_menu_button_set_label (GTK_MENU_BUTTON (sl->type), g_type_name (gtype));
  gtk_inspector_type_popover_set_gtype (GTK_INSPECTOR_TYPE_POPOVER (sl->type_popover),
                                        gtype);

  if (G_IS_OBJECT (sl->object))
    {
      tmp = g_strdup_printf ("%d", sl->object->ref_count);
      gtk_label_set_text (GTK_LABEL (sl->refcount), tmp);
      g_free (tmp);
    }

  if (GTK_IS_WIDGET (sl->object))
    {
      GtkWidget *child;
      GList *list, *l;

      update_direction (sl);

      while ((child = gtk_widget_get_first_child (sl->mnemonic_label)))
        gtk_box_remove (GTK_BOX (sl->mnemonic_label), child);

      list = gtk_widget_list_mnemonic_labels (GTK_WIDGET (sl->object));
      for (l = list; l; l = l->next)
        {
          GtkWidget *button;

          tmp = g_strdup_printf ("%p (%s)", l->data, g_type_name_from_instance ((GTypeInstance*)l->data));
          button = gtk_button_new_with_label (tmp);
          g_free (tmp);
          gtk_box_append (GTK_BOX (sl->mnemonic_label), button);
          g_object_set_data (G_OBJECT (button), "mnemonic-label", l->data);
          g_signal_connect (button, "clicked", G_CALLBACK (show_mnemonic_label), sl);
        }
      g_list_free (list);

      gtk_widget_set_visible (sl->tick_callback, gtk_widget_has_tick_callback (GTK_WIDGET (sl->object)));
      gtk_widget_set_visible (sl->realized, gtk_widget_get_realized (GTK_WIDGET (sl->object)));
      gtk_widget_set_visible (sl->mapped, gtk_widget_get_mapped (GTK_WIDGET (sl->object)));
      gtk_widget_set_visible (sl->is_toplevel, GTK_IS_NATIVE (sl->object));
      gtk_widget_set_visible (sl->child_visible, _gtk_widget_get_child_visible (GTK_WIDGET (sl->object)));
    }

  update_surface (sl);
  update_renderer (sl);
  update_frame_clock (sl);

  if (GTK_IS_BUILDABLE (sl->object))
    {
      gtk_label_set_text (GTK_LABEL (sl->buildable_id),
                          gtk_buildable_get_buildable_id (GTK_BUILDABLE (sl->object)));
    }

  if (GDK_IS_FRAME_CLOCK (sl->object))
    {
      GdkFrameClock *clock;
      gint64 frame;
      gint64 frame_time;
      gint64 history_start;
      gint64 history_len;
      gint64 previous_frame_time;
      GdkFrameTimings *previous_timings;

      clock = GDK_FRAME_CLOCK (sl->object);
      frame = gdk_frame_clock_get_frame_counter (clock);
      frame_time = gdk_frame_clock_get_frame_time (clock);

      tmp = g_strdup_printf ("%"G_GINT64_FORMAT, frame);
      gtk_label_set_label (GTK_LABEL (sl->framecount), tmp);
      g_free (tmp);

      history_start = gdk_frame_clock_get_history_start (clock);
      history_len = frame - history_start;

      if (history_len > 0 && sl->last_frame != frame)
        {
          previous_timings = gdk_frame_clock_get_timings (clock, history_start);
          previous_frame_time = gdk_frame_timings_get_frame_time (previous_timings);
          tmp = g_strdup_printf ("%4.1f ⁄ s", (G_USEC_PER_SEC * history_len) / (double) (frame_time - previous_frame_time));
          gtk_label_set_label (GTK_LABEL (sl->framerate), tmp);
          g_free (tmp);
        }
      else
        {
          gtk_label_set_label (GTK_LABEL (sl->framerate), "—");
        }

      sl->last_frame = frame;
    }

  if (GDK_IS_SURFACE (sl->object))
    {
      char buf[64];

      g_snprintf (buf, sizeof (buf), "%g", gdk_surface_get_scale (GDK_SURFACE (sl->object)));

      gtk_label_set_label (GTK_LABEL (sl->scale), buf);
    }

  return G_SOURCE_CONTINUE;
}

static GdkContentProvider *
measure_picture_drag_prepare (GtkDragSource *source,
                              double         x,
                              double         y,
                              gpointer       unused)
{
  GtkWidget *picture;
  GdkPaintable *measure_graph;
  GdkTexture *texture;

  picture = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  measure_graph = gtk_picture_get_paintable (GTK_PICTURE (picture));
  if (!GTK_IS_INSPECTOR_MEASURE_GRAPH (measure_graph))
    return NULL;

  texture = gtk_inspector_measure_graph_get_texture (GTK_INSPECTOR_MEASURE_GRAPH (measure_graph));
  if (texture == NULL)
    return NULL;

  return gdk_content_provider_new_typed (GDK_TYPE_TEXTURE, texture);
}

void
gtk_inspector_misc_info_set_object (GtkInspectorMiscInfo *sl,
                                    GObject              *object)
{
  if (sl->object)
    {
      g_signal_handlers_disconnect_by_func (sl->object, state_flags_changed, sl);
      disconnect_each_other (sl->object, G_OBJECT (sl));
      disconnect_each_other (sl, sl->object);
      sl->object = NULL;
    }

  gtk_widget_set_visible (GTK_WIDGET (sl), TRUE);

  sl->object = object;
  g_object_weak_ref (G_OBJECT (sl), disconnect_each_other, object);
  g_object_weak_ref (object, disconnect_each_other, sl);

  gtk_widget_set_visible (sl->refcount_row, G_IS_OBJECT (object));
  gtk_widget_set_visible (sl->state_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->direction_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->request_mode_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->bounds_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->baseline_row, GTK_IS_WIDGET (object));
  /* Don't autoshow, it may be slow, we have a button for this */
  if (!GTK_IS_WIDGET (object))
    gtk_widget_set_visible (sl->measure_row, FALSE);
  gtk_widget_set_visible (sl->measure_info_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->mnemonic_label_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->tick_callback_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->mapped_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->realized_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->is_toplevel_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->child_visible_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->frame_clock_row, GTK_IS_WIDGET (object));
  gtk_widget_set_visible (sl->buildable_id_row, GTK_IS_BUILDABLE (object));
  gtk_widget_set_visible (sl->framecount_row, GDK_IS_FRAME_CLOCK (object));
  gtk_widget_set_visible (sl->framerate_row, GDK_IS_FRAME_CLOCK (object));
  gtk_widget_set_visible (sl->scale_row, GDK_IS_SURFACE (object));

  if (GTK_IS_WIDGET (object))
    {
      g_signal_connect_object (object, "state-flags-changed", G_CALLBACK (state_flags_changed), sl, 0);
      state_flags_changed (GTK_WIDGET (sl->object), 0, sl);

      update_allocation (GTK_WIDGET (sl->object), sl);
      update_measure_picture (GTK_PICTURE (sl->measure_picture), GTK_TOGGLE_BUTTON (sl->measure_expand_toggle));
    }
  else
    {
      gtk_inspector_measure_graph_clear (GTK_INSPECTOR_MEASURE_GRAPH (sl->measure_graph));
    }

  update_info (sl);
}

static void
gtk_inspector_misc_info_init (GtkInspectorMiscInfo *sl)
{
  gtk_widget_init_template (GTK_WIDGET (sl));

  sl->type_popover = g_object_new (GTK_TYPE_INSPECTOR_TYPE_POPOVER, NULL);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (sl->type),
                               sl->type_popover);

}

static void
map (GtkWidget *widget)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (widget);

  GTK_WIDGET_CLASS (gtk_inspector_misc_info_parent_class)->map (widget);

  sl->update_source_id = g_timeout_add_seconds (1, update_info, sl);
  update_info (sl);
}

static void
unmap (GtkWidget *widget)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (widget);

  g_source_remove (sl->update_source_id);
  sl->update_source_id = 0;

  GTK_WIDGET_CLASS (gtk_inspector_misc_info_parent_class)->unmap (widget);
}

static void
dispose (GObject *o)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (o);

  gtk_widget_dispose_template (GTK_WIDGET (sl), GTK_TYPE_INSPECTOR_MISC_INFO);

  G_OBJECT_CLASS (gtk_inspector_misc_info_parent_class)->dispose (o);
}

static void
gtk_inspector_misc_info_class_init (GtkInspectorMiscInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;

  widget_class->map = map;
  widget_class->unmap = unmap;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/misc-info.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, swin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, address);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, type);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, refcount_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, refcount);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, state_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, state);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, direction_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, direction);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, buildable_id_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, buildable_id);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, mnemonic_label_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, mnemonic_label);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, request_mode_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, request_mode);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, measure_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, measure_info_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, measure_expand_toggle);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, measure_picture);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, measure_graph);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, bounds_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, bounds);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, baseline_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, baseline);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, surface_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, surface);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, surface_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, renderer_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, renderer);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, renderer_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, frame_clock_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, frame_clock);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, frame_clock_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, tick_callback_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, tick_callback);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, framecount_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, framecount);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, framerate_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, framerate);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, scale_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, scale);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, mapped_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, mapped);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, realized_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, realized);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, is_toplevel_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, is_toplevel);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, child_visible_row);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorMiscInfo, child_visible);

  gtk_widget_class_bind_template_callback (widget_class, update_measure_picture);
  gtk_widget_class_bind_template_callback (widget_class, measure_picture_drag_prepare);
  gtk_widget_class_bind_template_callback (widget_class, measure_graph_measure);
  gtk_widget_class_bind_template_callback (widget_class, show_surface);
  gtk_widget_class_bind_template_callback (widget_class, show_renderer);
  gtk_widget_class_bind_template_callback (widget_class, show_frame_clock);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

// vim: set et sw=2 ts=2:
