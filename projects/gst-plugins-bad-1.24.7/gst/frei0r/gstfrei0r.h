/* GStreamer
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_FREI0R_H__
#define __GST_FREI0R_H__

#include <gst/gst.h>

#include "frei0r.h"

G_BEGIN_DECLS

typedef struct _GstFrei0rFuncTable GstFrei0rFuncTable;
typedef struct _GstFrei0rProperty GstFrei0rProperty;
typedef struct _GstFrei0rPropertyValue GstFrei0rPropertyValue;

struct _GstFrei0rPropertyValue {
  union {
    f0r_param_bool b;
    f0r_param_double d;
    f0r_param_string *s;
    f0r_param_position_t position;
    f0r_param_color_t color;
  } data;
};

struct _GstFrei0rProperty {
  guint prop_id;
  guint n_prop_ids;

  gint prop_idx;
  f0r_param_info_t info;

  GstFrei0rPropertyValue default_value;
};

struct _GstFrei0rFuncTable {
  int (*init) (void);
  void (*deinit) (void);

  f0r_instance_t (*construct) (unsigned int width, unsigned int height);
  void (*destruct) (f0r_instance_t instance);

  void (*get_plugin_info) (f0r_plugin_info_t* info);  
  void (*get_param_info) (f0r_param_info_t* info, int param_index);

  void (*set_param_value) (f0r_instance_t instance, 
			   f0r_param_t param, int param_index);
  void (*get_param_value) (f0r_instance_t instance,
			   f0r_param_t param, int param_index);
  
  void (*update) (f0r_instance_t instance, 
		  double time, const guint32* inframe, guint32* outframe);
  void (*update2) (f0r_instance_t instance,
		   double time,
		   const guint32* inframe1,
		   const guint32* inframe2,
		   const guint32* inframe3,
		   guint32* outframe);
};

typedef enum {
  GST_FREI0R_PLUGIN_REGISTER_RETURN_OK,
  GST_FREI0R_PLUGIN_REGISTER_RETURN_FAILED,
  GST_FREI0R_PLUGIN_REGISTER_RETURN_ALREADY_REGISTERED
} GstFrei0rPluginRegisterReturn;

void gst_frei0r_klass_install_properties (GObjectClass *gobject_class, GstFrei0rFuncTable *ftable, GstFrei0rProperty *properties, gint n_properties);

f0r_instance_t * gst_frei0r_instance_construct (GstFrei0rFuncTable *ftable, GstFrei0rProperty *properties, gint n_properties, GstFrei0rPropertyValue *property_cache, gint width, gint height);

GstFrei0rPropertyValue * gst_frei0r_property_cache_init (GstFrei0rProperty *properties, gint n_properties);
void gst_frei0r_property_cache_free (GstFrei0rProperty *properties, GstFrei0rPropertyValue *property_cache, gint n_properties);

GstCaps * gst_frei0r_caps_from_color_model (gint color_model);
gboolean gst_frei0r_get_property (f0r_instance_t *instance, GstFrei0rFuncTable *ftable, GstFrei0rProperty *properties, gint n_properties, GstFrei0rPropertyValue *property_cache, guint prop_id, GValue *value);
gboolean gst_frei0r_set_property (f0r_instance_t *instance, GstFrei0rFuncTable *ftable, GstFrei0rProperty *properties, gint n_properties, GstFrei0rPropertyValue *property_cache, guint prop_id, const GValue *value);

G_END_DECLS

#endif /* __GST_FREI0R_H__ */
