/*
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 *
 * Copyright (C) 2015, Collabora Ltd.
 *   Author: Matthieu Bouron <matthieu.bouron@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstjniutils.h"
#include "gstamcsurface.h"

G_DEFINE_TYPE (GstAmcSurface, gst_amc_surface, G_TYPE_OBJECT);

static gpointer parent_class = NULL;
static void gst_amc_surface_dispose (GObject * object);

static gboolean
_cache_java_class (GstAmcSurfaceClass * klass, GError ** err)
{
  JNIEnv *env;

  gst_amc_jni_initialize ();

  env = gst_amc_jni_get_env ();

  klass->jklass = gst_amc_jni_get_class (env, err, "android/view/Surface");
  if (!klass->jklass) {
    return FALSE;
  }

  klass->constructor =
      gst_amc_jni_get_method_id (env, err, klass->jklass, "<init>",
      "(Landroid/graphics/SurfaceTexture;)V");
  if (!klass->constructor) {
    goto error;
  }

  klass->is_valid =
      gst_amc_jni_get_method_id (env, err, klass->jklass, "isValid", "()Z");
  if (!klass->is_valid) {
    goto error;
  }

  klass->release =
      gst_amc_jni_get_method_id (env, err, klass->jklass, "release", "()V");
  if (!klass->release) {
    goto error;
  }

  klass->describe_contents =
      gst_amc_jni_get_method_id (env, err, klass->jklass, "describeContents",
      "()I");
  if (!klass->describe_contents) {
    goto error;
  }

  return TRUE;

error:
  gst_amc_jni_object_unref (env, klass->jklass);
  return FALSE;
}

static void
gst_amc_surface_class_init (GstAmcSurfaceClass * klass)
{
  GError *err = NULL;
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  gobject_class->dispose = gst_amc_surface_dispose;

  if (!_cache_java_class (klass, &err)) {
    GST_ERROR ("Could not cache java class android/view/Surface: %s",
        err->message);
    g_clear_error (&err);
  }
}

static void
gst_amc_surface_init (GstAmcSurface * self)
{
  /* initialize the object */
}

static void
gst_amc_surface_dispose (GObject * object)
{
  GstAmcSurface *self;
  JNIEnv *env;
  GError *err = NULL;

  self = GST_AMC_SURFACE (object);
  env = gst_amc_jni_get_env ();

  if (!gst_amc_surface_release (self, &err)) {
    if (err) {
      GST_ERROR ("Error: %s", err->message);
    }
  }

  if (self->jobject) {
    gst_amc_jni_object_unref (env, self->jobject);
  }

  if (self->texture != NULL) {
    g_object_unref (self->texture);
    self->texture = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

GstAmcSurface *
gst_amc_surface_new (GstAmcSurfaceTextureJNI * texture, GError ** err)
{
  GstAmcSurface *surface;
  GstAmcSurfaceClass *klass;
  JNIEnv *env;

  surface = g_object_new (GST_TYPE_AMC_SURFACE, NULL);
  env = gst_amc_jni_get_env ();
  klass = GST_AMC_SURFACE_GET_CLASS (surface);

  surface->jobject = gst_amc_jni_new_object (env, err, TRUE, klass->jklass,
      klass->constructor, gst_amc_surface_texture_jni_get_jobject (texture));
  if (surface->jobject == NULL) {
    g_object_unref (surface);
    return NULL;
  }

  surface->texture = g_object_ref (texture);
  return surface;
}

gboolean
gst_amc_surface_is_valid (GstAmcSurface * self, gboolean * result,
    GError ** err)
{
  JNIEnv *env;
  GstAmcSurfaceClass *klass;

  env = gst_amc_jni_get_env ();
  klass = GST_AMC_SURFACE_GET_CLASS (self);

  return gst_amc_jni_call_boolean_method (env, err, self->jobject,
      klass->is_valid, result);
}

gboolean
gst_amc_surface_release (GstAmcSurface * self, GError ** err)
{
  JNIEnv *env;
  GstAmcSurfaceClass *klass;

  env = gst_amc_jni_get_env ();
  klass = GST_AMC_SURFACE_GET_CLASS (self);

  return gst_amc_jni_call_void_method (env, err, self->jobject, klass->release);
}

gboolean
gst_amc_surface_describe_contents (GstAmcSurface * self, gint * result,
    GError ** err)
{
  JNIEnv *env;
  GstAmcSurfaceClass *klass;

  env = gst_amc_jni_get_env ();
  klass = GST_AMC_SURFACE_GET_CLASS (self);

  return gst_amc_jni_call_int_method (env, err, self->jobject,
      klass->describe_contents, result);
}
