/* GTK - The GIMP Toolkit
 * Copyright © 2012 Red Hat, Inc.
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
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

/**
 * GtkLevelBar:
 *
 * `GtkLevelBar` is a widget that can be used as a level indicator.
 *
 * Typical use cases are displaying the strength of a password, or
 * showing the charge level of a battery.
 *
 * ![An example GtkLevelBar](levelbar.png)
 *
 * Use [method@Gtk.LevelBar.set_value] to set the current value, and
 * [method@Gtk.LevelBar.add_offset_value] to set the value offsets at which
 * the bar will be considered in a different state. GTK will add a few
 * offsets by default on the level bar: %GTK_LEVEL_BAR_OFFSET_LOW,
 * %GTK_LEVEL_BAR_OFFSET_HIGH and %GTK_LEVEL_BAR_OFFSET_FULL, with
 * values 0.25, 0.75 and 1.0 respectively.
 *
 * Note that it is your responsibility to update preexisting offsets
 * when changing the minimum or maximum value. GTK will simply clamp
 * them to the new range.
 *
 * ## Adding a custom offset on the bar
 *
 * ```c
 * static GtkWidget *
 * create_level_bar (void)
 * {
 *   GtkWidget *widget;
 *   GtkLevelBar *bar;
 *
 *   widget = gtk_level_bar_new ();
 *   bar = GTK_LEVEL_BAR (widget);
 *
 *   // This changes the value of the default low offset
 *
 *   gtk_level_bar_add_offset_value (bar,
 *                                   GTK_LEVEL_BAR_OFFSET_LOW,
 *                                   0.10);
 *
 *   // This adds a new offset to the bar; the application will
 *   // be able to change its color CSS like this:
 *   //
 *   // levelbar block.my-offset {
 *   //   background-color: magenta;
 *   //   border-style: solid;
 *   //   border-color: black;
 *   //   border-width: 1px;
 *   // }
 *
 *   gtk_level_bar_add_offset_value (bar, "my-offset", 0.60);
 *
 *   return widget;
 * }
 * ```
 *
 * The default interval of values is between zero and one, but it’s possible
 * to modify the interval using [method@Gtk.LevelBar.set_min_value] and
 * [method@Gtk.LevelBar.set_max_value]. The value will be always drawn in
 * proportion to the admissible interval, i.e. a value of 15 with a specified
 * interval between 10 and 20 is equivalent to a value of 0.5 with an interval
 * between 0 and 1. When %GTK_LEVEL_BAR_MODE_DISCRETE is used, the bar level
 * is rendered as a finite number of separated blocks instead of a single one.
 * The number of blocks that will be rendered is equal to the number of units
 * specified by the admissible interval.
 *
 * For instance, to build a bar rendered with five blocks, it’s sufficient to
 * set the minimum value to 0 and the maximum value to 5 after changing the
 * indicator mode to discrete.
 *
 * # GtkLevelBar as GtkBuildable
 *
 * The `GtkLevelBar` implementation of the `GtkBuildable` interface supports a
 * custom `<offsets>` element, which can contain any number of `<offset>` elements,
 * each of which must have "name" and "value" attributes.
 *
 * # CSS nodes
 *
 * ```
 * levelbar[.discrete]
 * ╰── trough
 *     ├── block.filled.level-name
 *     ┊
 *     ├── block.empty
 *     ┊
 * ```
 *
 * `GtkLevelBar` has a main CSS node with name levelbar and one of the style
 * classes .discrete or .continuous and a subnode with name trough. Below the
 * trough node are a number of nodes with name block and style class .filled
 * or .empty. In continuous mode, there is exactly one node of each, in discrete
 * mode, the number of filled and unfilled nodes corresponds to blocks that are
 * drawn. The block.filled nodes also get a style class .level-name corresponding
 * to the level for the current value.
 *
 * In horizontal orientation, the nodes are always arranged from left to right,
 * regardless of text direction.
 *
 * # Accessibility
 *
 * `GtkLevelBar` uses the %GTK_ACCESSIBLE_ROLE_METER role.
 */
#include "config.h"

#include "gtkaccessiblerange.h"
#include "gtkbinlayout.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkgizmoprivate.h"
#include "gtklevelbar.h"
#include "gtkmarshalers.h"
#include "gtkorientable.h"
#include "gtkcssnodeprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"

#include <math.h>
#include <stdlib.h>

enum {
  PROP_VALUE = 1,
  PROP_MIN_VALUE,
  PROP_MAX_VALUE,
  PROP_MODE,
  PROP_INVERTED,
  LAST_PROPERTY,
  PROP_ORIENTATION /* overridden */
};

enum {
  SIGNAL_OFFSET_CHANGED,
  NUM_SIGNALS
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };
static guint signals[NUM_SIGNALS] = { 0, };

typedef struct _GtkLevelBarClass   GtkLevelBarClass;

typedef struct {
  char *name;
  double value;
} GtkLevelBarOffset;

struct _GtkLevelBar {
  GtkWidget parent_instance;

  GtkOrientation orientation;

  GtkLevelBarMode bar_mode;

  double min_value;
  double max_value;
  double cur_value;

  GList *offsets;

  GtkWidget *trough_widget;
  GtkWidget **block_widget;
  guint n_blocks;

  guint inverted : 1;
};

struct _GtkLevelBarClass {
  GtkWidgetClass parent_class;

  void (* offset_changed) (GtkLevelBar *self,
                           const char *name);
};

static void gtk_level_bar_set_value_internal (GtkLevelBar *self,
                                              double       value);

static void gtk_level_bar_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkLevelBar, gtk_level_bar, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE_RANGE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_level_bar_buildable_init))

static GtkLevelBarOffset *
gtk_level_bar_offset_new (const char *name,
                          double       value)
{
  GtkLevelBarOffset *offset = g_new0 (GtkLevelBarOffset, 1);

  offset->name = g_strdup (name);
  offset->value = value;

  return offset;
}

static void
gtk_level_bar_offset_free (GtkLevelBarOffset *offset)
{
  g_free (offset->name);
  g_free (offset);
}

static int
offset_find_func (gconstpointer data,
                  gconstpointer user_data)
{
  const GtkLevelBarOffset *offset = data;
  const char *name = user_data;

  return g_strcmp0 (name, offset->name);
}

static int
offset_sort_func (gconstpointer a,
                  gconstpointer b)
{
  const GtkLevelBarOffset *offset_a = a;
  const GtkLevelBarOffset *offset_b = b;

  return (offset_a->value > offset_b->value);
}

static gboolean
gtk_level_bar_ensure_offset (GtkLevelBar *self,
                             const char *name,
                             double       value)
{
  GList *existing;
  GtkLevelBarOffset *offset = NULL;
  GtkLevelBarOffset *new_offset;

  existing = g_list_find_custom (self->offsets, name, offset_find_func);
  if (existing)
    offset = existing->data;

  if (offset && (offset->value == value))
    return FALSE;

  new_offset = gtk_level_bar_offset_new (name, value);

  if (offset)
    {
      gtk_level_bar_offset_free (offset);
      self->offsets = g_list_delete_link (self->offsets, existing);
    }

  self->offsets = g_list_insert_sorted (self->offsets, new_offset, offset_sort_func);

  return TRUE;
}

#ifndef G_DISABLE_CHECKS
static gboolean
gtk_level_bar_value_in_interval (GtkLevelBar *self,
                                 double       value)
{
  return ((value >= self->min_value) &&
          (value <= self->max_value));
}
#endif

static int
gtk_level_bar_get_num_blocks (GtkLevelBar *self)
{
  if (self->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    return 1;
  else if (self->bar_mode == GTK_LEVEL_BAR_MODE_DISCRETE)
    return MAX (1, (int) (round (self->max_value) - round (self->min_value)));

  return 0;
}

static int
gtk_level_bar_get_num_block_nodes (GtkLevelBar *self)
{
  if (self->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    return 2;
  else
    return gtk_level_bar_get_num_blocks (self);
}

static void
gtk_level_bar_get_min_block_size (GtkLevelBar *self,
                                  int         *block_width,
                                  int         *block_height)
{
  guint i, n_blocks;
  int width, height;

  *block_width = *block_height = 0;
  n_blocks = gtk_level_bar_get_num_block_nodes (self);

  for (i = 0; i < n_blocks; i++)
    {
      gtk_widget_measure (self->block_widget[i],
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          &width, NULL,
                          NULL, NULL);
      gtk_widget_measure (self->block_widget[i],
                          GTK_ORIENTATION_VERTICAL,
                          -1,
                          &height, NULL,
                          NULL, NULL);

      *block_width = MAX (width, *block_width);
      *block_height = MAX (height, *block_height);
    }
}

static gboolean
gtk_level_bar_get_real_inverted (GtkLevelBar *self)
{
  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL &&
      self->orientation == GTK_ORIENTATION_HORIZONTAL)
    return !self->inverted;

  return self->inverted;
}

static void
gtk_level_bar_render_trough (GtkGizmo    *gizmo,
                             GtkSnapshot *snapshot)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkLevelBar *self = GTK_LEVEL_BAR (gtk_widget_get_parent (GTK_WIDGET (gizmo)));

  if (self->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    {
      gboolean inverted;

      inverted = gtk_level_bar_get_real_inverted (self);

      /* render the empty (unfilled) part */
      gtk_widget_snapshot_child (widget, self->block_widget[inverted ? 0 : 1], snapshot);

      /* now render the filled part on top of it */
      if (self->cur_value != 0)
        gtk_widget_snapshot_child (widget, self->block_widget[inverted ? 1 : 0], snapshot);
    }
  else
    {
      int num_blocks, i;

      num_blocks = gtk_level_bar_get_num_blocks (self);

      for (i = 0; i < num_blocks; i++)
        gtk_widget_snapshot_child (widget, self->block_widget[i], snapshot);
    }
}

static void
gtk_level_bar_measure_trough (GtkGizmo       *gizmo,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkLevelBar *self = GTK_LEVEL_BAR (gtk_widget_get_parent (widget));
  int num_blocks, size;
  int block_width, block_height;

  num_blocks = gtk_level_bar_get_num_blocks (self);
  gtk_level_bar_get_min_block_size (self, &block_width, &block_height);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        size = num_blocks * block_width;
      else
        size = block_width;
    }
  else
    {
      if (self->orientation == GTK_ORIENTATION_VERTICAL)
        size = num_blocks * block_height;
      else
        size = block_height;
    }

  *minimum = size;
  *natural = size;
}

static void
gtk_level_bar_allocate_trough_continuous (GtkLevelBar *self,
                                          int          width,
                                          int          height,
                                          int          baseline)
{
  GtkAllocation block_area;
  double fill_percentage;
  gboolean inverted;
  int block_min;

  inverted = gtk_level_bar_get_real_inverted (self);

  /* allocate the empty (unfilled) part */
  gtk_widget_size_allocate (self->block_widget[inverted ? 0 : 1],
                            &(GtkAllocation) {0, 0, width, height},
                            baseline);

  if (self->cur_value == 0)
    return;

  /* now allocate the filled part */
  block_area = (GtkAllocation) {0, 0, width, height};
  fill_percentage = (self->cur_value - self->min_value) /
    (self->max_value - self->min_value);

  gtk_widget_measure (self->block_widget[inverted ? 1 : 0],
                      self->orientation, -1,
                      &block_min, NULL,
                      NULL, NULL);

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      block_area.width = (int) floor (block_area.width * fill_percentage);
      block_area.width = MAX (block_area.width, block_min);

      if (inverted)
        block_area.x += width - block_area.width;
    }
  else
    {
      block_area.height = (int) floor (block_area.height * fill_percentage);
      block_area.height = MAX (block_area.height, block_min);

      if (inverted)
        block_area.y += height - block_area.height;
    }

  gtk_widget_size_allocate (self->block_widget[inverted ? 1 : 0],
                            &block_area,
                            baseline);
}

static void
gtk_level_bar_allocate_trough_discrete (GtkLevelBar *self,
                                        int          width,
                                        int          height,
                                        int          baseline)
{
  GtkAllocation block_area;
  int num_blocks, i;
  int block_width, block_height;
  int extra_space;

  gtk_level_bar_get_min_block_size (self, &block_width, &block_height);
  num_blocks = gtk_level_bar_get_num_blocks (self);

  if (num_blocks == 0)
    return;

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      block_width = MAX (block_width, (int) floor ((double) width / num_blocks));
      block_height = height;
      extra_space = width - block_width * num_blocks;

      if (extra_space > 0)
        block_width++;
    }
  else
    {
      block_width = width;
      block_height = MAX (block_height, (int) floor ((double) height / num_blocks));
      extra_space = height - block_height * num_blocks;

      if (extra_space > 0)
        block_height++;
    }

  block_area.x = 0;
  block_area.y = 0;
  block_area.width = block_width;
  block_area.height = block_height;

  for (i = 0; i < num_blocks; i++)
    {
      if (extra_space > 0 && i == extra_space)
        {
          if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
            block_area.width--;
          else
            block_area.height--;
        }

      gtk_widget_size_allocate (self->block_widget[i],
                                &block_area,
                                baseline);

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        block_area.x += block_area.width;
      else
        block_area.y += block_area.height;
    }
}

static void
gtk_level_bar_allocate_trough (GtkGizmo *gizmo,
                               int       width,
                               int       height,
                               int       baseline)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkLevelBar *self = GTK_LEVEL_BAR (gtk_widget_get_parent (widget));

  if (self->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    gtk_level_bar_allocate_trough_continuous (self, width, height, baseline);
  else
    gtk_level_bar_allocate_trough_discrete (self, width, height, baseline);
}

static void
update_block_nodes (GtkLevelBar *self)
{
  guint n_blocks;
  guint i;

  n_blocks = gtk_level_bar_get_num_block_nodes (self);

  if (self->n_blocks == n_blocks)
    return;
  else if (n_blocks < self->n_blocks)
    {
      for (i = n_blocks; i < self->n_blocks; i++)
        {
          gtk_widget_unparent (self->block_widget[i]);
        }
      self->block_widget = g_renew (GtkWidget*, self->block_widget, n_blocks);
      self->n_blocks = n_blocks;
    }
  else
    {
      self->block_widget = g_renew (GtkWidget*, self->block_widget, n_blocks);
      for (i = self->n_blocks; i < n_blocks; i++)
        {
          self->block_widget[i] = gtk_gizmo_new_with_role ("block",
                                                           GTK_ACCESSIBLE_ROLE_NONE,
                                                           NULL, NULL, NULL, NULL, NULL, NULL);
          gtk_widget_insert_before (self->block_widget[i], GTK_WIDGET (self->trough_widget), NULL);
        }
      self->n_blocks = n_blocks;
    }
}

static void
update_mode_style_classes (GtkLevelBar *self)
{
  GtkCssNode *widget_node;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  if (self->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    {
      gtk_css_node_remove_class (widget_node, g_quark_from_static_string ("discrete"));
      gtk_css_node_add_class (widget_node, g_quark_from_static_string ("continuous"));
    }
  else if (self->bar_mode == GTK_LEVEL_BAR_MODE_DISCRETE)
    {
      gtk_css_node_add_class (widget_node, g_quark_from_static_string ("discrete"));
      gtk_css_node_remove_class (widget_node, g_quark_from_static_string ("continuous"));
    }
}

static void
update_level_style_classes (GtkLevelBar *self)
{
  double value;
  const char *value_class = NULL;
  GtkLevelBarOffset *offset, *prev_offset;
  GList *l;
  int num_filled, num_blocks, i;
  gboolean inverted;

  value = gtk_level_bar_get_value (self);

  for (l = self->offsets; l != NULL; l = l->next)
    {
      offset = l->data;

      /* find the right offset for our style class */
      if (value <= offset->value)
        {
          if (l->prev == NULL)
            {
              value_class = offset->name;
            }
          else
            {
              prev_offset = l->prev->data;
              if (prev_offset->value < value)
                value_class = offset->name;
            }
        }

      if (value_class)
        break;
    }

  inverted = gtk_level_bar_get_real_inverted (self);
  num_blocks = gtk_level_bar_get_num_block_nodes (self);

  if (self->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    num_filled = 1;
  else
    num_filled = MIN (num_blocks, (int) round (self->cur_value) - (int) round (self->min_value));

  for (i = 0; i < num_filled; i++)
    {
      GtkCssNode *node = gtk_widget_get_css_node (self->block_widget[inverted ? num_blocks - 1 - i : i]);

      gtk_css_node_set_classes (node, NULL);
      gtk_css_node_add_class (node, g_quark_from_static_string ("filled"));

      if (value_class)
        gtk_css_node_add_class (node, g_quark_from_string (value_class));
    }

  for (; i < num_blocks; i++)
    {
      GtkCssNode *node = gtk_widget_get_css_node (self->block_widget[inverted ? num_blocks - 1 - i : i]);

      gtk_css_node_set_classes (node, NULL);
      gtk_css_node_add_class (node, g_quark_from_static_string ("empty"));
    }
}

static void
gtk_level_bar_direction_changed (GtkWidget        *widget,
                                 GtkTextDirection  previous_dir)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (widget);

  update_level_style_classes (self);

  GTK_WIDGET_CLASS (gtk_level_bar_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_level_bar_ensure_offsets_in_range (GtkLevelBar *self)
{
  GtkLevelBarOffset *offset;
  GList *l = self->offsets;

  while (l != NULL)
    {
      offset = l->data;
      l = l->next;

      if (offset->value < self->min_value)
        gtk_level_bar_ensure_offset (self, offset->name, self->min_value);
      else if (offset->value > self->max_value)
        gtk_level_bar_ensure_offset (self, offset->name, self->max_value);
    }
}

typedef struct {
  GtkLevelBar *self;
  GtkBuilder *builder;
  GList *offsets;
} OffsetsParserData;

static void
offset_start_element (GtkBuildableParseContext *context,
                      const char               *element_name,
                      const char              **names,
                      const char              **values,
                      gpointer                  user_data,
                      GError                  **error)
{
  OffsetsParserData *data = user_data;

  if (strcmp (element_name, "offsets") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "offset") == 0)
    {
      const char *name;
      const char *value;
      GValue gvalue = G_VALUE_INIT;
      GtkLevelBarOffset *offset;

      if (!_gtk_builder_check_parent (data->builder, context, "offsets", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "value", &value,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_DOUBLE, value, &gvalue, error))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      offset = gtk_level_bar_offset_new (name, g_value_get_double (&gvalue));
      data->offsets = g_list_prepend (data->offsets, offset);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkLevelBar", element_name,
                                        error);
    }
}

static const GtkBuildableParser offset_parser =
{
  offset_start_element
};

static GtkBuildableIface *parent_buildable_iface;

static gboolean
gtk_level_bar_buildable_custom_tag_start (GtkBuildable       *buildable,
                                          GtkBuilder         *builder,
                                          GObject            *child,
                                          const char         *tagname,
                                          GtkBuildableParser *parser,
                                          gpointer           *parser_data)
{
  OffsetsParserData *data;

  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, parser_data))
    return TRUE;

  if (child)
    return FALSE;

  if (strcmp (tagname, "offsets") != 0)
    return FALSE;

  data = g_new0 (OffsetsParserData, 1);
  data->self = GTK_LEVEL_BAR (buildable);
  data->builder = builder;
  data->offsets = NULL;

  *parser = offset_parser;
  *parser_data = data;

  return TRUE;
}

static void
gtk_level_bar_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const char   *tagname,
                                         gpointer      user_data)
{
  OffsetsParserData *data = user_data;
  GtkLevelBar *self;
  GtkLevelBarOffset *offset;
  GList *l;

  self = data->self;

  if (strcmp (tagname, "offsets") != 0)
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
                                               tagname, user_data);
      return;
    }

  for (l = data->offsets; l != NULL; l = l->next)
    {
      offset = l->data;
      gtk_level_bar_add_offset_value (self, offset->name, offset->value);
    }

  g_list_free_full (data->offsets, (GDestroyNotify) gtk_level_bar_offset_free);
  g_free (data);
}

static void
gtk_level_bar_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_level_bar_buildable_custom_tag_start;
  iface->custom_finished = gtk_level_bar_buildable_custom_finished;
}

static void
gtk_level_bar_set_orientation (GtkLevelBar    *self,
                               GtkOrientation  orientation)
{
  if (self->orientation != orientation)
    {
      self->orientation = orientation;
      gtk_widget_update_orientation (GTK_WIDGET (self), self->orientation);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify (G_OBJECT (self), "orientation");
    }
}

static void
gtk_level_bar_get_property (GObject    *obj,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (obj);

  switch (property_id)
    {
    case PROP_VALUE:
      g_value_set_double (value, gtk_level_bar_get_value (self));
      break;
    case PROP_MIN_VALUE:
      g_value_set_double (value, gtk_level_bar_get_min_value (self));
      break;
    case PROP_MAX_VALUE:
      g_value_set_double (value, gtk_level_bar_get_max_value (self));
      break;
    case PROP_MODE:
      g_value_set_enum (value, gtk_level_bar_get_mode (self));
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, gtk_level_bar_get_inverted (self));
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_level_bar_set_property (GObject      *obj,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (obj);

  switch (property_id)
    {
    case PROP_VALUE:
      gtk_level_bar_set_value (self, g_value_get_double (value));
      break;
    case PROP_MIN_VALUE:
      gtk_level_bar_set_min_value (self, g_value_get_double (value));
      break;
    case PROP_MAX_VALUE:
      gtk_level_bar_set_max_value (self, g_value_get_double (value));
      break;
    case PROP_MODE:
      gtk_level_bar_set_mode (self, g_value_get_enum (value));
      break;
    case PROP_INVERTED:
      gtk_level_bar_set_inverted (self, g_value_get_boolean (value));
      break;
    case PROP_ORIENTATION:
      gtk_level_bar_set_orientation (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_level_bar_finalize (GObject *obj)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (obj);
  int i;

  g_list_free_full (self->offsets, (GDestroyNotify) gtk_level_bar_offset_free);

  for (i = 0; i < self->n_blocks; i++)
    gtk_widget_unparent (self->block_widget[i]);

  g_free (self->block_widget);

  gtk_widget_unparent (self->trough_widget);

  G_OBJECT_CLASS (gtk_level_bar_parent_class)->finalize (obj);
}

static void
gtk_level_bar_class_init (GtkLevelBarClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  oclass->get_property = gtk_level_bar_get_property;
  oclass->set_property = gtk_level_bar_set_property;
  oclass->finalize = gtk_level_bar_finalize;

  wclass->direction_changed = gtk_level_bar_direction_changed;

  g_object_class_override_property (oclass, PROP_ORIENTATION, "orientation");

  /**
   * GtkLevelBar::offset-changed:
   * @self: a `GtkLevelBar`
   * @name: the name of the offset that changed value
   *
   * Emitted when an offset specified on the bar changes value.
   *
   * This typically is the result of a [method@Gtk.LevelBar.add_offset_value]
   * call.
   *
   * The signal supports detailed connections; you can connect to the
   * detailed signal "changed::x" in order to only receive callbacks when
   * the value of offset "x" changes.
   */
  signals[SIGNAL_OFFSET_CHANGED] =
    g_signal_new (I_("offset-changed"),
                  GTK_TYPE_LEVEL_BAR,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GtkLevelBarClass, offset_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  /**
   * GtkLevelBar:value: (attributes org.gtk.Property.get=gtk_level_bar_get_value org.gtk.Property.set=gtk_level_bar_set_value)
   *
   * Determines the currently filled value of the level bar.
   */
  properties[PROP_VALUE] =
    g_param_spec_double ("value", NULL, NULL,
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:min-value: (attributes org.gtk.Property.get=gtk_level_bar_get_min_value org.gtk.Property.set=gtk_level_bar_set_min_value)
   *
   * Determines the minimum value of the interval that can be displayed by the bar.
   */
  properties[PROP_MIN_VALUE] =
    g_param_spec_double ("min-value", NULL, NULL,
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:max-value: (attributes org.gtk.Property.get=gtk_level_bar_get_max_value org.gtk.Property.set=gtk_level_bar_set_max_value)
   *
   * Determines the maximum value of the interval that can be displayed by the bar.
   */
  properties[PROP_MAX_VALUE] =
    g_param_spec_double ("max-value", NULL, NULL,
                         0.0, G_MAXDOUBLE, 1.0,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:mode: (attributes org.gtk.Property.get=gtk_level_bar_get_mode org.gtk.Property.set=gtk_level_bar_set_mode)
   *
   * Determines the way `GtkLevelBar` interprets the value properties to draw the
   * level fill area.
   *
   * Specifically, when the value is %GTK_LEVEL_BAR_MODE_CONTINUOUS,
   * `GtkLevelBar` will draw a single block representing the current value in
   * that area; when the value is %GTK_LEVEL_BAR_MODE_DISCRETE,
   * the widget will draw a succession of separate blocks filling the
   * draw area, with the number of blocks being equal to the units separating
   * the integral roundings of [property@Gtk.LevelBar:min-value] and
   * [property@Gtk.LevelBar:max-value].
   */
  properties[PROP_MODE] =
    g_param_spec_enum ("mode", NULL, NULL,
                       GTK_TYPE_LEVEL_BAR_MODE,
                       GTK_LEVEL_BAR_MODE_CONTINUOUS,
                       G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:inverted: (attributes org.gtk.Property.get=gtk_level_bar_get_inverted org.gtk.Property.set=gtk_level_bar_set_inverted)
   *
   * Whether the `GtkLeveBar` is inverted.
   *
   * Level bars normally grow from top to bottom or left to right.
   * Inverted level bars grow in the opposite direction.
   */
  properties[PROP_INVERTED] =
    g_param_spec_boolean ("inverted", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (oclass, LAST_PROPERTY, properties);

  gtk_widget_class_set_layout_manager_type (wclass, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (wclass, I_("levelbar"));
  gtk_widget_class_set_accessible_role (wclass, GTK_ACCESSIBLE_ROLE_METER);
}

static void
gtk_level_bar_init (GtkLevelBar *self)
{
  self->cur_value = 0.0;
  self->min_value = 0.0;
  self->max_value = 1.0;

  /* set initial orientation and style classes */
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  gtk_widget_update_orientation (GTK_WIDGET (self), self->orientation);

  self->inverted = FALSE;

  self->trough_widget = gtk_gizmo_new_with_role ("trough",
                                                 GTK_ACCESSIBLE_ROLE_NONE,
                                                 gtk_level_bar_measure_trough,
                                                 gtk_level_bar_allocate_trough,
                                                 gtk_level_bar_render_trough,
                                                 NULL,
                                                 NULL, NULL);
  gtk_widget_set_parent (self->trough_widget, GTK_WIDGET (self));

  gtk_level_bar_ensure_offset (self, GTK_LEVEL_BAR_OFFSET_LOW, 0.25);
  gtk_level_bar_ensure_offset (self, GTK_LEVEL_BAR_OFFSET_HIGH, 0.75);
  gtk_level_bar_ensure_offset (self, GTK_LEVEL_BAR_OFFSET_FULL, 1.0);

  self->block_widget = NULL;
  self->n_blocks = 0;

  self->bar_mode = GTK_LEVEL_BAR_MODE_CONTINUOUS;
  update_mode_style_classes (self);
  update_block_nodes (self);
  update_level_style_classes (self);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 1.0,
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.0,
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.0,
                                  -1);
}

/**
 * gtk_level_bar_new:
 *
 * Creates a new `GtkLevelBar`.
 *
 * Returns: a `GtkLevelBar`.
 */
GtkWidget *
gtk_level_bar_new (void)
{
  return g_object_new (GTK_TYPE_LEVEL_BAR, NULL);
}

/**
 * gtk_level_bar_new_for_interval:
 * @min_value: a positive value
 * @max_value: a positive value
 *
 * Creates a new `GtkLevelBar` for the specified interval.
 *
 * Returns: a `GtkLevelBar`
 */
GtkWidget *
gtk_level_bar_new_for_interval (double min_value,
                                double max_value)
{
  return g_object_new (GTK_TYPE_LEVEL_BAR,
                       "min-value", min_value,
                       "max-value", max_value,
                       NULL);
}

/**
 * gtk_level_bar_get_min_value: (attributes org.gtk.Method.get_property=min-value)
 * @self: a `GtkLevelBar`
 *
 * Returns the `min-value` of the `GtkLevelBar`.
 *
 * Returns: a positive value
 */
double
gtk_level_bar_get_min_value (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return self->min_value;
}

/**
 * gtk_level_bar_get_max_value: (attributes org.gtk.Method.get_property=max-value)
 * @self: a `GtkLevelBar`
 *
 * Returns the `max-value` of the `GtkLevelBar`.
 *
 * Returns: a positive value
 */
double
gtk_level_bar_get_max_value (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return self->max_value;
}

/**
 * gtk_level_bar_get_value: (attributes org.gtk.Method.get_property=value)
 * @self: a `GtkLevelBar`
 *
 * Returns the `value` of the `GtkLevelBar`.
 *
 * Returns: a value in the interval between
 *   [property@Gtk.LevelBar:min-value] and [property@Gtk.LevelBar:max-value]
 */
double
gtk_level_bar_get_value (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return self->cur_value;
}

static void
gtk_level_bar_set_value_internal (GtkLevelBar *self,
                                  double       value)
{
  self->cur_value = value;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);

  gtk_widget_queue_allocate (GTK_WIDGET (self->trough_widget));
}

/**
 * gtk_level_bar_set_min_value: (attributes org.gtk.Method.set_property=min-value)
 * @self: a `GtkLevelBar`
 * @value: a positive value
 *
 * Sets the `min-value` of the `GtkLevelBar`.
 *
 * You probably want to update preexisting level offsets after calling
 * this function.
 */
void
gtk_level_bar_set_min_value (GtkLevelBar *self,
                             double       value)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));
  g_return_if_fail (value >= 0.0);

  if (value == self->min_value)
    return;

  self->min_value = value;

  if (self->min_value > self->cur_value)
    gtk_level_bar_set_value_internal (self, self->min_value);

  update_block_nodes (self);
  update_level_style_classes (self);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, self->min_value,
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, self->cur_value,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_VALUE]);
}

/**
 * gtk_level_bar_set_max_value: (attributes org.gtk.Method.set_property=max-value)
 * @self: a `GtkLevelBar`
 * @value: a positive value
 *
 * Sets the `max-value` of the `GtkLevelBar`.
 *
 * You probably want to update preexisting level offsets after calling
 * this function.
 */
void
gtk_level_bar_set_max_value (GtkLevelBar *self,
                             double       value)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));
  g_return_if_fail (value >= 0.0);

  if (value == self->max_value)
    return;

  self->max_value = value;

  if (self->max_value < self->cur_value)
    gtk_level_bar_set_value_internal (self, self->max_value);

  gtk_level_bar_ensure_offsets_in_range (self);
  update_block_nodes (self);
  update_level_style_classes (self);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, self->max_value,
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, self->cur_value,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_VALUE]);
}

/**
 * gtk_level_bar_set_value: (attributes org.gtk.Method.set_property=value)
 * @self: a `GtkLevelBar`
 * @value: a value in the interval between
 *   [property@Gtk.LevelBar:min-value] and [property@Gtk.LevelBar:max-value]
 *
 * Sets the value of the `GtkLevelBar`.
 */
void
gtk_level_bar_set_value (GtkLevelBar *self,
                         double       value)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (value == self->cur_value)
    return;

  gtk_level_bar_set_value_internal (self, value);
  update_level_style_classes (self);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, self->cur_value,
                                  -1);
}

/**
 * gtk_level_bar_get_mode: (attributes org.gtk.Method.get_property=mode)
 * @self: a `GtkLevelBar`
 *
 * Returns the `mode` of the `GtkLevelBar`.
 *
 * Returns: a `GtkLevelBarMode`
 */
GtkLevelBarMode
gtk_level_bar_get_mode (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0);

  return self->bar_mode;
}

/**
 * gtk_level_bar_set_mode: (attributes org.gtk.Method.set_property=mode)
 * @self: a `GtkLevelBar`
 * @mode: a `GtkLevelBarMode`
 *
 * Sets the `mode` of the `GtkLevelBar`.
 */
void
gtk_level_bar_set_mode (GtkLevelBar     *self,
                        GtkLevelBarMode  mode)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (self->bar_mode == mode)
    return;

  self->bar_mode = mode;

  update_mode_style_classes (self);
  update_block_nodes (self);
  update_level_style_classes (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODE]);

}

/**
 * gtk_level_bar_get_inverted: (attributes org.gtk.Method.get_property=inverted)
 * @self: a `GtkLevelBar`
 *
 * Returns whether the levelbar is inverted.
 *
 * Returns: %TRUE if the level bar is inverted
 */
gboolean
gtk_level_bar_get_inverted (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), FALSE);

  return self->inverted;
}

/**
 * gtk_level_bar_set_inverted: (attributes org.gtk.Method.set_property=inverted)
 * @self: a `GtkLevelBar`
 * @inverted: %TRUE to invert the level bar
 *
 * Sets whether the `GtkLevelBar` is inverted.
 */
void
gtk_level_bar_set_inverted (GtkLevelBar *self,
                            gboolean     inverted)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (self->inverted == inverted)
    return;

  self->inverted = inverted;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  update_level_style_classes (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INVERTED]);
}

/**
 * gtk_level_bar_remove_offset_value:
 * @self: a `GtkLevelBar`
 * @name: (nullable): the name of an offset in the bar
 *
 * Removes an offset marker from a `GtkLevelBar`.
 *
 * The marker must have been previously added with
 * [method@Gtk.LevelBar.add_offset_value].
 */
void
gtk_level_bar_remove_offset_value (GtkLevelBar *self,
                                   const char *name)
{
  GList *existing;

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  existing = g_list_find_custom (self->offsets, name, offset_find_func);
  if (existing)
    {
      gtk_level_bar_offset_free (existing->data);
      self->offsets = g_list_delete_link (self->offsets, existing);

      update_level_style_classes (self);
    }
}

/**
 * gtk_level_bar_add_offset_value:
 * @self: a `GtkLevelBar`
 * @name: the name of the new offset
 * @value: the value for the new offset
 *
 * Adds a new offset marker on @self at the position specified by @value.
 *
 * When the bar value is in the interval topped by @value (or between @value
 * and [property@Gtk.LevelBar:max-value] in case the offset is the last one
 * on the bar) a style class named `level-`@name will be applied
 * when rendering the level bar fill.
 *
 * If another offset marker named @name exists, its value will be
 * replaced by @value.
 */
void
gtk_level_bar_add_offset_value (GtkLevelBar *self,
                                const char *name,
                                double       value)
{
  GQuark name_quark;

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));
  g_return_if_fail (gtk_level_bar_value_in_interval (self, value));

  if (!gtk_level_bar_ensure_offset (self, name, value))
    return;

  update_level_style_classes (self);
  name_quark = g_quark_from_string (name);
  g_signal_emit (self, signals[SIGNAL_OFFSET_CHANGED], name_quark, name);
}

/**
 * gtk_level_bar_get_offset_value:
 * @self: a `GtkLevelBar`
 * @name: (nullable): the name of an offset in the bar
 * @value: (out): location where to store the value
 *
 * Fetches the value specified for the offset marker @name in @self.
 *
 * Returns: %TRUE if the specified offset is found
 */
gboolean
gtk_level_bar_get_offset_value (GtkLevelBar *self,
                                const char *name,
                                double      *value)
{
  GList *existing;
  GtkLevelBarOffset *offset = NULL;

  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), FALSE);

  existing = g_list_find_custom (self->offsets, name, offset_find_func);
  if (existing)
    offset = existing->data;

  if (!offset)
    return FALSE;

  if (value)
    *value = offset->value;

  return TRUE;
}
