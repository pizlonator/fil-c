/*  Copyright 2023 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include "path-view.h"

struct _PathView
{
  GtkWidget parent_instance;

  GskPath *path;
  GskStroke *stroke;
  graphene_rect_t bounds;
  GskFillRule fill_rule;
  GdkRGBA fg;
  GdkRGBA bg;
  int padding;
  gboolean do_fill;
  gboolean show_points;
  gboolean show_controls;
  GskPath *line_path;
  GskPath *point_path;
  GdkRGBA point_color;
};

enum {
  PROP_PATH = 1,
  PROP_DO_FILL,
  PROP_STROKE,
  PROP_FILL_RULE,
  PROP_FG_COLOR,
  PROP_BG_COLOR,
  PROP_POINT_COLOR,
  PROP_SHOW_POINTS,
  PROP_SHOW_CONTROLS,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

struct _PathViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (PathView, path_view, GTK_TYPE_WIDGET)

static void
path_view_init (PathView *self)
{
  self->do_fill = TRUE;
  self->stroke = gsk_stroke_new (1);
  self->fill_rule = GSK_FILL_RULE_WINDING;
  self->fg = (GdkRGBA) { 0, 0, 0, 1};
  self->bg = (GdkRGBA) { 1, 1, 1, 1};
  self->point_color = (GdkRGBA) { 1, 0, 0, 1};
  self->padding = 10;
}

static void
path_view_dispose (GObject *object)
{
  PathView *self = PATH_VIEW (object);

  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->stroke, gsk_stroke_free);
  g_clear_pointer (&self->line_path, gsk_path_unref);
  g_clear_pointer (&self->point_path, gsk_path_unref);

  G_OBJECT_CLASS (path_view_parent_class)->dispose (object);
}

static void
path_view_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  PathView *self = PATH_VIEW (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_boxed (value, self->path);
      break;

    case PROP_DO_FILL:
      g_value_set_boolean (value, self->do_fill);
      break;

    case PROP_STROKE:
      g_value_set_boxed (value, self->stroke);
      break;

    case PROP_FILL_RULE:
      g_value_set_enum (value, self->fill_rule);
      break;

    case PROP_FG_COLOR:
      g_value_set_boxed (value, &self->fg);
      break;

    case PROP_BG_COLOR:
      g_value_set_boxed (value, &self->bg);
      break;

    case PROP_SHOW_POINTS:
      g_value_set_boolean (value, self->show_points);
      break;

    case PROP_SHOW_CONTROLS:
      g_value_set_boolean (value, self->show_controls);
      break;

    case PROP_POINT_COLOR:
      g_value_set_boxed (value, &self->point_color);
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
update_bounds (PathView *self)
{
  if (self->do_fill)
    gsk_path_get_bounds (self->path, &self->bounds);
  else
    gsk_path_get_stroke_bounds (self->path, self->stroke, &self->bounds);

  if (self->line_path)
    {
      graphene_rect_t bounds;

      gsk_path_get_stroke_bounds (self->line_path, self->stroke, &bounds);
      graphene_rect_union (&bounds, &self->bounds, &self->bounds);
    }

  if (self->point_path)
    {
      graphene_rect_t bounds;

      gsk_path_get_stroke_bounds (self->point_path, self->stroke, &bounds);
      graphene_rect_union (&bounds, &self->bounds, &self->bounds);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

typedef struct
{
  PathView *self;
  GskPathBuilder *line_builder;
  GskPathBuilder *point_builder;
} ControlData;

static gboolean
collect_cb (GskPathOperation        op,
            const graphene_point_t *pts,
            gsize                   n_pts,
            float                   weight,
            gpointer                data)
{
  ControlData *cd = data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      if (cd->point_builder)
        gsk_path_builder_add_circle (cd->point_builder, &pts[0], 4);
      if (cd->line_builder)
        gsk_path_builder_move_to (cd->line_builder, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_LINE:
    case GSK_PATH_CLOSE:
      if (cd->point_builder)
        gsk_path_builder_add_circle (cd->point_builder, &pts[1], 4);
      if (cd->line_builder)
        gsk_path_builder_line_to (cd->line_builder, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
      if (cd->point_builder)
        {
          if (cd->self->show_controls)
            gsk_path_builder_add_circle (cd->point_builder, &pts[1], 3);
          gsk_path_builder_add_circle (cd->point_builder, &pts[2], 4);
        }
      if (cd->line_builder)
        {
          gsk_path_builder_line_to (cd->line_builder, pts[1].x, pts[1].y);
          gsk_path_builder_line_to (cd->line_builder, pts[2].x, pts[2].y);
        }
      break;

    case GSK_PATH_CUBIC:
      if (cd->point_builder)
        {
          if (cd->self->show_controls)
            {
              gsk_path_builder_add_circle (cd->point_builder, &pts[1], 3);
              gsk_path_builder_add_circle (cd->point_builder, &pts[2], 3);
            }
          gsk_path_builder_add_circle (cd->point_builder, &pts[3], 4);
        }
      if (cd->line_builder)
        {
          gsk_path_builder_line_to (cd->line_builder, pts[1].x, pts[1].y);
          gsk_path_builder_line_to (cd->line_builder, pts[2].x, pts[2].y);
          gsk_path_builder_line_to (cd->line_builder, pts[3].x, pts[3].y);
        }
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
update_controls (PathView *self)
{
  ControlData data = { 0, };

  data.self = self;

  g_clear_pointer (&self->line_path, gsk_path_unref);
  g_clear_pointer (&self->point_path, gsk_path_unref);

  if (self->path && self->show_controls)
    data.line_builder = gsk_path_builder_new ();

  if (self->path && (self->show_points || self->show_controls))
    data.point_builder = gsk_path_builder_new ();

  if (data.line_builder || data.point_builder)
    {
      gsk_path_foreach (self->path, -1, collect_cb, &data);

      if (data.line_builder)
        self->line_path = gsk_path_builder_free_to_path (data.line_builder);
      if (data.point_builder)
        self->point_path = gsk_path_builder_free_to_path (data.point_builder);
    }

  update_bounds (self);
}

static void
path_view_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  PathView *self = PATH_VIEW (object);

  switch (prop_id)
    {

    case PROP_PATH:
      g_clear_pointer (&self->path, gsk_path_unref);
      self->path = g_value_dup_boxed (value);
      update_controls (self);
      break;

    case PROP_DO_FILL:
      self->do_fill = g_value_get_boolean (value);
      update_bounds (self);
      break;

    case PROP_STROKE:
      gsk_stroke_free (self->stroke);
      self->stroke = g_value_get_boxed (value);
      update_bounds (self);
      break;

    case PROP_FILL_RULE:
      self->fill_rule = g_value_get_enum (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_FG_COLOR:
      self->fg = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_BG_COLOR:
      self->bg = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

    case PROP_SHOW_POINTS:
      self->show_points = g_value_get_boolean (value);
      update_controls (self);
      break;

    case PROP_SHOW_CONTROLS:
      self->show_controls = g_value_get_boolean (value);
      update_controls (self);
      break;

    case PROP_POINT_COLOR:
      self->point_color = *(GdkRGBA *) g_value_get_boxed (value);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
path_view_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  PathView *self = PATH_VIEW (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = (int) ceilf (self->bounds.size.width) + 2 * self->padding;
  else
    *minimum = *natural = (int) ceilf (self->bounds.size.height) + 2 * self->padding;
}

static void
path_view_snapshot (GtkWidget   *widget,
                    GtkSnapshot *snapshot)
{
  PathView *self = PATH_VIEW (widget);
  graphene_rect_t bounds = self->bounds;

  graphene_rect_inset (&bounds, - self->padding, - self->padding);

  gtk_snapshot_save (snapshot);

  gtk_snapshot_append_color (snapshot, &self->bg, &bounds);

  if (self->do_fill)
    gtk_snapshot_append_fill (snapshot, self->path, self->fill_rule, &self->fg);
  else
    gtk_snapshot_append_stroke (snapshot, self->path, self->stroke, &self->fg);

  if (self->line_path)
    {
      GskStroke *stroke = gsk_stroke_new (1);

      gsk_stroke_set_dash (stroke, (const float[]) { 1, 1 }, 2);
      gtk_snapshot_append_stroke (snapshot, self->line_path, stroke, &self->fg);
    }

  if (self->point_path)
    {
      GskStroke *stroke = gsk_stroke_new (1);

      gtk_snapshot_append_fill (snapshot, self->point_path, GSK_FILL_RULE_WINDING, &self->point_color);
      gtk_snapshot_append_stroke (snapshot, self->point_path, stroke, &self->fg);
    }

  gtk_snapshot_restore (snapshot);
}

static void
path_view_class_init (PathViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = path_view_dispose;
  object_class->get_property = path_view_get_property;
  object_class->set_property = path_view_set_property;

  widget_class->measure = path_view_measure;
  widget_class->snapshot = path_view_snapshot;

  properties[PROP_PATH]
      = g_param_spec_boxed ("path", NULL, NULL,
                            GSK_TYPE_PATH,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_DO_FILL]
      = g_param_spec_boolean ("do-fill", NULL, NULL,
                              TRUE,
                              G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_STROKE]
      = g_param_spec_boxed ("stroke", NULL, NULL,
                            GSK_TYPE_STROKE,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_FILL_RULE]
      = g_param_spec_enum ("fill-rule", NULL, NULL,
                           GSK_TYPE_FILL_RULE,
                           GSK_FILL_RULE_WINDING,
                           G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_FG_COLOR]
      = g_param_spec_boxed ("fg-color", NULL, NULL,
                            GDK_TYPE_RGBA,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_BG_COLOR]
      = g_param_spec_boxed ("bg-color", NULL, NULL,
                            GDK_TYPE_RGBA,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_POINTS]
      = g_param_spec_boolean ("show-points", NULL, NULL,
                              FALSE,
                              G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SHOW_CONTROLS]
      = g_param_spec_boolean ("show-controls", NULL, NULL,
                              FALSE,
                              G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_POINT_COLOR]
      = g_param_spec_boxed ("point-color", NULL, NULL,
                            GDK_TYPE_RGBA,
                            G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

GtkWidget *
path_view_new (GskPath *path)
{
  return g_object_new (PATH_TYPE_VIEW,
                       "path", path,
                       NULL);
}
