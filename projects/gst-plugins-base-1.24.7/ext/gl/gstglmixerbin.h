/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef __GST_GL_MIXER_BIN_H__
#define __GST_GL_MIXER_BIN_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

GType gst_gl_mixer_bin_get_type(void);
#define GST_TYPE_GL_MIXER_BIN (gst_gl_mixer_bin_get_type())
#define GST_GL_MIXER_BIN(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_MIXER_BIN, GstGLMixerBin))
#define GST_GL_MIXER_BIN_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_MIXER_BIN, GstGLMixerBinClass))
#define GST_IS_GL_MIXER_BIN(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_MIXER_BIN))
#define GST_IS_GL_MIXER_BIN_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_MIXER_BIN))
#define GST_GL_MIXER_BIN_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_GL_MIXER_BIN,GstGLMixerBinClass))

typedef struct _GstGLMixerBin GstGLMixerBin;
typedef struct _GstGLMixerBinClass GstGLMixerBinClass;
typedef struct _GstGLMixerBinPrivate GstGLMixerBinPrivate;

struct _GstGLMixerBin
{
  GstBin parent;

  GstElement *mixer;
  GstElement *out_convert;
  GstElement *download;
  GstPad *srcpad;

  gboolean force_live;

  GstClockTime latency;
  guint start_time_selection;
  GstClockTime start_time;
  GstClockTime min_upstream_latency;

  GstGLMixerBinPrivate *priv;
};

struct _GstGLMixerBinClass
{
  GstBinClass parent_class;

  GstElement * (*create_element) (void);
  GstGhostPad * (*create_input_pad) (GstGLMixerBin * self, GstPad * mixer_pad);
};

void gst_gl_mixer_bin_finish_init (GstGLMixerBin * self);
void gst_gl_mixer_bin_finish_init_with_element (GstGLMixerBin * self,
    GstElement * element);

G_END_DECLS
#endif /* __GST_GL_MIXER_BIN_H__ */
