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

#ifndef __GST_GL_MIXER_H__
#define __GST_GL_MIXER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gstglbasemixer.h>

G_BEGIN_DECLS

typedef struct _GstGLMixer GstGLMixer;
typedef struct _GstGLMixerClass GstGLMixerClass;
typedef struct _GstGLMixerPrivate GstGLMixerPrivate;

#define GST_TYPE_GL_MIXER_PAD (gst_gl_mixer_pad_get_type())
#define GST_GL_MIXER_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_MIXER_PAD, GstGLMixerPad))
#define GST_GL_MIXER_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_MIXER_PAD, GstGLMixerPadClass))
#define GST_IS_GL_MIXER_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_MIXER_PAD))
#define GST_IS_GL_MIXER_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_MIXER_PAD))
#define GST_GL_MIXER_PAD_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_GL_MIXER_PAD,GstGLMixerPadClass))

typedef struct _GstGLMixerPad GstGLMixerPad;
typedef struct _GstGLMixerPadClass GstGLMixerPadClass;

/**
 * GstGLMixerPad:
 *
 * Since: 1.24
 */
struct _GstGLMixerPad
{
  /**
   * GstGLMixerPad.parent:
   *
   * parent #GstGLBaseMixerPad
   *
   * Since: 1.24
   */
  GstGLBaseMixerPad parent;

  /**
   * GstGLMixerPad.current_texture:
   *
   * the current input texture for this pad
   *
   * Since: 1.24
   */
  guint current_texture;

  /*< private >*/
  gpointer _padding[GST_PADDING];
};

/**
 * GstGLMixerPadClass:
 *
 * Since: 1.24
 */
struct _GstGLMixerPadClass
{
  /**
   * GstGLMixerPadClass.parent_class:
   *
   * parent #GstGLBaseMixerPadClass
   *
   * Since: 1.24
   */
  GstGLBaseMixerPadClass parent_class;

  /*< private >*/
  gpointer _padding[GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstGLMixerPad, gst_object_unref);

GST_GL_API
GType gst_gl_mixer_pad_get_type (void);

#define GST_TYPE_GL_MIXER (gst_gl_mixer_get_type())
#define GST_GL_MIXER(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_MIXER, GstGLMixer))
#define GST_GL_MIXER_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_MIXER, GstGLMixerClass))
#define GST_IS_GL_MIXER(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_MIXER))
#define GST_IS_GL_MIXER_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_MIXER))
#define GST_GL_MIXER_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_GL_MIXER,GstGLMixerClass))

/**
 * GstGLMixer:
 *
 * Since: 1.24
 */
struct _GstGLMixer
{
  /**
   * GstGLMixer.parent:
   *
   * Since: 1.24
   */
  GstGLBaseMixer parent;

  /**
   * GstGLMixer.out_caps:
   *
   * the configured output #GstCaps
   *
   * Since: 1.24
   */
  GstCaps *out_caps;

  /*< private >*/
  GstGLMixerPrivate *priv;

  /*< private >*/
  gpointer _padding[GST_PADDING];
};

/**
 * GstGLMixerClass:
 *
 * Since: 1.24
 */
struct _GstGLMixerClass
{
  /**
   * GstGLMixerClass.parent_class:
   *
   * Since: 1.24
   */
  GstGLBaseMixerClass parent_class;

  /**
   * GstGLMixerClass::process_buffers:
   *
   * Perform operations on the input buffers to produce an
   * output buffer.
   *
   * Since: 1.24
   */
  gboolean    (*process_buffers)      (GstGLMixer * mix, GstBuffer *outbuf);
  /**
   * GstGLMixerClass::process_textures:
   *
   * perform operations with the input and output buffers mapped
   * as textures.  Will not be called if @process_buffers is overriden.
   *
   * Since: 1.24
   */
  gboolean    (*process_textures)     (GstGLMixer * mix, GstGLMemory *out_tex);

  /*< private >*/
  gpointer _padding[GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstGLMixer, gst_object_unref);

GST_GL_API
GType gst_gl_mixer_get_type(void);

GST_GL_API
void gst_gl_mixer_class_add_rgba_pad_templates (GstGLMixerClass * klass);

GST_GL_API
gboolean gst_gl_mixer_process_textures (GstGLMixer * mix, GstBuffer * outbuf);

GST_GL_API
GstGLFramebuffer * gst_gl_mixer_get_framebuffer (GstGLMixer * mix);

G_END_DECLS
#endif /* __GST_GL_MIXER_H__ */
