/*
 * Copyright (C) 2012, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 * Copyright (C) 2014, Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2014, Collabora Ltd.
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
#ifndef __GST_AMC_JNI_UTILS_H__
#define __GST_AMC_JNI_UTILS_H__

#include <jni.h>
#include <glib.h>
#include <gst/gst.h>

#if GLIB_SIZEOF_VOID_P == 8
#define JLONG_TO_GPOINTER(value) (gpointer)(value)
#define GPOINTER_TO_JLONG(value) (jlong)(value)
#else
#define JLONG_TO_GPOINTER(value) (gpointer)(jint)(value)
#define GPOINTER_TO_JLONG(value) (jlong)(jint)(value)
#endif

gint      gst_amc_jni_get_android_level(void);

jclass    gst_amc_jni_get_class              (JNIEnv * env,
                                             GError ** err,
                                             const gchar * name);

jmethodID gst_amc_jni_get_method_id          (JNIEnv * env,
                                             GError ** err,
                                             jclass klass,
                                             const gchar * name,
                                             const gchar * signature);

jmethodID gst_amc_jni_get_static_method_id   (JNIEnv * env,
                                             GError ** err,
                                             jclass klass,
                                             const gchar * name,
                                             const gchar * signature);

jfieldID gst_amc_jni_get_field_id            (JNIEnv * env,
                                             GError ** err,
                                             jclass klass,
                                             const gchar * name,
                                             const gchar * type);

jfieldID gst_amc_jni_get_static_field_id     (JNIEnv * env,
                                             GError ** err,
                                             jclass klass,
                                             const gchar * name,
                                             const gchar * type);

jobject gst_amc_jni_new_object               (JNIEnv * env,
                                             GError ** err,
                                             gboolean global,
                                             jclass klass,
                                             jmethodID constructor,
                                             ...);

jobject gst_amc_jni_new_object_from_static   (JNIEnv * env,
                                             GError ** err,
                                             gboolean global,
                                             jclass klass,
                                             jmethodID constructor,
                                             ...);

jobject gst_amc_jni_object_make_global       (JNIEnv * env,
                                             jobject object);

jobject gst_amc_jni_object_ref               (JNIEnv * env,
                                             jobject object);

void gst_amc_jni_object_unref                (JNIEnv * env,
                                             jobject object);

void gst_amc_jni_object_local_unref          (JNIEnv * env,
                                             jobject object);

gchar *gst_amc_jni_string_to_gchar           (JNIEnv * env,
                                             jstring string,
                                             gboolean release);

jstring gst_amc_jni_string_from_gchar        (JNIEnv * env,
                                             GError ** error,
                                             gboolean global,
                                             const gchar * string);

G_GNUC_PRINTF (5, 6)
void gst_amc_jni_set_error                   (JNIEnv * env,
                                              GError ** error,
                                              GQuark domain,
                                              gint code,
                                              const gchar * format, ...);

void gst_amc_jni_set_java_vm                 (JavaVM *java_vm);

gboolean gst_amc_jni_initialize              (void);

gboolean gst_amc_jni_is_vm_started           (void);

JNIEnv *gst_amc_jni_get_env                  (void);

jclass gst_amc_jni_get_application_class     (JNIEnv * env,
                                             const gchar * name,
                                             GError ** err);

#define DEF_CALL_STATIC_TYPE_METHOD(_type, _name,  _jname, _retval) \
gboolean gst_amc_jni_call_static_##_name##_method (JNIEnv *env, GError ** err, jclass klass, jmethodID methodID, _type * value, ...)

DEF_CALL_STATIC_TYPE_METHOD (gboolean, boolean, Boolean, FALSE);
DEF_CALL_STATIC_TYPE_METHOD (gint8, byte, Byte, G_MININT8);
DEF_CALL_STATIC_TYPE_METHOD (gshort, short, Short, G_MINSHORT);
DEF_CALL_STATIC_TYPE_METHOD (gint, int, Int, G_MININT);
DEF_CALL_STATIC_TYPE_METHOD (gchar, char, Char, 0);
DEF_CALL_STATIC_TYPE_METHOD (gint64, long, Long, G_MINLONG);
DEF_CALL_STATIC_TYPE_METHOD (gfloat, float, Float, G_MINFLOAT);
DEF_CALL_STATIC_TYPE_METHOD (gdouble, double, Double, G_MINDOUBLE);
DEF_CALL_STATIC_TYPE_METHOD (jobject, object, Object, NULL);

gboolean gst_amc_jni_call_static_void_method        (JNIEnv * env,
                                                    GError ** error,
                                                    jclass klass,
                                                    jmethodID method, ...);

#define DEF_CALL_TYPE_METHOD(_type, _name,  _jname, _retval) \
gboolean gst_amc_jni_call_##_name##_method (JNIEnv *env, GError ** err, jobject obj, jmethodID methodID, _type * value, ...)

DEF_CALL_TYPE_METHOD (gboolean, boolean, Boolean, FALSE);
DEF_CALL_TYPE_METHOD (gint8, byte, Byte, G_MININT8);
DEF_CALL_TYPE_METHOD (gshort, short, Short, G_MINSHORT);
DEF_CALL_TYPE_METHOD (gint, int, Int, G_MININT);
DEF_CALL_TYPE_METHOD (gchar, char, Char, 0);
DEF_CALL_TYPE_METHOD (gint64, long, Long, G_MINLONG);
DEF_CALL_TYPE_METHOD (gfloat, float, Float, G_MINFLOAT);
DEF_CALL_TYPE_METHOD (gdouble, double, Double, G_MINDOUBLE);
DEF_CALL_TYPE_METHOD (jobject, object, Object, NULL);

gboolean gst_amc_jni_call_void_method        (JNIEnv * env,
                                             GError ** error,
                                             jobject obj,
                                             jmethodID method, ...);

#define DEF_GET_TYPE_FIELD(_type, _name, _jname) \
gboolean gst_amc_jni_get_##_name##_field (JNIEnv *env, GError ** err, jobject obj, jfieldID fieldID, _type * value)

DEF_GET_TYPE_FIELD (gboolean, boolean, Boolean);
DEF_GET_TYPE_FIELD (gint8, byte, Byte);
DEF_GET_TYPE_FIELD (gshort, short, Short);
DEF_GET_TYPE_FIELD (gint, int, Int);
DEF_GET_TYPE_FIELD (gchar, char, Char);
DEF_GET_TYPE_FIELD (gint64, long, Long);
DEF_GET_TYPE_FIELD (gfloat, float, Float);
DEF_GET_TYPE_FIELD (gdouble, double, Double);
DEF_GET_TYPE_FIELD (jobject, object, Object);

#define DEF_GET_STATIC_TYPE_FIELD(_type, _name, _jname) \
gboolean gst_amc_jni_get_static_##_name##_field (JNIEnv *env, GError ** err, jclass klass, jfieldID fieldID, _type * value)

DEF_GET_STATIC_TYPE_FIELD (gboolean, boolean, Boolean);
DEF_GET_STATIC_TYPE_FIELD (gint8, byte, Byte);
DEF_GET_STATIC_TYPE_FIELD (gshort, short, Short);
DEF_GET_STATIC_TYPE_FIELD (gint, int, Int);
DEF_GET_STATIC_TYPE_FIELD (gchar, char, Char);
DEF_GET_STATIC_TYPE_FIELD (gint64, long, Long);
DEF_GET_STATIC_TYPE_FIELD (gfloat, float, Float);
DEF_GET_STATIC_TYPE_FIELD (gdouble, double, Double);
DEF_GET_STATIC_TYPE_FIELD (jobject, object, Object);

#endif
