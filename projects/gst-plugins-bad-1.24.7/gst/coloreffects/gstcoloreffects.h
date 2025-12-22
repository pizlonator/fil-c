/* GStreamer
 * Copyright (C) <2010> Filippo Argiolas <filippo.argiolas@gmail.com>
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

#ifndef __GST_COLOR_EFFECTS_H__
#define __GST_COLOR_EFFECTS_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS
#define GST_TYPE_COLOR_EFFECTS \
  (gst_color_effects_get_type())
#define GST_COLOR_EFFECTS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_COLOR_EFFECTS,GstColorEffects))
#define GST_COLOR_EFFECTS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_COLOR_EFFECTS,GstColorEffectsClass))
#define GST_IS_COLOR_EFFECTS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_COLOR_EFFECTS))
#define GST_IS_COLOR_EFFECTS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_COLOR_EFFECTS))
typedef struct _GstColorEffects GstColorEffects;
typedef struct _GstColorEffectsClass GstColorEffectsClass;

/**
 * GstColorEffectsPreset:
 * @GST_CLUT_PRESET_NONE: Do nothing preset (default)
 * @GST_CLUT_PRESET_HEAT: Fake heat camera effect
 * @GST_CLUT_PRESET_SEPIA: Sepia toning filter
 * @GST_CLUT_PRESET_XRAY: Invert colors and slightly shade to cyan
 * @GST_CLUT_PRESET_XPRO: Cross Processing filter
 * @GST_CLUT_PRESET_YELLOWBLUE: Visual magnifier high-contrast color filter
 *
 * The lookup table to use to convert input colors
 */
typedef enum
{
  GST_COLOR_EFFECTS_PRESET_NONE,
  GST_COLOR_EFFECTS_PRESET_HEAT,
  GST_COLOR_EFFECTS_PRESET_SEPIA,
  GST_COLOR_EFFECTS_PRESET_XRAY,
  GST_COLOR_EFFECTS_PRESET_XPRO,
  GST_COLOR_EFFECTS_PRESET_YELLOWBLUE,
} GstColorEffectsPreset;

/**
 * GstColorEffects:
 *
 * Opaque datastructure.
 */
struct _GstColorEffects
{
  GstVideoFilter videofilter;

  /* < private > */
  GstColorEffectsPreset preset;
  const guint8 *table;
  gboolean map_luma;

  /* video format */
  GstVideoFormat format;
  gint width;
  gint height;

  void (*process) (GstColorEffects * filter, GstVideoFrame * frame);
};

struct _GstColorEffectsClass
{
  GstVideoFilterClass parent_class;
};

GType gst_color_effects_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (coloreffects);

G_END_DECLS
#endif /* __GST_COLOR_EFFECTS_H__ */
