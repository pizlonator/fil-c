/*
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 *
 * Copyright (C) 2014,2018 Collabora Ltd.
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
#include "gstamcsurfacetexture-jni.h"
#include "gstamc-jni.h"

struct _GstAmcSurfaceTextureJNI
{
  GstAmcSurfaceTexture parent_instance;
  jobject jobject;
  gint texture_id;

  jobject listener;
  jmethodID set_context_id;
  GstAmcSurfaceTextureOnFrameAvailableCallback callback;
  gpointer user_data;
};

static struct
{
  jclass jklass;
  jmethodID constructor;
  jmethodID set_on_frame_available_listener;
  jmethodID update_tex_image;
  jmethodID detach_from_gl_context;
  jmethodID attach_to_gl_context;
  jmethodID get_transform_matrix;
  jmethodID get_timestamp;
  jmethodID release;
} surface_texture;


G_DEFINE_TYPE (GstAmcSurfaceTextureJNI, gst_amc_surface_texture_jni,
    GST_TYPE_AMC_SURFACE_TEXTURE);

gboolean
gst_amc_surface_texture_jni_static_init (void)
{
  JNIEnv *env;
  GError *err = NULL;

  env = gst_amc_jni_get_env ();

  surface_texture.jklass = gst_amc_jni_get_class (env, &err,
      "android/graphics/SurfaceTexture");
  if (!surface_texture.jklass) {
    GST_ERROR ("Failed to get android.graphics.SurfaceTexture class: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  surface_texture.constructor =
      gst_amc_jni_get_method_id (env, &err, surface_texture.jklass, "<init>",
      "(I)V");
  if (!surface_texture.constructor) {
    goto error;
  }

  surface_texture.set_on_frame_available_listener =
      gst_amc_jni_get_method_id (env, &err, surface_texture.jklass,
      "setOnFrameAvailableListener",
      "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V");
  if (!surface_texture.set_on_frame_available_listener) {
    goto error;
  }

  surface_texture.update_tex_image =
      gst_amc_jni_get_method_id (env, &err, surface_texture.jklass,
      "updateTexImage", "()V");
  if (!surface_texture.update_tex_image) {
    goto error;
  }

  surface_texture.detach_from_gl_context =
      gst_amc_jni_get_method_id (env, &err, surface_texture.jklass,
      "detachFromGLContext", "()V");
  if (!surface_texture.detach_from_gl_context) {
    goto error;
  }

  surface_texture.attach_to_gl_context =
      gst_amc_jni_get_method_id (env, &err, surface_texture.jklass,
      "attachToGLContext", "(I)V");
  if (!surface_texture.attach_to_gl_context) {
    goto error;
  }

  surface_texture.get_transform_matrix =
      gst_amc_jni_get_method_id (env, &err, surface_texture.jklass,
      "getTransformMatrix", "([F)V");
  if (!surface_texture.get_transform_matrix) {
    goto error;
  }

  surface_texture.get_timestamp =
      gst_amc_jni_get_method_id (env, &err, surface_texture.jklass,
      "getTimestamp", "()J");
  if (!surface_texture.get_timestamp) {
    goto error;
  }

  surface_texture.release =
      gst_amc_jni_get_method_id (env, &err, surface_texture.jklass, "release",
      "()V");
  if (!surface_texture.release) {
    goto error;
  }

  return TRUE;

error:
  GST_ERROR ("Failed to get android.graphics.SurfaceTexture methods: %s",
      err->message);
  g_clear_error (&err);
  gst_amc_jni_object_unref (env, surface_texture.constructor);
  return FALSE;
}

static gboolean
gst_amc_surface_texture_jni_update_tex_image (GstAmcSurfaceTexture * base,
    GError ** err)
{
  GstAmcSurfaceTextureJNI *self = GST_AMC_SURFACE_TEXTURE_JNI (base);
  JNIEnv *env;

  env = gst_amc_jni_get_env ();

  return gst_amc_jni_call_void_method (env, err, self->jobject,
      surface_texture.update_tex_image);
}

static gboolean
gst_amc_surface_texture_jni_detach_from_gl_context (GstAmcSurfaceTexture * base,
    GError ** err)
{
  GstAmcSurfaceTextureJNI *self = GST_AMC_SURFACE_TEXTURE_JNI (base);
  JNIEnv *env;
  gboolean ret;

  env = gst_amc_jni_get_env ();

  ret =
      gst_amc_jni_call_void_method (env, err, self->jobject,
      surface_texture.detach_from_gl_context);
  self->texture_id = 0;
  return ret;
}

static gboolean
gst_amc_surface_texture_jni_attach_to_gl_context (GstAmcSurfaceTexture * base,
    gint texture_id, GError ** err)
{
  GstAmcSurfaceTextureJNI *self = GST_AMC_SURFACE_TEXTURE_JNI (base);
  JNIEnv *env;
  gboolean ret;

  env = gst_amc_jni_get_env ();

  ret =
      gst_amc_jni_call_void_method (env, err, self->jobject,
      surface_texture.attach_to_gl_context, texture_id);
  self->texture_id = texture_id;
  return ret;
}

static gboolean
gst_amc_surface_texture_jni_get_transform_matrix (GstAmcSurfaceTexture * base,
    gfloat * matrix, GError ** err)
{
  GstAmcSurfaceTextureJNI *self = GST_AMC_SURFACE_TEXTURE_JNI (base);
  JNIEnv *env;
  gboolean ret;
  /* 4x4 Matrix */
  jsize size = 16;
  jfloatArray floatarray;

  env = gst_amc_jni_get_env ();

  floatarray = (*env)->NewFloatArray (env, size);
  ret =
      gst_amc_jni_call_void_method (env, err, self->jobject,
      surface_texture.get_transform_matrix, floatarray);
  if (ret) {
    (*env)->GetFloatArrayRegion (env, floatarray, 0, size, (jfloat *) matrix);
    (*env)->DeleteLocalRef (env, floatarray);
  }

  return ret;
}

static gboolean
gst_amc_surface_texture_jni_get_timestamp (GstAmcSurfaceTexture * base,
    gint64 * result, GError ** err)
{
  GstAmcSurfaceTextureJNI *self = GST_AMC_SURFACE_TEXTURE_JNI (base);
  JNIEnv *env;

  env = gst_amc_jni_get_env ();

  return gst_amc_jni_call_long_method (env, err, self->jobject,
      surface_texture.get_timestamp, result);
}

static gboolean
gst_amc_surface_texture_jni_release (GstAmcSurfaceTexture * base, GError ** err)
{
  GstAmcSurfaceTextureJNI *self = GST_AMC_SURFACE_TEXTURE_JNI (base);
  JNIEnv *env;

  env = gst_amc_jni_get_env ();

  return gst_amc_jni_call_void_method (env, err, self->jobject,
      surface_texture.release);
}

static void
on_frame_available_cb (JNIEnv * env, jobject thiz,
    long long context, jobject surfaceTexture)
{
  GstAmcSurfaceTextureJNI *self = JLONG_TO_GPOINTER (context);
  if (!self || !self->callback)
    return;

  self->callback (GST_AMC_SURFACE_TEXTURE (self), self->user_data);
}

static gboolean
create_listener (GstAmcSurfaceTextureJNI * self, JNIEnv * env, GError ** err)
{
  jclass listener_cls = NULL;
  jmethodID constructor_id = 0;

  JNINativeMethod amcOnFrameAvailableListener = {
    "native_onFrameAvailable",
    "(JLandroid/graphics/SurfaceTexture;)V",
    (void *) on_frame_available_cb,
  };

  listener_cls =
      gst_amc_jni_get_application_class (env,
      "org/freedesktop/gstreamer/androidmedia/GstAmcOnFrameAvailableListener",
      err);
  if (!listener_cls) {
    return FALSE;
  }

  (*env)->RegisterNatives (env, listener_cls, &amcOnFrameAvailableListener, 1);
  if ((*env)->ExceptionCheck (env)) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to register native methods");
    goto done;
  }

  constructor_id =
      gst_amc_jni_get_method_id (env, err, listener_cls, "<init>", "()V");
  if (!constructor_id) {
    goto done;
  }

  self->set_context_id =
      gst_amc_jni_get_method_id (env, err, listener_cls, "setContext", "(J)V");
  if (!self->set_context_id) {
    goto done;
  }

  self->listener =
      gst_amc_jni_new_object (env, err, TRUE, listener_cls, constructor_id);
  if (!self->listener) {
    goto done;
  }

  if (!gst_amc_jni_call_void_method (env, err, self->listener,
          self->set_context_id, GPOINTER_TO_JLONG (self))) {
    gst_amc_jni_object_unref (env, self->listener);
    self->listener = NULL;
  }

done:
  gst_amc_jni_object_unref (env, listener_cls);

  return self->listener != NULL;
}

static gboolean
remove_listener (GstAmcSurfaceTextureJNI * self, JNIEnv * env, GError ** err)
{
  if (self->listener) {
    if (!gst_amc_jni_call_void_method (env, err, self->listener,
            self->set_context_id, GPOINTER_TO_JLONG (NULL)))
      return FALSE;

    gst_amc_jni_object_unref (env, self->listener);
    self->listener = NULL;
  }

  return TRUE;
}

static gboolean
    gst_amc_surface_texture_jni_set_on_frame_available_callback
    (GstAmcSurfaceTexture * base,
    GstAmcSurfaceTextureOnFrameAvailableCallback callback, gpointer user_data,
    GError ** err)
{
  GstAmcSurfaceTextureJNI *self = GST_AMC_SURFACE_TEXTURE_JNI (base);
  JNIEnv *env;
  GError *local_error = NULL;

  env = gst_amc_jni_get_env ();

  if (!remove_listener (self, env, err))
    return FALSE;

  self->callback = callback;
  self->user_data = user_data;
  if (callback == NULL)
    return TRUE;

  if (!create_listener (self, env, &local_error)) {
    GST_ERROR ("Could not create listener: %s", local_error->message);
    g_propagate_error (err, local_error);
    return FALSE;
  }

  if (!gst_amc_jni_call_void_method (env, err, self->jobject,
          surface_texture.set_on_frame_available_listener, self->listener)) {
    remove_listener (self, env, NULL);
    return FALSE;
  }

  return TRUE;
}

static void
gst_amc_surface_texture_jni_dispose (GObject * object)
{
  GstAmcSurfaceTextureJNI *self = GST_AMC_SURFACE_TEXTURE_JNI (object);
  JNIEnv *env;
  GError *err = NULL;

  env = gst_amc_jni_get_env ();

  if (!gst_amc_surface_texture_jni_release (GST_AMC_SURFACE_TEXTURE (self),
          &err)) {
    GST_ERROR ("Could not release surface texture: %s", err->message);
    g_clear_error (&err);
  }

  remove_listener (self, env, NULL);

  if (self->jobject) {
    gst_amc_jni_object_unref (env, self->jobject);
  }

  G_OBJECT_CLASS (gst_amc_surface_texture_jni_parent_class)->dispose (object);
}

static void
gst_amc_surface_texture_jni_class_init (GstAmcSurfaceTextureJNIClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAmcSurfaceTextureClass *surface_texture_class =
      GST_AMC_SURFACE_TEXTURE_CLASS (klass);

  gobject_class->dispose = gst_amc_surface_texture_jni_dispose;

  surface_texture_class->update_tex_image =
      gst_amc_surface_texture_jni_update_tex_image;
  surface_texture_class->detach_from_gl_context =
      gst_amc_surface_texture_jni_detach_from_gl_context;
  surface_texture_class->attach_to_gl_context =
      gst_amc_surface_texture_jni_attach_to_gl_context;
  surface_texture_class->get_transform_matrix =
      gst_amc_surface_texture_jni_get_transform_matrix;
  surface_texture_class->get_timestamp =
      gst_amc_surface_texture_jni_get_timestamp;
  surface_texture_class->release = gst_amc_surface_texture_jni_release;
  surface_texture_class->set_on_frame_available_callback =
      gst_amc_surface_texture_jni_set_on_frame_available_callback;
}

static void
gst_amc_surface_texture_jni_init (GstAmcSurfaceTextureJNI * self)
{
}

GstAmcSurfaceTextureJNI *
gst_amc_surface_texture_jni_new (GError ** err)
{
  GstAmcSurfaceTextureJNI *self = NULL;
  JNIEnv *env;

  self = g_object_new (GST_TYPE_AMC_SURFACE_TEXTURE_JNI, NULL);
  env = gst_amc_jni_get_env ();

  self->texture_id = 0;

  self->jobject =
      gst_amc_jni_new_object (env, err, TRUE, surface_texture.jklass,
      surface_texture.constructor, self->texture_id);
  if (self->jobject == NULL) {
    goto error;
  }

  if (!gst_amc_surface_texture_jni_detach_from_gl_context ((GstAmcSurfaceTexture
              *) self, err)) {
    goto error;
  }

  return self;

error:
  if (self)
    g_object_unref (self);
  return NULL;
}

jobject
gst_amc_surface_texture_jni_get_jobject (GstAmcSurfaceTextureJNI * self)
{
  return self->jobject;
}
