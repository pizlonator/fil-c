/* GStreamer
 * Copyright (C) 2016 Thibault Saunier <thibault.saunier@collabora.com>
 *               2016 Stefan Sauer <ensonic@users.sf.net>
 *
 * gstlv2utils.h: Header for LV2 plugin utils
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

#ifndef __GST_LV2_UTILS_H__
#define __GST_LV2_UTILS_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/audio-channels.h>

#include <lilv/lilv.h>

G_BEGIN_DECLS

typedef struct _GstLV2Group GstLV2Group;
typedef struct _GstLV2Port GstLV2Port;

typedef struct _GstLV2 GstLV2;
typedef struct _GstLV2Class GstLV2Class;

struct _GstLV2Group
{
  gchar *uri; /**< RDF resource (URI or blank node) */
  guint pad; /**< Gst pad index */
  gchar *symbol; /**< Gst pad name / LV2 group symbol */
  GArray *ports; /**< Array of GstLV2Port */
  /* FIXME: not set as of now */
  gboolean has_roles; /**< TRUE iff all ports have a known role */
};

typedef enum {
  GST_LV2_PORT_AUDIO = 0,
  GST_LV2_PORT_CONTROL,
  GST_LV2_PORT_CV
} GstLV2PortType;

struct _GstLV2Port
{
  gint index; /**< LV2 port index (on LV2 plugin) */
  GstLV2PortType type; /**< Port type */
  /**< Gst pad index (iff not part of a group), only for audio ports */
  gint pad;
  /* FIXME: not set as of now */
  LilvNode *role; /**< Channel position / port role */
  GstAudioChannelPosition position; /**< Channel position */
};

struct _GstLV2
{
  GstLV2Class *klass;

  LilvInstance *instance;
  GHashTable *presets;

  gboolean activated;
  unsigned long rate;

  struct
  {
    struct
    {
      gfloat *in;
      gfloat *out;
    } control;
  } ports;
};

struct _GstLV2Class
{
  guint properties;

  const LilvPlugin *plugin;
  GHashTable *sym_to_name;

  gint num_control_in, num_control_out;
  gint num_cv_in, num_cv_out;

  GstLV2Group in_group; /**< Array of GstLV2Group */
  GstLV2Group out_group; /**< Array of GstLV2Group */
  GArray *control_in_ports; /**< Array of GstLV2Port */
  GArray *control_out_ports; /**< Array of GstLV2Port */
};

gboolean gst_lv2_check_required_features (const LilvPlugin * lv2plugin);

void gst_lv2_host_init (void);

gchar **gst_lv2_get_preset_names (GstLV2 * lv2, GstObject * obj);
gboolean gst_lv2_load_preset (GstLV2 * lv2, GstObject * obj, const gchar * name);
gboolean gst_lv2_save_preset (GstLV2 * lv2, GstObject * obj, const gchar * name);
gboolean gst_lv2_delete_preset (GstLV2 * lv2, GstObject * obj, const gchar * name);


void gst_lv2_init (GstLV2 * lv2, GstLV2Class * lv2_class);
void gst_lv2_finalize (GstLV2 * lv2);

gboolean gst_lv2_setup (GstLV2 * lv2, unsigned long rate);
gboolean gst_lv2_cleanup (GstLV2 * lv2, GstObject *obj);

void gst_lv2_object_set_property (GstLV2 * lv2, GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
void gst_lv2_object_get_property (GstLV2 * lv2, GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

void gst_lv2_class_install_properties (GstLV2Class * lv2_class,
    GObjectClass * object_class, guint offset);
void gst_lv2_element_class_set_metadata (GstLV2Class * lv2_class,
    GstElementClass * elem_class, const gchar * lv2_class_tags);

void gst_lv2_class_init (GstLV2Class * lv2_class, GType type);
void gst_lv2_class_finalize (GstLV2Class * lv2_class);

G_END_DECLS

#endif /* __GST_LV2_UTILS_H__ */
