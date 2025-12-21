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

#ifndef __GST_GL_BASE_MIXER_H__
#define __GST_GL_BASE_MIXER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoaggregator.h>
#include <gst/gl/gstglcontext.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_BASE_MIXER_PAD (gst_gl_base_mixer_pad_get_type())
#define GST_GL_BASE_MIXER_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_BASE_MIXER_PAD, GstGLBaseMixerPad))
#define GST_GL_BASE_MIXER_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_BASE_MIXER_PAD, GstGLBaseMixerPadClass))
#define GST_IS_GL_BASE_MIXER_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_BASE_MIXER_PAD))
#define GST_IS_GL_BASE_MIXER_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_BASE_MIXER_PAD))
#define GST_GL_BASE_MIXER_PAD_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_GL_BASE_MIXER_PAD,GstGLBaseMixerPadClass))

typedef struct _GstGLBaseMixerPad GstGLBaseMixerPad;
typedef struct _GstGLBaseMixerPadClass GstGLBaseMixerPadClass;

/**
 * GstGLBaseMixerPad:
 *
 * Since: 1.24
 */
struct _GstGLBaseMixerPad
{
  /**
   * GstGLBaseMixerPad.parent:
   *
   * parent #GstVideoAggregatorPad
   *
   * Since: 1.24
   */
  GstVideoAggregatorPad parent;

  /*< private >*/
  gpointer _padding[GST_PADDING];
};

/**
 * GstGLBaseMixerPadClass:
 *
 * Since: 1.24
 */
struct _GstGLBaseMixerPadClass
{
  /**
   * GstGLBaseMixerPadClass.parent_class:
   *
   * parent #GstVideoAggregatorPadClass
   *
   * Since: 1.24
   */
  GstVideoAggregatorPadClass parent_class;

  /*< private >*/
  gpointer _padding[GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstGLBaseMixerPad, gst_object_unref);

GST_GL_API
GType gst_gl_base_mixer_pad_get_type (void);

#define GST_TYPE_GL_BASE_MIXER (gst_gl_base_mixer_get_type())
#define GST_GL_BASE_MIXER(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_BASE_MIXER, GstGLBaseMixer))
#define GST_GL_BASE_MIXER_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_BASE_MIXER, GstGLBaseMixerClass))
#define GST_IS_GL_BASE_MIXER(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_BASE_MIXER))
#define GST_IS_GL_BASE_MIXER_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_BASE_MIXER))
#define GST_GL_BASE_MIXER_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_GL_BASE_MIXER,GstGLBaseMixerClass))

typedef struct _GstGLBaseMixer GstGLBaseMixer;
typedef struct _GstGLBaseMixerClass GstGLBaseMixerClass;
typedef struct _GstGLBaseMixerPrivate GstGLBaseMixerPrivate;

/**
 * GstGLBaseMixer:
 *
 * Since: 1.24
 */
struct _GstGLBaseMixer
{
  /**
   * GstGLBaseMixer.parent:
   *
   * parent #GstVideoAggregator
   *
   * Since: 1.24
   */
  GstVideoAggregator     parent;

  /**
   * GstGLBaseMixer.display:
   *
   * the currently configured #GstGLDisplay
   *
   * Since: 1.24
   */
  GstGLDisplay          *display;
  /**
   * GstGLBaseMixer.context:
   *
   * the currently configured #GstGLContext
   *
   * Since: 1.24
   */
  GstGLContext          *context;

  /*< private >*/
  gpointer _padding[GST_PADDING];

  GstGLBaseMixerPrivate *priv;
};

/**
 * GstGLBaseMixerClass:
 *
 * Since: 1.24
 */
struct _GstGLBaseMixerClass
{
  /**
   * GstGLBaseMixerClass.parent_class:
   *
   * the parent #GstVideoAggregatorClass
   *
   * Since: 1.24
   */
  GstVideoAggregatorClass parent_class;
  /**
   * GstGLBaseMixerClass.supported_gl_api:
   *
   * the logical-OR of #GstGLAPI's supported by this element
   *
   * Since: 1.24
   */
  GstGLAPI supported_gl_api;

  /**
   * GstGLBaseMixerClass::gl_start:
   *
   * called in the GL thread to setup the element GL state.
   *
   * Returns: whether the start was successful
   *
   * Since: 1.24
   */
  gboolean      (*gl_start)     (GstGLBaseMixer * mix);
  /**
   * GstGLBaseMixerClass::gl_stop:
   *
   * called in the GL thread to setup the element GL state.
   *
   * Since: 1.24
   */
  void          (*gl_stop)      (GstGLBaseMixer * mix);

  /*< private >*/
  gpointer _padding[GST_PADDING];
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstGLBaseMixer, gst_object_unref);

GST_GL_API
GType gst_gl_base_mixer_get_type(void);

GST_GL_API
GstGLContext *      gst_gl_base_mixer_get_gl_context        (GstGLBaseMixer * mix);

G_END_DECLS
#endif /* __GST_GL_BASE_MIXER_H__ */
