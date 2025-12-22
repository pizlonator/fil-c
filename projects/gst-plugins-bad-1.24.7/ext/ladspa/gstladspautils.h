/* GStreamer
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
 *               2013 Stefan Sauer <ensonic@users.sf.net>
 *
 * gstladspautils.h: Header for LADSPA plugin utils
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

#ifndef __GST_LADSPA_UTILS_H__
#define __GST_LADSPA_UTILS_H__

#include <gmodule.h>
#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>
#include <gst/base/gstbasesrc.h>
#include <gst/base/gstbasesink.h>

#include <ladspa.h>

G_BEGIN_DECLS

typedef struct _GstLADSPA GstLADSPA;

typedef struct _GstLADSPAClass GstLADSPAClass;

struct _GstLADSPA
{
  GstLADSPAClass *klass;

  LADSPA_Handle *handle;
  gboolean activated;
  unsigned long rate;

  struct
  {
    struct
    {
      LADSPA_Data *in;
      LADSPA_Data *out;
    } control;

    struct
    {
      LADSPA_Data **in;
      LADSPA_Data **out;
    } audio;
  } ports;
};

struct _GstLADSPAClass
{
  guint properties;

  GModule *plugin;
  const LADSPA_Descriptor *descriptor;

  struct
  {
    struct
    {
      guint in;
      guint out;
    } control;

    struct
    {
      guint in;
      guint out;
    } audio;
  } count;

  struct
  {
    struct
    {
      unsigned long *in;
      unsigned long *out;
    } control;

    struct
    {
      unsigned long *in;
      unsigned long *out;
    } audio;
  } map;
};

gboolean
gst_ladspa_transform (GstLADSPA * ladspa, guint8 * outdata, guint samples,
    guint8 * indata);

gboolean
gst_ladspa_setup (GstLADSPA * ladspa, unsigned long rate);

gboolean
gst_ladspa_cleanup (GstLADSPA * ladspa);

void
gst_ladspa_object_set_property (GstLADSPA * ladspa, GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

void
gst_ladspa_object_get_property (GstLADSPA * ladspa, GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

void
gst_ladspa_object_class_install_properties (GstLADSPAClass * ladspa_class,
    GObjectClass * object_class, guint offset);

void
gst_ladspa_element_class_set_metadata (GstLADSPAClass * ladspa_class,
    GstElementClass * elem_class, const gchar * ladspa_class_tags);

void
gst_ladspa_filter_type_class_add_pad_templates (GstLADSPAClass * ladspa_class,
    GstAudioFilterClass * audio_class);

void
gst_ladspa_source_type_class_add_pad_template (GstLADSPAClass * ladspa_class,
    GstBaseSrcClass * audio_class);

void
gst_ladspa_sink_type_class_add_pad_template (GstLADSPAClass * ladspa_class,
    GstBaseSinkClass * base_class);

void
gst_ladspa_init (GstLADSPA * ladspa, GstLADSPAClass * ladspa_class);

void
gst_ladspa_finalize (GstLADSPA * ladspa);

void
gst_ladspa_class_init (GstLADSPAClass * ladspa_class, GType type);

void
gst_ladspa_class_finalize (GstLADSPAClass * ladspa_class);

void
ladspa_register_element (GstPlugin * plugin, GType parent_type,
    const GTypeInfo * info, GstStructure * ladspa_meta);

G_END_DECLS

#endif /* __GST_LADSPA_UTILS_H__ */
