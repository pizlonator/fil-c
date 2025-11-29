/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkscale.h"

#include "gtkadjustment.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkgizmoprivate.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkrangeprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#include <math.h>
#include <stdlib.h>


/**
 * GtkScale:
 *
 * A `GtkScale` is a slider control used to select a numeric value.
 *
 * ![An example GtkScale](scales.png)
 *
 * To use it, you’ll probably want to investigate the methods on its base
 * class, [class@Gtk.Range], in addition to the methods for `GtkScale` itself.
 * To set the value of a scale, you would normally use [method@Gtk.Range.set_value].
 * To detect changes to the value, you would normally use the
 * [signal@Gtk.Range::value-changed] signal.
 *
 * Note that using the same upper and lower bounds for the `GtkScale` (through
 * the `GtkRange` methods) will hide the slider itself. This is useful for
 * applications that want to show an undeterminate value on the scale, without
 * changing the layout of the application (such as movie or music players).
 *
 * # GtkScale as GtkBuildable
 *
 * `GtkScale` supports a custom `<marks>` element, which can contain multiple
 * `<mark\>` elements. The “value” and “position” attributes have the same
 * meaning as [method@Gtk.Scale.add_mark] parameters of the same name. If
 * the element is not empty, its content is taken as the markup to show at
 * the mark. It can be translated with the usual ”translatable” and
 * “context” attributes.
 *
 * # CSS nodes
 *
 * ```
 * scale[.fine-tune][.marks-before][.marks-after]
 * ├── [value][.top][.right][.bottom][.left]
 * ├── marks.top
 * │   ├── mark
 * │   ┊    ├── [label]
 * │   ┊    ╰── indicator
 * ┊   ┊
 * │   ╰── mark
 * ├── marks.bottom
 * │   ├── mark
 * │   ┊    ├── indicator
 * │   ┊    ╰── [label]
 * ┊   ┊
 * │   ╰── mark
 * ╰── trough
 *     ├── [fill]
 *     ├── [highlight]
 *     ╰── slider
 * ```
 *
 * `GtkScale` has a main CSS node with name scale and a subnode for its contents,
 * with subnodes named trough and slider.
 *
 * The main node gets the style class .fine-tune added when the scale is in
 * 'fine-tuning' mode.
 *
 * If the scale has an origin (see [method@Gtk.Scale.set_has_origin]), there is
 * a subnode with name highlight below the trough node that is used for rendering
 * the highlighted part of the trough.
 *
 * If the scale is showing a fill level (see [method@Gtk.Range.set_show_fill_level]),
 * there is a subnode with name fill below the trough node that is used for
 * rendering the filled in part of the trough.
 *
 * If marks are present, there is a marks subnode before or after the trough
 * node, below which each mark gets a node with name mark. The marks nodes get
 * either the .top or .bottom style class.
 *
 * The mark node has a subnode named indicator. If the mark has text, it also
 * has a subnode named label. When the mark is either above or left of the
 * scale, the label subnode is the first when present. Otherwise, the indicator
 * subnode is the first.
 *
 * The main CSS node gets the 'marks-before' and/or 'marks-after' style classes
 * added depending on what marks are present.
 *
 * If the scale is displaying the value (see [property@Gtk.Scale:draw-value]),
 * there is subnode with name value. This node will get the .top or .bottom style
 * classes similar to the marks node.
 *
 * # Accessibility
 *
 * `GtkScale` uses the %GTK_ACCESSIBLE_ROLE_SLIDER role.
 */


#define	MAX_DIGITS	(64)	/* don't change this,
				 * a) you don't need to and
				 * b) you might cause buffer overflows in
				 *    unrelated code portions otherwise
				 */

typedef struct _GtkScaleMark GtkScaleMark;

typedef struct _GtkScalePrivate       GtkScalePrivate;
struct _GtkScalePrivate
{
  GSList       *marks;

  GtkWidget    *value_widget;
  GtkWidget    *top_marks_widget;
  GtkWidget    *bottom_marks_widget;

  int           digits;

  guint         draw_value : 1;
  guint         value_pos  : 2;
  guint         has_markup : 1;

  GtkScaleFormatValueFunc format_value_func;
  gpointer format_value_func_user_data;
  GDestroyNotify format_value_func_destroy_notify;
};

struct _GtkScaleMark
{
  double           value;
  int              stop_position;
  GtkPositionType  position; /* always GTK_POS_TOP or GTK_POS_BOTTOM */
  char            *markup;
  GtkWidget       *label_widget;
  GtkWidget       *indicator_widget;
  GtkWidget       *widget;
};

enum {
  PROP_0,
  PROP_DIGITS,
  PROP_DRAW_VALUE,
  PROP_HAS_ORIGIN,
  PROP_VALUE_POS,
  LAST_PROP
};


static GParamSpec *properties[LAST_PROP];

static void     gtk_scale_set_property            (GObject        *object,
                                                   guint           prop_id,
                                                   const GValue   *value,
                                                   GParamSpec     *pspec);
static void     gtk_scale_get_property            (GObject        *object,
                                                   guint           prop_id,
                                                   GValue         *value,
                                                   GParamSpec     *pspec);
static void     gtk_scale_measure (GtkWidget      *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline);
static void     gtk_scale_get_range_border        (GtkRange       *range,
                                                   GtkBorder      *border);
static void     gtk_scale_finalize                (GObject        *object);
static void     gtk_scale_real_get_layout_offsets (GtkScale       *scale,
                                                   int            *x,
                                                   int            *y);
static void     gtk_scale_buildable_interface_init   (GtkBuildableIface  *iface);
static gboolean gtk_scale_buildable_custom_tag_start (GtkBuildable       *buildable,
                                                      GtkBuilder         *builder,
                                                      GObject            *child,
                                                      const char         *tagname,
                                                      GtkBuildableParser *parser,
                                                      gpointer           *data);
static void     gtk_scale_buildable_custom_finished  (GtkBuildable       *buildable,
                                                      GtkBuilder         *builder,
                                                      GObject            *child,
                                                      const char         *tagname,
                                                      gpointer            user_data);
static char   * gtk_scale_format_value               (GtkScale           *scale,
                                                      double              value);


G_DEFINE_TYPE_WITH_CODE (GtkScale, gtk_scale, GTK_TYPE_RANGE,
                         G_ADD_PRIVATE (GtkScale)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_scale_buildable_interface_init))

static int
compare_marks (gconstpointer a, gconstpointer b, gpointer data)
{
  gboolean inverted = GPOINTER_TO_INT (data);
  int val;
  const GtkScaleMark *ma, *mb;

  val = inverted ? -1 : 1;

  ma = a; mb = b;

  return (ma->value > mb->value) ? val : ((ma->value < mb->value) ? -val : 0);
}

static void
update_label_request (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkAdjustment *adjustment = gtk_range_get_adjustment (GTK_RANGE (scale));
  double lowest_value, highest_value;
  char *text;
  int size = 0;
  int min;

  g_assert (priv->value_widget != NULL);

  lowest_value = gtk_adjustment_get_lower (adjustment);
  highest_value = gtk_adjustment_get_upper (adjustment);

  gtk_widget_set_size_request (priv->value_widget, -1, -1);

  text = gtk_scale_format_value (scale, lowest_value);
  gtk_label_set_label (GTK_LABEL (priv->value_widget), text);

  gtk_widget_measure (priv->value_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                      &min, NULL, NULL, NULL);
  size = MAX (size, min);
  g_free (text);

  text = gtk_scale_format_value (scale, highest_value);
  gtk_label_set_label (GTK_LABEL (priv->value_widget), text);

  gtk_widget_measure (priv->value_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                      &min, NULL, NULL, NULL);
  size = MAX (size, min);
  g_free (text);

  text = gtk_scale_format_value (scale, gtk_adjustment_get_value (adjustment));
  gtk_widget_set_size_request (priv->value_widget, size, -1);
  gtk_label_set_label (GTK_LABEL (priv->value_widget), text);
  g_free (text);
}

static void
gtk_scale_notify (GObject    *object,
                  GParamSpec *pspec)
{
  GtkScale *scale = GTK_SCALE (object);
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  if (strcmp (pspec->name, "inverted") == 0)
    {
      GtkScaleMark *mark;
      GSList *m;
      int i, n;
      double *values;

      priv->marks = g_slist_sort_with_data (priv->marks,
                                            compare_marks,
                                            GINT_TO_POINTER (gtk_range_get_inverted (GTK_RANGE (scale))));

      n = g_slist_length (priv->marks);
      values = g_new (double, n);
      for (m = priv->marks, i = 0; m; m = m->next, i++)
        {
          mark = m->data;
          values[i] = mark->value;
        }

      _gtk_range_set_stop_values (GTK_RANGE (scale), values, n);

      if (priv->top_marks_widget)
        gtk_widget_queue_resize (priv->top_marks_widget);

      if (priv->bottom_marks_widget)
        gtk_widget_queue_resize (priv->bottom_marks_widget);

      g_free (values);
    }
  else if (strcmp (pspec->name, "adjustment") == 0)
    {
      if (priv->value_widget)
        update_label_request (scale);
    }

  if (G_OBJECT_CLASS (gtk_scale_parent_class)->notify)
    G_OBJECT_CLASS (gtk_scale_parent_class)->notify (object, pspec);
}

static void
gtk_scale_allocate_value (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkWidget *widget = GTK_WIDGET (scale);
  GtkRange *range = GTK_RANGE (widget);
  GtkWidget *slider_widget;
  GtkAllocation value_alloc;
  int range_width, range_height;
  graphene_rect_t slider_bounds;

  range_width = gtk_widget_get_width (widget);
  range_height = gtk_widget_get_height (widget);

  slider_widget = gtk_range_get_slider_widget (range);
  if (!gtk_widget_compute_bounds (slider_widget, widget, &slider_bounds))
    graphene_rect_init (&slider_bounds, 0, 0, gtk_widget_get_width (widget), gtk_widget_get_height (widget));

  gtk_widget_measure (priv->value_widget,
                      GTK_ORIENTATION_HORIZONTAL, -1,
                      &value_alloc.width, NULL,
                      NULL, NULL);
  gtk_widget_measure (priv->value_widget,
                      GTK_ORIENTATION_VERTICAL, -1,
                      &value_alloc.height, NULL,
                      NULL, NULL);

  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (range)) == GTK_ORIENTATION_HORIZONTAL)
    {
      switch (priv->value_pos)
        {
        case GTK_POS_LEFT:
          value_alloc.x = 0;
          value_alloc.y = (range_height - value_alloc.height) / 2;
          break;

        case GTK_POS_RIGHT:
          value_alloc.x = range_width - value_alloc.width;
          value_alloc.y = (range_height - value_alloc.height) / 2;
          break;

        case GTK_POS_TOP:
          value_alloc.x = slider_bounds.origin.x + (slider_bounds.size.width - value_alloc.width) / 2;
          value_alloc.y = slider_bounds.origin.y - value_alloc.height;
          break;

        case GTK_POS_BOTTOM:
          value_alloc.x = slider_bounds.origin.x + (slider_bounds.size.width - value_alloc.width) / 2;
          value_alloc.y = range_height - value_alloc.height;
          break;

        default:
          g_return_if_reached ();
          break;
        }
      if (priv->has_markup && (priv->value_pos == GTK_POS_LEFT || priv->value_pos == GTK_POS_RIGHT))
        {
          if (priv->top_marks_widget)
            {
              int marks_height;
              gtk_widget_measure (priv->top_marks_widget,
                                  GTK_ORIENTATION_VERTICAL, -1,
                                  &marks_height, NULL,
                                  NULL, NULL);
              value_alloc.y += marks_height / 2;
            }
          if (priv->bottom_marks_widget)
            {
              int marks_height;
              gtk_widget_measure (priv->bottom_marks_widget,
                                  GTK_ORIENTATION_VERTICAL, -1,
                                  &marks_height, NULL,
                                  NULL, NULL);
              value_alloc.y -= marks_height / 2;
            }
        }
    }
  else /* VERTICAL */
    {
      switch (priv->value_pos)
        {
        case GTK_POS_LEFT:
          value_alloc.x = 0;
          value_alloc.y = (slider_bounds.origin.y + (slider_bounds.size.height / 2)) - value_alloc.height / 2;
          break;

        case GTK_POS_RIGHT:
          value_alloc.x = range_width - value_alloc.width;
          value_alloc.y = (slider_bounds.origin.y + (slider_bounds.size.height / 2)) - value_alloc.height / 2;
          break;

        case GTK_POS_TOP:
          value_alloc.x = (range_width - value_alloc.width) / 2;
          value_alloc.y = 0;
          break;

        case GTK_POS_BOTTOM:
          value_alloc.x = (range_width - value_alloc.width) / 2;
          value_alloc.y = range_height - value_alloc.height;
          break;

        default:
          g_return_if_reached ();
        }
      if (priv->has_markup && (priv->value_pos == GTK_POS_TOP || priv->value_pos == GTK_POS_BOTTOM))
        {
          if (priv->top_marks_widget)
            {
              int marks_width;
              gtk_widget_measure (priv->top_marks_widget,
                                  GTK_ORIENTATION_HORIZONTAL, -1,
                                  &marks_width, NULL,
                                  NULL, NULL);
              value_alloc.x += marks_width / 2;
            }
          if (priv->bottom_marks_widget)
            {
              int marks_width;
              gtk_widget_measure (priv->bottom_marks_widget,
                                  GTK_ORIENTATION_HORIZONTAL, -1,
                                  &marks_width, NULL,
                                  NULL, NULL);
              value_alloc.x -= marks_width / 2;
            }
        }
    }

  gtk_widget_size_allocate (priv->value_widget, &value_alloc, -1);
}

static void
gtk_scale_allocate_mark (GtkGizmo *gizmo,
                         int       width,
                         int       height,
                         int       baseline)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkScale *scale = GTK_SCALE (gtk_widget_get_parent (gtk_widget_get_parent (widget)));
  GtkScaleMark *mark = g_object_get_data (G_OBJECT (gizmo), "mark");
  int indicator_width, indicator_height;
  GtkAllocation indicator_alloc;
  GtkOrientation orientation;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (scale));
  gtk_widget_measure (mark->indicator_widget,
                      GTK_ORIENTATION_HORIZONTAL, -1,
                      &indicator_width, NULL,
                      NULL, NULL);
  gtk_widget_measure (mark->indicator_widget,
                      GTK_ORIENTATION_VERTICAL, -1,
                      &indicator_height, NULL,
                      NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      indicator_alloc.x = (width - indicator_width) / 2;
      if (mark->position == GTK_POS_TOP)
        indicator_alloc.y =  height - indicator_height;
      else
        indicator_alloc.y = 0;

      indicator_alloc.width = indicator_width;
      indicator_alloc.height = indicator_height;
    }
  else
    {
      if (mark->position == GTK_POS_TOP)
        indicator_alloc.x = width - indicator_width;
      else
        indicator_alloc.x = 0;
      indicator_alloc.y = (height - indicator_height) / 2;
      indicator_alloc.width = indicator_width;
      indicator_alloc.height = indicator_height;
    }

  gtk_widget_size_allocate (mark->indicator_widget, &indicator_alloc, baseline);

  if (mark->label_widget)
    {
      GtkAllocation label_alloc;

      label_alloc = (GtkAllocation) {0, 0, width, height};

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          label_alloc.height = height - indicator_alloc.height;
          if (mark->position == GTK_POS_BOTTOM)
            label_alloc.y = indicator_alloc.y + indicator_alloc.height;
        }
      else
        {
          label_alloc.width = width - indicator_alloc.width;
          if (mark->position == GTK_POS_BOTTOM)
            label_alloc.x = indicator_alloc.x + indicator_alloc.width;
        }

      gtk_widget_size_allocate (mark->label_widget, &label_alloc, baseline);
    }
}

static void
gtk_scale_allocate_marks (GtkGizmo *gizmo,
                          int       width,
                          int       height,
                          int       baseline)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkScale *scale = GTK_SCALE (gtk_widget_get_parent (widget));
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkOrientation orientation;
  int *marks;
  int i;
  GSList *m;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (scale));
  _gtk_range_get_stop_positions (GTK_RANGE (scale), &marks);

  for (m = priv->marks, i = 0; m; m = m->next, i++)
    {
      GtkScaleMark *mark = m->data;
      GtkAllocation mark_alloc;
      int mark_size;

      if ((mark->position == GTK_POS_TOP && widget == priv->bottom_marks_widget) ||
          (mark->position == GTK_POS_BOTTOM && widget == priv->top_marks_widget))
        continue;

      gtk_widget_measure (mark->widget,
                          orientation, -1,
                          &mark_size, NULL,
                          NULL, NULL);
      mark->stop_position = marks[i];

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          mark_alloc.x = mark->stop_position;
          mark_alloc.y = 0;
          mark_alloc.width = mark_size;
          mark_alloc.height = height;

          mark_alloc.x -= mark_size / 2;
        }
      else
        {
          mark_alloc.x = 0;
          mark_alloc.y = mark->stop_position;
          mark_alloc.width = width;
          mark_alloc.height = mark_size;

          mark_alloc.y -= mark_size / 2;
        }

      gtk_widget_size_allocate (mark->widget, &mark_alloc, baseline);
    }

  g_free (marks);
}

static void
gtk_scale_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkScale *scale = GTK_SCALE (widget);
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkAllocation range_rect, marks_rect;
  GtkOrientation orientation;

  GTK_WIDGET_CLASS (gtk_scale_parent_class)->size_allocate (widget, width, height, baseline);

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));
  gtk_range_get_range_rect (GTK_RANGE (scale), &range_rect);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int marks_height = 0;

      if (priv->top_marks_widget)
        {
          gtk_widget_measure (priv->top_marks_widget,
                              GTK_ORIENTATION_VERTICAL, -1,
                              &marks_height, NULL,
                              NULL, NULL);
          marks_rect.x = 0;
          marks_rect.y = 0;
          marks_rect.width = range_rect.width;
          marks_rect.height = marks_height;
          gtk_widget_size_allocate (priv->top_marks_widget, &marks_rect, -1);
        }

      if (priv->bottom_marks_widget)
        {
          gtk_widget_measure (priv->bottom_marks_widget,
                              GTK_ORIENTATION_VERTICAL, -1,
                              &marks_height, NULL,
                              NULL, NULL);
          marks_rect.x = 0;
          marks_rect.y = range_rect.y + range_rect.height;
          marks_rect.width = range_rect.width;
          marks_rect.height = marks_height;
          gtk_widget_size_allocate (priv->bottom_marks_widget, &marks_rect, -1);
        }
    }
  else
    {
      int marks_width = 0;

      if (priv->top_marks_widget)
        {
          gtk_widget_measure (priv->top_marks_widget,
                              GTK_ORIENTATION_HORIZONTAL, -1,
                              &marks_width, NULL,
                              NULL, NULL);
          marks_rect.x = range_rect.x - marks_width;
          marks_rect.y = 0;
          marks_rect.width = marks_width;
          marks_rect.height = range_rect.height;
          gtk_widget_size_allocate (priv->top_marks_widget, &marks_rect, -1);
        }

      if (priv->bottom_marks_widget)
        {
          gtk_widget_measure (priv->bottom_marks_widget,
                              GTK_ORIENTATION_HORIZONTAL, -1,
                              &marks_width, NULL,
                              NULL, NULL);
          marks_rect = range_rect;
          marks_rect.x = range_rect.x + range_rect.width;
          marks_rect.y = 0;
          marks_rect.width = marks_width;
          marks_rect.height = range_rect.height;
          gtk_widget_size_allocate (priv->bottom_marks_widget, &marks_rect, -1);
        }
    }

  if (priv->value_widget)
    {
      gtk_scale_allocate_value (scale);
    }
}

#define add_slider_binding(binding_set, keyval, mask, scroll)        \
  gtk_widget_class_add_binding_signal (widget_class,                 \
                                       keyval, mask,                 \
                                       I_("move-slider"),            \
                                       "(i)", scroll)

static void
gtk_scale_value_changed (GtkRange *range)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (GTK_SCALE (range));
  GtkAdjustment *adjustment = gtk_range_get_adjustment (range);

  if (priv->value_widget)
    {
      char *text = gtk_scale_format_value (GTK_SCALE (range),
                                           gtk_adjustment_get_value (adjustment));
      gtk_label_set_label (GTK_LABEL (priv->value_widget), text);

      g_free (text);
    }
}

static void
gtk_scale_class_init (GtkScaleClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass  *range_class;

  gobject_class = G_OBJECT_CLASS (class);
  range_class = (GtkRangeClass*) class;
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_scale_set_property;
  gobject_class->get_property = gtk_scale_get_property;
  gobject_class->notify = gtk_scale_notify;
  gobject_class->finalize = gtk_scale_finalize;

  widget_class->size_allocate = gtk_scale_size_allocate;
  widget_class->measure = gtk_scale_measure;
  widget_class->grab_focus = gtk_widget_grab_focus_self;
  widget_class->focus = gtk_widget_focus_self;

  range_class->get_range_border = gtk_scale_get_range_border;
  range_class->value_changed = gtk_scale_value_changed;

  class->get_layout_offsets = gtk_scale_real_get_layout_offsets;

  /**
   * GtkScale:digits: (attributes org.gtk.Method.get=gtk_scale_get_digits org.gtk.Method.set=gtk_scale_set_digits)
   *
   * The number of decimal places that are displayed in the value.
   */
  properties[PROP_DIGITS] =
      g_param_spec_int ("digits", NULL, NULL,
                        -1, MAX_DIGITS,
                        1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScale:draw-value: (attributes org.gtk.Method.get=gtk_scale_get_draw_value org.gtk.Method.set=gtk_scale_set_draw_value)
   *
   * Whether the current value is displayed as a string next to the slider.
   */
  properties[PROP_DRAW_VALUE] =
      g_param_spec_boolean ("draw-value", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScale:has-origin: (attributes org.gtk.Method.get=gtk_scale_get_has_origin org.gtk.Method.set=gtk_scale_set_has_origin)
   *
   * Whether the scale has an origin.
   */
  properties[PROP_HAS_ORIGIN] =
      g_param_spec_boolean ("has-origin", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkScale:value-pos: (attributes org.gtk.Method.get=gtk_scale_get_value_pos org.gtk.Method.set=gtk_scale_set_value_pos)
   *
   * The position in which the current value is displayed.
   */
  properties[PROP_VALUE_POS] =
      g_param_spec_enum ("value-pos", NULL, NULL,
                         GTK_TYPE_POSITION_TYPE,
                         GTK_POS_TOP,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, properties);

  /* All bindings (even arrow keys) are on both h/v scale, because
   * blind users etc. don't care about scale orientation.
   */

  add_slider_binding (binding_set, GDK_KEY_Left, 0,
                      GTK_SCROLL_STEP_LEFT);

  add_slider_binding (binding_set, GDK_KEY_Left, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KEY_KP_Left, 0,
                      GTK_SCROLL_STEP_LEFT);

  add_slider_binding (binding_set, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KEY_Right, 0,
                      GTK_SCROLL_STEP_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_Right, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_KP_Right, 0,
                      GTK_SCROLL_STEP_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_Up, 0,
                      GTK_SCROLL_STEP_UP);

  add_slider_binding (binding_set, GDK_KEY_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KEY_KP_Up, 0,
                      GTK_SCROLL_STEP_UP);

  add_slider_binding (binding_set, GDK_KEY_KP_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KEY_Down, 0,
                      GTK_SCROLL_STEP_DOWN);

  add_slider_binding (binding_set, GDK_KEY_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_DOWN);

  add_slider_binding (binding_set, GDK_KEY_KP_Down, 0,
                      GTK_SCROLL_STEP_DOWN);

  add_slider_binding (binding_set, GDK_KEY_KP_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_DOWN);

  add_slider_binding (binding_set, GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KEY_KP_Page_Up, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_LEFT);

  add_slider_binding (binding_set, GDK_KEY_Page_Up, 0,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KEY_KP_Page_Up, 0,
                      GTK_SCROLL_PAGE_UP);

  add_slider_binding (binding_set, GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_KP_Page_Down, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_RIGHT);

  add_slider_binding (binding_set, GDK_KEY_Page_Down, 0,
                      GTK_SCROLL_PAGE_DOWN);

  add_slider_binding (binding_set, GDK_KEY_KP_Page_Down, 0,
                      GTK_SCROLL_PAGE_DOWN);

  /* Logical bindings (vs. visual bindings above) */

  add_slider_binding (binding_set, GDK_KEY_plus, 0,
                      GTK_SCROLL_STEP_FORWARD);

  add_slider_binding (binding_set, GDK_KEY_minus, 0,
                      GTK_SCROLL_STEP_BACKWARD);

  add_slider_binding (binding_set, GDK_KEY_plus, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_FORWARD);

  add_slider_binding (binding_set, GDK_KEY_minus, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_BACKWARD);


  add_slider_binding (binding_set, GDK_KEY_KP_Add, 0,
                      GTK_SCROLL_STEP_FORWARD);

  add_slider_binding (binding_set, GDK_KEY_KP_Subtract, 0,
                      GTK_SCROLL_STEP_BACKWARD);

  add_slider_binding (binding_set, GDK_KEY_KP_Add, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_FORWARD);

  add_slider_binding (binding_set, GDK_KEY_KP_Subtract, GDK_CONTROL_MASK,
                      GTK_SCROLL_PAGE_BACKWARD);

  add_slider_binding (binding_set, GDK_KEY_Home, 0,
                      GTK_SCROLL_START);

  add_slider_binding (binding_set, GDK_KEY_KP_Home, 0,
                      GTK_SCROLL_START);

  add_slider_binding (binding_set, GDK_KEY_End, 0,
                      GTK_SCROLL_END);

  add_slider_binding (binding_set, GDK_KEY_KP_End, 0,
                      GTK_SCROLL_END);

  gtk_widget_class_set_css_name (widget_class, I_("scale"));

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SLIDER);
}

static void
gtk_scale_init (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkRange *range = GTK_RANGE (scale);

  gtk_widget_set_focusable (GTK_WIDGET (scale), TRUE);

  priv->value_pos = GTK_POS_TOP;
  priv->digits = 1;

  gtk_range_set_slider_size_fixed (range, TRUE);

  _gtk_range_set_has_origin (range, TRUE);

  gtk_range_set_round_digits (range, -1);

  gtk_range_set_flippable (range, TRUE);
}

static void
gtk_scale_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkScale *scale;

  scale = GTK_SCALE (object);

  switch (prop_id)
    {
    case PROP_DIGITS:
      gtk_scale_set_digits (scale, g_value_get_int (value));
      break;
    case PROP_DRAW_VALUE:
      gtk_scale_set_draw_value (scale, g_value_get_boolean (value));
      break;
    case PROP_HAS_ORIGIN:
      gtk_scale_set_has_origin (scale, g_value_get_boolean (value));
      break;
    case PROP_VALUE_POS:
      gtk_scale_set_value_pos (scale, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_scale_get_property (GObject      *object,
			guint         prop_id,
			GValue       *value,
			GParamSpec   *pspec)
{
  GtkScale *scale = GTK_SCALE (object);
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  switch (prop_id)
    {
    case PROP_DIGITS:
      g_value_set_int (value, priv->digits);
      break;
    case PROP_DRAW_VALUE:
      g_value_set_boolean (value, priv->draw_value);
      break;
    case PROP_HAS_ORIGIN:
      g_value_set_boolean (value, gtk_scale_get_has_origin (scale));
      break;
    case PROP_VALUE_POS:
      g_value_set_enum (value, priv->value_pos);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_scale_new:
 * @orientation: the scale’s orientation.
 * @adjustment: (nullable): the [class@Gtk.Adjustment] which sets
 *   the range of the scale, or %NULL to create a new adjustment.
 *
 * Creates a new `GtkScale`.
 *
 * Returns: a new `GtkScale`
 */
GtkWidget *
gtk_scale_new (GtkOrientation  orientation,
               GtkAdjustment  *adjustment)
{
  g_return_val_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment),
                        NULL);

  return g_object_new (GTK_TYPE_SCALE,
                       "orientation", orientation,
                       "adjustment",  adjustment,
                       NULL);
}

/**
 * gtk_scale_new_with_range:
 * @orientation: the scale’s orientation.
 * @min: minimum value
 * @max: maximum value
 * @step: step increment (tick size) used with keyboard shortcuts
 *
 * Creates a new scale widget with a range from @min to @max.
 *
 * The returns scale will have the given orientation and will let the
 * user input a number between @min and @max (including @min and @max)
 * with the increment @step. @step must be nonzero; it’s the distance
 * the slider moves when using the arrow keys to adjust the scale
 * value.
 *
 * Note that the way in which the precision is derived works best if
 * @step is a power of ten. If the resulting precision is not suitable
 * for your needs, use [method@Gtk.Scale.set_digits] to correct it.
 *
 * Returns: a new `GtkScale`
 */
GtkWidget *
gtk_scale_new_with_range (GtkOrientation orientation,
                          double         min,
                          double         max,
                          double         step)
{
  GtkAdjustment *adj;
  int digits;

  g_return_val_if_fail (min < max, NULL);
  g_return_val_if_fail (step != 0.0, NULL);

  adj = gtk_adjustment_new (min, min, max, step, 10 * step, 0);

  if (fabs (step) >= 1.0 || step == 0.0)
    {
      digits = 0;
    }
  else
    {
      digits = abs ((int) floor (log10 (fabs (step))));
      if (digits > 5)
        digits = 5;
    }

  return g_object_new (GTK_TYPE_SCALE,
                       "orientation", orientation,
                       "adjustment",  adj,
                       "digits",      digits,
                       NULL);
}

/**
 * gtk_scale_set_digits: (attributes org.gtk.Method.set_property=digits)
 * @scale: a `GtkScale`
 * @digits: the number of decimal places to display,
 *   e.g. use 1 to display 1.0, 2 to display 1.00, etc
 *
 * Sets the number of decimal places that are displayed in the value.
 *
 * Also causes the value of the adjustment to be rounded to this number
 * of digits, so the retrieved value matches the displayed one, if
 * [property@Gtk.Scale:draw-value] is %TRUE when the value changes. If
 * you want to enforce rounding the value when [property@Gtk.Scale:draw-value]
 * is %FALSE, you can set [property@Gtk.Range:round-digits] instead.
 *
 * Note that rounding to a small number of digits can interfere with
 * the smooth autoscrolling that is built into `GtkScale`. As an alternative,
 * you can use [method@Gtk.Scale.set_format_value_func] to format the displayed
 * value yourself.
 */
void
gtk_scale_set_digits (GtkScale *scale,
		      int       digits)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkRange *range;

  g_return_if_fail (GTK_IS_SCALE (scale));

  range = GTK_RANGE (scale);

  digits = CLAMP (digits, -1, MAX_DIGITS);

  if (priv->digits != digits)
    {
      priv->digits = digits;
      if (priv->draw_value)
        gtk_range_set_round_digits (range, digits);

      if (priv->value_widget)
        update_label_request (scale);

      gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify_by_pspec (G_OBJECT (scale), properties[PROP_DIGITS]);
    }
}

/**
 * gtk_scale_get_digits: (attributes org.gtk.Method.get_property=digits)
 * @scale: a `GtkScale`
 *
 * Gets the number of decimal places that are displayed in the value.
 *
 * Returns: the number of decimal places that are displayed
 */
int
gtk_scale_get_digits (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  g_return_val_if_fail (GTK_IS_SCALE (scale), -1);

  return priv->digits;
}

static void
update_value_position (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  if (!priv->value_widget)
    return;

  gtk_widget_remove_css_class (priv->value_widget, "top");
  gtk_widget_remove_css_class (priv->value_widget, "right");
  gtk_widget_remove_css_class (priv->value_widget, "bottom");
  gtk_widget_remove_css_class (priv->value_widget, "left");

  switch (priv->value_pos)
    {
    case GTK_POS_TOP:
      gtk_widget_add_css_class (priv->value_widget, "top");
      break;
    case GTK_POS_RIGHT:
      gtk_widget_add_css_class (priv->value_widget, "right");
      break;
    case GTK_POS_BOTTOM:
      gtk_widget_add_css_class (priv->value_widget, "bottom");
      break;
    case GTK_POS_LEFT:
      gtk_widget_add_css_class (priv->value_widget, "left");
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

/**
 * gtk_scale_set_draw_value: (attributes org.gtk.Method.set_property=draw-value)
 * @scale: a `GtkScale`
 * @draw_value: %TRUE to draw the value
 *
 * Specifies whether the current value is displayed as a string next
 * to the slider.
 */
void
gtk_scale_set_draw_value (GtkScale *scale,
                          gboolean  draw_value)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  g_return_if_fail (GTK_IS_SCALE (scale));

  draw_value = draw_value != FALSE;

  if (priv->draw_value != draw_value)
    {
      priv->draw_value = draw_value;
      if (draw_value)
        {
          priv->value_widget = g_object_new (GTK_TYPE_LABEL,
                                             "css-name", "value",
                                             NULL);

          gtk_widget_insert_after (priv->value_widget, GTK_WIDGET (scale), NULL);
          gtk_range_set_round_digits (GTK_RANGE (scale), priv->digits);
          update_value_position (scale);
          update_label_request (scale);
        }
      else if (priv->value_widget)
        {
          g_clear_pointer (&priv->value_widget, gtk_widget_unparent);
          gtk_range_set_round_digits (GTK_RANGE (scale), -1);
        }

      g_object_notify_by_pspec (G_OBJECT (scale), properties[PROP_DRAW_VALUE]);
    }
}

/**
 * gtk_scale_get_draw_value: (attributes org.gtk.Method.get_property=draw-value)
 * @scale: a `GtkScale`
 *
 * Returns whether the current value is displayed as a string
 * next to the slider.
 *
 * Returns: whether the current value is displayed as a string
 */
gboolean
gtk_scale_get_draw_value (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  g_return_val_if_fail (GTK_IS_SCALE (scale), FALSE);

  return priv->draw_value;
}

/**
 * gtk_scale_set_has_origin: (attributes org.gtk.Method.set_property=has-origin)
 * @scale: a `GtkScale`
 * @has_origin: %TRUE if the scale has an origin
 *
 * Sets whether the scale has an origin.
 *
 * If [property@Gtk.Scale:has-origin] is set to %TRUE (the default),
 * the scale will highlight the part of the trough between the origin
 * (bottom or left side) and the current value.
 */
void
gtk_scale_set_has_origin (GtkScale *scale,
                          gboolean  has_origin)
{
  g_return_if_fail (GTK_IS_SCALE (scale));

  has_origin = has_origin != FALSE;

  if (_gtk_range_get_has_origin (GTK_RANGE (scale)) != has_origin)
    {
      _gtk_range_set_has_origin (GTK_RANGE (scale), has_origin);

      gtk_widget_queue_draw (GTK_WIDGET (scale));

      g_object_notify_by_pspec (G_OBJECT (scale), properties[PROP_HAS_ORIGIN]);
    }
}

/**
 * gtk_scale_get_has_origin: (attributes org.gtk.Method.get_property=has-origin)
 * @scale: a `GtkScale`
 *
 * Returns whether the scale has an origin.
 *
 * Returns: %TRUE if the scale has an origin.
 */
gboolean
gtk_scale_get_has_origin (GtkScale *scale)
{
  g_return_val_if_fail (GTK_IS_SCALE (scale), FALSE);

  return _gtk_range_get_has_origin (GTK_RANGE (scale));
}

/**
 * gtk_scale_set_value_pos: (attributes org.gtk.Method.set_property=value-pos)
 * @scale: a `GtkScale`
 * @pos: the position in which the current value is displayed
 *
 * Sets the position in which the current value is displayed.
 */
void
gtk_scale_set_value_pos (GtkScale        *scale,
                         GtkPositionType  pos)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  g_return_if_fail (GTK_IS_SCALE (scale));

  if (priv->value_pos != pos)
    {
      priv->value_pos = pos;

      update_value_position (scale);
      gtk_widget_queue_resize (GTK_WIDGET (scale));

      g_object_notify_by_pspec (G_OBJECT (scale), properties[PROP_VALUE_POS]);
    }
}

/**
 * gtk_scale_get_value_pos: (attributes org.gtk.Method.get_property=value-pos)
 * @scale: a `GtkScale`
 *
 * Gets the position in which the current value is displayed.
 *
 * Returns: the position in which the current value is displayed
 */
GtkPositionType
gtk_scale_get_value_pos (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  g_return_val_if_fail (GTK_IS_SCALE (scale), 0);

  return priv->value_pos;
}

static void
gtk_scale_get_range_border (GtkRange  *range,
                            GtkBorder *border)
{
  GtkScale *scale = GTK_SCALE (range);
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  border->left = 0;
  border->right = 0;
  border->top = 0;
  border->bottom = 0;

  if (priv->value_widget)
    {
      int value_size;
      GtkOrientation value_orientation;

      if (priv->value_pos == GTK_POS_LEFT || priv->value_pos == GTK_POS_RIGHT)
        value_orientation = GTK_ORIENTATION_HORIZONTAL;
      else
        value_orientation = GTK_ORIENTATION_VERTICAL;

      gtk_widget_measure (priv->value_widget,
                          value_orientation, -1,
                          &value_size, NULL,
                          NULL, NULL);

      switch (priv->value_pos)
        {
        case GTK_POS_LEFT:
          border->left += value_size;
          break;
        case GTK_POS_RIGHT:
          border->right += value_size;
          break;
        case GTK_POS_TOP:
          border->top += value_size;
          break;
        case GTK_POS_BOTTOM:
          border->bottom += value_size;
          break;
        default:
          g_assert_not_reached ();
          break;
        }
    }

  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (range)) == GTK_ORIENTATION_HORIZONTAL)
    {
      int height;
      bool need_symmetry = !priv->has_markup && (priv->value_pos == GTK_POS_LEFT || priv->value_pos == GTK_POS_RIGHT || !priv->draw_value);

      if (priv->top_marks_widget)
        {
          gtk_widget_measure (priv->top_marks_widget,
                              GTK_ORIENTATION_VERTICAL, -1,
                              &height, NULL,
                              NULL, NULL);
          if (height > 0)
            {
              border->top += height;
              if (need_symmetry)
                border->bottom += height;
            }
        }

      if (priv->bottom_marks_widget)
        {
          gtk_widget_measure (priv->bottom_marks_widget,
                              GTK_ORIENTATION_VERTICAL, -1,
                              &height, NULL,
                              NULL, NULL);
          if (height > 0)
            {
              border->bottom += height;
              if (need_symmetry)
                border->top += height;
            }
        }
    }
  else
    {
      int width;
      bool need_symmetry = !priv->has_markup && (priv->value_pos == GTK_POS_TOP || priv->value_pos == GTK_POS_BOTTOM || !priv->draw_value);

      if (priv->top_marks_widget)
        {
          gtk_widget_measure (priv->top_marks_widget,
                              GTK_ORIENTATION_HORIZONTAL, -1,
                              &width, NULL,
                              NULL, NULL);
          if (width > 0)
            {
              border->left += width;
              if (need_symmetry)
                border->right += width;
            }
        }

      if (priv->bottom_marks_widget)
        {
          gtk_widget_measure (priv->bottom_marks_widget,
                              GTK_ORIENTATION_HORIZONTAL, -1,
                              &width, NULL,
                              NULL, NULL);
          if (width > 0)
            {
              border->right += width;
              if (need_symmetry)
                border->left += width;
            }
        }
    }
}

static void
gtk_scale_measure_mark (GtkGizmo       *gizmo,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  GtkScaleMark *mark = g_object_get_data (G_OBJECT (gizmo), "mark");

  gtk_widget_measure (mark->indicator_widget,
                      orientation, -1,
                      minimum, natural,
                      NULL, NULL);

  if (mark->label_widget)
    {
      int label_min, label_nat;

      gtk_widget_measure (mark->label_widget,
                          orientation, -1,
                          &label_min, &label_nat,
                          NULL, NULL);
      *minimum += label_min;
      *natural += label_nat;
    }
}

static void
gtk_scale_measure_marks (GtkGizmo       *gizmo,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkScale *scale = GTK_SCALE (gtk_widget_get_parent (widget));;
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkOrientation scale_orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (scale));
  GSList *m;

  *minimum = *natural = 0;

  for (m = priv->marks; m; m = m->next)
    {
      GtkScaleMark *mark = m->data;
      int mark_size;

      if ((mark->position == GTK_POS_TOP && widget == priv->bottom_marks_widget) ||
          (mark->position == GTK_POS_BOTTOM && widget == priv->top_marks_widget))
        continue;

      gtk_widget_measure (mark->widget,
                          orientation, -1,
                          &mark_size, NULL,
                          NULL, NULL);

      if (scale_orientation == orientation)
        {
          *minimum += mark_size;
          *natural += mark_size;
        }
      else
        {
          *minimum = MAX (*minimum, mark_size);
          *natural = MAX (*natural, mark_size);
        }
    }
}

static void
gtk_scale_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkScale *scale = GTK_SCALE (widget);
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkOrientation scale_orientation;

  GTK_WIDGET_CLASS (gtk_scale_parent_class)->measure (widget,
                                                      orientation,
                                                      for_size,
                                                      minimum, natural,
                                                      minimum_baseline, natural_baseline);

  scale_orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));

  if (scale_orientation == orientation)
    {
      int top_marks_size = 0, bottom_marks_size = 0, marks_size;

      if (priv->top_marks_widget)
        gtk_widget_measure (priv->top_marks_widget,
                            orientation, for_size,
                            &top_marks_size, NULL,
                            NULL, NULL);
      if (priv->bottom_marks_widget)
        gtk_widget_measure (priv->bottom_marks_widget,
                            orientation, for_size,
                            &bottom_marks_size, NULL,
                            NULL, NULL);

      marks_size = MAX (top_marks_size, bottom_marks_size);

      *minimum = MAX (*minimum, marks_size);
      *natural = MAX (*natural, marks_size);
    }

  if (priv->value_widget)
    {
      int min, nat;

      gtk_widget_measure (priv->value_widget, orientation, -1, &min, &nat, NULL, NULL);

      if (priv->value_pos == GTK_POS_TOP ||
          priv->value_pos == GTK_POS_BOTTOM)
        {
          if (orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              *minimum = MAX (*minimum, min);
              *natural = MAX (*natural, nat);
            }
          else
            {
              *minimum += min;
              *natural += nat;
            }
        }
      else
        {
          if (orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              *minimum += min;
              *natural += nat;
            }
          else
            {
              *minimum = MAX (*minimum, min);
              *natural = MAX (*natural, nat);
            }
        }
    }
}

static void
gtk_scale_real_get_layout_offsets (GtkScale *scale,
                                   int      *x,
                                   int      *y)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  graphene_rect_t value_bounds;

  if (!priv->value_widget ||
      !gtk_widget_compute_bounds (priv->value_widget, GTK_WIDGET (scale), &value_bounds))
    {
      *x = 0;
      *y = 0;

      return;
    }


  *x = value_bounds.origin.x;
  *y = value_bounds.origin.y;
}

static char *
weed_out_neg_zero (char *str,
                   int    digits)
{
  if (str[0] == '-')
    {
      char neg_zero[8];
      g_snprintf (neg_zero, 8, "%0.*f", digits, -0.0);
      if (strcmp (neg_zero, str) == 0)
        memmove (str, str + 1, strlen (str));
    }
  return str;
}

static char *
gtk_scale_format_value (GtkScale *scale,
                        double    value)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  if (priv->format_value_func)
    {
      return priv->format_value_func (scale, value, priv->format_value_func_user_data);
    }
  else
    {
      char *fmt = g_strdup_printf ("%0.*f", priv->digits, value);
      return weed_out_neg_zero (fmt, priv->digits);
    }
}

static void
gtk_scale_finalize (GObject *object)
{
  GtkScale *scale = GTK_SCALE (object);
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  gtk_scale_clear_marks (scale);

  g_clear_pointer (&priv->value_widget, gtk_widget_unparent);

  if (priv->format_value_func_destroy_notify)
    priv->format_value_func_destroy_notify (priv->format_value_func_user_data);

  G_OBJECT_CLASS (gtk_scale_parent_class)->finalize (object);
}

/**
 * gtk_scale_get_layout:
 * @scale: A `GtkScale`
 *
 * Gets the `PangoLayout` used to display the scale.
 *
 * The returned object is owned by the scale so does not need
 * to be freed by the caller.
 *
 * Returns: (transfer none) (nullable): the [class@Pango.Layout]
 *   for this scale, or %NULL if the [property@Gtk.Scale:draw-value]
 *   property is %FALSE.
 */
PangoLayout *
gtk_scale_get_layout (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  g_return_val_if_fail (GTK_IS_SCALE (scale), NULL);

  if (priv->value_widget)
    return gtk_label_get_layout (GTK_LABEL (priv->value_widget));

  return NULL;
}

/**
 * gtk_scale_get_layout_offsets:
 * @scale: a `GtkScale`
 * @x: (out) (optional): location to store X offset of layout
 * @y: (out) (optional): location to store Y offset of layout
 *
 * Obtains the coordinates where the scale will draw the
 * `PangoLayout` representing the text in the scale.
 *
 * Remember when using the `PangoLayout` function you need to
 * convert to and from pixels using `PANGO_PIXELS()` or `PANGO_SCALE`.
 *
 * If the [property@Gtk.Scale:draw-value] property is %FALSE, the return
 * values are undefined.
 */
void
gtk_scale_get_layout_offsets (GtkScale *scale,
                              int      *x,
                              int      *y)
{
  int local_x = 0;
  int local_y = 0;

  g_return_if_fail (GTK_IS_SCALE (scale));

  if (GTK_SCALE_GET_CLASS (scale)->get_layout_offsets)
    (GTK_SCALE_GET_CLASS (scale)->get_layout_offsets) (scale, &local_x, &local_y);

  if (x)
    *x = local_x;

  if (y)
    *y = local_y;
}

static void
gtk_scale_mark_free (gpointer data)
{
  GtkScaleMark *mark = data;

  if (mark->label_widget)
    gtk_widget_unparent (mark->label_widget);

  gtk_widget_unparent (mark->indicator_widget);
  gtk_widget_unparent (mark->widget);
  g_free (mark->markup);
  g_free (mark);
}

/**
 * gtk_scale_clear_marks:
 * @scale: a `GtkScale`
 *
 * Removes any marks that have been added.
 */
void
gtk_scale_clear_marks (GtkScale *scale)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  g_return_if_fail (GTK_IS_SCALE (scale));

  g_slist_free_full (priv->marks, gtk_scale_mark_free);
  priv->marks = NULL;
  priv->has_markup = false;

  g_clear_pointer (&priv->top_marks_widget, gtk_widget_unparent);
  g_clear_pointer (&priv->bottom_marks_widget, gtk_widget_unparent);

  gtk_widget_remove_css_class (GTK_WIDGET (scale), "marks-before");
  gtk_widget_remove_css_class (GTK_WIDGET (scale), "marks-after");

  _gtk_range_set_stop_values (GTK_RANGE (scale), NULL, 0);

  gtk_widget_queue_resize (GTK_WIDGET (scale));
}

/**
 * gtk_scale_add_mark:
 * @scale: a `GtkScale`
 * @value: the value at which the mark is placed, must be between
 *   the lower and upper limits of the scales’ adjustment
 * @position: where to draw the mark. For a horizontal scale, %GTK_POS_TOP
 *   and %GTK_POS_LEFT are drawn above the scale, anything else below.
 *   For a vertical scale, %GTK_POS_LEFT and %GTK_POS_TOP are drawn to
 *   the left of the scale, anything else to the right.
 * @markup: (nullable): Text to be shown at the mark, using Pango markup
 *
 * Adds a mark at @value.
 *
 * A mark is indicated visually by drawing a tick mark next to the scale,
 * and GTK makes it easy for the user to position the scale exactly at the
 * marks value.
 *
 * If @markup is not %NULL, text is shown next to the tick mark.
 *
 * To remove marks from a scale, use [method@Gtk.Scale.clear_marks].
 */
void
gtk_scale_add_mark (GtkScale        *scale,
                    double           value,
                    GtkPositionType  position,
                    const char      *markup)
{
  GtkWidget *widget;
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);
  GtkScaleMark *mark;
  GSList *m;
  double *values;
  int n, i;
  GtkWidget *marks_widget;

  g_return_if_fail (GTK_IS_SCALE (scale));

  widget = GTK_WIDGET (scale);

  mark = g_new0 (GtkScaleMark, 1);
  mark->value = value;
  mark->markup = g_strdup (markup);
  if (position == GTK_POS_LEFT ||
      position == GTK_POS_TOP)
    mark->position = GTK_POS_TOP;
  else
    mark->position = GTK_POS_BOTTOM;

  priv->marks = g_slist_insert_sorted_with_data (priv->marks, mark,
                                                 compare_marks,
                                                 GINT_TO_POINTER (gtk_range_get_inverted (GTK_RANGE (scale))));

  if (mark->position == GTK_POS_TOP)
    {
      if (!priv->top_marks_widget)
        {
          priv->top_marks_widget = gtk_gizmo_new_with_role ("marks",
                                                            GTK_ACCESSIBLE_ROLE_NONE,
                                                            gtk_scale_measure_marks,
                                                            gtk_scale_allocate_marks,
                                                            NULL,
                                                            NULL,
                                                            NULL, NULL);

          gtk_widget_insert_after (priv->top_marks_widget,
                                   GTK_WIDGET (scale),
                                   priv->value_widget);
          gtk_widget_add_css_class (priv->top_marks_widget, "top");
        }
      marks_widget = priv->top_marks_widget;
    }
  else
    {
      if (!priv->bottom_marks_widget)
        {
          priv->bottom_marks_widget = gtk_gizmo_new_with_role ("marks",
                                                               GTK_ACCESSIBLE_ROLE_NONE,
                                                               gtk_scale_measure_marks,
                                                               gtk_scale_allocate_marks,
                                                               NULL,
                                                               NULL,
                                                               NULL, NULL);

          gtk_widget_insert_before (priv->bottom_marks_widget,
                                    GTK_WIDGET (scale),
                                    gtk_range_get_trough_widget (GTK_RANGE (scale)));
          gtk_widget_add_css_class (priv->bottom_marks_widget, "bottom");
        }
      marks_widget = priv->bottom_marks_widget;
    }

  mark->widget = gtk_gizmo_new ("mark", gtk_scale_measure_mark, gtk_scale_allocate_mark, NULL, NULL, NULL, NULL);
  g_object_set_data (G_OBJECT (mark->widget), "mark", mark);

  mark->indicator_widget = gtk_gizmo_new ("indicator", NULL, NULL, NULL, NULL, NULL, NULL);
  gtk_widget_set_parent (mark->indicator_widget, mark->widget);
  if (mark->markup && *mark->markup)
    {
      mark->label_widget = g_object_new (GTK_TYPE_LABEL,
                                         "use-markup", TRUE,
                                         "label", mark->markup,
                                         NULL);
      if (marks_widget == priv->top_marks_widget)
        gtk_widget_insert_after (mark->label_widget, mark->widget, NULL);
      else
        gtk_widget_insert_before (mark->label_widget, mark->widget, NULL);
      priv->has_markup = true;
    }

  m = g_slist_find (priv->marks, mark);
  m = m->next;
  while (m)
    {
      GtkScaleMark *next = m->data;
      if (next->position == mark->position)
        break;
      m = m->next;
    }

  if (m)
    {
      GtkScaleMark *next = m->data;
      gtk_widget_insert_before (mark->widget, marks_widget, next->widget);
    }
  else
    {
      gtk_widget_set_parent (mark->widget, marks_widget);
    }

  n = g_slist_length (priv->marks);
  values = g_new (double, n);
  for (m = priv->marks, i = 0; m; m = m->next, i++)
    {
      mark = m->data;
      values[i] = mark->value;
    }

  _gtk_range_set_stop_values (GTK_RANGE (scale), values, n);

  g_free (values);

  if (priv->top_marks_widget)
    gtk_widget_add_css_class (GTK_WIDGET (scale), "marks-before");

  if (priv->bottom_marks_widget)
    gtk_widget_add_css_class (GTK_WIDGET (scale), "marks-after");

  gtk_widget_queue_resize (widget);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_scale_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->custom_tag_start = gtk_scale_buildable_custom_tag_start;
  iface->custom_finished = gtk_scale_buildable_custom_finished;
}

typedef struct
{
  GtkScale *scale;
  GtkBuilder *builder;
  GSList *marks;
} MarksSubparserData;

typedef struct
{
  double value;
  GtkPositionType position;
  GString *markup;
  char *context;
  gboolean translatable;
} MarkData;

static void
mark_data_free (MarkData *data)
{
  g_string_free (data->markup, TRUE);
  g_free (data->context);
  g_free (data);
}

static void
marks_start_element (GtkBuildableParseContext *context,
                     const char               *element_name,
                     const char              **names,
                     const char              **values,
                     gpointer                  user_data,
                     GError                  **error)
{
  MarksSubparserData *data = (MarksSubparserData*)user_data;

  if (strcmp (element_name, "marks") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "mark") == 0)
    {
      const char *value_str;
      double value = 0;
      const char *position_str = NULL;
      GtkPositionType position = GTK_POS_BOTTOM;
      const char *msg_context = NULL;
      gboolean translatable = FALSE;
      MarkData *mark;

      if (!_gtk_builder_check_parent (data->builder, context, "marks", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "value", &value_str,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &msg_context,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "position", &position_str,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (value_str != NULL)
        {
          GValue gvalue = G_VALUE_INIT;

          if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_DOUBLE, value_str, &gvalue, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
              return;
            }

          value = g_value_get_double (&gvalue);
        }

      if (position_str != NULL)
        {
          GValue gvalue = G_VALUE_INIT;

          if (!gtk_builder_value_from_string_type (data->builder, GTK_TYPE_POSITION_TYPE, position_str, &gvalue, error))
            {
              _gtk_builder_prefix_error (data->builder, context, error);
              return;
            }

          position = g_value_get_enum (&gvalue);
        }

      mark = g_new (MarkData, 1);
      mark->value = value;
      if (position == GTK_POS_LEFT || position == GTK_POS_TOP)
        mark->position = GTK_POS_TOP;
      else
        mark->position = GTK_POS_BOTTOM;
      mark->markup = g_string_new ("");
      mark->context = g_strdup (msg_context);
      mark->translatable = translatable;

      data->marks = g_slist_prepend (data->marks, mark);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkScale", element_name,
                                        error);
    }
}

static void
marks_text (GtkBuildableParseContext  *context,
            const char                *text,
            gsize                      text_len,
            gpointer                   user_data,
            GError                   **error)
{
  MarksSubparserData *data = (MarksSubparserData*)user_data;

  if (strcmp (gtk_buildable_parse_context_get_element (context), "mark") == 0)
    {
      MarkData *mark = data->marks->data;

      g_string_append_len (mark->markup, text, text_len);
    }
}

static const GtkBuildableParser marks_parser =
  {
    marks_start_element,
    NULL,
    marks_text,
  };


static gboolean
gtk_scale_buildable_custom_tag_start (GtkBuildable       *buildable,
                                      GtkBuilder         *builder,
                                      GObject            *child,
                                      const char         *tagname,
                                      GtkBuildableParser *parser,
                                      gpointer           *parser_data)
{
  MarksSubparserData *data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "marks") == 0)
    {
      data = g_new0 (MarksSubparserData, 1);
      data->scale = GTK_SCALE (buildable);
      data->builder = builder;
      data->marks = NULL;

      *parser = marks_parser;
      *parser_data = data;

      return TRUE;
    }

  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                   tagname, parser, parser_data);
}

static void
gtk_scale_buildable_custom_finished (GtkBuildable *buildable,
                                     GtkBuilder   *builder,
                                     GObject      *child,
                                     const char   *tagname,
                                     gpointer      user_data)
{
  GtkScale *scale = GTK_SCALE (buildable);
  MarksSubparserData *marks_data;

  if (strcmp (tagname, "marks") == 0)
    {
      GSList *m;
      const char *markup;

      marks_data = (MarksSubparserData *)user_data;

      for (m = marks_data->marks; m; m = m->next)
        {
          MarkData *mdata = m->data;

          if (mdata->translatable && mdata->markup->len)
            markup = _gtk_builder_parser_translate (gtk_builder_get_translation_domain (builder),
                                                    mdata->context,
                                                    mdata->markup->str);
          else
            markup = mdata->markup->str;

          gtk_scale_add_mark (scale, mdata->value, mdata->position, markup);

          mark_data_free (mdata);
        }

      g_slist_free (marks_data->marks);
      g_free (marks_data);
    }
  else
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
                                               tagname, user_data);
    }

}

/**
 * gtk_scale_set_format_value_func:
 * @scale: a `GtkScale`
 * @func: (nullable): function that formats the value
 * @user_data: (closure): user data to pass to @func
 * @destroy_notify: (nullable): destroy function for @user_data
 *
 * @func allows you to change how the scale value is displayed.
 *
 * The given function will return an allocated string representing
 * @value. That string will then be used to display the scale's value.
 *
 * If #NULL is passed as @func, the value will be displayed on
 * its own, rounded according to the value of the
 * [property@Gtk.Scale:digits] property.
 */
void
gtk_scale_set_format_value_func (GtkScale                *scale,
                                 GtkScaleFormatValueFunc  func,
                                 gpointer                 user_data,
                                 GDestroyNotify           destroy_notify)
{
  GtkScalePrivate *priv = gtk_scale_get_instance_private (scale);

  g_return_if_fail (GTK_IS_SCALE (scale));

  if (priv->format_value_func_destroy_notify)
    priv->format_value_func_destroy_notify (priv->format_value_func_user_data);

  priv->format_value_func = func;
  priv->format_value_func_user_data = user_data;
  priv->format_value_func_destroy_notify = destroy_notify;

  if (priv->value_widget)
    update_label_request (scale);
}
