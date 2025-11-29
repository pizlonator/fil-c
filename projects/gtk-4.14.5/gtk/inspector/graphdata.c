/*
 * Copyright (c) 2014 Benjamin Otte <otte@gnome.org>
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

#include "graphdata.h"

enum {
  PROP_0,
  PROP_N_VALUES
};

struct _GraphData
{
  GObject parent;

  guint n_values;
  guint offset;
  double *values;
};


G_DEFINE_TYPE (GraphData, graph_data, G_TYPE_OBJECT)

static void
graph_data_get_property (GObject    *object,
                         guint       param_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GraphData *graph = GRAPH_DATA (object);

  switch (param_id)
    {
    case PROP_N_VALUES:
      g_value_set_boolean (value, graph->n_values);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
graph_data_finalize (GObject *object)
{
  GraphData *graph = GRAPH_DATA (object);

  g_free (graph->values);

  G_OBJECT_CLASS (graph_data_parent_class)->finalize (object);
}

static void
graph_data_set_property (GObject      *object,
                         guint         param_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GraphData *graph = GRAPH_DATA (object);

  switch (param_id)
    {
    case PROP_N_VALUES:
      graph->n_values = g_value_get_uint (value);
      graph->values = g_new0 (double, graph->n_values);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
graph_data_class_init (GraphDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = graph_data_finalize;
  object_class->get_property = graph_data_get_property;
  object_class->set_property = graph_data_set_property;

  g_object_class_install_property (object_class,
                                   PROP_N_VALUES,
                                   g_param_spec_uint ("n-values", NULL, NULL,
                                                      1, G_MAXUINT, 1,
                                                      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

static void
graph_data_init (GraphData *graph)
{
}

GraphData *
graph_data_new (guint n_values)
{
  return g_object_new (graph_data_get_type (),
                       "n-values", n_values,
                       NULL);
}

guint
graph_data_get_n_values (GraphData *data)
{
  return data->n_values;
}

double
graph_data_get_value (GraphData *data,
                      guint      i)
{
  return data->values[(data->offset + i) % data->n_values];
}

double
graph_data_get_minimum (GraphData *data)
{
  double minimum = G_MAXDOUBLE;
  guint i;

  for (i = 0; i < data->n_values; i++)
    {
      minimum = MIN (minimum, data->values[i]);
    }

  return minimum;
}

double
graph_data_get_maximum (GraphData *data)
{
  double maximum = -G_MAXDOUBLE;
  guint i;

  for (i = 0; i < data->n_values; i++)
    {
      maximum = MAX (maximum, data->values[i]);
    }

  return maximum;
}

void
graph_data_prepend_value (GraphData *data,
                          double     value)
{
  data->offset = (data->offset + data->n_values - 1) % data->n_values;
  data->values[data->offset] = value;
}
